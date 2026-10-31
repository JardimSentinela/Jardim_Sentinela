// ===== ARMAZENAMENTO LOCAL ===================================================

#include "armazenamento.h"
#include "config.h"
#include "estado.h"
#include "cursor.h"
#include "latencia.h"
#include "linha.h"
#include "relogio.h"
#include "resolvidos.h"

// Reescreve /fila.ndjson guardando apenas os bytes [inicio, fim) do arquivo
// atual. Grava antes em /fila.tmp e só então substitui o original por rename:
// se faltar energia no meio, o arquivo original continua íntegro.
static bool reescreverTrecho(size_t inicio, size_t fim) {
  if (inicio >= fim) return true;

  File orig = LittleFS.open(ARQ_FILA, "r");
  if (!orig) return false;

  File tmp = LittleFS.open(ARQ_TMP, "w");
  if (!tmp) { orig.close(); return false; }

  if (inicio > 0 && !orig.seek(inicio, SeekSet)) { tmp.close(); orig.close(); return false; }

  // Cópia em blocos: o tamanho do arquivo não é limitado pela RAM.
  char bloco[512];
  size_t restantes = fim - inicio;
  while (restantes > 0) {
    size_t pedir = (restantes < sizeof(bloco)) ? restantes : sizeof(bloco);
    int lidos = orig.read((uint8_t*)bloco, pedir);
    if (lidos <= 0) break;
    tmp.write((uint8_t*)bloco, (size_t)lidos);
    restantes -= (size_t)lidos;
  }
  tmp.close();
  orig.close();

  return LittleFS.rename(ARQ_TMP, ARQ_FILA);
}

// Devolve o offset do começo da primeira linha completa que começa depois de
// 'aPartir'. Devolve 0 se não achar '\n' (não dá para cortar sem quebrar linha).
static size_t inicioDaLinhaSeguinte(File& f, size_t aPartir) {
  if (!f.seek(aPartir, SeekSet)) return 0;
  char bloco[256];
  size_t pos = aPartir;
  for (;;) {
    int lidos = f.read((uint8_t*)bloco, sizeof(bloco));
    if (lidos <= 0) return 0;
    for (int i = 0; i < lidos; i++) {
      if (bloco[i] == '\n') return pos + (size_t)i + 1;
    }
    pos += (size_t)lidos;
  }
}

// Devolve o offset logo após o último '\n' antes de 'limite' (ou 0 se não houver).
static size_t fimDaUltimaLinhaCompleta(File& f, size_t limite) {
  const size_t TAM = 512;
  size_t fim = limite;
  char bloco[512];
  while (fim > 0) {
    size_t inicio = (fim > TAM) ? (fim - TAM) : 0;
    size_t pedir = fim - inicio;
    if (!f.seek(inicio, SeekSet)) return 0;
    int lidos = f.read((uint8_t*)bloco, pedir);
    if (lidos <= 0) return 0;
    for (int i = lidos - 1; i >= 0; i--) {
      if (bloco[i] == '\n') return inicio + (size_t)i + 1;
    }
    fim = inicio;
  }
  return 0;
}

// Conta quantos registros (linhas) existem no prefixo [0, ate) do arquivo.
uint32_t contarLinhasNoPrefixo(File& f, size_t ate) {
  uint32_t n = 0;
  if (!f.seek(0, SeekSet)) return 0;
  char bloco[512];
  size_t lidos = 0;
  while (lidos < ate) {
    size_t pedir = (ate - lidos < sizeof(bloco)) ? (ate - lidos) : sizeof(bloco);
    int r = f.read((uint8_t*)bloco, pedir);
    if (r <= 0) break;
    for (int i = 0; i < r; i++) if (bloco[i] == '\n') n++;
    lidos += (size_t)r;
  }
  return n;
}

// Offset do inicio da linha que contem a chave do cursor. (size_t)-1 quando o
// cursor nao esta no arquivo (sumiu por compactacao ou nunca foi gravado).
static size_t offsetDoCursor(File& f) {
  if (!g_ackValido) return (size_t)-1;
  if (!f.seek(0, SeekSet)) return (size_t)-1;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0;
  size_t pos = 0;
  while (lerLinha(f, linha, sizeof(linha))) {
    size_t inicio = pos;
    pos = (size_t)f.position();
    if (!extrairChave(linha, boot, &seq)) continue;
    if (strcmp(boot, g_ackBootId) == 0 && seq == g_ackSeq) return inicio;
  }
  return (size_t)-1;
}

// Conta, no prefixo [0, ate), as linhas que a compactacao vai remover sem
// confirmacao. Uma linha antes/na chave do cursor esta confirmada; uma linha
// depois so nao esta se tambem nao constar do conjunto de confirmados fora de
// ordem. Sem cursor no arquivo, conta o prefixo inteiro (conservador).
static uint32_t contarPerdidosNoPrefixo(File& f, size_t ate) {
  size_t cursorOff = offsetDoCursor(f);
  if (!f.seek(0, SeekSet)) return 0;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0, perdidos = 0;
  size_t pos = 0;
  while (pos < ate && lerLinha(f, linha, sizeof(linha))) {
    size_t inicio = pos;
    pos = (size_t)f.position();
    if (inicio >= ate) break;
    if (cursorOff != (size_t)-1 && inicio <= cursorOff) continue;
    if (extrairChave(linha, boot, &seq) && indiceResolvido(boot, seq) >= 0) continue;
    perdidos++;
  }
  return perdidos;
}

// Repara o arquivo se a última gravação ficou pela metade (falta de energia):
// reescreve o arquivo sem a linha truncada, para que ele termine sempre em
// linha completa.
static void recuperarLinhaParcial() {
  File f = LittleFS.open(ARQ_FILA, "r");
  if (!f) return;
  size_t total = f.size();
  if (total == 0) { f.close(); return; }

  f.seek(total - 1, SeekSet);
  int ultimo = f.read();
  if (ultimo == '\n') { f.close(); return; }  // arquivo íntegro

  size_t corte = fimDaUltimaLinhaCompleta(f, total);
  f.close();
  if (corte < total) {
    Serial.printf("AVISO: linha parcial removida (%u bytes)\n", (unsigned)(total - corte));
    reescreverTrecho(0, corte);
  }
}

// Política de arquivo cheio: descarta os registros MAIS ANTIGOS e mantém os
// novos. Libera espaço em blocos (3/4 do limite), reescrevendo o que sobra
// num arquivo temporário e trocando por rename. Com forcar=true (comando 'f'),
// compacta mesmo sem o arquivo estar cheio, mantendo 3/4 do tamanho atual.
static bool compactarFila(bool forcar = false) {
  File f = LittleFS.open(ARQ_FILA, "r");
  if (!f) return false;
  size_t total = f.size();
  if (total == 0) { f.close(); return true; }

  size_t alvo = forcar ? (total * FRACAO_MANTER / 4)
                       : ((MAX_BYTES_FILA / 4) * FRACAO_MANTER);
  if (total <= alvo) { f.close(); return true; }  // já cabe (ou pequeno demais)

  size_t inicio = inicioDaLinhaSeguinte(f, total - alvo);
  if (inicio == 0 || inicio >= total) { f.close(); return false; }

  uint32_t removidos = contarLinhasNoPrefixo(f, inicio);
  uint32_t perdidos  = contarPerdidosNoPrefixo(f, inicio);
  f.close();

  if (!reescreverTrecho(inicio, total)) return false;

  descartados += removidos;
  g_envio.perdidos += perdidos;
  Serial.printf("%s: %lu registro(s) antigo(s) descartado(s), %lu nao confirmado(s) (total %lu/%lu)\n",
                forcar ? "Fila compactada (forcada)" : "Fila cheia",
                (unsigned long)removidos, (unsigned long)perdidos,
                (unsigned long)descartados.load(), (unsigned long)g_envio.perdidos);
  return true;
}

// Grava um registro no fim do arquivo /fila.ndjson (uma linha JSON terminada em
// LF). Se o arquivo estiver cheio, descarta os registros mais antigos para
// caber o novo; o registro novo nunca é descartado por falta de espaço.
void persistir(const Evento& e) {
  // Se a gravação anterior pode ter sido interrompida, repara o arquivo antes.
  if (arquivoPodeTerLinhaParcial) {
    recuperarLinhaParcial();
    arquivoPodeTerLinhaParcial = false;
  }

  char ts[25];
  formatarTimestamp(e.tMs, e.relogioValido, ts, sizeof(ts));

  char idSensor[16];
  formatarIdSensor(e, idSensor, sizeof(idSensor));

  char linha[TAMANHO_LINHA];
  montarLinha(e, ts, idSensor, linha, sizeof(linha));

  size_t necessario = strlen(linha) + 1;  // linha + '\n'

  File f = LittleFS.open(ARQ_FILA, "a");
  if (!f) { Serial.println("ERRO: nao abriu arquivo"); descartados++; return; }

  if (f.size() + necessario > MAX_BYTES_FILA) {
    f.close();
    compactarFila();
    f = LittleFS.open(ARQ_FILA, "a");
    if (!f) { Serial.println("ERRO: nao abriu arquivo"); descartados++; return; }
    if (f.size() + necessario > MAX_BYTES_FILA) {
      f.close();
      Serial.printf("ERRO: nao coube no arquivo, registro descartado (total %lu)\n",
                    (unsigned long)descartados.load());
      descartados++;
      return;
    }
  }

  size_t escritos = f.print(linha);
  f.print("\n");
  f.close();
  if (escritos != strlen(linha)) arquivoPodeTerLinhaParcial = true;

  if (e.tipo == REG_EVENTO) {
    Serial.printf("[LIMIAR] Sensor: %s | nivelAgua: %.1f cm | Delta t: %lu ms | Timestamp: %s\n",
                  idSensor, e.nivelAgua, (unsigned long)e.deltaTMs, ts);
  }
  Serial.println(linha);
}

// Comando 'd': imprime o conteúdo do arquivo no monitor serial.
void despejarFila() {
  Serial.println("--- INICIO ---");
  File f = LittleFS.open(ARQ_FILA, "r");
  if (f) {
    while (f.available()) Serial.println(f.readStringUntil('\n'));
    f.close();
  }
  Serial.printf("--- FIM (descartados: %lu) ---\n", (unsigned long)descartados.load());
}

// Comando 'L': apaga o arquivo da fila (e o temporário de compactação).
void limparFila() {
  LittleFS.remove(ARQ_FILA);
  LittleFS.remove(ARQ_TMP);
  descartados = 0;
  g_envio.perdidos = 0;  // comeca uma contabilidade nova junto com o arquivo
  zerarLatencia();       // o teste de 20 min comeca com p50/p95 limpos
  arquivoPodeTerLinhaParcial = false;

  // INC 7B: o arquivo sumiu, entao o conjunto de confirmados fora de ordem e a
  // ancora nao valem mais. O cursor na NVS e preservado de proposito (como no
  // INC 4): a proxima selecao nao acha a chave e reancora no inicio.
  g_resolvidosN = 0;
  g_ancoraEnviada = false;
  g_ancoraSelecionada = false;

  Serial.println("Arquivo apagado.");
}

// Comando 'x' (INC 8): injeta uma linha JSON truncada, sem LF, no fim do
// arquivo. O proximo persistir (ou o proximo boot) chama recuperarLinhaParcial
// e remove so essa cauda, sem tocar nas linhas completas anteriores.
void injetarLinhaTruncada() {
  File f = LittleFS.open(ARQ_FILA, "a");
  if (!f) { Serial.println("TESTE: nao abriu a fila para injetar"); return; }
  static const char* TRUNCADA =
      "{\"tipo\":\"snapshot\",\"idJardim\":\"jardim-centro-01\","
      "\"idSensor\":\"jardim\",\"nivelAgua\":";
  size_t n = f.print(TRUNCADA);  // sem '\n' de proposito
  f.close();
  arquivoPodeTerLinhaParcial = true;
  Serial.printf("TESTE: linha truncada injetada (%u bytes, sem LF); resete para o boot recuperar\n",
                (unsigned)n);
}

// Comando 'f' (INC 8): força a compactação mesmo com o arquivo abaixo do
// limite, mantendo 3/4 do tamanho atual. Antes, repara uma eventual cauda
// parcial para a contabilidade de linhas nao misturar os dois casos.
void forcarCompactacao() {
  if (arquivoPodeTerLinhaParcial) {
    recuperarLinhaParcial();
    arquivoPodeTerLinhaParcial = false;
  }
  File f = LittleFS.open(ARQ_FILA, "r");
  size_t antes = f ? f.size() : 0;
  if (f) f.close();
  Serial.println("--- COMPACTACAO FORCADA ---");
  bool ok = compactarFila(true);
  f = LittleFS.open(ARQ_FILA, "r");
  size_t depois = f ? f.size() : 0;
  if (f) f.close();
  Serial.printf("TESTE: compactacao forcada %s (%u -> %u bytes)\n",
                ok && depois < antes ? "ok" : "sem efeito",
                (unsigned)antes, (unsigned)depois);
}

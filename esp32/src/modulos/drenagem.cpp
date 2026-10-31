// ===== DRENAGEM (INC 7B) =====================================================

#include "drenagem.h"
#include "config.h"
#include "estado.h"
#include "cursor.h"
#include "gateway.h"
#include "latencia.h"
#include "rede.h"
#include "resolvidos.h"
#include <LittleFS.h>

// Backoff exponencial das falhas transitórias: 2, 4, 8, 16, 32, 60, 60... s.
static uint32_t backoffMs(uint32_t falhas) {
  uint32_t ms = 2000;
  while (falhas > 1 && ms < 60000) { ms *= 2; falhas--; }
  return ms > 60000 ? 60000 : ms;
}

// 429 sem Retry-After: 30, 60, 120, 240, 300, 300... s.
static uint32_t backoff429Ms(uint32_t falhas) {
  uint32_t seg = 30;
  while (falhas > 1 && seg < 300) { seg *= 2; falhas--; }
  return (seg > 300 ? 300 : seg) * 1000;
}

// Classifica a falha e agenda a próxima tentativa. O cursor NÃO avança: a linha
// segue pendente (no 429 o receptor pode ter gravado o corpo; haverá duplicata).
static void agendarNovaTentativa(int codigo) {
  g_falhasConsecutivas++;
  uint32_t espera;

  if (codigo == 429 && g_retryAfterSeg > 0) {
    uint32_t seg = g_retryAfterSeg > 300 ? 300 : g_retryAfterSeg;
    espera = seg * 1000;
    if (espera < 5000) espera = 5000;
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "http 429 (Retry-After)");
  } else if (codigo == 429) {
    espera = backoff429Ms(g_falhasConsecutivas);
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "http 429 (limite)");
  } else if (codigo == -2) {
    espera = 60000;
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "config: URL vazia");
  } else if (codigo == -4) {
    espera = 60000;
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "codec: linha invalida");
  } else if (codigo >= 400 && codigo < 500) {
    // Erro de contrato é sistêmico: para a fila e retenta em 5 min (INC 5).
    espera = 300000;
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "http %d (contrato)", codigo);
  } else if (codigo < 0) {
    espera = backoffMs(g_falhasConsecutivas);
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "rede/timeout");
  } else {
    espera = backoffMs(g_falhasConsecutivas);
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "http %d", codigo);
  }

  g_proximaTentativaMs = millis() + espera;
  Serial.printf("ENVIO: %s; falha %lu, proxima em %lu s\n",
                g_ultimoErro, (unsigned long)g_falhasConsecutivas,
                (unsigned long)(espera / 1000));
}

static void adicionarCandidato(LoteLinhas* lote, const char* linha, const char* boot, uint32_t seq) {
  if (lote->n >= 6) return;
  strncpy(lote->linha[lote->n], linha, sizeof(lote->linha[0]) - 1);
  lote->linha[lote->n][sizeof(lote->linha[0]) - 1] = '\0';
  memcpy(lote->boot[lote->n], boot, 9);
  lote->seq[lote->n] = seq;
  lote->n++;
}

// Âncora: o snapshot mais recente ainda não resolvido, uma vez por drenagem.
static bool buscarAncora(LoteLinhas* lote) {
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (!ok) return false;
  char linha[TAMANHO_LINHA + 16];
  char melhor[TAMANHO_LINHA + 16];
  char boot[9], melhorBoot[9];
  uint32_t seq = 0, melhorSeq = 0;
  bool tem = false;

  while (lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) break;
    if (indiceResolvido(boot, seq) >= 0) continue;
    if (tipoDaLinha(linha) != codec::SNAPSHOT) continue;
    strncpy(melhor, linha, sizeof(melhor) - 1);
    melhor[sizeof(melhor) - 1] = '\0';
    memcpy(melhorBoot, boot, 9);
    melhorSeq = seq;
    tem = true;
  }
  f.close();
  if (tem) adicionarCandidato(lote, melhor, melhorBoot, melhorSeq);
  return tem;
}

// Boot indicado (se ainda não resolvido) + até max-1 eventos dele, em ordem.
static int coletarBoot(LoteLinhas* lote, const char* alvo, int max) {
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (!ok) return 0;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0;
  bool achouBoot = false;

  while (lote->n < max && lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) break;
    if (strcmp(boot, alvo) != 0) continue;
    if (indiceResolvido(boot, seq) >= 0) continue;
    uint8_t tipo = tipoDaLinha(linha);
    if (!achouBoot) {
      if (tipo == codec::BOOT) {
        adicionarCandidato(lote, linha, boot, seq);
        achouBoot = true;
      } else if (tipo == codec::EVENTO) {
        break;  // boot já resolvido/antes do cursor; eventos ficam para a etapa 4
      }
      continue;
    }
    if (tipo != codec::EVENTO) continue;
    uint32_t anterior = lote->seq[lote->n - 1];
    if (seq <= anterior || seq - anterior - 1 > 255) break;
    adicionarCandidato(lote, linha, boot, seq);
  }
  f.close();
  return lote->n;
}

// Eventos FIFO: o mais antigo não resolvido (qualquer boot) e os próximos do
// mesmo boot, até max. Um evento novo entra aqui e fura os snapshots.
static int buscarEventos(LoteLinhas* lote, int max) {
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (!ok) return 0;
  char linha[TAMANHO_LINHA + 16];
  char boot[9], alvo[9] = {0};
  uint32_t seq = 0;
  bool temAlvo = false;

  while (lote->n < max && lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) break;
    if (indiceResolvido(boot, seq) >= 0) continue;
    if (tipoDaLinha(linha) != codec::EVENTO) continue;
    if (!temAlvo) {
      memcpy(alvo, boot, 9);
      temAlvo = true;
    } else if (strcmp(boot, alvo) != 0) {
      break;  // mantém a ordem FIFO: outro boot fica para o próximo lote
    }
    if (lote->n > 0) {
      uint32_t anterior = lote->seq[lote->n - 1];
      if (seq <= anterior || seq - anterior - 1 > 255) break;
    }
    adicionarCandidato(lote, linha, boot, seq);
  }
  f.close();
  return lote->n;
}

// Backfill a partir do cursor: inclui o boot antigo (contexto/coordenadas) e
// os snapshots mantidos (seq % N == 0) do mesmo boot, até max. Um boot não é
// dizimado; os snapshots fora da política serão descartados pelo cursor. Para
// no próximo boot ou evento (prioridade já tratada nas etapas anteriores).
static int buscarBackfill(LoteLinhas* lote, int max) {
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (!ok) return 0;
  char linha[TAMANHO_LINHA + 16];
  char boot[9], alvo[9] = {0};
  uint32_t seq = 0;

  while (lote->n < max && lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) break;
    uint8_t tipo = tipoDaLinha(linha);
    if (tipo == 255) break;
    if (indiceResolvido(boot, seq) >= 0) continue;
    if (tipo == codec::EVENTO) break;

    if (tipo == codec::BOOT) {
      if (lote->n > 0 && strcmp(boot, alvo) != 0) break;
      if (lote->n == 0) {
        memcpy(alvo, boot, 9);
        adicionarCandidato(lote, linha, boot, seq);
      }
      continue;
    }

    if ((seq % DIZIMACAO_SNAPSHOTS) != 0) continue;
    if (lote->n == 0) {
      memcpy(alvo, boot, 9);
    } else {
      if (strcmp(boot, alvo) != 0) break;
      uint32_t anterior = lote->seq[lote->n - 1];
      if (seq <= anterior || seq - anterior - 1 > 255) break;
    }
    adicionarCandidato(lote, linha, boot, seq);
  }
  f.close();
  return lote->n;
}

// Escolhe o próximo quadro segundo a prioridade do INC 7B.
static int selecionarLote(LoteLinhas* lote, int max) {
  lote->n = 0;
  g_ancoraSelecionada = false;

  // Valida/reancora o cursor uma vez, antes de decidir a ordem. Se ele sumiu,
  // abrirAposCursor limpa o conjunto e a âncora; a âncora abaixo já vale para
  // a nova base. Evita reancorar no meio de um lote e pular a âncora.
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (f) f.close();
  if (!ok) return 0;

  if (!g_ancoraEnviada) {
    if (buscarAncora(lote)) {
      g_ancoraSelecionada = true;
      return lote->n;
    }
    g_ancoraEnviada = true;  // não há snapshot pendente para ancorar
  }

  avancarCursorResolvido();

  // Reancorou durante o avanço (compactação): escolhe a âncora agora, antes de
  // qualquer backfill, em vez de deixar para a próxima rodada.
  if (!g_ancoraEnviada) {
    if (buscarAncora(lote)) {
      g_ancoraSelecionada = true;
      return lote->n;
    }
    g_ancoraEnviada = true;
  }

  if (coletarBoot(lote, g_bootId, max) > 0) return lote->n;
  if (buscarEventos(lote, max) > 0) return lote->n;
  if (buscarBackfill(lote, max) > 0) return lote->n;
  return 0;
}

static const char* nomeTipoCodec(uint8_t t) {
  return t == codec::EVENTO ? "evento" : t == codec::SNAPSHOT ? "snapshot"
       : t == codec::BOOT ? "boot" : "?";
}

// Tenta enviar o próximo pendente. Sem pendente, libera a varredura; em falha,
// aplica o backoff classificado pelo código HTTP ou pelo erro de rede.
// Tenta enviar um lote de até 6 registros: monta o quadro, o gateway local
// decodifica e posta um JSON por registro; confirma o maior prefixo 2xx e
// avança o cursor até o último confirmado. Em falha, aplica o backoff.
void tentarEnviarLote() {
  // Estático para não somar ~2 KB de pilha ao WiFiClientSecure/HTTPClient.
  static LoteLinhas lote;
  int achadas = selecionarLote(&lote, 6);
  if (achadas == 0) {
    g_temPendente = false;
    g_ancoraEnviada = false;       // próxima drenagem ancora de novo
    g_ancoraSelecionada = false;
    return;
  }

  codec::Quadro q{};
  bool horaSinc[6] = {false};
  int n = 0;
  for (int i = 0; i < achadas; i++) {
    // Registros do mesmo quadro precisam de seq crescente e gap <= 255.
    if (n > 0) {
      uint32_t anterior = lote.seq[i - 1];
      if (lote.seq[i] <= anterior || lote.seq[i] - anterior - 1 > 255) break;
    }
    codec::Quadro uma;
    bool hs = false;
    if (!montarQuadroDaLinha(lote.linha[i], &uma, &hs)) break;
    if (n == 0) {
      q.flags = uma.flags;
      q.jardimIdx = uma.jardimIdx;
      q.seqBase = uma.seqBase;
      q.bootId = uma.bootId;
      q.baseTs = uma.baseTs;
    } else if (uma.bootId != q.bootId ||
               ((uma.flags ^ q.flags) & codec::FLAG_SIMULACAO)) {
      break;  // não mistura boots/origens no mesmo quadro
    } else {
      q.flags |= uma.flags;  // liga coords se algum boot do lote tiver
    }
    q.regs[n] = uma.regs[0];
    horaSinc[n] = hs;
    n++;
  }

  if (n == 0) {
    Serial.println("ENVIO: primeira linha do lote invalida");
    g_ancoraSelecionada = false;
    g_envio.falhas++;
    agendarNovaTentativa(-4);
    return;
  }
  q.n = (uint8_t)n;

  uint8_t quadro[codec::MAX_COM_EXTRAS];
  size_t tam = codec::codificar(q, quadro, sizeof(quadro));
  if (tam == 0) {
    Serial.println("ENVIO: falha ao codificar o lote");
    g_envio.falhas++;
    agendarNovaTentativa(-4);
    return;
  }

#ifdef SIMULATION
  codec::Quadro d;
  if (!codec::decodificar(quadro, tam, &d)) {
    Serial.println("ENVIO: falha ao decodificar o lote");
    g_envio.falhas++;
    agendarNovaTentativa(-4);
    return;
  }

  // Gateway local: um POST por registro, na ordem do quadro. O prefixo 2xx é
  // o que o cursor pode confirmar; o primeiro erro interrompe o lote.
  Serial.printf("ENVIO: lote %d [%lu ms]:", d.n, (unsigned long)millis());
  for (int i = 0; i < d.n; i++) {
    Serial.printf(" %s/%u", nomeTipoCodec(d.regs[i].tipo), (unsigned)d.regs[i].seq);
  }
  Serial.println();

  int confirmados = 0;
  int codigo = 0;
  for (int i = 0; i < d.n; i++) {
    char json[TAMANHO_LINHA];
    if (!montarJsonGateway(d, i, horaSinc[i], json, sizeof(json))) {
      codigo = -4;
      g_envio.falhas++;
      break;
    }
    g_envio.tentativas++;
    codigo = postarLinha(json);
    if (codigo >= 200 && codigo < 300) {
      registrarLatenciaDoRegistro(d.regs[i], horaSinc[i], lote.boot[i]);
      confirmados++;
      continue;
    }
    g_envio.falhas++;
    break;
  }

  if (confirmados > 0) {
    for (int i = 0; i < confirmados; i++) marcarResolvido(lote.boot[i], lote.seq[i]);
    g_envio.confirmados += (uint32_t)confirmados;
    if (g_ancoraSelecionada) g_ancoraEnviada = true;
    g_ancoraSelecionada = false;
    // Pode avançar o cursor por registros já resolvidos/dizimados; grava a NVS
    // no máximo uma vez. O que não for contíguo fica no conjunto em RAM.
    avancarCursorResolvido();
  } else {
    g_ancoraSelecionada = false;
  }

  if (confirmados == d.n) {
    if (g_falhasConsecutivas > 0) {
      Serial.printf("ENVIO: recuperado apos %lu falhas\n", (unsigned long)g_falhasConsecutivas);
    }
    g_falhasConsecutivas = 0;
    g_proximaTentativaMs = 0;
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "nenhum");
    Serial.printf("ENVIO: lote %d/%d 2xx\n", confirmados, d.n);
  } else {
    agendarNovaTentativa(codigo);
    Serial.printf("ENVIO: lote %d/%d 2xx (%s)\n", confirmados, d.n, g_ultimoErro);
  }
#else
  // NÃO COMPILADO NO WOKWI — validar no hardware: enviar o quadro pelo rádio e
  // confirmar o prefixo que o gateway reportar. Sem rádio aqui, não há envio.
  (void)quadro;
  (void)tam;
  g_envio.falhas++;
  agendarNovaTentativa(-4);
#endif
}

// Comando 'o': alterna o modo offline lógico; o arquivo continua crescendo.
void alternarOffline() {
  g_offline = !g_offline;
  if (!g_offline) g_ancoraEnviada = false;  // nova drenagem: ancora de novo
  Serial.printf("ENVIO: modo %s\n", g_offline ? "offline (nao envia)" : "online");
}

// Comando 'u': alterna para uma URL .invalid, exercitando DNS/transporte reais.
// Também zera a espera para o teste não ficar preso no backoff anterior.
void alternarUrlInvalida() {
  g_urlInvalida = !g_urlInvalida;
  g_proximaTentativaMs = 0;
  g_falhasConsecutivas = 0;
  Serial.printf("ENVIO: URL de teste = %s\n",
                g_urlInvalida ? "invalida (dominio .invalid)" : "normal");
}

// Comando 'e': cicla ok -> 500 -> 400 -> timeout, para exercitar o backoff.
void alternarRespostaForcada() {
  switch (g_resposta) {
    case RESP_OK:      g_resposta = RESP_500; break;
    case RESP_500:     g_resposta = RESP_400; break;
    case RESP_400:     g_resposta = RESP_TIMEOUT; break;
    default:           g_resposta = RESP_OK; break;
  }
  const char* nomes[] = {"ok", "500", "400", "timeout"};
  // Trocar de modo tambem zera a espera/backoff, para o teste nao ficar
  // preso nos 5 min de um 400 anterior.
  g_proximaTentativaMs = 0;
  g_falhasConsecutivas = 0;
  Serial.printf("ENVIO: resposta forcada = %s\n", nomes[g_resposta]);
}

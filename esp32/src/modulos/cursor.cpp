// ===== CURSOR DE CONFIRMACAO (INC 4) =========================================

#include "cursor.h"
#include "config.h"
#include "estado.h"
#include "gateway.h"
#include "resolvidos.h"
#include <Preferences.h>

// Lê uma linha do arquivo sem o LF; false no fim. Linha maior que o buffer é
// truncada, mas o excedente é descartado até o LF.
bool lerLinha(File& f, char* buffer, size_t tamanho) {
  size_t n = 0;
  while (f.available()) {
    int c = f.read();
    if (c < 0 || c == '\n') break;
    if (n < tamanho - 1) buffer[n++] = (char)c;
  }
  buffer[n] = '\0';
  return n > 0;
}

// Extrai (bootId, seq) de uma linha gravada; linha parcial/inválida é ignorada.
bool extrairChave(const char* linha, char* bootId, uint32_t* seq) {
  const char* p = strstr(linha, "\"bootId\":\"");
  if (!p || strlen(p + 10) < 8) return false;
  memcpy(bootId, p + 10, 8);
  bootId[8] = '\0';
  p = strstr(linha, "\"seq\":");
  if (!p) return false;
  *seq = (uint32_t)strtoul(p + 6, nullptr, 10);
  return true;
}

// Carrega o cursor da NVS no boot. Ausente = nada foi confirmado ainda.
void carregarCursor() {
  Preferences p;
  if (!p.begin(NVS_ESPACO, true)) return;
  String boot = p.getString(NVS_ACK_BOOT, "");
  g_ackSeq = p.getUInt(NVS_ACK_SEQ, 0);
  p.end();
  if (boot.length() != 8) return;
  memcpy(g_ackBootId, boot.c_str(), 8);
  g_ackBootId[8] = '\0';
  g_ackValido = true;
}

// Conta o que ainda não foi confirmado pela mesma regra da varredura. Se o
// cursor não existir no arquivo, conta tudo (o envio vai reancorar).
uint32_t contarPendentes() {
  File f = LittleFS.open(ARQ_FILA, "r");
  if (!f) return 0;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0, total = 0, depois = 0;
  bool cursorAchado = !g_ackValido;
  while (lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) continue;
    total++;
    if (!cursorAchado) {
      if (strcmp(boot, g_ackBootId) == 0 && seq == g_ackSeq) cursorAchado = true;
      continue;
    }
    depois++;
  }
  f.close();
  return cursorAchado ? depois : total;
}

// Posiciona o arquivo logo após o cursor; false se a chave sumiu.
static bool pularAteCursor(File& f) {
  if (!g_ackValido) return true;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0;
  while (lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) continue;
    if (strcmp(boot, g_ackBootId) == 0 && seq == g_ackSeq) return true;
  }
  return false;
}

// Abre a fila depois do cursor. Se a chave sumiu (compactação), reancora no
// início e limpa o conjunto: os confirmados fora de ordem podem duplicar.
File abrirAposCursor(bool* ok) {
  *ok = false;
  File f = LittleFS.open(ARQ_FILA, "r");
  if (!f) return f;
  if (pularAteCursor(f)) { *ok = true; return f; }
  f.close();
  g_reancoragens++;
  g_resolvidosN = 0;
  g_ancoraEnviada = false;
  g_ackValido = false;       // cursor antigo sumiu: não tenta reencontrá-lo
  g_ackBootId[0] = '\0';
  g_ackSeq = 0;
  Serial.println("ENVIO: cursor nao encontrado; reancorando no inicio");
  f = LittleFS.open(ARQ_FILA, "r");
  *ok = (bool)f;
  return f;
}

// Grava o cursor atual na NVS (uma vez por avanço, não por registro).
static void persistirCursor() {
  if (!g_ackValido) return;
  Preferences p;
  if (!p.begin(NVS_ESPACO, false)) { Serial.println("NVS: falha ao gravar cursor"); return; }
  p.putString(NVS_ACK_BOOT, g_ackBootId);
  p.putUInt(NVS_ACK_SEQ, g_ackSeq);
  p.end();
}

// Avança o cursor enquanto a próxima linha já foi confirmada (fora de ordem)
// ou é um snapshot dizimado. Para no primeiro registro que precisa ser
// enviado (boot, evento, snapshot mantido ou linha inválida).
bool avancarCursorResolvido() {
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (!ok) return false;
  // Se o cursor sumiu, abrirAposCursor reancorou e zerou a âncora. Não descarta
  // snapshots antes de a âncora nova ser escolhida.
  if (!g_ancoraEnviada) { f.close(); return false; }
  bool mudou = false;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0;

  while (lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) break;
    int idx = indiceResolvido(boot, seq);
    if (idx >= 0) {
      removerResolvido(idx);
      memcpy(g_ackBootId, boot, 9);
      g_ackSeq = seq;
      g_ackValido = true;
      mudou = true;
      continue;
    }
    uint8_t tipo = tipoDaLinha(linha);
    if (tipo == codec::SNAPSHOT && (seq % DIZIMACAO_SNAPSHOTS) != 0) {
      g_envio.dizimados++;
      memcpy(g_ackBootId, boot, 9);
      g_ackSeq = seq;
      g_ackValido = true;
      mudou = true;
      continue;
    }
    break;
  }
  f.close();
  if (mudou) persistirCursor();
  return mudou;
}

// Seca automatizada de 20 min: 241 registros e politica real de drenagem 7B.
// Tambem simula uma falha total no primeiro lote para provar que nada e perdido
// e que o mesmo lote (ancora) e retentado.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace codec { enum : uint8_t { EVENTO = 0, SNAPSHOT = 1, BOOT = 2 }; }

constexpr size_t TAMANHO_LINHA = 320;
static const uint8_t DIZIMACAO_SNAPSHOTS = 10;
static const char* ARQ_FILA = "/fila.ndjson";
static const char* CAMPO_TIPO = "tipo";
static const char* TIPO_EVENTO = "evento";
static const char* TIPO_SNAPSHOT = "snapshot";
static const char* TIPO_BOOT = "boot";

static std::string g_conteudo;

struct File {
  bool aberto = false;
  size_t pos = 0;
  explicit operator bool() const { return aberto; }
  int available() { return aberto ? (int)(g_conteudo.size() - pos) : 0; }
  int read() {
    if (!aberto || pos >= g_conteudo.size()) return -1;
    return (unsigned char)g_conteudo[pos++];
  }
  void close() { aberto = false; }
};

struct LittleFSFake {
  File open(const char*, const char*) {
    File f;
    f.aberto = true;
    f.pos = 0;
    return f;
  }
} LittleFS;

struct SerialFake {
  void printf(const char*, ...) {}
  void println(const char*) {}
} Serial;

static char g_bootId[9] = "AAAA0001";
static char g_ackBootId[9] = {0};
static uint32_t g_ackSeq = 0;
static bool g_ackValido = false;
static uint32_t g_reancoragens = 0;

struct ChaveResolvida { char boot[9]; uint32_t seq; };
static ChaveResolvida g_resolvidos[32];
static uint8_t g_resolvidosN = 0;
static bool g_ancoraEnviada = false;
static bool g_ancoraSelecionada = false;
static struct { uint32_t dizimados; } g_envio = {0};

struct LoteLinhas {
  char linha[6][TAMANHO_LINHA + 16];
  char boot[6][9];
  uint32_t seq[6];
  int n;
};

static bool campoTxt(const char* linha, const char* chave, char* out, size_t tam) {
  char padrao[40];
  std::snprintf(padrao, sizeof(padrao), "\"%s\":\"", chave);
  const char* p = std::strstr(linha, padrao);
  if (!p) return false;
  p += std::strlen(padrao);
  const char* fim = std::strchr(p, '"');
  if (!fim || (size_t)(fim - p) >= tam) return false;
  std::memcpy(out, p, (size_t)(fim - p));
  out[fim - p] = '\0';
  return true;
}

static void persistirCursor() {}

#include "drenagem.inc"

static int falhas = 0;

static void checar(bool cond, const char* nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome);
  if (!cond) falhas++;
}

static void linha(const char* tipo, uint32_t seq) {
  char b[300];
  std::snprintf(b, sizeof(b),
                "{\"tipo\":\"%s\",\"bootId\":\"%s\",\"seq\":%lu,"
                "\"timestamp\":\"2026-10-22T12:00:00Z\",\"origem\":\"simulacao\"}\n",
                tipo, g_bootId, (unsigned long)seq);
  g_conteudo += b;
}

static void montarArquivo() {
  g_conteudo.clear();
  linha(TIPO_BOOT, 0);
  for (uint32_t s = 1; s <= 240; s++) {
    bool evento = (s == 10 || s == 50 || s == 100);
    linha(evento ? TIPO_EVENTO : TIPO_SNAPSHOT, s);
  }
}

static void resetarEstado() {
  g_ackBootId[0] = '\0';
  g_ackSeq = 0;
  g_ackValido = false;
  g_reancoragens = 0;
  g_resolvidosN = 0;
  g_ancoraEnviada = false;
  g_ancoraSelecionada = false;
  g_envio.dizimados = 0;
}

struct Enviado { uint8_t tipo; uint32_t seq; int lote; };

struct Resultado {
  std::vector<Enviado> enviados;
  std::set<std::string> chaves;
  int duplicatas = 0;
  int rodadas = 0;
  bool falhouPrimeiro = false;
  uint8_t falhaTipo = 255;
  uint32_t falhaSeq = 0;
  uint32_t dizimadosAposFalha = 0;
  uint32_t dizimados = 0;
  bool ackValido = false;
  uint32_t ackSeq = 0;
  uint8_t resolvidosN = 0;
  uint32_t reancoragens = 0;
};

static Resultado executar(bool falharPrimeira) {
  Resultado r;
  resetarEstado();
  bool falhar = falharPrimeira;

  while (r.rodadas < 100) {
    LoteLinhas lote;
    int n = selecionarLote(&lote, 6);
    if (n == 0) break;

    // Simula um lote inteiro sem nenhum 2xx (500/timeout no primeiro POST).
    if (falhar) {
      falhar = false;
      r.falhouPrimeiro = true;
      r.falhaTipo = tipoDaLinha(lote.linha[0]);
      r.falhaSeq = lote.seq[0];
      r.dizimadosAposFalha = g_envio.dizimados;
      g_ancoraSelecionada = false;
      r.rodadas++;
      continue;
    }

    for (int i = 0; i < n; i++) {
      char chave[24];
      std::snprintf(chave, sizeof(chave), "%s/%lu", lote.boot[i],
                    (unsigned long)lote.seq[i]);
      if (!r.chaves.insert(chave).second) r.duplicatas++;
      r.enviados.push_back({tipoDaLinha(lote.linha[i]), lote.seq[i], r.rodadas});
    }

    // Todos os POSTs do lote foram 2xx.
    for (int i = 0; i < n; i++) marcarResolvido(lote.boot[i], lote.seq[i]);
    if (g_ancoraSelecionada) g_ancoraEnviada = true;
    g_ancoraSelecionada = false;
    avancarCursorResolvido();
    r.rodadas++;
  }

  r.dizimados = g_envio.dizimados;
  r.ackValido = g_ackValido;
  r.ackSeq = g_ackSeq;
  r.resolvidosN = g_resolvidosN;
  r.reancoragens = g_reancoragens;
  return r;
}

static bool temChave(const std::set<std::string>& chaves, uint32_t seq) {
  char b[24];
  std::snprintf(b, sizeof(b), "%s/%lu", g_bootId, (unsigned long)seq);
  return chaves.count(b) == 1;
}

static void validarCenario(const Resultado& r, const char* prefixo,
                           int rodadasEsperadas) {
  char nome[128];
  std::snprintf(nome, sizeof(nome), "%s: 25 registros enviados", prefixo);
  checar(r.enviados.size() == 25, nome);
  std::snprintf(nome, sizeof(nome), "%s: 25 chaves unicas", prefixo);
  checar(r.chaves.size() == 25, nome);
  std::snprintf(nome, sizeof(nome), "%s: sem duplicatas", prefixo);
  checar(r.duplicatas == 0, nome);
  std::snprintf(nome, sizeof(nome), "%s: 216 snapshots dizimados", prefixo);
  checar(r.dizimados == 216, nome);
  std::snprintf(nome, sizeof(nome), "%s: cursor final em 240", prefixo);
  checar(r.ackValido && r.ackSeq == 240, nome);
  std::snprintf(nome, sizeof(nome), "%s: conjunto de resolvidos vazio", prefixo);
  checar(r.resolvidosN == 0, nome);
  std::snprintf(nome, sizeof(nome), "%s: sem reancoragem", prefixo);
  checar(r.reancoragens == 0, nome);
  std::snprintf(nome, sizeof(nome), "%s: %d rodadas", prefixo, rodadasEsperadas);
  checar(r.rodadas == rodadasEsperadas, nome);
  std::snprintf(nome, sizeof(nome), "%s: ancora 240 e o primeiro enviado", prefixo);
  checar(!r.enviados.empty() &&
             r.enviados[0].tipo == codec::SNAPSHOT &&
             r.enviados[0].seq == 240,
         nome);
  std::snprintf(nome, sizeof(nome), "%s: boot e eventos 10/50/100 enviados", prefixo);
  checar(temChave(r.chaves, 0) && temChave(r.chaves, 10) &&
             temChave(r.chaves, 50) && temChave(r.chaves, 100),
         nome);

  bool mantidosOk = true;
  for (uint32_t s = 20; s <= 240; s += 10) {
    if (s == 50 || s == 100) continue;
    if (!temChave(r.chaves, s)) mantidosOk = false;
  }
  std::snprintf(nome, sizeof(nome), "%s: todos os mantidos foram enviados", prefixo);
  checar(mantidosOk, nome);
}

int main() {
  montarArquivo();

  std::printf("== 1. 20 min offline, todos os lotes com 2xx ==\n");
  Resultado ok = executar(false);
  validarCenario(ok, "normal", 6);
  std::printf("contabilidade: 241 gerados = 25 enviados + 216 dizimados\n");

  std::printf("== 2. primeiro lote falha (500/timeout), depois recupera ==\n");
  Resultado retry = executar(true);
  checar(retry.falhouPrimeiro, "primeira tentativa falhou de proposito");
  checar(retry.falhaTipo == codec::SNAPSHOT && retry.falhaSeq == 240,
         "falha ocorreu na ancora 240");
  checar(retry.dizimadosAposFalha == 0,
         "falha nao avancou cursor nem dizimou nada");
  validarCenario(retry, "retry", 7);
  checar(retry.enviados.size() == ok.enviados.size() &&
             retry.chaves == ok.chaves,
         "retry entregou exatamente o mesmo conjunto, sem perda");

  std::printf("== 3. perda do cursor depois de drenar: duplicatas por chave ==\n");
  Resultado reenvio = executar(false);
  int repetidas = 0;
  for (const Enviado& e : reenvio.enviados) {
    char chave[24];
    std::snprintf(chave, sizeof(chave), "%s/%lu", g_bootId,
                  (unsigned long)e.seq);
    if (ok.chaves.count(chave) == 1) repetidas++;
  }
  checar(reenvio.enviados.size() == 25 && reenvio.dizimados == 216,
         "reenvio manteve 25 registros e 216 dizimados");
  checar(reenvio.chaves == ok.chaves && repetidas == 25,
         "as 25 chaves (bootId/seq) identificam as duplicatas");

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

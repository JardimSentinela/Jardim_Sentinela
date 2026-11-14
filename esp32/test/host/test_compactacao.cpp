// Testes da compactacao, recuperacao de cauda e contabilidade de perdas (8A).
// As funcoes reais vem de test/host/.build/compactacao.inc.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static const int SeekSet = 0;

struct File {
  std::string* data = nullptr;
  size_t pos = 0;
  bool aberto = false;
  bool escrita = false;

  operator bool() const { return aberto; }
  size_t size() const { return data ? data->size() : 0; }
  size_t position() const { return pos; }
  int available() const {
    if (!data || pos >= data->size()) return 0;
    return (int)(data->size() - pos);
  }
  bool seek(long off, int modo) {
    if (!data || modo != SeekSet) return false;
    if (off < 0 || (size_t)off > data->size()) return false;
    pos = (size_t)off;
    return true;
  }
  int read() {
    if (!aberto || !data || pos >= data->size()) return -1;
    return (unsigned char)(*data)[pos++];
  }
  int read(uint8_t* buf, size_t n) {
    if (!aberto || !data || pos >= data->size()) return 0;
    size_t disponivel = data->size() - pos;
    if (n > disponivel) n = disponivel;
    std::memcpy(buf, data->data() + pos, n);
    pos += n;
    return (int)n;
  }
  size_t write(const uint8_t* buf, size_t n) {
    if (!data || !escrita) return 0;
    if (pos > data->size()) data->resize(pos, '\0');
    for (size_t i = 0; i < n; i++) {
      if (pos + i < data->size()) (*data)[pos + i] = (char)buf[i];
      else data->push_back((char)buf[i]);
    }
    pos += n;
    return n;
  }
  size_t print(const char* texto) {
    return write((const uint8_t*)texto, std::strlen(texto));
  }
  void close() { aberto = false; }
};

struct FakeLittleFS {
  std::string fila;
  std::string tmp;
  bool temFila = false;
  bool temTmp = false;

  File open(const char* caminho, const char* modo) {
    File f;
    std::string* alvo = nullptr;
    bool ehFila = (std::strcmp(caminho, "/fila.ndjson") == 0);
    bool ehTmp = (std::strcmp(caminho, "/fila.tmp") == 0);
    if (ehFila) {
      if (modo[0] == 'w') { fila.clear(); temFila = true; }
      if (!temFila) { if (modo[0] == 'r') return f; temFila = true; }
      alvo = &fila;
    } else if (ehTmp) {
      if (modo[0] == 'w') { tmp.clear(); temTmp = true; }
      if (!temTmp) { if (modo[0] == 'r') return f; temTmp = true; }
      alvo = &tmp;
    } else {
      return f;
    }
    f.data = alvo;
    f.aberto = true;
    f.escrita = (modo[0] == 'a' || modo[0] == 'w');
    f.pos = (modo[0] == 'a') ? alvo->size() : 0;
    return f;
  }

  bool rename(const char* de, const char* para) {
    if (std::strcmp(de, "/fila.tmp") == 0 &&
        std::strcmp(para, "/fila.ndjson") == 0) {
      fila = tmp;
      temFila = true;
      tmp.clear();
      temTmp = false;
      return true;
    }
    return false;
  }

  bool remove(const char* caminho) {
    if (std::strcmp(caminho, "/fila.ndjson") == 0) {
      fila.clear();
      temFila = false;
      return true;
    }
    if (std::strcmp(caminho, "/fila.tmp") == 0) {
      tmp.clear();
      temTmp = false;
      return true;
    }
    return false;
  }
};

static FakeLittleFS LittleFS;

struct SerialStub {
  template <typename... Args>
  int printf(const char* formato, Args... args) {
    return std::printf(formato, args...);
  }
  void println(const char* texto) { std::printf("%s\n", texto); }
};
static SerialStub Serial;

static const size_t TAMANHO_LINHA = 320;
static const size_t MAX_BYTES_FILA = 64 * 1024;
static const size_t FRACAO_MANTER = 3;
static const char* ARQ_FILA = "/fila.ndjson";
static const char* ARQ_TMP = "/fila.tmp";

static bool arquivoPodeTerLinhaParcial = false;

struct ChaveResolvida { char boot[9]; uint32_t seq; };
static ChaveResolvida g_resolvidos[32];
static uint8_t g_resolvidosN = 0;

static char g_ackBootId[9] = {0};
static uint32_t g_ackSeq = 0;
static bool g_ackValido = false;

static std::atomic<uint32_t> descartados{0};

struct ContadoresEnvio {
  uint32_t tentativas;
  uint32_t confirmados;
  uint32_t falhas;
  uint32_t dizimados;
  uint32_t perdidos;
};
static ContadoresEnvio g_envio = {0, 0, 0, 0, 0};

#include "compactacao.inc"

static int falhasTeste = 0;

static void checar(bool cond, const char* nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome);
  if (!cond) falhasTeste++;
}

static std::string linha(uint32_t seq) {
  char b[96];
  std::snprintf(b, sizeof(b), "{\"bootId\":\"AAAA0001\",\"seq\":%u}", seq);
  std::string s = b;
  if (s.size() < 39) s.resize(39, ' ');
  s += '\n';
  return s;
}

static void montar(uint32_t n) {
  LittleFS.fila.clear();
  for (uint32_t i = 0; i < n; i++) LittleFS.fila += linha(i);
  LittleFS.temFila = true;
}

static uint32_t contarLinhas() {
  uint32_t n = 0;
  for (char c : LittleFS.fila)
    if (c == '\n') n++;
  return n;
}

static void resetar() {
  LittleFS.fila.clear();
  LittleFS.tmp.clear();
  LittleFS.temFila = false;
  LittleFS.temTmp = false;
  descartados = 0;
  g_envio = {0, 0, 0, 0, 0};
  g_resolvidosN = 0;
  g_ackValido = false;
  g_ackBootId[0] = '\0';
  g_ackSeq = 0;
  arquivoPodeTerLinhaParcial = false;
}

static void cursor(uint32_t seq) {
  g_ackValido = true;
  std::strcpy(g_ackBootId, "AAAA0001");
  g_ackSeq = seq;
}

static void resolvido(uint32_t seq) {
  std::strcpy(g_resolvidos[g_resolvidosN].boot, "AAAA0001");
  g_resolvidos[g_resolvidosN].seq = seq;
  g_resolvidosN++;
}

static bool contemSeq(uint32_t seq) {
  char b[64];
  std::snprintf(b, sizeof(b), "\"seq\":%u}", seq);
  return LittleFS.fila.find(b) != std::string::npos;
}

int main() {
  std::printf("== 1. forcada, sem cursor ==\n");
  resetar();
  montar(10);
  checar(compactarFila(true), "compactou");
  checar(descartados.load() == 3, "descartados=3");
  checar(g_envio.perdidos == 3, "perdidos=3");
  checar(contarLinhas() == 7, "restaram 7 linhas");
  checar(!contemSeq(0) && !contemSeq(1) && !contemSeq(2), "removeu seq 0..2");
  checar(contemSeq(3) && contemSeq(9), "manteve seq 3..9");

  std::printf("== 2. cursor no seq 1, removido junto ==\n");
  resetar();
  montar(10);
  cursor(1);
  checar(compactarFila(true), "compactou");
  checar(descartados.load() == 3, "descartados=3");
  checar(g_envio.perdidos == 1, "perdidos=1 (so o seq 2)");

  std::printf("== 3. cursor no seq 2, ultimo removido ==\n");
  resetar();
  montar(10);
  cursor(2);
  checar(compactarFila(true), "compactou");
  checar(descartados.load() == 3, "descartados=3");
  checar(g_envio.perdidos == 0, "perdidos=0");

  std::printf("== 4. cursor no seq 8, preservado ==\n");
  resetar();
  montar(10);
  cursor(8);
  checar(compactarFila(true), "compactou");
  checar(descartados.load() == 3, "descartados=3");
  checar(g_envio.perdidos == 0, "perdidos=0");
  {
    File f = LittleFS.open(ARQ_FILA, "r");
    checar(offsetDoCursor(f) != (size_t)-1, "cursor ainda encontrado");
    f.close();
  }

  std::printf("== 5. cursor ausente do arquivo ==\n");
  resetar();
  montar(10);
  cursor(99);
  checar(compactarFila(true), "compactou");
  checar(descartados.load() == 3, "descartados=3");
  checar(g_envio.perdidos == 3, "perdidos=3");

  std::printf("== 6. cursor 1 e seq 2 resolvido fora de ordem ==\n");
  resetar();
  montar(10);
  cursor(1);
  resolvido(2);
  checar(compactarFila(true), "compactou");
  checar(descartados.load() == 3, "descartados=3");
  checar(g_envio.perdidos == 0, "perdidos=0");

  std::printf("== 7. cursor 0, seq 1 resolvido, seq 2 pendente ==\n");
  resetar();
  montar(10);
  cursor(0);
  resolvido(1);
  checar(compactarFila(true), "compactou");
  checar(descartados.load() == 3, "descartados=3");
  checar(g_envio.perdidos == 1, "perdidos=1 (seq 2)");

  std::printf("== 8. compactacao normal em arquivo pequeno nao mexe ==\n");
  resetar();
  montar(10);
  {
    std::string antes = LittleFS.fila;
    checar(compactarFila(false), "retornou ok");
    checar(LittleFS.fila == antes, "arquivo intacto");
    checar(descartados.load() == 0, "descartados=0");
    checar(g_envio.perdidos == 0, "perdidos=0");
  }

  std::printf("== 9. injecao de linha truncada + recuperacao ==\n");
  resetar();
  montar(2);
  {
    std::string antes = LittleFS.fila;
    injetarLinhaTruncada();
    checar(LittleFS.fila.size() > antes.size(), "arquivo cresceu");
    checar(!LittleFS.fila.empty() && LittleFS.fila.back() != '\n',
           "sem LF no fim");
    checar(arquivoPodeTerLinhaParcial, "flag de recuperacao ligada");
    recuperarLinhaParcial();
    checar(LittleFS.fila == antes, "recuperou exatamente as 2 linhas");
    checar(contarLinhas() == 2, "2 linhas completas");
    checar(g_envio.perdidos == 0, "injecao nao conta como perda");
  }

  std::printf("== 10. duas compactacoes forcadas acumulam ==\n");
  resetar();
  montar(10);
  cursor(1);
  checar(compactarFila(true), "primeira compactou");
  checar(g_envio.perdidos == 1, "perdidos=1 apos a primeira");
  checar(compactarFila(true), "segunda compactou");
  checar(descartados.load() == 5, "descartados=5 no total");
  checar(g_envio.perdidos == 3, "perdidos=3 no total");

  std::printf("== 11. forcada em 2 linhas remove a mais antiga ==\n");
  resetar();
  montar(2);
  checar(compactarFila(true), "compactou");
  checar(descartados.load() == 1, "descartados=1");
  checar(g_envio.perdidos == 1, "perdidos=1");
  checar(contarLinhas() == 1 && contemSeq(1), "sobrou seq 1");

  std::printf("== 12. comando 'f' repara a cauda antes de compactar ==\n");
  resetar();
  montar(10);
  injetarLinhaTruncada();
  arquivoPodeTerLinhaParcial = true;
  forcarCompactacao();
  checar(contarLinhas() == 7, "7 linhas completas depois do 'f'");
  checar(!LittleFS.fila.empty() && LittleFS.fila.back() == '\n',
         "termina em LF");
  checar(descartados.load() == 3, "descartados=3");
  checar(g_envio.perdidos == 3, "perdidos=3");

  std::printf("== 13. compactacao normal no limite de 64 KB ==\n");
  resetar();
  montar(2000);
  size_t antes = LittleFS.fila.size();
  checar(antes > MAX_BYTES_FILA, "arquivo artificial acima de 64 KB");
  checar(compactarFila(false), "compactacao normal executou");
  checar(LittleFS.fila.size() <= (MAX_BYTES_FILA / 4) * FRACAO_MANTER + 40,
         "sobrou <= 3/4 do limite + uma linha");
  checar(LittleFS.fila.size() < antes, "arquivo diminuiu");
  checar(contemSeq(1999) && !contemSeq(0), "manteve os novos, removeu os antigos");
  checar(descartados.load() > 0, "descartados contabilizados");
  checar(g_envio.perdidos == descartados.load(),
         "sem cursor, todos os removidos sao nao confirmados");

  std::printf("\n%s\n", falhasTeste == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhasTeste == 0 ? 0 : 1;
}

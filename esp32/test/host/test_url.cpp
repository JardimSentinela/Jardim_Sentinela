// Comando 'u': alterna a URL invalida e zera o backoff (8C).
#include <cstdarg>
#include <cstdint>
#include <cstdio>

struct SerialFake {
  void printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::vprintf(fmt, ap);
    va_end(ap);
  }
} Serial;

static bool g_urlInvalida = false;
static uint32_t g_proximaTentativaMs = 1234;
static uint32_t g_falhasConsecutivas = 3;

#include "url.inc"

static int falhas = 0;

static void checar(bool cond, const char* nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome);
  if (!cond) falhas++;
}

int main() {
  alternarUrlInvalida();
  checar(g_urlInvalida, "primeiro 'u' ativa URL invalida");
  checar(g_proximaTentativaMs == 0, "primeiro 'u' zera a espera");
  checar(g_falhasConsecutivas == 0, "primeiro 'u' zera as falhas");

  g_proximaTentativaMs = 999;
  g_falhasConsecutivas = 4;
  alternarUrlInvalida();
  checar(!g_urlInvalida, "segundo 'u' volta para a URL normal");
  checar(g_proximaTentativaMs == 0, "segundo 'u' zera a espera");
  checar(g_falhasConsecutivas == 0, "segundo 'u' zera as falhas");

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

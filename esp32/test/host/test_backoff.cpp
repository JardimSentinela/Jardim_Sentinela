// Backoff exponencial e classificacao de falhas (INC 5/6/8C).
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint32_t g_agoraMs = 100000;
static uint32_t millis() { return g_agoraMs; }

struct SerialFake {
  void printf(const char*, ...) {}
} Serial;

static uint32_t g_falhasConsecutivas = 0;
static uint32_t g_retryAfterSeg = 0;
static uint32_t g_proximaTentativaMs = 0;
static char g_ultimoErro[40] = "nenhum";

#include "backoff.inc"

static int falhas = 0;

static void checar(bool cond, const char* nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome);
  if (!cond) falhas++;
}

static void resetar() {
  g_falhasConsecutivas = 0;
  g_retryAfterSeg = 0;
  g_proximaTentativaMs = 0;
  std::strcpy(g_ultimoErro, "nenhum");
}

static bool espera(uint32_t valor) {
  return g_proximaTentativaMs == g_agoraMs + valor;
}

int main() {
  std::printf("== 1. backoff transitorio ==\n");
  checar(backoffMs(1) == 2000, "falha 1 = 2 s");
  checar(backoffMs(2) == 4000, "falha 2 = 4 s");
  checar(backoffMs(3) == 8000, "falha 3 = 8 s");
  checar(backoffMs(4) == 16000, "falha 4 = 16 s");
  checar(backoffMs(5) == 32000, "falha 5 = 32 s");
  checar(backoffMs(6) == 60000, "falha 6 = 60 s");
  checar(backoffMs(50) == 60000, "falha 50 = 60 s (teto)");

  std::printf("== 2. backoff 429 ==\n");
  checar(backoff429Ms(1) == 30000, "429 1 = 30 s");
  checar(backoff429Ms(2) == 60000, "429 2 = 60 s");
  checar(backoff429Ms(3) == 120000, "429 3 = 120 s");
  checar(backoff429Ms(4) == 240000, "429 4 = 240 s");
  checar(backoff429Ms(5) == 300000, "429 5 = 300 s");
  checar(backoff429Ms(99) == 300000, "429 99 = 300 s (teto)");

  std::printf("== 3. classificacao de falhas ==\n");
  resetar();
  agendarNovaTentativa(500);
  checar(g_falhasConsecutivas == 1 && espera(2000), "500 -> 2 s");
  checar(std::strcmp(g_ultimoErro, "http 500") == 0, "500 -> erro visivel");

  resetar();
  g_retryAfterSeg = 1;
  agendarNovaTentativa(429);
  checar(espera(5000), "429 Retry-After 1 -> piso de 5 s");
  checar(std::strcmp(g_ultimoErro, "http 429 (Retry-After)") == 0,
         "429 Retry-After -> erro visivel");

  resetar();
  g_retryAfterSeg = 120;
  agendarNovaTentativa(429);
  checar(espera(120000), "429 Retry-After 120 -> 120 s");

  resetar();
  agendarNovaTentativa(429);
  checar(espera(30000), "429 sem header -> 30 s");

  resetar();
  agendarNovaTentativa(400);
  checar(espera(300000), "400 -> 5 min");
  checar(std::strcmp(g_ultimoErro, "http 400 (contrato)") == 0,
         "400 -> erro de contrato");

  resetar();
  agendarNovaTentativa(-2);
  checar(espera(60000), "URL vazia -> 60 s");
  checar(std::strcmp(g_ultimoErro, "config: URL vazia") == 0,
         "URL vazia -> erro visivel");

  resetar();
  agendarNovaTentativa(-4);
  checar(espera(60000), "codec invalido -> 60 s");
  checar(std::strcmp(g_ultimoErro, "codec: linha invalida") == 0,
         "codec invalido -> erro visivel");

  resetar();
  agendarNovaTentativa(-1);
  checar(espera(2000), "timeout/rede -> 2 s");
  checar(std::strcmp(g_ultimoErro, "rede/timeout") == 0,
         "timeout/rede -> erro visivel");

  resetar();
  agendarNovaTentativa(503);
  checar(espera(2000), "503 -> 2 s");
  checar(std::strcmp(g_ultimoErro, "http 503") == 0, "503 -> erro visivel");

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

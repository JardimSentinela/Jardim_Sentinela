// p50/p95 de latencia criacao->2xx, com filtros de boot/hora (8B).
#include <cstdint>
#include <cstdio>
#include <cstring>

// A funcao real chama time(nullptr); aqui o relogio e controlado pelo teste.
typedef long time_t;
static time_t g_agoraFake = 0;
static time_t tempoFake(time_t* p) {
  if (p) *p = g_agoraFake;
  return g_agoraFake;
}
#define time(x) tempoFake(x)

namespace codec {
struct Registro { uint32_t ts; };
}

static const time_t EPOCH_MINIMO_VALIDO = 1700000000;

static const uint8_t LATENCIAS_MAX = 128;
static uint32_t g_latSeg[LATENCIAS_MAX];
static uint8_t g_latN = 0;
static uint8_t g_latProx = 0;
static uint32_t g_latDesconhecidas = 0;
static uint32_t g_latForaBoot = 0;
static char g_bootId[9] = "AAAA0001";

#include "latencia.inc"

static int falhas = 0;

static void checar(bool cond, const char* nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome);
  if (!cond) falhas++;
}

int main() {
  std::printf("== 1. sem amostras ==\n");
  zerarLatencia();
  checar(g_latN == 0, "n=0");
  checar(percentilLatencia(50) == 0, "p50=0");
  checar(percentilLatencia(95) == 0, "p95=0");

  std::printf("== 2. 1..100: p50/p95 nearest-rank ==\n");
  for (uint32_t i = 1; i <= 100; i++) registrarLatencia(i);
  checar(g_latN == 100, "n=100");
  checar(percentilLatencia(50) == 50, "p50=50");
  checar(percentilLatencia(95) == 95, "p95=95");

  std::printf("== 3. anel de 128: ultimas amostras ==\n");
  for (uint32_t i = 101; i <= 200; i++) registrarLatencia(i);
  checar(g_latN == 128, "n=128 (anel cheio)");
  checar(percentilLatencia(50) == 136, "p50=136");
  checar(percentilLatencia(95) == 194, "p95=194");

  std::printf("== 4. filtros de boot e horaSincronizada ==\n");
  zerarLatencia();
  g_agoraFake = 1800000000;
  codec::Registro r{1800000000};
  registrarLatenciaDoRegistro(r, false, g_bootId);
  checar(g_latN == 0 && g_latDesconhecidas == 1,
         "horaSinc=false -> desconhecida");
  registrarLatenciaDoRegistro(r, true, "BBBB0002");
  checar(g_latN == 0 && g_latForaBoot == 1, "boot antigo -> foraBoot");
  g_agoraFake = 1800000100;
  r.ts = 1800000000;
  registrarLatenciaDoRegistro(r, true, g_bootId);
  checar(g_latN == 1 && g_latSeg[0] == 100, "boot corrente: 100 s");
  g_agoraFake = 1800000000;
  r.ts = 1800000100;
  registrarLatenciaDoRegistro(r, true, g_bootId);
  checar(g_latN == 2 && g_latSeg[1] == 0, "timestamp futuro -> 0 s");
  g_agoraFake = 1600000000;
  r.ts = 1599999900;
  registrarLatenciaDoRegistro(r, true, g_bootId);
  checar(g_latDesconhecidas == 2,
         "relogio invalido no 2xx -> desconhecida");
  g_agoraFake = 1800000000;
  r.ts = 1600000000;
  registrarLatenciaDoRegistro(r, true, g_bootId);
  checar(g_latDesconhecidas == 3, "timestamp invalido -> desconhecida");

  std::printf("== 5. zerarLatencia limpa tudo ==\n");
  zerarLatencia();
  checar(g_latN == 0 && g_latProx == 0 && g_latDesconhecidas == 0 &&
             g_latForaBoot == 0,
         "contadores zerados");

  std::printf("== 6. duas amostras ==\n");
  zerarLatencia();
  registrarLatencia(10);
  registrarLatencia(20);
  checar(percentilLatencia(50) == 10, "p50=10");
  checar(percentilLatencia(95) == 20, "p95=20");

  std::printf("== 7. 128 amostras iguais ==\n");
  zerarLatencia();
  for (int i = 0; i < 128; i++) registrarLatencia(42);
  checar(percentilLatencia(50) == 42, "p50=42");
  checar(percentilLatencia(95) == 42, "p95=42");

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

// ===== LATENCIA CRIACAO -> 2XX (INC 8B) ======================================

#include "latencia.h"
#include "config.h"
#include "estado.h"

#include <cstring>
#include <time.h>

// Anel das ultimas confirmacoes do boot corrente. Os demais casos sao
// contados como desconhecidos (horaSincronizada:false) ou fora do boot.
static uint32_t g_latSeg[LATENCIAS_MAX];
static uint8_t  g_latN = 0;
static uint8_t  g_latProx = 0;
static uint32_t g_latDesconhecidas = 0;
static uint32_t g_latForaBoot = 0;

void zerarLatencia() {
  g_latN = 0;
  g_latProx = 0;
  g_latDesconhecidas = 0;
  g_latForaBoot = 0;
}

// Guarda uma amostra de latencia (segundos) no anel; sobrescreve a mais antiga.
void registrarLatencia(uint32_t segundos) {
  g_latSeg[g_latProx] = segundos;
  g_latProx = (uint8_t)((g_latProx + 1) % LATENCIAS_MAX);
  if (g_latN < LATENCIAS_MAX) g_latN++;
}

// Classifica a confirmacao de um registro e, quando aplicavel, mede
// agora_epoch - timestamp_epoch. So entra no p50/p95 o boot corrente com
// horaSincronizada:true; os demais sao contados e ficam como desconhecidos.
void registrarLatenciaDoRegistro(const codec::Registro& r, bool horaSinc, const char* boot) {
  if (!horaSinc) { g_latDesconhecidas++; return; }
  if (strcmp(boot, g_bootId) != 0) { g_latForaBoot++; return; }
  time_t agora = time(nullptr);
  if (agora < EPOCH_MINIMO_VALIDO || r.ts < (uint32_t)EPOCH_MINIMO_VALIDO) {
    g_latDesconhecidas++;
    return;
  }
  uint32_t seg = (agora >= (time_t)r.ts) ? (uint32_t)(agora - (time_t)r.ts) : 0;
  registrarLatencia(seg);
}

// Percentil por posto mais proximo (nearest-rank). Devolve 0 sem amostras.
uint32_t percentilLatencia(uint8_t p) {
  if (g_latN == 0) return 0;
  uint32_t copia[LATENCIAS_MAX];
  memcpy(copia, g_latSeg, (size_t)g_latN * sizeof(uint32_t));
  for (uint8_t i = 1; i < g_latN; i++) {
    uint32_t chave = copia[i];
    int j = (int)i - 1;
    while (j >= 0 && copia[j] > chave) {
      copia[j + 1] = copia[j];
      j--;
    }
    copia[j + 1] = chave;
  }
  uint32_t idx = ((uint32_t)g_latN * p + 99) / 100;
  if (idx == 0) idx = 1;
  if (idx > g_latN) idx = g_latN;
  return copia[idx - 1];
}

uint8_t latenciaAmostras() { return g_latN; }
uint32_t latenciaDesconhecidas() { return g_latDesconhecidas; }
uint32_t latenciaForaBoot() { return g_latForaBoot; }

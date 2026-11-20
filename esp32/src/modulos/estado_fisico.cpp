// ===== ESTADO FISICO DO JARDIM ==============================================

#include "estado_fisico.h"

#include <math.h>

// Limite inferior (cm) da faixa do estado. NORMAL nao tem limite inferior.
static float limiteInferior(EstadoFisico estado) {
  switch (estado) {
    case EstadoFisico::ENCHENDO:      return LIMITE_ENCHENDO_CM;
    case EstadoFisico::TRANSBORDANDO: return LIMITE_TRANSBORDANDO_CM;
    case EstadoFisico::ALAGADO:       return LIMITE_ALAGADO_CM;
    default:                          return 0.0f;
  }
}

EstadoFisico classificarEstado(float nivelCm, EstadoFisico anterior) {
  if (!isfinite(nivelCm) || nivelCm < 0.0f) return anterior;

  uint8_t atual = (uint8_t)anterior;
  const uint8_t ultimo = (uint8_t)EstadoFisico::ALAGADO;

  // Subida: imediata, ao atingir o limite do proximo estado.
  while (atual < ultimo && nivelCm >= limiteInferior((EstadoFisico)(atual + 1))) atual++;

  // Descida: so abaixo do limite do estado atual menos a folga.
  while (atual > (uint8_t)EstadoFisico::NORMAL &&
         nivelCm < limiteInferior((EstadoFisico)atual) - FOLGA_HISTERESE_CM) {
    atual--;
  }
  return (EstadoFisico)atual;
}

const char* nomeEstado(EstadoFisico estado) {
  switch (estado) {
    case EstadoFisico::ENCHENDO:      return "ENCHENDO";
    case EstadoFisico::TRANSBORDANDO: return "TRANSBORDANDO";
    case EstadoFisico::ALAGADO:       return "ALAGADO";
    default:                          return "NORMAL";
  }
}

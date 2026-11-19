// ===== SINALIZACAO DO TOTEM =================================================

#include "sinalizacao.h"

bool somPermitido(EstadoFisico estado, TendenciaLocal tendencia) {
  if (estado == EstadoFisico::ALAGADO) return true;
  return estado == EstadoFisico::TRANSBORDANDO && tendencia == TendenciaLocal::SUBINDO;
}

// Verdadeiro dentro de um dos pulsos da rajada, no instante do ciclo.
static bool dentroDeUmPulso(uint32_t instanteNoCiclo) {
  for (uint32_t i = 0; i < SOM_PULSOS_POR_RAJADA; i++) {
    const uint32_t inicio = i * (SOM_PULSO_MS + SOM_PAUSA_PULSOS_MS);
    if (instanteNoCiclo >= inicio && instanteNoCiclo < inicio + SOM_PULSO_MS) return true;
  }
  return false;
}

SaidaSinalizacao calcularSinalizacao(EstadoFisico estado, TendenciaLocal tendencia,
                                     uint32_t agoraMs) {
  SaidaSinalizacao saida = {false, false, false, false};

  switch (estado) {
    case EstadoFisico::NORMAL:
      saida.ledVerde = true;
      break;
    case EstadoFisico::ENCHENDO:
      saida.ledAmarelo = true;
      break;
    case EstadoFisico::TRANSBORDANDO:
      saida.ledVermelho = true;
      break;
    case EstadoFisico::ALAGADO:
      saida.ledVermelho = (agoraMs % PISCA_PERIODO_MS) < PISCA_PERIODO_MS / 2;
      break;
  }

  saida.buzzer = somPermitido(estado, tendencia) && dentroDeUmPulso(agoraMs % SOM_CICLO_MS);
  return saida;
}

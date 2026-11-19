#ifndef SINALIZACAO_H
#define SINALIZACAO_H

// Sinalizacao do totem (SW-13): qual LED acende e quando o buzzer soa.
// C++ puro, sem Arduino: roda no host. Regras: docs/decisoes-tecnicas.md §4.
// Funciona sem internet: depende so do estado fisico e da tendencia locais.
// O som indica que as pessoas devem se afastar da area do jardim. Nao e sirene
// nem alerta oficial; o padrao e distinto de qualquer sirene (nunca continuo).
//
// O codigo Arduino (totem.cpp) so liga/desliga os pinos conforme a saida
// calculada aqui.

#include <stdint.h>

#include "estado_fisico.h"
#include "tendencia_local.h"

struct SaidaSinalizacao {
  bool ledVerde;
  bool ledAmarelo;
  bool ledVermelho;
  bool buzzer;
};

// PROVISORIO, ver docs/decisoes-tecnicas.md §4.1 (a definir na implementacao):
// no ALAGADO o vermelho pisca (aceso na primeira metade do periodo).
constexpr uint32_t PISCA_PERIODO_MS = 1000;

// PROVISORIO, ver docs/decisoes-tecnicas.md §4.2 (a confirmar): rajada de tres
// pulsos curtos repetida em intervalo fixo. Pulsos em [0,150) [300,450)
// [600,750) de cada ciclo de 2000 ms.
constexpr uint32_t SOM_PULSOS_POR_RAJADA = 3;
constexpr uint32_t SOM_PULSO_MS          = 150;
constexpr uint32_t SOM_PAUSA_PULSOS_MS   = 150;
constexpr uint32_t SOM_CICLO_MS          = 2000;

// Politica: o som so e permitido no ALAGADO (sempre) e no TRANSBORDANDO com
// tendencia SUBINDO. Nos demais estados, e com qualquer outra tendencia, calado.
bool somPermitido(EstadoFisico estado, TendenciaLocal tendencia);

// Saida dos LEDs e do buzzer no instante agoraMs (millis() do dispositivo).
// Sem estado interno: o padrao depende so de agoraMs. Se o som comecar no meio
// de um ciclo, o primeiro pulso pode sair parcial.
SaidaSinalizacao calcularSinalizacao(EstadoFisico estado, TendenciaLocal tendencia,
                                     uint32_t agoraMs);

#endif

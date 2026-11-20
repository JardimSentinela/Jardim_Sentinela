#ifndef ESTADO_FISICO_H
#define ESTADO_FISICO_H

// Estado fisico do jardim (SW-13). C++ puro, sem Arduino: roda no host.
// Faixas e histerese: docs/decisoes-tecnicas.md §3 e §3.1.
// "estado" aqui e o nivel de agua. Nao e alerta oficial.

#include <stdint.h>

enum class EstadoFisico : uint8_t { NORMAL = 0, ENCHENDO = 1, TRANSBORDANDO = 2, ALAGADO = 3 };

// PROVISORIO, ver docs/decisoes-tecnicas.md §3: limites em cm de nivel do
// jardim fisico. O estado sobe ao ATINGIR o limite.
constexpr float LIMITE_ENCHENDO_CM      = 22.0f;
constexpr float LIMITE_TRANSBORDANDO_CM = 45.0f;
constexpr float LIMITE_ALAGADO_CM       = 60.0f;

// PROVISORIO, ver docs/decisoes-tecnicas.md §3.1 (a confirmar): o estado so
// desce quando o nivel cai abaixo de (limite - folga).
constexpr float FOLGA_HISTERESE_CM = 2.0f;

// PROVISORIO: versao da regra gravada junto de cada registro (§5.1).
constexpr const char* VERSAO_REGRA = "v1-provisoria-2026-11";

// Classifica o nivel (cm) usando o estado anterior para aplicar a histerese.
// Nivel invalido (NaN, infinito ou negativo) mantem o estado anterior.
EstadoFisico classificarEstado(float nivelCm, EstadoFisico anterior);

// "NORMAL", "ENCHENDO", "TRANSBORDANDO" ou "ALAGADO".
const char* nomeEstado(EstadoFisico estado);

#endif

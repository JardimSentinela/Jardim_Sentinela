#ifndef EVENTO_H
#define EVENTO_H

// Registro criado pela tarefa de amostragem e gravado na fila. Header puro (sem
// Arduino), para os testes de host usarem a mesma struct do firmware.

#include <stdint.h>

#include "codec_quadro.h"

// Tipo do registro gravado no arquivo.
enum TipoRegistro : uint8_t { REG_EVENTO = 0, REG_SNAPSHOT = 1, REG_BOOT = 2 };

struct Evento {
  TipoRegistro tipo;
  uint8_t  sensor;
  float    nivelAgua;
  uint32_t deltaTMs;
  uint32_t tMs;
  uint32_t seq;           // atribuido na criacao do registro (deixa buraco se perder)
  bool     relogioValido; // estado do relogio no momento em que o registro nasceu
  // Classificacao do registro: ordinais de EstadoFisico e de TendenciaLocal, ou
  // codec::SEM_CLASSIFICACAO (0xFF). Sem valor padrao de proposito: quem cria o
  // Evento informa os dois (um 0 omitido viraria NORMAL/SUBINDO sem ninguem ver).
  uint8_t  estado;
  uint8_t  tendencia;
};

#endif

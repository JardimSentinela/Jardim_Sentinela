#ifndef CLASSIFICADOR_SONDA_H
#define CLASSIFICADOR_SONDA_H

// Classificador da sonda HW-02 (SW-13/SW-14): junta o estado fisico (com
// histerese) e a tendencia local de um jardim. C++ puro, sem Arduino.
//  - O estado vem da sonda (continua, 0-100 cm), nao dos T1592 (decisoes §8,
//    mudanca a confirmar pelo grupo). Sem leitura da sonda, o estado e 0xFF.
//  - A tendencia usa as leituras de SNAPSHOT (intervalo regular): a idade maxima
//    e 3 x o intervalo de snapshot (§5.2). Leituras da sonda a cada 500 ms nao
//    entram no historico.

#include <stdint.h>

#include "codec_quadro.h"
#include "estado_fisico.h"
#include "tendencia_local.h"

struct ClassificadorSonda {
  EstadoFisico estado;
  bool temEstado;               // ja houve leitura valida da sonda
  HistoricoTendencia historico; // ultimas leituras de snapshot
  uint32_t bootId;              // boot atual (so as leituras dele contam)
  uint32_t idadeMaximaMs;       // 3 x PERIODO_SNAPSHOT_MS
};

void iniciarClassificador(ClassificadorSonda& c, uint32_t bootId, uint32_t idadeMaximaMs);

// Leitura da sonda (cm): atualiza o estado com histerese. Leitura invalida
// (NaN, infinita, negativa) e ignorada.
void classificarLeituraSonda(ClassificadorSonda& c, float nivelCm);

// Registra a leitura de um snapshot no historico da tendencia.
void registrarSnapshotSonda(ClassificadorSonda& c, uint32_t seq, uint32_t tMs, float nivelCm);

// Valores prontos para gravar no registro: ordinal do estado ou 0xFF (sem leitura).
uint8_t estadoParaRegistro(const ClassificadorSonda& c);

// Ordinal da tendencia no instante agoraMs.
uint8_t tendenciaParaRegistro(const ClassificadorSonda& c, uint32_t agoraMs);

#endif

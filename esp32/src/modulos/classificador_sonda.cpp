// ===== CLASSIFICADOR DA SONDA ===============================================

#include "classificador_sonda.h"

#include <math.h>

void iniciarClassificador(ClassificadorSonda& c, uint32_t bootId, uint32_t idadeMaximaMs) {
  HistoricoTendencia vazio = {};
  c.estado = EstadoFisico::NORMAL;
  c.temEstado = false;
  c.historico = vazio;
  c.bootId = bootId;
  c.idadeMaximaMs = idadeMaximaMs;
}

void classificarLeituraSonda(ClassificadorSonda& c, float nivelCm) {
  if (!isfinite(nivelCm) || nivelCm < 0.0f) return;
  c.estado = classificarEstado(nivelCm, c.estado);
  c.temEstado = true;
}

void registrarSnapshotSonda(ClassificadorSonda& c, uint32_t seq, uint32_t tMs, float nivelCm) {
  registrarLeituraTendencia(c.historico, LeituraTendencia{c.bootId, seq, tMs, nivelCm});
}

uint8_t estadoParaRegistro(const ClassificadorSonda& c) {
  return c.temEstado ? (uint8_t)c.estado : codec::SEM_CLASSIFICACAO;
}

uint8_t tendenciaParaRegistro(const ClassificadorSonda& c, uint32_t agoraMs) {
  return (uint8_t)tendenciaDoHistorico(c.historico, c.bootId, agoraMs, c.idadeMaximaMs);
}

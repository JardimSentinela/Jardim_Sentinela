#ifndef TENDENCIA_LOCAL_H
#define TENDENCIA_LOCAL_H

// Tendencia local do nivel (SW-14, parte do firmware). C++ puro, sem Arduino:
// roda no host. Regra: docs/decisoes-tecnicas.md §5.2. Funciona sem internet.
// Tendencia NAO e previsao e NAO e alerta: so mostra a direcao recente.

#include <stddef.h>
#include <stdint.h>

enum class TendenciaLocal : uint8_t {
  SUBINDO = 0,
  ESTAVEL = 1,
  DESCENDO = 2,
  INSUFICIENTE = 3,   // menos de JANELA_TENDENCIA leituras validas no boot atual
  INDETERMINADO = 4,  // reservado: a ordem por seq elimina empates no firmware
  DESATUALIZADO = 5   // ultima leitura valida mais velha que a idade maxima
};

// Uma leitura do jardim. seq e tMs recomecam a cada boot (bootId); por isso
// NAO sao comparaveis entre boots.
struct LeituraTendencia {
  uint32_t bootId;
  uint32_t seq;
  uint32_t tMs;      // millis() do dispositivo no boot em que a leitura nasceu
  float    nivelCm;
};

// PROVISORIO, ver docs/decisoes-tecnicas.md §5.2: janela de leituras VALIDAS
// (nivel finito e nao negativo).
constexpr size_t JANELA_TENDENCIA = 5;

// PROVISORIO, ver docs/decisoes-tecnicas.md §5.2: variacao = ultima - primeira
// da janela. Acima de +tolerancia = SUBINDO; abaixo de -tolerancia = DESCENDO;
// no limite exato (inclusive) = ESTAVEL.
constexpr float TOLERANCIA_TENDENCIA_CM = 1.5f;

// PROVISORIO, ver docs/decisoes-tecnicas.md §5.2: idade maxima = 3 x intervalo
// de snapshot (15 s na simulacao, 180 s no hardware).
constexpr uint32_t FATOR_IDADE_MAXIMA = 3;
constexpr uint32_t idadeMaximaTendenciaMs(uint32_t periodoSnapshotMs) {
  return FATOR_IDADE_MAXIMA * periodoSnapshotMs;
}

// Calcula a tendencia.
//  - So entram leituras de bootIdAtual (o chamador sabe qual e); leituras de
//    outro boot sao ignoradas, mesmo as que chegam depois.
//  - A ordem e pelo seq (nao pela posicao no vetor). seq repetido nao e tratado.
//  - A janela e formada pelas JANELA_TENDENCIA ultimas leituras validas.
//  - Idade = agoraMs - tMs da ultima leitura VALIDA da janela (relogio do
//    proprio dispositivo, dentro do boot). Idade igual ao maximo nao e velha.
//  - Precedencia: INSUFICIENTE > DESATUALIZADO > INDETERMINADO > direcao.
// Nao aloca memoria: guarda so as JANELA_TENDENCIA ultimas leituras.
TendenciaLocal calcularTendenciaLocal(const LeituraTendencia* leituras, size_t n,
                                      uint32_t bootIdAtual, uint32_t agoraMs,
                                      uint32_t idadeMaximaMs);

// "SUBINDO", "ESTAVEL", "DESCENDO", "INSUFICIENTE", "INDETERMINADO" ou "DESATUALIZADO".
const char* nomeTendencia(TendenciaLocal tendencia);

// Historico de tamanho fixo: guarda so as JANELA_TENDENCIA leituras mais
// recentes, da mais antiga para a mais nova. Inicialize com "= {}".
struct HistoricoTendencia {
  LeituraTendencia itens[JANELA_TENDENCIA];
  size_t n;
};

// Acrescenta a leitura; com o historico cheio, descarta a mais antiga.
void registrarLeituraTendencia(HistoricoTendencia& historico, const LeituraTendencia& leitura);

// Tendencia calculada sobre o historico (mesma regra de calcularTendenciaLocal).
TendenciaLocal tendenciaDoHistorico(const HistoricoTendencia& historico, uint32_t bootIdAtual,
                                    uint32_t agoraMs, uint32_t idadeMaximaMs);

#endif

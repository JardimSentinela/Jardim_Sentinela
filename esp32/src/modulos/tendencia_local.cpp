// ===== TENDENCIA LOCAL ======================================================

#include "tendencia_local.h"

#include <math.h>

static bool leituraValida(const LeituraTendencia& l) {
  return isfinite(l.nivelCm) && l.nivelCm >= 0.0f;
}

TendenciaLocal calcularTendenciaLocal(const LeituraTendencia* leituras, size_t n,
                                      uint32_t bootIdAtual, uint32_t agoraMs,
                                      uint32_t idadeMaximaMs) {
  // Guarda as JANELA_TENDENCIA leituras validas de maior seq, em ordem crescente.
  LeituraTendencia janela[JANELA_TENDENCIA];
  size_t guardadas = 0;

  if (leituras != nullptr) {
    for (size_t i = 0; i < n; i++) {
      const LeituraTendencia& l = leituras[i];
      if (l.bootId != bootIdAtual || !leituraValida(l)) continue;

      if (guardadas < JANELA_TENDENCIA) {
        // Ainda cabe: entra no fim e desce ate a posicao certa.
        size_t k = guardadas++;
        janela[k] = l;
        for (; k > 0 && janela[k - 1].seq > janela[k].seq; k--) {
          LeituraTendencia troca = janela[k - 1];
          janela[k - 1] = janela[k];
          janela[k] = troca;
        }
      } else if (l.seq > janela[0].seq) {
        // Cheia: substitui a de menor seq e sobe ate a posicao certa.
        janela[0] = l;
        for (size_t k = 0; k + 1 < JANELA_TENDENCIA && janela[k].seq > janela[k + 1].seq; k++) {
          LeituraTendencia troca = janela[k];
          janela[k] = janela[k + 1];
          janela[k + 1] = troca;
        }
      }
    }
  }

  if (guardadas < JANELA_TENDENCIA) return TendenciaLocal::INSUFICIENTE;

  // Idade pelo tMs do proprio dispositivo (so dentro do boot atual). Se "agora"
  // for anterior ao tMs (relogio inconsistente), nao conta como velha.
  const uint32_t idade = agoraMs - janela[JANELA_TENDENCIA - 1].tMs;
  if ((int32_t)idade >= 0 && idade > idadeMaximaMs) return TendenciaLocal::DESATUALIZADO;

  const float variacao = janela[JANELA_TENDENCIA - 1].nivelCm - janela[0].nivelCm;
  if (variacao > TOLERANCIA_TENDENCIA_CM) return TendenciaLocal::SUBINDO;
  if (variacao < -TOLERANCIA_TENDENCIA_CM) return TendenciaLocal::DESCENDO;
  return TendenciaLocal::ESTAVEL;
}

const char* nomeTendencia(TendenciaLocal tendencia) {
  switch (tendencia) {
    case TendenciaLocal::SUBINDO:       return "SUBINDO";
    case TendenciaLocal::DESCENDO:      return "DESCENDO";
    case TendenciaLocal::INSUFICIENTE:  return "INSUFICIENTE";
    case TendenciaLocal::INDETERMINADO: return "INDETERMINADO";
    case TendenciaLocal::DESATUALIZADO: return "DESATUALIZADO";
    default:                            return "ESTAVEL";
  }
}

void registrarLeituraTendencia(HistoricoTendencia& historico, const LeituraTendencia& leitura) {
  if (historico.n < JANELA_TENDENCIA) {
    historico.itens[historico.n++] = leitura;
    return;
  }
  for (size_t i = 1; i < JANELA_TENDENCIA; i++) historico.itens[i - 1] = historico.itens[i];
  historico.itens[JANELA_TENDENCIA - 1] = leitura;
}

TendenciaLocal tendenciaDoHistorico(const HistoricoTendencia& historico, uint32_t bootIdAtual,
                                    uint32_t agoraMs, uint32_t idadeMaximaMs) {
  return calcularTendenciaLocal(historico.itens, historico.n, bootIdAtual, agoraMs, idadeMaximaMs);
}

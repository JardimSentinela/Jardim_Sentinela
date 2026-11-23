package br.edu.cesar.jardimsentinela.dto;

import java.time.Instant;

import br.edu.cesar.jardimsentinela.tendencia.CalculadoraTendencia;
import br.edu.cesar.jardimsentinela.tendencia.ResultadoTendencia;
import br.edu.cesar.jardimsentinela.tendencia.Tendencia;

/**
 * [SCRUM-28][SW-14] Resposta de {@code GET /api/jardins/{id}/tendencia} (docs/tendencia-nivel.md,
 * seção 11). Inclui o contexto do cálculo (tolerância e janela) para o dashboard não
 * apresentar parâmetros experimentais como se fossem calibrados.
 *
 * Não há {@code variacaoLiquidaCm}: com empates a variação seria uma faixa, não um valor.
 *
 * @param jardimId                        jardim consultado.
 * @param tendencia                       um dos 6 estados. Clientes devem tratar valores
 *                                        desconhecidos de forma segura (podem surgir novos).
 * @param leiturasValidas                 leituras usadas na janela (no máximo N).
 * @param leiturasDescartadas             leituras inválidas examinadas; não é o total de
 *                                        inválidas no banco.
 * @param janelaLeituras                  tamanho da janela, N.
 * @param toleranciaCm                    tolerância usada (configuração; experimental).
 * @param idadeMaximaSegundos              idade máxima usada para DESATUALIZADO, em segundos
 *                                        (configuração; PROVISÓRIO, ver docs/decisoes-tecnicas.md §5.2).
 * @param confiabilidadeCronologia        sempre {@code "NAO_VALIDADA"} neste MVP: a API
 *                                        ainda não persiste bootId, seq e horaSincronizada.
 * @param semTimestampDispositivoNaJanela sinal informativo; não prova confiabilidade.
 * @param multiplosSensorId               sinal informativo; não prova mistura de tecnologias.
 * @param ultimaRecepcaoEm                maior recebidoEm entre as leituras válidas: o
 *                                        tempo desde a última RECEPÇÃO pela API, não a
 *                                        idade da medição. {@code null} (serializado como
 *                                        {@code null}) significa "atualidade desconhecida",
 *                                        nunca "dado atual".
 */
public record TendenciaResponse(
        String jardimId,
        Tendencia tendencia,
        int leiturasValidas,
        int leiturasDescartadas,
        int janelaLeituras,
        double toleranciaCm,
        long idadeMaximaSegundos,
        String confiabilidadeCronologia,
        boolean semTimestampDispositivoNaJanela,
        boolean multiplosSensorId,
        Instant ultimaRecepcaoEm
) {

    public static final String CRONOLOGIA_NAO_VALIDADA = "NAO_VALIDADA";

    public static TendenciaResponse de(String jardimId, ResultadoTendencia resultado,
            double toleranciaCm, long idadeMaximaSegundos) {
        return new TendenciaResponse(
                jardimId,
                resultado.tendencia(),
                resultado.leiturasValidas(),
                resultado.leiturasDescartadas(),
                CalculadoraTendencia.TAMANHO_JANELA,
                toleranciaCm,
                idadeMaximaSegundos,
                CRONOLOGIA_NAO_VALIDADA,
                resultado.semTimestampDispositivoNaJanela(),
                resultado.multiplosSensorId(),
                resultado.ultimaRecepcaoEm());
    }
}

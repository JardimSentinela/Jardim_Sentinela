package br.edu.cesar.jardimsentinela.tendencia;

import java.time.Instant;

/**
 * [SCRUM-28][SW-14] Resultado do cálculo de tendência, devolvido pela função pura.
 *
 * É só um valor: imutável, sem lógica e sem dependência de Spring, JPA ou JSON. O que
 * pertence ao contexto da requisição (id do jardim, tamanho da janela, tolerância e
 * confiabilidade da cronologia) NÃO fica aqui; entra no DTO de resposta.
 *
 * @param tendencia                       classificação, ou o motivo de não haver tendência
 *                                        atual a informar.
 * @param leiturasValidas                 quantas leituras foram efetivamente usadas na
 *                                        janela, no máximo N = 5. Em empates, a análise de
 *                                        possibilidades pode olhar mais candidatos, mas
 *                                        isso não aumenta esta contagem.
 * @param leiturasDescartadas             leituras inválidas examinadas durante a
 *                                        recuperação e o cálculo. NÃO é o total de
 *                                        registros inválidos existentes no banco.
 * @param semTimestampDispositivoNaJanela sinal informativo: há leitura sem
 *                                        {@code timestampDispositivo} (cronologia pelo
 *                                        {@code recebidoEm}). Com menos de 5 leituras
 *                                        válidas, vale para as válidas disponíveis (janela
 *                                        parcial). Considera os candidatos relevantes de
 *                                        um empate no limite da janela. Não prova
 *                                        confiabilidade.
 * @param multiplosSensorId               sinal informativo: há mais de um {@code sensorId}.
 *                                        Mesmas regras de janela parcial e de empate do
 *                                        campo anterior. Não prova mistura de tecnologias.
 * @param ultimaRecepcaoEm                maior {@code recebidoEm} entre as leituras
 *                                        válidas examinadas: o tempo desde a última
 *                                        RECEPÇÃO pela API, não a idade real da medição.
 *                                        É {@code null} quando não existe nenhuma leitura
 *                                        válida ou nenhuma tem {@code recebidoEm}; não se
 *                                        inventa data. {@code null} significa "atualidade
 *                                        desconhecida", nunca "dado atual".
 */
public record ResultadoTendencia(
        Tendencia tendencia,
        int leiturasValidas,
        int leiturasDescartadas,
        boolean semTimestampDispositivoNaJanela,
        boolean multiplosSensorId,
        Instant ultimaRecepcaoEm
) {
}

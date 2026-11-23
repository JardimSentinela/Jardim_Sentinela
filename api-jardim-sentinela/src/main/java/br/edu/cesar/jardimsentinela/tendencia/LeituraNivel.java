package br.edu.cesar.jardimsentinela.tendencia;

import java.time.Instant;

/**
 * [SCRUM-28][SW-14] Entrada da função pura de tendência: uma leitura reduzida ao que o
 * cálculo precisa. Existe para que a lógica de tendência não dependa da entidade JPA
 * {@code Leitura}.
 *
 * Não tem {@code id} de propósito: a decisão D8 proíbe desempatar leituras de mesmo
 * horário pela ordem de gravação. Sem o campo, o compilador impede esse atalho.
 *
 * Os valores chegam como estão, sem correção: o filtro de leituras válidas (D10) é
 * responsabilidade da calculadora, que descarta e conta as inválidas. Por isso
 * {@code nivelCm} é {@code Double} (aceita {@code null}, {@code NaN} e infinito).
 *
 * Atenção: por ser um record, {@code equals} compara por valor, então duas leituras com
 * os mesmos dados são "iguais". Quem usa esta classe deve preservar leituras distintas
 * com listas e posições, nunca com {@code Set}, que as fundiria.
 *
 * @param nivelCm              nível em centímetros; pode ser {@code null}, {@code NaN},
 *                             infinito ou negativo (inválido, tratado pela calculadora).
 * @param timestampDispositivo instante da medição informado pelo dispositivo; pode ser
 *                             {@code null}. Chave cronológica preferida (contrato 3.4).
 * @param recebidoEm           instante de recepção pela API; chave cronológica quando
 *                             {@code timestampDispositivo} é {@code null}, e base da
 *                             idade dos dados.
 * @param sensorId             sensor de origem; só alimenta o sinal informativo de
 *                             múltiplos sensores.
 */
public record LeituraNivel(
        Double nivelCm,
        Instant timestampDispositivo,
        Instant recebidoEm,
        String sensorId
) {
}

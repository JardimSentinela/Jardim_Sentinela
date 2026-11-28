package br.edu.cesar.jardimsentinela.tendencia;

/**
 * [SCRUM-28][SW-14] Resultado da identificação de tendência de nível de um jardim.
 *
 * Os três primeiros valores são a classificação em si. Os três últimos dizem por que
 * NÃO há tendência atual a informar. Um único campo carrega os dois casos, então não
 * existe combinação inválida (por exemplo, "SUBINDO" junto de "histórico insuficiente").
 *
 * Os clientes devem tratar valores desconhecidos de forma segura: novos estados podem
 * ser acrescentados no futuro.
 *
 * Ordem de precedência quando mais de uma situação vale ao mesmo tempo:
 * INSUFICIENTE, depois DESATUALIZADO, depois INDETERMINADO, depois a tendência calculada.
 */
public enum Tendencia {

    /** A variação líquida da janela passou da tolerância, para cima. */
    SUBINDO,

    /**
     * A variação líquida da janela ficou DENTRO da tolerância (|variação| <= tolerância).
     *
     * Atenção: isso NÃO significa que o nível ficou constante durante a janela. O nível
     * pode ter oscilado bastante e voltado ao ponto de partida; só a diferença entre a
     * primeira e a última leitura é considerada.
     */
    ESTAVEL,

    /** A variação líquida da janela passou da tolerância, para baixo. */
    DESCENDO,

    /** Há menos leituras válidas do que o tamanho da janela. Não há o que calcular. */
    INSUFICIENTE,

    /**
     * A ordem das leituras nas pontas da janela não é definida pelos dados (empate no
     * horário) e as ordens possíveis levariam a classificações diferentes.
     */
    INDETERMINADO,

    /**
     * A leitura recebida mais recentemente é mais antiga que o limite configurado.
     * Mede o tempo desde a última RECEPÇÃO, não a idade real da medição.
     */
    DESATUALIZADO
}

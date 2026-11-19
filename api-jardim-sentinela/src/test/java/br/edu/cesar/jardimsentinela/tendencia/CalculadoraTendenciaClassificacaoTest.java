package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.TOLERANCIA;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.serie;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.util.List;

import org.junit.jupiter.api.Test;

/**
 * [SCRUM-28][SW-14] Bloco B: classificação em ENCHENDO, ESTAVEL e DRENANDO pela variação
 * líquida (última menos primeira) comparada à tolerância, com limite inclusivo.
 * Níveis de cada série em ordem cronológica; N = 5.
 */
class CalculadoraTendenciaClassificacaoTest {

    private static ResultadoTendencia calcular(List<LeituraNivel> leituras) {
        return LeiturasDeTeste.calcular(leituras, TOLERANCIA);
    }

    private static Tendencia tendencia(double... niveis) {
        return calcular(serie(niveis)).tendencia();
    }

    @Test
    void cincoLeiturasSubindoDevolvemEnchendoComOResultadoCompleto() {
        // Cenário 1. Última recepção: leitura em T0 + 4 s, recebida 1 s depois.
        assertEquals(
                new ResultadoTendencia(Tendencia.ENCHENDO, 5, 0, false, false, T0.plusSeconds(5)),
                calcular(serie(10, 11, 12, 13, 14)));
    }

    @Test
    void cincoLeiturasDescendoDevolvemDrenando() {
        // Cenário 2
        assertEquals(Tendencia.DRENANDO, tendencia(14, 13, 12, 11, 10));
    }

    @Test
    void variacaoPequenaDentroDaToleranciaEEstavel() {
        // Cenário 3: +0,6 cm
        assertEquals(Tendencia.ESTAVEL, tendencia(10, 10.4, 9.8, 10.3, 10.6));
    }

    @Test
    void cincoLeiturasIguaisSaoEstaveis() {
        // Cenário 4
        assertEquals(Tendencia.ESTAVEL, tendencia(10, 10, 10, 10, 10));
    }

    @Test
    void variacaoExatamenteIgualAToleranciaEEstavelNosDoisSentidos() {
        // Cenário 5: limite inclusivo
        assertEquals(Tendencia.ESTAVEL, tendencia(10, 10, 10, 10, 11.5));   // +1,5
        assertEquals(Tendencia.ESTAVEL, tendencia(11.5, 10, 10, 10, 10));   // -1,5
    }

    @Test
    void variacaoAcimaDaToleranciaClassificaNosDoisSentidos() {
        // Cenário 6
        assertEquals(Tendencia.ENCHENDO, tendencia(10, 10, 10, 10, 11.6));  // +1,6
        assertEquals(Tendencia.DRENANDO, tendencia(11.6, 10, 10, 10, 10));  // -1,6
    }

    @Test
    void oscilacaoQueVoltaAoPontoDeOrigemEEstavel() {
        // Cenário 8: documenta a limitação aceita (D1). Variação líquida 0.
        assertEquals(Tendencia.ESTAVEL, tendencia(10, 15, 10, 15, 10));
    }

    @Test
    void soAsPontasDaJanelaEntramNaConta() {
        // Cenário 9: o meio oscila muito, mas a variação líquida é +1.
        assertEquals(Tendencia.ESTAVEL, tendencia(10, 20, 0, 30, 11));
    }

    // ---- Ciclo B2: comparação exata com BigDecimal (cenário 7) ----

    @Test
    void toleranciaDecimalNaoSofreComErroDePontoFlutuante() {
        // Em double, 1,1 - 1,0 = 0,10000000000000009, que "passaria" de 0,1. Com
        // BigDecimal.valueOf a variação é exatamente 0,1: dentro do limite inclusivo.
        assertEquals(Tendencia.ESTAVEL,
                LeiturasDeTeste.calcular(serie(1.0, 1.0, 1.0, 1.0, 1.1), 0.1).tendencia());
        assertEquals(Tendencia.ESTAVEL,
                LeiturasDeTeste.calcular(serie(1.1, 1.0, 1.0, 1.0, 1.0), 0.1).tendencia());
    }

    @Test
    void variacaoRealmenteAcimaDaToleranciaDecimalAindaClassifica() {
        assertEquals(Tendencia.ENCHENDO,
                LeiturasDeTeste.calcular(serie(1.0, 1.0, 1.0, 1.0, 1.2), 0.1).tendencia());
        assertEquals(Tendencia.DRENANDO,
                LeiturasDeTeste.calcular(serie(1.2, 1.0, 1.0, 1.0, 1.0), 0.1).tendencia());
    }

    // ---- Ciclo B3: tolerância inválida falha alto (D3) ----

    @Test
    void toleranciaNegativaEhRejeitada() {
        assertThrows(IllegalArgumentException.class,
                () -> LeiturasDeTeste.calcular(serie(10, 11, 12, 13, 14), -0.1));
    }

    @Test
    void toleranciaNaNEhRejeitada() {
        assertThrows(IllegalArgumentException.class,
                () -> LeiturasDeTeste.calcular(serie(10, 11, 12, 13, 14), Double.NaN));
    }

    @Test
    void toleranciaInfinitaEhRejeitada() {
        assertThrows(IllegalArgumentException.class,
                () -> LeiturasDeTeste.calcular(serie(10, 11, 12, 13, 14), Double.POSITIVE_INFINITY));
    }
}

package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.leitura;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.semTimestamp;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.serie;
import static org.junit.jupiter.api.Assertions.assertEquals;

import java.time.Instant;
import java.util.List;

import org.junit.jupiter.api.Test;

/**
 * [SCRUM-28][SW-14] Testes da função pura de tendência, escritos em ciclos TDD
 * (RED, GREEN, REFACTOR): um comportamento por vez.
 */
class CalculadoraTendenciaTest {

    // Único ponto que chama a função: se a assinatura mudar, só este método muda.
    private static ResultadoTendencia calcular(List<LeituraNivel> leituras) {
        return LeiturasDeTeste.calcular(leituras, LeiturasDeTeste.TOLERANCIA);
    }

    @Test
    void historicoVazioDevolveInsuficiente() {
        ResultadoTendencia resultado = calcular(List.of());

        assertEquals(
                new ResultadoTendencia(Tendencia.INSUFICIENTE, 0, 0, false, false, null),
                resultado);
    }

    @Test
    void leituraComNivelNuloEDescartadaEContada() {
        // A leitura tem as duas datas de propósito: a data de uma leitura descartada não
        // pode aparecer em ultimaRecepcaoEm (assertSomenteDescartada espera null).
        assertSomenteDescartada(leitura(null, 0));
    }

    @Test
    void leituraComNivelNaNEDescartadaEContada() {
        assertSomenteDescartada(leitura(Double.NaN, 0));
    }

    // ---- Bloco A, ciclo A1: o resto das regras de leitura inválida (D10) ----

    @Test
    void leituraComInfinitoPositivoEDescartadaEContada() {
        assertSomenteDescartada(leitura(Double.POSITIVE_INFINITY, 0));
    }

    @Test
    void leituraComInfinitoNegativoEDescartadaEContada() {
        assertSomenteDescartada(leitura(Double.NEGATIVE_INFINITY, 0));
    }

    @Test
    void leituraComNivelNegativoEDescartadaEContada() {
        assertSomenteDescartada(leitura(-0.1, 0));
    }

    @Test
    void leituraSemNenhumaReferenciaTemporalEDescartadaEContada() {
        // Sem timestampDispositivo e sem recebidoEm: não há como posicioná-la no tempo.
        assertSomenteDescartada(new LeituraNivel(10.0, null, null, "S01"));
    }

    // ---- Ciclo A2: contagem de válidas, última recepção e sinais (menos de 5 válidas) ----

    @Test
    void menosDeCincoValidasContaAsValidasEContinuaInsuficiente() {
        // Quatro leituras, a última recebida em T0 + 4 s.
        assertEquals(
                new ResultadoTendencia(Tendencia.INSUFICIENTE, 4, 0, false, false, T0.plusSeconds(4)),
                calcular(serie(10, 11, 12, 13)));
    }

    @Test
    void ultimaRecepcaoIgnoraLeiturasDescartadas() {
        List<LeituraNivel> leituras = List.of(
                leitura(10.0, 0), leitura(11.0, 1), leitura(12.0, 2),
                leitura(null, 50),            // recebida bem depois, mas inválida
                leitura(Double.NaN, 60));     // idem

        assertEquals(
                new ResultadoTendencia(Tendencia.INSUFICIENTE, 3, 2, false, false, T0.plusSeconds(3)),
                calcular(leituras));
    }

    @Test
    void nivelZeroEUmaLeituraValida() {
        assertEquals(
                new ResultadoTendencia(Tendencia.INSUFICIENTE, 1, 0, false, false, T0.plusSeconds(1)),
                calcular(List.of(leitura(0.0, 0))));
    }

    @Test
    void sinalSemTimestampDispositivoLigaQuandoUmaValidaNaoTem() {
        assertEquals(
                new ResultadoTendencia(Tendencia.INSUFICIENTE, 2, 0, true, false, T0.plusSeconds(1)),
                calcular(List.of(leitura(10.0, 0), semTimestamp(11.0, 1))));
    }

    @Test
    void leituraDescartadaNaoLigaOSinalDeSemTimestamp() {
        LeituraNivel invalidaSemTimestamp = new LeituraNivel(null, null, T0.plusSeconds(5), "S01");

        assertEquals(
                new ResultadoTendencia(Tendencia.INSUFICIENTE, 1, 1, false, false, T0.plusSeconds(1)),
                calcular(List.of(leitura(10.0, 0), invalidaSemTimestamp)));
    }

    @Test
    void sinalMultiplosSensoresLigaComMaisDeUmSensorId() {
        LeituraNivel outroSensor = new LeituraNivel(11.0, T0.plusSeconds(1), T0.plusSeconds(2), "S02");

        assertEquals(
                new ResultadoTendencia(Tendencia.INSUFICIENTE, 2, 0, false, true, T0.plusSeconds(2)),
                calcular(List.of(leitura(10.0, 0), outroSensor)));
    }

    @Test
    void leituraDescartadaNaoLigaOSinalDeMultiplosSensores() {
        LeituraNivel outroSensorInvalido = new LeituraNivel(Double.NaN, T0, T0.plusSeconds(1), "S02");

        assertEquals(
                new ResultadoTendencia(Tendencia.INSUFICIENTE, 1, 1, false, false, T0.plusSeconds(1)),
                calcular(List.of(leitura(10.0, 0), outroSensorInvalido)));
    }

    private static void assertSomenteDescartada(LeituraNivel leitura) {
        assertEquals(
                new ResultadoTendencia(Tendencia.INSUFICIENTE, 0, 1, false, false, null),
                calcular(List.of(leitura)));
    }
}

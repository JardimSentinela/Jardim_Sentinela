package br.edu.cesar.jardimsentinela.drenagem;

import static org.junit.jupiter.api.Assertions.assertEquals;

import java.time.Duration;
import java.time.Instant;
import java.util.List;

import org.junit.jupiter.api.Test;

import br.edu.cesar.jardimsentinela.tendencia.LeituraNivel;

/**
 * [SCRUM-30][SW-15] Cenários da regra provisória de drenagem lenta (v1, janela de 60 min,
 * limite de 0,5 cm/min). Critério experimental: ver docs/drenagem-lenta.md.
 */
class DetectorDrenagemLentaTest {

    private static final Instant T0 = Instant.parse("2026-11-20T12:00:00Z");
    private static final Duration JANELA = Duration.ofMinutes(60);
    private static final double LIMITE = 0.5;

    private static LeituraNivel leitura(double nivelCm, Instant quando) {
        return new LeituraNivel(nivelCm, quando, quando, "S01");
    }

    private static DetectorDrenagemLenta.Resultado avaliar(List<LeituraNivel> leituras) {
        return DetectorDrenagemLenta.avaliar(leituras, JANELA, LIMITE);
    }

    @Test
    void quedaLentaDentroDaJanelaEDetectada() {
        var r = avaliar(List.of(
                leitura(20.0, T0),
                leitura(18.5, T0.plus(Duration.ofMinutes(30))),
                leitura(17.0, T0.plus(Duration.ofMinutes(60)))));

        assertEquals(DetectorDrenagemLenta.Situacao.DRENAGEM_LENTA, r.situacao());
        assertEquals(0.05, r.taxaCmMin(), 1e-9);
        assertEquals(3, r.leiturasUsadas());
    }

    @Test
    void taxaIgualAoLimiteNaoEhLenta() {
        var r = avaliar(List.of(
                leitura(40.0, T0),
                leitura(10.0, T0.plus(Duration.ofMinutes(60)))));

        assertEquals(DetectorDrenagemLenta.Situacao.DRENAGEM_NORMAL, r.situacao());
        assertEquals(0.5, r.taxaCmMin(), 1e-9);
    }

    @Test
    void quedaRapidaNaoEhLenta() {
        var r = avaliar(List.of(
                leitura(40.0, T0),
                leitura(25.0, T0.plus(Duration.ofMinutes(30))),
                leitura(10.0, T0.plus(Duration.ofMinutes(60)))));

        assertEquals(DetectorDrenagemLenta.Situacao.DRENAGEM_NORMAL, r.situacao());
    }

    @Test
    void nivelSubindoNaoEhDrenagem() {
        var r = avaliar(List.of(
                leitura(10.0, T0),
                leitura(15.0, T0.plus(Duration.ofMinutes(60)))));

        assertEquals(DetectorDrenagemLenta.Situacao.SEM_QUEDA, r.situacao());
    }

    @Test
    void umaUnicaLeituraEhInsuficiente() {
        var r = avaliar(List.of(leitura(20.0, T0)));

        assertEquals(DetectorDrenagemLenta.Situacao.INSUFICIENTE, r.situacao());
        assertEquals(1, r.leiturasUsadas());
    }

    @Test
    void leiturasForaDaJanelaSaoIgnoradas() {
        var r = avaliar(List.of(
                leitura(60.0, T0.minus(Duration.ofHours(2))),
                leitura(20.0, T0.plus(Duration.ofMinutes(0))),
                leitura(19.0, T0.plus(Duration.ofMinutes(50)))));

        assertEquals(DetectorDrenagemLenta.Situacao.DRENAGEM_LENTA, r.situacao());
        assertEquals(2, r.leiturasUsadas());
        assertEquals(1.0 / 50.0, r.taxaCmMin(), 1e-9);
    }

    @Test
    void leiturasInvalidasNaoEntramNaConta() {
        var r = avaliar(List.of(
                leitura(20.0, T0),
                new LeituraNivel(null, T0.plus(Duration.ofMinutes(10)), null, "S01"),
                new LeituraNivel(-5.0, T0.plus(Duration.ofMinutes(20)), null, "S01"),
                leitura(17.0, T0.plus(Duration.ofMinutes(60)))));

        assertEquals(DetectorDrenagemLenta.Situacao.DRENAGEM_LENTA, r.situacao());
        assertEquals(2, r.leiturasUsadas());
        assertEquals(2, r.leiturasDescartadas());
    }
}

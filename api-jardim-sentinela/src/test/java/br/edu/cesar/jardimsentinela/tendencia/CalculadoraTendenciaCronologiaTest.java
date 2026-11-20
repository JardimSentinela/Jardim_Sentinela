package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.TOLERANCIA;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.leitura;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.semTimestamp;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.serie;
import static org.junit.jupiter.api.Assertions.assertEquals;

import java.util.List;

import org.junit.jupiter.api.Test;

/**
 * [SCRUM-28][SW-14] Bloco C: cronologia. A ordem de chegada não é a ordem de medição;
 * a calculadora ordena por conta própria (chave: timestampDispositivo, ou recebidoEm
 * quando ele falta) e escolhe a janela depois de ordenar.
 */
class CalculadoraTendenciaCronologiaTest {

    private static ResultadoTendencia calcular(List<LeituraNivel> leituras) {
        return LeiturasDeTeste.calcular(leituras, TOLERANCIA);
    }

    // ---- Ciclo C1: ordenação (cenário 12) ----

    @Test
    void entradaEmbaralhadaNaoAlteraOResultado() {
        // Leituras sem timestampDispositivo (a chave é o recebidoEm), fora de ordem.
        // Na ordem cronológica: 10, 11, 12, 13, 14 (+4).
        List<LeituraNivel> embaralhada = List.of(
                semTimestamp(12.0, 2), semTimestamp(10.0, 0), semTimestamp(14.0, 4),
                semTimestamp(11.0, 1), semTimestamp(13.0, 3));

        assertEquals(
                new ResultadoTendencia(Tendencia.SUBINDO, 5, 0, true, false, T0.plusSeconds(4)),
                calcular(embaralhada));
    }

    @Test
    void mesmaSerieEmOrdemDiretaEEmbaralhadaDaOMesmoResultado() {
        List<LeituraNivel> direta = List.of(
                semTimestamp(14.0, 0), semTimestamp(13.0, 1), semTimestamp(12.0, 2),
                semTimestamp(11.0, 3), semTimestamp(10.0, 4));
        List<LeituraNivel> embaralhada = List.of(
                semTimestamp(11.0, 3), semTimestamp(14.0, 0), semTimestamp(10.0, 4),
                semTimestamp(12.0, 2), semTimestamp(13.0, 1));

        assertEquals(Tendencia.DESCENDO, calcular(direta).tendencia());
        assertEquals(calcular(direta), calcular(embaralhada));
    }

    // ---- Ciclo C2: timestampDispositivo com prioridade; recebidoEm como alternativa ----

    @Test
    void timestampDispositivoTemPrioridadeSobreRecebidoEm() {
        // Cenário 13 (retransmissão). Medições 24, 20, 16, 12, 8: o jardim está drenando.
        // A fila offline envia primeiro a mais recente (8) e depois as antigas, então a
        // ordem de chegada (recebidoEm) é 8, 24, 20, 16, 12.
        List<LeituraNivel> leituras = List.of(
                new LeituraNivel(8.0, T0.plusSeconds(4), T0.plusSeconds(10), "S01"),
                new LeituraNivel(24.0, T0.plusSeconds(0), T0.plusSeconds(11), "S01"),
                new LeituraNivel(20.0, T0.plusSeconds(1), T0.plusSeconds(12), "S01"),
                new LeituraNivel(16.0, T0.plusSeconds(2), T0.plusSeconds(13), "S01"),
                new LeituraNivel(12.0, T0.plusSeconds(3), T0.plusSeconds(14), "S01"));

        // Pela ordem de chegada seria 12 - 8 = +4 (SUBINDO, errado). Pela medição,
        // 8 - 24 = -16.
        assertEquals(
                new ResultadoTendencia(Tendencia.DESCENDO, 5, 0, false, false, T0.plusSeconds(14)),
                calcular(leituras));
    }

    @Test
    void leituraSemTimestampUsaORecebidoEmComoChaveEServeDeSinal() {
        // Cenário 21. Chaves: 10 em T0+0 (timestamp), 11 em T0+2 (sem timestamp: vale o
        // recebidoEm), 12 em T0+3, 13 em T0+4, 14 em T0+5. O recebidoEm do 10 é tardio de
        // propósito (T0+30): ordenar só por recebidoEm daria 11, 12, 13, 14, 10 (-1).
        List<LeituraNivel> leituras = List.of(
                new LeituraNivel(14.0, T0.plusSeconds(5), T0.plusSeconds(6), "S01"),
                new LeituraNivel(10.0, T0.plusSeconds(0), T0.plusSeconds(30), "S01"),
                new LeituraNivel(12.0, T0.plusSeconds(3), T0.plusSeconds(4), "S01"),
                new LeituraNivel(11.0, null, T0.plusSeconds(2), "S01"),
                new LeituraNivel(13.0, T0.plusSeconds(4), T0.plusSeconds(5), "S01"));

        assertEquals(
                new ResultadoTendencia(Tendencia.SUBINDO, 5, 0, true, false, T0.plusSeconds(30)),
                calcular(leituras));
    }

    // ---- Ciclo C3: a janela são as 5 válidas mais recentes ----

    @Test
    void comMaisDeCincoValidasUsaSoAsCincoMaisRecentes() {
        // Cenário 11: a mais antiga (100) é outlier e não pode entrar na conta.
        assertEquals(
                new ResultadoTendencia(Tendencia.SUBINDO, 5, 0, false, false, T0.plusSeconds(6)),
                calcular(serie(100, 10, 11, 12, 13, 14)));
    }

    @Test
    void sinaisInformativosValemSoParaALeiturasDaJanela() {
        // A mais antiga fica fora da janela: não tem timestamp e é de outro sensor, mas
        // isso não deve ligar os sinais.
        LeituraNivel antigaForaDaJanela = new LeituraNivel(100.0, null, T0, "S99");
        List<LeituraNivel> leituras = List.of(
                antigaForaDaJanela,
                leitura(10.0, 1), leitura(11.0, 2), leitura(12.0, 3),
                leitura(13.0, 4), leitura(14.0, 5));

        assertEquals(
                new ResultadoTendencia(Tendencia.SUBINDO, 5, 0, false, false, T0.plusSeconds(6)),
                calcular(leituras));
    }

    @Test
    void leiturasInvalidasNaoOcupamLugarNaJanela() {
        // Cenário 20 (proteção: já deve passar). Nulo, NaN, infinito e negativo são
        // descartados e contados; a janela continua sendo as cinco válidas.
        List<LeituraNivel> leituras = List.of(
                leitura(10.0, 0), leitura(null, 1), leitura(11.0, 2), leitura(Double.NaN, 3),
                leitura(12.0, 4), leitura(Double.POSITIVE_INFINITY, 5), leitura(13.0, 6),
                leitura(-1.0, 7), leitura(14.0, 8));

        assertEquals(
                new ResultadoTendencia(Tendencia.SUBINDO, 5, 4, false, false, T0.plusSeconds(9)),
                calcular(leituras));
    }
}

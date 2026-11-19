package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.TOLERANCIA;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.leitura;
import static org.junit.jupiter.api.Assertions.assertEquals;

import java.util.ArrayList;
import java.util.List;

import org.junit.jupiter.api.Test;

/**
 * [SCRUM-28][SW-14] Bloco C, empates (decisão A3). Leituras com a mesma chave
 * cronológica são um conjunto de possibilidades: a calculadora não inventa ordem entre
 * elas. Se todas as possibilidades dão a mesma classificação, ela é informada; se não,
 * o resultado é INDETERMINADO. N = 5, tolerância 1,5 cm.
 *
 * Notação nos comentários: "nível@segundo"; leituras no mesmo segundo estão empatadas.
 */
class CalculadoraTendenciaEmpatesTest {

    private static ResultadoTendencia calcular(List<LeituraNivel> leituras) {
        return LeiturasDeTeste.calcular(leituras, TOLERANCIA);
    }

    /** Leituras todas no mesmo instante (T0). */
    private static List<LeituraNivel> mesmoInstante(double... niveis) {
        List<LeituraNivel> leituras = new ArrayList<>();
        for (double nivel : niveis) {
            leituras.add(leitura(nivel, 0));
        }
        return leituras;
    }

    @Test
    void empateSoNoMeioDaJanelaEhCalculavel() {
        // Cenário 14: o empate (12 e 12,5 em @2) não toca nas pontas; 14 - 10 = +4.
        List<LeituraNivel> leituras = List.of(
                leitura(10.0, 0), leitura(11.0, 1), leitura(12.0, 2), leitura(12.5, 2),
                leitura(14.0, 3));

        assertEquals(Tendencia.ENCHENDO, calcular(leituras).tendencia());
    }

    @Test
    void empateNaPontaComEstadosDiferentesEIndeterminadoEIndependeDaOrdemDeEntrada() {
        // Cenário 15: a última pode ser 14,0 (+4,0 ENCHENDO) ou 10,5 (+0,5 ESTAVEL).
        List<LeituraNivel> umaOrdem = List.of(
                leitura(10.0, 0), leitura(11.0, 1), leitura(12.0, 2),
                leitura(14.0, 3), leitura(10.5, 3));
        List<LeituraNivel> outraOrdem = List.of(
                leitura(10.5, 3), leitura(12.0, 2), leitura(14.0, 3),
                leitura(10.0, 0), leitura(11.0, 1));

        ResultadoTendencia esperado =
                new ResultadoTendencia(Tendencia.INDETERMINADO, 5, 0, false, false, T0.plusSeconds(4));
        assertEquals(esperado, calcular(umaOrdem));
        assertEquals(esperado, calcular(outraOrdem));
    }

    @Test
    void empateNaPontaComMesmoEstadoEhCalculavel() {
        // Cenário 16: a última pode ser 14,0 (+4,0) ou 13,0 (+3,0): as duas são ENCHENDO.
        List<LeituraNivel> leituras = List.of(
                leitura(10.0, 0), leitura(11.0, 1), leitura(12.0, 2),
                leitura(14.0, 3), leitura(13.0, 3));

        assertEquals(Tendencia.ENCHENDO, calcular(leituras).tendencia());
    }

    @Test
    void empateAtravessandoOCorteComEstadosDiferentesEIndeterminado() {
        // Cenário 17: a mais antiga (50@0) fica fora. As quatro mais novas ocupam quatro
        // vagas; a quinta vaga é de UMA entre 10,0 e 11,0 (empatadas em @1). Com última
        // 12,0: +2,0 (ENCHENDO) ou +1,0 (ESTAVEL).
        List<LeituraNivel> leituras = List.of(
                leitura(50.0, 0), leitura(10.0, 1), leitura(11.0, 1), leitura(10.5, 2),
                leitura(11.0, 3), leitura(11.5, 4), leitura(12.0, 5));

        assertEquals(
                new ResultadoTendencia(Tendencia.INDETERMINADO, 5, 0, false, false, T0.plusSeconds(6)),
                calcular(leituras));
    }

    @Test
    void empateAtravessandoOCorteComMesmoEstadoEhCalculavel() {
        // Cenário 18: primeira 10,0 ou 10,5, última 14,0: +4,0 ou +3,5, ambas ENCHENDO.
        List<LeituraNivel> leituras = List.of(
                leitura(50.0, 0), leitura(10.0, 1), leitura(10.5, 1), leitura(11.0, 2),
                leitura(12.0, 3), leitura(13.0, 4), leitura(14.0, 5));

        assertEquals(
                new ResultadoTendencia(Tendencia.ENCHENDO, 5, 0, false, false, T0.plusSeconds(6)),
                calcular(leituras));
    }

    @Test
    void todasNoMesmoInstanteComNiveisMuitoDiferentesEIndeterminado() {
        // Cenário 19: sem ordem nenhuma, qualquer par de leituras pode ser (primeira,
        // última): a variação vai de -40 a +40.
        assertEquals(Tendencia.INDETERMINADO,
                calcular(mesmoInstante(10, 20, 30, 40, 50)).tendencia());
    }

    @Test
    void todasNoMesmoInstanteComNiveisIguaisSaoEstaveis() {
        // Cenário 19: todos iguais, qualquer ordem dá variação 0.
        assertEquals(Tendencia.ESTAVEL, calcular(mesmoInstante(10, 10, 10, 10, 10)).tendencia());
    }

    @Test
    void todasNoMesmoInstanteComAmplitudeDentroDaToleranciaSaoEstaveis() {
        // Amplitude (maior - menor) 1,0 e 1,5 (limite inclusivo): todas as ordens são ESTAVEL.
        assertEquals(Tendencia.ESTAVEL, calcular(mesmoInstante(10, 10.5, 11, 10.2, 10.8)).tendencia());
        assertEquals(Tendencia.ESTAVEL, calcular(mesmoInstante(10, 11.5, 10.7, 10, 11)).tendencia());
    }

    @Test
    void todasNoMesmoInstanteComAmplitudeAcimaDaToleranciaEIndeterminado() {
        // Amplitude 1,6: a variação possível vai de -1,6 (DRENANDO) a +1,6 (ENCHENDO).
        assertEquals(Tendencia.INDETERMINADO, calcular(mesmoInstante(10, 11.6, 10.7, 10, 11)).tendencia());
    }

    @Test
    void grupoEmpatadoMaiorQueAJanelaConsideraTodosOsCandidatos() {
        // Sete leituras no mesmo instante: qualquer cinco delas, em qualquer ordem. O 30
        // pode entrar na janela ou não; não se pode adivinhar.
        assertEquals(Tendencia.INDETERMINADO,
                calcular(mesmoInstante(10, 10, 10, 10, 10, 10, 30)).tendencia());
        assertEquals(Tendencia.ESTAVEL,
                calcular(mesmoInstante(10, 10, 10, 10, 10, 10, 10)).tendencia());
    }

    @Test
    void empateNasDuasPontasCombinaAsDuasEscolhas() {
        // Primeira: 10 ou 14 (empatadas em @0, cortando a janela). Última: 15 ou 11
        // (empatadas em @3). Variações possíveis: +5, +1, +1 e -3.
        List<LeituraNivel> leituras = List.of(
                leitura(10.0, 0), leitura(14.0, 0), leitura(12.0, 1), leitura(12.0, 2),
                leitura(15.0, 3), leitura(11.0, 3));

        assertEquals(Tendencia.INDETERMINADO, calcular(leituras).tendencia());
    }

    @Test
    void sinaisInformativosConsideramTodosOsCandidatosDoGrupoDoCorte() {
        // Das duas leituras empatadas em @1, só uma entra na janela, mas não se sabe qual:
        // a do sensor S02 é candidata, então o sinal de múltiplos sensores liga.
        LeituraNivel outroSensor = new LeituraNivel(11.0, T0.plusSeconds(1), T0.plusSeconds(2), "S02");
        List<LeituraNivel> umaOrdem = List.of(
                leitura(50.0, 0), leitura(10.0, 1), outroSensor, leitura(11.5, 2),
                leitura(12.0, 3), leitura(13.0, 4), leitura(14.0, 5));
        List<LeituraNivel> ordemInversaNoEmpate = List.of(
                leitura(50.0, 0), outroSensor, leitura(10.0, 1), leitura(11.5, 2),
                leitura(12.0, 3), leitura(13.0, 4), leitura(14.0, 5));

        ResultadoTendencia esperado =
                new ResultadoTendencia(Tendencia.ENCHENDO, 5, 0, false, true, T0.plusSeconds(6));
        assertEquals(esperado, calcular(umaOrdem));
        assertEquals(esperado, calcular(ordemInversaNoEmpate));
    }
}

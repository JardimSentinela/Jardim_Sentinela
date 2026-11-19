package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.TOLERANCIA;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.leitura;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.serie;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.time.Duration;
import java.time.Instant;
import java.util.List;

import org.junit.jupiter.api.Test;

/**
 * [SCRUM-28][SW-14] Bloco D: atualidade (DESATUALIZADO) e precedência dos estados.
 * A idade é medida desde o maior recebidoEm entre as leituras válidas até o "agora"
 * recebido como argumento: a calculadora não lê o relógio. Limite de 30 s (configurável);
 * idade exatamente igual ao limite ainda é atual.
 *
 * Precedência: INSUFICIENTE, depois DESATUALIZADO, depois INDETERMINADO, depois a tendência.
 */
class CalculadoraTendenciaAtualidadeTest {

    private static final Duration LIMITE = Duration.ofSeconds(30);
    /** Última recepção de serie(10, 11, 12, 13, 14): a quinta leitura, recebida em T0 + 5 s. */
    private static final Instant ULTIMA = T0.plusSeconds(5);

    private static ResultadoTendencia calcularEm(List<LeituraNivel> leituras, Duration limite,
            Instant agora) {
        return CalculadoraTendencia.calcular(leituras, TOLERANCIA, limite, agora);
    }

    @Test
    void idadeAcimaDoLimiteEDesatualizadoEMantemOsDemaisCampos() {
        // Cenário 23.
        assertEquals(
                new ResultadoTendencia(Tendencia.DESATUALIZADO, 5, 0, false, false, ULTIMA),
                calcularEm(serie(10, 11, 12, 13, 14), LIMITE, ULTIMA.plusSeconds(31)));
    }

    @Test
    void idadeExatamenteNoLimiteAindaEAtual() {
        // Cenário 23: 30 s exatos é atual.
        assertEquals(Tendencia.SUBINDO,
                calcularEm(serie(10, 11, 12, 13, 14), LIMITE, ULTIMA.plusSeconds(30)).tendencia());
    }

    @Test
    void umNanossegundoAlemDoLimiteJaEDesatualizado() {
        assertEquals(Tendencia.DESATUALIZADO,
                calcularEm(serie(10, 11, 12, 13, 14), LIMITE,
                        ULTIMA.plusSeconds(30).plusNanos(1)).tendencia());
    }

    @Test
    void limiteDeIdadeEConfiguravel() {
        Instant agora = ULTIMA.plusSeconds(6);   // idade de 6 s

        assertEquals(Tendencia.DESATUALIZADO,
                calcularEm(serie(10, 11, 12, 13, 14), Duration.ofSeconds(5), agora).tendencia());
        assertEquals(Tendencia.SUBINDO,
                calcularEm(serie(10, 11, 12, 13, 14), Duration.ofSeconds(10), agora).tendencia());
    }

    @Test
    void idadeUsaOMaiorRecebidoEmEntreTodasAsValidasENaoSoDaJanela() {
        // Proteção (doc, seção 10). A leitura mais antiga pela chave (fora da janela) foi
        // recebida por último, em T0 + 100 (retransmissão): conta como recepção recente.
        List<LeituraNivel> leituras = List.of(
                new LeituraNivel(100.0, T0, T0.plusSeconds(100), "S01"),
                leitura(10.0, 1), leitura(11.0, 2), leitura(12.0, 3),
                leitura(13.0, 4), leitura(14.0, 5));

        assertEquals(
                new ResultadoTendencia(Tendencia.SUBINDO, 5, 0, false, false, T0.plusSeconds(100)),
                calcularEm(leituras, LIMITE, T0.plusSeconds(110)));
    }

    @Test
    void insuficienteTemPrecedenciaSobreDesatualizado() {
        // Cenário 24: quatro leituras, e muito antigas. Sem 5 válidas nada mais se calcula.
        assertEquals(Tendencia.INSUFICIENTE,
                calcularEm(serie(10, 11, 12, 13), LIMITE, T0.plusSeconds(1000)).tendencia());
    }

    @Test
    void desatualizadoTemPrecedenciaSobreIndeterminado() {
        // Cenário 24: a série do cenário 15 (empate na ponta, estados diferentes).
        List<LeituraNivel> comEmpate = List.of(
                leitura(10.0, 0), leitura(11.0, 1), leitura(12.0, 2),
                leitura(14.0, 3), leitura(10.5, 3));

        assertEquals(Tendencia.INDETERMINADO,
                calcularEm(comEmpate, LIMITE, T0.plusSeconds(5)).tendencia());
        assertEquals(Tendencia.DESATUALIZADO,
                calcularEm(comEmpate, LIMITE, T0.plusSeconds(1000)).tendencia());
    }

    @Test
    void recepcaoNoFuturoDoAgoraNaoEDesatualizada() {
        // Proteção: diferença entre relógios (recebidoEm depois do "agora"): idade negativa.
        assertEquals(Tendencia.SUBINDO,
                calcularEm(serie(10, 11, 12, 13, 14), LIMITE, ULTIMA.minusSeconds(10)).tendencia());
    }

    @Test
    void semNenhumRecebidoEmNaoHaComoMedirAIdadeEClassificaNormalmente() {
        // Decisão de 2026-11-19: leituras válidas só com timestampDispositivo e sem
        // recebidoEm não têm última recepção. A idade não pode ser avaliada e não se
        // inventa data: não é DESATUALIZADO, mas ultimaRecepcaoEm = null também NÃO afirma
        // que o dado está atual (atualidade desconhecida). O servidor sempre preenche
        // recebidoEm, então isto só vale para dados anômalos.
        List<LeituraNivel> leituras = List.of(
                new LeituraNivel(10.0, T0, null, "S01"),
                new LeituraNivel(11.0, T0.plusSeconds(1), null, "S01"),
                new LeituraNivel(12.0, T0.plusSeconds(2), null, "S01"),
                new LeituraNivel(13.0, T0.plusSeconds(3), null, "S01"),
                new LeituraNivel(14.0, T0.plusSeconds(4), null, "S01"));

        assertEquals(
                new ResultadoTendencia(Tendencia.SUBINDO, 5, 0, false, false, null),
                calcularEm(leituras, LIMITE, T0.plusSeconds(1000)));
    }

    @Test
    void limiteDeIdadeNegativoEhRejeitado() {
        assertThrows(IllegalArgumentException.class,
                () -> calcularEm(serie(10, 11, 12, 13, 14), Duration.ofSeconds(-1), ULTIMA));
    }
}

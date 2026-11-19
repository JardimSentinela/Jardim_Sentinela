package br.edu.cesar.jardimsentinela.tendencia;

import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;

/**
 * [SCRUM-28][SW-14] Construtores de leituras e constantes compartilhados pelos testes da
 * calculadora. Existe só para os testes não repetirem montagem de dados.
 */
final class LeiturasDeTeste {

    static final Instant T0 = Instant.parse("2026-11-17T12:00:00Z");
    static final double TOLERANCIA = 1.5;
    static final Duration IDADE_MAXIMA = Duration.ofSeconds(30);
    /** "Agora" padrão dos testes: perto das leituras, para elas não ficarem desatualizadas. */
    static final Instant AGORA = T0.plusSeconds(20);

    private LeiturasDeTeste() {
    }

    /** Chama a calculadora com o limite de idade e o "agora" padrão dos testes. */
    static ResultadoTendencia calcular(List<LeituraNivel> leituras, double toleranciaCm) {
        return CalculadoraTendencia.calcular(leituras, toleranciaCm, IDADE_MAXIMA, AGORA);
    }

    /** Leitura medida em T0 + seg, recebida 1 s depois, do sensor S01. */
    static LeituraNivel leitura(Double nivel, int seg) {
        return new LeituraNivel(nivel, T0.plusSeconds(seg), T0.plusSeconds(seg + 1), "S01");
    }

    /** Leitura sem timestampDispositivo: só tem recebidoEm = T0 + seg. */
    static LeituraNivel semTimestamp(Double nivel, int seg) {
        return new LeituraNivel(nivel, null, T0.plusSeconds(seg), "S01");
    }

    /** Série em ordem cronológica: o primeiro nível em T0, o seguinte 1 s depois, etc. */
    static List<LeituraNivel> serie(double... niveis) {
        List<LeituraNivel> leituras = new ArrayList<>();
        for (int i = 0; i < niveis.length; i++) {
            leituras.add(leitura(niveis[i], i));
        }
        return leituras;
    }
}

package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.TOLERANCIA;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.leitura;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumMap;
import java.util.EnumSet;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.TreeMap;

import org.junit.jupiter.api.Test;

/**
 * [SCRUM-28][SW-14] Cenário 25: oráculo por força bruta.
 *
 * O oráculo não usa a lógica da calculadora. Para cada série ele enumera TODAS as ordens
 * totais compatíveis com os horários (todas as permutações dentro de cada grupo de mesmo
 * horário), toma as 5 leituras mais recentes de cada ordem, classifica a variação
 * última - primeira e junta os resultados: um só resultado possível é a resposta; mais de
 * um é INDETERMINADO. A calculadora deve concordar em todas as séries.
 *
 * As séries são sorteadas com semente fixa (reprodutível), com poucos horários distintos
 * para forçar muitos empates, inclusive atravessando o corte da janela, e a entrada é
 * embaralhada antes de chamar a calculadora.
 */
class CalculadoraTendenciaOraculoTest {

    private static final int JANELA = CalculadoraTendencia.TAMANHO_JANELA;

    @Test
    void calculadoraConcordaComAEnumeracaoDeTodasAsOrdensPossiveis() {
        Random sorteio = new Random(20261117L);
        Map<Tendencia, Integer> contagem = new EnumMap<>(Tendencia.class);

        for (int caso = 0; caso < 3000; caso++) {
            int quantidade = JANELA + sorteio.nextInt(3);          // 5, 6 ou 7 leituras
            List<LeituraNivel> leituras = new ArrayList<>();
            for (int i = 0; i < quantidade; i++) {
                int segundo = sorteio.nextInt(4);                  // só 4 horários: muitos empates
                double nivel = sorteio.nextInt(11) * 0.5;          // 0,0 a 5,0 em passos de 0,5
                leituras.add(leitura(nivel, segundo));
            }

            Tendencia esperada = oraculo(leituras);
            contagem.merge(esperada, 1, Integer::sum);

            List<LeituraNivel> embaralhada = new ArrayList<>(leituras);
            Collections.shuffle(embaralhada, sorteio);
            Tendencia obtida = LeiturasDeTeste.calcular(embaralhada, TOLERANCIA).tendencia();

            assertEquals(esperada, obtida, () -> "série (nível@segundo): " + descrever(leituras));
        }

        // O teste só vale se o sorteio exercitou todos os desfechos: confere a cobertura.
        for (Tendencia tendencia : List.of(Tendencia.SUBINDO, Tendencia.ESTAVEL,
                Tendencia.DESCENDO, Tendencia.INDETERMINADO)) {
            assertTrue(contagem.getOrDefault(tendencia, 0) >= 50,
                    "o sorteio quase não exercitou " + tendencia + ": " + contagem);
        }
    }

    // ---- o oráculo ----

    private static Tendencia oraculo(List<LeituraNivel> leituras) {
        // Grupos de mesmo horário, do mais antigo ao mais novo.
        Map<Instant, List<LeituraNivel>> porHorario = new TreeMap<>();
        for (LeituraNivel leitura : leituras) {
            porHorario.computeIfAbsent(leitura.timestampDispositivo(), h -> new ArrayList<>())
                    .add(leitura);
        }
        Set<Tendencia> possiveis = EnumSet.noneOf(Tendencia.class);
        enumerar(new ArrayList<>(porHorario.values()), 0, new ArrayList<>(), possiveis);
        return possiveis.size() == 1 ? possiveis.iterator().next() : Tendencia.INDETERMINADO;
    }

    /** Escolhe uma permutação para cada grupo, em sequência; ao fim, classifica a ordem. */
    private static void enumerar(List<List<LeituraNivel>> grupos, int indice,
            List<LeituraNivel> ordemAteAqui, Set<Tendencia> possiveis) {
        if (indice == grupos.size()) {
            List<LeituraNivel> janela =
                    ordemAteAqui.subList(ordemAteAqui.size() - JANELA, ordemAteAqui.size());
            BigDecimal variacao = BigDecimal.valueOf(janela.get(JANELA - 1).nivelCm())
                    .subtract(BigDecimal.valueOf(janela.get(0).nivelCm()));
            possiveis.add(classificar(variacao));
            return;
        }
        for (List<LeituraNivel> permutacao : permutacoes(grupos.get(indice))) {
            List<LeituraNivel> ordem = new ArrayList<>(ordemAteAqui);
            ordem.addAll(permutacao);
            enumerar(grupos, indice + 1, ordem, possiveis);
        }
    }

    private static List<List<LeituraNivel>> permutacoes(List<LeituraNivel> itens) {
        if (itens.size() <= 1) {
            return List.of(new ArrayList<>(itens));
        }
        List<List<LeituraNivel>> resultado = new ArrayList<>();
        for (int i = 0; i < itens.size(); i++) {
            List<LeituraNivel> resto = new ArrayList<>(itens);
            LeituraNivel escolhida = resto.remove(i);
            for (List<LeituraNivel> parcial : permutacoes(resto)) {
                List<LeituraNivel> permutacao = new ArrayList<>();
                permutacao.add(escolhida);
                permutacao.addAll(parcial);
                resultado.add(permutacao);
            }
        }
        return resultado;
    }

    private static Tendencia classificar(BigDecimal variacao) {
        BigDecimal tolerancia = BigDecimal.valueOf(TOLERANCIA);
        if (variacao.compareTo(tolerancia) > 0) {
            return Tendencia.SUBINDO;
        }
        if (variacao.compareTo(tolerancia.negate()) < 0) {
            return Tendencia.DESCENDO;
        }
        return Tendencia.ESTAVEL;
    }

    private static String descrever(List<LeituraNivel> leituras) {
        StringBuilder texto = new StringBuilder();
        for (LeituraNivel leitura : leituras) {
            long segundo = leitura.timestampDispositivo().getEpochSecond()
                    - LeiturasDeTeste.T0.getEpochSecond();
            texto.append(leitura.nivelCm()).append('@').append(segundo).append(' ');
        }
        return texto.toString().trim();
    }
}

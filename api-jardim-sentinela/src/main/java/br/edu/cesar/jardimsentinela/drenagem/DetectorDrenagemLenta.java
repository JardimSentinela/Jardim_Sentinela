package br.edu.cesar.jardimsentinela.drenagem;

import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import br.edu.cesar.jardimsentinela.tendencia.CalculadoraTendencia;
import br.edu.cesar.jardimsentinela.tendencia.LeituraNivel;

/**
 * [SCRUM-30][SW-15] Regra provisória e experimental de drenagem lenta.
 *
 * Usa a janela de tempo que termina na leitura mais recente. Dentro dela, a taxa é a queda
 * entre a primeira e a última leitura, dividida pelos minutos entre elas. Queda com taxa
 * menor que o limite é drenagem lenta; o limite em si já é drenagem normal (comparação
 * estrita). Função pura: o tempo vem das leituras, sem relógio global.
 *
 * Critério experimental, ver docs/drenagem-lenta.md.
 *
 * <p><b>Decisão mutável:</b> esta regra recebe snapshots periódicos <b>e</b> eventos
 * discretos (T1592) como medições, porque ambos trazem nível válido e a taxa usa o
 * tempo real entre a primeira e a última leitura. Essa escolha é provisória e pode
 * mudar por decisão da equipe — ver issue #29, item "Divergência entre Eventos T1592
 * e Snapshots periódicos". Se a API passar a considerar somente snapshots, o filtro
 * deve ser aplicado a quem monta a lista {@code leituras} (ver DrenagemService) e os
 * testes desta classe precisam ser ajustados junto. Nenhuma das duas políticas está
 * acordada como definitiva.
 */
public final class DetectorDrenagemLenta {

    public static final String VERSAO_REGRA = "v1-provisoria-drenagem-2026-11";

    public enum Situacao {
        DRENAGEM_LENTA,
        DRENAGEM_NORMAL,
        SEM_QUEDA,
        INSUFICIENTE
    }

    /**
     * @param taxaCmMin queda por minuto (positiva = descendo); 0 quando insuficiente.
     */
    public record Resultado(
            Situacao situacao,
            double taxaCmMin,
            int leiturasUsadas,
            int leiturasDescartadas,
            String versaoRegra) {
    }

    private DetectorDrenagemLenta() {
    }

    /**
     * @param leituras medições elegíveis, já ordenadas ou não. Hoje inclui snapshots e
     *                 eventos T1592; essa composição é uma decisão provisória (issue #29)
     *                 e pode ser alterada pela equipe sem mudar o cálculo da taxa.
     */
    public static Resultado avaliar(List<LeituraNivel> leituras, Duration janela, double limiteCmPorMin) {
        List<LeituraNivel> validas = new ArrayList<>();
        int descartadas = 0;
        for (LeituraNivel leitura : leituras) {
            if (CalculadoraTendencia.ehValida(leitura)) {
                validas.add(leitura);
            } else {
                descartadas++;
            }
        }
        validas.sort(Comparator.comparing(CalculadoraTendencia::chaveCronologica));

        if (validas.isEmpty()) {
            return new Resultado(Situacao.INSUFICIENTE, 0, 0, descartadas, VERSAO_REGRA);
        }

        Instant fim = CalculadoraTendencia.chaveCronologica(validas.get(validas.size() - 1));
        Instant inicio = fim.minus(janela);
        List<LeituraNivel> naJanela = validas.stream()
                .filter(l -> !CalculadoraTendencia.chaveCronologica(l).isBefore(inicio))
                .toList();

        LeituraNivel primeira = naJanela.get(0);
        LeituraNivel ultima = naJanela.get(naJanela.size() - 1);
        long segundos = Duration.between(CalculadoraTendencia.chaveCronologica(primeira),
                CalculadoraTendencia.chaveCronologica(ultima)).getSeconds();

        if (naJanela.size() < 2 || segundos <= 0) {
            return new Resultado(Situacao.INSUFICIENTE, 0, naJanela.size(), descartadas, VERSAO_REGRA);
        }

        double queda = primeira.nivelCm() - ultima.nivelCm();
        double taxa = queda / (segundos / 60.0);

        if (queda <= 0) {
            return new Resultado(Situacao.SEM_QUEDA, taxa, naJanela.size(), descartadas, VERSAO_REGRA);
        }
        Situacao situacao = taxa < limiteCmPorMin ? Situacao.DRENAGEM_LENTA : Situacao.DRENAGEM_NORMAL;
        return new Resultado(situacao, taxa, naJanela.size(), descartadas, VERSAO_REGRA);
    }
}

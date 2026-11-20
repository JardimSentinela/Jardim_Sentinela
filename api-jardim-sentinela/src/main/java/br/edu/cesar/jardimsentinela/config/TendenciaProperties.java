package br.edu.cesar.jardimsentinela.config;

import java.time.Duration;

import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.boot.context.properties.bind.DefaultValue;

/**
 * [SCRUM-28][SW-14] Parâmetros ajustáveis da tendência de nível, sem alterar código
 * (prefixo {@code jardimsentinela.tendencia}). Valores inválidos fazem a aplicação falhar
 * ao iniciar, em vez de dar resultados errados em silêncio.
 *
 * PROVISÓRIO, ver docs/decisoes-tecnicas.md §5.2: tolerância e idade máxima serão
 * revisadas com dados de campo. O tamanho da janela (N = 5) é uma constante do código
 * (D2), não uma propriedade.
 *
 * @param toleranciaCm D3: tolerância absoluta, em cm, sobre a variação líquida da janela.
 *                     Finita e não negativa. Padrão: 1,5.
 * @param idadeMaxima  D12: tempo máximo desde a última RECEPÇÃO (não desde a medição) para
 *                     o dado ainda ser atual. Não negativo. Padrão: 180 s (3 vezes o
 *                     snapshot de 60 s do hardware); perfil "simulacao": 15 s (3 vezes
 *                     5 s). Aceita formatos como {@code 180s} ou {@code PT3M}.
 * @param tamanhoPagina quantas leituras o serviço lê por página ao montar a janela. Não
 *                     muda o resultado (a busca continua até ter a janela e o grupo
 *                     empatado do corte completos); só o número de consultas. Mínimo 1.
 */
@ConfigurationProperties(prefix = "jardimsentinela.tendencia")
public record TendenciaProperties(
        @DefaultValue("1.5") double toleranciaCm,
        @DefaultValue("180s") Duration idadeMaxima,
        @DefaultValue("" + TendenciaProperties.TAMANHO_PAGINA_PADRAO) int tamanhoPagina) {

    public static final int TAMANHO_PAGINA_PADRAO = 50;

    public TendenciaProperties {
        if (!Double.isFinite(toleranciaCm) || toleranciaCm < 0) {
            throw new IllegalArgumentException(
                    "jardimsentinela.tendencia.tolerancia-cm deve ser finita e não negativa: "
                            + toleranciaCm);
        }
        if (idadeMaxima == null || idadeMaxima.isNegative()) {
            throw new IllegalArgumentException(
                    "jardimsentinela.tendencia.idade-maxima não pode ser negativa: " + idadeMaxima);
        }
        if (tamanhoPagina < 1) {
            throw new IllegalArgumentException(
                    "jardimsentinela.tendencia.tamanho-pagina deve ser pelo menos 1: " + tamanhoPagina);
        }
    }
}

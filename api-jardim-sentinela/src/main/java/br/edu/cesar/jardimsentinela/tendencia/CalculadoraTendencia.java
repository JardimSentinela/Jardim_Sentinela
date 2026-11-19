package br.edu.cesar.jardimsentinela.tendencia;

import java.math.BigDecimal;
import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Objects;

/**
 * [SCRUM-28][SW-14] Função pura de tendência de nível: sem Spring, sem banco e sem
 * relógio global. Só métodos estáticos; por isso não pode ser instanciada.
 *
 * Recebe as leituras, a tolerância, o limite de idade e o instante "agora", e devolve um
 * {@link ResultadoTendencia}. Decisões aplicadas (docs/tendencia-nivel.md): variação
 * líquida entre a primeira e a última leitura da janela de N = 5 leituras válidas (D1,
 * D2); limites inclusivos com BigDecimal (D4); precedência INSUFICIENTE, DESATUALIZADO,
 * INDETERMINADO, tendência (D6); cronologia por timestampDispositivo com recebidoEm como
 * alternativa, nunca validada (D7); empates como conjunto de possibilidades, sem
 * desempate por id ou nível (D8); leituras inválidas descartadas e contadas (D10);
 * idade pela última recepção (D12).
 *
 * Desenvolvida com TDD; os testes estão em CalculadoraTendencia*Test.
 */
public final class CalculadoraTendencia {

    /** D2: tamanho da janela, em leituras válidas (4 intervalos). */
    public static final int TAMANHO_JANELA = 5;

    /**
     * Ordem cronológica crescente pela chave de cada leitura. Não desempata: leituras
     * de mesma chave comparam como iguais (decisão D8; sem id e sem nível).
     */
    private static final Comparator<LeituraNivel> ORDEM_CRONOLOGICA =
            Comparator.comparing(CalculadoraTendencia::chaveCronologica);

    private CalculadoraTendencia() {
    }

    /**
     * D7 / contrato 3.4: a chave é o timestampDispositivo e, quando ele falta, o
     * recebidoEm. Fica num único lugar para a futura unificação com a consulta (D14).
     * Só é chamada para leituras válidas, que têm ao menos uma das duas datas.
     */
    public static Instant chaveCronologica(LeituraNivel leitura) {
        return leitura.timestampDispositivo() != null
                ? leitura.timestampDispositivo()
                : leitura.recebidoEm();
    }

    /**
     * @param leituras     leituras do jardim, em qualquer ordem; a calculadora ordena.
     * @param toleranciaCm D3: tolerância absoluta em cm; finita e não negativa.
     * @param idadeMaxima  D12: idade máxima da última recepção; não negativa.
     * @param agora        instante de referência para a idade (relógio injetado).
     * @throws IllegalArgumentException se a tolerância ou o limite de idade forem inválidos.
     */
    public static ResultadoTendencia calcular(List<LeituraNivel> leituras, double toleranciaCm,
            Duration idadeMaxima, Instant agora) {
        if (!Double.isFinite(toleranciaCm) || toleranciaCm < 0) {
            throw new IllegalArgumentException(
                    "A tolerância deve ser finita e não negativa: " + toleranciaCm);
        }
        if (idadeMaxima.isNegative()) {
            throw new IllegalArgumentException(
                    "O limite de idade não pode ser negativo: " + idadeMaxima);
        }

        List<LeituraNivel> validas = new ArrayList<>();
        int descartadas = 0;
        for (LeituraNivel leitura : leituras) {
            if (ehValida(leitura)) {
                validas.add(leitura);
            } else {
                descartadas++;
            }
        }
        validas.sort(ORDEM_CRONOLOGICA);

        List<List<LeituraNivel>> gruposDaJanela = gruposDaJanela(agruparPorChave(validas));
        List<LeituraNivel> candidatas = gruposDaJanela.stream().flatMap(List::stream).toList();
        boolean semTimestamp = algumaSemTimestampDispositivo(candidatas);
        boolean multiplosSensores = maisDeUmSensor(candidatas);
        Instant ultimaRecepcao = ultimaRecepcao(validas);

        // D6: a precedência é INSUFICIENTE, depois DESATUALIZADO, depois INDETERMINADO e
        // só então a tendência calculada.
        if (validas.size() < TAMANHO_JANELA) {
            return new ResultadoTendencia(Tendencia.INSUFICIENTE, validas.size(), descartadas,
                    semTimestamp, multiplosSensores, ultimaRecepcao);
        }
        if (estaDesatualizado(ultimaRecepcao, idadeMaxima, agora)) {
            return new ResultadoTendencia(Tendencia.DESATUALIZADO, TAMANHO_JANELA, descartadas,
                    semTimestamp, multiplosSensores, ultimaRecepcao);
        }

        List<LeituraNivel> grupoMaisNovo = gruposDaJanela.get(0);
        List<LeituraNivel> grupoMaisAntigo = gruposDaJanela.get(gruposDaJanela.size() - 1);
        Tendencia tendencia = classificarComEmpates(grupoMaisNovo, grupoMaisAntigo, toleranciaCm);
        return new ResultadoTendencia(tendencia, TAMANHO_JANELA, descartadas,
                semTimestamp, multiplosSensores, ultimaRecepcao);
    }

    /**
     * Agrupa leituras já ordenadas em grupos de mesma chave cronológica, do mais antigo
     * ao mais novo. Usa listas, nunca Set: leituras iguais em valor continuam distintas.
     */
    private static List<List<LeituraNivel>> agruparPorChave(List<LeituraNivel> ordenadas) {
        List<List<LeituraNivel>> grupos = new ArrayList<>();
        Instant chaveDoGrupo = null;
        for (LeituraNivel leitura : ordenadas) {
            Instant chave = chaveCronologica(leitura);
            if (grupos.isEmpty() || !chave.equals(chaveDoGrupo)) {
                grupos.add(new ArrayList<>());
                chaveDoGrupo = chave;
            }
            grupos.get(grupos.size() - 1).add(leitura);
        }
        return grupos;
    }

    /**
     * Grupos candidatos à janela: do mais novo para o mais antigo, até somar ao menos N
     * leituras. O último grupo tomado é o do corte: pode entrar só em parte na janela,
     * mas todos os seus membros são candidatos (A3), sem palpite sobre quais entram nem
     * sobre a ordem entre eles. Com menos de N leituras, devolve todos os grupos.
     */
    private static List<List<LeituraNivel>> gruposDaJanela(List<List<LeituraNivel>> grupos) {
        List<List<LeituraNivel>> tomados = new ArrayList<>();
        int total = 0;
        for (int i = grupos.size() - 1; i >= 0 && total < TAMANHO_JANELA; i--) {
            tomados.add(grupos.get(i));
            total += grupos.get(i).size();
        }
        return tomados;
    }

    /**
     * Classifica considerando a faixa de variações possíveis (última - primeira). A
     * primeira leitura da janela é uma do grupo mais antigo tomado; a última, uma do mais
     * novo. Se for o mesmo grupo (janela toda empatada), a primeira e a última são
     * leituras distintas dele, e a faixa é [-D, +D], com D = maior nível - menor nível.
     * A classificação é monotônica na variação, então basta comparar as duas extremidades
     * da faixa: se coincidem, o resultado independe da ordem desconhecida; se diferem, é
     * INDETERMINADO.
     */
    private static Tendencia classificarComEmpates(List<LeituraNivel> grupoMaisNovo,
            List<LeituraNivel> grupoMaisAntigo, double toleranciaCm) {
        BigDecimal menorVariacao = menorNivel(grupoMaisNovo).subtract(maiorNivel(grupoMaisAntigo));
        BigDecimal maiorVariacao = maiorNivel(grupoMaisNovo).subtract(menorNivel(grupoMaisAntigo));
        Tendencia noPiorCaso = classificar(menorVariacao, toleranciaCm);
        Tendencia noMelhorCaso = classificar(maiorVariacao, toleranciaCm);
        return noPiorCaso == noMelhorCaso ? noPiorCaso : Tendencia.INDETERMINADO;
    }

    private static BigDecimal menorNivel(List<LeituraNivel> leituras) {
        return leituras.stream().map(l -> BigDecimal.valueOf(l.nivelCm()))
                .min(Comparator.naturalOrder()).orElseThrow();
    }

    private static BigDecimal maiorNivel(List<LeituraNivel> leituras) {
        return leituras.stream().map(l -> BigDecimal.valueOf(l.nivelCm()))
                .max(Comparator.naturalOrder()).orElseThrow();
    }

    /**
     * D10: uma leitura é válida se o nível é não nulo, finito (nem NaN nem infinito) e
     * maior ou igual a zero, e se existe alguma referência temporal para posicioná-la
     * (timestampDispositivo ou recebidoEm). Não se inventa data para quem não tem.
     * Pública para o serviço decidir quando parar de buscar com a mesma regra.
     */
    public static boolean ehValida(LeituraNivel leitura) {
        Double nivel = leitura.nivelCm();
        boolean nivelValido = nivel != null && Double.isFinite(nivel) && nivel >= 0;
        boolean temReferenciaTemporal =
                leitura.timestampDispositivo() != null || leitura.recebidoEm() != null;
        return nivelValido && temReferenciaTemporal;
    }

    /**
     * D4: ENCHENDO acima de +tolerância, DRENANDO abaixo de -tolerância e ESTAVEL nos
     * limites (inclusivos). A comparação é exata: BigDecimal.valueOf usa a representação
     * decimal do double (1.1 vira 1.1), sem o erro de 1.1 - 1.0 = 0.10000000000000009.
     */
    private static Tendencia classificar(BigDecimal variacaoCm, double toleranciaCm) {
        BigDecimal tolerancia = BigDecimal.valueOf(toleranciaCm);
        if (variacaoCm.compareTo(tolerancia) > 0) {
            return Tendencia.ENCHENDO;
        }
        if (variacaoCm.compareTo(tolerancia.negate()) < 0) {
            return Tendencia.DRENANDO;
        }
        return Tendencia.ESTAVEL;
    }

    /**
     * D12: a idade é o tempo desde a última RECEPÇÃO (maior recebidoEm entre as válidas)
     * até "agora", que chega como argumento. Idade igual ao limite ainda é atual; só acima
     * dele é desatualizado. Sem recebidoEm em nenhuma leitura não há como medir a idade, e
     * não se inventa data: nesse caso não se declara desatualizado, mas isso NÃO significa
     * que o dado esteja atual. O resultado traz ultimaRecepcaoEm = null ("atualidade
     * desconhecida"). Limitação conhecida, aceita para o MVP.
     */
    private static boolean estaDesatualizado(Instant ultimaRecepcao, Duration idadeMaxima,
            Instant agora) {
        return ultimaRecepcao != null
                && Duration.between(ultimaRecepcao, agora).compareTo(idadeMaxima) > 0;
    }

    private static boolean algumaSemTimestampDispositivo(List<LeituraNivel> leituras) {
        return leituras.stream().anyMatch(l -> l.timestampDispositivo() == null);
    }

    private static boolean maisDeUmSensor(List<LeituraNivel> leituras) {
        return leituras.stream().map(LeituraNivel::sensorId).distinct().count() > 1;
    }

    /** Maior recebidoEm entre as leituras dadas; null se nenhuma o tiver. Sem data inventada. */
    private static Instant ultimaRecepcao(List<LeituraNivel> leituras) {
        return leituras.stream()
                .map(LeituraNivel::recebidoEm)
                .filter(Objects::nonNull)
                .max(Comparator.naturalOrder())
                .orElse(null);
    }
}

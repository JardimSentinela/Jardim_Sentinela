package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.AGORA;
import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.time.Clock;
import java.time.Duration;
import java.time.ZoneOffset;
import java.util.EnumMap;
import java.util.List;
import java.util.Map;
import java.util.Random;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;

import br.edu.cesar.jardimsentinela.config.TendenciaProperties;
import br.edu.cesar.jardimsentinela.dto.TendenciaResponse;
import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;
import br.edu.cesar.jardimsentinela.service.TendenciaService;

/**
 * [SCRUM-28][SW-14] Etapa 6, revisão final: a busca em páginas não pode perder candidatos.
 *
 * Para 400 históricos sorteados (semente fixa) com muitos empates de horário, leituras
 * inválidas e leituras sem timestamp, o resultado do serviço com páginas de 1, 2, 3, 7 e 50
 * leituras tem de ser IGUAL ao da calculadora aplicada ao histórico INTEIRO, sem
 * paginação. Se a busca parasse cedo demais (empate cortado, inválidas puladas), a
 * tendência ou os sinais divergiriam. A idade é neutralizada (limite enorme) porque
 * {@code ultimaRecepcaoEm} considera só as leituras buscadas, por desenho.
 */
@SpringBootTest
class TendenciaServicePaginacaoTests {

    private static final Duration SEM_LIMITE_DE_IDADE = Duration.ofDays(3650);
    private static final int[] TAMANHOS_DE_PAGINA = {1, 2, 3, 7, 50};

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    @Test
    void oResultadoNaoDependeDoTamanhoDaPaginaEBateComOHistoricoInteiro() {
        Random sorteio = new Random(20261119L);
        Clock relogio = Clock.fixed(AGORA, ZoneOffset.UTC);
        Map<Tendencia, Integer> cobertura = new EnumMap<>(Tendencia.class);

        for (int caso = 0; caso < 400; caso++) {
            repository.deleteAll();
            int quantidade = 1 + sorteio.nextInt(14);
            for (int i = 0; i < quantidade; i++) {
                int segundo = sorteio.nextInt(5);                       // poucos horários: muitos empates
                boolean semTimestamp = sorteio.nextInt(4) == 0;
                repository.save(new Leitura(
                        "J01",
                        sorteio.nextBoolean() ? "S01" : "S02",
                        nivelSorteado(sorteio),
                        100,
                        T0.plusSeconds(sorteio.nextInt(8)),             // recebidoEm
                        semTimestamp ? null : T0.plusSeconds(segundo)));
            }

            List<LeituraNivel> historicoInteiro = repository.findAll().stream()
                    .map(l -> new LeituraNivel(l.getNivelCm(), l.getTimestampDispositivo(),
                            l.getRecebidoEm(), l.getSensorId()))
                    .toList();
            ResultadoTendencia esperado = CalculadoraTendencia.calcular(
                    historicoInteiro, 1.5, SEM_LIMITE_DE_IDADE, AGORA);
            cobertura.merge(esperado.tendencia(), 1, Integer::sum);

            for (int tamanho : TAMANHOS_DE_PAGINA) {
                TendenciaService servico = new TendenciaService(repository,
                        new TendenciaProperties(1.5, SEM_LIMITE_DE_IDADE, tamanho), relogio);
                TendenciaResponse obtido = servico.calcularTendencia("J01").orElseThrow();

                String contexto = "caso " + caso + ", página de " + tamanho + ", histórico: "
                        + historicoInteiro;
                assertEquals(esperado.tendencia(), obtido.tendencia(), contexto);
                assertEquals(esperado.leiturasValidas(), obtido.leiturasValidas(), contexto);
                assertEquals(esperado.semTimestampDispositivoNaJanela(),
                        obtido.semTimestampDispositivoNaJanela(), contexto);
                assertEquals(esperado.multiplosSensorId(), obtido.multiplosSensorId(), contexto);
            }
        }

        // Só vale se o sorteio exercitou os desfechos que importam: confere a cobertura.
        for (Tendencia tendencia : List.of(Tendencia.INSUFICIENTE, Tendencia.INDETERMINADO,
                Tendencia.ENCHENDO, Tendencia.ESTAVEL, Tendencia.DRENANDO)) {
            assertTrue(cobertura.getOrDefault(tendencia, 0) >= 15,
                    "o sorteio quase não exercitou " + tendencia + ": " + cobertura);
        }
    }

    /** 25% de inválidos (nulo, NaN, infinito, negativo); o resto, 0,0 a 5,0 em passos de 0,5. */
    private static Double nivelSorteado(Random sorteio) {
        if (sorteio.nextInt(4) == 0) {
            return switch (sorteio.nextInt(5)) {
                case 0 -> null;
                case 1 -> Double.NaN;
                case 2 -> Double.POSITIVE_INFINITY;
                case 3 -> Double.NEGATIVE_INFINITY;
                default -> -1.0 - sorteio.nextInt(3);
            };
        }
        return sorteio.nextInt(11) * 0.5;
    }
}

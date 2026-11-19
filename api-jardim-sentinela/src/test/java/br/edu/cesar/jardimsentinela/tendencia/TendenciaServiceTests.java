package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import java.time.Duration;
import java.time.Instant;
import java.util.Optional;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.context.annotation.Import;
import org.springframework.data.domain.Pageable;
import org.springframework.test.context.TestPropertySource;
import org.springframework.test.context.bean.override.mockito.MockitoSpyBean;

import br.edu.cesar.jardimsentinela.dto.TendenciaResponse;
import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;
import br.edu.cesar.jardimsentinela.service.TendenciaService;

/**
 * [SCRUM-28][SW-14] Etapa 6, Bloco B: o serviço que consulta o histórico, converte as
 * leituras sem alterar valores, busca em páginas até ter a janela correta e chama a
 * calculadora com a tolerância, a idade máxima e o relógio configurados.
 *
 * Usa H2 e páginas de 2 leituras para que empates e leituras inválidas atravessem o
 * limite de uma página. Os dois cuidados obrigatórios da consulta (docs/tendencia-nivel.md,
 * seção 12): não usar margem arbitrária nos empates do corte e não concluir histórico
 * insuficiente só porque há inválidas entre as mais recentes.
 */
@SpringBootTest
@Import(RelogioDeTesteConfig.class)
@TestPropertySource(properties = "jardimsentinela.tendencia.tamanho-pagina=2")
class TendenciaServiceTests {

    @Autowired
    TendenciaService service;

    @MockitoSpyBean
    LeituraRepository repository;

    @Autowired
    RelogioControlado relogio;

    @BeforeEach
    void preparar() {
        repository.deleteAll();
        relogio.definir(LeiturasDeTeste.AGORA);
    }

    /** Grava uma leitura do sensor S01; recebidoEm = 1 s depois da medição. */
    private void salvar(Double nivel, int segundo) {
        salvar("S01", nivel, T0.plusSeconds(segundo), T0.plusSeconds(segundo + 1));
    }

    private void salvar(String sensorId, Double nivel, Instant timestampDispositivo, Instant recebidoEm) {
        repository.save(new Leitura("J01", sensorId, nivel, 100, recebidoEm, timestampDispositivo));
    }

    private TendenciaResponse calcular() {
        return service.calcularTendencia("J01").orElseThrow();
    }

    @Test
    void jardimSemNenhumaLeituraDevolveVazio() {
        assertTrue(service.calcularTendencia("J99").isEmpty());
    }

    @Test
    void cincoLeiturasSubindoDevolvemSubindoComOContextoDaResposta() {
        for (int i = 0; i < 5; i++) {
            salvar(10.0 + i, i);
        }

        assertEquals(
                new TendenciaResponse("J01", Tendencia.SUBINDO, 5, 0, 5, 1.5, 180, "NAO_VALIDADA",
                        false, false, T0.plusSeconds(5)),
                calcular());
    }

    @Test
    void invalidasEntreAsMaisRecentesNaoCausamInsuficienteFalso() {
        // Cuidado 2. As 5 válidas são as mais ANTIGAS; as 6 mais recentes são inválidas.
        // Com páginas de 2, as 3 primeiras páginas só têm inválidas: a busca tem de continuar.
        for (int i = 0; i < 5; i++) {
            salvar(10.0 + i, i);                         // válidas: 10..14 em T0+0..4
        }
        salvar(null, 5);
        salvar(Double.NaN, 6);
        salvar(-3.0, 7);
        salvar(Double.POSITIVE_INFINITY, 8);
        salvar(null, 9);
        salvar(-0.5, 10);

        TendenciaResponse resposta = calcular();

        assertEquals(Tendencia.SUBINDO, resposta.tendencia());
        assertEquals(5, resposta.leiturasValidas());
        assertEquals(6, resposta.leiturasDescartadas());
    }

    @Test
    void menosDeCincoValidasEmMeioAMuitasInvalidasEInsuficienteDeVerdade() {
        // 4 válidas misturadas a 7 inválidas: a busca vai até o fim do histórico e só então
        // conclui INSUFICIENTE (agora de forma legítima).
        salvar(null, 0);
        salvar(10.0, 1);
        salvar(Double.NaN, 2);
        salvar(11.0, 3);
        salvar(-1.0, 4);
        salvar(null, 5);
        salvar(12.0, 6);
        salvar(Double.NEGATIVE_INFINITY, 7);
        salvar(13.0, 8);
        salvar(null, 9);
        salvar(-2.0, 10);

        TendenciaResponse resposta = calcular();

        assertEquals(Tendencia.INSUFICIENTE, resposta.tendencia());
        assertEquals(4, resposta.leiturasValidas());
        assertEquals(7, resposta.leiturasDescartadas());
    }

    @Test
    void jardimSoComLeiturasInvalidasDevolveInsuficienteEAindaExiste() {
        // Há leituras (então não é 404), mas nenhuma válida. Sem última recepção: null.
        salvar(null, 0);
        salvar(Double.NaN, 1);
        salvar(-1.0, 2);

        TendenciaResponse resposta = calcular();

        assertEquals(Tendencia.INSUFICIENTE, resposta.tendencia());
        assertEquals(0, resposta.leiturasValidas());
        assertEquals(3, resposta.leiturasDescartadas());
        assertNull(resposta.ultimaRecepcaoEm());
    }

    @Test
    void empateAtravessandoOCorteEmVariasPaginasConsideraTodoOGrupo() {
        // Cuidado 1. Grupo empatado em T0 com 5 leituras: 20, 10, 12,5, 13 e 12,8 (nessa
        // ordem de gravação, então ids crescentes). Mais 13 em T0+1 e 14 em T0+2. A janela
        // são as 5 mais recentes: 14, 13 e TRÊS do grupo, sem saber quais. A primeira
        // leitura pode ser qualquer uma do grupo: 14-20 = -6 (DESCENDO) ou 14-10 = +4
        // (SUBINDO). Com uma margem fixa (só 5 linhas), a busca veria 12,8, 13 e 12,5 e
        // concluiria ESTAVEL, errado. Com páginas de 2, o grupo ocupa as páginas 1 a 3.
        Instant t0 = T0;
        salvar("S01", 20.0, t0, t0.plusSeconds(10));
        salvar("S01", 10.0, t0, t0.plusSeconds(11));
        salvar("S01", 12.5, t0, t0.plusSeconds(12));
        salvar("S01", 13.0, t0, t0.plusSeconds(13));
        salvar("S01", 12.8, t0, t0.plusSeconds(14));
        salvar("S01", 13.0, T0.plusSeconds(1), T0.plusSeconds(15));
        salvar("S01", 14.0, T0.plusSeconds(2), T0.plusSeconds(16));

        TendenciaResponse resposta = calcular();

        assertEquals(Tendencia.INDETERMINADO, resposta.tendencia());
        assertEquals(5, resposta.leiturasValidas());
    }

    @Test
    void aBuscaParaAssimQueTemAJanelaEOGrupoDoCorteCompleto() {
        // Critério de parada verificável: 10 leituras com chaves distintas, páginas de 2.
        // Páginas 0 e 1 trazem as 4 mais novas; a página 2 traz a 5ª e a 6ª. A 6ª é
        // ESTRITAMENTE mais antiga que a 5ª: prova que o grupo do corte (a 5ª sozinha)
        // está completo. A busca para na página 2, sem ler as páginas 3 e 4.
        for (int i = 0; i < 10; i++) {
            salvar((double) i, i);
        }
        clearInvocations(repository);

        TendenciaResponse resposta = calcular();

        assertEquals(Tendencia.SUBINDO, resposta.tendencia());   // janela: 5..9
        verify(repository, times(3)).findHistoricoPorChaveCronologica(eq("J01"), any(Pageable.class));
    }

    @Test
    void retransmissaoComHorariosForaDeOrdemUsaAOrdemDaMedicao() {
        // Medições 24, 20, 16, 12, 8 (o jardim drena). Chegada (e gravação): 8 primeiro,
        // depois 24, 20, 16, 12.
        salvar("S01", 8.0, T0.plusSeconds(4), T0.plusSeconds(10));
        salvar("S01", 24.0, T0.plusSeconds(0), T0.plusSeconds(11));
        salvar("S01", 20.0, T0.plusSeconds(1), T0.plusSeconds(12));
        salvar("S01", 16.0, T0.plusSeconds(2), T0.plusSeconds(13));
        salvar("S01", 12.0, T0.plusSeconds(3), T0.plusSeconds(14));

        assertEquals(Tendencia.DESCENDO, calcular().tendencia());
    }

    @Test
    void retransmissaoComSnapshotPrimeiroEEventosAntigosDepoisEscolheAJanelaPelaMedicao() {
        // O cenário do doc (seção 7.1) com mais de 5 leituras: a fila offline envia primeiro
        // o snapshot mais recente (T0+6, nível 14) e depois os eventos, do mais antigo ao
        // mais novo. Pela ordem de recebimento o snapshot seria o MAIS ANTIGO; ordenar por
        // recebidoEm e parar cedo escolheria a janela errada (29 a 13, DESCENDO). Pela
        // medição, a janela são as 5 mais recentes (T0+2 a T0+6): 10 a 14, SUBINDO.
        salvar("S01", 14.0, T0.plusSeconds(6), T0.plusSeconds(10));    // snapshot, chegou 1º
        salvar("S01", 30.0, T0.plusSeconds(0), T0.plusSeconds(11));
        salvar("S01", 29.0, T0.plusSeconds(1), T0.plusSeconds(12));
        salvar("S01", 10.0, T0.plusSeconds(2), T0.plusSeconds(13));
        salvar("S01", 11.0, T0.plusSeconds(3), T0.plusSeconds(14));
        salvar("S01", 12.0, T0.plusSeconds(4), T0.plusSeconds(15));
        salvar("S01", 13.0, T0.plusSeconds(5), T0.plusSeconds(16));

        assertEquals(Tendencia.SUBINDO, calcular().tendencia());
    }

    @Test
    void leituraSemTimestampDeDispositivoLigaOSinalEUsaORecebidoEm() {
        salvar("S01", 10.0, T0, T0.plusSeconds(1));
        salvar("S01", 11.0, null, T0.plusSeconds(2));       // sem timestampDispositivo
        salvar("S01", 12.0, T0.plusSeconds(3), T0.plusSeconds(4));
        salvar("S01", 13.0, T0.plusSeconds(4), T0.plusSeconds(5));
        salvar("S01", 14.0, T0.plusSeconds(5), T0.plusSeconds(6));

        TendenciaResponse resposta = calcular();

        assertEquals(Tendencia.SUBINDO, resposta.tendencia());
        assertTrue(resposta.semTimestampDispositivoNaJanela());
    }

    @Test
    void maisDeUmSensorIdLigaOSinal() {
        salvar("S01", 10.0, T0, T0.plusSeconds(1));
        salvar("S02", 11.0, T0.plusSeconds(1), T0.plusSeconds(2));
        salvar("S01", 12.0, T0.plusSeconds(2), T0.plusSeconds(3));
        salvar("S01", 13.0, T0.plusSeconds(3), T0.plusSeconds(4));
        salvar("S01", 14.0, T0.plusSeconds(4), T0.plusSeconds(5));

        assertTrue(calcular().multiplosSensorId());
    }

    @Test
    void idadeMaximaUsaORelogioInjetado() {
        for (int i = 0; i < 5; i++) {
            salvar(10.0 + i, i);
        }
        Instant ultimaRecepcao = T0.plusSeconds(5);

        relogio.definir(ultimaRecepcao.plus(Duration.ofSeconds(180)));
        assertEquals(Tendencia.SUBINDO, calcular().tendencia(), "180 s exatos ainda é atual");

        relogio.avancar(Duration.ofNanos(1));
        assertEquals(Tendencia.DESATUALIZADO, calcular().tendencia(), "acima de 180 s é desatualizado");
    }

    @Test
    void servicoDevolveOptionalVazioSoQuandoNaoHaNenhumaLeitura() {
        salvar(null, 0);

        Optional<TendenciaResponse> resposta = service.calcularTendencia("J01");

        assertTrue(resposta.isPresent());
    }
}

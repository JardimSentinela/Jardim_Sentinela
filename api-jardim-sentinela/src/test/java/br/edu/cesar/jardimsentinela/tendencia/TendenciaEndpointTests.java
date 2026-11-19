package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.not;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.content;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import java.time.Duration;
import java.time.Instant;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.webmvc.test.autoconfigure.AutoConfigureMockMvc;
import org.springframework.context.annotation.Import;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.ResultActions;

import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

/**
 * [SCRUM-28][SW-14] Etapa 6, Bloco C: {@code GET /api/jardins/{id}/tendencia} de ponta a
 * ponta (MockMvc, serviço, repositório e H2). HTTP 200 para os seis estados e 404 só para
 * jardim sem nenhuma leitura. A idade usa um relógio controlado.
 */
@SpringBootTest
@AutoConfigureMockMvc
@Import(RelogioDeTesteConfig.class)
class TendenciaEndpointTests {

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @Autowired
    RelogioControlado relogio;

    @BeforeEach
    void preparar() {
        repository.deleteAll();
        relogio.definir(LeiturasDeTeste.AGORA);
    }

    // ---- apoio ----

    /** Grava direto no banco (controle total dos horários). recebidoEm = 1 s após a medição. */
    private void gravar(String sensorId, Double nivel, int segundo) {
        repository.save(new Leitura("J01", sensorId, nivel, 100, T0.plusSeconds(segundo + 1),
                T0.plusSeconds(segundo)));
    }

    private void gravarSerie(double... niveis) {
        for (int i = 0; i < niveis.length; i++) {
            gravar("S01", niveis[i], i);
        }
    }

    /** Envia pelo endpoint de ingestão, como o firmware/simulador; timestamp pode ser null. */
    private void postar(String sensor, double nivel, String timestamp) throws Exception {
        String campoTimestamp = timestamp == null ? "" : ",\"timestamp\":\"" + timestamp + "\"";
        mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {"idJardim":"J01","idSensor":"%s","nivelAgua":%s,"bateria":90%s}
                        """.formatted(sensor, nivel, campoTimestamp)))
                .andExpect(status().isCreated());
    }

    private ResultActions tendencia() throws Exception {
        return mvc.perform(get("/api/jardins/J01/tendencia"));
    }

    private static String t(int segundo) {
        return T0.plusSeconds(segundo).toString();
    }

    /** O mais tardio dos recebidoEm gravados; os testes por POST usam o relógio real. */
    private Instant ultimaRecepcaoGravada() {
        return repository.findAll().stream().map(Leitura::getRecebidoEm).max(Instant::compareTo).orElseThrow();
    }

    // ---- os seis estados, todos com HTTP 200 ----

    @Test
    void subindo() throws Exception {
        gravarSerie(10, 11, 12, 13, 14);
        tendencia().andExpect(status().isOk()).andExpect(jsonPath("$.tendencia").value("SUBINDO"));
    }

    @Test
    void estavel() throws Exception {
        gravarSerie(10, 10.4, 9.8, 10.3, 10.6);
        tendencia().andExpect(status().isOk()).andExpect(jsonPath("$.tendencia").value("ESTAVEL"));
    }

    @Test
    void descendo() throws Exception {
        gravarSerie(14, 13, 12, 11, 10);
        tendencia().andExpect(status().isOk()).andExpect(jsonPath("$.tendencia").value("DESCENDO"));
    }

    @Test
    void insuficiente() throws Exception {
        gravarSerie(10, 11, 12, 13);
        tendencia().andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("INSUFICIENTE"))
                .andExpect(jsonPath("$.leiturasValidas").value(4));
    }

    @Test
    void indeterminado() throws Exception {
        // Empate na última posição (@3): 14,0 (+4,0 SUBINDO) ou 10,5 (+0,5 ESTAVEL).
        gravar("S01", 10.0, 0);
        gravar("S01", 11.0, 1);
        gravar("S01", 12.0, 2);
        gravar("S01", 14.0, 3);
        gravar("S02", 10.5, 3);
        tendencia().andExpect(status().isOk()).andExpect(jsonPath("$.tendencia").value("INDETERMINADO"));
    }

    @Test
    void desatualizado() throws Exception {
        gravarSerie(10, 11, 12, 13, 14);
        relogio.definir(T0.plusSeconds(5).plus(Duration.ofSeconds(31)));   // última recepção + 31 s
        tendencia().andExpect(status().isOk()).andExpect(jsonPath("$.tendencia").value("DESATUALIZADO"));
    }

    // ---- 404 e formato ----

    @Test
    void jardimSemNenhumaLeituraRetorna404ComOCorpoPadraoDaApi() throws Exception {
        mvc.perform(get("/api/jardins/J99/tendencia"))
                .andExpect(status().isNotFound())
                .andExpect(jsonPath("$.status").value(404))
                .andExpect(jsonPath("$.erro").value("JARDIM_NAO_ENCONTRADO"))
                .andExpect(jsonPath("$.campos.length()").value(0));
    }

    @Test
    void jsonTemExatamenteOsOnzeCampos() throws Exception {
        gravarSerie(10, 11, 12, 13, 14);

        tendencia().andExpect(status().isOk())
                .andExpect(content().contentTypeCompatibleWith(MediaType.APPLICATION_JSON))
                .andExpect(jsonPath("$.length()").value(11))
                .andExpect(jsonPath("$.jardimId").value("J01"))
                .andExpect(jsonPath("$.tendencia").value("SUBINDO"))
                .andExpect(jsonPath("$.leiturasValidas").value(5))
                .andExpect(jsonPath("$.leiturasDescartadas").value(0))
                .andExpect(jsonPath("$.janelaLeituras").value(5))
                .andExpect(jsonPath("$.toleranciaCm").value(1.5))
                .andExpect(jsonPath("$.idadeMaximaSegundos").value(180))
                .andExpect(jsonPath("$.confiabilidadeCronologia").value("NAO_VALIDADA"))
                .andExpect(jsonPath("$.semTimestampDispositivoNaJanela").value(false))
                .andExpect(jsonPath("$.multiplosSensorId").value(false))
                .andExpect(jsonPath("$.ultimaRecepcaoEm").value("2026-11-17T12:00:05Z"))
                // Nada além do contrato: sem variação líquida, sem id nem horários internos.
                .andExpect(content().string(not(containsString("variacao"))));
    }

    @Test
    void ultimaRecepcaoEmNulaEhSerializadaComoNullSemInventarData() throws Exception {
        // Decisão de 2026-11-19: sem nenhum recebidoEm não há como medir a idade.
        for (int i = 0; i < 5; i++) {
            repository.save(new Leitura("J01", "S01", 10.0 + i, 100, null, T0.plusSeconds(i)));
        }

        tendencia().andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("SUBINDO"))
                .andExpect(jsonPath("$.length()").value(11))
                .andExpect(content().string(containsString("\"ultimaRecepcaoEm\":null")));
    }

    @Test
    void jardimSoComInvalidasTemRespostaEUltimaRecepcaoNula() throws Exception {
        gravar("S01", null, 0);
        gravar("S01", -1.0, 1);

        tendencia().andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("INSUFICIENTE"))
                .andExpect(jsonPath("$.leiturasValidas").value(0))
                .andExpect(jsonPath("$.leiturasDescartadas").value(2))
                .andExpect(content().string(containsString("\"ultimaRecepcaoEm\":null")));
    }

    // ---- cenários de dados ----

    @Test
    void leiturasInvalidasSaoDescartadasEContadasNaResposta() throws Exception {
        gravar("S01", null, 0);
        gravar("S01", 10.0, 1);
        gravar("S01", Double.NaN, 2);
        gravar("S01", 11.0, 3);
        gravar("S01", -2.0, 4);
        gravar("S01", 12.0, 5);
        gravar("S01", Double.POSITIVE_INFINITY, 6);
        gravar("S01", 13.0, 7);
        gravar("S01", 14.0, 8);

        tendencia().andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("SUBINDO"))
                .andExpect(jsonPath("$.leiturasValidas").value(5))
                .andExpect(jsonPath("$.leiturasDescartadas").value(4));
    }

    @Test
    void empateCronologicoRecebidoPeloEndpointDeIngestaoEIndeterminado() throws Exception {
        // Dois sensores medem no mesmo segundo (a ingestão aceita: a chave de idempotência
        // inclui o sensor): 14,0 do S01 e 10,5 do S02 em T0+3.
        postar("S01", 10.0, t(0));
        postar("S01", 11.0, t(1));
        postar("S01", 12.0, t(2));
        postar("S01", 14.0, t(3));
        postar("S02", 10.5, t(3));
        relogio.definir(ultimaRecepcaoGravada().plusSeconds(1));

        tendencia().andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("INDETERMINADO"))
                .andExpect(jsonPath("$.multiplosSensorId").value(true));
    }

    @Test
    void retransmissaoComHorariosForaDeOrdemUsaAOrdemDaMedicao() throws Exception {
        // Fila offline: o snapshot mais recente (T0+6) chega primeiro; depois os eventos,
        // do mais antigo ao mais novo. A medição sobe de 10 a 14 (T0+2 a T0+6).
        postar("S01", 14.0, t(6));
        postar("S01", 30.0, t(0));
        postar("S01", 29.0, t(1));
        postar("S01", 10.0, t(2));
        postar("S01", 11.0, t(3));
        postar("S01", 12.0, t(4));
        postar("S01", 13.0, t(5));
        relogio.definir(ultimaRecepcaoGravada().plusSeconds(1));

        tendencia().andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("SUBINDO"))
                .andExpect(jsonPath("$.leiturasValidas").value(5))
                .andExpect(jsonPath("$.confiabilidadeCronologia").value("NAO_VALIDADA"));
    }

    @Test
    void leituraSemTimestampDoDispositivoUsaORecebidoEmELigaOSinal() throws Exception {
        // Sem "timestamp": a ordem é a de recebimento. Degraus grandes: mesmo que dois
        // recebimentos caiam no mesmo instante, a classificação não muda.
        postar("S01", 10.0, null);
        postar("S01", 20.0, null);
        postar("S01", 30.0, null);
        postar("S01", 40.0, null);
        postar("S01", 50.0, null);
        relogio.definir(ultimaRecepcaoGravada().plusSeconds(1));

        tendencia().andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("SUBINDO"))
                .andExpect(jsonPath("$.semTimestampDispositivoNaJanela").value(true));
    }

    // ---- idade máxima com relógio controlado ----

    @Test
    void idadeMaximaPadraoDe180SegundosUsaORelogioInjetado() throws Exception {
        for (int i = 0; i < 5; i++) {
            postar("S01", 10.0 + i, t(i));
        }
        Instant ultima = ultimaRecepcaoGravada();

        relogio.definir(ultima.plusSeconds(180));
        tendencia().andExpect(jsonPath("$.tendencia").value("SUBINDO"));      // 180 s exatos: atual

        relogio.avancar(Duration.ofNanos(1));
        tendencia().andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("DESATUALIZADO"))      // acima: desatualizado
                .andExpect(jsonPath("$.leiturasValidas").value(5));
    }
}

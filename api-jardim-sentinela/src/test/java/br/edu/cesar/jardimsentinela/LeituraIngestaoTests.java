package br.edu.cesar.jardimsentinela;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.webmvc.test.autoconfigure.AutoConfigureMockMvc;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;

import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

/**
 * [SCRUM-43][SW-09] e [SCRUM-44][SW-10]: cobre o ponto que SW-12 (validar
 * integração) cobra como "correções de contrato" e "validar exemplos JSON" —
 * as duas fontes (simulador e firmware) batendo no mesmo endpoint, mais
 * idempotência básica e tratamento de erro.
 */
@SpringBootTest
@AutoConfigureMockMvc
class LeituraIngestaoTests {

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    @Test
    void aceitaVocabularioDoSimulador() throws Exception {
        // Exemplo real do GeradorDadosJardim.proximaLeituraJson().
        mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {"jardimId":"J01","sensorId":"S01","nivelCm":18.5,"bateria":97}
                        """))
                .andExpect(status().isCreated())
                .andExpect(jsonPath("$.jardimId").value("J01"))
                .andExpect(jsonPath("$.nivelCm").value(18.5));
    }

    @Test
    void aceitaVocabularioDoFirmwareNoMesmoEndpoint() throws Exception {
        // Mesmo formato da linha gravada por esp32/src/main_alt.cpp (persistir()), com os ids no
        // padrão do dicionário (J01/S02). O firmware ainda emite jardim-centro-01 (legado a migrar).
        mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {"idJardim":"J01","idSensor":"S02",
                         "nivelAgua":10.0,"bateria":100,"timestamp":"2026-10-15T00:00:01Z"}
                        """))
                .andExpect(status().isCreated())
                .andExpect(jsonPath("$.jardimId").value("J01"))
                .andExpect(jsonPath("$.sensorId").value("S02"))
                .andExpect(jsonPath("$.nivelCm").value(10.0));
    }

    @Test
    void reenvioComMesmoJardimSensorETimestampNaoDuplicaLeitura() throws Exception {
        String payload = """
                {"idJardim":"J01","idSensor":"S02","nivelAgua":10.0,
                 "bateria":100,"timestamp":"2026-10-15T00:00:01Z"}
                """;

        mvc.perform(post("/api/leituras").contentType(MediaType.APPLICATION_JSON).content(payload))
                .andExpect(status().isCreated());

        // Mesma leitura chega de novo (retransmissão típica de enlace instável/offline).
        mvc.perform(post("/api/leituras").contentType(MediaType.APPLICATION_JSON).content(payload))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.jardimId").value("J01"));

        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.totalLeituras").value(1));
    }

    @Test
    void camposObrigatoriosAusentesRetornaPayloadInvalido() throws Exception {
        mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {"sensorId":"S01","nivelCm":18.5,"bateria":97}
                        """))
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.erro").value("PAYLOAD_INVALIDO"))
                .andExpect(jsonPath("$.campos[0].campo").value("jardimId"));
    }

    @Test
    void corpoComJsonInvalidoRetornaCorpoIlegivel() throws Exception {
        mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{ isso nao e json"))
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.erro").value("CORPO_ILEGIVEL"));
    }
}

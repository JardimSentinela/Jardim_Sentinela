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
import org.springframework.test.web.servlet.ResultActions;

import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

/**
 * Garante que Wokwi (ESP32) e simulador em software usam o mesmo contrato de
 * ingestão e são processados pelo mesmo endpoint, sem ramificação por origem.
 */
@SpringBootTest
@AutoConfigureMockMvc
class TelemetriaIntercambiavelTests {

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    /** Linha completa como o firmware do Wokwi envia (contrato + metadados). */
    private String payloadWokwi(String idJardim, String idSensor, double nivelAgua,
                                int bateria, String timestamp, int seq, String origem) {
        return """
                {"tipo":"evento","idJardim":"%s","idSensor":"%s","nivelAgua":%s,
                 "bateria":%d,"timestamp":"%s","seq":%d,"bootId":"A1B2C3D4",
                 "deltaTMs":3000,"horaSincronizada":true,"origem":"%s"}
                """.formatted(idJardim, idSensor, nivelAgua, bateria, timestamp, seq, origem);
    }

    /** Payload mínimo do simulador em software: apenas o contrato obrigatório. */
    private String payloadSimulador(String idJardim, String idSensor, double nivelAgua,
                                    int bateria, String timestamp) {
        return """
                {"idJardim":"%s","idSensor":"%s","nivelAgua":%s,
                 "bateria":%d,"timestamp":"%s"}
                """.formatted(idJardim, idSensor, nivelAgua, bateria, timestamp);
    }

    private ResultActions enviar(String json) throws Exception {
        return mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content(json))
                .andExpect(status().isCreated());
    }

    @Test
    void fontesAlternadasSaoProcessadasPeloMesmoEndpoint() throws Exception {
        enviar(payloadWokwi("jardim-centro-01", "sensor-01", 5.0, 100,
                "2026-11-12T12:00:00Z", 2, "simulacao"))
                .andExpect(jsonPath("$.timestampDispositivo").value("2026-11-12T12:00:00Z"));

        enviar(payloadSimulador("jardim-centro-01", "sensor-01", 10.0, 99,
                "2026-11-12T12:00:01Z"));

        enviar(payloadWokwi("jardim-centro-01", "sensor-01", 15.0, 98,
                "2026-11-12T12:00:02Z", 3, "simulacao"));

        enviar(payloadSimulador("jardim-centro-01", "sensor-01", 0.0, 97,
                "2026-11-12T12:00:03Z"));

        mvc.perform(get("/api/jardins/jardim-centro-01"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.jardimId").value("jardim-centro-01"))
                .andExpect(jsonPath("$.nivelCm").value(0.0))
                .andExpect(jsonPath("$.totalLeituras").value(4));

        mvc.perform(get("/api/jardins/jardim-centro-01/leituras"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.length()").value(4))
                .andExpect(jsonPath("$[0].nivelCm").value(0.0))
                .andExpect(jsonPath("$[0].timestampDispositivo").value("2026-11-12T12:00:03Z"))
                .andExpect(jsonPath("$[3].nivelCm").value(5.0))
                .andExpect(jsonPath("$[3].timestampDispositivo").value("2026-11-12T12:00:00Z"));
    }

    @Test
    void origemDoWokwiNaoAlteraOProcessamento() throws Exception {
        enviar(payloadWokwi("jardim-centro-01", "jardim", 0.0, 100,
                "2026-11-12T12:00:00Z", 0, "campo"));
        enviar(payloadWokwi("jardim-centro-01", "jardim", 1.0, 100,
                "2026-11-12T12:00:05Z", 1, "simulacao"));

        mvc.perform(get("/api/jardins/jardim-centro-01"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.totalLeituras").value(2));
    }

    @Test
    void nomesAntigosDoSimuladorSaoAceitosPelaPonte() throws Exception {
        // Ponte transitória (@JsonAlias): jardimId/sensorId/nivelCm continuam válidos.
        mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {"jardimId":"J01","sensorId":"S01","nivelCm":10.0,"bateria":90}
                        """))
                .andExpect(status().isCreated());
    }

    @Test
    void timestampEhOpcionalENaoAtivaIdempotencia() throws Exception {
        String semTimestamp = """
                {"idJardim":"J01","idSensor":"S01","nivelAgua":10.0,"bateria":90}
                """;
        enviar(semTimestamp);
        enviar(semTimestamp);

        mvc.perform(get("/api/jardins/J01"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.totalLeituras").value(2));
    }

    @Test
    void timestampInvalidoEhRejeitado() throws Exception {
        mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {"idJardim":"J01","idSensor":"S01","nivelAgua":10.0,
                         "bateria":90,"timestamp":"12/11/2026 12:00"}
                        """))
                .andExpect(status().isBadRequest());
    }

    @Test
    void nivelDeAguaNegativoEhRejeitado() throws Exception {
        mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {"idJardim":"J01","idSensor":"S01","nivelAgua":-1.0,
                         "bateria":90,"timestamp":"2026-11-12T12:00:00Z"}
                        """))
                .andExpect(status().isBadRequest());
    }
}

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

@SpringBootTest
@AutoConfigureMockMvc
class JardimControllerTests {

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    private void enviar(String jardimId, double nivelCm, int bateria) throws Exception {
        mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {"jardimId":"%s","sensorId":"S-%s","nivelCm":%s,"bateria":%d}
                        """.formatted(jardimId, jardimId, nivelCm, bateria)))
                .andExpect(status().isCreated());
    }

    @Test
    void jardimRetornaUltimaLeituraETotal() throws Exception {
        enviar("J01", 10.0, 90);
        enviar("J01", 22.5, 89);
        enviar("J02", 5.0, 70);

        mvc.perform(get("/api/jardins/J01"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.jardimId").value("J01"))
                .andExpect(jsonPath("$.nivelCm").value(22.5))
                .andExpect(jsonPath("$.bateria").value(89))
                .andExpect(jsonPath("$.totalLeituras").value(2));
    }

    @Test
    void jardimInexistenteRetorna404() throws Exception {
        mvc.perform(get("/api/jardins/J99")).andExpect(status().isNotFound());
        mvc.perform(get("/api/jardins/J99/leituras")).andExpect(status().isNotFound());
    }

    @Test
    void historicoVemDoMaisRecenteParaOMaisAntigoEFiltraPorJardim() throws Exception {
        enviar("J01", 1.0, 90);
        enviar("J01", 2.0, 90);
        enviar("J01", 3.0, 90);
        enviar("J02", 99.0, 90);

        mvc.perform(get("/api/jardins/J01/leituras"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.length()").value(3))
                .andExpect(jsonPath("$[0].nivelCm").value(3.0))
                .andExpect(jsonPath("$[2].nivelCm").value(1.0));
    }

    @Test
    void historicoRespeitaLimite() throws Exception {
        enviar("J01", 1.0, 90);
        enviar("J01", 2.0, 90);
        enviar("J01", 3.0, 90);

        mvc.perform(get("/api/jardins/J01/leituras").param("limite", "2"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.length()").value(2))
                .andExpect(jsonPath("$[0].nivelCm").value(3.0));
    }
}

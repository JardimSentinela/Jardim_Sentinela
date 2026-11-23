package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static org.hamcrest.Matchers.nullValue;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import java.time.Duration;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.webmvc.test.autoconfigure.AutoConfigureMockMvc;
import org.springframework.context.annotation.Import;
import org.springframework.test.web.servlet.MockMvc;

import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

/**
 * [SCRUM-28][SW-14] GET /api/jardins/{id} passa a preencher o campo "tendencia" com a mesma
 * regra de GET /api/jardins/{id}/tendencia (mesmo serviço). O campo "estado" continua
 * vindo do registro, sem recálculo, e os demais campos não mudam.
 */
@SpringBootTest
@AutoConfigureMockMvc
@Import(RelogioDeTesteConfig.class)
class JardimTendenciaTests {

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @Autowired
    RelogioControlado relogio;

    @BeforeEach
    void preparar() {
        repository.deleteAll();
        relogio.definir(T0.plusSeconds(10));
    }

    private void gravarSerie(double... niveis) {
        for (int i = 0; i < niveis.length; i++) {
            repository.save(new Leitura("J01", "S01", niveis[i], 100, T0.plusSeconds(i + 1),
                    T0.plusSeconds(i)));
        }
    }

    @Test
    void jardimComNivelSubindoTraTendenciaSubindo() throws Exception {
        gravarSerie(10, 11, 12, 13, 14);
        mvc.perform(get("/api/jardins/J01"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("SUBINDO"));
    }

    @Test
    void jardimComNivelDescendoTraTendenciaDescendo() throws Exception {
        gravarSerie(14, 13, 12, 11, 10);
        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.tendencia").value("DESCENDO"));
    }

    @Test
    void jardimComNivelEstavelTraTendenciaEstavel() throws Exception {
        gravarSerie(10, 10.5, 10, 10.5, 10);
        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.tendencia").value("ESTAVEL"));
    }

    @Test
    void poucasLeiturasTrazInsuficiente() throws Exception {
        gravarSerie(10, 11, 12);
        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.tendencia").value("INSUFICIENTE"));
    }

    @Test
    void ultimaRecepcaoAcimaDaIdadeMaximaTraDesatualizado() throws Exception {
        gravarSerie(10, 11, 12, 13, 14);
        relogio.definir(T0.plusSeconds(5).plus(Duration.ofSeconds(181)));   // limite padrão: 180 s
        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.tendencia").value("DESATUALIZADO"));
    }

    @Test
    void mesmaRegraDoEndpointDeTendencia() throws Exception {
        gravarSerie(10, 11, 12, 13, 14);
        relogio.definir(T0.plusSeconds(5).plus(Duration.ofSeconds(180)));   // 180 s exatos: atual
        String doEndpoint = com.jayway.jsonpath.JsonPath.read(
                mvc.perform(get("/api/jardins/J01/tendencia")).andReturn().getResponse().getContentAsString(),
                "$.tendencia");
        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.tendencia").value(doEndpoint));
    }

    @Test
    void estadoContinuaVindoDoRegistroEOsDemaisCamposNaoMudam() throws Exception {
        gravarSerie(10, 11, 12, 13, 14);
        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.jardimId").value("J01"))
                .andExpect(jsonPath("$.nivelCm").value(14.0))
                .andExpect(jsonPath("$.totalLeituras").value(5))
                .andExpect(jsonPath("$.estado").value(nullValue()))      // não é recalculado
                .andExpect(jsonPath("$.tendencia").value("SUBINDO"));
    }

    @Test
    void jardimSemLeituraContinuaRetornando404() throws Exception {
        mvc.perform(get("/api/jardins/J99"))
                .andExpect(status().isNotFound())
                .andExpect(jsonPath("$.erro").value("JARDIM_NAO_ENCONTRADO"));
    }
}

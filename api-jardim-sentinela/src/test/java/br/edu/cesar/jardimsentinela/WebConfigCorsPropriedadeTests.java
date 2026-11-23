package br.edu.cesar.jardimsentinela;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.header;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.webmvc.test.autoconfigure.AutoConfigureMockMvc;
import org.springframework.http.HttpHeaders;
import org.springframework.test.context.TestPropertySource;
import org.springframework.test.web.servlet.MockMvc;

/** [SCRUM-20][SW-11] A lista de origens é configurável por jardimsentinela.cors.allowed-origins. */
@SpringBootTest
@AutoConfigureMockMvc
@TestPropertySource(properties = "jardimsentinela.cors.allowed-origins=https://painel.exemplo.org")
class WebConfigCorsPropriedadeTests {

    @Autowired
    MockMvc mvc;

    @Test
    void usaAOrigemConfiguradaPorPropriedade() throws Exception {
        mvc.perform(get("/api/leituras").header(HttpHeaders.ORIGIN, "https://painel.exemplo.org"))
                .andExpect(header().string(HttpHeaders.ACCESS_CONTROL_ALLOW_ORIGIN, "https://painel.exemplo.org"));
    }

    @Test
    void origemPadraoDeixaDeSerPermitidaQuandoAPropriedadeMuda() throws Exception {
        mvc.perform(get("/api/leituras").header(HttpHeaders.ORIGIN, "http://localhost:5173"))
                .andExpect(header().doesNotExist(HttpHeaders.ACCESS_CONTROL_ALLOW_ORIGIN));
    }
}

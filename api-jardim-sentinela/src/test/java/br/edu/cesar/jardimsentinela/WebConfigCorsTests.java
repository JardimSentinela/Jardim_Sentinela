package br.edu.cesar.jardimsentinela;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.options;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.header;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.webmvc.test.autoconfigure.AutoConfigureMockMvc;
import org.springframework.http.HttpHeaders;
import org.springframework.test.web.servlet.MockMvc;

/**
 * [SCRUM-20][SW-11] Escrito ANTES de WebConfig.java existir (ciclo TDD):
 * sem CORS configurado, a API não manda nenhum cabeçalho Access-Control-*,
 * então as duas asserções abaixo devem falhar (RED). Depois que WebConfig.java
 * registra o CorsRegistry, devem passar (GREEN).
 *
 * [Dado a verificar] Não executei este teste — não compilo no ambiente onde
 * preparei isso. A lógica de preflight do Spring pode ter mudado no Spring
 * Framework 7 (base do Boot 4.1.1); por isso as asserções são propositalmente
 * fracas (existência do cabeçalho, não o valor exato).
 */
@SpringBootTest
@AutoConfigureMockMvc
class WebConfigCorsTests {

    @Autowired
    MockMvc mvc;

    @Test
    void respostaDeLeiturasIncluiCabecalhoCorsQuandoOrigemEInformada() throws Exception {
        // Requisição "simples" (GET) com Origin: o navegador não manda preflight
        // pra isso, mas espera o cabeçalho de volta pra liberar a leitura da resposta.
        mvc.perform(get("/api/leituras").header(HttpHeaders.ORIGIN, "http://localhost:5173"))
                .andExpect(status().isOk())
                .andExpect(header().exists(HttpHeaders.ACCESS_CONTROL_ALLOW_ORIGIN));
    }

    @Test
    void preflightParaPostEmLeiturasLiberaOMetodo() throws Exception {
        // Preflight: o que o navegador manda ANTES de um POST "não simples"
        // (com Content-Type: application/json) de outra origem.
        mvc.perform(options("/api/leituras")
                        .header(HttpHeaders.ORIGIN, "http://localhost:5173")
                        .header(HttpHeaders.ACCESS_CONTROL_REQUEST_METHOD, "POST"))
                .andExpect(status().isOk())
                .andExpect(header().exists(HttpHeaders.ACCESS_CONTROL_ALLOW_METHODS));
    }
}

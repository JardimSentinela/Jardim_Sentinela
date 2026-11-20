package br.edu.cesar.jardimsentinela;

import static org.hamcrest.Matchers.nullValue;
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
import org.springframework.test.web.servlet.ResultActions;
import org.springframework.test.web.servlet.MockMvc;

import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

/**
 * [SCRUM-20][SW-11] Campos opcionais estado, tendenciaDispositivo e versaoRegra
 * (docs/decisoes-tecnicas.md §6.3). Guardados como vieram, nunca recalculados.
 */
@SpringBootTest
@AutoConfigureMockMvc
class LeituraCamposEstadoTests {

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    private ResultActions enviar(String camposExtras) throws Exception {
        return mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {"idJardim":"J01","idSensor":"S01","nivelAgua":5.0,"bateria":100,
                         "timestamp":"2026-11-12T12:00:00Z"%s}
                        """.formatted(camposExtras)));
    }

    @Test
    void camposNovosSaoArmazenadosSemRecalculo() throws Exception {
        // 5 cm seria NORMAL pelas faixas; o servidor NÃO reclassifica: guarda o que o dispositivo mandou.
        enviar(",\"estado\":\"ALAGADO\",\"tendenciaDispositivo\":\"SUBINDO\","
                + "\"versaoRegra\":\"v1-provisoria-2026-11\"")
                .andExpect(status().isCreated())
                .andExpect(jsonPath("$.estado").value("ALAGADO"))
                .andExpect(jsonPath("$.tendenciaDispositivo").value("SUBINDO"))
                .andExpect(jsonPath("$.versaoRegra").value("v1-provisoria-2026-11"));

        mvc.perform(get("/api/jardins/J01/leituras"))
                .andExpect(jsonPath("$[0].estado").value("ALAGADO"))
                .andExpect(jsonPath("$[0].tendenciaDispositivo").value("SUBINDO"))
                .andExpect(jsonPath("$[0].versaoRegra").value("v1-provisoria-2026-11"));
    }

    @Test
    void semCamposNovosFicamNulos() throws Exception {
        enviar("")
                .andExpect(status().isCreated())
                .andExpect(jsonPath("$.estado").value(nullValue()))
                .andExpect(jsonPath("$.tendenciaDispositivo").value(nullValue()))
                .andExpect(jsonPath("$.versaoRegra").value(nullValue()));
    }

    @Test
    void aceitaTodosOsValoresDeEstadoETendencia() throws Exception {
        String[] estados = {"NORMAL", "ENCHENDO", "TRANSBORDANDO", "ALAGADO"};
        String[] tendencias = {"SUBINDO", "ESTAVEL", "DESCENDO", "INSUFICIENTE", "INDETERMINADO", "DESATUALIZADO"};
        int i = 0;
        for (String estado : estados) {
            enviarComTimestamp(i++, ",\"estado\":\"" + estado + "\"").andExpect(status().isCreated());
        }
        for (String tendencia : tendencias) {
            enviarComTimestamp(i++, ",\"tendenciaDispositivo\":\"" + tendencia + "\"").andExpect(status().isCreated());
        }
    }

    private ResultActions enviarComTimestamp(int n, String extras) throws Exception {
        return mvc.perform(post("/api/leituras")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {"idJardim":"J01","idSensor":"S01","nivelAgua":5.0,"bateria":100,
                         "timestamp":"2026-11-12T12:00:%02dZ"%s}
                        """.formatted(n, extras)));
    }

    @Test
    void estadoForaDaListaRetorna400PayloadInvalido() throws Exception {
        enviar(",\"estado\":\"SECO\"")
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.erro").value("PAYLOAD_INVALIDO"))
                .andExpect(jsonPath("$.campos[0].campo").value("estado"));
    }

    @Test
    void estadoEmMinusculasRetorna400() throws Exception {
        enviar(",\"estado\":\"normal\"")
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.erro").value("PAYLOAD_INVALIDO"));
    }

    @Test
    void tendenciaForaDaListaRetorna400PayloadInvalido() throws Exception {
        enviar(",\"tendenciaDispositivo\":\"ENCHENDO\"")
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.erro").value("PAYLOAD_INVALIDO"))
                .andExpect(jsonPath("$.campos[0].campo").value("tendenciaDispositivo"));
    }

    @Test
    void versaoRegraMuitoLongaRetorna400() throws Exception {
        enviar(",\"versaoRegra\":\"" + "x".repeat(65) + "\"")
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.erro").value("PAYLOAD_INVALIDO"))
                .andExpect(jsonPath("$.campos[0].campo").value("versaoRegra"));
    }

    @Test
    void estadoAtualDoJardimTrazEstadoDoRegistroETendenciaCalculada() throws Exception {
        enviar(",\"estado\":\"ENCHENDO\"").andExpect(status().isCreated());

        mvc.perform(get("/api/jardins/J01"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.estado").value("ENCHENDO"))
                // tendência calculada pela API (SW-14): com 1 leitura, menos que as 5 da janela.
                .andExpect(jsonPath("$.tendencia").value("INSUFICIENTE"));
    }

    @Test
    void estadoAtualDeRegistroAntigoSemClassificacaoVemNulo() throws Exception {
        enviar("").andExpect(status().isCreated());

        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.estado").value(nullValue()));
    }
}

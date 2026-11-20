package br.edu.cesar.jardimsentinela;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import java.time.Instant;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.webmvc.test.autoconfigure.AutoConfigureMockMvc;
import org.springframework.test.web.servlet.MockMvc;

import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

/** [SCRUM-20][SW-11] GET /api/jardins: resumo por jardim (decisões §6.4). */
@SpringBootTest
@AutoConfigureMockMvc
class ListaJardinsTests {

    private static final Instant BASE = Instant.parse("2026-11-12T12:00:00Z");

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    private void gravar(String jardim, double nivel, Instant recebidoEm, Instant timestamp, String estado) {
        repository.save(new Leitura(jardim, "S" + jardim.substring(1), nivel, 90, recebidoEm, timestamp,
                estado, null, null));
    }

    @Test
    void semDadosRetornaListaVazia() throws Exception {
        mvc.perform(get("/api/jardins"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.length()").value(0));
    }

    @Test
    void listaResumoOrdenadoPorJardimIdESemJardimSemLeitura() throws Exception {
        // J02 gravado antes de J01; J03 e J04 nunca enviaram dados e não aparecem.
        gravar("J02", 31.0, BASE.plusSeconds(10), BASE.plusSeconds(10), "ENCHENDO");
        gravar("J01", 5.0, BASE.plusSeconds(1), BASE.plusSeconds(1), "NORMAL");
        gravar("J01", 8.0, BASE.plusSeconds(2), BASE.plusSeconds(2), "NORMAL");

        mvc.perform(get("/api/jardins"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.length()").value(2))
                .andExpect(jsonPath("$[0].jardimId").value("J01"))
                .andExpect(jsonPath("$[0].nivelCm").value(8.0))
                .andExpect(jsonPath("$[0].estado").value("NORMAL"))
                .andExpect(jsonPath("$[0].ultimaLeituraEm").value("2026-11-12T12:00:02Z"))
                .andExpect(jsonPath("$[0].totalLeituras").value(2))
                .andExpect(jsonPath("$[1].jardimId").value("J02"))
                .andExpect(jsonPath("$[1].nivelCm").value(31.0))
                .andExpect(jsonPath("$[1].estado").value("ENCHENDO"))
                .andExpect(jsonPath("$[1].totalLeituras").value(1));
    }

    @Test
    void resumoUsaALeituraMaisRecenteSegundoOTimestampDoDispositivo() throws Exception {
        gravar("J01", 40.0, BASE.plusSeconds(1), BASE.plusSeconds(300), "ENCHENDO"); // evento mais novo, chegou antes
        gravar("J01", 10.0, BASE.plusSeconds(2), BASE.plusSeconds(100), "NORMAL");

        mvc.perform(get("/api/jardins"))
                .andExpect(jsonPath("$[0].nivelCm").value(40.0))
                .andExpect(jsonPath("$[0].estado").value("ENCHENDO"))
                .andExpect(jsonPath("$[0].ultimaLeituraEm").value("2026-11-12T12:05:00Z"));
    }
}

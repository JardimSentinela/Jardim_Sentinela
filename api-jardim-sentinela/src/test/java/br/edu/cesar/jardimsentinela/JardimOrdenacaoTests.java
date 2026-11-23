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

/**
 * [SCRUM-20][SW-11] Ordenação opção B (docs/decisoes-tecnicas.md §6.2):
 * COALESCE(timestampDispositivo, recebidoEm) decrescente, desempate por id decrescente.
 * As leituras são gravadas direto no repositório para controlar recebidoEm e
 * simular chegada fora de ordem (retransmissão da fila do dispositivo).
 */
@SpringBootTest
@AutoConfigureMockMvc
class JardimOrdenacaoTests {

    private static final Instant BASE = Instant.parse("2026-11-12T12:00:00Z");

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    private void gravar(double nivelCm, Instant recebidoEm, Instant timestampDispositivo) {
        repository.save(new Leitura("J01", "S01", nivelCm, 90, recebidoEm, timestampDispositivo));
    }

    @Test
    void historicoOrdenaPeloTimestampDoDispositivoMesmoComChegadaForaDeOrdem() throws Exception {
        // Chegaram na ordem 30, 10, 20 (recebidoEm crescente), mas aconteceram na ordem 10, 20, 30.
        gravar(30.0, BASE.plusSeconds(1), BASE.plusSeconds(300));
        gravar(10.0, BASE.plusSeconds(2), BASE.plusSeconds(100));
        gravar(20.0, BASE.plusSeconds(3), BASE.plusSeconds(200));

        mvc.perform(get("/api/jardins/J01/leituras"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$[0].nivelCm").value(30.0))
                .andExpect(jsonPath("$[1].nivelCm").value(20.0))
                .andExpect(jsonPath("$[2].nivelCm").value(10.0));
    }

    @Test
    void estadoAtualUsaALeituraMaisRecenteSegundoOTimestampDoDispositivo() throws Exception {
        gravar(30.0, BASE.plusSeconds(1), BASE.plusSeconds(300)); // mais recente no evento, recebida primeiro
        gravar(10.0, BASE.plusSeconds(2), BASE.plusSeconds(100));

        mvc.perform(get("/api/jardins/J01"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.nivelCm").value(30.0))
                .andExpect(jsonPath("$.ultimaLeituraEm").value("2026-11-12T12:05:00Z"))
                .andExpect(jsonPath("$.totalLeituras").value(2));
    }

    @Test
    void leituraSemTimestampUsaRecebidoEmComoChave() throws Exception {
        // Sem timestamp, a chave é recebidoEm (BASE+200). Fica entre as duas com timestamp.
        gravar(10.0, BASE.plusSeconds(1), BASE.plusSeconds(100));
        gravar(20.0, BASE.plusSeconds(200), null);
        gravar(30.0, BASE.plusSeconds(2), BASE.plusSeconds(300));

        mvc.perform(get("/api/jardins/J01/leituras"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$[0].nivelCm").value(30.0))
                .andExpect(jsonPath("$[1].nivelCm").value(20.0))
                .andExpect(jsonPath("$[2].nivelCm").value(10.0));
    }

    @Test
    void empateNaChaveDesempataPorIdDecrescente() throws Exception {
        gravar(1.0, BASE, BASE);
        gravar(2.0, BASE, BASE);
        gravar(3.0, BASE, BASE);

        mvc.perform(get("/api/jardins/J01/leituras"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$[0].nivelCm").value(3.0))
                .andExpect(jsonPath("$[2].nivelCm").value(1.0));
        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.nivelCm").value(3.0));
    }

    @Test
    void limiteAplicaDepoisDaOrdenacao() throws Exception {
        gravar(30.0, BASE.plusSeconds(1), BASE.plusSeconds(300));
        gravar(10.0, BASE.plusSeconds(2), BASE.plusSeconds(100));
        gravar(20.0, BASE.plusSeconds(3), BASE.plusSeconds(200));

        mvc.perform(get("/api/jardins/J01/leituras").param("limite", "2"))
                .andExpect(jsonPath("$.length()").value(2))
                .andExpect(jsonPath("$[0].nivelCm").value(30.0))
                .andExpect(jsonPath("$[1].nivelCm").value(20.0));
    }
}

package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static org.hamcrest.Matchers.nullValue;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;

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
 * [SCRUM-28][SW-14] O "último estado válido" usado quando o registro mais recente não traz
 * estado só vale dentro da idade máxima (a mesma da tendência, 180 s). Sem esse limite, uma
 * sonda parada deixaria um estado antigo (ex.: NORMAL) aparecendo como atual: falsa segurança.
 */
@SpringBootTest
@AutoConfigureMockMvc
@Import(RelogioDeTesteConfig.class)
class EstadoFallbackIdadeTests {

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @Autowired
    RelogioControlado relogio;

    @BeforeEach
    void preparar() {
        repository.deleteAll();
        // 5 snapshots com estado ENCHENDO (recebidos em T0+1..T0+5) e um evento T1592 sem
        // estado, o mais recente (recebido em T0+6).
        for (int i = 1; i <= 5; i++) {
            repository.save(new Leitura("J01", "jardim", 30.0, 100, T0.plusSeconds(i), T0.plusSeconds(i),
                    "ENCHENDO", null, null, "snapshot", "AAAA0001", (long) i));
        }
        repository.save(new Leitura("J01", "sensor-01", 5.0, 100, T0.plusSeconds(6), T0.plusSeconds(6),
                null, null, null, "evento", "AAAA0001", 6L));
    }

    @Test
    void dentroDaIdadeMaximaUsaOUltimoEstadoValido() throws Exception {
        relogio.definir(T0.plusSeconds(10));
        mvc.perform(get("/api/jardins/J01")).andExpect(jsonPath("$.estado").value("ENCHENDO"));
    }

    @Test
    void noLimiteExatoDaIdadeMaximaAindaValeComoNaTendencia() throws Exception {
        // Snapshot mais recente com estado: recebido em T0+5. Idade exatamente 180 s: ainda atual.
        relogio.definir(T0.plusSeconds(5).plus(Duration.ofSeconds(180)));
        mvc.perform(get("/api/jardins/J01")).andExpect(jsonPath("$.estado").value("ENCHENDO"));
    }

    @Test
    void foraDaIdadeMaximaOEstadoDeFallbackNaoEUsado() throws Exception {
        relogio.definir(T0.plusSeconds(5).plus(Duration.ofSeconds(181)));
        mvc.perform(get("/api/jardins/J01")).andExpect(jsonPath("$.estado").value(nullValue()));
    }

    @Test
    void estadoDoProprioRegistroMaisRecenteContinuaComoAntesMesmoVelho() throws Exception {
        // Contrato da SW-11 preservado: se o registro mais recente traz estado, ele vale. A
        // defasagem fica visível por ultimaLeituraEm e por tendencia = DESATUALIZADO.
        repository.save(new Leitura("J01", "jardim", 50.0, 100, T0.plusSeconds(7), T0.plusSeconds(7),
                "TRANSBORDANDO", null, null, "snapshot", "AAAA0001", 7L));
        relogio.definir(T0.plusSeconds(7).plus(Duration.ofSeconds(500)));
        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.estado").value("TRANSBORDANDO"))
                .andExpect(jsonPath("$.tendencia").value("DESATUALIZADO"));
    }
}

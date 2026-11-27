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
 * Regressões para F1 (horaSincronizada=false ordena por recebidoEm) e
 * F2 (registro tipo=boot não sobrepõe leitura real como atual).
 */
@SpringBootTest
@AutoConfigureMockMvc
class DivergenciasF1F2Tests {

    private static final Instant T0 = Instant.parse("2026-10-15T00:00:00Z");

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    @Test
    void f1ReinicioSemHoraSincronizadaOrdenaPorRecebidoEm() throws Exception {
        // 5º arg = recebidoEm, 6º arg = timestampDispositivo
        Leitura antiga1 = new Leitura("J01", "jardim", 10.0, 100,
                T0.plusSeconds(10), T0.plusSeconds(600), "NORMAL", null, null, "snapshot", "AAAA0001", 1L);
        antiga1.setHoraSincronizada(false);
        Leitura antiga2 = new Leitura("J01", "jardim", 12.0, 100,
                T0.plusSeconds(20), T0.plusSeconds(660), "NORMAL", null, null, "snapshot", "AAAA0001", 2L);
        antiga2.setHoraSincronizada(false);
        // Após reinício sem NTP, timestamp volta para 00:00:05, mas recebidoEm é mais recente (00:00:30).
        Leitura posReinicio = new Leitura("J01", "jardim", 30.0, 100,
                T0.plusSeconds(30), T0.plusSeconds(5), "ENCHENDO", null, null, "snapshot", "BBBB0002", 1L);
        posReinicio.setHoraSincronizada(false);

        repository.save(antiga1);
        repository.save(antiga2);
        repository.save(posReinicio);

        mvc.perform(get("/api/jardins/J01"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.nivelCm").value(30.0));

        mvc.perform(get("/api/jardins"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$[0].nivelCm").value(30.0));
    }

    @Test
    void f2BootNaoSobrepoeLeituraRealComoAtual() throws Exception {
        repository.save(new Leitura("J01", "jardim", 25.0, 100,
                T0.plusSeconds(10), T0.plusSeconds(10), "ENCHENDO", null, null, "snapshot", "FFFF0001", 1L));
        repository.save(new Leitura("J01", "jardim", 0.0, 100,
                T0.plusSeconds(300), T0.plusSeconds(300), "NORMAL", null, null, "boot", "FFFF0002", 0L));

        mvc.perform(get("/api/jardins/J01"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.nivelCm").value(25.0));

        mvc.perform(get("/api/jardins"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$[0].nivelCm").value(25.0));
    }
}

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
 * [SCRUM-28][SW-14] GET /api/jardins (lista) usa a mesma regra de estado de
 * GET /api/jardins/{id}: último estado válido dentro da idade máxima, sem boot.
 */
@SpringBootTest
@AutoConfigureMockMvc
@Import(RelogioDeTesteConfig.class)
class ListaEstadoCoerenteTests {

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

    private void gravar(String jardim, int seg, String tipo, String estado, long seq) {
        repository.save(new Leitura(jardim, "jardim", 30.0, 100, T0.plusSeconds(seg), T0.plusSeconds(seg),
                estado, null, null, tipo, "AAAA0001", seq));
    }

    @Test
    void ultimoRegistroSemEstadoUsaOUltimoEstadoValidoNaLista() throws Exception {
        gravar("J01", 1, "snapshot", "ENCHENDO", 1);
        gravar("J01", 2, "evento", null, 2);   // T1592 sem estado, o mais recente
        mvc.perform(get("/api/jardins")).andExpect(jsonPath("$[0].estado").value("ENCHENDO"));
        mvc.perform(get("/api/jardins/J01")).andExpect(jsonPath("$.estado").value("ENCHENDO"));
    }

    @Test
    void estadoDoBootNaoSobrepoeOSnapshotNaLista() throws Exception {
        gravar("J01", 1, "snapshot", "ENCHENDO", 1);
        gravar("J01", 2, "boot", "NORMAL", 0);
        mvc.perform(get("/api/jardins")).andExpect(jsonPath("$[0].estado").value("ENCHENDO"));
    }

    @Test
    void estadoDeReservaForaDaIdadeMaximaNaoApareceNaLista() throws Exception {
        gravar("J01", 1, "snapshot", "ENCHENDO", 1);
        gravar("J01", 2, "evento", null, 2);
        relogio.definir(T0.plusSeconds(1).plus(Duration.ofSeconds(181)));
        mvc.perform(get("/api/jardins")).andExpect(jsonPath("$[0].estado").value(nullValue()));
    }

    @Test
    void cadaJardimTemSeuProprioEstadoNaLista() throws Exception {
        gravar("J01", 1, "snapshot", "ENCHENDO", 1);
        gravar("J01", 2, "evento", null, 2);
        gravar("J02", 1, "snapshot", "ALAGADO", 1);
        gravar("J03", 1, "evento", null, 1);   // nenhum estado
        mvc.perform(get("/api/jardins"))
                .andExpect(jsonPath("$[0].estado").value("ENCHENDO"))
                .andExpect(jsonPath("$[1].estado").value("ALAGADO"))
                .andExpect(jsonPath("$[2].estado").value(nullValue()));
    }
}

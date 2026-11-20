package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
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
import org.springframework.context.annotation.Import;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.ResultActions;

import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

/**
 * [SCRUM-28][SW-14] Regressões apontadas nos reviews do PR #23:
 * (1) boot e outros registros inelegíveis não entram na tendência;
 * (2) leitura identificada por bootId+seq: dois boots com o mesmo horário não se descartam;
 * (3) registros antigos (sem tipo/bootId/seq) continuam funcionando;
 * (4) estado e tendência coerentes em GET /api/jardins/{id}.
 * Tudo pela API HTTP, como o firmware envia (tipo, bootId, seq no payload).
 */
@SpringBootTest
@AutoConfigureMockMvc
@Import(RelogioDeTesteConfig.class)
class ElegibilidadeEIdentificacaoTests {

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

    private ResultActions enviar(String json) throws Exception {
        return mvc.perform(post("/api/leituras").contentType(MediaType.APPLICATION_JSON).content(json));
    }

    private String json(String sensor, double nivel, int seg, String tipo, String bootId, Integer seq,
            String estado) {
        StringBuilder s = new StringBuilder("{\"idJardim\":\"J01\",\"idSensor\":\"" + sensor + "\"");
        s.append(",\"nivelAgua\":").append(nivel).append(",\"bateria\":100");
        s.append(",\"timestamp\":\"").append(T0.plusSeconds(seg)).append("\"");
        if (tipo != null) s.append(",\"tipo\":\"").append(tipo).append("\"");
        if (bootId != null) s.append(",\"bootId\":\"").append(bootId).append("\"");
        if (seq != null) s.append(",\"seq\":").append(seq);
        if (estado != null) s.append(",\"estado\":\"").append(estado).append("\"");
        return s.append("}").toString();
    }

    private void snapshot(double nivel, int seg, String bootId, int seq, String estado) throws Exception {
        enviar(json("jardim", nivel, seg, "snapshot", bootId, seq, estado)).andExpect(status().isCreated());
    }

    // ===== 1. boot não entra na tendência =====

    @Test
    void bootDeZeroMaisQuatroSnapshotsEstaveisNaoGeraSubindo() throws Exception {
        enviar(json("jardim", 0.0, 0, "boot", "AAAA0001", 0, null)).andExpect(status().isCreated());
        for (int i = 1; i <= 4; i++) snapshot(46.0, i, "AAAA0001", i, null);

        // 4 válidas (<5): o mesmo resultado do firmware. Com o boot contado seria SUBINDO (0 -> 46).
        mvc.perform(get("/api/jardins/J01/tendencia"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("INSUFICIENTE"))
                .andExpect(jsonPath("$.leiturasValidas").value(4));
        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.tendencia").value("INSUFICIENTE"));
    }

    @Test
    void bootNaoEntraNaJanelaDeCincoMesmoComSnapshotsSuficientes() throws Exception {
        enviar(json("jardim", 0.0, 0, "boot", "AAAA0001", 0, null)).andExpect(status().isCreated());
        for (int i = 1; i <= 5; i++) snapshot(46.0, i, "AAAA0001", i, null);

        mvc.perform(get("/api/jardins/J01/tendencia"))
                .andExpect(jsonPath("$.tendencia").value("ESTAVEL"));
    }

    @Test
    void jardimSoComBootExisteMasTemTendenciaInsuficiente() throws Exception {
        enviar(json("jardim", 0.0, 0, "boot", "AAAA0001", 0, null)).andExpect(status().isCreated());

        mvc.perform(get("/api/jardins/J01/tendencia"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("INSUFICIENTE"))
                .andExpect(jsonPath("$.leiturasValidas").value(0));
        mvc.perform(get("/api/jardins/J01"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.tendencia").value("INSUFICIENTE"));
    }

    // ===== 2. identificação da leitura por bootId + seq =====

    @Test
    void doisBootsComOMesmoHorarioNaoDescartamALeituraNova() throws Exception {
        // Relógio não sincronizado: o firmware reinicia e repete o mesmo timestamp (fallback).
        enviar(json("jardim", 30.0, 5, "snapshot", "AAAA0001", 1, null)).andExpect(status().isCreated());
        enviar(json("jardim", 31.0, 5, "snapshot", "BBBB0002", 1, null)).andExpect(status().isCreated());

        mvc.perform(get("/api/jardins/J01")).andExpect(jsonPath("$.totalLeituras").value(2));
    }

    @Test
    void retransmissaoComMesmoBootIdESeqDevolve200SemDuplicar() throws Exception {
        enviar(json("jardim", 30.0, 5, "snapshot", "AAAA0001", 7, null)).andExpect(status().isCreated());
        enviar(json("jardim", 30.0, 5, "snapshot", "AAAA0001", 7, null)).andExpect(status().isOk());

        mvc.perform(get("/api/jardins/J01")).andExpect(jsonPath("$.totalLeituras").value(1));
    }

    @Test
    void mesmoBootIdComSeqDiferenteSaoLeiturasDiferentesMesmoNoMesmoSegundo() throws Exception {
        enviar(json("sensor-01", 5.0, 5, "evento", "AAAA0001", 2, null)).andExpect(status().isCreated());
        enviar(json("sensor-01", 10.0, 5, "evento", "AAAA0001", 3, null)).andExpect(status().isCreated());

        mvc.perform(get("/api/jardins/J01")).andExpect(jsonPath("$.totalLeituras").value(2));
    }

    // ===== 3. registros antigos / sem os campos novos =====

    @Test
    void leituraSemTipoBootIdESeqContinuaAceitaComIdempotenciaPeloTimestamp() throws Exception {
        String antigo = json("S01", 10.0, 1, null, null, null, null);
        enviar(antigo).andExpect(status().isCreated());
        enviar(antigo).andExpect(status().isOk());

        mvc.perform(get("/api/jardins/J01")).andExpect(jsonPath("$.totalLeituras").value(1));
    }

    @Test
    void registrosSemTipoContinuamEntrandoNaTendencia() throws Exception {
        for (int i = 0; i < 5; i++) {
            enviar(json("S01", 10.0 + i, i, null, null, null, null)).andExpect(status().isCreated());
        }
        mvc.perform(get("/api/jardins/J01/tendencia"))
                .andExpect(jsonPath("$.tendencia").value("SUBINDO"));
    }

    @Test
    void tipoBootIdOuSeqInvalidosSaoRecusados() throws Exception {
        enviar(json("jardim", 1.0, 1, "inventado", "AAAA0001", 1, null)).andExpect(status().isBadRequest());
        enviar(json("jardim", 1.0, 1, "snapshot", "nao-e-hex", 1, null)).andExpect(status().isBadRequest());
        enviar(json("jardim", 1.0, 1, "snapshot", "AAAA0001", -1, null)).andExpect(status().isBadRequest());
    }

    // ===== 4. estado coerente em GET /api/jardins/{id} =====

    @Test
    void estadoVemDoUltimoRegistroElegivelComEstadoQuandoOMaisRecenteNaoTem() throws Exception {
        for (int i = 1; i <= 5; i++) snapshot(30.0, i, "AAAA0001", i, "ENCHENDO");
        // Último registro: evento de T1592 (sem estado).
        enviar(json("sensor-01", 5.0, 6, "evento", "AAAA0001", 6, null)).andExpect(status().isCreated());

        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.estado").value("ENCHENDO"));
    }

    @Test
    void estadoDoBootNaoSobrepoeOUltimoEstadoDeUmSnapshot() throws Exception {
        for (int i = 1; i <= 5; i++) snapshot(30.0, i, "AAAA0001", i, "ENCHENDO");
        // Reinício: o boot (nível 0 fictício) chega com estado NORMAL.
        enviar(json("jardim", 0.0, 6, "boot", "BBBB0002", 0, "NORMAL")).andExpect(status().isCreated());

        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.estado").value("ENCHENDO"));
    }

    @Test
    void semNenhumEstadoElegivelOEstadoContinuaNulo() throws Exception {
        enviar(json("S01", 10.0, 1, null, null, null, null)).andExpect(status().isCreated());
        enviar(json("jardim", 0.0, 2, "boot", "AAAA0001", 0, "NORMAL")).andExpect(status().isCreated());

        mvc.perform(get("/api/jardins/J01")).andExpect(jsonPath("$.estado").value(nullValue()));
    }

    @Test
    void estadoUsaOMaisRecenteQuandoOMaisRecenteTemEstado() throws Exception {
        snapshot(30.0, 1, "AAAA0001", 1, "ENCHENDO");
        snapshot(50.0, 2, "AAAA0001", 2, "TRANSBORDANDO");

        mvc.perform(get("/api/jardins/J01"))
                .andExpect(jsonPath("$.estado").value("TRANSBORDANDO"));
    }
}

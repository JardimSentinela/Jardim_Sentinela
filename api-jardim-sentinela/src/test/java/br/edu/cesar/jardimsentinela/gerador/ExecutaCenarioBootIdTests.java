package br.edu.cesar.jardimsentinela.gerador;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import java.time.Instant;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.webmvc.test.autoconfigure.AutoConfigureMockMvc;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;

import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

/**
 * [SCRUM-32][SW-16] Os cenários controlados precisam ser aceitos pela API de verdade.
 *
 * <p>Motivo: o {@code ExecutaCenario} gerava por padrão um {@code bootId} em formato UUID
 * (36 caracteres), mas {@code POST /api/leituras} exige 8 caracteres hexadecimais. Todo envio
 * recebia HTTP 400 e nada era gravado. Os testes de {@code SequenciaDeterministica} só olhavam
 * o JSON gerado (com {@code bootId = "boot-teste"}) e nunca enviavam a sequência ao endpoint,
 * então o erro passou pelo CI.
 */
@SpringBootTest
@AutoConfigureMockMvc
class ExecutaCenarioBootIdTests {

    private static final String OITO_HEX = "[0-9A-Fa-f]{8}";
    private static final Instant INICIO = Instant.parse("2026-11-21T10:00:00Z");

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    @Test
    void bootIdPadraoTemOitoCaracteresHexadecimais() {
        for (int i = 0; i < 200; i++) {
            String bootId = ExecutaCenario.bootIdPadrao();
            assertTrue(bootId.matches(OITO_HEX), "bootId padrão inválido: " + bootId);
        }
    }

    @Test
    void bootIdPadraoMudaACadaExecucao() {
        Set<String> vistos = new HashSet<>();
        for (int i = 0; i < 50; i++) {
            vistos.add(ExecutaCenario.bootIdPadrao());
        }
        // 50 sorteios de 32 bits: uma colisão é possível, mas um conjunto quase todo igual não é.
        assertTrue(vistos.size() >= 49, "bootId padrão não está variando: " + vistos);
    }

    @Test
    void validarBootIdRecusaUuidEFormatoLivre() {
        List<String> invalidos = List.of(
                UUID.randomUUID().toString(), // era o padrão antigo do runner
                "cenario-fixado-01",          // era o exemplo da docs/cenarios-controlados.md
                "abc",                        // curto demais
                "a1b2c3d45",                  // longo demais
                "a1b2c3dg",                   // 'g' não é hexadecimal
                "");
        for (String invalido : invalidos) {
            IllegalArgumentException erro = assertThrows(IllegalArgumentException.class,
                    () -> ExecutaCenario.validarBootId(invalido), "deveria recusar: '" + invalido + "'");
            assertTrue(erro.getMessage().contains("8"), "a mensagem deve dizer o formato: " + erro.getMessage());
        }
    }

    @Test
    void validarBootIdAceitaOitoHexadecimaisEDevolveOMesmoValor() {
        assertEquals("a1b2c3d4", assertDoesNotThrow(() -> ExecutaCenario.validarBootId("a1b2c3d4")));
        assertEquals("A1B2C3D4", assertDoesNotThrow(() -> ExecutaCenario.validarBootId("A1B2C3D4")));
        assertEquals("00000000", assertDoesNotThrow(() -> ExecutaCenario.validarBootId("00000000")));
    }

    @Test
    void apiRecusaBootIdEmFormatoUuid() throws Exception {
        // Documenta o contrato que o runner precisa respeitar (LeituraRequest: 8 hex).
        String json = SequenciaDeterministica.gerar(new SequenciaDeterministica.Configuracao(
                Cenario.CHUVA_MODERADA, "J01", "S01", 7L, INICIO, 3,
                UUID.randomUUID().toString())).get(0).json();

        mvc.perform(post("/api/leituras").contentType(MediaType.APPLICATION_JSON).content(json))
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.campos[0].campo").value("bootId"));
        assertEquals(0, repository.count());
    }

    @Test
    void todosOsCenariosComBootIdPadraoSaoAceitosENadaSeRepete() throws Exception {
        int esperado = 0;
        int indice = 1;
        for (Cenario cenario : Cenario.values()) {
            String jardim = "J0" + indice;
            String sensor = "S0" + indice;
            indice++;

            List<SequenciaDeterministica.LeituraGerada> leituras = SequenciaDeterministica.gerar(
                    new SequenciaDeterministica.Configuracao(
                            cenario, jardim, sensor, cenario.sementePadrao(), INICIO,
                            cenario.passosPadrao(), ExecutaCenario.bootIdPadrao()));

            for (SequenciaDeterministica.LeituraGerada leitura : leituras) {
                mvc.perform(post("/api/leituras")
                                .contentType(MediaType.APPLICATION_JSON)
                                .content(leitura.json()))
                        .andExpect(status().isCreated());
            }
            esperado += leituras.size();
            assertEquals(esperado, repository.count(),
                    "o cenário " + cenario + " deve gravar todas as leituras que gerou");
        }
    }
}

package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import java.time.Duration;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.webmvc.test.autoconfigure.AutoConfigureMockMvc;
import org.springframework.context.annotation.Import;
import org.springframework.test.context.TestPropertySource;
import org.springframework.test.web.servlet.MockMvc;

import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

/**
 * [SCRUM-28][SW-14] Etapa 6, Bloco D: a configuração muda o resultado sem mudar código.
 * Com tolerância 0,5 cm e idade máxima de 5 s (os padrões são 1,5 cm e 30 s), uma variação
 * de 1,0 cm vira SUBINDO e 6 s de silêncio já é DESATUALIZADO. A tolerância usada aparece
 * na resposta (transparência sobre o parâmetro experimental).
 */
@SpringBootTest
@AutoConfigureMockMvc
@Import(RelogioDeTesteConfig.class)
@TestPropertySource(properties = {
        "jardimsentinela.tendencia.tolerancia-cm=0.5",
        "jardimsentinela.tendencia.idade-maxima=5s"
})
class TendenciaConfiguracaoTests {

    @Autowired
    MockMvc mvc;

    @Autowired
    LeituraRepository repository;

    @Autowired
    RelogioControlado relogio;

    @BeforeEach
    void preparar() {
        repository.deleteAll();
        relogio.definir(LeiturasDeTeste.AGORA);
        double[] niveis = {10, 10, 10, 10, 11};                 // variação líquida de +1,0 cm
        for (int i = 0; i < niveis.length; i++) {
            repository.save(new Leitura("J01", "S01", niveis[i], 100, T0.plusSeconds(i + 1),
                    T0.plusSeconds(i)));
        }
    }

    @Test
    void toleranciaConfiguradaMudaAClassificacaoEApareceNaResposta() throws Exception {
        relogio.definir(T0.plusSeconds(5));                     // mesma hora da última recepção

        mvc.perform(get("/api/jardins/J01/tendencia"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.toleranciaCm").value(0.5))
                .andExpect(jsonPath("$.tendencia").value("SUBINDO"));   // com 1,5 seria ESTAVEL
    }

    @Test
    void idadeMaximaConfiguradaMudaOLimiteDeDesatualizado() throws Exception {
        relogio.definir(T0.plusSeconds(5).plus(Duration.ofSeconds(5)));   // 5 s exatos: atual
        mvc.perform(get("/api/jardins/J01/tendencia"))
                .andExpect(jsonPath("$.tendencia").value("SUBINDO"));

        relogio.definir(T0.plusSeconds(5).plus(Duration.ofSeconds(6)));   // 6 s: com 30 s seria atual
        mvc.perform(get("/api/jardins/J01/tendencia"))
                .andExpect(jsonPath("$.tendencia").value("DESATUALIZADO"));
    }
}

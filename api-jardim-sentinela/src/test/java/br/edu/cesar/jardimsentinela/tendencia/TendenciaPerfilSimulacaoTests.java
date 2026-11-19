package br.edu.cesar.jardimsentinela.tendencia;

import static org.junit.jupiter.api.Assertions.assertEquals;

import java.time.Duration;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.ActiveProfiles;

import br.edu.cesar.jardimsentinela.config.TendenciaProperties;

/**
 * [SCRUM-28][SW-14] Com o perfil "simulacao" a idade máxima é de 15 s (3 vezes o snapshot
 * de 5 s da simulação). A tolerância continua a do padrão. PROVISÓRIO, ver
 * docs/decisoes-tecnicas.md §5.2.
 */
@SpringBootTest
@ActiveProfiles("simulacao")
class TendenciaPerfilSimulacaoTests {

    @Autowired
    TendenciaProperties propriedades;

    @Test
    void perfilSimulacaoUsaIdadeMaximaDe15Segundos() {
        assertEquals(Duration.ofSeconds(15), propriedades.idadeMaxima());
        assertEquals(1.5, propriedades.toleranciaCm());
    }
}

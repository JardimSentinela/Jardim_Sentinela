package br.edu.cesar.jardimsentinela.tendencia;

import org.springframework.boot.test.context.TestConfiguration;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Primary;

/**
 * [SCRUM-28][SW-14] Troca o {@link java.time.Clock} da aplicação por um
 * {@link RelogioControlado} nos testes de integração que a importam
 * ({@code @Import(RelogioDeTesteConfig.class)}). O bean real (UTC) continua existindo, mas
 * este é o primário.
 */
@TestConfiguration
class RelogioDeTesteConfig {

    @Bean
    @Primary
    RelogioControlado relogioControlado() {
        return new RelogioControlado(LeiturasDeTeste.AGORA);
    }
}

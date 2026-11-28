package br.edu.cesar.jardimsentinela.config;

import java.time.Clock;

import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

/**
 * [SCRUM-28][SW-14] Configuração da tendência: registra as propriedades e o relógio.
 * O {@link Clock} é injetado no serviço para a idade dos dados poder ser testada de forma
 * determinística (um teste substitui este bean por um relógio controlado).
 */
@Configuration
@EnableConfigurationProperties(TendenciaProperties.class)
public class TendenciaConfig {

    @Bean
    public Clock relogio() {
        return Clock.systemUTC();
    }
}

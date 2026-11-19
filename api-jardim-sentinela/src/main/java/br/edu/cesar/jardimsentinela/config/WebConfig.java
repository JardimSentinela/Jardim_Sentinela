package br.edu.cesar.jardimsentinela.config;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.config.annotation.CorsRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

/**
 * [SCRUM-20][SW-11] CORS para o dashboard consumir a API a partir de outra origem.
 *
 * Lista fechada de origens (docs/decisoes-tecnicas.md §6.6): servidor de dev (5173) e
 * preview do build (4173) do Vite. Nada de "*": com ele, qualquer página aberta no
 * navegador de quem roda a API poderia enviar dados a ela, inclusive por POST.
 * A origem de produção entra pela propriedade jardimsentinela.cors.allowed-origins
 * (lista separada por vírgula), quando houver deploy.
 */
@Configuration
public class WebConfig implements WebMvcConfigurer {

    @Value("${jardimsentinela.cors.allowed-origins:http://localhost:5173,http://localhost:4173}")
    private String[] origensPermitidas;

    @Override
    public void addCorsMappings(CorsRegistry registry) {
        registry.addMapping("/api/**")
                .allowedOrigins(origensPermitidas)
                .allowedMethods("GET", "POST")
                .allowedHeaders("*");
    }
}

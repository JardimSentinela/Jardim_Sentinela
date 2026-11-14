package br.edu.cesar.jardimsentinela.config;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.config.annotation.CorsRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

/**
 * [SCRUM-20][SW-11] CORS para o dashboard gerado (Figma → código) consumir a
 * API a partir de outra origem (ex.: servidor de dev do front, Vercel/Netlify
 * de preview) sem retrabalho estrutural.
 *
 * Origem padrão "*" é deliberadamente permissiva para o dashboard de preview
 * poder bater na API sem fricção enquanto a equipe ainda está integrando.
 * Antes de qualquer deploy fora do ambiente de desenvolvimento, isso precisa
 * virar uma lista fechada de origens — não deixar "*" em produção.
 */
@Configuration
public class WebConfig implements WebMvcConfigurer {

    @Value("${jardimsentinela.cors.allowed-origins:*}")
    private String[] origensPermitidas;

    @Override
    public void addCorsMappings(CorsRegistry registry) {
        registry.addMapping("/api/**")
                .allowedOrigins(origensPermitidas)
                .allowedMethods("GET", "POST")
                .allowedHeaders("*");
    }
}

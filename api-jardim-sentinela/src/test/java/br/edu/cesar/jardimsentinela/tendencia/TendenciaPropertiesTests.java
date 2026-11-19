package br.edu.cesar.jardimsentinela.tendencia;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Map;
import java.util.Properties;

import org.junit.jupiter.api.Test;
import org.springframework.boot.context.properties.bind.BindException;
import org.springframework.boot.context.properties.bind.Binder;
import org.springframework.boot.context.properties.source.MapConfigurationPropertySource;

import br.edu.cesar.jardimsentinela.config.TendenciaProperties;

/**
 * [SCRUM-28][SW-14] Etapa 6, Bloco D: os parâmetros configuráveis da tendência. Valores
 * iniciais da decisão (D3 e D12): tolerância 1,5 cm e idade máxima de 30 s. São
 * experimentais (calibrados no simulador), não no hardware real. O tamanho da janela
 * (N = 5) é constante do código (D2) e não aparece aqui.
 */
class TendenciaPropertiesTests {

    private static TendenciaProperties ligar(Map<String, String> propriedades) {
        return new Binder(new MapConfigurationPropertySource(propriedades))
                .bindOrCreate("jardimsentinela.tendencia", TendenciaProperties.class);
    }

    @Test
    void semConfiguracaoUsaOsValoresIniciaisDaDecisao() {
        TendenciaProperties padrao = ligar(Map.of());

        assertEquals(1.5, padrao.toleranciaCm());
        assertEquals(Duration.ofSeconds(30), padrao.idadeMaxima());
        assertEquals(TendenciaProperties.TAMANHO_PAGINA_PADRAO, padrao.tamanhoPagina());
    }

    @Test
    void oApplicationPropertiesEntregueDeclaraOsValoresIniciaisDaDecisao() throws Exception {
        // Lê o arquivo de produção (o de teste o substitui no classpath dos testes). Os
        // valores precisam estar explícitos lá para quem for ajustá-los saber onde.
        // O diretório de trabalho é o módulo no Maven, mas pode ser a raiz do repositório
        // numa IDE: aceita os dois.
        Path noModulo = Path.of("src/main/resources/application.properties");
        Path naRaiz = Path.of("api-jardim-sentinela", "src/main/resources/application.properties");
        Properties arquivo = new Properties();
        try (InputStream entrada = Files.newInputStream(Files.exists(noModulo) ? noModulo : naRaiz)) {
            arquivo.load(entrada);
        }
        assertEquals("1.5", arquivo.getProperty("jardimsentinela.tendencia.tolerancia-cm"));
        assertEquals("30s", arquivo.getProperty("jardimsentinela.tendencia.idade-maxima"));
        assertEquals(String.valueOf(TendenciaProperties.TAMANHO_PAGINA_PADRAO),
                arquivo.getProperty("jardimsentinela.tendencia.tamanho-pagina"));
    }

    @Test
    void valoresPodemSerAjustadosSemMudarCodigo() {
        TendenciaProperties ajustada = ligar(Map.of(
                "jardimsentinela.tendencia.tolerancia-cm", "2.5",
                "jardimsentinela.tendencia.idade-maxima", "45s",
                "jardimsentinela.tendencia.tamanho-pagina", "10"));

        assertEquals(2.5, ajustada.toleranciaCm());
        assertEquals(Duration.ofSeconds(45), ajustada.idadeMaxima());
        assertEquals(10, ajustada.tamanhoPagina());
    }

    @Test
    void toleranciaNegativaNaConfiguracaoImpedeAInicializacao() {
        assertThrows(BindException.class,
                () -> ligar(Map.of("jardimsentinela.tendencia.tolerancia-cm", "-1")));
    }

    @Test
    void idadeMaximaNegativaNaConfiguracaoImpedeAInicializacao() {
        assertThrows(BindException.class,
                () -> ligar(Map.of("jardimsentinela.tendencia.idade-maxima", "-5s")));
    }

    @Test
    void tamanhoDePaginaMenorQueUmImpedeAInicializacao() {
        assertThrows(BindException.class,
                () -> ligar(Map.of("jardimsentinela.tendencia.tamanho-pagina", "0")));
    }

    @Test
    void construtorRejeitaToleranciaNaNEInfinita() {
        Duration idade = Duration.ofSeconds(30);
        assertThrows(IllegalArgumentException.class,
                () -> new TendenciaProperties(Double.NaN, idade, 10));
        assertThrows(IllegalArgumentException.class,
                () -> new TendenciaProperties(Double.POSITIVE_INFINITY, idade, 10));
    }
}

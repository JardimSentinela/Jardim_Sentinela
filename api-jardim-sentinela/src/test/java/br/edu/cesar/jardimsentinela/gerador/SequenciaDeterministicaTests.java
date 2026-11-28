package br.edu.cesar.jardimsentinela.gerador;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.time.Duration;
import java.time.Instant;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.junit.jupiter.api.Test;

class SequenciaDeterministicaTests {

    private static final Instant INICIO = Instant.parse("2026-11-21T10:00:00Z");

    private SequenciaDeterministica.Configuracao configuracao(Cenario cenario, long semente, int passos) {
        return new SequenciaDeterministica.Configuracao(
                cenario, "J01", "S01", semente, INICIO, passos, "boot-teste");
    }

    private List<SequenciaDeterministica.LeituraGerada> gerar(Cenario cenario, long semente, int passos) {
        return SequenciaDeterministica.gerar(configuracao(cenario, semente, passos));
    }

    private String texto(String json, String campo) {
        Matcher m = Pattern.compile("\"" + campo + "\":\"([^\"]*)\"").matcher(json);
        assertTrue(m.find(), "campo " + campo + " ausente em " + json);
        return m.group(1);
    }

    private double numero(String json, String campo) {
        Matcher m = Pattern.compile("\"" + campo + "\":(-?[0-9]+(?:\\.[0-9]+)?)").matcher(json);
        assertTrue(m.find(), "campo " + campo + " ausente em " + json);
        return Double.parseDouble(m.group(1));
    }

    @Test
    void mesmaEntradaGeraMesmaSequencia() {
        List<String> primeira = gerar(Cenario.CHUVA_MODERADA, 7, 20).stream()
                .map(SequenciaDeterministica.LeituraGerada::json).toList();
        List<String> segunda = gerar(Cenario.CHUVA_MODERADA, 7, 20).stream()
                .map(SequenciaDeterministica.LeituraGerada::json).toList();

        assertEquals(primeira, segunda);
    }

    @Test
    void sementeDiferenteMudaORuido() {
        List<String> primeira = gerar(Cenario.CHUVA_MODERADA, 7, 20).stream()
                .map(SequenciaDeterministica.LeituraGerada::json).toList();
        List<String> segunda = gerar(Cenario.CHUVA_MODERADA, 8, 20).stream()
                .map(SequenciaDeterministica.LeituraGerada::json).toList();

        assertFalse(primeira.equals(segunda));
    }

    @Test
    void primeiroRegistroEhBootComSeqZero() {
        String json = gerar(Cenario.CHUVA_MODERADA, 1, 5).get(0).json();

        assertEquals("boot", texto(json, "tipo"));
        assertEquals("boot-teste", texto(json, "bootId"));
        assertEquals(0.0, numero(json, "seq"));
        assertEquals(0.0, numero(json, "nivelAgua"));
        assertEquals("J01", texto(json, "idJardim"));
        assertEquals("S01", texto(json, "idSensor"));
        assertEquals(INICIO.minusSeconds(Cenario.CHUVA_MODERADA.intervaloSegundos()),
                Instant.parse(texto(json, "timestamp")));
    }

    @Test
    void snapshotsTemSeqEIncrementalETimestampsCrescentes() {
        List<SequenciaDeterministica.LeituraGerada> leituras = gerar(Cenario.CHUVA_MODERADA, 1, 12);

        Instant anterior = null;
        for (int i = 1; i < leituras.size(); i++) {
            String json = leituras.get(i).json();
            assertEquals(i, (int) numero(json, "seq"));
            assertEquals("snapshot", texto(json, "tipo"));
            assertEquals("boot-teste", texto(json, "bootId"));

            Instant atual = Instant.parse(texto(json, "timestamp"));
            if (anterior != null) {
                assertTrue(atual.isAfter(anterior));
            }
            anterior = atual;
        }
    }

    @Test
    void chuvaModeradaSobeCercaDeUmCentimetroPorLeitura() {
        List<SequenciaDeterministica.LeituraGerada> leituras = gerar(Cenario.CHUVA_MODERADA, 7, 10);

        double primeira = leituras.get(1).nivelCm();
        double ultima = leituras.get(10).nivelCm();
        double porLeitura = (ultima - primeira) / 9;

        assertTrue(porLeitura > 0.8 && porLeitura < 1.2, "taxa por leitura: " + porLeitura);
    }

    @Test
    void chuvaIntensaSobeMaisQueModerada() {
        double moderada = variacaoTotal(Cenario.CHUVA_MODERADA, 10);
        double intensa = variacaoTotal(Cenario.CHUVA_INTENSA, 10);

        assertTrue(intensa > moderada * 1.5, "moderada=" + moderada + " intensa=" + intensa);
    }

    @Test
    void saturacaoParaNoLimiteDoGerador() {
        List<SequenciaDeterministica.LeituraGerada> leituras = gerar(Cenario.SATURACAO, 1, 30);

        assertEquals(50.0, leituras.get(20).nivelCm());
        assertEquals(50.0, leituras.get(30).nivelCm());
    }

    @Test
    void drenagemLentaFicaAbaixoDeMeioCentimetroPorMinuto() {
        List<SequenciaDeterministica.LeituraGerada> leituras = gerar(Cenario.DRENAGEM_LENTA, 1, 180);

        SequenciaDeterministica.LeituraGerada primeira = leituras.get(1);
        SequenciaDeterministica.LeituraGerada ultima = leituras.get(180);
        double minutos = Duration.between(primeira.timestamp(), ultima.timestamp()).toSeconds() / 60.0;
        double taxa = (primeira.nivelCm() - ultima.nivelCm()) / minutos;

        assertTrue(taxa > 0, "esperava queda: " + taxa);
        assertTrue(taxa < 0.5, "taxa deve ser lenta: " + taxa);
    }

    @Test
    void nivelNuncaSaiDosLimitesDoGerador() {
        for (Cenario cenario : Cenario.values()) {
            for (SequenciaDeterministica.LeituraGerada leitura : gerar(cenario, 99, 200)) {
                assertTrue(leitura.nivelCm() >= 0 && leitura.nivelCm() <= 50,
                        cenario + " gerou " + leitura.nivelCm());
            }
        }
    }

    private double variacaoTotal(Cenario cenario, int passos) {
        List<SequenciaDeterministica.LeituraGerada> leituras = gerar(cenario, 7, passos);
        return leituras.get(passos).nivelCm() - leituras.get(1).nivelCm();
    }
}

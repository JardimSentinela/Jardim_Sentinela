package br.edu.cesar.jardimsentinela.gerador;

import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;

/**
 * CLI dos cenários controlados do [SCRUM-32][SW-16].
 *
 * Uso:
 * {@code ExecutaCenario --cenario=CHUVA_MODERADA --jardim=J01 --passos=60}
 *
 * Por padrão publica em {@code http://localhost:8080/api/leituras} em sequência
 * rápida, com timestamps sintéticos espaçados pelo intervalo do cenário. Isso
 * permite apresentar os 60 min do cenário de drenagem em segundos. Use
 * {@code --tempo-real} para esperar o intervalo entre envios e {@code --dry-run}
 * para apenas imprimir o JSON.
 *
 * Cada execução gera um {@code bootId} novo e reinicia o {@code seq} em zero:
 * esse é o reset do cenário. Para reproduzir exatamente a mesma sequência,
 * informe {@code --boot-id}, {@code --seed} e {@code --inicio}.
 */
public final class ExecutaCenario {

    private static final String URL_PADRAO = "http://localhost:8080/api/leituras";

    private ExecutaCenario() {
    }

    public static void main(String[] args) throws Exception {
        Map<String, String> opcoes = interpretar(args);
        if (!opcoes.containsKey("cenario")) {
            System.err.println("obrigatorio: --cenario=" + java.util.Arrays.toString(Cenario.values()));
            imprimirUso();
            System.exit(1);
        }

        Cenario cenario = Cenario.valueOf(opcoes.get("cenario").toUpperCase(Locale.ROOT));
        String jardimId = opcoes.getOrDefault("jardim", "J01");
        String sensorId = opcoes.getOrDefault("sensor", sensorPadrao(jardimId));
        long semente = Long.parseLong(opcoes.getOrDefault("seed", Long.toString(cenario.sementePadrao())));
        int passos = Integer.parseInt(opcoes.getOrDefault("passos", Integer.toString(cenario.passosPadrao())));
        Instant inicio = opcoes.containsKey("inicio")
                ? Instant.parse(opcoes.get("inicio"))
                : Instant.now().truncatedTo(ChronoUnit.SECONDS);
        String bootId = opcoes.getOrDefault("boot-id", UUID.randomUUID().toString());

        SequenciaDeterministica.Configuracao config = new SequenciaDeterministica.Configuracao(
                cenario, jardimId, sensorId, semente, inicio, passos, bootId);
        List<SequenciaDeterministica.LeituraGerada> leituras = SequenciaDeterministica.gerar(config);

        boolean seco = opcoes.containsKey("dry-run");
        boolean tempoReal = opcoes.containsKey("tempo-real");
        String url = opcoes.getOrDefault("url", URL_PADRAO);

        System.out.printf("Cenario=%s jardim=%s sensor=%s seed=%d passos=%d "
                        + "intervalo=%ds inicio=%s bootId=%s%n",
                cenario, jardimId, sensorId, semente, passos,
                cenario.intervaloSegundos(), inicio, bootId);
        if (opcoes.containsKey("reset")) {
            System.out.println("Reset: bootId novo, seq reiniciado em 0 (histórico mantido).");
        }

        HttpClient client = HttpClient.newHttpClient();
        for (int i = 0; i < leituras.size(); i++) {
            SequenciaDeterministica.LeituraGerada leitura = leituras.get(i);
            System.out.println(leitura.json());

            if (!seco) {
                publicar(client, url, leitura);
            }
            if (tempoReal && i < leituras.size() - 1) {
                Thread.sleep(cenario.intervaloSegundos() * 1000);
            }
        }
        System.out.println("Cenario encerrado.");
    }

    private static void publicar(HttpClient client, String url,
            SequenciaDeterministica.LeituraGerada leitura) {
        HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(url))
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(leitura.json()))
                .build();
        try {
            HttpResponse<String> resposta = client.send(request, HttpResponse.BodyHandlers.ofString());
            System.out.printf("  -> seq=%s HTTP %d%n", leitura.seq(), resposta.statusCode());
        } catch (Exception e) {
            System.err.printf("  -> seq=%s falha: %s%n", leitura.seq(), e.getMessage());
        }
    }

    private static String sensorPadrao(String jardimId) {
        return jardimId.matches("J\\d{2}") ? "S" + jardimId.substring(1) : "S01";
    }

    private static Map<String, String> interpretar(String[] args) {
        Map<String, String> opcoes = new HashMap<>();
        for (String arg : args) {
            if (!arg.startsWith("--")) {
                throw new IllegalArgumentException("argumento invalido: " + arg);
            }
            String corpo = arg.substring(2);
            int igual = corpo.indexOf('=');
            if (igual < 0) {
                opcoes.put(corpo, "");
            } else {
                opcoes.put(corpo.substring(0, igual), corpo.substring(igual + 1));
            }
        }
        return opcoes;
    }

    private static void imprimirUso() {
        System.err.println("""
                Opcoes:
                  --cenario=CHUVA_MODERADA|CHUVA_INTENSA|SATURACAO|DRENAGEM_LENTA
                  --jardim=J01 (padrao J01)   --sensor=S01 (padrao derivado do jardim)
                  --seed=7                    --inicio=2026-11-21T10:00:00Z
                  --passos=60                 --boot-id=<uuid>
                  --url=http://localhost:8080/api/leituras
                  --dry-run                   --tempo-real                  --reset
                Exemplo:
                  ExecutaCenario --cenario=DRENAGEM_LENTA --jardim=J03 --reset
                """);
    }
}

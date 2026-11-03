package br.edu.cesar.jardimsentinela.gerador;

import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.util.List;
import java.util.Locale;
import java.util.Random;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

/**
 * Simula a rede Sentinela: cada jardim é um dispositivo independente, com seu próprio
 * sensor, frequência de envio e comportamento do nível de água.
 *
 * Uso: GeradorDadosJardim [urlApi] [duracaoSegundos]
 * Padrão: http://localhost:8080/api/leituras por 60 segundos.
 */
public class GeradorDadosJardim {

    private static final double NIVEL_MAXIMO_CM = 50.0;

    /** Um jardim simulado. O nível faz um passeio aleatório limitado entre 0 e NIVEL_MAXIMO_CM. */
    static class Dispositivo {
        final String jardimId;
        final String sensorId;
        final long intervaloSegundos;
        final double passoMaximoCm;
        final double tendenciaCm; // deslocamento médio por leitura (positivo = enchendo)
        final Random random;

        double nivelCm;
        double bateria;

        Dispositivo(String jardimId, String sensorId, long intervaloSegundos,
                    double nivelInicialCm, double passoMaximoCm, double tendenciaCm, long semente) {
            this.jardimId = jardimId;
            this.sensorId = sensorId;
            this.intervaloSegundos = intervaloSegundos;
            this.nivelCm = nivelInicialCm;
            this.passoMaximoCm = passoMaximoCm;
            this.tendenciaCm = tendenciaCm;
            this.bateria = 100;
            this.random = new Random(semente);
        }

        String proximaLeituraJson() {
            double variacao = tendenciaCm + (random.nextDouble() * 2 - 1) * passoMaximoCm;
            nivelCm = Math.max(0, Math.min(NIVEL_MAXIMO_CM, nivelCm + variacao));
            bateria = Math.max(0, bateria - 0.2);

            return String.format(Locale.ROOT,
                    "{\"jardimId\":\"%s\",\"sensorId\":\"%s\",\"nivelCm\":%.1f,\"bateria\":%d}",
                    jardimId, sensorId, nivelCm, (int) Math.round(bateria));
        }
    }

    public static void main(String[] args) throws Exception {
        String url = args.length > 0 ? args[0] : "http://localhost:8080/api/leituras";
        long duracaoSegundos = args.length > 1 ? Long.parseLong(args[1]) : 60;

        // Parâmetros diferentes por jardim: frequência, ponto de partida, ruído e tendência.
        List<Dispositivo> dispositivos = List.of(
                new Dispositivo("J01", "S01", 2, 18.0, 1.5, 0.0, 1),   // estável
                new Dispositivo("J02", "S02", 3, 10.0, 1.0, 1.2, 2),   // enchendo
                new Dispositivo("J03", "S03", 4, 40.0, 1.0, -1.0, 3),  // drenando
                new Dispositivo("J04", "S04", 5, 25.0, 3.0, 0.0, 4)    // oscilando
        );

        HttpClient client = HttpClient.newHttpClient();
        ScheduledExecutorService executor = Executors.newScheduledThreadPool(dispositivos.size());

        for (Dispositivo d : dispositivos) {
            executor.scheduleAtFixedRate(() -> enviar(client, url, d),
                    0, d.intervaloSegundos, TimeUnit.SECONDS);
        }

        System.out.printf("Simulando %d jardins por %ds em %s%n", dispositivos.size(), duracaoSegundos, url);
        Thread.sleep(duracaoSegundos * 1000);
        executor.shutdownNow();
        System.out.println("Simulação encerrada.");
    }

    private static void enviar(HttpClient client, String url, Dispositivo d) {
        try {
            String json = d.proximaLeituraJson();
            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create(url))
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(json))
                    .build();
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            System.out.printf("[%s] %s -> HTTP %d%n", d.jardimId, json, response.statusCode());
        } catch (Exception e) {
            // uma falha de um jardim (ex.: API fora do ar) não pode derrubar os outros
            System.err.printf("[%s] falha ao enviar: %s%n", d.jardimId, e.getMessage());
        }
    }
}

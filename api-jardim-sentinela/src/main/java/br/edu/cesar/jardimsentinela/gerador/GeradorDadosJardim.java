package br.edu.cesar.jardimsentinela.gerador;

import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.util.List;

public class GeradorDadosJardim {

    public static void main(String[] args) throws Exception {

        // Lista de leituras simuladas dos jardins
        List<String> leituras = List.of(
                """
                {
                    "jardimId": "J01",
                    "sensorId": "S01",
                    "nivelCm": 18.7,
                    "bateria": 87
                }
                """,

                """
                {
                    "jardimId": "J02",
                    "sensorId": "S02",
                    "nivelCm": 25.3,
                    "bateria": 92
                }
                """,

                """
                {
                    "jardimId": "J03",
                    "sensorId": "S03",
                    "nivelCm": 12.5,
                    "bateria": 78
                }
                """,

                """
                {
                    "jardimId": "J04",
                    "sensorId": "S04",
                    "nivelCm": 31.2,
                    "bateria": 85
                }
                """
        );

        HttpClient client = HttpClient.newHttpClient();

        // Envia cada leitura para a API
        for (String jsonBody : leituras) {

            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create("http://localhost:8080/api/leituras"))
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(jsonBody))
                    .build();

            HttpResponse<String> response = client.send(
                    request,
                    HttpResponse.BodyHandlers.ofString()
            );

            System.out.println("Status da API: " + response.statusCode());
        }
    }
}
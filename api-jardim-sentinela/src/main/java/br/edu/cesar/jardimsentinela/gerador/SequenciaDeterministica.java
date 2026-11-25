package br.edu.cesar.jardimsentinela.gerador;

import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Random;

/**
 * Gera a sequência de leituras de um cenário controlado sem relógio global nem
 * efeito de rede. Mesma configuração produz sempre o mesmo JSON, o que permite
 * repetir um teste ou uma apresentação.
 *
 * O primeiro registro é um {@code boot} com nível 0 e {@code seq = 0}. Os
 * demais são {@code snapshot} com {@code seq} crescente. O {@code bootId}
 * identifica a execução: trocá-lo é o reset do cenário, sem apagar o histórico
 * já recebido pela API.
 */
public final class SequenciaDeterministica {

    public static final double NIVEL_MAXIMO_CM = 50.0;

    private SequenciaDeterministica() {
    }

    public record Configuracao(
            Cenario cenario,
            String jardimId,
            String sensorId,
            long semente,
            Instant inicio,
            int passos,
            String bootId) {
    }

    public record LeituraGerada(
            String json,
            Instant timestamp,
            double nivelCm,
            int seq,
            String tipo) {
    }

    public static List<LeituraGerada> gerar(Configuracao config) {
        if (config.passos() < 2) {
            throw new IllegalArgumentException("um cenário precisa de pelo menos 2 snapshots");
        }

        List<LeituraGerada> leituras = new ArrayList<>(config.passos() + 1);
        Instant instanteBoot = config.inicio().minusSeconds(config.cenario().intervaloSegundos());
        leituras.add(new LeituraGerada(
                paraJson(config, "boot", 0, 0.0, 100, instanteBoot),
                instanteBoot, 0.0, 0, "boot"));

        Random random = new Random(config.semente());
        for (int i = 1; i <= config.passos(); i++) {
            Instant timestamp = config.inicio().plusSeconds((long) (i - 1) * config.cenario().intervaloSegundos());
            double nivelCm = nivelDaLeitura(config.cenario(), random, i);
            int bateria = (int) Math.round(100 - 0.2 * i);
            leituras.add(new LeituraGerada(
                    paraJson(config, "snapshot", i, nivelCm, bateria, timestamp),
                    timestamp, nivelCm, i, "snapshot"));
        }
        return leituras;
    }

    private static double nivelDaLeitura(Cenario cenario, Random random, int indice) {
        double nivel;
        if (cenario.leiturasPorDecrescimo() > 0) {
            nivel = cenario.nivelInicialCm()
                    - (indice / cenario.leiturasPorDecrescimo()) * 0.1;
        } else {
            double ruido = cenario.ruidoCm() == 0
                    ? 0
                    : (random.nextDouble() * 2 - 1) * cenario.ruidoCm();
            nivel = cenario.nivelInicialCm()
                    + indice * cenario.variacaoCmPorLeitura()
                    + ruido;
        }
        double arredondado = Math.round(nivel * 10.0) / 10.0;
        return Math.max(0, Math.min(NIVEL_MAXIMO_CM, arredondado));
    }

    private static String paraJson(Configuracao config, String tipo, int seq,
            double nivelCm, int bateria, Instant timestamp) {
        return String.format(Locale.ROOT,
                "{\"idJardim\":\"%s\",\"idSensor\":\"%s\",\"nivelAgua\":%.1f,"
                        + "\"bateria\":%d,\"timestamp\":\"%s\",\"tipo\":\"%s\","
                        + "\"bootId\":\"%s\",\"seq\":%d}",
                config.jardimId(), config.sensorId(), nivelCm, bateria,
                timestamp, tipo, config.bootId(), seq);
    }
}

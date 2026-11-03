package br.edu.cesar.jardimsentinela.dto;

import java.time.Instant;

import br.edu.cesar.jardimsentinela.entity.Leitura;

public record JardimResponse(
        String jardimId,
        String sensorId,
        Double nivelCm,
        Integer bateria,
        Instant ultimaLeituraEm,
        long totalLeituras
) {

    public static JardimResponse de(Leitura ultima, long totalLeituras) {
        return new JardimResponse(
                ultima.getJardimId(),
                ultima.getSensorId(),
                ultima.getNivelCm(),
                ultima.getBateria(),
                ultima.getRecebidoEm(),
                totalLeituras);
    }
}

package br.edu.cesar.jardimsentinela.dto;

public record LeituraRequest(
        String jardimId,
        String sensorId,
        Double nivelCm,
        Integer bateria
) {
}
package br.edu.cesar.jardimsentinela.dto;

import java.time.Instant;

import br.edu.cesar.jardimsentinela.entity.Leitura;

public record JardimResponse(
        String jardimId,
        String sensorId,
        Double nivelCm,
        Integer bateria,
        Instant ultimaLeituraEm,
        long totalLeituras,
        /** Estado do registro mais recente, como o dispositivo enviou. Nulo = sem classificação. */
        String estado,
        /** Tendência calculada pela API (mesma regra de GET /api/jardins/{id}/tendencia). */
        String tendencia
) {

    /** {@code estado}: último estado válido do jardim (ver LeituraRepository#findComEstadoElegivel). */
    public static JardimResponse de(Leitura ultima, long totalLeituras, String estado, String tendencia) {
        return new JardimResponse(
                ultima.getJardimId(),
                ultima.getSensorId(),
                ultima.getNivelCm(),
                ultima.getBateria(),
                ultima.getTimestampDispositivo() != null ? ultima.getTimestampDispositivo() : ultima.getRecebidoEm(),
                totalLeituras,
                estado,
                tendencia);
    }
}

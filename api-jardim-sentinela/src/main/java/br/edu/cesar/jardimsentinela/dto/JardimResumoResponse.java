package br.edu.cesar.jardimsentinela.dto;

import java.time.Instant;

import br.edu.cesar.jardimsentinela.entity.Leitura;

/** [SCRUM-20][SW-11] Item de {@code GET /api/jardins}: resumo do jardim pela leitura mais recente. */
public record JardimResumoResponse(
        String jardimId,
        Double nivelCm,
        /** Estado do registro mais recente, como o dispositivo enviou. Nulo = sem classificação. */
        String estado,
        Instant ultimaLeituraEm,
        long totalLeituras
) {

    public static JardimResumoResponse de(Leitura ultima, long totalLeituras) {
        Instant chave = ultima.getTimestampDispositivo() != null
                ? ultima.getTimestampDispositivo()
                : ultima.getRecebidoEm();
        return new JardimResumoResponse(
                ultima.getJardimId(),
                ultima.getNivelCm(),
                ultima.getEstado(),
                chave,
                totalLeituras);
    }
}

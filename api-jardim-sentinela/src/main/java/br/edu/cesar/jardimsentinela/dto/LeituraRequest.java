package br.edu.cesar.jardimsentinela.dto;

import java.time.Instant;

import com.fasterxml.jackson.annotation.JsonAlias;

import jakarta.validation.constraints.Max;
import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.PositiveOrZero;

/**
 * Payload de {@code POST /api/leituras}.
 *
 * [SCRUM-43][SW-09] Aceita os dois vocabulários em uso hoje no projeto, sem
 * branch por origem no controller: o do simulador Java ({@code jardimId},
 * {@code sensorId}, {@code nivelCm}) e o do firmware ESP32 atual em
 * {@code esp32/src/main_alt.cpp} ({@code idJardim}, {@code idSensor},
 * {@code nivelAgua}, {@code timestamp}), documentado em
 * {@code docs/contrato-telemetria.md}. As duas fontes caem no mesmo campo Java
 * via {@link JsonAlias}. Isto é uma ponte pragmática para fechar o critério de
 * aceite do SW-09 agora ("mesmo endpoint, sem código especial por origem") —
 * o nome oficial do contrato continua em aberto, ver "Decisões em aberto" no
 * mesmo doc.
 *
 * {@code timestamp} é opcional: o simulador não o envia hoje e seu
 * comportamento não muda. Quando o firmware o envia, habilita a idempotência
 * básica do SCRUM-44 [SW-10] (ver LeituraController). Não usei
 * {@code bootId}/{@code seq} como chave porque esses campos só existem no
 * formato NDJSON mais rico da branch feat/SCRUM-34 (ainda não mergeada em
 * develop) — ver "Dado a verificar" na entrega.
 */
public record LeituraRequest(
        @NotBlank
        @JsonAlias("idJardim")
        String jardimId,

        @NotBlank
        @JsonAlias("idSensor")
        String sensorId,

        @NotNull @PositiveOrZero
        @JsonAlias("nivelAgua")
        Double nivelCm,

        @NotNull @Min(0) @Max(100)
        Integer bateria,

        /** Instante do evento no dispositivo (ISO-8601 UTC). Ausente no simulador hoje. */
        Instant timestamp
) {
}

package br.edu.cesar.jardimsentinela.dto;

import java.time.Instant;

import com.fasterxml.jackson.annotation.JsonAlias;

import jakarta.validation.constraints.Max;
import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Pattern;
import jakarta.validation.constraints.PositiveOrZero;
import jakarta.validation.constraints.Size;

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
        Instant timestamp,

        /**
         * [SCRUM-20][SW-11] Estado físico classificado pelo dispositivo (decisões §6.3).
         * Opcional; guardado como veio, a API não reclassifica.
         */
        @Pattern(regexp = "NORMAL|ENCHENDO|TRANSBORDANDO|ALAGADO",
                message = "deve ser um de NORMAL, ENCHENDO, TRANSBORDANDO, ALAGADO")
        String estado,

        /** Tendência calculada pelo dispositivo. Opcional; guardada para auditoria, nunca recalculada. */
        @Pattern(regexp = "SUBINDO|ESTAVEL|DESCENDO|INSUFICIENTE|INDETERMINADO|DESATUALIZADO",
                message = "deve ser um de SUBINDO, ESTAVEL, DESCENDO, INSUFICIENTE, INDETERMINADO, DESATUALIZADO")
        String tendenciaDispositivo,

        /** Versão da regra que classificou o registro (ex.: "v1-provisoria-2026-11"). Opcional. */
        @Size(max = 64)
        String versaoRegra,

        /**
         * [SCRUM-28][SW-14] Tipo do registro enviado pelo firmware. Um "boot" não é medição
         * (nível fictício) e fica fora da tendência. Opcional: ausente no simulador e em
         * registros antigos, que continuam elegíveis.
         */
        @Pattern(regexp = "evento|snapshot|boot", message = "deve ser um de evento, snapshot, boot")
        String tipo,

        /** [SCRUM-28][SW-14] Identificador do boot (8 hex). Com seq, identifica a leitura. Opcional. */
        @Pattern(regexp = "[0-9A-Fa-f]{8}", message = "deve ter 8 caracteres hexadecimais")
        String bootId,

        /** [SCRUM-28][SW-14] Contador do registro no boot (32 bits sem sinal no firmware). Opcional. */
        @PositiveOrZero @Max(4294967295L)
        Long seq
) {
}

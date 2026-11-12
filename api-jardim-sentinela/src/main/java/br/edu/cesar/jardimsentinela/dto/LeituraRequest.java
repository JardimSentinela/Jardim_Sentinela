package br.edu.cesar.jardimsentinela.dto;

import java.time.Instant;

import jakarta.validation.constraints.Max;
import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.PositiveOrZero;

public record LeituraRequest(
        @NotBlank String idJardim,
        @NotBlank String idSensor,
        @NotNull @PositiveOrZero Double nivelAgua,
        @NotNull @Min(0) @Max(100) Integer bateria,
        @NotNull Instant timestamp
) {
}

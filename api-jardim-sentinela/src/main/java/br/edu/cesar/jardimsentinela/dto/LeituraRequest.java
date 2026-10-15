package br.edu.cesar.jardimsentinela.dto;

import jakarta.validation.constraints.Max;
import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.PositiveOrZero;

public record LeituraRequest(
        @NotBlank String jardimId,
        @NotBlank String sensorId,
        @NotNull @PositiveOrZero Double nivelCm,
        @NotNull @Min(0) @Max(100) Integer bateria
) {
}

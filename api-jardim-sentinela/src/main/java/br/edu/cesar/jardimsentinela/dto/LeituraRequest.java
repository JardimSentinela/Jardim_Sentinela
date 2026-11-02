package br.edu.cesar.jardimsentinela.dto;

import jakarta.validation.constraints.Max;
import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.PositiveOrZero;

public record LeituraRequest(
        @NotBlank(message = "não pode ser vazio")
        String jardimId,

        @NotBlank(message = "não pode ser vazio")
        String sensorId,

        @NotNull(message = "é obrigatório")
        @PositiveOrZero(message = "não pode ser negativo")
        Double nivelCm,

        @NotNull(message = "é obrigatório")
        @Min(value = 0, message = "deve estar entre 0 e 100")
        @Max(value = 100, message = "deve estar entre 0 e 100")
        Integer bateria
) {
}

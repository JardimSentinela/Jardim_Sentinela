package br.edu.cesar.jardimsentinela.gerador;

import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.time.Instant;

import org.junit.jupiter.api.Test;

class GeradorDadosJardimTests {

    @Test
    void simuladorEmiteOMesmoContratoDoFirmware() {
        GeradorDadosJardim.Dispositivo dispositivo = new GeradorDadosJardim.Dispositivo(
                "jardim-centro-01", "sensor-01", 2, 18.0, 1.5, 0.0, 1);

        String json = dispositivo.proximaLeituraJson(
                Instant.parse("2026-11-12T12:00:00Z"));

        assertTrue(json.contains("\"idJardim\":\"jardim-centro-01\""));
        assertTrue(json.contains("\"idSensor\":\"sensor-01\""));
        assertTrue(json.contains("\"nivelAgua\":"));
        assertTrue(json.contains("\"bateria\":"));
        assertTrue(json.contains("\"timestamp\":\"2026-11-12T12:00:00Z\""));

        // Nenhum nome do contrato antigo deve sobreviver no payload.
        assertFalse(json.contains("\"jardimId\""));
        assertFalse(json.contains("\"sensorId\""));
        assertFalse(json.contains("\"nivelCm\""));
    }
}

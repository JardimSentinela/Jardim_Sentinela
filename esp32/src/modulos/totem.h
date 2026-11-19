#ifndef TOTEM_H
#define TOTEM_H

// Totem: liga LEDs e buzzer conforme calcularSinalizacaoDeBytes() (sinalizacao.h).
// So fala com os pinos; a decisao fica no modulo puro e testado no host.
// Funciona sem internet. O buzzer nao tem botao de silenciar (decisoes §4.2).

#include <Arduino.h>

// Pinos (Wokwi/ESP32 DevKit V1). D12 e pino de strapping: o amarelo usa o
// GPIO16, rotulado RX2 na placa wokwi-esp32-devkit-v1 (diagram.json: "esp:RX2").
constexpr uint8_t PINO_LED_VERDE    = 13;
constexpr uint8_t PINO_LED_AMARELO  = 16;
constexpr uint8_t PINO_LED_VERMELHO = 14;
constexpr uint8_t PINO_BUZZER       = 23;

// PROVISORIO: frequencia do tom do buzzer. O padrao (pulsos) esta em sinalizacao.h.
constexpr uint32_t FREQUENCIA_BUZZER_HZ = 2000;

void iniciarTotem();
void atualizarTotem();   // chamar a cada volta do loop(), sem bloquear

#endif

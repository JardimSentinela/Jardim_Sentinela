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

// Periodo da tarefa do totem. Bem menor que o pulso do buzzer (150 ms) para o
// padrao sonoro e o pisca do LED nao distorcerem.
constexpr uint32_t PERIODO_TOTEM_MS = 20;

void iniciarTotem();
// Cria a tarefa FreeRTOS que chama atualizarTotem() a cada PERIODO_TOTEM_MS. A
// sinalizacao NAO pode depender do loop(): ele bloqueia em http.POST() (ate
// HTTP_TIMEOUT_MS) e, com o buzzer dentro de um pulso, o som ficaria continuo.
void iniciarTarefaTotem();
// Uso interno da tarefa (e dos testes de bancada). Nao chamar do loop().
void atualizarTotem();

#endif

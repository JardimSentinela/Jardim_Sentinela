// ===== TOTEM (LEDs E BUZZER) ================================================

#include "totem.h"

#include "estado.h"
#include "estado_fisico.h"
#include "sinalizacao.h"
#include "tendencia_local.h"

// A API de tom do buzzer (LEDC) mudou no core Arduino-ESP32 3.x.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static void iniciarBuzzer() { ledcAttach(PINO_BUZZER, FREQUENCIA_BUZZER_HZ, 8); }
static void tocarBuzzer(bool ligado) { ledcWriteTone(PINO_BUZZER, ligado ? FREQUENCIA_BUZZER_HZ : 0); }
#else
constexpr uint8_t CANAL_BUZZER = 0;
static void iniciarBuzzer() {
  ledcSetup(CANAL_BUZZER, FREQUENCIA_BUZZER_HZ, 8);
  ledcAttachPin(PINO_BUZZER, CANAL_BUZZER);
}
static void tocarBuzzer(bool ligado) { ledcWriteTone(CANAL_BUZZER, ligado ? FREQUENCIA_BUZZER_HZ : 0); }
#endif

static bool buzzerLigado = false;
static uint8_t ultimoEstado = 0xFE;      // valor impossivel: forca o primeiro log
static uint8_t ultimaTendencia = 0xFE;

void iniciarTotem() {
  const uint8_t leds[] = {PINO_LED_VERDE, PINO_LED_AMARELO, PINO_LED_VERMELHO};
  for (uint8_t pino : leds) {
    pinMode(pino, OUTPUT);
    digitalWrite(pino, LOW);
  }
  iniciarBuzzer();
  tocarBuzzer(false);
}

static void tarefaTotem(void*) {
  TickType_t ultimo = xTaskGetTickCount();
  for (;;) {
    atualizarTotem();
    vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_TOTEM_MS));
  }
}

void iniciarTarefaTotem() {
  // Prioridade 2: acima do loop() (1), abaixo da amostragem (3). Mesmo nucleo do
  // loop(), mas o loop bloqueado em rede nao ocupa CPU, entao a tarefa roda.
  xTaskCreatePinnedToCore(tarefaTotem, "totem", 4096, nullptr, 2, nullptr, 1);
}

void atualizarTotem() {
  const uint8_t estado = g_estadoTotem.load();
  const uint8_t tendencia = g_tendenciaTotem.load();
  const SaidaSinalizacao s = calcularSinalizacaoDeBytes(estado, tendencia, millis());

  digitalWrite(PINO_LED_VERDE, s.ledVerde ? HIGH : LOW);
  digitalWrite(PINO_LED_AMARELO, s.ledAmarelo ? HIGH : LOW);
  digitalWrite(PINO_LED_VERMELHO, s.ledVermelho ? HIGH : LOW);
  if (s.buzzer != buzzerLigado) {   // so escreve quando muda
    buzzerLigado = s.buzzer;
    tocarBuzzer(buzzerLigado);
  }

  // Um log por mudanca (nao por volta do loop), para o roteiro de validacao.
  if (estado != ultimoEstado || tendencia != ultimaTendencia) {
    ultimoEstado = estado;
    ultimaTendencia = tendencia;
    const bool estadoValido = estado <= (uint8_t)EstadoFisico::ALAGADO;
    const bool tendenciaValida = tendencia <= (uint8_t)TendenciaLocal::DESATUALIZADO;
    const TendenciaLocal t = tendenciaValida ? (TendenciaLocal)tendencia : TendenciaLocal::INSUFICIENTE;
    const bool somLiberado = estadoValido && somPermitido((EstadoFisico)estado, t);
    Serial.printf("TOTEM: estado=%s tendencia=%s som=%s\n",
                  estadoValido ? nomeEstado((EstadoFisico)estado) : "SEM_LEITURA",
                  tendenciaValida ? nomeTendencia((TendenciaLocal)tendencia) : "SEM_LEITURA",
                  somLiberado ? "permitido" : "calado");
  }
}

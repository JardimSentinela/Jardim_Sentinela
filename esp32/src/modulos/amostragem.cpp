// ===== AMOSTRAGEM ===========================================================

#include "amostragem.h"
#include "config.h"
#include "estado.h"
#include "hw02_sonda.h"
#include "relogio.h"

static uint16_t lerSensor(uint8_t i) {
  digitalWrite(PINO_ENERGIA[i], HIGH);
  delayMicroseconds(SETTLE_US);
  uint32_t soma = 0;
  for (int k = 0; k < 4; k++) soma += analogRead(PINO_LEITURA[i]);
  digitalWrite(PINO_ENERGIA[i], LOW);
  uint16_t raw = soma / 4;
#ifdef SIMULATION
  // Pico esporádico (~2% das leituras) para exercitar o debounce
  if (esp_random() % 100 < 2) raw = (raw > 2000) ? raw - 1500 : raw + 1500;
#endif
  return raw;
}

static float nivelDe(const bool* estado) {
  float n = 0.0f;
  for (uint8_t i = 0; i < N_SENSORES; i++) if (estado[i]) n = ALTURA_CM[i];
  return n;
}

static void enviar(const Evento& e) {
  if (xQueueSend(filaEventos, &e, 0) != pdTRUE) descartados++;
}

void tarefaAmostragem(void*) {
  bool estado[N_SENSORES];
  uint8_t cont[N_SENSORES] = {0};
  uint32_t tPrimeiro[N_SENSORES] = {0};
  uint32_t tUltimoPatamar = 0;

  // Estado inicial lido dos sensores
  for (uint8_t i = 0; i < N_SENSORES; i++) {
    estado[i] = lerSensor(i) > (LIMIAR_MOLHA[i] + LIMIAR_SECA[i]) / 2;
  }

  // Ultima leitura da sonda, usada no snapshot (heartbeat). Enquanto ela ainda
  // nao tiver amostrado, o snapshot cai para a escada T1592.
  float ultimoNivelSonda = 0.0f;
  bool  temLeituraSonda  = false;

  // Primeira linha do arquivo: "boot" (seq 0), com o nível lido no boot e as
  // coordenadas configuradas. Fica na frente de qualquer evento ou snapshot.
  Evento b{REG_BOOT, 0, nivelDe(estado), 0, millis(), g_seq++, relogioValido()};
  enviar(b);

  uint32_t proxSnap = millis();
  TickType_t ultimo = xTaskGetTickCount();

  for (;;) {
    // ===== HW-02 - SONDA DE NIVEL =====
    // A sonda se auto-limita a 500 ms; esta task chama a cada 100 ms.
    LeituraSonda sonda;
    if (atualizarSonda(sonda)) {
      ultimoNivelSonda = sonda.nivelCm;
      temLeituraSonda  = true;

      Serial.printf(
          "[HW-02] ADC: %d | Nivel: %.2f cm | Taxa: %.2f cm/min | Mudou: %s\n",
          sonda.adc, sonda.nivelCm, sonda.taxaCmMin,
          sonda.mudou ? "SIM" : "NAO");

      // Variacao >= 1 cm: registra como evento do sensor-04. A taxa ainda nao
      // cabe no quadro v1; por enquanto fica so no log local.
      if (sonda.mudou) {
        Evento e{REG_EVENTO, SENSOR_SONDA, sonda.nivelCm, 0,
                 millis(), g_seq++, relogioValido()};
        enviar(e);
      }
    }

    for (uint8_t i = 0; i < N_SENSORES; i++) {
      uint16_t raw = lerSensor(i);
      bool cruzou = estado[i] ? (raw < LIMIAR_SECA[i]) : (raw > LIMIAR_MOLHA[i]);

      if (!cruzou) { cont[i] = 0; continue; }
      if (cont[i] == 0) tPrimeiro[i] = millis();

      if (++cont[i] >= AMOSTRAS_CONFIRMACAO) {
        estado[i] = !estado[i];
        cont[i] = 0;

        uint32_t dt = 0;
        if (estado[i]) {
          // Subida de nível: registra tempo entre patamares (Δt = tJS - tcontrole)
          if (tUltimoPatamar > 0 && tPrimeiro[i] >= tUltimoPatamar) {
            dt = tPrimeiro[i] - tUltimoPatamar;
          }
          tUltimoPatamar = tPrimeiro[i];
        } else if (nivelDe(estado) == 0.0f) {
          tUltimoPatamar = 0;
        }

        Evento e{REG_EVENTO, i, nivelDe(estado), dt, tPrimeiro[i], g_seq++, relogioValido()};
        enviar(e);
      }
    }

    if ((int32_t)(millis() - proxSnap) >= 0) {
      uint8_t sensAtivo = 0;
      for (int8_t i = N_SENSORES - 1; i >= 0; i--) {
        if (estado[i]) { sensAtivo = i; break; }
      }
      // sensAtivo identifica o sensor molhado mais alto; no arquivo o snapshot
      // usa idSensor "jardim", então esse valor não é gravado.
      // O nivel do heartbeat vem da sonda continua; sem leitura dela ainda,
      // cai para a escada T1592.
      float nivelSnap = temLeituraSonda ? ultimoNivelSonda : nivelDe(estado);
      Evento s{REG_SNAPSHOT, sensAtivo, nivelSnap, 0, millis(), g_seq++, relogioValido()};
      enviar(s);
      proxSnap += PERIODO_SNAPSHOT_MS;
    }
    vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_AMOSTRA_MS));
  }
}

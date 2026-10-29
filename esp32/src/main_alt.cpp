#define SIMULATION
#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>
#include <atomic>
#include "hw02_sonda.h"

// Compile com -DSIMULATION no Wokwi (build_flags no platformio.ini) para
// injetar ruído nas leituras e encurtar o período do snapshot.

// ===== CONFIGURAÇÃO ==========================================================
static const char* JARDIM_ID = "jardim-centro-01";  // Conforme contrato de telemetria

// Sensores resistivos T1592 em ordem CRESCENTE de altura (índice 0 = mais baixo).
// Wokwi: 3 potenciômetros em GPIO32, GPIO33, GPIO34 com alimentação em GPIO25, GPIO26, GPIO27.
constexpr uint8_t N_SENSORES = 3;
constexpr uint8_t PINO_LEITURA[N_SENSORES] = {32, 33, 34};
constexpr uint8_t PINO_ENERGIA[N_SENSORES] = {25, 26, 27};
constexpr float   ALTURA_CM[N_SENSORES]    = {5.0f, 10.0f, 15.0f};

// Limiares (ADC 0-4095) com histerese para evitar oscilações na borda.
constexpr uint16_t LIMIAR_MOLHA[N_SENSORES] = {1500, 1500, 1500};
constexpr uint16_t LIMIAR_SECA[N_SENSORES]  = {1000, 1000, 1000};

constexpr uint32_t PERIODO_AMOSTRA_MS   = 100;
constexpr uint8_t  AMOSTRAS_CONFIRMACAO = 3;
constexpr uint32_t SETTLE_US            = 500;
constexpr size_t   MAX_BYTES_FILA       = 64 * 1024;
constexpr uint8_t  BATERIA_PADRAO       = 100;  // 0 a 100 conforme contrato

// Snapshot periódico (heartbeat).
#ifdef SIMULATION
constexpr uint32_t PERIODO_SNAPSHOT_MS = 5000;
#else
constexpr uint32_t PERIODO_SNAPSHOT_MS = 60000;
#endif

static const char* ARQ_FILA = "/fila.ndjson";

// ===== ESTADO GLOBAL =========================================================

struct Evento {
  bool     snapshot;
  uint8_t  sensor;
  float    nivelAgua;
  uint32_t deltaTMs;
  uint32_t tMs;
};

QueueHandle_t filaEventos;
std::atomic<uint32_t> descartados{0};

// ===== FUNÇÕES AUXILIARES ====================================================

static void formatarTimestamp(uint32_t ms, char* buffer, size_t tamanho) {
  time_t now = time(nullptr);
  if (now < 100000) {
    // Sem sincronização NTP prévia (Etapa 1): marco de referência em 2026-10-15T00:00:00Z
    now = 1792022400 + (ms / 1000);
  }
  struct tm tmInfo;
  gmtime_r(&now, &tmInfo);
  strftime(buffer, tamanho, "%Y-%m-%dT%H:%M:%SZ", &tmInfo);
}

uint16_t lerSensor(uint8_t i) {
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
  uint32_t proxSnap = millis();
  TickType_t ultimo = xTaskGetTickCount();

  for (;;) {

    // ===== HW-02 - SONDA DE NIVEL =====
    LeituraSonda leituraSonda;

    if (atualizarSonda(leituraSonda)) {
      Serial.printf(
          "[HW-02] ADC: %d | Nivel: %.2f cm | Taxa: %.2f cm/min | Mudou: %s\n",
          leituraSonda.adc,
          leituraSonda.nivelCm,
          leituraSonda.taxaCmMin,
          leituraSonda.mudou ? "SIM" : "NAO"
      );
    }

    // ===== SENSORES RESISTIVOS T1592 =====
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

        Evento e{false, i, nivelDe(estado), dt, tPrimeiro[i]};
        enviar(e);
      }
    }

    if ((int32_t)(millis() - proxSnap) >= 0) {
      uint8_t sensAtivo = 0;
      for (int8_t i = N_SENSORES - 1; i >= 0; i--) {
        if (estado[i]) { sensAtivo = i; break; }
      }
      Evento s{true, sensAtivo, nivelDe(estado), 0, millis()};
      enviar(s);
      proxSnap += PERIODO_SNAPSHOT_MS;
    }
    vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_AMOSTRA_MS));
  }
}

// ===== ARMAZENAMENTO LOCAL ===================================================

void persistir(const Evento& e) {
  File f = LittleFS.open(ARQ_FILA, "a");
  if (!f) { Serial.println("ERRO: nao abriu arquivo"); descartados++; return; }

  if (f.size() > MAX_BYTES_FILA) {
    f.close(); descartados++;
    Serial.printf("ERRO: arquivo cheio, evento descartado (total %lu)\n", (unsigned long)descartados.load());
    return;
  }

  char ts[25];
  formatarTimestamp(e.tMs, ts, sizeof(ts));

  char idSensor[16];
  snprintf(idSensor, sizeof(idSensor), "sensor-%02u", e.sensor + 1);

  char linha[160];
  snprintf(linha, sizeof(linha),
    "{\"idJardim\":\"%s\",\"idSensor\":\"%s\",\"nivelAgua\":%.1f,\"bateria\":%u,\"timestamp\":\"%s\"}",
    JARDIM_ID, idSensor, e.nivelAgua, BATERIA_PADRAO, ts);

  f.println(linha);
  f.close();

  if (!e.snapshot) {
    Serial.printf("[LIMIAR] Sensor: %s | nivelAgua: %.1f cm | Delta t: %lu ms | Timestamp: %s\n",
                  idSensor, e.nivelAgua, (unsigned long)e.deltaTMs, ts);
  }
  Serial.println(linha);
}

void despejarFila() {
  Serial.println("--- INICIO ---");
  File f = LittleFS.open(ARQ_FILA, "r");
  if (f) {
    while (f.available()) Serial.println(f.readStringUntil('\n'));
    f.close();
  }
  Serial.printf("--- FIM (descartados: %lu) ---\n", (unsigned long)descartados.load());
}

void limparFila() {
  LittleFS.remove(ARQ_FILA);
  descartados = 0;
  Serial.println("Arquivo apagado.");
}

// ===== INICIALIZAÇÃO E LOOP PRINCIPAL ========================================

void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < N_SENSORES; i++) {
    pinMode(PINO_ENERGIA[i], OUTPUT);
    digitalWrite(PINO_ENERGIA[i], LOW);
    pinMode(PINO_LEITURA[i], INPUT);
  }
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  iniciarSonda();

  if (!LittleFS.begin(true)) Serial.println("ERRO: LittleFS");

  filaEventos = xQueueCreate(32, sizeof(Evento));
  xTaskCreatePinnedToCore(tarefaAmostragem, "amostra", 4096, nullptr, 3, nullptr, 1);
}

void loop() {
  Evento e;
  while (xQueueReceive(filaEventos, &e, 0) == pdTRUE) persistir(e);

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'd') despejarFila();
    else if (c == 'L') limparFila();
  }

  // PONTO DE EXTENSÃO (envio): transmitir aqui o arquivo, só apagando o que o
  // receptor confirmar (ACK). Responsabilidade da Pessoa 3.

  delay(20);
}
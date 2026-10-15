#define SIMULATION
#include <Arduino.h> //framework Arduino (Não é definitiva)
#include <LittleFS.h>  // sistema de arquivos na flash (armazenamento local)
#include <atomic>

// Compile com -DSIMULATION no Wokwi (build_flags no platformio.ini) para
// injetar ruído nas leituras e encurtar o período do snapshot.

// ===== CONFIGURAÇÃO (edite aqui) =============================================
static const char* JARDIM_ID = "jardim-proto-01";  // identifica este protótipo

// Sensores em ordem CRESCENTE de altura (índice 0 = mais baixo).
// Para 4 sensores: PINO_LEITURA {32,33,34,35}, PINO_ENERGIA {25,26,27,14},
// ALTURA_CM {5,10,15,20}. GPIO34/35 são só de entrada: nunca usar como energia.
constexpr uint8_t N_SENSORES = 3;
constexpr uint8_t PINO_LEITURA[N_SENSORES] = {32, 33, 34};
constexpr uint8_t PINO_ENERGIA[N_SENSORES] = {25, 26, 27};
constexpr float   ALTURA_CM[N_SENSORES]    = {5.0f, 10.0f, 15.0f};

// Limiares (ADC 0-4095), calibrar por unidade. Histerese evita oscilar.
constexpr uint16_t LIMIAR_MOLHA[N_SENSORES] = {1500, 1500, 1500};
constexpr uint16_t LIMIAR_SECA[N_SENSORES]  = {1000, 1000, 1000};

constexpr uint32_t PERIODO_AMOSTRA_MS   = 100;
constexpr uint8_t  AMOSTRAS_CONFIRMACAO = 3;
constexpr uint32_t SETTLE_US            = 500;
constexpr size_t   MAX_BYTES_FILA       = 64 * 1024;

// Snapshot periódico com o estado completo (heartbeat + recuperação de estado).
#ifdef SIMULATION
constexpr uint32_t PERIODO_SNAPSHOT_MS = 5000;
#else
constexpr uint32_t PERIODO_SNAPSHOT_MS = 60000;
#endif

static const char* ARQ_FILA = "/fila.ndjson";

// ===== ESTADO GLOBAL =========================================================

// tipo SNAPSHOT: estado de todos os sensores (mascara, bit i = sensor i molhado).
// tipo EVENTO: um sensor mudou de estado.
struct Evento {
  bool     snapshot;
  uint8_t  sensor;    // só em EVENTO
  bool     molhado;   // só em EVENTO
  uint8_t  mascara;   // estado de todos após o evento/snapshot
  float    nivelCm;   // altura do patamar mais alto molhado (0 se nenhum)
  uint32_t seq;       // atribuído na amostragem: lacuna na sequência = perda
  uint32_t tMs;       // millis() no primeiro cruzamento (evento) ou no snapshot
};

QueueHandle_t filaEventos;
uint32_t bootId;
std::atomic<uint32_t> descartados{0};  // fila cheia, arquivo cheio ou erro de arquivo

// ===== SENSORES ==============================================================

uint16_t lerSensor(uint8_t i) {
  digitalWrite(PINO_ENERGIA[i], HIGH);
  delayMicroseconds(SETTLE_US);
  uint32_t soma = 0;
  for (int k = 0; k < 4; k++) soma += analogRead(PINO_LEITURA[i]);
  digitalWrite(PINO_ENERGIA[i], LOW);
  uint16_t raw = soma / 4;
#ifdef SIMULATION
  // Pico esporádico (~2% das leituras) para exercitar o debounce, que com
  // potenciômetro nunca seria testado.
  if (esp_random() % 100 < 2) raw = (raw > 2000) ? raw - 1500 : raw + 1500;
#endif
  return raw;
}

static uint8_t mascaraDe(const bool* estado) {
  uint8_t m = 0;
  for (uint8_t i = 0; i < N_SENSORES; i++) if (estado[i]) m |= (1u << i);
  return m;
}

static float nivelDe(const bool* estado) {
  float n = 0;
  for (uint8_t i = 0; i < N_SENSORES; i++) if (estado[i]) n = ALTURA_CM[i];
  return n;
}

// Envia à fila; se estiver cheia, conta o descarte (antes era silencioso).
static void enviar(const Evento& e) {
  if (xQueueSend(filaEventos, &e, 0) != pdTRUE) descartados++;
}

void tarefaAmostragem(void*) {
  bool estado[N_SENSORES];
  uint8_t cont[N_SENSORES] = {0};
  uint32_t tPrimeiro[N_SENSORES] = {0};
  uint32_t seqLocal = 0;

  // Estado inicial LIDO, não assumido: se o jardim já está molhado no boot,
  // o primeiro snapshot já reflete isso.
  for (uint8_t i = 0; i < N_SENSORES; i++) {
    estado[i] = lerSensor(i) > (LIMIAR_MOLHA[i] + LIMIAR_SECA[i]) / 2;
  }
  uint32_t proxSnap = millis();  // snapshot imediato
  TickType_t ultimo = xTaskGetTickCount();

  for (;;) {
    for (uint8_t i = 0; i < N_SENSORES; i++) {
      uint16_t raw = lerSensor(i);
      bool cruzou = estado[i] ? (raw < LIMIAR_SECA[i]) : (raw > LIMIAR_MOLHA[i]);

      if (!cruzou) { cont[i] = 0; continue; }
      if (cont[i] == 0) tPrimeiro[i] = millis();

      if (++cont[i] >= AMOSTRAS_CONFIRMACAO) {
        estado[i] = !estado[i];
        cont[i] = 0;
        Evento e{false, i, estado[i], mascaraDe(estado), nivelDe(estado), seqLocal++, tPrimeiro[i]};
        enviar(e);
      }
    }

    if ((int32_t)(millis() - proxSnap) >= 0) {
      Evento s{true, 0, false, mascaraDe(estado), nivelDe(estado), seqLocal++, millis()};
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

  char linha[200];
  if (e.snapshot) {
    snprintf(linha, sizeof(linha),
      "{\"tipo\":\"snapshot\",\"idJardim\":\"%s\",\"mascara\":%u,\"nivelCm\":%.1f,\"bootId\":%lu,\"seq\":%lu,\"tMs\":%lu}",
      JARDIM_ID, e.mascara, e.nivelCm,
      (unsigned long)bootId, (unsigned long)e.seq, (unsigned long)e.tMs);
  } else {
    snprintf(linha, sizeof(linha),
      "{\"tipo\":\"evento\",\"idJardim\":\"%s\",\"idSensor\":\"S%u\",\"molhado\":%s,\"mascara\":%u,\"nivelCm\":%.1f,\"bootId\":%lu,\"seq\":%lu,\"tMs\":%lu}",
      JARDIM_ID, e.sensor + 1, e.molhado ? "true" : "false", e.mascara, e.nivelCm,
      (unsigned long)bootId, (unsigned long)e.seq, (unsigned long)e.tMs);
  }
  f.println(linha);
  f.close();
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
  bootId = esp_random();

  for (uint8_t i = 0; i < N_SENSORES; i++) {
    pinMode(PINO_ENERGIA[i], OUTPUT);
    digitalWrite(PINO_ENERGIA[i], LOW);
    pinMode(PINO_LEITURA[i], INPUT);
  }
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

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
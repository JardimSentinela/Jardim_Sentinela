#include <Arduino.h> //framework Arduino (Não é definitiva)
#include <LittleFS.h>  // sistema de arquivos na flash (armazenamento local)

// ===== CONFIGURAÇÃO (edite aqui) =============================================
static const char* JARDIM_ID = "jardim-proto-01";  // identifica este protótipo

constexpr uint8_t N_SENSORES = 3;

// Pinos de leitura (ADC1).
constexpr uint8_t PINO_LEITURA[N_SENSORES] = {32, 33, 34};

// Cada sensor é alimentado por um GPIO próprio, ligado só no instante da
// leitura. Corrente contínua corrói as trilhas do sensor resistivo e faria a
// calibração derivar entre os ensaios.
constexpr uint8_t PINO_ENERGIA[N_SENSORES] = {25, 26, 27};

// Limiares de detecção (valores brutos do ADC, 0-4095). Devem ser calibrados
// por unidade no ensaio 1, pois dependem da condutividade da água.
//   seco -> molhado só quando passa de LIMIAR_MOLHA
//   molhado -> seco só quando cai abaixo de LIMIAR_SECA
// A diferença entre os dois (histerese) evita oscilar perto da fronteira.
constexpr uint16_t LIMIAR_MOLHA[N_SENSORES] = {1500, 1500, 1500};
constexpr uint16_t LIMIAR_SECA[N_SENSORES]  = {1000, 1000, 1000};

constexpr uint32_t PERIODO_AMOSTRA_MS   = 100;   // resolução temporal dos eventos
constexpr uint8_t  AMOSTRAS_CONFIRMACAO = 3;     // leituras seguidas para confirmar (debounce)
constexpr uint32_t SETTLE_US            = 500;   // espera após ligar o sensor, antes de ler
constexpr size_t   MAX_BYTES_FILA       = 64 * 1024;  // teto do arquivo de eventos

static const char* ARQ_FILA = "/fila.ndjson";  // um JSON por linha, em ordem de ocorrência

// ===== ESTADO GLOBAL =========================================================

// Um evento = um sensor mudou de estado (seco<->molhado).
struct Evento {
  uint8_t  sensor;   // índice 0..N_SENSORES-1
  bool     molhado;  // novo estado
  uint32_t tMs;      // millis() no PRIMEIRO cruzamento (não na confirmação)
};

QueueHandle_t filaEventos;   // canal tarefaAmostragem -> loop()
uint32_t bootId;             // número aleatório gerado a cada boot
uint32_t seq = 0;            // contador de eventos desde o boot
uint32_t descartados = 0;    // eventos perdidos por arquivo cheio/erro de arquivo

// ===== SENSORES ==============================================================

// Lê um sensor: liga a energia, espera estabilizar, faz 4 leituras e tira a
// média (reduz ruído), e desliga a energia. Retorna 0-4095; mais alto = mais molhado.
uint16_t lerSensor(uint8_t i) {
  digitalWrite(PINO_ENERGIA[i], HIGH);
  delayMicroseconds(SETTLE_US);
  uint32_t soma = 0;
  for (int k = 0; k < 4; k++) soma += analogRead(PINO_LEITURA[i]);
  digitalWrite(PINO_ENERGIA[i], LOW);
  return soma / 4;
}

// Tarefa que roda sozinha, em paralelo ao loop(). A cada 100 ms lê todos os
// sensores e decide se algum mudou de estado. Só ela decide o estado dos
// sensores; ela não grava arquivo.
void tarefaAmostragem(void*) {
  bool estado[N_SENSORES] = {false};  // estado confirmado de cada sensor (assume seco ao ligar)
  uint8_t cont[N_SENSORES] = {0};     // leituras consecutivas já "do outro lado" do limiar
  uint32_t tPrimeiro[N_SENSORES] = {0};  // quando começou a sequência atual
  TickType_t ultimo = xTaskGetTickCount();

  for (;;) {
    for (uint8_t i = 0; i < N_SENSORES; i++) {
      uint16_t raw = lerSensor(i);

      // O limiar a vencer depende do estado atual (histerese).
      bool cruzou = estado[i] ? (raw < LIMIAR_SECA[i]) : (raw > LIMIAR_MOLHA[i]);

      if (!cruzou) { cont[i] = 0; continue; }  // voltou: era ruído, zera a contagem

      // Primeira leitura além do limiar: guarda o instante. É ele que vai no
      // evento, para não atrasar o tempo em AMOSTRAS_CONFIRMACAO amostras.
      if (cont[i] == 0) tPrimeiro[i] = millis();

      // Leituras suficientes seguidas: muda de fato o estado e emite o evento.
      if (++cont[i] >= AMOSTRAS_CONFIRMACAO) {
        estado[i] = !estado[i];
        cont[i] = 0;
        Evento e{i, estado[i], tPrimeiro[i]};
        xQueueSend(filaEventos, &e, 0);  // não bloqueia; se a fila (32) encher, o evento é perdido
      }
    }
    // Espera até o próximo múltiplo de 100 ms, sem acumular atraso.
    vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_AMOSTRA_MS));
  }
}

// ===== ARMAZENAMENTO LOCAL ===================================================

// Grava o evento no fim do arquivo, como uma linha JSON.
// (bootId, seq) identifica cada evento de forma única.
void persistir(const Evento& e) {
  File f = LittleFS.open(ARQ_FILA, "a");  // "a" = acrescentar (cria se não existir)
  if (!f) { Serial.println("ERRO: nao abriu arquivo"); descartados++; return; }

  // Arquivo cheio: descarta o evento NOVO e conta. Não é silencioso: sai no Serial
  // e o total aparece no fim do despejo.
  if (f.size() > MAX_BYTES_FILA) {
    f.close(); descartados++;
    Serial.printf("ERRO: arquivo cheio, evento descartado (total %lu)\n", (unsigned long)descartados);
    return;
  }

  char linha[160];
  snprintf(linha, sizeof(linha),
    "{\"jardimId\":\"%s\",\"sensorId\":\"S%u\",\"molhado\":%s,\"bootId\":%lu,\"seq\":%lu,\"tMs\":%lu}",
    JARDIM_ID, e.sensor + 1, e.molhado ? "true" : "false",
    (unsigned long)bootId, (unsigned long)seq++, (unsigned long)e.tMs);
  f.println(linha);
  f.close();
  Serial.println(linha);  // log ao vivo para acompanhar o ensaio
}

// Comando 'd': imprime todo o arquivo na Serial, entre marcadores, para copiar
// do monitor serial. NÃO apaga nada.
void despejarFila() {
  Serial.println("--- INICIO ---");
  File f = LittleFS.open(ARQ_FILA, "r");
  if (f) {
    while (f.available()) Serial.println(f.readStringUntil('\n'));
    f.close();
  }
  Serial.printf("--- FIM (descartados: %lu) ---\n", (unsigned long)descartados);
}

// Comando 'L' (maiúsculo, para não apagar sem querer): apaga o arquivo.
// Só use depois de ter copiado os dados com 'd'.
void limparFila() {
  LittleFS.remove(ARQ_FILA);
  descartados = 0;
  Serial.println("Arquivo apagado.");
}

// ===== INICIALIZAÇÃO E LOOP PRINCIPAL ========================================

// Roda uma vez ao ligar/reiniciar.
void setup() {
  Serial.begin(115200);
  bootId = esp_random();  // distingue este boot dos anteriores

  // Energia dos sensores começa desligada; os pinos de leitura são entradas.
  for (uint8_t i = 0; i < N_SENSORES; i++) {
    pinMode(PINO_ENERGIA[i], OUTPUT);
    digitalWrite(PINO_ENERGIA[i], LOW);
    pinMode(PINO_LEITURA[i], INPUT);
  }
  analogReadResolution(12);          // 0-4095
  analogSetAttenuation(ADC_11db);    // faixa de entrada até ~3,3 V

  // true = formata a flash se for a primeira vez.
  if (!LittleFS.begin(true)) Serial.println("ERRO: LittleFS");

  // A fila precisa existir antes da tarefa que escreve nela.
  filaEventos = xQueueCreate(32, sizeof(Evento));
  xTaskCreatePinnedToCore(tarefaAmostragem, "amostra", 4096, nullptr, 3, nullptr, 1);
}

// Roda em repetição. Papel: receber os eventos da tarefa de amostragem,
// guardá-los no arquivo e atender comandos da Serial.
void loop() {
  Evento e;
  while (xQueueReceive(filaEventos, &e, 0) == pdTRUE) persistir(e);  // 0 = não espera

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'd') despejarFila();
    else if (c == 'L') limparFila();
  }

  // PONTO DE EXTENSÃO (LoRa): transmitir aqui os eventos do arquivo, só
  // apagando o que o receptor confirmar. Sem ACK não há taxa de entrega.

  delay(20);
}
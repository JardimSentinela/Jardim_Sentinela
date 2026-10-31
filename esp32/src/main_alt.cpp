// ===== INICIALIZACAO E LOOP PRINCIPAL ========================================
// O firmware foi dividido em modulos, todos em src/modulos/. Este arquivo e
// apenas o ponto de entrada: setup() inicializa o hardware/estado e loop()
// coordena amostragem, comandos seriais e a drenagem da fila.

#include <Arduino.h>
#include <LittleFS.h>

#include "modulos/amostragem.h"
#include "modulos/armazenamento.h"
#include "modulos/config.h"
#include "modulos/cursor.h"
#include "modulos/diagnostico.h"
#include "modulos/drenagem.h"
#include "modulos/estado.h"
#include "modulos/gateway.h"
#include "modulos/hw02_sonda.h"
#include "modulos/rede.h"
#include "modulos/relogio.h"

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

  // Identificador deste boot (8 hex) e contador de registros zerado: o receptor
  // usa os dois para separar boots e detectar buracos na sequência.
  snprintf(g_bootId, sizeof(g_bootId), "%08X", (unsigned)esp_random());
  g_seq = 0;

  // Cursor de confirmação persistido na NVS: evita reenviar após um reset.
  carregarCursor();

  // Relógio: o NTP começa quando o Wi-Fi conectar (iniciarSincronizacaoNtp).
  // Enquanto isso, as linhas saem com horaSincronizada:false e hora de fallback.

  // Diagnóstico do boot, agora que o bootId já existe e o relógio foi tentado.
  logarEstadoBoot();

  // Wi-Fi em segundo plano: o setup não espera a conexão terminar.
  wifiIniciar();

  // Há arquivo antigo? Então pode haver linha não confirmada para drenar.
  g_temPendente = LittleFS.exists(ARQ_FILA);

  filaEventos = xQueueCreate(32, sizeof(Evento));
  xTaskCreatePinnedToCore(tarefaAmostragem, "amostra", 4096, nullptr, 3, nullptr, 1);
}

void loop() {
  atualizarWifi();
  atualizarHora();

  Evento e;
  while (xQueueReceive(filaEventos, &e, 0) == pdTRUE) {
    persistir(e);
    g_temPendente = true;
  }

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'd') despejarFila();
    else if (c == 'L') limparFila();
    else if (c == 'i') imprimirDiagnostico();
    else if (c == 'o') alternarOffline();
    else if (c == 'u') alternarUrlInvalida();
    else if (c == 'e') alternarRespostaForcada();
    else if (c == 't') rodarTesteCodec();
    else if (c == 'x') injetarLinhaTruncada();
    else if (c == 'f') forcarCompactacao();
  }

  // ENVIO (INC 7A): varre o arquivo e manda um lote de até 6 não confirmados.
  // Offline ou sem Wi-Fi não tenta; falha impõe o backoff.
  if (g_temPendente && !g_offline && g_wifi == WIFI_CONECTADO &&
      (int32_t)(millis() - g_proximaTentativaMs) >= 0) {
    tentarEnviarLote();
  }

  // Sem delay(): o loop não bloqueia (critério 12). A amostragem tem
  // prioridade maior (3) e mantém seu período de 100 ms.
}

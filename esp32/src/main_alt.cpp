#ifndef SIMULATION
#define SIMULATION  // também vem de -DSIMULATION no platformio.ini; o guard evita o warning
#endif
#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>
#include <sys/time.h>
#include <atomic>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "codec_quadro.h"     // quadro binario v1 (INC 6)
#if __has_include("secrets.h")
#include "secrets.h"          // ignorado pelo git: URL, SSID e senha
#else
#include "secrets.example.h"  // placeholders: compila sem segredos
#endif

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
constexpr uint8_t  BATERIA_PADRAO       = 100;  // 0 a 100 conforme contrato

// Teto do arquivo de eventos. Constante única, de fácil alteração.
// SIMULAÇÃO: 64 KB. PILOTO: usar um valor maior (ex.: 512 KB) para dar mais
// autonomia quando o rádio não tem cobertura.
// A compactação (item 9) copia o arquivo em blocos de 512 bytes, então um
// limite maior NÃO exige mais RAM — só mais tempo de gravação na flash.
// Se o piloto exigir uma janela maior do que cabe na RAM de forma folgada,
// aí sim será preciso reescrever a cópia em pedaços (não implementado aqui).
constexpr size_t   MAX_BYTES_FILA       = 64 * 1024;

// Fração do arquivo mantida quando ele enche: os registros mais antigos além
// desta marca são descartados. 3/4 deixa folga para os registros novos.
constexpr size_t   FRACAO_MANTER        = 3;

// Coordenadas do jardim.
// PLACEHOLDER: valor configurado, não medido; a equipe define o valor real.
// Na simulação lat/lon saem só na linha de boot. No piloto eles vão em TODAS
// as linhas, para confirmar a localização do jardim a cada registro.
constexpr double   LATITUDE              = 0.0;
constexpr double   LONGITUDE             = 0.0;

// Snapshot periódico (heartbeat).
#ifdef SIMULATION
constexpr uint32_t PERIODO_SNAPSHOT_MS = 5000;
#else
constexpr uint32_t PERIODO_SNAPSHOT_MS = 60000;
#endif

// Rede e envio (INC 2). Timeouts em ms; o loop consulta o estado sem esperar.
constexpr uint32_t WIFI_TIMEOUT_MS     = 15000;  // desiste de conectar sem travar
constexpr uint32_t WIFI_RETENTATIVA_MS = 30000;  // espera para tentar de novo
constexpr uint32_t HTTP_TIMEOUT_MS     = 8000;   // conexão + resposta do POST
constexpr uint32_t NTP_TIMEOUT_MS      = 10000;  // espera máxima pelo NTP

static const char* ARQ_FILA = "/fila.ndjson";
static const char* ARQ_TMP  = "/fila.tmp";   // arquivo temporário da compactação

// NVS do cursor de confirmação (INC 4): último (bootId, seq) confirmado com 2xx.
static const char* NVS_ESPACO    = "envio";
static const char* NVS_ACK_BOOT  = "ackBoot";
static const char* NVS_ACK_SEQ   = "ackSeq";

// Buffer da linha JSON: a linha mais longa é a de boot (~236 bytes com origem).
constexpr size_t   TAMANHO_LINHA         = 320;

// ===== NOMES DOS CAMPOS JSON ================================================
// Centralizados aqui de propósito: se a equipe mudar o nome de um campo no
// contrato, a troca é feita em UM único lugar do firmware.
static const char* CAMPO_TIPO             = "tipo";
static const char* CAMPO_ID_JARDIM        = "idJardim";
static const char* CAMPO_ID_SENSOR        = "idSensor";
static const char* CAMPO_NIVEL_AGUA       = "nivelAgua";
static const char* CAMPO_BATERIA          = "bateria";
static const char* CAMPO_TIMESTAMP        = "timestamp";
static const char* CAMPO_SEQ              = "seq";
static const char* CAMPO_BOOT_ID          = "bootId";
static const char* CAMPO_DELTA_T_MS       = "deltaTMs";
static const char* CAMPO_HORA_SINC        = "horaSincronizada";
static const char* CAMPO_LAT              = "lat";
static const char* CAMPO_LON              = "lon";
static const char* CAMPO_ORIGEM           = "origem";

// Valores textuais dos campos.
static const char* TIPO_EVENTO            = "evento";    // mudança de patamar
static const char* TIPO_SNAPSHOT          = "snapshot";  // heartbeat periódico
static const char* TIPO_BOOT              = "boot";      // primeira linha do boot
static const char* ID_SENSOR_JARDIM       = "jardim";    // snapshot/boot = unidade inteira

// Origem do dado: na simulação não há rádio; no piloto (ramo LoRa) é "campo".
static const char* TEXTO_SIMULACAO        = "simulacao";
static const char* TEXTO_CAMPO            = "campo";
#ifdef SIMULATION
static const char* TEXTO_ORIGEM           = "simulacao";
#else
static const char* TEXTO_ORIGEM           = "campo";
#endif

// ===== RELÓGIO ===============================================================
// Epoch a partir do qual o relógio é considerado sincronizado (NTP ligado).
constexpr time_t   EPOCH_MINIMO_VALIDO   = 1700000000;
// Marco do fallback sem NTP: 2026-10-15T00:00:00Z + milissegundos do boot.
constexpr time_t   MARCO_FALLBACK         = 1792022400;

// ===== ESTADO GLOBAL =========================================================

// Tipo do registro gravado no arquivo.
enum TipoRegistro : uint8_t { REG_EVENTO = 0, REG_SNAPSHOT = 1, REG_BOOT = 2 };

struct Evento {
  TipoRegistro tipo;
  uint8_t  sensor;
  float    nivelAgua;
  uint32_t deltaTMs;
  uint32_t tMs;
  uint32_t seq;         // atribuído na criação do registro (deixa buraco se perder)
  bool     relogioValido; // estado do relógio no momento em que o registro nasceu
};

QueueHandle_t filaEventos;
std::atomic<uint32_t> descartados{0};

// Contadores do envio (comando 'i'). "pendentes" é derivado do arquivo; os
// demais só passam a ser incrementados quando o transporte real existir (INC 2).
struct ContadoresEnvio {
  uint32_t tentativas;
  uint32_t confirmados;
  uint32_t falhas;
  uint32_t dizimados;   // snapshots de backfill descartados pela política (INC 7B)
  uint32_t perdidos;    // nao confirmados removidos pela compactacao (INC 8A)
};
static ContadoresEnvio g_envio = {0, 0, 0, 0, 0};

// INC 8B: latencia criacao->2xx medida por epoch, em segundos. O anel guarda
// as ultimas 128 confirmacoes do boot corrente; os demais casos sao contados
// como desconhecidos (horaSincronizada:false) ou fora do boot corrente.
static const uint8_t LATENCIAS_MAX = 128;
static uint32_t g_latSeg[LATENCIAS_MAX];
static uint8_t  g_latN = 0;
static uint8_t  g_latProx = 0;
static uint32_t g_latDesconhecidas = 0;
static uint32_t g_latForaBoot = 0;
static void zerarLatencia() {
  g_latN = 0;
  g_latProx = 0;
  g_latDesconhecidas = 0;
  g_latForaBoot = 0;
}

// Estado do Wi-Fi, avançado por atualizarWifi() no loop (nunca bloqueia).
enum EstadoWifi : uint8_t { WIFI_OCIOSO, WIFI_CONECTANDO, WIFI_CONECTADO };
static EstadoWifi g_wifi = WIFI_OCIOSO;
static uint32_t    g_wifiMarcMs = 0;

static bool     g_offline = false;            // comando 'o': não tenta rede
static bool     g_temPendente = false;         // há linha ainda não confirmada
static uint32_t g_proximaTentativaMs = 0;      // pausa após falha de envio
static uint32_t g_falhasConsecutivas = 0;      // alimenta o backoff exponencial
static uint32_t g_retryAfterSeg = 0;           // header Retry-After do último 429
static char     g_ultimoErro[40] = "nenhum";   // aparece no comando 'i'

// Comando 'e' (INC 5/6B): força a resposta do gateway local, sem rede.
enum RespostaForcada : uint8_t { RESP_OK, RESP_500, RESP_400, RESP_TIMEOUT };
static RespostaForcada g_resposta = RESP_OK;
static char     g_ackBootId[9] = {0};          // cursor: último 2xx confirmado
static uint32_t g_ackSeq = 0;
static bool     g_ackValido = false;
static uint32_t g_reancoragens = 0;            // cursor sumiu (compactação): recomeçou

// INC 7B: drenagem priorizada. O cursor continua sendo o último confirmado em
// ordem de arquivo; confirmações fora de ordem ficam neste conjunto em RAM
// (perdido no reboot => duplicata, nunca perda). O snapshot mais recente do
// início da drenagem é a âncora; snapshots de backfill são 1 a cada N pelo seq.
static const uint8_t DIZIMACAO_SNAPSHOTS = 10;
struct ChaveResolvida { char boot[9]; uint32_t seq; };
static ChaveResolvida g_resolvidos[32];
static uint8_t  g_resolvidosN = 0;
static bool     g_ancoraEnviada = false;
static bool     g_ancoraSelecionada = false;

// Estado da hora (INC 3): NTP e header Date são tentados sem bloquear o loop.
static bool     g_ntpIniciado = false;
static uint32_t g_ntpInicioMs = 0;
static bool     g_horaResolvida = false;   // já se sabe qual fonte venceu
static bool     g_fallbackLogado = false;
static time_t   g_epochHttp = 0;           // epoch do primeiro 2xx, se ainda sem hora

// Contador de registros deste boot (atribuído só pela tarefa de amostragem,
// que é a única que cria registros).
static uint32_t g_seq = 0;
// Identificador deste boot: 8 hexadecimais, muda a cada reinício.
static char     g_bootId[9] = {0};
// Verdadeiro quando o arquivo pode terminar com uma linha parcial (após o
// boot, ou se uma gravação falhou no meio). Evita pagar a verificação a cada
// registro.
static bool     arquivoPodeTerLinhaParcial = true;

// ===== FUNÇÕES AUXILIARES ====================================================

// Diz se o relógio do sistema já está sincronizado (NTP) e portanto é confiável.
static bool relogioValido() {
  return time(nullptr) >= EPOCH_MINIMO_VALIDO;
}

// PONTO DE LIGAÇÃO DO NTP (INC 3): programa o SNTP sem esperar resposta; o
// loop checa time() e, se o NTP não vier em 10 s, usa o Date do primeiro 2xx.
static void iniciarSincronizacaoNtp() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  g_ntpIniciado = true;
  g_ntpInicioMs = millis();
  Serial.printf("HORA: ntp iniciado (timeout de %u s)\n", (unsigned)(NTP_TIMEOUT_MS / 1000));
}

// Converte o header Date (RFC 7231: "Wed, 21 Oct 2026 20:39:43 GMT") em epoch
// UTC sem depender de timegm/TZ, que podem não existir no core.
static bool parsearDataHttp(const char* texto, time_t* epoch) {
  char semana[4] = {0}, mes[4] = {0};
  int dia = 0, ano = 0, h = 0, m = 0, s = 0;
  if (!texto) return false;
  if (sscanf(texto, "%3s, %d %3s %d %d:%d:%d", semana, &dia, mes, &ano, &h, &m, &s) != 7) return false;
  if (dia < 1 || dia > 31 || ano < 1970 || h > 23 || m > 59 || s > 60) return false;

  static const char* MESES = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* p = strstr(MESES, mes);
  if (!p || ((p - MESES) % 3) != 0) return false;
  int mi = (int)(p - MESES) / 3 + 1;  // 1..12

  // Dias desde 1970-01-01 (algoritmo civil de Howard Hinnant).
  int a = ano - (mi <= 2 ? 1 : 0);
  int era = (a >= 0 ? a : a - 399) / 400;
  unsigned yoe = (unsigned)(a - era * 400);
  unsigned mp = (unsigned)(mi + (mi > 2 ? -3 : 9));
  unsigned doy = (153u * mp + 2u) / 5u + (unsigned)(dia - 1);
  unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  long dias = (long)era * 146097L + (long)doe - 719468L;

  *epoch = (time_t)dias * 86400 + h * 3600 + m * 60 + s;
  return true;
}

// Aplica a hora do header Date (só quando o NTP não venceu) e loga a fonte.
static void aplicarHoraHttp(time_t epoch) {
  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  g_horaResolvida = true;
  Serial.printf("HORA: fonte=http epoch=%ld%s\n", (long)epoch,
                g_fallbackLogado ? " (venceu fallback)" : "");
}

// Acompanha a sincronização sem bloquear: NTP vence se chegar; senão aplica o
// epoch capturado do primeiro 2xx após o timeout; sem nenhum, fica fallback.
static void atualizarHora() {
  if (g_horaResolvida) return;
  if (relogioValido()) {
    g_horaResolvida = true;
    Serial.printf("HORA: fonte=ntp epoch=%ld\n", (long)time(nullptr));
    return;
  }
  uint32_t base = g_ntpIniciado ? g_ntpInicioMs : 0;
  bool expirou = (millis() - base >= NTP_TIMEOUT_MS);
  if (g_epochHttp > 0 && expirou) {
    aplicarHoraHttp(g_epochHttp);
    return;
  }
  if (expirou && !g_fallbackLogado) {
    g_fallbackLogado = true;
    Serial.println("HORA: fonte=fallback (sem sync em 10 s; aguardando Date)");
  }
}

// Converte o instante do evento (ms = millis() quando o evento aconteceu) para
// ISO-8601 UTC. Com relógio sincronizado usa o instante REAL do evento (agora
// menos o tempo já decorrido); sem relógio usa o fallback, que avança 1 s por
// segundo de boot e por isso preserva o intervalo entre os eventos.
static void formatarTimestamp(uint32_t ms, bool sincronizado, char* buffer, size_t tamanho) {
  time_t now = time(nullptr);
  time_t instante;
  if (sincronizado && now >= EPOCH_MINIMO_VALIDO) {
    uint32_t decorrido = millis() - ms;  // sem sinal: seguro no estouro de 2^32 ms
    instante = now - (time_t)(decorrido / 1000);
  } else {
    instante = MARCO_FALLBACK + (time_t)(ms / 1000);
  }
  struct tm tmInfo;
  gmtime_r(&instante, &tmInfo);
  strftime(buffer, tamanho, "%Y-%m-%dT%H:%M:%SZ", &tmInfo);
}

// Converte "YYYY-MM-DDTHH:MM:SSZ" em epoch UTC. Implementado sem timegm, que
// nao existe em todo core; usado pelo codec e pelo teste 't'.
static bool parsearIso(const char* texto, uint32_t* epoch) {
  int ano, mes, dia, hora, min, seg;
  if (!texto || strlen(texto) < 20 || texto[19] != 'Z') return false;
  if (sscanf(texto, "%4d-%2d-%2dT%2d:%2d:%2d", &ano, &mes, &dia, &hora, &min, &seg) != 6) return false;
  if (ano < 1970 || mes < 1 || mes > 12 || dia < 1 || dia > 31) return false;
  if (hora > 23 || min > 59 || seg > 60) return false;

  // Dias desde 1970-01-01 (algoritmo days_from_civil).
  int yy = ano - (mes <= 2 ? 1 : 0);
  int era = (yy >= 0 ? yy : yy - 399) / 400;
  unsigned yoe = (unsigned)(yy - era * 400);
  unsigned doy = (153u * (unsigned)(mes + (mes > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)dia - 1u;
  unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  int32_t dias = era * 146097 + (int32_t)doe - 719468;
  int64_t t = (int64_t)dias * 86400 + hora * 3600 + min * 60 + seg;
  if (t < 0 || t > 0xFFFFFFFFLL) return false;
  *epoch = (uint32_t)t;
  return true;
}

// Formata um epoch UTC no mesmo padrao gravado no arquivo.
static void formatarIso(uint32_t epoch, char* buffer, size_t tamanho) {
  time_t t = (time_t)epoch;
  struct tm tmInfo;
  gmtime_r(&t, &tmInfo);
  strftime(buffer, tamanho, "%Y-%m-%dT%H:%M:%SZ", &tmInfo);
}

// Devolve o texto do tipo do registro ("evento", "snapshot" ou "boot").
static const char* nomeTipo(TipoRegistro tipo) {
  if (tipo == REG_BOOT)     return TIPO_BOOT;
  if (tipo == REG_SNAPSHOT) return TIPO_SNAPSHOT;
  return TIPO_EVENTO;
}

// Monta o identificador do sensor: "sensor-01".."sensor-03" nos eventos;
// "jardim" nos snapshots e na linha de boot, que representam a unidade inteira.
static void formatarIdSensor(const Evento& e, char* buffer, size_t tamanho) {
  if (e.tipo == REG_SNAPSHOT || e.tipo == REG_BOOT) {
    snprintf(buffer, tamanho, "%s", ID_SENSOR_JARDIM);
  } else {
    snprintf(buffer, tamanho, "sensor-%02u", e.sensor + 1);
  }
}

// Anexa um trecho ao fim da linha, sem nunca ultrapassar o buffer.
static void anexar(char* buffer, size_t tamanho, size_t& usado, const char* texto) {
  if (usado >= tamanho - 1) return;
  int n = snprintf(buffer + usado, tamanho - usado, "%s", texto);
  if (n < 0) return;
  usado += (size_t)n;
  if (usado >= tamanho) usado = tamanho - 1;
}

// Monta a linha JSON (sem o '\n' final) de um registro. deltaTMs entra só nos
// eventos de subida de nível; lat/lon entram só na linha de boot.
static void montarLinha(const Evento& e, const char* ts, const char* idSensor,
                        char* buffer, size_t tamanho) {
  char parte[160];
  size_t usado = 0;
  buffer[0] = '\0';

  anexar(buffer, tamanho, usado, "{");
  snprintf(parte, sizeof(parte), "\"%s\":\"%s\",", CAMPO_TIPO, nomeTipo(e.tipo));
  anexar(buffer, tamanho, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":\"%s\",", CAMPO_ID_JARDIM, JARDIM_ID);
  anexar(buffer, tamanho, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":\"%s\",", CAMPO_ID_SENSOR, idSensor);
  anexar(buffer, tamanho, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":%.1f,", CAMPO_NIVEL_AGUA, e.nivelAgua);
  anexar(buffer, tamanho, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":%u,", CAMPO_BATERIA, (unsigned)BATERIA_PADRAO);
  anexar(buffer, tamanho, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":\"%s\",", CAMPO_TIMESTAMP, ts);
  anexar(buffer, tamanho, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":%lu,", CAMPO_SEQ, (unsigned long)e.seq);
  anexar(buffer, tamanho, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":\"%s\"", CAMPO_BOOT_ID, g_bootId);
  anexar(buffer, tamanho, usado, parte);

  if (e.tipo == REG_EVENTO && e.deltaTMs > 0) {
    snprintf(parte, sizeof(parte), ",\"%s\":%lu", CAMPO_DELTA_T_MS, (unsigned long)e.deltaTMs);
    anexar(buffer, tamanho, usado, parte);
  }
  if (e.tipo == REG_BOOT) {
    snprintf(parte, sizeof(parte), ",\"%s\":%.6f,\"%s\":%.6f", CAMPO_LAT, LATITUDE, CAMPO_LON, LONGITUDE);
    anexar(buffer, tamanho, usado, parte);
  }
  snprintf(parte, sizeof(parte), ",\"%s\":%s", CAMPO_HORA_SINC, e.relogioValido ? "true" : "false");
  anexar(buffer, tamanho, usado, parte);
  snprintf(parte, sizeof(parte), ",\"%s\":\"%s\"}", CAMPO_ORIGEM, TEXTO_ORIGEM);
  anexar(buffer, tamanho, usado, parte);
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

  // Primeira linha do arquivo: "boot" (seq 0), com o nível lido no boot e as
  // coordenadas configuradas. Fica na frente de qualquer evento ou snapshot.
  Evento b{REG_BOOT, 0, nivelDe(estado), 0, millis(), g_seq++, relogioValido()};
  enviar(b);

  uint32_t proxSnap = millis();
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
      Evento s{REG_SNAPSHOT, sensAtivo, nivelDe(estado), 0, millis(), g_seq++, relogioValido()};
      enviar(s);
      proxSnap += PERIODO_SNAPSHOT_MS;
    }
    vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_AMOSTRA_MS));
  }
}

// ===== ARMAZENAMENTO LOCAL ===================================================

// Declaracoes antecipadas: a contabilidade da compactacao (INC 8A) usa estas
// funcoes, definidas mais abaixo junto do cursor e da drenagem.
static bool lerLinha(File& f, char* buffer, size_t tamanho);
static bool extrairChave(const char* linha, char* bootId, uint32_t* seq);
static int  indiceResolvido(const char* boot, uint32_t seq);

// Reescreve /fila.ndjson guardando apenas os bytes [inicio, fim) do arquivo
// atual. Grava antes em /fila.tmp e só então substitui o original por rename:
// se faltar energia no meio, o arquivo original continua íntegro.
static bool reescreverTrecho(size_t inicio, size_t fim) {
  if (inicio >= fim) return true;

  File orig = LittleFS.open(ARQ_FILA, "r");
  if (!orig) return false;

  File tmp = LittleFS.open(ARQ_TMP, "w");
  if (!tmp) { orig.close(); return false; }

  if (inicio > 0 && !orig.seek(inicio, SeekSet)) { tmp.close(); orig.close(); return false; }

  // Cópia em blocos: o tamanho do arquivo não é limitado pela RAM.
  char bloco[512];
  size_t restantes = fim - inicio;
  while (restantes > 0) {
    size_t pedir = (restantes < sizeof(bloco)) ? restantes : sizeof(bloco);
    int lidos = orig.read((uint8_t*)bloco, pedir);
    if (lidos <= 0) break;
    tmp.write((uint8_t*)bloco, (size_t)lidos);
    restantes -= (size_t)lidos;
  }
  tmp.close();
  orig.close();

  return LittleFS.rename(ARQ_TMP, ARQ_FILA);
}

// Devolve o offset do começo da primeira linha completa que começa depois de
// 'aPartir'. Devolve 0 se não achar '\n' (não dá para cortar sem quebrar linha).
static size_t inicioDaLinhaSeguinte(File& f, size_t aPartir) {
  if (!f.seek(aPartir, SeekSet)) return 0;
  char bloco[256];
  size_t pos = aPartir;
  for (;;) {
    int lidos = f.read((uint8_t*)bloco, sizeof(bloco));
    if (lidos <= 0) return 0;
    for (int i = 0; i < lidos; i++) {
      if (bloco[i] == '\n') return pos + (size_t)i + 1;
    }
    pos += (size_t)lidos;
  }
}

// Devolve o offset logo após o último '\n' antes de 'limite' (ou 0 se não houver).
static size_t fimDaUltimaLinhaCompleta(File& f, size_t limite) {
  const size_t TAM = 512;
  size_t fim = limite;
  char bloco[512];
  while (fim > 0) {
    size_t inicio = (fim > TAM) ? (fim - TAM) : 0;
    size_t pedir = fim - inicio;
    if (!f.seek(inicio, SeekSet)) return 0;
    int lidos = f.read((uint8_t*)bloco, pedir);
    if (lidos <= 0) return 0;
    for (int i = lidos - 1; i >= 0; i--) {
      if (bloco[i] == '\n') return inicio + (size_t)i + 1;
    }
    fim = inicio;
  }
  return 0;
}

// Conta quantos registros (linhas) existem no prefixo [0, ate) do arquivo.
static uint32_t contarLinhasNoPrefixo(File& f, size_t ate) {
  uint32_t n = 0;
  if (!f.seek(0, SeekSet)) return 0;
  char bloco[512];
  size_t lidos = 0;
  while (lidos < ate) {
    size_t pedir = (ate - lidos < sizeof(bloco)) ? (ate - lidos) : sizeof(bloco);
    int r = f.read((uint8_t*)bloco, pedir);
    if (r <= 0) break;
    for (int i = 0; i < r; i++) if (bloco[i] == '\n') n++;
    lidos += (size_t)r;
  }
  return n;
}

// Offset do inicio da linha que contem a chave do cursor. (size_t)-1 quando o
// cursor nao esta no arquivo (sumiu por compactacao ou nunca foi gravado).
static size_t offsetDoCursor(File& f) {
  if (!g_ackValido) return (size_t)-1;
  if (!f.seek(0, SeekSet)) return (size_t)-1;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0;
  size_t pos = 0;
  while (lerLinha(f, linha, sizeof(linha))) {
    size_t inicio = pos;
    pos = (size_t)f.position();
    if (!extrairChave(linha, boot, &seq)) continue;
    if (strcmp(boot, g_ackBootId) == 0 && seq == g_ackSeq) return inicio;
  }
  return (size_t)-1;
}

// Conta, no prefixo [0, ate), as linhas que a compactacao vai remover sem
// confirmacao. Uma linha antes/na chave do cursor esta confirmada; uma linha
// depois so nao esta se tambem nao constar do conjunto de confirmados fora de
// ordem. Sem cursor no arquivo, conta o prefixo inteiro (conservador).
static uint32_t contarPerdidosNoPrefixo(File& f, size_t ate) {
  size_t cursorOff = offsetDoCursor(f);
  if (!f.seek(0, SeekSet)) return 0;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0, perdidos = 0;
  size_t pos = 0;
  while (pos < ate && lerLinha(f, linha, sizeof(linha))) {
    size_t inicio = pos;
    pos = (size_t)f.position();
    if (inicio >= ate) break;
    if (cursorOff != (size_t)-1 && inicio <= cursorOff) continue;
    if (extrairChave(linha, boot, &seq) && indiceResolvido(boot, seq) >= 0) continue;
    perdidos++;
  }
  return perdidos;
}

// Repara o arquivo se a última gravação ficou pela metade (falta de energia):
// reescreve o arquivo sem a linha truncada, para que ele termine sempre em
// linha completa.
static void recuperarLinhaParcial() {
  File f = LittleFS.open(ARQ_FILA, "r");
  if (!f) return;
  size_t total = f.size();
  if (total == 0) { f.close(); return; }

  f.seek(total - 1, SeekSet);
  int ultimo = f.read();
  if (ultimo == '\n') { f.close(); return; }  // arquivo íntegro

  size_t corte = fimDaUltimaLinhaCompleta(f, total);
  f.close();
  if (corte < total) {
    Serial.printf("AVISO: linha parcial removida (%u bytes)\n", (unsigned)(total - corte));
    reescreverTrecho(0, corte);
  }
}

// Política de arquivo cheio: descarta os registros MAIS ANTIGOS e mantém os
// novos. Libera espaço em blocos (3/4 do limite), reescrevendo o que sobra
// num arquivo temporário e trocando por rename. Com forcar=true (comando 'f'),
// compacta mesmo sem o arquivo estar cheio, mantendo 3/4 do tamanho atual.
static bool compactarFila(bool forcar = false) {
  File f = LittleFS.open(ARQ_FILA, "r");
  if (!f) return false;
  size_t total = f.size();
  if (total == 0) { f.close(); return true; }

  size_t alvo = forcar ? (total * FRACAO_MANTER / 4)
                       : ((MAX_BYTES_FILA / 4) * FRACAO_MANTER);
  if (total <= alvo) { f.close(); return true; }  // já cabe (ou pequeno demais)

  size_t inicio = inicioDaLinhaSeguinte(f, total - alvo);
  if (inicio == 0 || inicio >= total) { f.close(); return false; }

  uint32_t removidos = contarLinhasNoPrefixo(f, inicio);
  uint32_t perdidos  = contarPerdidosNoPrefixo(f, inicio);
  f.close();

  if (!reescreverTrecho(inicio, total)) return false;

  descartados += removidos;
  g_envio.perdidos += perdidos;
  Serial.printf("%s: %lu registro(s) antigo(s) descartado(s), %lu nao confirmado(s) (total %lu/%lu)\n",
                forcar ? "Fila compactada (forcada)" : "Fila cheia",
                (unsigned long)removidos, (unsigned long)perdidos,
                (unsigned long)descartados.load(), (unsigned long)g_envio.perdidos);
  return true;
}

// Grava um registro no fim do arquivo /fila.ndjson (uma linha JSON terminada em
// LF). Se o arquivo estiver cheio, descarta os registros mais antigos para
// caber o novo; o registro novo nunca é descartado por falta de espaço.
void persistir(const Evento& e) {
  // Se a gravação anterior pode ter sido interrompida, repara o arquivo antes.
  if (arquivoPodeTerLinhaParcial) {
    recuperarLinhaParcial();
    arquivoPodeTerLinhaParcial = false;
  }

  char ts[25];
  formatarTimestamp(e.tMs, e.relogioValido, ts, sizeof(ts));

  char idSensor[16];
  formatarIdSensor(e, idSensor, sizeof(idSensor));

  char linha[TAMANHO_LINHA];
  montarLinha(e, ts, idSensor, linha, sizeof(linha));

  size_t necessario = strlen(linha) + 1;  // linha + '\n'

  File f = LittleFS.open(ARQ_FILA, "a");
  if (!f) { Serial.println("ERRO: nao abriu arquivo"); descartados++; return; }

  if (f.size() + necessario > MAX_BYTES_FILA) {
    f.close();
    compactarFila();
    f = LittleFS.open(ARQ_FILA, "a");
    if (!f) { Serial.println("ERRO: nao abriu arquivo"); descartados++; return; }
    if (f.size() + necessario > MAX_BYTES_FILA) {
      f.close();
      Serial.printf("ERRO: nao coube no arquivo, registro descartado (total %lu)\n",
                    (unsigned long)descartados.load());
      descartados++;
      return;
    }
  }

  size_t escritos = f.print(linha);
  f.print("\n");
  f.close();
  if (escritos != strlen(linha)) arquivoPodeTerLinhaParcial = true;

  if (e.tipo == REG_EVENTO) {
    Serial.printf("[LIMIAR] Sensor: %s | nivelAgua: %.1f cm | Delta t: %lu ms | Timestamp: %s\n",
                  idSensor, e.nivelAgua, (unsigned long)e.deltaTMs, ts);
  }
  Serial.println(linha);
}

// Comando 'd': imprime o conteúdo do arquivo no monitor serial.
void despejarFila() {
  Serial.println("--- INICIO ---");
  File f = LittleFS.open(ARQ_FILA, "r");
  if (f) {
    while (f.available()) Serial.println(f.readStringUntil('\n'));
    f.close();
  }
  Serial.printf("--- FIM (descartados: %lu) ---\n", (unsigned long)descartados.load());
}

// Comando 'L': apaga o arquivo da fila (e o temporário de compactação).
void limparFila() {
  LittleFS.remove(ARQ_FILA);
  LittleFS.remove(ARQ_TMP);
  descartados = 0;
  g_envio.perdidos = 0;  // comeca uma contabilidade nova junto com o arquivo
  zerarLatencia();       // o teste de 20 min comeca com p50/p95 limpos
  arquivoPodeTerLinhaParcial = false;

  // INC 7B: o arquivo sumiu, entao o conjunto de confirmados fora de ordem e a
  // ancora nao valem mais. O cursor na NVS e preservado de proposito (como no
  // INC 4): a proxima selecao nao acha a chave e reancora no inicio.
  g_resolvidosN = 0;
  g_ancoraEnviada = false;
  g_ancoraSelecionada = false;

  Serial.println("Arquivo apagado.");
}

// Comando 'x' (INC 8): injeta uma linha JSON truncada, sem LF, no fim do
// arquivo. O proximo persistir (ou o proximo boot) chama recuperarLinhaParcial
// e remove so essa cauda, sem tocar nas linhas completas anteriores.
static void injetarLinhaTruncada() {
  File f = LittleFS.open(ARQ_FILA, "a");
  if (!f) { Serial.println("TESTE: nao abriu a fila para injetar"); return; }
  static const char* TRUNCADA =
      "{\"tipo\":\"snapshot\",\"idJardim\":\"jardim-centro-01\","
      "\"idSensor\":\"jardim\",\"nivelAgua\":";
  size_t n = f.print(TRUNCADA);  // sem '\n' de proposito
  f.close();
  arquivoPodeTerLinhaParcial = true;
  Serial.printf("TESTE: linha truncada injetada (%u bytes, sem LF); resete para o boot recuperar\n",
                (unsigned)n);
}

// Comando 'f' (INC 8): força a compactação mesmo com o arquivo abaixo do
// limite, mantendo 3/4 do tamanho atual. Antes, repara uma eventual cauda
// parcial para a contabilidade de linhas nao misturar os dois casos.
static void forcarCompactacao() {
  if (arquivoPodeTerLinhaParcial) {
    recuperarLinhaParcial();
    arquivoPodeTerLinhaParcial = false;
  }
  File f = LittleFS.open(ARQ_FILA, "r");
  size_t antes = f ? f.size() : 0;
  if (f) f.close();
  Serial.println("--- COMPACTACAO FORCADA ---");
  bool ok = compactarFila(true);
  f = LittleFS.open(ARQ_FILA, "r");
  size_t depois = f ? f.size() : 0;
  if (f) f.close();
  Serial.printf("TESTE: compactacao forcada %s (%u -> %u bytes)\n",
                ok && depois < antes ? "ok" : "sem efeito",
                (unsigned)antes, (unsigned)depois);
}

// ===== CURSOR DE CONFIRMACAO (INC 4) =========================================

// Carrega o cursor da NVS no boot. Ausente = nada foi confirmado ainda.
static void carregarCursor() {
  Preferences p;
  if (!p.begin(NVS_ESPACO, true)) return;
  String boot = p.getString(NVS_ACK_BOOT, "");
  g_ackSeq = p.getUInt(NVS_ACK_SEQ, 0);
  p.end();
  if (boot.length() != 8) return;
  memcpy(g_ackBootId, boot.c_str(), 8);
  g_ackBootId[8] = '\0';
  g_ackValido = true;
}

// Lê uma linha do arquivo sem o LF; false no fim. Linha maior que o buffer é
// truncada, mas o excedente é descartado até o LF.
static bool lerLinha(File& f, char* buffer, size_t tamanho) {
  size_t n = 0;
  while (f.available()) {
    int c = f.read();
    if (c < 0 || c == '\n') break;
    if (n < tamanho - 1) buffer[n++] = (char)c;
  }
  buffer[n] = '\0';
  return n > 0;
}

// Extrai (bootId, seq) de uma linha gravada; linha parcial/inválida é ignorada.
static bool extrairChave(const char* linha, char* bootId, uint32_t* seq) {
  const char* p = strstr(linha, "\"bootId\":\"");
  if (!p || strlen(p + 10) < 8) return false;
  memcpy(bootId, p + 10, 8);
  bootId[8] = '\0';
  p = strstr(linha, "\"seq\":");
  if (!p) return false;
  *seq = (uint32_t)strtoul(p + 6, nullptr, 10);
  return true;
}

// Conta o que ainda não foi confirmado pela mesma regra da varredura. Se o
// cursor não existir no arquivo, conta tudo (o envio vai reancorar).
static uint32_t contarPendentes() {
  File f = LittleFS.open(ARQ_FILA, "r");
  if (!f) return 0;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0, total = 0, depois = 0;
  bool cursorAchado = !g_ackValido;
  while (lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) continue;
    total++;
    if (!cursorAchado) {
      if (strcmp(boot, g_ackBootId) == 0 && seq == g_ackSeq) cursorAchado = true;
      continue;
    }
    depois++;
  }
  f.close();
  return cursorAchado ? depois : total;
}

// Guarda uma amostra de latencia (segundos) no anel; sobrescreve a mais antiga.
static void registrarLatencia(uint32_t segundos) {
  g_latSeg[g_latProx] = segundos;
  g_latProx = (uint8_t)((g_latProx + 1) % LATENCIAS_MAX);
  if (g_latN < LATENCIAS_MAX) g_latN++;
}

// Classifica a confirmacao de um registro e, quando aplicavel, mede
// agora_epoch - timestamp_epoch. So entra no p50/p95 o boot corrente com
// horaSincronizada:true; os demais sao contados e ficam como desconhecidos.
static void registrarLatenciaDoRegistro(const codec::Registro& r, bool horaSinc, const char* boot) {
  if (!horaSinc) { g_latDesconhecidas++; return; }
  if (strcmp(boot, g_bootId) != 0) { g_latForaBoot++; return; }
  time_t agora = time(nullptr);
  if (agora < EPOCH_MINIMO_VALIDO || r.ts < (uint32_t)EPOCH_MINIMO_VALIDO) {
    g_latDesconhecidas++;
    return;
  }
  uint32_t seg = (agora >= (time_t)r.ts) ? (uint32_t)(agora - (time_t)r.ts) : 0;
  registrarLatencia(seg);
}

// Percentil por posto mais proximo (nearest-rank). Devolve 0 sem amostras.
static uint32_t percentilLatencia(uint8_t p) {
  if (g_latN == 0) return 0;
  uint32_t copia[LATENCIAS_MAX];
  memcpy(copia, g_latSeg, (size_t)g_latN * sizeof(uint32_t));
  for (uint8_t i = 1; i < g_latN; i++) {
    uint32_t chave = copia[i];
    int j = (int)i - 1;
    while (j >= 0 && copia[j] > chave) {
      copia[j + 1] = copia[j];
      j--;
    }
    copia[j + 1] = chave;
  }
  uint32_t idx = ((uint32_t)g_latN * p + 99) / 100;
  if (idx == 0) idx = 1;
  if (idx > g_latN) idx = g_latN;
  return copia[idx - 1];
}

// Comando 'i': estado do arquivo e contadores do envio. "pendentes" é o que
// ainda não foi confirmado; o cursor identifica o último 2xx (bootId/seq).
void imprimirDiagnostico() {
  File f = LittleFS.open(ARQ_FILA, "r");
  size_t bytes = 0;
  uint32_t linhas = 0;
  bool existe = (bool)f;
  if (f) {
    bytes = f.size();
    linhas = contarLinhasNoPrefixo(f, bytes);
    f.close();
  }
  char cursor[24];
  if (g_ackValido) snprintf(cursor, sizeof(cursor), "%s/%lu", g_ackBootId, (unsigned long)g_ackSeq);
  else snprintf(cursor, sizeof(cursor), "sem cursor");
  Serial.println("--- DIAGNOSTICO ---");
  Serial.printf("arquivo: %s\n", existe ? "existe" : "inexistente");
  Serial.printf("bytes: %u\n", (unsigned)bytes);
  Serial.printf("linhas: %lu\n", (unsigned long)linhas);
  Serial.printf("pendentes: %lu\n", (unsigned long)contarPendentes());
  Serial.printf("cursor: %s\n", cursor);
  Serial.printf("reancoragens: %lu\n", (unsigned long)g_reancoragens);
  Serial.printf("offline: %s\n", g_offline ? "sim" : "nao");
  Serial.printf("tentativas: %lu\n", (unsigned long)g_envio.tentativas);
  Serial.printf("confirmados: %lu\n", (unsigned long)g_envio.confirmados);
  Serial.printf("falhas: %lu\n", (unsigned long)g_envio.falhas);
  Serial.printf("dizimados: %lu\n", (unsigned long)g_envio.dizimados);
  Serial.printf("resolvidosFora: %u\n", (unsigned)g_resolvidosN);
  Serial.printf("ancora: %s\n", g_ancoraEnviada ? "enviada" : "pendente");
  Serial.printf("ultimoErro: %s\n", g_ultimoErro);
  int32_t restante = (int32_t)(g_proximaTentativaMs - millis());
  Serial.printf("proximoEnvioEm: %ld s\n", restante > 0 ? (long)(restante / 1000) : 0L);
  Serial.printf("descartados: %lu\n", (unsigned long)descartados.load());
  Serial.printf("perdidosNaoConfirmados: %lu\n", (unsigned long)g_envio.perdidos);
  if (g_latN == 0) {
    Serial.printf("latencia: desconhecida (0 amostras; desconhecidas=%lu; foraBoot=%lu)\n",
                  (unsigned long)g_latDesconhecidas, (unsigned long)g_latForaBoot);
  } else {
    Serial.printf("latencia: p50=%lu s p95=%lu s amostras=%u desconhecidas=%lu foraBoot=%lu\n",
                  (unsigned long)percentilLatencia(50), (unsigned long)percentilLatencia(95),
                  (unsigned)g_latN, (unsigned long)g_latDesconhecidas,
                  (unsigned long)g_latForaBoot);
  }
  Serial.printf("bootId: %s\n", g_bootId);
  Serial.printf("relogio: %s\n", relogioValido() ? "valido" : "invalido");
}

// ===== REDE E ENVIO ==========================================================

// Inicia a conexão sem esperar: quem acompanha é o loop, tick a tick.
static void wifiIniciar() {
  Serial.println("WIFI: conectando...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  g_wifi = WIFI_CONECTANDO;
  g_wifiMarcMs = millis();
}

// Máquina de estados do Wi-Fi: só consulta status/millis, nunca bloqueia.
static void atualizarWifi() {
  switch (g_wifi) {
    case WIFI_CONECTANDO:
      if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        g_wifi = WIFI_CONECTADO;
        Serial.printf("WIFI: conectado em %lu ms, IP %u.%u.%u.%u\n",
                      (unsigned long)(millis() - g_wifiMarcMs), ip[0], ip[1], ip[2], ip[3]);
        // Só faz sentido tentar NTP depois que há rede; configTime não bloqueia.
        if (!g_ntpIniciado) iniciarSincronizacaoNtp();
      } else if (millis() - g_wifiMarcMs >= WIFI_TIMEOUT_MS) {
        WiFi.disconnect(true);
        g_wifi = WIFI_OCIOSO;
        g_wifiMarcMs = millis();
        Serial.println("WIFI: timeout de 15 s; segue offline e tenta depois");
      }
      break;
    case WIFI_CONECTADO:
      if (WiFi.status() != WL_CONNECTED) {
        g_wifi = WIFI_OCIOSO;
        g_wifiMarcMs = millis();
        Serial.println("WIFI: caiu; tentando reconectar");
      }
      break;
    case WIFI_OCIOSO:
    default:
      if (millis() - g_wifiMarcMs >= WIFI_RETENTATIVA_MS) wifiIniciar();
      break;
  }
}

// ===== GATEWAY LOCAL (INC 6B) ===============================================
// No Wokwi nao ha radio: o firmware monta o quadro, o "gateway" local decodifica
// e o JSON resultante e o que vai no POST. Fora do SIMULATION quem decodifica e
// o gateway remoto (ramo LoRa, nao compilado aqui).

// Aponta para o valor bruto de "chave":... (sem as aspas); nullptr se ausente.
static const char* valorCampo(const char* linha, const char* chave) {
  char padrao[24];
  snprintf(padrao, sizeof(padrao), "\"%s\":", chave);
  const char* p = strstr(linha, padrao);
  return p ? p + strlen(padrao) : nullptr;
}

// Copia o texto de um campo string (com aspas) para o buffer.
static bool campoTxt(const char* linha, const char* chave, char* out, size_t tam) {
  const char* p = valorCampo(linha, chave);
  if (!p || *p != '"') return false;
  p++;
  const char* fim = strchr(p, '"');
  if (!fim || (size_t)(fim - p) >= tam) return false;
  memcpy(out, p, (size_t)(fim - p));
  out[fim - p] = '\0';
  return true;
}

// Le um campo inteiro nao negativo (strtoul); false se ausente/invalido.
static bool campoU32(const char* linha, const char* chave, uint32_t* valor) {
  const char* p = valorCampo(linha, chave);
  if (!p) return false;
  char* fim = nullptr;
  unsigned long v = strtoul(p, &fim, 10);
  if (fim == p) return false;
  *valor = (uint32_t)v;
  return true;
}

// Le um campo decimal (strtod); false se ausente/invalido.
static bool campoDouble(const char* linha, const char* chave, double* valor) {
  const char* p = valorCampo(linha, chave);
  if (!p) return false;
  char* fim = nullptr;
  double v = strtod(p, &fim);
  if (fim == p) return false;
  *valor = v;
  return true;
}

// Converte uma linha NDJSON do arquivo no quadro v1 (n=1 por enquanto; o lote
// de ate 6 entra no INC 7). "horaSinc" e metadado fora da banda do quadro v1.
static bool montarQuadroDaLinha(const char* linha, codec::Quadro* q, bool* horaSinc) {
  char tipo[12], idSensor[16], bootTexto[9], tsTexto[25], origem[12];
  uint32_t seq = 0, bateria = 0, bootId = 0, ts = 0, deltaMs = 0;
  double nivel = 0.0, lat = 0.0, lon = 0.0;

  if (!campoTxt(linha, CAMPO_TIPO, tipo, sizeof(tipo))) return false;
  uint8_t t;
  if (strcmp(tipo, TIPO_EVENTO) == 0) t = codec::EVENTO;
  else if (strcmp(tipo, TIPO_SNAPSHOT) == 0) t = codec::SNAPSHOT;
  else if (strcmp(tipo, TIPO_BOOT) == 0) t = codec::BOOT;
  else return false;

  uint8_t sensor = codec::SENSOR_JARDIM;
  if (t == codec::EVENTO) {
    unsigned nSensor = 0;
    if (!campoTxt(linha, CAMPO_ID_SENSOR, idSensor, sizeof(idSensor))) return false;
    if (sscanf(idSensor, "sensor-%u", &nSensor) != 1 || nSensor < 1 || nSensor > 31) return false;
    sensor = (uint8_t)(nSensor - 1);
  }

  if (!campoU32(linha, CAMPO_SEQ, &seq)) return false;
  if (!campoTxt(linha, CAMPO_BOOT_ID, bootTexto, sizeof(bootTexto))) return false;
  bootId = (uint32_t)strtoul(bootTexto, nullptr, 16);
  if (!campoTxt(linha, CAMPO_TIMESTAMP, tsTexto, sizeof(tsTexto))) return false;
  if (!parsearIso(tsTexto, &ts)) return false;
  if (!campoDouble(linha, CAMPO_NIVEL_AGUA, &nivel) || nivel < 0.0 || nivel > 255.0) return false;
  if (!campoU32(linha, CAMPO_BATERIA, &bateria) || bateria > 100) return false;
  campoU32(linha, CAMPO_DELTA_T_MS, &deltaMs);  // opcional

  const char* pSinc = valorCampo(linha, CAMPO_HORA_SINC);
  *horaSinc = pSinc && strncmp(pSinc, "true", 4) == 0;
  if (!campoTxt(linha, CAMPO_ORIGEM, origem, sizeof(origem))) origem[0] = '\0';

  if (t == codec::BOOT) {
    campoDouble(linha, CAMPO_LAT, &lat);
    campoDouble(linha, CAMPO_LON, &lon);
  }

  q->flags = (uint8_t)((strcmp(origem, TEXTO_SIMULACAO) == 0 ? codec::FLAG_SIMULACAO : 0) |
                       (t == codec::BOOT ? codec::FLAG_COORDENADAS : 0));
  q->jardimIdx = 0;                    // config do gateway: 0 -> JARDIM_ID
  q->seqBase = (uint16_t)seq;          // v1 usa u16; o cursor guarda o seq original
  q->bootId = bootId;
  q->baseTs = ts;
  q->n = 1;

  codec::Registro& r = q->regs[0];
  r.tipo = t;
  r.sensor = sensor;
  r.seq = q->seqBase;
  r.ts = ts;
  r.nivelCm = (uint8_t)(nivel + 0.5);
  r.bateria = (uint8_t)bateria;
  r.deltaT = (uint16_t)(deltaMs / 100 > 65535 ? 65535 : deltaMs / 100);
  r.lat = (int32_t)(lat * 10000000.0 + (lat >= 0 ? 0.5 : -0.5));
  r.lon = (int32_t)(lon * 10000000.0 + (lon >= 0 ? 0.5 : -0.5));
  r.coords = (t == codec::BOOT);
  return true;
}

// Reconstroi o JSON do registro "idx" a partir do quadro decodificado, na
// MESMA ordem de campos do arquivo. "horaSinc" vem da linha original (o quadro
// v1 nao o carrega).
static bool montarJsonGateway(const codec::Quadro& q, int idx, bool horaSinc, char* out, size_t tam) {
  if (idx < 0 || idx >= q.n || q.jardimIdx != 0) return false;
  const codec::Registro& r = q.regs[idx];
  const char* tipo = (r.tipo == codec::EVENTO) ? TIPO_EVENTO
                   : (r.tipo == codec::SNAPSHOT) ? TIPO_SNAPSHOT : TIPO_BOOT;
  char idSensor[16];
  if (r.tipo == codec::EVENTO) snprintf(idSensor, sizeof(idSensor), "sensor-%02u", r.sensor + 1);
  else snprintf(idSensor, sizeof(idSensor), "%s", ID_SENSOR_JARDIM);
  char ts[25];
  formatarIso(r.ts, ts, sizeof(ts));
  const char* origem = (q.flags & codec::FLAG_SIMULACAO) ? TEXTO_SIMULACAO : TEXTO_CAMPO;

  char parte[160];
  size_t usado = 0;
  out[0] = '\0';
  anexar(out, tam, usado, "{");
  snprintf(parte, sizeof(parte), "\"%s\":\"%s\",", CAMPO_TIPO, tipo);
  anexar(out, tam, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":\"%s\",", CAMPO_ID_JARDIM, JARDIM_ID);
  anexar(out, tam, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":\"%s\",", CAMPO_ID_SENSOR, idSensor);
  anexar(out, tam, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":%.1f,", CAMPO_NIVEL_AGUA, (double)r.nivelCm);
  anexar(out, tam, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":%u,", CAMPO_BATERIA, (unsigned)r.bateria);
  anexar(out, tam, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":\"%s\",", CAMPO_TIMESTAMP, ts);
  anexar(out, tam, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":%u,", CAMPO_SEQ, (unsigned)r.seq);
  anexar(out, tam, usado, parte);
  snprintf(parte, sizeof(parte), "\"%s\":\"%08X\"", CAMPO_BOOT_ID, q.bootId);
  anexar(out, tam, usado, parte);
  if (r.tipo == codec::EVENTO && r.deltaT > 0) {
    snprintf(parte, sizeof(parte), ",\"%s\":%u", CAMPO_DELTA_T_MS, (unsigned)r.deltaT * 100);
    anexar(out, tam, usado, parte);
  }
  if (r.tipo == codec::BOOT) {
    snprintf(parte, sizeof(parte), ",\"%s\":%.6f,\"%s\":%.6f",
             CAMPO_LAT, r.lat / 10000000.0, CAMPO_LON, r.lon / 10000000.0);
    anexar(out, tam, usado, parte);
  }
  snprintf(parte, sizeof(parte), ",\"%s\":%s,\"%s\":\"%s\"}",
           CAMPO_HORA_SINC, horaSinc ? "true" : "false", CAMPO_ORIGEM, origem);
  anexar(out, tam, usado, parte);
  return usado > 0 && usado < tam - 1;
}

// Hexdump compacto do teste 't' (16 bytes por linha).
static void imprimirHex(const uint8_t* b, size_t n) {
  Serial.printf("  hex (%u bytes):", (unsigned)n);
  for (size_t i = 0; i < n; i++) {
    if (i % 16 == 0) Serial.print("\n   ");
    Serial.printf(" %02X", b[i]);
  }
  Serial.println();
}

// Testa um tipo: codifica, decodifica, compara campo a campo e confronta o JSON
// reconstruido com o JSON do arquivo.
static bool testarCodec(const char* nome, const Evento& e, const char* tsTexto) {
  char idSensor[16];
  formatarIdSensor(e, idSensor, sizeof(idSensor));
  char original[TAMANHO_LINHA];
  montarLinha(e, tsTexto, idSensor, original, sizeof(original));

  uint32_t ts = 0;
  if (!parsearIso(tsTexto, &ts)) {
    Serial.printf("CODEC %s: timestamp invalido\n", nome);
    return false;
  }

  codec::Quadro q{};
  q.flags = (uint8_t)((strcmp(TEXTO_ORIGEM, TEXTO_SIMULACAO) == 0 ? codec::FLAG_SIMULACAO : 0) |
                      (e.tipo == REG_BOOT ? codec::FLAG_COORDENADAS : 0));
  q.jardimIdx = 0;
  q.seqBase = (uint16_t)e.seq;
  q.bootId = (uint32_t)strtoul(g_bootId, nullptr, 16);
  q.baseTs = ts;
  q.n = 1;

  codec::Registro& r = q.regs[0];
  r.tipo = (uint8_t)e.tipo;   // REG_EVENTO/SNAPSHOT/BOOT = 0/1/2 no codec
  r.sensor = (e.tipo == REG_EVENTO) ? e.sensor : codec::SENSOR_JARDIM;
  r.seq = e.seq;
  r.ts = ts;
  r.nivelCm = (uint8_t)(e.nivelAgua + 0.5f);
  r.bateria = BATERIA_PADRAO;
  r.deltaT = (uint16_t)(e.deltaTMs / 100);
  r.lat = 0;
  r.lon = 0;
  r.coords = (e.tipo == REG_BOOT);

  uint8_t quadro[codec::MAX_COM_EXTRAS];
  size_t n = codec::codificar(q, quadro, sizeof(quadro));
  codec::Quadro d{};
  bool ok = n > 0 && codec::decodificar(quadro, n, &d) && d.n == 1 &&
            d.flags == q.flags && d.seqBase == q.seqBase && d.bootId == q.bootId &&
            d.baseTs == q.baseTs &&
            d.regs[0].tipo == r.tipo && d.regs[0].sensor == r.sensor &&
            d.regs[0].seq == r.seq && d.regs[0].ts == r.ts &&
            d.regs[0].nivelCm == r.nivelCm && d.regs[0].bateria == r.bateria &&
            d.regs[0].deltaT == r.deltaT && d.regs[0].lat == r.lat &&
            d.regs[0].lon == r.lon && d.regs[0].coords == r.coords;

  char gateway[TAMANHO_LINHA];
  bool jsonOk = ok && montarJsonGateway(d, 0, e.relogioValido, gateway, sizeof(gateway)) &&
                strcmp(original, gateway) == 0;
  Serial.printf("CODEC %-8s: %s (%u bytes) JSON %s\n",
                nome, ok ? "OK" : "FALHOU", (unsigned)n, jsonOk ? "OK" : "FALHOU");
  if (!jsonOk) {
    Serial.printf("  arquivo: %s\n", original);
    Serial.printf("  gateway: %s\n", gateway);
  }
  if (ok) imprimirHex(quadro, n);
  return ok && jsonOk;
}

// Comando 't': round-trip dos tres tipos, incluindo o JSON do gateway.
static void rodarTesteCodec() {
  Serial.println("--- TESTE CODEC ---");
  Evento ev{REG_EVENTO, 0, 5.0f, 3200, 0, 7, true};
  Evento sn{REG_SNAPSHOT, 0, 10.0f, 0, 0, 8, true};
  Evento bt{REG_BOOT, 0, 0.0f, 0, 0, 0, false};
  bool ok = testarCodec("evento", ev, "2026-10-21T20:40:35Z");
  ok = testarCodec("snapshot", sn, "2026-10-21T20:40:40Z") && ok;
  ok = testarCodec("boot", bt, "2026-10-21T20:40:00Z") && ok;
  Serial.printf("CODEC: %s\n", ok ? "TODOS OK" : "FALHOU");
}

// Posta UMA linha já gravada no receptor, sem remontar o JSON (timestamp e
// campos ficam idênticos ao arquivo). Devolve o código HTTP, ou negativo em
// falha de configuração/transporte. Bloqueia no máximo HTTP_TIMEOUT_MS.
static int postarLinha(const char* linha) {
  g_retryAfterSeg = 0;

  // Comando 'e': força a resposta do gateway local, sem tocar na rede.
  if (g_resposta != RESP_OK) {
    if (g_resposta == RESP_TIMEOUT) { Serial.println("HTTP: forcado timeout"); return -1; }
    int forcado = (g_resposta == RESP_500) ? 500 : 400;
    Serial.printf("HTTP: forcado %d\n", forcado);
    return forcado;
  }

  if (strlen(URL_RECEPTOR) == 0) {
    static bool avisouUrl = false;
    if (!avisouUrl) { Serial.println("HTTP: URL_RECEPTOR vazia (preencha secrets.h)"); avisouUrl = true; }
    return -2;
  }

  WiFiClientSecure cliente;
  // LIMITAÇÃO: o gateway do Wokwi não entrega a cadeia de certificados; a
  // validação TLS fica desligada aqui. No hardware, trocar por setCACert().
  cliente.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(cliente, String(URL_RECEPTOR))) {
    Serial.println("HTTP: falha ao iniciar a requisicao");
    return -3;
  }
  http.addHeader("Content-Type", "application/json");
  int codigo = http.POST((uint8_t*)linha, strlen(linha));
  // 429 pode trazer Retry-After em segundos (data HTTP vira 0 -> backoff).
  if (codigo == 429) {
    String retry = http.header("Retry-After");
    if (retry.length() > 0) g_retryAfterSeg = (uint32_t)strtoul(retry.c_str(), nullptr, 10);
  }
  // Primeiro 2xx sem hora confiável: guarda o Date para aplicar se o NTP falhar.
  if (codigo >= 200 && codigo < 300 && !relogioValido() && g_epochHttp == 0) {
    String data = http.header("Date");
    time_t epoch = 0;
    if (data.length() > 0 && parsearDataHttp(data.c_str(), &epoch)) {
      g_epochHttp = epoch;
      Serial.printf("HORA: Date capturado (%ld), aguardando timeout do ntp\n", (long)epoch);
    }
  }
  http.end();
  if (codigo > 0) {
    Serial.printf("HTTP: codigo %d, %u bytes\n", codigo, (unsigned)strlen(linha));
  } else {
    Serial.printf("HTTP: falha %d\n", codigo);
  }
  return codigo;
}

// Backoff exponencial das falhas transitórias: 2, 4, 8, 16, 32, 60, 60... s.
static uint32_t backoffMs(uint32_t falhas) {
  uint32_t ms = 2000;
  while (falhas > 1 && ms < 60000) { ms *= 2; falhas--; }
  return ms > 60000 ? 60000 : ms;
}

// 429 sem Retry-After: 30, 60, 120, 240, 300, 300... s.
static uint32_t backoff429Ms(uint32_t falhas) {
  uint32_t seg = 30;
  while (falhas > 1 && seg < 300) { seg *= 2; falhas--; }
  return (seg > 300 ? 300 : seg) * 1000;
}

// Classifica a falha e agenda a próxima tentativa. O cursor NÃO avança: a linha
// segue pendente (no 429 o receptor pode ter gravado o corpo; haverá duplicata).
static void agendarNovaTentativa(int codigo) {
  g_falhasConsecutivas++;
  uint32_t espera;

  if (codigo == 429 && g_retryAfterSeg > 0) {
    uint32_t seg = g_retryAfterSeg > 300 ? 300 : g_retryAfterSeg;
    espera = seg * 1000;
    if (espera < 5000) espera = 5000;
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "http 429 (Retry-After)");
  } else if (codigo == 429) {
    espera = backoff429Ms(g_falhasConsecutivas);
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "http 429 (limite)");
  } else if (codigo == -2) {
    espera = 60000;
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "config: URL vazia");
  } else if (codigo == -4) {
    espera = 60000;
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "codec: linha invalida");
  } else if (codigo >= 400 && codigo < 500) {
    // Erro de contrato é sistêmico: para a fila e retenta em 5 min (INC 5).
    espera = 300000;
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "http %d (contrato)", codigo);
  } else if (codigo < 0) {
    espera = backoffMs(g_falhasConsecutivas);
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "rede/timeout");
  } else {
    espera = backoffMs(g_falhasConsecutivas);
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "http %d", codigo);
  }

  g_proximaTentativaMs = millis() + espera;
  Serial.printf("ENVIO: %s; falha %lu, proxima em %lu s\n",
                g_ultimoErro, (unsigned long)g_falhasConsecutivas,
                (unsigned long)(espera / 1000));
}

// ===== DRENAGEM (INC 7B) =====================================================
// Ordem: snapshot mais recente (âncora, uma vez por drenagem) -> boot do boot
// corrente -> eventos FIFO -> backfill (boot antigo + snapshots 1 a cada
// DIZIMACAO_SNAPSHOTS). Um quadro não mistura boots. Eventos novos furam a fila
// dos snapshots porque são escolhidos antes deles em cada rodada.
struct LoteLinhas {
  char linha[6][TAMANHO_LINHA + 16];
  char boot[6][9];
  uint32_t seq[6];
  int n;
};

// Posiciona o arquivo logo após o cursor; false se a chave sumiu.
static bool pularAteCursor(File& f) {
  if (!g_ackValido) return true;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0;
  while (lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) continue;
    if (strcmp(boot, g_ackBootId) == 0 && seq == g_ackSeq) return true;
  }
  return false;
}

static int indiceResolvido(const char* boot, uint32_t seq) {
  for (int i = 0; i < g_resolvidosN; i++) {
    if (g_resolvidos[i].seq == seq && strcmp(g_resolvidos[i].boot, boot) == 0) return i;
  }
  return -1;
}

static void removerResolvido(int idx) {
  if (idx < 0 || idx >= g_resolvidosN) return;
  for (int i = idx; i < g_resolvidosN - 1; i++) g_resolvidos[i] = g_resolvidos[i + 1];
  g_resolvidosN--;
}

static void marcarResolvido(const char* boot, uint32_t seq) {
  if (indiceResolvido(boot, seq) >= 0) return;
  if (g_resolvidosN >= 32) {
    // Sem espaço: descarta a chave mais antiga. O cursor pode reenviar depois
    // (duplicata), mas não perde o registro.
    removerResolvido(0);
  }
  memcpy(g_resolvidos[g_resolvidosN].boot, boot, 9);
  g_resolvidos[g_resolvidosN].seq = seq;
  g_resolvidosN++;
}

// Abre a fila depois do cursor. Se a chave sumiu (compactação), reancora no
// início e limpa o conjunto: os confirmados fora de ordem podem duplicar.
static File abrirAposCursor(bool* ok) {
  *ok = false;
  File f = LittleFS.open(ARQ_FILA, "r");
  if (!f) return f;
  if (pularAteCursor(f)) { *ok = true; return f; }
  f.close();
  g_reancoragens++;
  g_resolvidosN = 0;
  g_ancoraEnviada = false;
  g_ackValido = false;       // cursor antigo sumiu: não tenta reencontrá-lo
  g_ackBootId[0] = '\0';
  g_ackSeq = 0;
  Serial.println("ENVIO: cursor nao encontrado; reancorando no inicio");
  f = LittleFS.open(ARQ_FILA, "r");
  *ok = (bool)f;
  return f;
}

static uint8_t tipoDaLinha(const char* linha) {
  char tipo[12];
  if (!campoTxt(linha, CAMPO_TIPO, tipo, sizeof(tipo))) return 255;
  if (strcmp(tipo, TIPO_EVENTO) == 0) return codec::EVENTO;
  if (strcmp(tipo, TIPO_SNAPSHOT) == 0) return codec::SNAPSHOT;
  if (strcmp(tipo, TIPO_BOOT) == 0) return codec::BOOT;
  return 255;
}

static const char* nomeTipoCodec(uint8_t t) {
  return t == codec::EVENTO ? "evento" : t == codec::SNAPSHOT ? "snapshot"
       : t == codec::BOOT ? "boot" : "?";
}

// Grava o cursor atual na NVS (uma vez por avanço, não por registro).
static void persistirCursor() {
  if (!g_ackValido) return;
  Preferences p;
  if (!p.begin(NVS_ESPACO, false)) { Serial.println("NVS: falha ao gravar cursor"); return; }
  p.putString(NVS_ACK_BOOT, g_ackBootId);
  p.putUInt(NVS_ACK_SEQ, g_ackSeq);
  p.end();
}

// Avança o cursor enquanto a próxima linha já foi confirmada (fora de ordem)
// ou é um snapshot dizimado. Para no primeiro registro que precisa ser
// enviado (boot, evento, snapshot mantido ou linha inválida).
static bool avancarCursorResolvido() {
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (!ok) return false;
  // Se o cursor sumiu, abrirAposCursor reancorou e zerou a âncora. Não descarta
  // snapshots antes de a âncora nova ser escolhida.
  if (!g_ancoraEnviada) { f.close(); return false; }
  bool mudou = false;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0;

  while (lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) break;
    int idx = indiceResolvido(boot, seq);
    if (idx >= 0) {
      removerResolvido(idx);
      memcpy(g_ackBootId, boot, 9);
      g_ackSeq = seq;
      g_ackValido = true;
      mudou = true;
      continue;
    }
    uint8_t tipo = tipoDaLinha(linha);
    if (tipo == codec::SNAPSHOT && (seq % DIZIMACAO_SNAPSHOTS) != 0) {
      g_envio.dizimados++;
      memcpy(g_ackBootId, boot, 9);
      g_ackSeq = seq;
      g_ackValido = true;
      mudou = true;
      continue;
    }
    break;
  }
  f.close();
  if (mudou) persistirCursor();
  return mudou;
}

static void adicionarCandidato(LoteLinhas* lote, const char* linha, const char* boot, uint32_t seq) {
  if (lote->n >= 6) return;
  strncpy(lote->linha[lote->n], linha, sizeof(lote->linha[0]) - 1);
  lote->linha[lote->n][sizeof(lote->linha[0]) - 1] = '\0';
  memcpy(lote->boot[lote->n], boot, 9);
  lote->seq[lote->n] = seq;
  lote->n++;
}

// Âncora: o snapshot mais recente ainda não resolvido, uma vez por drenagem.
static bool buscarAncora(LoteLinhas* lote) {
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (!ok) return false;
  char linha[TAMANHO_LINHA + 16];
  char melhor[TAMANHO_LINHA + 16];
  char boot[9], melhorBoot[9];
  uint32_t seq = 0, melhorSeq = 0;
  bool tem = false;

  while (lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) break;
    if (indiceResolvido(boot, seq) >= 0) continue;
    if (tipoDaLinha(linha) != codec::SNAPSHOT) continue;
    strncpy(melhor, linha, sizeof(melhor) - 1);
    melhor[sizeof(melhor) - 1] = '\0';
    memcpy(melhorBoot, boot, 9);
    melhorSeq = seq;
    tem = true;
  }
  f.close();
  if (tem) adicionarCandidato(lote, melhor, melhorBoot, melhorSeq);
  return tem;
}

// Boot indicado (se ainda não resolvido) + até max-1 eventos dele, em ordem.
static int coletarBoot(LoteLinhas* lote, const char* alvo, int max) {
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (!ok) return 0;
  char linha[TAMANHO_LINHA + 16];
  char boot[9];
  uint32_t seq = 0;
  bool achouBoot = false;

  while (lote->n < max && lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) break;
    if (strcmp(boot, alvo) != 0) continue;
    if (indiceResolvido(boot, seq) >= 0) continue;
    uint8_t tipo = tipoDaLinha(linha);
    if (!achouBoot) {
      if (tipo == codec::BOOT) {
        adicionarCandidato(lote, linha, boot, seq);
        achouBoot = true;
      } else if (tipo == codec::EVENTO) {
        break;  // boot já resolvido/antes do cursor; eventos ficam para a etapa 4
      }
      continue;
    }
    if (tipo != codec::EVENTO) continue;
    uint32_t anterior = lote->seq[lote->n - 1];
    if (seq <= anterior || seq - anterior - 1 > 255) break;
    adicionarCandidato(lote, linha, boot, seq);
  }
  f.close();
  return lote->n;
}

// Eventos FIFO: o mais antigo não resolvido (qualquer boot) e os próximos do
// mesmo boot, até max. Um evento novo entra aqui e fura os snapshots.
static int buscarEventos(LoteLinhas* lote, int max) {
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (!ok) return 0;
  char linha[TAMANHO_LINHA + 16];
  char boot[9], alvo[9] = {0};
  uint32_t seq = 0;
  bool temAlvo = false;

  while (lote->n < max && lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) break;
    if (indiceResolvido(boot, seq) >= 0) continue;
    if (tipoDaLinha(linha) != codec::EVENTO) continue;
    if (!temAlvo) {
      memcpy(alvo, boot, 9);
      temAlvo = true;
    } else if (strcmp(boot, alvo) != 0) {
      break;  // mantém a ordem FIFO: outro boot fica para o próximo lote
    }
    if (lote->n > 0) {
      uint32_t anterior = lote->seq[lote->n - 1];
      if (seq <= anterior || seq - anterior - 1 > 255) break;
    }
    adicionarCandidato(lote, linha, boot, seq);
  }
  f.close();
  return lote->n;
}

// Backfill a partir do cursor: inclui o boot antigo (contexto/coordenadas) e
// os snapshots mantidos (seq % N == 0) do mesmo boot, até max. Um boot não é
// dizimado; os snapshots fora da política serão descartados pelo cursor. Para
// no próximo boot ou evento (prioridade já tratada nas etapas anteriores).
static int buscarBackfill(LoteLinhas* lote, int max) {
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (!ok) return 0;
  char linha[TAMANHO_LINHA + 16];
  char boot[9], alvo[9] = {0};
  uint32_t seq = 0;

  while (lote->n < max && lerLinha(f, linha, sizeof(linha))) {
    if (!extrairChave(linha, boot, &seq)) break;
    uint8_t tipo = tipoDaLinha(linha);
    if (tipo == 255) break;
    if (indiceResolvido(boot, seq) >= 0) continue;
    if (tipo == codec::EVENTO) break;

    if (tipo == codec::BOOT) {
      if (lote->n > 0 && strcmp(boot, alvo) != 0) break;
      if (lote->n == 0) {
        memcpy(alvo, boot, 9);
        adicionarCandidato(lote, linha, boot, seq);
      }
      continue;
    }

    if ((seq % DIZIMACAO_SNAPSHOTS) != 0) continue;
    if (lote->n == 0) {
      memcpy(alvo, boot, 9);
    } else {
      if (strcmp(boot, alvo) != 0) break;
      uint32_t anterior = lote->seq[lote->n - 1];
      if (seq <= anterior || seq - anterior - 1 > 255) break;
    }
    adicionarCandidato(lote, linha, boot, seq);
  }
  f.close();
  return lote->n;
}

// Escolhe o próximo quadro segundo a prioridade do INC 7B.
static int selecionarLote(LoteLinhas* lote, int max) {
  lote->n = 0;
  g_ancoraSelecionada = false;

  // Valida/reancora o cursor uma vez, antes de decidir a ordem. Se ele sumiu,
  // abrirAposCursor limpa o conjunto e a âncora; a âncora abaixo já vale para
  // a nova base. Evita reancorar no meio de um lote e pular a âncora.
  bool ok = false;
  File f = abrirAposCursor(&ok);
  if (f) f.close();
  if (!ok) return 0;

  if (!g_ancoraEnviada) {
    if (buscarAncora(lote)) {
      g_ancoraSelecionada = true;
      return lote->n;
    }
    g_ancoraEnviada = true;  // não há snapshot pendente para ancorar
  }

  avancarCursorResolvido();

  // Reancorou durante o avanço (compactação): escolhe a âncora agora, antes de
  // qualquer backfill, em vez de deixar para a próxima rodada.
  if (!g_ancoraEnviada) {
    if (buscarAncora(lote)) {
      g_ancoraSelecionada = true;
      return lote->n;
    }
    g_ancoraEnviada = true;
  }

  if (coletarBoot(lote, g_bootId, max) > 0) return lote->n;
  if (buscarEventos(lote, max) > 0) return lote->n;
  if (buscarBackfill(lote, max) > 0) return lote->n;
  return 0;
}

// Tenta enviar o próximo pendente. Sem pendente, libera a varredura; em falha,
// aplica o backoff classificado pelo código HTTP ou pelo erro de rede.
// Tenta enviar um lote de até 6 registros: monta o quadro, o gateway local
// decodifica e posta um JSON por registro; confirma o maior prefixo 2xx e
// avança o cursor até o último confirmado. Em falha, aplica o backoff.
static void tentarEnviarLote() {
  // Estático para não somar ~2 KB de pilha ao WiFiClientSecure/HTTPClient.
  static LoteLinhas lote;
  int achadas = selecionarLote(&lote, 6);
  if (achadas == 0) {
    g_temPendente = false;
    g_ancoraEnviada = false;       // próxima drenagem ancora de novo
    g_ancoraSelecionada = false;
    return;
  }

  codec::Quadro q{};
  bool horaSinc[6] = {false};
  int n = 0;
  for (int i = 0; i < achadas; i++) {
    // Registros do mesmo quadro precisam de seq crescente e gap <= 255.
    if (n > 0) {
      uint32_t anterior = lote.seq[i - 1];
      if (lote.seq[i] <= anterior || lote.seq[i] - anterior - 1 > 255) break;
    }
    codec::Quadro uma;
    bool hs = false;
    if (!montarQuadroDaLinha(lote.linha[i], &uma, &hs)) break;
    if (n == 0) {
      q.flags = uma.flags;
      q.jardimIdx = uma.jardimIdx;
      q.seqBase = uma.seqBase;
      q.bootId = uma.bootId;
      q.baseTs = uma.baseTs;
    } else if (uma.bootId != q.bootId ||
               ((uma.flags ^ q.flags) & codec::FLAG_SIMULACAO)) {
      break;  // não mistura boots/origens no mesmo quadro
    } else {
      q.flags |= uma.flags;  // liga coords se algum boot do lote tiver
    }
    q.regs[n] = uma.regs[0];
    horaSinc[n] = hs;
    n++;
  }

  if (n == 0) {
    Serial.println("ENVIO: primeira linha do lote invalida");
    g_ancoraSelecionada = false;
    g_envio.falhas++;
    agendarNovaTentativa(-4);
    return;
  }
  q.n = (uint8_t)n;

  uint8_t quadro[codec::MAX_COM_EXTRAS];
  size_t tam = codec::codificar(q, quadro, sizeof(quadro));
  if (tam == 0) {
    Serial.println("ENVIO: falha ao codificar o lote");
    g_envio.falhas++;
    agendarNovaTentativa(-4);
    return;
  }

#ifdef SIMULATION
  codec::Quadro d;
  if (!codec::decodificar(quadro, tam, &d)) {
    Serial.println("ENVIO: falha ao decodificar o lote");
    g_envio.falhas++;
    agendarNovaTentativa(-4);
    return;
  }

  // Gateway local: um POST por registro, na ordem do quadro. O prefixo 2xx é
  // o que o cursor pode confirmar; o primeiro erro interrompe o lote.
  Serial.printf("ENVIO: lote %d [%lu ms]:", d.n, (unsigned long)millis());
  for (int i = 0; i < d.n; i++) {
    Serial.printf(" %s/%u", nomeTipoCodec(d.regs[i].tipo), (unsigned)d.regs[i].seq);
  }
  Serial.println();

  int confirmados = 0;
  int codigo = 0;
  for (int i = 0; i < d.n; i++) {
    char json[TAMANHO_LINHA];
    if (!montarJsonGateway(d, i, horaSinc[i], json, sizeof(json))) {
      codigo = -4;
      g_envio.falhas++;
      break;
    }
    g_envio.tentativas++;
    codigo = postarLinha(json);
    if (codigo >= 200 && codigo < 300) {
      registrarLatenciaDoRegistro(d.regs[i], horaSinc[i], lote.boot[i]);
      confirmados++;
      continue;
    }
    g_envio.falhas++;
    break;
  }

  if (confirmados > 0) {
    for (int i = 0; i < confirmados; i++) marcarResolvido(lote.boot[i], lote.seq[i]);
    g_envio.confirmados += (uint32_t)confirmados;
    if (g_ancoraSelecionada) g_ancoraEnviada = true;
    g_ancoraSelecionada = false;
    // Pode avançar o cursor por registros já resolvidos/dizimados; grava a NVS
    // no máximo uma vez. O que não for contíguo fica no conjunto em RAM.
    avancarCursorResolvido();
  } else {
    g_ancoraSelecionada = false;
  }

  if (confirmados == d.n) {
    if (g_falhasConsecutivas > 0) {
      Serial.printf("ENVIO: recuperado apos %lu falhas\n", (unsigned long)g_falhasConsecutivas);
    }
    g_falhasConsecutivas = 0;
    g_proximaTentativaMs = 0;
    snprintf(g_ultimoErro, sizeof(g_ultimoErro), "nenhum");
    Serial.printf("ENVIO: lote %d/%d 2xx\n", confirmados, d.n);
  } else {
    agendarNovaTentativa(codigo);
    Serial.printf("ENVIO: lote %d/%d 2xx (%s)\n", confirmados, d.n, g_ultimoErro);
  }
#else
  // NÃO COMPILADO NO WOKWI — validar no hardware: enviar o quadro pelo rádio e
  // confirmar o prefixo que o gateway reportar. Sem rádio aqui, não há envio.
  (void)quadro;
  (void)tam;
  g_envio.falhas++;
  agendarNovaTentativa(-4);
#endif
}

// Comando 'o': alterna o modo offline lógico; o arquivo continua crescendo.
static void alternarOffline() {
  g_offline = !g_offline;
  if (!g_offline) g_ancoraEnviada = false;  // nova drenagem: ancora de novo
  Serial.printf("ENVIO: modo %s\n", g_offline ? "offline (nao envia)" : "online");
}

// Comando 'e': cicla ok -> 500 -> 400 -> timeout, para exercitar o backoff.
static void alternarRespostaForcada() {
  switch (g_resposta) {
    case RESP_OK:      g_resposta = RESP_500; break;
    case RESP_500:     g_resposta = RESP_400; break;
    case RESP_400:     g_resposta = RESP_TIMEOUT; break;
    default:           g_resposta = RESP_OK; break;
  }
  const char* nomes[] = {"ok", "500", "400", "timeout"};
  // Trocar de modo tambem zera a espera/backoff, para o teste nao ficar
  // preso nos 5 min de um 400 anterior.
  g_proximaTentativaMs = 0;
  g_falhasConsecutivas = 0;
  Serial.printf("ENVIO: resposta forcada = %s\n", nomes[g_resposta]);
}

// Loga no boot se a fila antiga existe, quantos bytes tem e se o relógio é
// confiável; é o diagnóstico mínimo para interpretar os registros seguintes.
static void logarEstadoBoot() {
  File f = LittleFS.open(ARQ_FILA, "r");
  bool existe = (bool)f;
  size_t bytes = 0;
  if (f) { bytes = f.size(); f.close(); }
  Serial.printf("BOOT: filaExiste=%s bytes=%u bootId=%s relogio=%s\n",
                existe ? "sim" : "nao", (unsigned)bytes, g_bootId,
                relogioValido() ? "valido" : "invalido");
  char cursor[24];
  if (g_ackValido) snprintf(cursor, sizeof(cursor), "%s/%lu", g_ackBootId, (unsigned long)g_ackSeq);
  else snprintf(cursor, sizeof(cursor), "sem");
  Serial.printf("ENVIO: urlConfigurada=%s cursor=%s\n", strlen(URL_RECEPTOR) > 0 ? "sim" : "nao", cursor);
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

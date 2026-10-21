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
};
static ContadoresEnvio g_envio = {0, 0, 0};

// Estado do Wi-Fi, avançado por atualizarWifi() no loop (nunca bloqueia).
enum EstadoWifi : uint8_t { WIFI_OCIOSO, WIFI_CONECTANDO, WIFI_CONECTADO };
static EstadoWifi g_wifi = WIFI_OCIOSO;
static uint32_t    g_wifiMarcMs = 0;

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
// num arquivo temporário e trocando por rename.
static bool compactarFila() {
  File f = LittleFS.open(ARQ_FILA, "r");
  if (!f) return false;
  size_t total = f.size();
  if (total == 0) { f.close(); return true; }

  size_t alvo = (MAX_BYTES_FILA / 4) * FRACAO_MANTER;  // 3/4 do limite
  if (total <= alvo) { f.close(); return true; }        // já cabe

  size_t inicio = inicioDaLinhaSeguinte(f, total - alvo);
  if (inicio == 0 || inicio >= total) { f.close(); return false; }

  uint32_t removidos = contarLinhasNoPrefixo(f, inicio);
  f.close();

  if (!reescreverTrecho(inicio, total)) return false;

  descartados += removidos;
  Serial.printf("Fila cheia: %lu registro(s) antigo(s) descartado(s) (total %lu)\n",
                (unsigned long)removidos, (unsigned long)descartados.load());
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
  arquivoPodeTerLinhaParcial = false;
  Serial.println("Arquivo apagado.");
}

// Comando 'i': estado do arquivo e contadores do envio. "pendentes" é o que
// ainda não foi confirmado; sem cursor (INC 4) nada está confirmado ainda.
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
  uint32_t pendentes = (linhas > g_envio.confirmados) ? (linhas - g_envio.confirmados) : 0;
  Serial.println("--- DIAGNOSTICO ---");
  Serial.printf("arquivo: %s\n", existe ? "existe" : "inexistente");
  Serial.printf("bytes: %u\n", (unsigned)bytes);
  Serial.printf("linhas: %lu\n", (unsigned long)linhas);
  Serial.printf("pendentes: %lu\n", (unsigned long)pendentes);
  Serial.printf("tentativas: %lu\n", (unsigned long)g_envio.tentativas);
  Serial.printf("confirmados: %lu\n", (unsigned long)g_envio.confirmados);
  Serial.printf("falhas: %lu\n", (unsigned long)g_envio.falhas);
  Serial.printf("descartados: %lu\n", (unsigned long)descartados.load());
  Serial.printf("bootId: %s\n", g_bootId);
  Serial.printf("relogio: %s\n", relogioValido() ? "valido" : "invalido");
}

// ===== REDE E ENVIO ==========================================================

// Monta o payload com o MESMO formatador usado no arquivo, para o POST levar
// exatamente os campos gravados (origem, horaSincronizada, seq, bootId).
static void montarPayload(const Evento& e, char* buffer, size_t tamanho) {
  char ts[25];
  char idSensor[16];
  formatarTimestamp(e.tMs, e.relogioValido, ts, sizeof(ts));
  formatarIdSensor(e, idSensor, sizeof(idSensor));
  montarLinha(e, ts, idSensor, buffer, tamanho);
}

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

// Posta UM registro no receptor. Bloqueia no máximo HTTP_TIMEOUT_MS; a
// amostragem roda em prioridade maior e mantém seu período de 100 ms.
static bool postarRegistro(const Evento& e) {
  if (strlen(URL_RECEPTOR) == 0) {
    static bool avisouUrl = false;
    if (!avisouUrl) { Serial.println("HTTP: URL_RECEPTOR vazia (preencha secrets.h)"); avisouUrl = true; }
    return false;
  }

  char payload[TAMANHO_LINHA];
  montarPayload(e, payload, sizeof(payload));

  WiFiClientSecure cliente;
  // LIMITAÇÃO: o gateway do Wokwi não entrega a cadeia de certificados; a
  // validação TLS fica desligada aqui. No hardware, trocar por setCACert().
  cliente.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(cliente, String(URL_RECEPTOR))) {
    Serial.println("HTTP: falha ao iniciar a requisicao");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  int codigo = http.POST((uint8_t*)payload, strlen(payload));
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
    Serial.printf("HTTP: codigo %d, %u bytes\n", codigo, (unsigned)strlen(payload));
  } else {
    Serial.printf("HTTP: falha %d\n", codigo);
  }
  return codigo >= 200 && codigo < 300;
}

// Entrega o registro ao transporte. Offline, devolve false na hora e o dado
// continua no arquivo; o cursor de confirmação entra no INC 4.
static bool enviarRegistro(const Evento& e) {
  if (g_wifi != WIFI_CONECTADO) return false;
  g_envio.tentativas++;
  if (postarRegistro(e)) {
    g_envio.confirmados++;
    return true;
  }
  g_envio.falhas++;
  return false;
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
  Serial.printf("ENVIO: urlConfigurada=%s\n", strlen(URL_RECEPTOR) > 0 ? "sim" : "nao");
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

  // Relógio: o NTP começa quando o Wi-Fi conectar (iniciarSincronizacaoNtp).
  // Enquanto isso, as linhas saem com horaSincronizada:false e hora de fallback.

  // Diagnóstico do boot, agora que o bootId já existe e o relógio foi tentado.
  logarEstadoBoot();

  // Wi-Fi em segundo plano: o setup não espera a conexão terminar.
  wifiIniciar();

  filaEventos = xQueueCreate(32, sizeof(Evento));
  xTaskCreatePinnedToCore(tarefaAmostragem, "amostra", 4096, nullptr, 3, nullptr, 1);
}

void loop() {
  atualizarWifi();
  atualizarHora();

  Evento e;
  bool temParaEnviar = false;
  Evento ultimoGravado;
  while (xQueueReceive(filaEventos, &e, 0) == pdTRUE) {
    persistir(e);
    ultimoGravado = e;
    temParaEnviar = true;
  }

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'd') despejarFila();
    else if (c == 'L') limparFila();
    else if (c == 'i') imprimirDiagnostico();
  }

  // PONTO DE EXTENSÃO (envio): entrega ao transporte o último registro gravado.
  // O INC 2 troca o stub por HTTPS; a varredura por cursor entra no INC 4.
  if (temParaEnviar) enviarRegistro(ultimoGravado);

  // Sem delay(): o loop não bloqueia (critério 12). A amostragem tem
  // prioridade maior (3) e mantém seu período de 100 ms.
}

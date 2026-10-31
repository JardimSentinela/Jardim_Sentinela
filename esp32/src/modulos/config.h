#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <time.h>

// Compile com -DSIMULATION no Wokwi (build_flags no platformio.ini) para
// injetar ruido nas leituras e encurtar o periodo do snapshot. O guard evita
// o warning quando a flag tambem vem do compilador.
#ifndef SIMULATION
#define SIMULATION
#endif

#if __has_include("../secrets.h")
#include "../secrets.h"          // ignorado pelo git: URL, SSID e senha
#else
#include "../secrets.example.h"  // placeholders: compila sem segredos
#endif

// ===== CONFIGURACAO ==========================================================
constexpr const char* JARDIM_ID = "jardim-centro-01";  // Conforme contrato de telemetria

// Sensores resistivos T1592 em ordem CRESCENTE de altura (indice 0 = mais baixo).
// Wokwi: 3 potenciometros em GPIO32, GPIO33, GPIO34 com alimentacao em GPIO25, GPIO26, GPIO27.
constexpr uint8_t N_SENSORES = 3;

// Sonda de nivel HW-02: potenciometro em GPIO35 (3V3/GND). O indice 3 vira
// "sensor-04" no arquivo; o codec v1 aceita sensores de 0 a 30.
constexpr uint8_t SENSOR_SONDA = 3;
constexpr uint8_t PINO_LEITURA[N_SENSORES] = {32, 33, 34};
constexpr uint8_t PINO_ENERGIA[N_SENSORES] = {25, 26, 27};
constexpr float   ALTURA_CM[N_SENSORES]    = {5.0f, 10.0f, 15.0f};

// Limiares (ADC 0-4095) com histerese para evitar oscilacoes na borda.
constexpr uint16_t LIMIAR_MOLHA[N_SENSORES] = {1500, 1500, 1500};
constexpr uint16_t LIMIAR_SECA[N_SENSORES]  = {1000, 1000, 1000};

constexpr uint32_t PERIODO_AMOSTRA_MS   = 100;
constexpr uint8_t  AMOSTRAS_CONFIRMACAO = 3;
constexpr uint32_t SETTLE_US            = 500;
constexpr uint8_t  BATERIA_PADRAO       = 100;  // 0 a 100 conforme contrato

// Teto do arquivo de eventos. Constante unica, de facil alteracao.
// SIMULACAO: 64 KB. PILOTO: usar um valor maior (ex.: 512 KB) para dar mais
// autonomia quando o radio nao tem cobertura.
// A compactacao (item 9) copia o arquivo em blocos de 512 bytes, entao um
// limite maior NAO exige mais RAM - so mais tempo de gravacao na flash.
constexpr size_t   MAX_BYTES_FILA       = 64 * 1024;

// Fracao do arquivo mantida quando ele enche: os registros mais antigos alem
// desta marca sao descartados. 3/4 deixa folga para os registros novos.
constexpr size_t   FRACAO_MANTER        = 3;

// Coordenadas do jardim.
// PLACEHOLDER: valor configurado, nao medido; a equipe define o valor real.
// Na simulacao lat/lon saem so na linha de boot. No piloto eles vao em TODAS
// as linhas, para confirmar a localizacao do jardim a cada registro.
constexpr double   LATITUDE              = 0.0;
constexpr double   LONGITUDE             = 0.0;

// Snapshot periodico (heartbeat).
#ifdef SIMULATION
constexpr uint32_t PERIODO_SNAPSHOT_MS = 5000;
#else
constexpr uint32_t PERIODO_SNAPSHOT_MS = 60000;
#endif

// Rede e envio (INC 2). Timeouts em ms; o loop consulta o estado sem esperar.
constexpr uint32_t WIFI_TIMEOUT_MS     = 15000;  // desiste de conectar sem travar
constexpr uint32_t WIFI_RETENTATIVA_MS = 30000;  // espera para tentar de novo
constexpr uint32_t HTTP_TIMEOUT_MS     = 8000;   // conexao + resposta do POST
constexpr uint32_t NTP_TIMEOUT_MS      = 10000;  // espera maxima pelo NTP

// URL do comando 'u' (INC 8C): dominio .invalid (RFC 2606) nao resolve, para
// exercitar DNS/transporte de verdade sem tocar na URL real de secrets.h.
constexpr const char* URL_INVALIDA = "https://url-invalida.invalid/";

constexpr const char* ARQ_FILA = "/fila.ndjson";
constexpr const char* ARQ_TMP  = "/fila.tmp";   // arquivo temporario da compactacao

// NVS do cursor de confirmacao (INC 4): ultimo (bootId, seq) confirmado com 2xx.
constexpr const char* NVS_ESPACO    = "envio";
constexpr const char* NVS_ACK_BOOT  = "ackBoot";
constexpr const char* NVS_ACK_SEQ   = "ackSeq";

// Buffer da linha JSON: a linha mais longa e a de boot (~236 bytes com origem).
constexpr size_t   TAMANHO_LINHA         = 320;

// ===== NOMES DOS CAMPOS JSON ================================================
// Centralizados aqui de proposito: se a equipe mudar o nome de um campo no
// contrato, a troca e feita em UM unico lugar do firmware.
constexpr const char* CAMPO_TIPO             = "tipo";
constexpr const char* CAMPO_ID_JARDIM        = "idJardim";
constexpr const char* CAMPO_ID_SENSOR        = "idSensor";
constexpr const char* CAMPO_NIVEL_AGUA       = "nivelAgua";
constexpr const char* CAMPO_BATERIA          = "bateria";
constexpr const char* CAMPO_TIMESTAMP        = "timestamp";
constexpr const char* CAMPO_SEQ              = "seq";
constexpr const char* CAMPO_BOOT_ID          = "bootId";
constexpr const char* CAMPO_DELTA_T_MS       = "deltaTMs";
constexpr const char* CAMPO_HORA_SINC        = "horaSincronizada";
constexpr const char* CAMPO_LAT              = "lat";
constexpr const char* CAMPO_LON              = "lon";
constexpr const char* CAMPO_ORIGEM           = "origem";

// Valores textuais dos campos.
constexpr const char* TIPO_EVENTO            = "evento";    // mudanca de patamar
constexpr const char* TIPO_SNAPSHOT          = "snapshot";  // heartbeat periodico
constexpr const char* TIPO_BOOT              = "boot";      // primeira linha do boot
constexpr const char* ID_SENSOR_JARDIM       = "jardim";    // snapshot/boot = unidade inteira

// Origem do dado: na simulacao nao ha radio; no piloto (ramo LoRa) e "campo".
constexpr const char* TEXTO_SIMULACAO        = "simulacao";
constexpr const char* TEXTO_CAMPO            = "campo";
#ifdef SIMULATION
constexpr const char* TEXTO_ORIGEM           = "simulacao";
#else
constexpr const char* TEXTO_ORIGEM           = "campo";
#endif

// ===== RELOGIO ===============================================================
// Epoch a partir do qual o relogio e considerado sincronizado (NTP ligado).
constexpr time_t   EPOCH_MINIMO_VALIDO   = 1700000000;
// Marco do fallback sem NTP: 2026-10-15T00:00:00Z + milissegundos do boot.
constexpr time_t   MARCO_FALLBACK         = 1792022400;

// Politica de dizimacao do backfill (INC 7B): 1 snapshot a cada N.
constexpr uint8_t  DIZIMACAO_SNAPSHOTS    = 10;

#endif

#ifndef ESTADO_H
#define ESTADO_H

#include <Arduino.h>
#include <atomic>
#include <time.h>

#include "config.h"

// Tipo do registro gravado no arquivo.
enum TipoRegistro : uint8_t { REG_EVENTO = 0, REG_SNAPSHOT = 1, REG_BOOT = 2 };

struct Evento {
  TipoRegistro tipo;
  uint8_t  sensor;
  float    nivelAgua;
  uint32_t deltaTMs;
  uint32_t tMs;
  uint32_t seq;           // atribuido na criacao do registro (deixa buraco se perder)
  bool     relogioValido; // estado do relogio no momento em que o registro nasceu
};

// Contadores do envio (comando 'i'). "pendentes" e derivado do arquivo; os
// demais so passam a ser incrementados quando o transporte real existir.
struct ContadoresEnvio {
  uint32_t tentativas;
  uint32_t confirmados;
  uint32_t falhas;
  uint32_t dizimados;   // snapshots de backfill descartados pela politica (INC 7B)
  uint32_t perdidos;    // nao confirmados removidos pela compactacao (INC 8A)
};

// Estado do Wi-Fi, avancado por atualizarWifi() no loop (nunca bloqueia).
enum EstadoWifi : uint8_t { WIFI_OCIOSO, WIFI_CONECTANDO, WIFI_CONECTADO };

// Comando 'e' (INC 5/6B): forca a resposta do gateway local, sem rede.
enum RespostaForcada : uint8_t { RESP_OK, RESP_500, RESP_400, RESP_TIMEOUT };

// INC 7B: confirmacoes fora de ordem ficam neste conjunto em RAM (perdido no
// reboot => duplicata, nunca perda).
struct ChaveResolvida { char boot[9]; uint32_t seq; };

extern QueueHandle_t filaEventos;
extern std::atomic<uint32_t> descartados;
extern ContadoresEnvio g_envio;

extern EstadoWifi g_wifi;
extern uint32_t    g_wifiMarcMs;

extern bool     g_offline;            // comando 'o': nao tenta rede
extern bool     g_urlInvalida;         // comando 'u': usa URL que nao resolve
extern bool     g_temPendente;         // ha linha ainda nao confirmada
extern uint32_t g_proximaTentativaMs;  // pausa apos falha de envio
extern uint32_t g_falhasConsecutivas;  // alimenta o backoff exponencial
extern uint32_t g_retryAfterSeg;       // header Retry-After do ultimo 429
extern char     g_ultimoErro[40];      // aparece no comando 'i'

extern RespostaForcada g_resposta;
extern char     g_ackBootId[9];        // cursor: ultimo 2xx confirmado
extern uint32_t g_ackSeq;
extern bool     g_ackValido;
extern uint32_t g_reancoragens;        // cursor sumiu (compactacao): recomecou

extern ChaveResolvida g_resolvidos[32];
extern uint8_t  g_resolvidosN;
extern bool     g_ancoraEnviada;
extern bool     g_ancoraSelecionada;

// Estado da hora (INC 3): NTP e header Date sao tentados sem bloquear o loop.
extern bool     g_ntpIniciado;
extern uint32_t g_ntpInicioMs;
extern bool     g_horaResolvida;   // ja se sabe qual fonte venceu
extern bool     g_fallbackLogado;
extern time_t   g_epochHttp;       // epoch do primeiro 2xx, se ainda sem hora

// Contador de registros deste boot (atribuido so pela tarefa de amostragem,
// que e a unica que cria registros).
extern uint32_t g_seq;
// Identificador deste boot: 8 hexadecimais, muda a cada reinicio.
extern char     g_bootId[9];
// Verdadeiro quando o arquivo pode terminar com uma linha parcial.
extern bool     arquivoPodeTerLinhaParcial;

#endif

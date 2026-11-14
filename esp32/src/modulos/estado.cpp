#include "estado.h"

QueueHandle_t filaEventos = nullptr;
std::atomic<uint32_t> descartados{0};
ContadoresEnvio g_envio = {0, 0, 0, 0, 0};

EstadoWifi g_wifi = WIFI_OCIOSO;
uint32_t    g_wifiMarcMs = 0;

bool     g_offline = false;
bool     g_urlInvalida = false;
bool     g_temPendente = false;
uint32_t g_proximaTentativaMs = 0;
uint32_t g_falhasConsecutivas = 0;
uint32_t g_retryAfterSeg = 0;
char     g_ultimoErro[40] = "nenhum";

RespostaForcada g_resposta = RESP_OK;
char     g_ackBootId[9] = {0};
uint32_t g_ackSeq = 0;
bool     g_ackValido = false;
uint32_t g_reancoragens = 0;

ChaveResolvida g_resolvidos[32];
uint8_t  g_resolvidosN = 0;
bool     g_ancoraEnviada = false;
bool     g_ancoraSelecionada = false;

bool     g_ntpIniciado = false;
uint32_t g_ntpInicioMs = 0;
bool     g_horaResolvida = false;
bool     g_fallbackLogado = false;
time_t   g_epochHttp = 0;

uint32_t g_seq = 0;
char     g_bootId[9] = {0};
bool     arquivoPodeTerLinhaParcial = true;

// ===== RELOGIO ==============================================================

#include "relogio.h"
#include "config.h"
#include "estado.h"

#include <sys/time.h>

// Diz se o relógio do sistema já está sincronizado (NTP) e portanto é confiável.
bool relogioValido() {
  return time(nullptr) >= EPOCH_MINIMO_VALIDO;
}

// PONTO DE LIGAÇÃO DO NTP (INC 3): programa o SNTP sem esperar resposta; o
// loop checa time() e, se o NTP não vier em 10 s, usa o Date do primeiro 2xx.
void iniciarSincronizacaoNtp() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  g_ntpIniciado = true;
  g_ntpInicioMs = millis();
  Serial.printf("HORA: ntp iniciado (timeout de %u s)\n", (unsigned)(NTP_TIMEOUT_MS / 1000));
}

// Converte o header Date (RFC 7231: "Wed, 21 Oct 2026 20:39:43 GMT") em epoch
// UTC sem depender de timegm/TZ, que podem não existir no core.
bool parsearDataHttp(const char* texto, time_t* epoch) {
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
void atualizarHora() {
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
void formatarTimestamp(uint32_t ms, bool sincronizado, char* buffer, size_t tamanho) {
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
bool parsearIso(const char* texto, uint32_t* epoch) {
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
void formatarIso(uint32_t epoch, char* buffer, size_t tamanho) {
  time_t t = (time_t)epoch;
  struct tm tmInfo;
  gmtime_r(&t, &tmInfo);
  strftime(buffer, tamanho, "%Y-%m-%dT%H:%M:%SZ", &tmInfo);
}

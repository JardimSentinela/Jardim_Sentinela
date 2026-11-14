// ===== LINHA JSON ===========================================================

#include "linha.h"
#include "config.h"
#include "estado.h"

// Devolve o texto do tipo do registro ("evento", "snapshot" ou "boot").
const char* nomeTipo(TipoRegistro tipo) {
  if (tipo == REG_BOOT)     return TIPO_BOOT;
  if (tipo == REG_SNAPSHOT) return TIPO_SNAPSHOT;
  return TIPO_EVENTO;
}

// Monta o identificador do sensor: "sensor-01".."sensor-03" nos eventos;
// "jardim" nos snapshots e na linha de boot, que representam a unidade inteira.
void formatarIdSensor(const Evento& e, char* buffer, size_t tamanho) {
  if (e.tipo == REG_SNAPSHOT || e.tipo == REG_BOOT) {
    snprintf(buffer, tamanho, "%s", ID_SENSOR_JARDIM);
  } else {
    snprintf(buffer, tamanho, "sensor-%02u", e.sensor + 1);
  }
}

// Anexa um trecho ao fim da linha, sem nunca ultrapassar o buffer.
void anexar(char* buffer, size_t tamanho, size_t& usado, const char* texto) {
  if (usado >= tamanho - 1) return;
  int n = snprintf(buffer + usado, tamanho - usado, "%s", texto);
  if (n < 0) return;
  usado += (size_t)n;
  if (usado >= tamanho) usado = tamanho - 1;
}

// Monta a linha JSON (sem o '\n' final) de um registro. deltaTMs entra só nos
// eventos de subida de nível; lat/lon entram só na linha de boot.
void montarLinha(const Evento& e, const char* ts, const char* idSensor,
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

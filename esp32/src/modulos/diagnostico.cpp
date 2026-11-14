// ===== DIAGNOSTICO (COMANDO 'i') ==============================================

#include "diagnostico.h"
#include "config.h"
#include "estado.h"
#include "armazenamento.h"
#include "cursor.h"
#include "latencia.h"
#include "relogio.h"
#include <LittleFS.h>

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
  Serial.printf("urlTeste: %s\n", g_urlInvalida ? "invalida" : "normal");
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
  uint8_t amostras = latenciaAmostras();
  uint32_t desconhecidas = latenciaDesconhecidas();
  uint32_t foraBoot = latenciaForaBoot();
  if (amostras == 0) {
    Serial.printf("latencia: desconhecida (0 amostras; desconhecidas=%lu; foraBoot=%lu)\n",
                  (unsigned long)desconhecidas, (unsigned long)foraBoot);
  } else {
    Serial.printf("latencia: p50=%lu s p95=%lu s amostras=%u desconhecidas=%lu foraBoot=%lu\n",
                  (unsigned long)percentilLatencia(50), (unsigned long)percentilLatencia(95),
                  (unsigned)amostras, (unsigned long)desconhecidas,
                  (unsigned long)foraBoot);
  }
  Serial.printf("bootId: %s\n", g_bootId);
  Serial.printf("relogio: %s\n", relogioValido() ? "valido" : "invalido");
}

// Loga no boot se a fila antiga existe, quantos bytes tem e se o relógio é
// confiável; é o diagnóstico mínimo para interpretar os registros seguintes.
void logarEstadoBoot() {
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

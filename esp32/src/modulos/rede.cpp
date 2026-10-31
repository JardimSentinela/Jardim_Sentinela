// ===== REDE E ENVIO ==========================================================

#include "rede.h"
#include "config.h"
#include "estado.h"
#include "relogio.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Inicia a conexão sem esperar: quem acompanha é o loop, tick a tick.
void wifiIniciar() {
  Serial.println("WIFI: conectando...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  g_wifi = WIFI_CONECTANDO;
  g_wifiMarcMs = millis();
}

// Máquina de estados do Wi-Fi: só consulta status/millis, nunca bloqueia.
void atualizarWifi() {
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

// Posta UMA linha já gravada no receptor, sem remontar o JSON (timestamp e
// campos ficam idênticos ao arquivo). Devolve o código HTTP, ou negativo em
// falha de configuração/transporte. Bloqueia no máximo HTTP_TIMEOUT_MS.
int postarLinha(const char* linha) {
  g_retryAfterSeg = 0;

  // Comando 'e': força a resposta do gateway local, sem tocar na rede.
  if (g_resposta != RESP_OK) {
    if (g_resposta == RESP_TIMEOUT) { Serial.println("HTTP: forcado timeout"); return -1; }
    int forcado = (g_resposta == RESP_500) ? 500 : 400;
    Serial.printf("HTTP: forcado %d\n", forcado);
    return forcado;
  }

  const char* url = g_urlInvalida ? URL_INVALIDA : URL_RECEPTOR;
  if (strlen(url) == 0) {
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
  if (!http.begin(cliente, String(url))) {
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

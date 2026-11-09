// ===== GATEWAY LOCAL (INC 6B) =================================================

#include "gateway.h"
#include "config.h"
#include "estado.h"
#include "linha.h"
#include "relogio.h"

// Aponta para o valor bruto de "chave":... (sem as aspas); nullptr se ausente.
static const char* valorCampo(const char* linha, const char* chave) {
  char padrao[24];
  snprintf(padrao, sizeof(padrao), "\"%s\":", chave);
  const char* p = strstr(linha, padrao);
  return p ? p + strlen(padrao) : nullptr;
}

// Copia o texto de um campo string (com aspas) para o buffer.
bool campoTxt(const char* linha, const char* chave, char* out, size_t tam) {
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
bool montarQuadroDaLinha(const char* linha, codec::Quadro* q, bool* horaSinc) {
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
bool montarJsonGateway(const codec::Quadro& q, int idx, bool horaSinc, char* out, size_t tam) {
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
void rodarTesteCodec() {
  Serial.println("--- TESTE CODEC ---");
  Evento ev{REG_EVENTO, 0, 5.0f, 3200, 0, 7, true};
  Evento sn{REG_SNAPSHOT, 0, 10.0f, 0, 0, 8, true};
  Evento bt{REG_BOOT, 0, 0.0f, 0, 0, 0, false};
  bool ok = testarCodec("evento", ev, "2026-10-21T20:40:35Z");
  ok = testarCodec("snapshot", sn, "2026-10-21T20:40:40Z") && ok;
  ok = testarCodec("boot", bt, "2026-10-21T20:40:00Z") && ok;
  Serial.printf("CODEC: %s\n", ok ? "TODOS OK" : "FALHOU");
}

uint8_t tipoDaLinha(const char* linha) {
  char tipo[12];
  if (!campoTxt(linha, CAMPO_TIPO, tipo, sizeof(tipo))) return 255;
  if (strcmp(tipo, TIPO_EVENTO) == 0) return codec::EVENTO;
  if (strcmp(tipo, TIPO_SNAPSHOT) == 0) return codec::SNAPSHOT;
  if (strcmp(tipo, TIPO_BOOT) == 0) return codec::BOOT;
  return 255;
}

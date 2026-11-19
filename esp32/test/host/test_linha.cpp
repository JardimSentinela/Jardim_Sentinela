// Linha NDJSON gravada pelo firmware (montarLinha): idJardim = JARDIM_ID (J01),
// campos novos (estado, tendenciaDispositivo, versaoRegra) so quando o registro
// tem classificacao, na ordem combinada com o gateway, e o round-trip
// linha -> quadro v2 -> JSON, que precisa devolver a mesma linha.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include "evento.h"
#include "config.h"
#include "estado_fisico.cpp"
#include "tendencia_local.cpp"

static char g_bootId[9] = "A1B2C3D4";

#include "iso.inc"
#include "anexar.inc"
#include "linha.inc"
#include "gateway.inc"

static int falhas = 0;

static void checar(bool cond, const std::string& nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome.c_str());
  if (!cond) falhas++;
}

static const char* TS = "2026-10-21T20:40:35Z";
static const uint8_t SEM = codec::SEM_CLASSIFICACAO;

static Evento evento(TipoRegistro tipo, uint8_t sensor, float nivel, uint32_t deltaTMs,
                     uint32_t seq, bool relogio, uint8_t estado, uint8_t tendencia) {
  Evento e;
  e.tipo = tipo;
  e.sensor = sensor;
  e.nivelAgua = nivel;
  e.deltaTMs = deltaTMs;
  e.tMs = 0;
  e.seq = seq;
  e.relogioValido = relogio;
  e.estado = estado;
  e.tendencia = tendencia;
  return e;
}

static std::string montar(const Evento& e) {
  char idSensor[16];
  formatarIdSensor(e, idSensor, sizeof(idSensor));
  char linha[TAMANHO_LINHA];
  montarLinha(e, TS, idSensor, linha, sizeof(linha));
  return linha;
}

// linha -> quadro -> bytes -> quadro -> JSON, como no envio.
static bool viaGateway(const std::string& entrada, std::string* saida) {
  codec::Quadro q{};
  bool horaSinc = false;
  if (!montarQuadroDaLinha(entrada.c_str(), &q, &horaSinc)) return false;
  uint8_t bytes[codec::MAX_COM_EXTRAS];
  size_t n = codec::codificar(q, bytes, sizeof(bytes));
  codec::Quadro d{};
  if (n == 0 || !codec::decodificar(bytes, n, &d)) return false;
  char json[TAMANHO_LINHA];
  if (!montarJsonGateway(d, 0, horaSinc, json, sizeof(json))) return false;
  *saida = json;
  return true;
}

static bool contem(const std::string& t, const char* trecho) { return t.find(trecho) != std::string::npos; }

static std::string novos(const char* estado, const char* tendencia) {
  std::string s;
  if (estado) s += std::string(",\"estado\":\"") + estado + "\"";
  if (tendencia) s += std::string(",\"tendenciaDispositivo\":\"") + tendencia + "\"";
  if (estado || tendencia) s += std::string(",\"versaoRegra\":\"") + VERSAO_REGRA + "\"";
  return s;
}

static std::string esperada(const char* tipo, const char* idSensor, const char* nivel, unsigned seq,
                            const std::string& extras, const std::string& campos, bool relogio) {
  char b[480];
  std::snprintf(b, sizeof(b),
                "{\"tipo\":\"%s\",\"idJardim\":\"%s\",\"idSensor\":\"%s\",\"nivelAgua\":%s,"
                "\"bateria\":100,\"timestamp\":\"%s\",\"seq\":%u,\"bootId\":\"A1B2C3D4\"%s%s,"
                "\"horaSincronizada\":%s,\"origem\":\"simulacao\"}",
                tipo, JARDIM_ID, idSensor, nivel, TS, seq, extras.c_str(), campos.c_str(),
                relogio ? "true" : "false");
  return b;
}

static void testarIdDoJardim() {
  checar(std::strcmp(JARDIM_ID, "J01") == 0, "JARDIM_ID = J01 (decisoes §6.1)");
  checar(contem(montar(evento(REG_SNAPSHOT, 0, 10.0f, 0, 1, true, SEM, SEM)), "\"idJardim\":\"J01\""),
         "a linha gravada usa idJardim J01");
}

// Registro sem classificacao: a linha e a mesma de sempre (so muda o idJardim).
static void testarSemClassificacao() {
  checar(montar(evento(REG_EVENTO, 0, 12.0f, 3200, 7, true, SEM, SEM)) ==
             esperada("evento", "sensor-01", "12.0", 7, ",\"deltaTMs\":3200", "", true),
         "evento sem classificacao: linha no formato antigo");
  checar(montar(evento(REG_SNAPSHOT, 0, 30.0f, 0, 8, true, SEM, SEM)) ==
             esperada("snapshot", "jardim", "30.0", 8, "", "", true),
         "snapshot sem classificacao: linha no formato antigo");
  checar(montar(evento(REG_BOOT, 0, 0.0f, 0, 0, false, SEM, SEM)) ==
             esperada("boot", "jardim", "0.0", 0, ",\"lat\":0.000000,\"lon\":0.000000", "", false),
         "boot sem classificacao: linha no formato antigo");
}

// Registro com classificacao: campos novos depois de deltaTMs/lat/lon e antes de horaSincronizada.
static void testarComClassificacao() {
  checar(montar(evento(REG_EVENTO, 3, 30.0f, 3200, 9, true, 1, 0)) ==
             esperada("evento", "sensor-04", "30.0", 9, ",\"deltaTMs\":3200",
                      novos("ENCHENDO", "SUBINDO"), true),
         "evento da sonda (sensor-04): estado e tendencia depois de deltaTMs");
  checar(montar(evento(REG_SNAPSHOT, 0, 50.0f, 0, 10, true, 2, 1)) ==
             esperada("snapshot", "jardim", "50.0", 10, "", novos("TRANSBORDANDO", "ESTAVEL"), true),
         "snapshot: estado e tendencia depois de bootId");
  checar(montar(evento(REG_BOOT, 0, 0.0f, 0, 0, false, 0, 3)) ==
             esperada("boot", "jardim", "0.0", 0, ",\"lat\":0.000000,\"lon\":0.000000",
                      novos("NORMAL", "INSUFICIENTE"), false),
         "boot: estado e tendencia depois de lat/lon");
  checar(montar(evento(REG_SNAPSHOT, 0, 65.0f, 0, 11, true, 3, 2)) ==
             esperada("snapshot", "jardim", "65.0", 11, "", novos("ALAGADO", "DESCENDO"), true),
         "ALAGADO/DESCENDO gravados com os nomes oficiais");
  checar(montar(evento(REG_SNAPSHOT, 0, 10.0f, 0, 12, true, 0, 5)) ==
             esperada("snapshot", "jardim", "10.0", 12, "", novos("NORMAL", "DESATUALIZADO"), true),
         "DESATUALIZADO gravado com o nome oficial");
}

static void testarSoUmCampo() {
  checar(montar(evento(REG_SNAPSHOT, 0, 30.0f, 0, 8, true, 1, SEM)) ==
             esperada("snapshot", "jardim", "30.0", 8, "", novos("ENCHENDO", nullptr), true),
         "so estado: grava estado e versaoRegra");
  checar(montar(evento(REG_SNAPSHOT, 0, 30.0f, 0, 8, true, SEM, 0)) ==
             esperada("snapshot", "jardim", "30.0", 8, "", novos(nullptr, "SUBINDO"), true),
         "so tendencia: grava tendenciaDispositivo e versaoRegra");
  // Valor fora da lista nunca vira lixo na linha.
  std::string l = montar(evento(REG_SNAPSHOT, 0, 30.0f, 0, 8, true, 7, 9));
  checar(!contem(l, "estado") && !contem(l, "tendenciaDispositivo") && !contem(l, "versaoRegra"),
         "estado/tendencia fora da lista (7, 9) nao sao gravados");
}

// O que o firmware grava, o gateway devolve igual.
static void testarRoundTrip() {
  const Evento casos[] = {
      evento(REG_EVENTO, 0, 12.0f, 3200, 7, true, SEM, SEM),
      evento(REG_SNAPSHOT, 0, 30.0f, 0, 8, true, SEM, SEM),
      evento(REG_BOOT, 0, 0.0f, 0, 0, false, SEM, SEM),
      evento(REG_EVENTO, 3, 30.0f, 3200, 9, true, 1, 0),
      evento(REG_SNAPSHOT, 0, 50.0f, 0, 70000, true, 2, 1),
      evento(REG_BOOT, 0, 0.0f, 0, 0, false, 0, 3),
      evento(REG_SNAPSHOT, 0, 65.0f, 0, 4294967295u, true, 3, 2),
  };
  int i = 0;
  for (const Evento& e : casos) {
    std::string linha = montar(e), json;
    bool ok = viaGateway(linha, &json);
    checar(ok && json == linha, "round-trip linha -> quadro -> JSON identico (caso " + std::to_string(i++) + ")");
  }
}

// A linha mais longa (boot, todos os campos, seq de 32 bits) nao e truncada.
static void testarMaiorLinhaNaoTrunca() {
  std::string l = montar(evento(REG_BOOT, 0, 100.0f, 0, 4294967295u, false, 3, 4));
  checar(l.size() + 16 < TAMANHO_LINHA, "boot mais longo deixa folga em TAMANHO_LINHA");
  checar(!l.empty() && l.back() == '}' && contem(l, "\"origem\":\"simulacao\"}"),
         "boot mais longo termina inteiro (nada truncado)");
}

static void testarIdSensor() {
  char b[16];
  formatarIdSensor(evento(REG_EVENTO, 3, 1, 0, 1, true, SEM, SEM), b, sizeof(b));
  checar(std::strcmp(b, "sensor-04") == 0, "sonda (indice 3) continua sensor-04");
  formatarIdSensor(evento(REG_SNAPSHOT, 3, 1, 0, 1, true, SEM, SEM), b, sizeof(b));
  checar(std::strcmp(b, "jardim") == 0, "snapshot continua idSensor 'jardim'");
}

int main() {
  testarIdDoJardim();
  testarSemClassificacao();
  testarComClassificacao();
  testarSoUmCampo();
  testarRoundTrip();
  testarMaiorLinhaNaoTrunca();
  testarIdSensor();

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

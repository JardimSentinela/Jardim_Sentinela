// Gateway local (linha NDJSON -> quadro v2 -> JSON) e contador de quadros
// rejeitados por versao. O caso que mais importa: uma linha NDJSON ANTIGA, que
// o firmware anterior deixou pendente no arquivo (sem estado, tendencia nem
// versaoRegra), continua sendo enviada, sem esses campos e sem inventar valor.
//
// O quadro nao e persistido: ele e montado a partir do NDJSON no envio. O que
// fica pendente no disco sao linhas NDJSON.
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include "codec_quadro.h"
#include "config.h"
#include "estado_fisico.cpp"
#include "tendencia_local.cpp"

// ---- stubs do que o firmware real tem em estado.h / Serial ----
struct ContadoresEnvio {
  uint32_t tentativas;
  uint32_t confirmados;
  uint32_t falhas;
  uint32_t dizimados;
  uint32_t perdidos;
  uint32_t rejeitadosVersao;
};
static ContadoresEnvio g_envio = {0, 0, 0, 0, 0, 0};
static char g_ultimoErro[40] = "nenhum";

static std::string g_log;
struct SerialFake {
  void printf(const char* fmt, ...) {
    char b[200];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    g_log += b;
  }
} Serial;

// ---- funcoes reais extraidas de src/modulos/*.cpp ----
#include "iso.inc"
#include "anexar.inc"
#include "gateway.inc"
#include "rejeicao.inc"

static int falhas = 0;

static void checar(bool cond, const std::string& nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome.c_str());
  if (!cond) falhas++;
}

static bool contem(const std::string& texto, const char* trecho) {
  return texto.find(trecho) != std::string::npos;
}

// Monta uma linha exatamente na ordem do montarLinha() do firmware antigo.
// "extras" entra depois do bootId (deltaTMs, lat/lon); "novos" tambem (campos v2).
static std::string linha(const char* tipo, const char* idJardim, const char* idSensor,
                         const char* nivel, unsigned seq, const std::string& extras,
                         const std::string& novos) {
  char b[480];
  std::snprintf(b, sizeof(b),
                "{\"tipo\":\"%s\",\"idJardim\":\"%s\",\"idSensor\":\"%s\",\"nivelAgua\":%s,"
                "\"bateria\":100,\"timestamp\":\"2026-10-21T20:40:35Z\",\"seq\":%u,"
                "\"bootId\":\"A1B2C3D4\"%s%s,\"horaSincronizada\":true,\"origem\":\"simulacao\"}",
                tipo, idJardim, idSensor, nivel, seq, extras.c_str(), novos.c_str());
  return b;
}

static const std::string EXTRA_BOOT = ",\"lat\":0.000000,\"lon\":0.000000";
static const std::string EXTRA_EVENTO = ",\"deltaTMs\":3200";

static std::string camposNovos(const char* estado, const char* tendencia) {
  std::string s;
  if (estado) s += std::string(",\"") + CAMPO_ESTADO + "\":\"" + estado + "\"";
  if (tendencia) s += std::string(",\"") + CAMPO_TENDENCIA + "\":\"" + tendencia + "\"";
  if (estado || tendencia) s += std::string(",\"") + CAMPO_VERSAO_REGRA + "\":\"" + VERSAO_REGRA + "\"";
  return s;
}

// linha NDJSON -> quadro -> bytes -> quadro -> JSON, como no envio.
static bool viaGateway(const std::string& entrada, std::string* saida, codec::Quadro* qOut = nullptr) {
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
  if (qOut) *qOut = d;
  return true;
}

// ===== linha ANTIGA (firmware anterior): enviada sem os campos novos =====
static void testarLinhaAntiga() {
  const struct { const char* nome; std::string texto; } casos[] = {
      {"evento", linha("evento", JARDIM_ID, "sensor-01", "12.0", 7, EXTRA_EVENTO, "")},
      {"snapshot", linha("snapshot", JARDIM_ID, "jardim", "30.0", 8, "", "")},
      {"boot", linha("boot", JARDIM_ID, "jardim", "0.0", 0, EXTRA_BOOT, "")},
  };
  for (const auto& c : casos) {
    std::string json;
    codec::Quadro d{};
    bool ok = viaGateway(c.texto, &json, &d);
    checar(ok, std::string("linha antiga (") + c.nome + ") e aceita e enviada");
    checar(ok && d.regs[0].estado == codec::SEM_CLASSIFICACAO &&
               d.regs[0].tendencia == codec::SEM_CLASSIFICACAO,
           std::string("linha antiga (") + c.nome + ") vira 'sem classificacao' (0xFF), sem inventar valor");
    checar(ok && json == c.texto,
           std::string("linha antiga (") + c.nome + ") sai identica, sem campos novos");
    checar(ok && !contem(json, CAMPO_ESTADO) && !contem(json, CAMPO_TENDENCIA) &&
               !contem(json, CAMPO_VERSAO_REGRA),
           std::string("linha antiga (") + c.nome + ") nao tem estado, tendencia nem versaoRegra");
  }
}

// Linha gravada com o id antigo do jardim: continua enviada, com o id do firmware
// atual (JARDIM_ID). E uma migracao silenciosa, registrada no relatorio.
static void testarLinhaAntigaComIdAntigo() {
  std::string json;
  bool ok = viaGateway(linha("snapshot", "jardim-centro-01", "jardim", "30.0", 8, "", ""), &json);
  checar(ok, "linha antiga com idJardim 'jardim-centro-01' ainda e enviada");
  checar(ok && contem(json, (std::string("\"idJardim\":\"") + JARDIM_ID + "\"").c_str()),
         "idJardim sai com o JARDIM_ID do firmware atual");
  checar(ok && !contem(json, CAMPO_ESTADO), "linha antiga com id antigo continua sem estado");
}

// ===== linha NOVA: estado e tendencia chegam ao JSON =====
static void testarLinhaNova() {
  const struct { const char* nome; std::string texto; } casos[] = {
      {"evento", linha("evento", JARDIM_ID, "sensor-04", "30.0", 9, EXTRA_EVENTO,
                       camposNovos("ENCHENDO", "SUBINDO"))},
      {"snapshot", linha("snapshot", JARDIM_ID, "jardim", "50.0", 10, "",
                         camposNovos("TRANSBORDANDO", "ESTAVEL"))},
      {"boot", linha("boot", JARDIM_ID, "jardim", "0.0", 0, EXTRA_BOOT,
                     camposNovos("NORMAL", "INSUFICIENTE"))},
      {"alagado/descendo", linha("snapshot", JARDIM_ID, "jardim", "65.0", 11, "",
                                 camposNovos("ALAGADO", "DESCENDO"))},
      {"desatualizado", linha("snapshot", JARDIM_ID, "jardim", "10.0", 12, "",
                              camposNovos("NORMAL", "DESATUALIZADO"))},
      {"indeterminado", linha("snapshot", JARDIM_ID, "jardim", "10.0", 13, "",
                              camposNovos("NORMAL", "INDETERMINADO"))},
  };
  for (const auto& c : casos) {
    std::string json;
    bool ok = viaGateway(c.texto, &json);
    checar(ok && json == c.texto,
           std::string("linha nova (") + c.nome + ") faz round-trip identico, com os campos novos");
  }
}

// So um dos dois campos presentes: o outro fica ausente; versaoRegra acompanha.
static void testarSoUmCampo() {
  std::string json;
  bool ok = viaGateway(linha("snapshot", JARDIM_ID, "jardim", "30.0", 8, "", camposNovos("ENCHENDO", nullptr)), &json);
  checar(ok && contem(json, "\"estado\":\"ENCHENDO\"") && !contem(json, CAMPO_TENDENCIA) &&
             contem(json, CAMPO_VERSAO_REGRA),
         "so estado: sai estado e versaoRegra, sem tendenciaDispositivo");
  ok = viaGateway(linha("snapshot", JARDIM_ID, "jardim", "30.0", 8, "", camposNovos(nullptr, "SUBINDO")), &json);
  checar(ok && contem(json, "\"tendenciaDispositivo\":\"SUBINDO\"") && !contem(json, "\"estado\"") &&
             contem(json, CAMPO_VERSAO_REGRA),
         "so tendencia: sai tendenciaDispositivo e versaoRegra, sem estado");
}

// Nome desconhecido e corrupcao: a linha e invalida, nunca vira 'sem classificacao'.
static void testarNomesInvalidos() {
  std::string json;
  checar(!viaGateway(linha("snapshot", JARDIM_ID, "jardim", "30.0", 8, "",
                           ",\"estado\":\"XYZ\""), &json),
         "estado desconhecido invalida a linha (nao vira 0xFF em silencio)");
  checar(!viaGateway(linha("snapshot", JARDIM_ID, "jardim", "30.0", 8, "",
                           ",\"tendenciaDispositivo\":\"XYZ\""), &json),
         "tendencia desconhecida invalida a linha (nao vira 0xFF em silencio)");
  checar(!viaGateway(linha("snapshot", JARDIM_ID, "jardim", "30.0", 8, "",
                           ",\"estado\":\"alagado\""), &json),
         "nomes sao exatos: 'alagado' em minusculas e invalido");
}

// O pior caso de tamanho cabe em TAMANHO_LINHA: nada e truncado nem recusado.
static void testarMaiorLinhaCabe() {
  codec::Quadro q{};
  q.flags = codec::FLAG_SIMULACAO | codec::FLAG_COORDENADAS;
  q.seqBase = 0xFFFF;
  q.bootId = 0xFFFFFFFF;
  q.baseTs = 4102444799u;
  q.n = 1;
  q.regs[0] = {codec::BOOT, codec::SENSOR_JARDIM, 0xFFFF, 4102444799u, 255, 100, 0,
               -1234567890, -1234567890, true, codec::ESTADO_MAX, codec::TENDENCIA_MAX};
  char json[TAMANHO_LINHA];
  bool ok = montarJsonGateway(q, 0, false, json, sizeof(json));
  checar(ok, "boot com todos os campos novos cabe em TAMANHO_LINHA");
  checar(ok && std::strlen(json) + 16 < TAMANHO_LINHA,
         "boot mais longo deixa folga em TAMANHO_LINHA (sem truncar)");
  checar(ok && contem(json, "\"horaSincronizada\":false,\"origem\":\"simulacao\"}"),
         "boot mais longo termina inteiro (nada truncado)");

  q.flags = codec::FLAG_SIMULACAO;
  q.regs[0] = {codec::EVENTO, 30, 0xFFFF, 4102444799u, 255, 100, 0xFFFF, 0, 0, false,
               codec::ESTADO_MAX, codec::TENDENCIA_MAX};
  ok = montarJsonGateway(q, 0, true, json, sizeof(json));
  checar(ok && std::strlen(json) + 16 < TAMANHO_LINHA, "evento mais longo cabe em TAMANHO_LINHA");
}

// ===== quadro rejeitado por versao: contado e registrado =====
static void testarRejeicaoPorVersao() {
  g_envio = {0, 0, 0, 0, 0, 0};
  g_log.clear();
  std::snprintf(g_ultimoErro, sizeof(g_ultimoErro), "nenhum");

  contarRejeicaoQuadro(codec::MotivoRejeicao::VERSAO, 0x01);
  checar(g_envio.rejeitadosVersao == 1, "quadro v1 rejeitado: contador = 1");
  checar(contem(g_ultimoErro, "versao"), "quadro rejeitado por versao aparece em ultimoErro");
  checar(contem(g_log.c_str(), "0x01") && contem(g_log.c_str(), "0x02"),
         "log mostra a versao recebida (0x01) e a esperada (0x02)");

  contarRejeicaoQuadro(codec::MotivoRejeicao::VERSAO, 0x07);
  checar(g_envio.rejeitadosVersao == 2, "cada quadro rejeitado soma 1 (agora 2)");

  contarRejeicaoQuadro(codec::MotivoRejeicao::OUTRO, 0x02);
  contarRejeicaoQuadro(codec::MotivoRejeicao::NENHUM, 0x02);
  checar(g_envio.rejeitadosVersao == 2, "rejeicao por outro motivo nao conta como versao");
}

int main() {
  testarLinhaAntiga();
  testarLinhaAntigaComIdAntigo();
  testarLinhaNova();
  testarSoUmCampo();
  testarNomesInvalidos();
  testarMaiorLinhaCabe();
  testarRejeicaoPorVersao();

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

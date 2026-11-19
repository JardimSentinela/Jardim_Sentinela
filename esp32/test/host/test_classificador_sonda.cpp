// Classificador da sonda HW-02: junta o estado fisico (com histerese) e a
// tendencia local de um jardim. Decisoes (docs/decisoes-tecnicas.md §3, §5.2, §8):
//  - o estado vem da sonda (continua, 0-100 cm); sem leitura da sonda = 0xFF;
//  - a tendencia usa as leituras de SNAPSHOT (intervalo regular), nao cada
//    leitura da sonda: a idade maxima e 3 x o intervalo de snapshot.
// C++ puro: roda no host.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>

#include "classificador_sonda.cpp"
#include "estado_fisico.cpp"
#include "tendencia_local.cpp"

static int falhas = 0;

static void checar(bool cond, const std::string& nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome.c_str());
  if (!cond) falhas++;
}

static const uint32_t SNAPSHOT_MS = 5000;
static const uint32_t IDADE_MAX_MS = 15000;
static const uint8_t SEM = 0xFF;

static ClassificadorSonda novo(uint32_t boot = 1) {
  ClassificadorSonda c;
  iniciarClassificador(c, boot, IDADE_MAX_MS);
  return c;
}

static void testarEstadoSemLeitura() {
  ClassificadorSonda c = novo();
  checar(estadoParaRegistro(c) == SEM, "sem leitura da sonda: estado = 0xFF (nao se inventa NORMAL)");
  checar(tendenciaParaRegistro(c, 0) == (uint8_t)TendenciaLocal::INSUFICIENTE,
         "sem snapshots: tendencia = INSUFICIENTE");
}

static void testarEstadoDaSonda() {
  ClassificadorSonda c = novo();
  classificarLeituraSonda(c, 10.0f);
  checar(estadoParaRegistro(c) == (uint8_t)EstadoFisico::NORMAL, "sonda em 10 cm: NORMAL");
  classificarLeituraSonda(c, 22.0f);
  checar(estadoParaRegistro(c) == (uint8_t)EstadoFisico::ENCHENDO, "sonda em 22 cm: ENCHENDO");
  classificarLeituraSonda(c, 21.0f);
  checar(estadoParaRegistro(c) == (uint8_t)EstadoFisico::ENCHENDO, "sonda em 21 cm: histerese mantem ENCHENDO");
  classificarLeituraSonda(c, 19.9f);
  checar(estadoParaRegistro(c) == (uint8_t)EstadoFisico::NORMAL, "sonda em 19,9 cm: volta a NORMAL");
  classificarLeituraSonda(c, 65.0f);
  checar(estadoParaRegistro(c) == (uint8_t)EstadoFisico::ALAGADO, "sonda em 65 cm: ALAGADO");
  classificarLeituraSonda(c, 58.0f);
  checar(estadoParaRegistro(c) == (uint8_t)EstadoFisico::ALAGADO, "sonda em 58 cm: histerese mantem ALAGADO");
  classificarLeituraSonda(c, 57.9f);
  checar(estadoParaRegistro(c) == (uint8_t)EstadoFisico::TRANSBORDANDO, "sonda em 57,9 cm: TRANSBORDANDO");
}

// Leitura invalida: nao cria estado do nada e nao muda o estado que ja existe.
static void testarLeituraInvalida() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  ClassificadorSonda c = novo();
  classificarLeituraSonda(c, nan);
  classificarLeituraSonda(c, -3.0f);
  checar(estadoParaRegistro(c) == SEM, "leituras invalidas sem estado previo: continua 0xFF");
  classificarLeituraSonda(c, 30.0f);
  classificarLeituraSonda(c, nan);
  checar(estadoParaRegistro(c) == (uint8_t)EstadoFisico::ENCHENDO, "leitura invalida mantem o estado");
}

// A tendencia so enxerga snapshots: leituras da sonda (500 ms) nao a alimentam.
static void testarTendenciaSoDeSnapshots() {
  ClassificadorSonda c = novo();
  for (int i = 0; i < 50; i++) classificarLeituraSonda(c, 10.0f + (float)i);
  checar(tendenciaParaRegistro(c, 1000) == (uint8_t)TendenciaLocal::INSUFICIENTE,
         "50 leituras da sonda sem snapshot: tendencia segue INSUFICIENTE");
}

static void testarTendenciaDosSnapshots() {
  ClassificadorSonda c = novo();
  for (uint32_t i = 0; i < 4; i++) registrarSnapshotSonda(c, i + 1, i * SNAPSHOT_MS, 10.0f + (float)i * 2);
  checar(tendenciaParaRegistro(c, 3 * SNAPSHOT_MS) == (uint8_t)TendenciaLocal::INSUFICIENTE,
         "4 snapshots: INSUFICIENTE");
  registrarSnapshotSonda(c, 5, 4 * SNAPSHOT_MS, 18.0f);
  checar(tendenciaParaRegistro(c, 4 * SNAPSHOT_MS) == (uint8_t)TendenciaLocal::SUBINDO,
         "5 snapshots 10..18 cm: SUBINDO");
  checar(tendenciaParaRegistro(c, 4 * SNAPSHOT_MS + IDADE_MAX_MS) == (uint8_t)TendenciaLocal::SUBINDO,
         "idade igual ao maximo (15 s): ainda SUBINDO");
  checar(tendenciaParaRegistro(c, 4 * SNAPSHOT_MS + IDADE_MAX_MS + 1) ==
             (uint8_t)TendenciaLocal::DESATUALIZADO,
         "idade acima de 3 x snapshot: DESATUALIZADO");

  // Janela desliza: mais 5 snapshots estaveis substituem os de subida.
  for (uint32_t i = 5; i < 10; i++) registrarSnapshotSonda(c, i + 1, i * SNAPSHOT_MS, 18.0f);
  checar(tendenciaParaRegistro(c, 9 * SNAPSHOT_MS) == (uint8_t)TendenciaLocal::ESTAVEL,
         "snapshots estaveis substituem os antigos: ESTAVEL");
}

// seq de 32 bits atravessando 65.535 no historico.
static void testarSeqAtravessaDezesseisBits() {
  ClassificadorSonda c = novo();
  for (uint32_t i = 0; i < 5; i++) registrarSnapshotSonda(c, 65533u + i, i * SNAPSHOT_MS, 10.0f + (float)i * 2);
  checar(tendenciaParaRegistro(c, 4 * SNAPSHOT_MS) == (uint8_t)TendenciaLocal::SUBINDO,
         "seq 65533..65537 no historico: SUBINDO");
}

// O boot do classificador e o boot atual: leituras de outro boot nao contam.
static void testarBootAtual() {
  ClassificadorSonda c = novo(7);
  HistoricoTendencia& h = c.historico;
  for (uint32_t i = 0; i < 5; i++) registrarLeituraTendencia(h, LeituraTendencia{3u, i, i * SNAPSHOT_MS, 10.0f + (float)i * 2});
  checar(tendenciaParaRegistro(c, 4 * SNAPSHOT_MS) == (uint8_t)TendenciaLocal::INSUFICIENTE,
         "leituras do boot 3 nao contam para o boot atual 7");
}

int main() {
  testarEstadoSemLeitura();
  testarEstadoDaSonda();
  testarLeituraInvalida();
  testarTendenciaSoDeSnapshots();
  testarTendenciaDosSnapshots();
  testarSeqAtravessaDezesseisBits();
  testarBootAtual();

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

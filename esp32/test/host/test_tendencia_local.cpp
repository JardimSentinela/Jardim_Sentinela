// Tendencia local (SW-14, parte do firmware): janela de 5 leituras validas,
// tolerancia de 1,5 cm e DESATUALIZADO pelo tMs do dispositivo. O modulo e
// C++ puro, entao o .cpp real e incluido direto. Os casos de direcao e idade
// vem de test/vetores/tendencia.csv, o mesmo arquivo que a API vai ler.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "tendencia_local.cpp"

static int falhas = 0;

static void checar(bool cond, const std::string& nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome.c_str());
  if (!cond) falhas++;
}

static const uint32_t SNAPSHOT_SIM_MS = 5000;
static const uint32_t IDADE_MAX_SIM_MS = 15000;

static bool lerTendencia(const std::string& texto, TendenciaLocal* saida) {
  const TendenciaLocal todas[] = {
      TendenciaLocal::SUBINDO,       TendenciaLocal::ESTAVEL,
      TendenciaLocal::DESCENDO,      TendenciaLocal::INSUFICIENTE,
      TendenciaLocal::INDETERMINADO, TendenciaLocal::DESATUALIZADO};
  for (TendenciaLocal t : todas) {
    if (texto == nomeTendencia(t)) { *saida = t; return true; }
  }
  return false;
}

// O caminho do CSV sai de __FILE__, para o teste rodar de qualquer diretorio.
static std::string caminhoVetores() {
  std::string aqui = __FILE__;
  size_t barra = aqui.find_last_of("/\\");
  std::string pasta = (barra == std::string::npos) ? "." : aqui.substr(0, barra);
  return pasta + "/../vetores/tendencia.csv";
}

// Monta leituras em ordem de chegada: mesmo boot, seq crescente, 5 s entre elas.
static std::vector<LeituraTendencia> montar(const std::vector<float>& niveis) {
  std::vector<LeituraTendencia> v;
  for (size_t i = 0; i < niveis.size(); i++) {
    v.push_back(LeituraTendencia{1u, (uint32_t)i, (uint32_t)(i * SNAPSHOT_SIM_MS), niveis[i]});
  }
  return v;
}

static TendenciaLocal calcular(const std::vector<LeituraTendencia>& v, uint32_t agoraMs) {
  return calcularTendenciaLocal(v.data(), v.size(), agoraMs, IDADE_MAX_SIM_MS);
}

static void testarConstantes() {
  checar(JANELA_TENDENCIA == 5, "janela N = 5 leituras validas");
  checar(TOLERANCIA_TENDENCIA_CM == 1.5f, "tolerancia = 1,5 cm");
  checar(idadeMaximaTendenciaMs(5000) == 15000, "idade maxima: snapshot de 5 s -> 15 s");
  checar(idadeMaximaTendenciaMs(60000) == 180000, "idade maxima: snapshot de 60 s -> 180 s");
}

static void testarNomes() {
  checar(std::strcmp(nomeTendencia(TendenciaLocal::SUBINDO), "SUBINDO") == 0, "nome SUBINDO");
  checar(std::strcmp(nomeTendencia(TendenciaLocal::ESTAVEL), "ESTAVEL") == 0, "nome ESTAVEL");
  checar(std::strcmp(nomeTendencia(TendenciaLocal::DESCENDO), "DESCENDO") == 0, "nome DESCENDO");
  checar(std::strcmp(nomeTendencia(TendenciaLocal::INSUFICIENTE), "INSUFICIENTE") == 0,
         "nome INSUFICIENTE");
  checar(std::strcmp(nomeTendencia(TendenciaLocal::INDETERMINADO), "INDETERMINADO") == 0,
         "nome INDETERMINADO");
  checar(std::strcmp(nomeTendencia(TendenciaLocal::DESATUALIZADO), "DESATUALIZADO") == 0,
         "nome DESATUALIZADO");
}

static void testarVetores() {
  std::ifstream arq(caminhoVetores());
  checar(arq.is_open(), "abre test/vetores/tendencia.csv");
  if (!arq.is_open()) return;

  std::string linha;
  int casos = 0;
  while (std::getline(arq, linha)) {
    if (linha.empty() || linha[0] == '#' || linha.rfind("niveisCm,", 0) == 0) continue;
    size_t v1 = linha.find(',');
    size_t v2 = (v1 == std::string::npos) ? v1 : linha.find(',', v1 + 1);
    if (v2 == std::string::npos) {
      checar(false, "linha mal formada no CSV: " + linha);
      continue;
    }
    std::string campoNiveis = linha.substr(0, v1);
    long idadeSeg = std::strtol(linha.substr(v1 + 1, v2 - v1 - 1).c_str(), nullptr, 10);
    TendenciaLocal esperado;
    if (!lerTendencia(linha.substr(v2 + 1), &esperado)) {
      checar(false, "tendencia desconhecida no CSV: " + linha);
      continue;
    }

    std::vector<float> niveis;
    size_t ini = 0;
    while (!campoNiveis.empty() && ini <= campoNiveis.size()) {
      size_t fim = campoNiveis.find(';', ini);
      if (fim == std::string::npos) fim = campoNiveis.size();
      niveis.push_back(std::strtof(campoNiveis.substr(ini, fim - ini).c_str(), nullptr));
      ini = fim + 1;
    }

    std::vector<LeituraTendencia> leituras = montar(niveis);
    uint32_t tUltima = leituras.empty() ? 0 : leituras.back().tMs;
    uint32_t agora = tUltima + (uint32_t)idadeSeg * 1000u;
    TendenciaLocal obtido = calcular(leituras, agora);
    checar(obtido == esperado, "csv: " + linha + " -> obtido " + nomeTendencia(obtido));
    casos++;
  }
  checar(casos >= 20, "CSV tem pelo menos 20 casos (tem " + std::to_string(casos) + ")");
}

// A ordem vem do seq, nao da posicao no vetor.
static void testarOrdemPorSeq() {
  // Por seq: 10, 11, 12, 13, 14 (SUBINDO). Por posicao seria 14 ... 11 (DESCENDO).
  std::vector<LeituraTendencia> v = {
      {1u, 4u, 20000u, 14.0f}, {1u, 2u, 10000u, 12.0f}, {1u, 0u, 0u, 10.0f},
      {1u, 3u, 15000u, 13.0f}, {1u, 1u, 5000u, 11.0f}};
  checar(calcular(v, 20000u) == TendenciaLocal::SUBINDO,
         "ordem: leituras fora de ordem sao ordenadas por seq");
}

// seq com buracos (registros perdidos) continua ordenando normalmente.
static void testarSeqComBuracos() {
  std::vector<LeituraTendencia> v = {
      {1u, 0u, 0u, 10.0f},   {1u, 3u, 15000u, 11.0f},  {1u, 4u, 20000u, 12.0f},
      {1u, 9u, 45000u, 13.0f}, {1u, 10u, 50000u, 14.0f}};
  checar(calcular(v, 50000u) == TendenciaLocal::SUBINDO, "seq com buracos: ordena e calcula");
}

// Boot novo: seq recomeca e nao se compara com o boot anterior.
static void testarTrocaDeBoot() {
  std::vector<LeituraTendencia> v;
  for (uint32_t i = 0; i < 5; i++) {
    v.push_back(LeituraTendencia{1u, 100u + i, i * SNAPSHOT_SIM_MS, 10.0f + (float)i});
  }
  for (uint32_t i = 0; i < 5; i++) {
    v.push_back(LeituraTendencia{2u, i, i * SNAPSHOT_SIM_MS, 10.0f});
  }
  checar(calcular(v, 20000u) == TendenciaLocal::ESTAVEL,
         "boot novo: so o boot atual entra na janela (boot 1 subia, boot 2 estavel)");

  std::vector<LeituraTendencia> poucas(v.begin(), v.begin() + 5);
  poucas.push_back(LeituraTendencia{2u, 0u, 0u, 10.0f});
  poucas.push_back(LeituraTendencia{2u, 1u, 5000u, 10.0f});
  checar(calcular(poucas, 5000u) == TendenciaLocal::INSUFICIENTE,
         "boot novo: com menos de 5 leituras do boot atual e INSUFICIENTE");
}

static void testarEntradaVazia() {
  checar(calcularTendenciaLocal(nullptr, 0, 0u, IDADE_MAX_SIM_MS) == TendenciaLocal::INSUFICIENTE,
         "entrada vazia (nullptr, n = 0) e INSUFICIENTE");
}

int main() {
  testarConstantes();
  testarNomes();
  testarVetores();
  testarOrdemPorSeq();
  testarSeqComBuracos();
  testarTrocaDeBoot();
  testarEntradaVazia();

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

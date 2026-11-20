// Tendencia local (SW-14, parte do firmware): janela de 5 leituras validas,
// tolerancia de 1,5 cm e DESATUALIZADO pelo tMs do dispositivo. O modulo e
// C++ puro, entao o .cpp real e incluido direto. Os casos de direcao, boot e
// idade vem de test/vetores/tendencia.csv, o mesmo arquivo que a API vai ler.
//
// Regra de boot: o chamador informa o boot atual (bootIdAtual). So as leituras
// dele entram na janela. seq e tMs recomecam a cada boot e NAO sao comparaveis
// entre boots, nem mesmo quando um registro de boot antigo chega depois.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
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

static TendenciaLocal calcular(const std::vector<LeituraTendencia>& v, uint32_t bootAtual,
                               uint32_t agoraMs) {
  return calcularTendenciaLocal(v.data(), v.size(), bootAtual, agoraMs, IDADE_MAX_SIM_MS);
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

// Quebra "a;b;c" em itens. Campo vazio = nenhum item.
static std::vector<std::string> dividir(const std::string& texto, char sep) {
  std::vector<std::string> itens;
  if (texto.empty()) return itens;
  size_t ini = 0;
  while (ini <= texto.size()) {
    size_t fim = texto.find(sep, ini);
    if (fim == std::string::npos) fim = texto.size();
    itens.push_back(texto.substr(ini, fim - ini));
    ini = fim + 1;
  }
  return itens;
}

static void testarVetores() {
  std::ifstream arq(caminhoVetores());
  checar(arq.is_open(), "abre test/vetores/tendencia.csv");
  if (!arq.is_open()) return;

  std::string linha;
  int casos = 0, casosDeBoot = 0;
  while (std::getline(arq, linha)) {
    // Windows com core.autocrlf grava os CSV com CRLF: descarta o \r do fim da linha.
    if (!linha.empty() && linha.back() == '\r') linha.pop_back();
    if (linha.empty() || linha[0] == '#' || linha.rfind("leituras,", 0) == 0) continue;
    size_t v1 = linha.find(',');
    size_t v2 = (v1 == std::string::npos) ? v1 : linha.find(',', v1 + 1);
    size_t v3 = (v2 == std::string::npos) ? v2 : linha.find(',', v2 + 1);
    if (v3 == std::string::npos) {
      checar(false, "linha mal formada no CSV: " + linha);
      continue;
    }
    uint32_t bootAtual = (uint32_t)std::strtoul(linha.substr(v1 + 1, v2 - v1 - 1).c_str(), nullptr, 10);
    long idadeSeg = std::strtol(linha.substr(v2 + 1, v3 - v2 - 1).c_str(), nullptr, 10);
    TendenciaLocal esperado;
    if (!lerTendencia(linha.substr(v3 + 1), &esperado)) {
      checar(false, "tendencia desconhecida no CSV: " + linha);
      continue;
    }

    // Cada boot tem seu proprio contador: seq e tMs recomecam em zero.
    std::vector<LeituraTendencia> leituras;
    std::map<uint32_t, uint32_t> contador;
    std::map<uint32_t, uint32_t> ultimoTMs;
    bool temOutroBoot = false;
    for (const std::string& item : dividir(linha.substr(0, v1), ';')) {
      uint32_t boot = 1;
      std::string nivelTxt = item;
      size_t doisPontos = item.find(':');
      if (doisPontos != std::string::npos) {
        boot = (uint32_t)std::strtoul(item.substr(0, doisPontos).c_str(), nullptr, 10);
        nivelTxt = item.substr(doisPontos + 1);
      }
      if (boot != bootAtual) temOutroBoot = true;
      uint32_t n = contador[boot]++;
      uint32_t tMs = n * SNAPSHOT_SIM_MS;
      ultimoTMs[boot] = tMs;
      leituras.push_back(LeituraTendencia{boot, n, tMs, std::strtof(nivelTxt.c_str(), nullptr)});
    }

    uint32_t agora = ultimoTMs[bootAtual] + (uint32_t)idadeSeg * 1000u;
    TendenciaLocal obtido = calcular(leituras, bootAtual, agora);
    checar(obtido == esperado, "csv: " + linha + " -> obtido " + nomeTendencia(obtido));
    casos++;
    if (temOutroBoot) casosDeBoot++;
  }
  checar(casos >= 20, "CSV tem pelo menos 20 casos (tem " + std::to_string(casos) + ")");
  checar(casosDeBoot >= 6, "CSV tem pelo menos 6 casos com mais de um boot (tem " +
                               std::to_string(casosDeBoot) + ")");
}

// A ordem vem do seq, nao da posicao no vetor.
static void testarOrdemPorSeq() {
  // Por seq: 10, 11, 12, 13, 14 (SUBINDO). Por posicao seria 14 ... 11 (DESCENDO).
  std::vector<LeituraTendencia> v = {
      {1u, 4u, 20000u, 14.0f}, {1u, 2u, 10000u, 12.0f}, {1u, 0u, 0u, 10.0f},
      {1u, 3u, 15000u, 13.0f}, {1u, 1u, 5000u, 11.0f}};
  checar(calcular(v, 1u, 20000u) == TendenciaLocal::SUBINDO,
         "ordem: leituras fora de ordem sao ordenadas por seq");
}

// seq com buracos (registros perdidos) continua ordenando normalmente.
static void testarSeqComBuracos() {
  std::vector<LeituraTendencia> v = {
      {1u, 0u, 0u, 10.0f},     {1u, 3u, 15000u, 11.0f}, {1u, 4u, 20000u, 12.0f},
      {1u, 9u, 45000u, 13.0f}, {1u, 10u, 50000u, 14.0f}};
  checar(calcular(v, 1u, 50000u) == TendenciaLocal::SUBINDO, "seq com buracos: ordena e calcula");
}

// seq do boot antigo MAIOR que o do boot novo: nao pode ganhar so por ser maior.
static void testarSeqDoBootAntigoMaior() {
  std::vector<LeituraTendencia> v;
  for (uint32_t i = 0; i < 5; i++) {
    v.push_back(LeituraTendencia{1u, 100u + i, 4000000u + i * SNAPSHOT_SIM_MS, 10.0f + (float)i});
  }
  for (uint32_t i = 0; i < 5; i++) {
    v.push_back(LeituraTendencia{2u, i, i * SNAPSHOT_SIM_MS, 10.0f});
  }
  checar(calcular(v, 2u, 20000u) == TendenciaLocal::ESTAVEL,
         "boot: seq maior do boot antigo nao vence (boot 1 subia, boot 2 estavel)");
}

// tMs de boots diferentes nao se comparam: o boot antigo tinha muito uptime.
static void testarTMsEntreBoots() {
  std::vector<LeituraTendencia> v;
  for (uint32_t i = 0; i < 5; i++) {
    v.push_back(LeituraTendencia{1u, 100u + i, 4000000u + i * SNAPSHOT_SIM_MS, 10.0f});
  }
  for (uint32_t i = 0; i < 5; i++) {
    v.push_back(LeituraTendencia{2u, i, i * SNAPSHOT_SIM_MS, 10.0f});
  }
  // Boot 2 atualizado (agora = 20 s, ultima leitura em 20 s). Misturar o tMs do
  // boot 1 (4.000.000 ms) faria a idade parecer negativa ou gigante.
  checar(calcular(v, 2u, 20000u) == TendenciaLocal::ESTAVEL,
         "tMs: idade usa so o tMs do boot atual (boot atual atualizado)");
  // Boot 2 velho (agora = 40 s, ultima leitura em 20 s => 20 s > 15 s).
  checar(calcular(v, 2u, 40000u) == TendenciaLocal::DESATUALIZADO,
         "tMs: idade usa so o tMs do boot atual (boot atual desatualizado)");
  // Retardatario do boot antigo chegando por ultimo, com tMs enorme.
  v.push_back(LeituraTendencia{1u, 105u, 4025000u, 10.0f});
  checar(calcular(v, 2u, 20000u) == TendenciaLocal::ESTAVEL,
         "tMs: retardatario do boot antigo nao altera a idade do boot atual");
}

// seq de 32 bits: a ordem nao pode "dar a volta" em 65.535 (uint16_t). Se o seq
// fosse truncado em 16 bits, 65536 viraria 0 e iria para o inicio da janela.
static void testarSeqAtravessaDezesseisBits() {
  // Por seq: 65533..65537 com niveis 10..14 (SUBINDO), entregues embaralhados.
  std::vector<LeituraTendencia> v = {
      {1u, 65536u, 15000u, 13.0f}, {1u, 65533u, 0u, 10.0f},     {1u, 65537u, 20000u, 14.0f},
      {1u, 65535u, 10000u, 12.0f}, {1u, 65534u, 5000u, 11.0f}};
  checar(calcular(v, 1u, 20000u) == TendenciaLocal::SUBINDO,
         "seq 65533..65537 (cruza 65535 -> 65536): ordem correta, SUBINDO");

  // Mesmo cruzamento em queda: niveis 14..10 por seq => DESCENDO.
  std::vector<LeituraTendencia> w = {
      {1u, 65537u, 20000u, 10.0f}, {1u, 65533u, 0u, 14.0f},     {1u, 65536u, 15000u, 11.0f},
      {1u, 65534u, 5000u, 13.0f},  {1u, 65535u, 10000u, 12.0f}};
  checar(calcular(w, 1u, 20000u) == TendenciaLocal::DESCENDO,
         "seq cruzando 65535 -> 65536 em queda: DESCENDO");

  // Janela deslizando por cima do limite: 6 leituras, so as 5 ultimas contam.
  std::vector<LeituraTendencia> j;
  for (uint32_t i = 0; i < 6; i++) j.push_back(LeituraTendencia{1u, 65532u + i, i * 5000u, i < 1 ? 50.0f : 10.0f});
  checar(calcular(j, 1u, 25000u) == TendenciaLocal::ESTAVEL,
         "janela desliza por cima de 65535: a primeira (seq 65532) sai da janela");

  // Limite superior de 32 bits.
  std::vector<LeituraTendencia> u = {
      {1u, 0xFFFFFFFBu, 0u, 10.0f},     {1u, 0xFFFFFFFCu, 5000u, 11.0f}, {1u, 0xFFFFFFFDu, 10000u, 12.0f},
      {1u, 0xFFFFFFFEu, 15000u, 13.0f}, {1u, 0xFFFFFFFFu, 20000u, 14.0f}};
  checar(calcular(u, 1u, 20000u) == TendenciaLocal::SUBINDO, "seq proximo de 2^32 ordena corretamente");
}

// Historico de tamanho fixo: guarda so as JANELA_TENDENCIA leituras mais recentes.
static void testarHistorico() {
  HistoricoTendencia h = {};
  checar(h.n == 0, "historico novo esta vazio");
  checar(tendenciaDoHistorico(h, 1u, 0u, IDADE_MAX_SIM_MS) == TendenciaLocal::INSUFICIENTE,
         "historico vazio: INSUFICIENTE");

  for (uint32_t i = 0; i < 3; i++) {
    registrarLeituraTendencia(h, LeituraTendencia{1u, i, i * SNAPSHOT_SIM_MS, 10.0f + (float)i * 2});
  }
  checar(h.n == 3, "3 registros: n = 3");
  checar(tendenciaDoHistorico(h, 1u, 10000u, IDADE_MAX_SIM_MS) == TendenciaLocal::INSUFICIENTE,
         "3 leituras no historico: INSUFICIENTE");

  for (uint32_t i = 3; i < 7; i++) {
    registrarLeituraTendencia(h, LeituraTendencia{1u, i, i * SNAPSHOT_SIM_MS, 10.0f + (float)i * 2});
  }
  checar(h.n == JANELA_TENDENCIA, "depois de 7 registros o historico guarda so 5");
  checar(h.itens[0].seq == 2 && h.itens[JANELA_TENDENCIA - 1].seq == 6,
         "os 5 guardados sao os mais recentes (seq 2..6), do mais antigo ao mais novo");
  checar(tendenciaDoHistorico(h, 1u, 6 * SNAPSHOT_SIM_MS, IDADE_MAX_SIM_MS) == TendenciaLocal::SUBINDO,
         "historico com niveis 14..22: SUBINDO");
  checar(tendenciaDoHistorico(h, 1u, 6 * SNAPSHOT_SIM_MS + 16000u, IDADE_MAX_SIM_MS) ==
             TendenciaLocal::DESATUALIZADO,
         "historico velho (16 s sem leitura): DESATUALIZADO");
  checar(tendenciaDoHistorico(h, 2u, 6 * SNAPSHOT_SIM_MS, IDADE_MAX_SIM_MS) == TendenciaLocal::INSUFICIENTE,
         "outro boot atual: o historico do boot 1 nao conta");

  // Mesmo resultado que o calculo direto sobre as 5 ultimas.
  std::vector<LeituraTendencia> v(h.itens, h.itens + h.n);
  checar(calcular(v, 1u, 6 * SNAPSHOT_SIM_MS) ==
             tendenciaDoHistorico(h, 1u, 6 * SNAPSHOT_SIM_MS, IDADE_MAX_SIM_MS),
         "historico e calculo direto concordam");

  // seq cruzando 65.535 dentro do historico.
  HistoricoTendencia g = {};
  for (uint32_t i = 0; i < 7; i++) {
    registrarLeituraTendencia(g, LeituraTendencia{1u, 65532u + i, i * SNAPSHOT_SIM_MS, 30.0f - (float)i * 2});
  }
  checar(g.itens[0].seq == 65534u && g.itens[4].seq == 65538u,
         "historico atravessa 65535 -> 65536 sem perder a ordem");
  checar(tendenciaDoHistorico(g, 1u, 6 * SNAPSHOT_SIM_MS, IDADE_MAX_SIM_MS) == TendenciaLocal::DESCENDO,
         "historico em queda cruzando 65535: DESCENDO");
}

static void testarEntradaVazia() {
  checar(calcularTendenciaLocal(nullptr, 0, 1u, 0u, IDADE_MAX_SIM_MS) ==
             TendenciaLocal::INSUFICIENTE,
         "entrada vazia (nullptr, n = 0) e INSUFICIENTE");
}

int main() {
  testarConstantes();
  testarNomes();
  testarVetores();
  testarOrdemPorSeq();
  testarSeqComBuracos();
  testarSeqDoBootAntigoMaior();
  testarTMsEntreBoots();
  testarSeqAtravessaDezesseisBits();
  testarHistorico();
  testarEntradaVazia();

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

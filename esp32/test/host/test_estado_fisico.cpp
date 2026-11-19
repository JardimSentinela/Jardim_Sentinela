// Estado fisico do jardim (SW-13): faixas da §3 e histerese de 2 cm da §3.1
// de decisoes-tecnicas. O modulo e C++ puro (sem Arduino), entao o .cpp real
// e incluido direto. Os casos de borda vem de test/vetores/estado.csv, o mesmo
// arquivo que a API vai ler (Tarefa B).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

#include "estado_fisico.cpp"

static int falhas = 0;

static void checar(bool cond, const std::string& nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome.c_str());
  if (!cond) falhas++;
}

static bool lerEstado(const std::string& texto, EstadoFisico* saida) {
  const EstadoFisico todos[] = {EstadoFisico::NORMAL, EstadoFisico::ENCHENDO,
                                EstadoFisico::TRANSBORDANDO, EstadoFisico::ALAGADO};
  for (EstadoFisico e : todos) {
    if (texto == nomeEstado(e)) { *saida = e; return true; }
  }
  return false;
}

// O caminho do CSV sai de __FILE__, para o teste rodar de qualquer diretorio.
static std::string caminhoVetores() {
  std::string aqui = __FILE__;
  size_t barra = aqui.find_last_of("/\\");
  std::string pasta = (barra == std::string::npos) ? "." : aqui.substr(0, barra);
  return pasta + "/../vetores/estado.csv";
}

static void testarConstantes() {
  checar(LIMITE_ENCHENDO_CM == 22.0f, "limite ENCHENDO = 22 cm");
  checar(LIMITE_TRANSBORDANDO_CM == 45.0f, "limite TRANSBORDANDO = 45 cm");
  checar(LIMITE_ALAGADO_CM == 60.0f, "limite ALAGADO = 60 cm");
  checar(FOLGA_HISTERESE_CM == 2.0f, "folga de histerese = 2 cm");
  checar(std::strcmp(VERSAO_REGRA, "v1-provisoria-2026-11") == 0,
         "versaoRegra = v1-provisoria-2026-11");
}

static void testarNomes() {
  checar(std::strcmp(nomeEstado(EstadoFisico::NORMAL), "NORMAL") == 0, "nome NORMAL");
  checar(std::strcmp(nomeEstado(EstadoFisico::ENCHENDO), "ENCHENDO") == 0, "nome ENCHENDO");
  checar(std::strcmp(nomeEstado(EstadoFisico::TRANSBORDANDO), "TRANSBORDANDO") == 0,
         "nome TRANSBORDANDO");
  checar(std::strcmp(nomeEstado(EstadoFisico::ALAGADO), "ALAGADO") == 0, "nome ALAGADO");
}

static void testarVetores() {
  std::ifstream arq(caminhoVetores());
  checar(arq.is_open(), "abre test/vetores/estado.csv");
  if (!arq.is_open()) return;

  std::string linha;
  int casos = 0;
  while (std::getline(arq, linha)) {
    if (linha.empty() || linha[0] == '#' || linha.rfind("anterior,", 0) == 0) continue;
    size_t v1 = linha.find(',');
    size_t v2 = linha.find(',', v1 + 1);
    if (v1 == std::string::npos || v2 == std::string::npos) {
      checar(false, "linha mal formada no CSV: " + linha);
      continue;
    }
    EstadoFisico anterior, esperado;
    bool ok = lerEstado(linha.substr(0, v1), &anterior) &&
              lerEstado(linha.substr(v2 + 1), &esperado);
    float nivel = std::strtof(linha.substr(v1 + 1, v2 - v1 - 1).c_str(), nullptr);
    if (!ok) { checar(false, "estado desconhecido no CSV: " + linha); continue; }

    EstadoFisico obtido = classificarEstado(nivel, anterior);
    checar(obtido == esperado, "csv: " + linha + " -> obtido " + nomeEstado(obtido));
    casos++;
  }
  checar(casos >= 40, "CSV tem pelo menos 40 casos (tem " + std::to_string(casos) + ")");
}

// Nivel oscilando na borda de 22 cm nao pode alternar o estado.
static void testarOscilacaoNaBorda() {
  EstadoFisico e = EstadoFisico::NORMAL;
  e = classificarEstado(21.9f, e);
  checar(e == EstadoFisico::NORMAL, "oscilacao: 21,9 segue NORMAL");
  e = classificarEstado(22.1f, e);
  checar(e == EstadoFisico::ENCHENDO, "oscilacao: 22,1 sobe para ENCHENDO");
  const float serie[] = {21.9f, 22.1f, 21.5f, 22.0f, 20.5f, 21.9f};
  bool estavel = true;
  for (float n : serie) estavel = estavel && (e = classificarEstado(n, e)) == EstadoFisico::ENCHENDO;
  checar(estavel, "oscilacao: entre 20 e 22,1 cm o estado fica em ENCHENDO");
  e = classificarEstado(19.9f, e);
  checar(e == EstadoFisico::NORMAL, "oscilacao: 19,9 volta para NORMAL");
}

// Subida completa e descida completa, passo a passo.
static void testarCicloCompleto() {
  EstadoFisico e = EstadoFisico::NORMAL;
  const float subida[] = {10, 22, 30, 45, 50, 60, 65};
  const EstadoFisico esperadoSubida[] = {
      EstadoFisico::NORMAL,        EstadoFisico::ENCHENDO, EstadoFisico::ENCHENDO,
      EstadoFisico::TRANSBORDANDO, EstadoFisico::TRANSBORDANDO, EstadoFisico::ALAGADO,
      EstadoFisico::ALAGADO};
  bool ok = true;
  for (int i = 0; i < 7; i++) ok = ok && (e = classificarEstado(subida[i], e)) == esperadoSubida[i];
  checar(ok, "ciclo: subida 10 -> 65 cm passa pelos 4 estados");

  const float descida[] = {59, 58, 57, 44, 43, 42, 21, 20, 19};
  const EstadoFisico esperadoDescida[] = {
      EstadoFisico::ALAGADO,       EstadoFisico::ALAGADO,  EstadoFisico::TRANSBORDANDO,
      EstadoFisico::TRANSBORDANDO, EstadoFisico::TRANSBORDANDO, EstadoFisico::ENCHENDO,
      EstadoFisico::ENCHENDO,      EstadoFisico::ENCHENDO, EstadoFisico::NORMAL};
  ok = true;
  for (int i = 0; i < 9; i++) ok = ok && (e = classificarEstado(descida[i], e)) == esperadoDescida[i];
  checar(ok, "ciclo: descida 59 -> 19 cm respeita a folga de 2 cm");
}

// Leitura invalida nao muda o estado (decisao do modulo, nao da §3).
static void testarEntradaInvalida() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  const EstadoFisico todos[] = {EstadoFisico::NORMAL, EstadoFisico::ENCHENDO,
                                EstadoFisico::TRANSBORDANDO, EstadoFisico::ALAGADO};
  bool ok = true;
  for (EstadoFisico anterior : todos) {
    ok = ok && classificarEstado(nan, anterior) == anterior;
    ok = ok && classificarEstado(inf, anterior) == anterior;
    ok = ok && classificarEstado(-inf, anterior) == anterior;
    ok = ok && classificarEstado(-0.5f, anterior) == anterior;
  }
  checar(ok, "entrada invalida (NaN, infinito, negativa) mantem o estado anterior");
}

int main() {
  testarConstantes();
  testarNomes();
  testarVetores();
  testarOscilacaoNaBorda();
  testarCicloCompleto();
  testarEntradaInvalida();

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

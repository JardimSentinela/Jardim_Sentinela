// Sonda HW-02: conversao ADC -> cm, taxa assinada e separacao pronta/mudou.
// Compila o hw02_sonda.cpp real contra stubs de Arduino (sem placa).
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "Arduino.h"

unsigned long g_arduinoMillis = 0;
int g_arduinoAdc = 0;
int g_arduinoLeituras = 0;
int g_pinModeChamadas = 0;
int g_resolucao = 0;

void pinMode(uint8_t, uint8_t) { g_pinModeChamadas++; }
void analogReadResolution(int bits) { g_resolucao = bits; }
int analogRead(uint8_t) {
  g_arduinoLeituras++;
  return g_arduinoAdc;
}
unsigned long millis() { return g_arduinoMillis; }

#include "sonda.inc"

static int falhas = 0;

static void checar(bool cond, const char* nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome);
  if (!cond) falhas++;
}

static bool quase(float a, float b, float tol) {
  return std::fabs(a - b) <= tol;
}

static void resetar() {
  ultimoNivelAmostrado = 0.0f;
  ultimoNivelNotificado = 0.0f;
  ultimoTempoAmostrado = 0;
  ultimaAmostragem = 0;
  primeiraLeitura = true;
  g_arduinoMillis = 0;
  g_arduinoAdc = 0;
  g_arduinoLeituras = 0;
  g_pinModeChamadas = 0;
  g_resolucao = 0;
}

int main() {
  LeituraSonda l;

  std::printf("== 1. inicializacao ==\n");
  resetar();
  iniciarSonda();
  checar(g_pinModeChamadas == 1, "iniciarSonda configura o pino");
  checar(g_resolucao == 12, "iniciarSonda usa resolucao de 12 bits");

  std::printf("== 2. janela de 500 ms ==\n");
  resetar();
  g_arduinoMillis = 499;
  checar(!atualizarSonda(l), "antes de 500 ms retorna false");
  checar(g_arduinoLeituras == 0, "antes de 500 ms nao le o ADC");

  g_arduinoMillis = 500;
  g_arduinoAdc = 4095;
  checar(atualizarSonda(l), "em 500 ms retorna true");
  checar(l.pronta, "pronta=true");
  checar(l.mudou, "primeira leitura conta como mudanca");
  checar(quase(l.nivelCm, 100.0f, 0.01f), "nivel = adc/4095*100");
  checar(quase(l.taxaCmMin, 0.0f, 0.01f), "taxa inicial = 0");
  checar(g_arduinoLeituras == 10, "media de 10 leituras");

  std::printf("== 3. pronta separada de mudou ==\n");
  resetar();
  g_arduinoMillis = 500;
  g_arduinoAdc = 0;
  atualizarSonda(l);  // primeira leitura: 0 cm
  g_arduinoMillis = 1000;
  g_arduinoAdc = 20;  // ~0,49 cm
  checar(atualizarSonda(l), "amostra de 500 ms fica pronta");
  checar(l.pronta, "pronta=true mesmo sem mudanca");
  checar(!l.mudou, "variacao < 1 cm nao muda");
  checar(quase(l.nivelCm, 0.4884f, 0.01f), "nivel continua disponivel");

  g_arduinoMillis = 1500;
  g_arduinoAdc = 41;  // ~1,001 cm
  checar(atualizarSonda(l), "amostra fica pronta");
  checar(l.mudou, "variacao >= 1 cm muda");

  std::printf("== 4. taxa assinada ==\n");
  resetar();
  g_arduinoMillis = 500;
  g_arduinoAdc = 4095;  // 100 cm
  atualizarSonda(l);
  g_arduinoMillis = 1500;
  g_arduinoAdc = 2048;  // ~50,01 cm em 1 s
  atualizarSonda(l);
  checar(l.taxaCmMin < 0.0f, "descida -> taxa negativa");
  checar(quase(l.taxaCmMin, -2999.27f, 2.0f), "taxa ~ -3000 cm/min");

  std::printf("== 5. regressao: 10 min parado e sobe 2 cm ==\n");
  resetar();
  g_arduinoMillis = 500;
  g_arduinoAdc = 0;
  atualizarSonda(l);  // primeira leitura: 0 cm
  g_arduinoMillis = 500 + 600000;  // 10 min depois
  g_arduinoAdc = 0;
  atualizarSonda(l);
  checar(quase(l.taxaCmMin, 0.0f, 0.01f), "10 min parado -> taxa 0");
  g_arduinoMillis += 500;
  g_arduinoAdc = 82;  // ~2,002 cm em 500 ms
  atualizarSonda(l);
  checar(l.mudou, "subida de 2 cm muda");
  checar(quase(l.taxaCmMin, 240.29f, 2.0f),
         "taxa usa a ultima amostra (~240 cm/min)");
  checar(l.taxaCmMin > 100.0f,
         "nao usa a janela de 10 min (~0,2 cm/min)");

  std::printf("== 6. limitacao conhecida: sem saturacao ==\n");
  resetar();
  g_arduinoMillis = 500;
  g_arduinoAdc = 5000;  // acima de 4095
  atualizarSonda(l);
  checar(quase(l.nivelCm, 122.1f, 0.1f),
         "ADC acima de 4095 nao satura (calibracao ainda nominal)");

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

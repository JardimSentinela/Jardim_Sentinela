#include "hw02_sonda.h"
#include <math.h>

constexpr uint8_t PINO_SONDA = 35;
constexpr float NIVEL_MAXIMO_CM = 100.0f;
constexpr float VARIACAO_MINIMA_CM = 1.0f;
constexpr unsigned long INTERVALO_MS = 500;

static float ultimoNivelAmostrado = 0.0f;
static float ultimoNivelNotificado = 0.0f;
static unsigned long ultimoTempoAmostrado = 0;
static unsigned long ultimaAmostragem = 0;
static bool primeiraLeitura = true;

void iniciarSonda() {
    pinMode(PINO_SONDA, INPUT);
    analogReadResolution(12);
}

bool atualizarSonda(LeituraSonda& leitura) {
    leitura.pronta = false;
    leitura.mudou = false;

    unsigned long agora = millis();

    if (agora - ultimaAmostragem < INTERVALO_MS) {
        return false;
    }

    ultimaAmostragem = agora;

    unsigned long soma = 0;
    for (int i = 0; i < 10; i++) {
        soma += analogRead(PINO_SONDA);
    }

    int adc = soma / 10;
    float nivel = (adc / 4095.0f) * NIVEL_MAXIMO_CM;
    agora = millis();

    if (primeiraLeitura) {
        ultimoNivelAmostrado = nivel;
        ultimoNivelNotificado = nivel;
        ultimoTempoAmostrado = agora;
        primeiraLeitura = false;

        leitura = {adc, nivel, 0.0f, true, true};
        return true;
    }

    unsigned long intervalo = agora - ultimoTempoAmostrado;
    float variacao = nivel - ultimoNivelAmostrado;
    float taxa = intervalo > 0
        ? variacao * 60000.0f / intervalo
        : 0.0f;

    ultimoNivelAmostrado = nivel;
    ultimoTempoAmostrado = agora;

    bool mudou =
        fabsf(nivel - ultimoNivelNotificado) >= VARIACAO_MINIMA_CM;

    if (mudou) {
        ultimoNivelNotificado = nivel;
    }

    leitura = {adc, nivel, taxa, true, mudou};
    return true;
}
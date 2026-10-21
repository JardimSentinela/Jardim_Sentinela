
#include <Arduino.h>
#include <math.h>

// HW-02 - Emulacao da sonda de nivel QDY30A-VD

constexpr uint8_t PINO_SONDA = 35;
constexpr float NIVEL_MAXIMO_CM = 100.0f;
constexpr float VARIACAO_MINIMA_CM = 1.0f;
constexpr unsigned long INTERVALO_MS = 500;

struct LeituraSonda {
    int adc;
    float nivelCm;
    float taxaCmMin;
};

static float ultimoNivel = 0.0f;
static unsigned long ultimoTempo = 0;
static unsigned long ultimaAmostragem = 0;
static bool primeiraLeitura = true;

void iniciarSonda() {
    pinMode(PINO_SONDA, INPUT);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
}

bool lerSonda(LeituraSonda& leitura) {
    unsigned long agora = millis();

    if (agora - ultimaAmostragem < INTERVALO_MS) {
        return false;
    }
    ultimaAmostragem = agora;

    // Media de 10 leituras para reduzir ruido.
    unsigned long soma = 0;
    for (int i = 0; i < 10; i++) {
        soma += analogRead(PINO_SONDA);
        delay(2);
    }

    int adc = soma / 10;
    float nivel = (adc / 4095.0f) * NIVEL_MAXIMO_CM;

    if (primeiraLeitura) {
        leitura = {adc, nivel, 0.0f};
        ultimoNivel = nivel;
        ultimoTempo = millis();
        primeiraLeitura = false;
        return true;
    }

    float variacao = nivel - ultimoNivel;

    // Ignora pequenas variacoes causadas por ruido.
    if (fabsf(variacao) < VARIACAO_MINIMA_CM) {
        return false;
    }

    agora = millis();
    float minutos = (agora - ultimoTempo) / 60000.0f;

    if (minutos <= 0.0f) {
        return false;
    }

    leitura = {adc, nivel, variacao / minutos};
    ultimoNivel = nivel;
    ultimoTempo = agora;

    return true;
}

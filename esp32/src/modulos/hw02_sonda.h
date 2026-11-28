#ifndef HW02_SONDA_H
#define HW02_SONDA_H

#include <Arduino.h>

struct LeituraSonda {
    int adc;
    float nivelCm;
    float taxaCmMin;
    bool pronta;
    bool mudou;
};

void iniciarSonda();
bool atualizarSonda(LeituraSonda& leitura);

#endif
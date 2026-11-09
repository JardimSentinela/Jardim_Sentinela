#pragma once
// Stub minimo de Arduino para os testes de host da sonda HW-02.
// As funcoes sao definidas no test_sonda.cpp; aqui ficam so as declaracoes.

#include <stdint.h>
#include <stddef.h>

#define INPUT 0x01
#define OUTPUT 0x03
#define HIGH 0x1
#define LOW 0x0

extern unsigned long g_arduinoMillis;
extern int g_arduinoAdc;
extern int g_arduinoLeituras;

void pinMode(uint8_t pino, uint8_t modo);
void analogReadResolution(int bits);
int analogRead(uint8_t pino);
unsigned long millis();

#ifndef LATENCIA_H
#define LATENCIA_H

#include <Arduino.h>

#include "codec_quadro.h"

// INC 8B: anel das ultimas confirmacoes do boot corrente.
constexpr uint8_t LATENCIAS_MAX = 128;

void zerarLatencia();
void registrarLatencia(uint32_t segundos);
void registrarLatenciaDoRegistro(const codec::Registro& r, bool horaSinc, const char* boot);
uint32_t percentilLatencia(uint8_t p);
uint8_t latenciaAmostras();
uint32_t latenciaDesconhecidas();
uint32_t latenciaForaBoot();

#endif

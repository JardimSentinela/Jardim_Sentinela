#ifndef RESOLVIDOS_H
#define RESOLVIDOS_H

#include <Arduino.h>

#include "estado.h"

int indiceResolvido(const char* boot, uint32_t seq);
void removerResolvido(int idx);
void marcarResolvido(const char* boot, uint32_t seq);

#endif

#ifndef LINHA_H
#define LINHA_H

#include <Arduino.h>

#include "estado.h"

const char* nomeTipo(TipoRegistro tipo);
void formatarIdSensor(const Evento& e, char* buffer, size_t tamanho);
void anexar(char* buffer, size_t tamanho, size_t& usado, const char* texto);
void montarLinha(const Evento& e, const char* ts, const char* idSensor,
                 char* buffer, size_t tamanho);

#endif

#ifndef ARMAZENAMENTO_H
#define ARMAZENAMENTO_H

#include <Arduino.h>
#include <LittleFS.h>

#include "estado.h"

void persistir(const Evento& e);
void despejarFila();
void limparFila();
void injetarLinhaTruncada();
void forcarCompactacao();
uint32_t contarLinhasNoPrefixo(File& f, size_t ate);

#endif

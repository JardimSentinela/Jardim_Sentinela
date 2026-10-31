#ifndef RELOGIO_H
#define RELOGIO_H

#include <Arduino.h>
#include <time.h>

bool relogioValido();
void iniciarSincronizacaoNtp();
bool parsearDataHttp(const char* texto, time_t* epoch);
void atualizarHora();
void formatarTimestamp(uint32_t ms, bool sincronizado, char* buffer, size_t tamanho);
bool parsearIso(const char* texto, uint32_t* epoch);
void formatarIso(uint32_t epoch, char* buffer, size_t tamanho);

#endif

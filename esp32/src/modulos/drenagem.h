#ifndef DRENAGEM_H
#define DRENAGEM_H

#include <Arduino.h>

#include "config.h"

// Lote de ate 6 linhas lidas do arquivo. O envio monta um unico quadro.
struct LoteLinhas {
  char linha[6][TAMANHO_LINHA + 16];
  char boot[6][9];
  uint32_t seq[6];
  int n;
};

void tentarEnviarLote();
void alternarOffline();
void alternarUrlInvalida();
void alternarRespostaForcada();

#endif

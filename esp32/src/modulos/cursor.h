#ifndef CURSOR_H
#define CURSOR_H

#include <Arduino.h>
#include <LittleFS.h>

#include "estado.h"

void carregarCursor();
bool lerLinha(File& f, char* buffer, size_t tamanho);
bool extrairChave(const char* linha, char* bootId, uint32_t* seq);
uint32_t contarPendentes();
File abrirAposCursor(bool* ok);
bool avancarCursorResolvido();

#endif

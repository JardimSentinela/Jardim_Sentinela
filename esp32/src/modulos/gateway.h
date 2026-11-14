#ifndef GATEWAY_H
#define GATEWAY_H

#include <Arduino.h>

#include "codec_quadro.h"

bool campoTxt(const char* linha, const char* chave, char* out, size_t tam);
uint8_t tipoDaLinha(const char* linha);
bool montarQuadroDaLinha(const char* linha, codec::Quadro* q, bool* horaSinc);
bool montarJsonGateway(const codec::Quadro& q, int idx, bool horaSinc,
                       char* out, size_t tam);
void rodarTesteCodec();

#endif

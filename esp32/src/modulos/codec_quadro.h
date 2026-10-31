#pragma once
#include <stdint.h>
#include <stddef.h>

// Codec do "Quadro no ar v1" (little-endian), compartilhado pelo firmware e
// pelo gateway local da simulacao. Nao depende de Arduino (testavel no host).
namespace codec {

constexpr uint8_t  VERSAO          = 0x01;
constexpr size_t   CABECALHO       = 14;
constexpr uint8_t  MAX_REGISTROS   = 6;
constexpr size_t   MAX_BYTES       = 56;   // 6 registros sem gap/absoluto/coords
constexpr size_t   MAX_COM_EXTRAS  = 134;  // pior caso: 6 x (gap+abs+coords)
constexpr uint8_t  SENSOR_JARDIM   = 31;
constexpr uint16_t DTS_ABSOLUTO    = 0xFFFF;
constexpr uint8_t  FLAG_SIMULACAO  = 0x01; // bit0: origem simulada
constexpr uint8_t  FLAG_COORDENADAS = 0x02; // bit1: lat/lon presentes no boot

enum Tipo : uint8_t { EVENTO = 0, SNAPSHOT = 1, BOOT = 2 };

// Um registro ja quantizado para o quadro. "ts" e o epoch absoluto; o encoder
// decide entre dTs curto e u32 absoluto. "coords" so vale para BOOT.
struct Registro {
  uint8_t  tipo;
  uint8_t  sensor;     // 0..30; 31 = jardim
  uint32_t seq;
  uint32_t ts;
  uint8_t  nivelCm;
  uint8_t  bateria;
  uint16_t deltaT;     // unidades de 0,1 s; 0 = ausente
  int32_t  lat;        // 1e-7 graus
  int32_t  lon;
  bool     coords;
};

struct Quadro {
  uint8_t  flags;
  uint8_t  jardimIdx;
  uint16_t seqBase;
  uint32_t bootId;
  uint32_t baseTs;
  uint8_t  n;
  Registro regs[MAX_REGISTROS];
};

// Helpers de escrita/leitura little-endian (o ESP32 e o host x86 sao LE, mas
// o formato e definido byte a byte para nao depender da plataforma).
inline void put16(uint8_t* b, size_t& p, uint16_t v) {
  b[p++] = (uint8_t)(v & 0xFF);
  b[p++] = (uint8_t)(v >> 8);
}
inline void put32(uint8_t* b, size_t& p, uint32_t v) {
  b[p++] = (uint8_t)(v & 0xFF);
  b[p++] = (uint8_t)((v >> 8) & 0xFF);
  b[p++] = (uint8_t)((v >> 16) & 0xFF);
  b[p++] = (uint8_t)((v >> 24) & 0xFF);
}
inline uint16_t get16(const uint8_t* b, size_t& p) {
  uint16_t v = (uint16_t)b[p] | ((uint16_t)b[p + 1] << 8);
  p += 2;
  return v;
}
inline uint32_t get32(const uint8_t* b, size_t& p) {
  uint32_t v = (uint32_t)b[p] | ((uint32_t)b[p + 1] << 8) |
               ((uint32_t)b[p + 2] << 16) | ((uint32_t)b[p + 3] << 24);
  p += 4;
  return v;
}
inline bool cabe(size_t p, size_t k, size_t tam) { return p + k <= tam; }

// Valida e devolve o tamanho exato do quadro, ou 0 se for invalido.
inline size_t tamanho(const Quadro& q) {
  if (q.n < 1 || q.n > MAX_REGISTROS) return 0;
  if (q.flags & ~(FLAG_SIMULACAO | FLAG_COORDENADAS)) return 0;
  size_t t = CABECALHO;
  uint32_t anterior = q.seqBase;
  for (uint8_t i = 0; i < q.n; i++) {
    const Registro& r = q.regs[i];
    if (r.tipo > BOOT || r.sensor > SENSOR_JARDIM || r.bateria > 100) return 0;
    if (i == 0) {
      if (r.seq != q.seqBase) return 0;   // seqBase e o seq do primeiro registro
    } else {
      if (r.seq <= anterior) return 0;
      uint32_t gap = r.seq - anterior - 1;
      if (gap > 255) return 0;
      if (gap > 0) t += 1;
    }
    int64_t d = (int64_t)r.ts - (int64_t)q.baseTs;
    if (d < 0 || d > (int64_t)(DTS_ABSOLUTO - 1)) t += 4;
    if ((q.flags & FLAG_COORDENADAS) && r.tipo == BOOT) t += 8;
    t += 7;
    anterior = r.seq;
  }
  return t;
}

// Codifica o quadro em "out"; devolve o tamanho escrito ou 0 se nao couber.
inline size_t codificar(const Quadro& q, uint8_t* out, size_t capacidade) {
  size_t total = tamanho(q);
  if (total == 0 || total > capacidade) return 0;

  size_t p = 0;
  out[p++] = VERSAO;
  out[p++] = q.flags;
  out[p++] = q.jardimIdx;
  out[p++] = q.n;
  put16(out, p, q.seqBase);
  put32(out, p, q.bootId);
  put32(out, p, q.baseTs);

  uint32_t anterior = q.seqBase;
  for (uint8_t i = 0; i < q.n; i++) {
    const Registro& r = q.regs[i];
    uint32_t gap = (i == 0) ? 0 : (r.seq - anterior - 1);
    uint8_t meta = (uint8_t)((r.tipo & 0x03) | ((r.sensor & 0x1F) << 2));
    if (gap > 0) meta |= 0x80;
    out[p++] = meta;
    if (gap > 0) out[p++] = (uint8_t)gap;

    int64_t d = (int64_t)r.ts - (int64_t)q.baseTs;
    if (d < 0 || d > (int64_t)(DTS_ABSOLUTO - 1)) {
      put16(out, p, DTS_ABSOLUTO);
      put32(out, p, r.ts);
    } else {
      put16(out, p, (uint16_t)d);
    }
    out[p++] = r.nivelCm;
    out[p++] = r.bateria;
    put16(out, p, r.deltaT);

    if ((q.flags & FLAG_COORDENADAS) && r.tipo == BOOT) {
      put32(out, p, (uint32_t)r.lat);
      put32(out, p, (uint32_t)r.lon);
    }
    anterior = r.seq;
  }
  return p;   // == total
}

// Decodifica um quadro completo. Rejeita versao, n, tipo, bateria e tamanho
// invalidos; exige que o buffer termine exatamente no fim do ultimo registro.
inline bool decodificar(const uint8_t* in, size_t tam, Quadro* q) {
  if (!in || !q || tam < CABECALHO) return false;
  if (in[0] != VERSAO) return false;
  q->flags = in[1];
  if (q->flags & ~(FLAG_SIMULACAO | FLAG_COORDENADAS)) return false;
  q->jardimIdx = in[2];
  q->n = in[3];
  if (q->n < 1 || q->n > MAX_REGISTROS) return false;
  if (tam < CABECALHO + (size_t)q->n * 7) return false;

  size_t p = 4;
  q->seqBase = get16(in, p);
  q->bootId = get32(in, p);
  q->baseTs = get32(in, p);

  uint32_t anterior = q->seqBase;
  for (uint8_t i = 0; i < q->n; i++) {
    if (!cabe(p, 1, tam)) return false;
    uint8_t meta = in[p++];
    if (i == 0 && (meta & 0x80)) return false;  // gap nao faz sentido no primeiro
    uint8_t gap = 0;
    if (meta & 0x80) {
      if (!cabe(p, 1, tam)) return false;
      gap = in[p++];
    }
    Registro& r = q->regs[i];
    r.tipo = meta & 0x03;
    r.sensor = (meta >> 2) & 0x1F;
    if (r.tipo > BOOT) return false;

    if (i == 0) {
      r.seq = q->seqBase;
    } else {
      uint32_t s = anterior + gap + 1;
      if (s <= anterior) return false;
      r.seq = s;
    }

    if (!cabe(p, 2, tam)) return false;
    uint16_t dts = get16(in, p);
    if (dts == DTS_ABSOLUTO) {
      if (!cabe(p, 4, tam)) return false;
      r.ts = get32(in, p);
    } else {
      r.ts = q->baseTs + dts;
    }
    if (!cabe(p, 3, tam)) return false;
    r.nivelCm = in[p++];
    r.bateria = in[p++];
    if (r.bateria > 100) return false;
    r.deltaT = get16(in, p);

    r.coords = (q->flags & FLAG_COORDENADAS) && r.tipo == BOOT;
    if (r.coords) {
      if (!cabe(p, 8, tam)) return false;
      r.lat = (int32_t)get32(in, p);
      r.lon = (int32_t)get32(in, p);
    } else {
      r.lat = 0;
      r.lon = 0;
    }
    anterior = r.seq;
  }
  return p == tam;
}

}  // namespace codec

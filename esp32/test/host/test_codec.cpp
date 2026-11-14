// Round-trip do codec v1 para evento, snapshot e boot.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "codec_quadro.h"

static int falhas = 0;

static void checar(bool cond, const char* nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome);
  if (!cond) falhas++;
}

static bool mesmoRegistro(const codec::Registro& a, const codec::Registro& b) {
  return a.tipo == b.tipo && a.sensor == b.sensor && a.seq == b.seq &&
         a.ts == b.ts && a.nivelCm == b.nivelCm && a.bateria == b.bateria &&
         a.deltaT == b.deltaT && a.lat == b.lat && a.lon == b.lon &&
         a.coords == b.coords;
}

int main() {
  codec::Quadro q{};
  q.flags = codec::FLAG_SIMULACAO | codec::FLAG_COORDENADAS;
  q.jardimIdx = 0;
  q.seqBase = 0;
  q.bootId = 0xA1B2C3D4;
  q.baseTs = 1790000000;
  q.n = 3;
  // tipo, sensor, seq, ts, nivel, bateria, deltaT, lat, lon, coords
  q.regs[0] = {codec::BOOT, codec::SENSOR_JARDIM, 0, 1790000000,
               0, 88, 0, -123456789, 987654321, true};
  q.regs[1] = {codec::EVENTO, 3, 2, 1790000010,
               42, 87, 100, 0, 0, false};
  q.regs[2] = {codec::SNAPSHOT, 7, 3, 1790100000,
               12, 86, 50, 0, 0, false};

  uint8_t buf[codec::MAX_COM_EXTRAS];
  size_t tam = codec::codificar(q, buf, sizeof(buf));
  checar(tam > 0, "codifica quadro com os 3 tipos");
  checar(tam == codec::tamanho(q), "tamanho declarado == bytes escritos");

  codec::Quadro d{};
  checar(codec::decodificar(buf, tam, &d), "decodifica o quadro");
  checar(d.flags == q.flags, "flags preservadas");
  checar(d.jardimIdx == q.jardimIdx, "jardimIdx preservado");
  checar(d.seqBase == q.seqBase, "seqBase preservado");
  checar(d.bootId == q.bootId, "bootId preservado");
  checar(d.baseTs == q.baseTs, "baseTs preservado");
  checar(d.n == q.n, "n preservado");
  for (uint8_t i = 0; i < q.n; i++) {
    char nome[64];
    std::snprintf(nome, sizeof(nome), "registro %u identico campo a campo", i);
    checar(mesmoRegistro(d.regs[i], q.regs[i]), nome);
  }
  checar(d.regs[1].seq == 2, "gap de seq reconstruido");
  checar(d.regs[2].ts == q.regs[2].ts, "timestamp absoluto reconstruido");
  checar(d.regs[0].lat == q.regs[0].lat && d.regs[0].lon == q.regs[0].lon,
         "coordenadas do boot preservadas");

  // Seis registros sequenciais, sem extras, devem caber no pior caso normal.
  codec::Quadro s{};
  s.flags = codec::FLAG_SIMULACAO;
  s.jardimIdx = 1;
  s.seqBase = 10;
  s.bootId = 0x01020304;
  s.baseTs = 1800000000;
  s.n = codec::MAX_REGISTROS;
  for (uint8_t i = 0; i < s.n; i++) {
    s.regs[i] = {codec::SNAPSHOT, 1, (uint32_t)(10 + i),
                 (uint32_t)(1800000000 + i), (uint8_t)i, 50, 10, 0, 0, false};
  }
  uint8_t buf6[codec::MAX_COM_EXTRAS];
  size_t tam6 = codec::codificar(s, buf6, sizeof(buf6));
  codec::Quadro d6{};
  checar(tam6 == codec::tamanho(s) && tam6 <= codec::MAX_BYTES,
         "6 registros cabem no tamanho maximo");
  checar(codec::decodificar(buf6, tam6, &d6) && d6.n == 6 &&
             d6.regs[5].seq == 15,
         "6 registros decodificam");

  // Flags sem coordenadas: boot nao carrega lat/lon.
  codec::Quadro nc = q;
  nc.flags = codec::FLAG_SIMULACAO;
  uint8_t bufnc[codec::MAX_COM_EXTRAS];
  size_t tamnc = codec::codificar(nc, bufnc, sizeof(bufnc));
  codec::Quadro dnc{};
  checar(tamnc > 0 && codec::decodificar(bufnc, tamnc, &dnc),
         "quadro sem coordenadas decodifica");
  checar(!dnc.regs[0].coords && dnc.regs[0].lat == 0 && dnc.regs[0].lon == 0,
         "sem FLAG_COORDENADAS nao le lat/lon");

  // Rejeicoes.
  uint8_t ruim[codec::MAX_COM_EXTRAS];
  std::memcpy(ruim, buf, tam);
  ruim[0] = 0x02;
  checar(!codec::decodificar(ruim, tam, &d), "rejeita versao invalida");
  std::memcpy(ruim, buf, tam);
  ruim[3] = 0;
  checar(!codec::decodificar(ruim, tam, &d), "rejeita n=0");
  std::memcpy(ruim, buf, tam);
  ruim[1] = 0x80;
  checar(!codec::decodificar(ruim, tam, &d), "rejeita flags invalidas");
  checar(!codec::decodificar(buf, tam - 1, &d), "rejeita quadro truncado");

  codec::Quadro inv = q;
  inv.regs[0].bateria = 101;
  checar(codec::tamanho(inv) == 0, "rejeita bateria > 100");
  inv = q;
  inv.n = 0;
  checar(codec::tamanho(inv) == 0, "rejeita n=0 no tamanho");

  uint8_t pequeno[codec::MAX_COM_EXTRAS];
  checar(codec::codificar(q, pequeno, tam - 1) == 0,
         "rejeita buffer menor que o quadro");

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

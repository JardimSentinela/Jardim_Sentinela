// Round-trip do codec v2 (quadro v1 + estado e tendencia) para evento,
// snapshot e boot. v2 acrescenta 2 bytes por registro: estado fisico e
// tendencia, com 0xFF = "sem classificacao" (registro antigo, sem os campos).
// versaoRegra NAO viaja no quadro: e constante (VERSAO_REGRA, estado_fisico.h).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "codec_quadro.h"
#include "estado_fisico.h"
#include "tendencia_local.h"

static int falhas = 0;

static void checar(bool cond, const char* nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome);
  if (!cond) falhas++;
}

static bool mesmoRegistro(const codec::Registro& a, const codec::Registro& b) {
  return a.tipo == b.tipo && a.sensor == b.sensor && a.seq == b.seq &&
         a.ts == b.ts && a.nivelCm == b.nivelCm && a.bateria == b.bateria &&
         a.deltaT == b.deltaT && a.lat == b.lat && a.lon == b.lon &&
         a.coords == b.coords && a.estado == b.estado && a.tendencia == b.tendencia;
}

// Quadro minimo: 1 evento, sem gap, sem ts absoluto, sem coordenadas.
// Layout v2: cabecalho (14) + meta(1) dTs(2) nivel(1) bateria(1) deltaT(2)
// estado(1) tendencia(1) = 23 bytes; estado em [21] e tendencia em [22].
static codec::Quadro quadroMinimo(uint8_t estado, uint8_t tendencia) {
  codec::Quadro u{};
  u.flags = codec::FLAG_SIMULACAO;
  u.seqBase = 5;
  u.bootId = 0x0A0B0C0D;
  u.baseTs = 1790000000;
  u.n = 1;
  u.regs[0] = {codec::EVENTO, 1, 5, 1790000000, 30, 100, 0, 0, 0, false, estado, tendencia};
  return u;
}

static void testarConstantes() {
  checar(codec::VERSAO == 0x02, "versao do quadro = 0x02");
  checar(codec::SEM_CLASSIFICACAO == 0xFF, "sem classificacao = 0xFF");
  checar(codec::MAX_BYTES == 68, "MAX_BYTES = 14 + 6 x 9 = 68");
  checar(codec::MAX_COM_EXTRAS == 146, "MAX_COM_EXTRAS = 68 + 6 x 13 = 146");
  // O valor no fio e o ordinal dos enums: o codec nao os inclui, entao o teste confere.
  checar(codec::ESTADO_MAX == (uint8_t)EstadoFisico::ALAGADO, "ESTADO_MAX = ultimo EstadoFisico");
  checar(codec::TENDENCIA_MAX == (uint8_t)TendenciaLocal::DESATUALIZADO,
         "TENDENCIA_MAX = ultima TendenciaLocal");
}

// Registro que nao informa estado/tendencia (linha antiga) nasce "sem classificacao".
static void testarRegistroAntigo() {
  codec::Registro r = {codec::EVENTO, 1, 5, 1790000000, 30, 100, 0, 0, 0, false};
  checar(r.estado == codec::SEM_CLASSIFICACAO, "registro sem estado nasce com 0xFF");
  checar(r.tendencia == codec::SEM_CLASSIFICACAO, "registro sem tendencia nasce com 0xFF");

  codec::Quadro u = quadroMinimo(codec::SEM_CLASSIFICACAO, codec::SEM_CLASSIFICACAO);
  uint8_t buf[codec::MAX_COM_EXTRAS];
  size_t tam = codec::codificar(u, buf, sizeof(buf));
  codec::Quadro d{};
  checar(tam == 23 && codec::decodificar(buf, tam, &d), "quadro com 0xFF/0xFF codifica e decodifica");
  checar(d.regs[0].estado == codec::SEM_CLASSIFICACAO &&
             d.regs[0].tendencia == codec::SEM_CLASSIFICACAO,
         "0xFF/0xFF volta como 'sem classificacao', sem inventar valor");
}

static void testarLayoutDosBytesNovos() {
  codec::Quadro u = quadroMinimo(2, 5);
  uint8_t buf[codec::MAX_COM_EXTRAS];
  size_t tam = codec::codificar(u, buf, sizeof(buf));
  checar(tam == 23 && tam == codec::tamanho(u), "quadro minimo v2 tem 23 bytes (v1 tinha 21)");
  checar(buf[0] == 0x02, "primeiro byte do quadro e a versao 0x02");
  checar(buf[21] == 2 && buf[22] == 5, "estado em [21] e tendencia em [22]");
}

static void testarValoresInvalidos() {
  codec::Quadro u = quadroMinimo(0, 0);
  uint8_t buf[codec::MAX_COM_EXTRAS];
  size_t tam = codec::codificar(u, buf, sizeof(buf));
  codec::Quadro d{};
  checar(tam == 23, "quadro base para corromper");

  const uint8_t ruins[] = {codec::ESTADO_MAX + 1, 0x7F, 0xFE};
  bool ok = true;
  for (uint8_t v : ruins) {
    uint8_t c[codec::MAX_COM_EXTRAS];
    std::memcpy(c, buf, tam);
    c[21] = v;
    ok = ok && !codec::decodificar(c, tam, &d);
  }
  checar(ok, "decodificador rejeita estado fora de 0..3 e diferente de 0xFF");

  ok = true;
  for (uint8_t v : {(uint8_t)(codec::TENDENCIA_MAX + 1), (uint8_t)0x7F, (uint8_t)0xFE}) {
    uint8_t c[codec::MAX_COM_EXTRAS];
    std::memcpy(c, buf, tam);
    c[22] = v;
    ok = ok && !codec::decodificar(c, tam, &d);
  }
  checar(ok, "decodificador rejeita tendencia fora de 0..5 e diferente de 0xFF");

  checar(codec::tamanho(quadroMinimo(codec::ESTADO_MAX + 1, 0)) == 0,
         "codificador rejeita estado invalido");
  checar(codec::tamanho(quadroMinimo(0, codec::TENDENCIA_MAX + 1)) == 0,
         "codificador rejeita tendencia invalida");
  checar(codec::tamanho(quadroMinimo(codec::ESTADO_MAX, codec::TENDENCIA_MAX)) == 23,
         "extremos validos (3 e 5) sao aceitos");
}

int main() {
  testarConstantes();
  testarRegistroAntigo();
  testarLayoutDosBytesNovos();
  testarValoresInvalidos();

  codec::Quadro q{};
  q.flags = codec::FLAG_SIMULACAO | codec::FLAG_COORDENADAS;
  q.jardimIdx = 0;
  q.seqBase = 0;
  q.bootId = 0xA1B2C3D4;
  q.baseTs = 1790000000;
  q.n = 3;
  // tipo, sensor, seq, ts, nivel, bateria, deltaT, lat, lon, coords, estado, tendencia
  q.regs[0] = {codec::BOOT, codec::SENSOR_JARDIM, 0, 1790000000,
               0, 88, 0, -123456789, 987654321, true, 0, 3};
  q.regs[1] = {codec::EVENTO, 3, 2, 1790000010,
               42, 87, 100, 0, 0, false, 2, 0};
  q.regs[2] = {codec::SNAPSHOT, 7, 3, 1790100000,
               12, 86, 50, 0, 0, false, 3, 2};

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
  checar(d.regs[0].estado == 0 && d.regs[0].tendencia == 3, "estado e tendencia do boot");
  checar(d.regs[1].estado == 2 && d.regs[1].tendencia == 0, "estado e tendencia do evento");
  checar(d.regs[2].estado == 3 && d.regs[2].tendencia == 2, "estado e tendencia do snapshot");
  checar(d.regs[1].seq == 2, "gap de seq reconstruido");
  checar(d.regs[2].ts == q.regs[2].ts, "timestamp absoluto reconstruido");
  checar(d.regs[0].lat == q.regs[0].lat && d.regs[0].lon == q.regs[0].lon,
         "coordenadas do boot preservadas");

  // Seis registros sequenciais, sem extras, devem caber no pior caso normal.
  // Cada registro leva um par (estado, tendencia) diferente, incluindo 0xFF.
  codec::Quadro s{};
  s.flags = codec::FLAG_SIMULACAO;
  s.jardimIdx = 1;
  s.seqBase = 10;
  s.bootId = 0x01020304;
  s.baseTs = 1800000000;
  s.n = codec::MAX_REGISTROS;
  const uint8_t estados[6] = {0, 1, 2, 3, codec::SEM_CLASSIFICACAO, 0};
  const uint8_t tendencias[6] = {0, 1, 2, 3, 4, codec::SEM_CLASSIFICACAO};
  for (uint8_t i = 0; i < s.n; i++) {
    s.regs[i] = {codec::SNAPSHOT, 1, (uint32_t)(10 + i),
                 (uint32_t)(1800000000 + i), (uint8_t)i, 50, 10, 0, 0, false,
                 estados[i], tendencias[i]};
  }
  uint8_t buf6[codec::MAX_COM_EXTRAS];
  size_t tam6 = codec::codificar(s, buf6, sizeof(buf6));
  codec::Quadro d6{};
  checar(tam6 == codec::tamanho(s) && tam6 == codec::MAX_BYTES,
         "6 registros ocupam exatamente MAX_BYTES (68)");
  checar(codec::decodificar(buf6, tam6, &d6) && d6.n == 6 &&
             d6.regs[5].seq == 15,
         "6 registros decodificam");
  bool pares = true;
  for (uint8_t i = 0; i < 6; i++) pares = pares && mesmoRegistro(d6.regs[i], s.regs[i]);
  checar(pares, "os 6 pares (estado, tendencia) voltam identicos");

  // Pior caso real: 6 boots, todos com ts absoluto e coordenadas, e gap em 5
  // (o primeiro nao tem gap): 14 + 6x9 + 5 + 6x4 + 6x8 = 145 <= MAX_COM_EXTRAS (146).
  codec::Quadro w{};
  w.flags = codec::FLAG_SIMULACAO | codec::FLAG_COORDENADAS;
  w.seqBase = 0;
  w.bootId = 0x11223344;
  w.baseTs = 1790000000;
  w.n = codec::MAX_REGISTROS;
  for (uint8_t i = 0; i < w.n; i++) {
    w.regs[i] = {codec::BOOT, codec::SENSOR_JARDIM, (uint32_t)(i * 10),
                 (uint32_t)(1791000000 + i), 1, 90, 0, 10, 20, true, 1, 1};
  }
  uint8_t bufw[codec::MAX_COM_EXTRAS];
  size_t tamw = codec::codificar(w, bufw, sizeof(bufw));
  checar(tamw == 145 && tamw <= codec::MAX_COM_EXTRAS,
         "pior caso (gap + ts absoluto + coords) = 145 bytes, cabe em MAX_COM_EXTRAS");

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

  // Rejeicoes. A versao 0x03 nao existe; 0x02 e a versao atual.
  uint8_t ruim[codec::MAX_COM_EXTRAS];
  std::memcpy(ruim, buf, tam);
  ruim[0] = 0x03;
  checar(!codec::decodificar(ruim, tam, &d), "rejeita versao invalida");
  std::memcpy(ruim, buf, tam);
  ruim[3] = 0;
  checar(!codec::decodificar(ruim, tam, &d), "rejeita n=0");
  std::memcpy(ruim, buf, tam);
  ruim[1] = 0x80;
  checar(!codec::decodificar(ruim, tam, &d), "rejeita flags invalidas");
  checar(!codec::decodificar(buf, tam - 1, &d), "rejeita quadro truncado");

  // Motivo da rejeicao: so a versao conta para o contador de quadros rejeitados.
  codec::MotivoRejeicao motivo = codec::MotivoRejeicao::NENHUM;
  checar(codec::decodificar(buf, tam, &d, &motivo) && motivo == codec::MotivoRejeicao::NENHUM,
         "quadro valido: motivo NENHUM");
  for (uint8_t v : {(uint8_t)0x00, (uint8_t)0x01, (uint8_t)0x03, (uint8_t)0xFF}) {
    std::memcpy(ruim, buf, tam);
    ruim[0] = v;
    motivo = codec::MotivoRejeicao::NENHUM;
    char nome[64];
    std::snprintf(nome, sizeof(nome), "versao 0x%02X rejeitada com motivo VERSAO", v);
    checar(!codec::decodificar(ruim, tam, &d, &motivo) && motivo == codec::MotivoRejeicao::VERSAO, nome);
  }
  std::memcpy(ruim, buf, tam);
  ruim[3] = 0;
  motivo = codec::MotivoRejeicao::NENHUM;
  checar(!codec::decodificar(ruim, tam, &d, &motivo) && motivo == codec::MotivoRejeicao::OUTRO,
         "n=0 rejeitado com motivo OUTRO (nao e versao)");
  motivo = codec::MotivoRejeicao::NENHUM;
  checar(!codec::decodificar(buf, tam - 1, &d, &motivo) && motivo == codec::MotivoRejeicao::OUTRO,
         "quadro truncado rejeitado com motivo OUTRO");
  checar(!codec::decodificar(nullptr, 0, &d, &motivo) && motivo == codec::MotivoRejeicao::OUTRO,
         "ponteiro nulo rejeitado com motivo OUTRO");

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

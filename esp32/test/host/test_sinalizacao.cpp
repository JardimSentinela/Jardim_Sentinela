// Sinalizacao do totem (decisoes-tecnicas §4): qual LED acende e quando o
// buzzer soa. A logica e C++ puro e recebe "agoraMs" como parametro; o codigo
// Arduino (commit 10) so chama essas funcoes. Padrao sonoro e piscar: PROVISORIO.
#include <cstdint>
#include <cstdio>
#include <string>

#include "sinalizacao.cpp"

static int falhas = 0;

static void checar(bool cond, const std::string& nome) {
  std::printf("%s %s\n", cond ? "OK  " : "FALHOU", nome.c_str());
  if (!cond) falhas++;
}

static const EstadoFisico ESTADOS[] = {EstadoFisico::NORMAL, EstadoFisico::ENCHENDO,
                                       EstadoFisico::TRANSBORDANDO, EstadoFisico::ALAGADO};
static const TendenciaLocal TENDENCIAS[] = {
    TendenciaLocal::SUBINDO,       TendenciaLocal::ESTAVEL,       TendenciaLocal::DESCENDO,
    TendenciaLocal::INSUFICIENTE,  TendenciaLocal::INDETERMINADO, TendenciaLocal::DESATUALIZADO};

static std::string nome(EstadoFisico e, TendenciaLocal t) {
  return std::to_string((int)e) + "/" + std::to_string((int)t);
}

static void testarConstantes() {
  checar(SOM_PULSOS_POR_RAJADA == 3, "rajada de 3 pulsos curtos");
  checar(SOM_PULSO_MS == 150, "pulso de 150 ms");
  checar(SOM_PAUSA_PULSOS_MS == 150, "pausa de 150 ms entre pulsos");
  checar(SOM_CICLO_MS == 2000, "ciclo da rajada = 2000 ms (intervalo fixo)");
  checar(PISCA_PERIODO_MS == 1000, "periodo do piscar = 1000 ms");
  // A rajada (3 pulsos + 2 pausas = 750 ms) precisa caber no ciclo, com silencio no fim.
  checar(SOM_PULSOS_POR_RAJADA * SOM_PULSO_MS + (SOM_PULSOS_POR_RAJADA - 1) * SOM_PAUSA_PULSOS_MS <
             SOM_CICLO_MS,
         "a rajada cabe no ciclo e sobra silencio");
}

// ===== politica: quem soa =====
static void testarPoliticaDoSom() {
  for (EstadoFisico e : ESTADOS) {
    for (TendenciaLocal t : TENDENCIAS) {
      // Regra provisoria (2026-11-19, a confirmar pelo grupo): TRANSBORDANDO tambem
      // soa enquanto a tendencia for INSUFICIENTE (ex.: logo apos um reinicio).
      bool esperado = (e == EstadoFisico::ALAGADO) ||
                      (e == EstadoFisico::TRANSBORDANDO &&
                       (t == TendenciaLocal::SUBINDO || t == TendenciaLocal::INSUFICIENTE));
      checar(somPermitido(e, t) == esperado, "politica do som " + nome(e, t));
    }
  }
  // Pontos que mais importam, nomeados.
  checar(somPermitido(EstadoFisico::ALAGADO, TendenciaLocal::DESCENDO),
         "ALAGADO soa sempre, ate descendo");
  checar(somPermitido(EstadoFisico::ALAGADO, TendenciaLocal::DESATUALIZADO),
         "ALAGADO soa mesmo com leitura desatualizada");
  checar(!somPermitido(EstadoFisico::TRANSBORDANDO, TendenciaLocal::ESTAVEL),
         "TRANSBORDANDO estavel nao soa");
  checar(somPermitido(EstadoFisico::TRANSBORDANDO, TendenciaLocal::INSUFICIENTE),
         "TRANSBORDANDO com tendencia INSUFICIENTE soa (regra provisoria)");
  checar(!somPermitido(EstadoFisico::TRANSBORDANDO, TendenciaLocal::DESATUALIZADO),
         "TRANSBORDANDO com tendencia DESATUALIZADO continua calado");
  checar(!somPermitido(EstadoFisico::TRANSBORDANDO, TendenciaLocal::INDETERMINADO),
         "TRANSBORDANDO com tendencia INDETERMINADO continua calado");
  checar(!somPermitido(EstadoFisico::TRANSBORDANDO, TendenciaLocal::DESCENDO),
         "TRANSBORDANDO descendo continua calado");
  checar(!somPermitido(EstadoFisico::ENCHENDO, TendenciaLocal::INSUFICIENTE),
         "ENCHENDO com tendencia INSUFICIENTE nao soa");
  checar(!somPermitido(EstadoFisico::NORMAL, TendenciaLocal::INSUFICIENTE),
         "NORMAL com tendencia INSUFICIENTE nao soa");
  checar(!somPermitido(EstadoFisico::ENCHENDO, TendenciaLocal::SUBINDO),
         "ENCHENDO subindo nao soa");
  checar(!somPermitido(EstadoFisico::NORMAL, TendenciaLocal::SUBINDO), "NORMAL nao soa");
}

// ===== LEDs =====
static void testarLeds() {
  const uint32_t instantes[] = {0, 100, 499, 500, 999, 1000, 1500, 123456};
  for (TendenciaLocal t : TENDENCIAS) {
    bool verde = true, amarelo = true, transb = true;
    for (uint32_t ms : instantes) {
      SaidaSinalizacao n = calcularSinalizacao(EstadoFisico::NORMAL, t, ms);
      verde = verde && n.ledVerde && !n.ledAmarelo && !n.ledVermelho;
      SaidaSinalizacao en = calcularSinalizacao(EstadoFisico::ENCHENDO, t, ms);
      amarelo = amarelo && !en.ledVerde && en.ledAmarelo && !en.ledVermelho;
      SaidaSinalizacao tr = calcularSinalizacao(EstadoFisico::TRANSBORDANDO, t, ms);
      transb = transb && !tr.ledVerde && !tr.ledAmarelo && tr.ledVermelho;
    }
    checar(verde, "NORMAL: so o verde, fixo (tendencia " + std::to_string((int)t) + ")");
    checar(amarelo, "ENCHENDO: so o amarelo, fixo (tendencia " + std::to_string((int)t) + ")");
    checar(transb, "TRANSBORDANDO: so o vermelho, fixo (tendencia " + std::to_string((int)t) + ")");
  }
}

// ALAGADO: vermelho piscando; verde e amarelo sempre apagados.
static void testarAlagadoPisca() {
  bool soVermelho = true;
  int acesos = 0, apagados = 0;
  for (uint32_t ms = 0; ms < 4 * PISCA_PERIODO_MS; ms += 10) {
    SaidaSinalizacao s = calcularSinalizacao(EstadoFisico::ALAGADO, TendenciaLocal::ESTAVEL, ms);
    soVermelho = soVermelho && !s.ledVerde && !s.ledAmarelo;
    (s.ledVermelho ? acesos : apagados)++;
  }
  checar(soVermelho, "ALAGADO: verde e amarelo sempre apagados");
  checar(acesos == apagados && acesos > 0, "ALAGADO: vermelho aceso metade do tempo, apagado a outra metade");
  checar(calcularSinalizacao(EstadoFisico::ALAGADO, TendenciaLocal::ESTAVEL, 0).ledVermelho,
         "ALAGADO: vermelho aceso no inicio do periodo");
  checar(calcularSinalizacao(EstadoFisico::ALAGADO, TendenciaLocal::ESTAVEL, PISCA_PERIODO_MS / 2 - 1)
             .ledVermelho,
         "ALAGADO: vermelho ainda aceso no fim da primeira metade");
  checar(!calcularSinalizacao(EstadoFisico::ALAGADO, TendenciaLocal::ESTAVEL, PISCA_PERIODO_MS / 2)
              .ledVermelho,
         "ALAGADO: vermelho apagado no inicio da segunda metade");
  checar(calcularSinalizacao(EstadoFisico::ALAGADO, TendenciaLocal::ESTAVEL, PISCA_PERIODO_MS)
             .ledVermelho,
         "ALAGADO: o piscar se repete a cada periodo");
}

// Nunca dois LEDs de estados diferentes ao mesmo tempo.
static void testarLedsExclusivos() {
  bool ok = true;
  for (EstadoFisico e : ESTADOS) {
    for (TendenciaLocal t : TENDENCIAS) {
      for (uint32_t ms = 0; ms < 2 * SOM_CICLO_MS; ms += 10) {
        SaidaSinalizacao s = calcularSinalizacao(e, t, ms);
        int acesos = (s.ledVerde ? 1 : 0) + (s.ledAmarelo ? 1 : 0) + (s.ledVermelho ? 1 : 0);
        ok = ok && acesos <= 1;
      }
    }
  }
  checar(ok, "no maximo um LED aceso por vez, em todas as combinacoes");
}

// ===== buzzer: padrao de tres pulsos curtos, em intervalo fixo =====
static void testarBuzzerCalado() {
  bool calado = true;
  for (EstadoFisico e : ESTADOS) {
    for (TendenciaLocal t : TENDENCIAS) {
      if (somPermitido(e, t)) continue;
      for (uint32_t ms = 0; ms < 2 * SOM_CICLO_MS; ms += 10) {
        calado = calado && !calcularSinalizacao(e, t, ms).buzzer;
      }
    }
  }
  checar(calado, "sem permissao do som, o buzzer nunca liga (nenhum instante)");
}

static void testarBuzzerPadrao() {
  const EstadoFisico e = EstadoFisico::ALAGADO;
  const TendenciaLocal t = TendenciaLocal::ESTAVEL;

  // Janelas esperadas dentro do ciclo: [0,150) [300,450) [600,750).
  bool janelas = true;
  for (uint32_t ms = 0; ms < SOM_CICLO_MS; ms++) {
    bool dentro = false;
    for (uint32_t i = 0; i < SOM_PULSOS_POR_RAJADA; i++) {
      uint32_t ini = i * (SOM_PULSO_MS + SOM_PAUSA_PULSOS_MS);
      if (ms >= ini && ms < ini + SOM_PULSO_MS) dentro = true;
    }
    janelas = janelas && (calcularSinalizacao(e, t, ms).buzzer == dentro);
  }
  checar(janelas, "buzzer liga so nas janelas dos 3 pulsos [0,150) [300,450) [600,750)");

  // Contagem de pulsos e tempo ligado em um ciclo.
  int subidas = 0;
  uint32_t ligado = 0, maiorSequencia = 0, sequencia = 0;
  bool anterior = calcularSinalizacao(e, t, SOM_CICLO_MS - 1).buzzer;
  for (uint32_t ms = 0; ms < SOM_CICLO_MS; ms++) {
    bool agora = calcularSinalizacao(e, t, ms).buzzer;
    if (agora && !anterior) subidas++;
    if (agora) { ligado++; sequencia++; if (sequencia > maiorSequencia) maiorSequencia = sequencia; }
    else sequencia = 0;
    anterior = agora;
  }
  checar(subidas == 3, "tres pulsos por ciclo");
  checar(ligado == 3 * SOM_PULSO_MS, "ligado 450 ms por ciclo");
  // Distinto de uma sirene oficial: nunca um tom continuo.
  checar(maiorSequencia == SOM_PULSO_MS, "nenhum tom continuo: cada pulso dura so 150 ms");
  checar(SOM_CICLO_MS - (3 * SOM_PULSO_MS + 2 * SOM_PAUSA_PULSOS_MS) >= 1000,
         "pelo menos 1 s de silencio entre rajadas");

  // O intervalo e fixo: o padrao se repete identico a cada ciclo.
  bool periodico = true;
  for (uint32_t ms = 0; ms < SOM_CICLO_MS; ms += 5) {
    periodico = periodico && calcularSinalizacao(e, t, ms).buzzer ==
                                 calcularSinalizacao(e, t, ms + 7 * SOM_CICLO_MS).buzzer;
  }
  checar(periodico, "padrao se repete a cada ciclo de 2000 ms");
}

// TRANSBORDANDO soa so enquanto a tendencia for SUBINDO; o padrao e o mesmo do ALAGADO.
static void testarTransbordandoSubindo() {
  bool igual = true, soa = false;
  for (uint32_t ms = 0; ms < SOM_CICLO_MS; ms += 5) {
    bool a = calcularSinalizacao(EstadoFisico::TRANSBORDANDO, TendenciaLocal::SUBINDO, ms).buzzer;
    bool b = calcularSinalizacao(EstadoFisico::ALAGADO, TendenciaLocal::SUBINDO, ms).buzzer;
    igual = igual && a == b;
    soa = soa || a;
  }
  checar(soa, "TRANSBORDANDO subindo: o buzzer soa");
  checar(igual, "TRANSBORDANDO subindo usa o mesmo padrao do ALAGADO");

  // Quando a tendencia deixa de ser SUBINDO, o som cessa no mesmo instante.
  checar(calcularSinalizacao(EstadoFisico::TRANSBORDANDO, TendenciaLocal::SUBINDO, 0).buzzer &&
             !calcularSinalizacao(EstadoFisico::TRANSBORDANDO, TendenciaLocal::ESTAVEL, 0).buzzer,
         "TRANSBORDANDO: trocar SUBINDO por ESTAVEL cala o buzzer");
}

// A tarefa de amostragem publica estado e tendencia como bytes (0xFF = sem
// leitura). O totem converte os bytes sem aceitar valor invalido.
static void testarSinalDeBytes() {
  // Sem leitura da sonda (0xFF) ou estado fora da lista: tudo apagado, calado.
  bool apagado = true;
  const uint8_t semEstado[] = {0xFF, 4, 7, 0x80};
  for (uint8_t e : semEstado) {
    for (uint32_t ms = 0; ms < 2 * SOM_CICLO_MS; ms += 10) {
      SaidaSinalizacao s = calcularSinalizacaoDeBytes(e, 0 /* SUBINDO */, ms);
      apagado = apagado && !s.ledVerde && !s.ledAmarelo && !s.ledVermelho && !s.buzzer;
    }
  }
  checar(apagado, "estado 0xFF ou invalido: nenhum LED aceso e buzzer calado");

  // Bytes validos equivalem a chamar com os enums.
  bool igual = true;
  for (EstadoFisico e : ESTADOS) {
    for (TendenciaLocal t : TENDENCIAS) {
      for (uint32_t ms = 0; ms < SOM_CICLO_MS; ms += 10) {
        SaidaSinalizacao a = calcularSinalizacaoDeBytes((uint8_t)e, (uint8_t)t, ms);
        SaidaSinalizacao b = calcularSinalizacao(e, t, ms);
        igual = igual && a.ledVerde == b.ledVerde && a.ledAmarelo == b.ledAmarelo &&
                a.ledVermelho == b.ledVermelho && a.buzzer == b.buzzer;
      }
    }
  }
  checar(igual, "bytes validos dao a mesma saida que os enums (24 combinacoes)");

  // Tendencia 0xFF ou invalida conta como INSUFICIENTE: TRANSBORDANDO soa (regra
  // provisoria) com o mesmo padrao do ALAGADO, e o LED vermelho segue fixo...
  bool igual2 = true, soou = false, vermelho = true;
  const uint8_t tendRuim[] = {0xFF, 6, 9};
  for (uint8_t t : tendRuim) {
    for (uint32_t ms = 0; ms < 2 * SOM_CICLO_MS; ms += 10) {
      SaidaSinalizacao s = calcularSinalizacaoDeBytes((uint8_t)EstadoFisico::TRANSBORDANDO, t, ms);
      igual2 = igual2 && s.buzzer == calcularSinalizacao(EstadoFisico::ALAGADO, TendenciaLocal::SUBINDO, ms).buzzer;
      soou = soou || s.buzzer;
      vermelho = vermelho && s.ledVermelho;
    }
  }
  checar(soou && igual2, "TRANSBORDANDO com tendencia invalida (0xFF, 6, 9): buzzer soa no padrao dos 3 pulsos");
  checar(vermelho, "TRANSBORDANDO com tendencia invalida: LED vermelho continua aceso");

  // ...e ALAGADO soa sempre, mesmo sem tendencia.
  bool soa = false;
  for (uint32_t ms = 0; ms < SOM_CICLO_MS; ms++) {
    soa = soa || calcularSinalizacaoDeBytes((uint8_t)EstadoFisico::ALAGADO, 0xFF, ms).buzzer;
  }
  checar(soa, "ALAGADO com tendencia 0xFF: o buzzer soa (ALAGADO soa sempre)");
}

int main() {
  testarSinalDeBytes();
  testarConstantes();
  testarPoliticaDoSom();
  testarLeds();
  testarAlagadoPisca();
  testarLedsExclusivos();
  testarBuzzerCalado();
  testarBuzzerPadrao();
  testarTransbordandoSubindo();

  std::printf("\n%s\n", falhas == 0 ? "TODOS OS CENARIOS OK" : "HOUVE FALHAS");
  return falhas == 0 ? 0 : 1;
}

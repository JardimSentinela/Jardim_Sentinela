# Cenários controlados — [SCRUM-32][SW-16]

Objetivo: executar situações previsíveis em teste e apresentação, com sequências
reproduzíveis de **chuva moderada, chuva intensa, saturação e drenagem lenta**.

A entrega tem duas partes complementares:

1. **Gerador Java da API** — telemetria determinística para J01–J04, usada no
   dashboard e na validação da tendência (SW-14).
2. **Firmware/Wokwi** — roteiro com sonda HW-02 (`pot4`), estado físico e totem
   da SW-13.

Dependências: SW-08 (J01–J04 no gerador), SW-13, SW-14 e SW-11. O SW-13
(#24), o SW-14 (#23), o SW-11 (#22) e o CI (#28) foram mergeados na `develop`
em 23/11. Esta branch nasceu da `develop` anterior (`1652df3`), então o diff do
PR traz somente os arquivos novos deste card; a validação no GitHub roda sobre
a `develop` atual, já com essas dependências.

## Limites e convenções

- Nível entre **0 e 50 cm**, uma casa decimal, timestamp ISO-8601 UTC com
  resolução de 1 s — mesmos limites de `GeradorDadosJardim`.
- `tipo` é `boot` no primeiro registro e `snapshot` nos demais; `bootId`
  identifica a execução e `seq` recomeça em 0.
- **Reset** = novo `bootId` + `seq` zerado. O histórico recebido pela API não
  é apagado; a SW-14 ignora registros `boot`.
- Cada execução com `--inicio`, `--seed` e `--boot-id` iguais reproduz a mesma
  sequência. Sem esses parâmetros, o início é o instante atual e o `bootId` é
  novo a cada execução.
- O `bootId` tem **exatamente 8 caracteres hexadecimais** (ex.: `a1b2c3d4`), que
  é o formato aceito por `POST /api/leituras`. O `--boot-id` informado fora desse
  formato é recusado pelo runner antes de enviar qualquer coisa. Um UUID ou um
  texto livre (como `cenario-fixado-01`) era recusado pela API com HTTP 400.

| Cenário | Nível inicial | Variação | Intervalo | Passos padrão | Resultado esperado |
|---|---|---|---|---|---|
| `CHUVA_MODERADA` | 10 cm | +1,0 cm/leitura ±0,2 | 3 s | 60 | sobe até o teto de 50 cm. Tendência `SUBINDO` enquanto sobe (verificado com 20 a 40 passos); com os 60 padrão termina parado em 50 cm e a tendência é `ESTAVEL` |
| `CHUVA_INTENSA` | 5 cm | +2,5 cm/leitura ±0,3 | 3 s | 30 | sobe rápido até o teto de 50 cm. Tendência `SUBINDO` enquanto sobe (verificado com 8, 12 e 16 passos); com os 30 padrão termina parado em 50 cm e a tendência é `ESTAVEL` |
| `SATURACAO` | 46 cm | +1,0 cm/leitura | 3 s | 30 | para em 50 cm e fica lá |
| `DRENAGEM_LENTA` | 45 cm | −0,1 cm a cada 2 leituras | 20 s | 180 | ~0,15 cm/min → `DRENAGEM_LENTA` |

> Saturação aqui é o teto do gerador (50 cm). O estado `ALAGADO` da SW-13
> começa em 60 cm, fora do intervalo do gerador; ele é exercitável somente no
> firmware, com `pot4` acima de 60 %.

## Parte A — gerador Java

Compilar e listar o JSON sem publicar:

```bash
cd api-jardim-sentinela
./mvnw -B -q package -DskipTests
java -cp target/classes br.edu.cesar.jardimsentinela.gerador.ExecutaCenario \
  --cenario=CHUVA_MODERADA --jardim=J01 --dry-run --passos=8
```

Publicar na API local em sequência rápida (padrão):

```bash
java -cp target/classes br.edu.cesar.jardimsentinela.gerador.ExecutaCenario \
  --cenario=CHUVA_MODERADA --jardim=J01
java -cp target/classes br.edu.cesar.jardimsentinela.gerador.ExecutaCenario \
  --cenario=CHUVA_INTENSA --jardim=J02
java -cp target/classes br.edu.cesar.jardimsentinela.gerador.ExecutaCenario \
  --cenario=SATURACAO --jardim=J04
java -cp target/classes br.edu.cesar.jardimsentinela.gerador.ExecutaCenario \
  --cenario=DRENAGEM_LENTA --jardim=J03 --reset
```

Reproduzir a mesma sequência de novo (mesmo `bootId`, seed e instante):

```bash
java -cp target/classes br.edu.cesar.jardimsentinela.gerador.ExecutaCenario \
  --cenario=CHUVA_MODERADA --jardim=J01 \
  --boot-id=a1b2c3d4 --seed=7 --inicio=2026-11-21T10:00:00Z
```

Use `--tempo-real` para esperar o intervalo entre envios, e `--url=` para
apontar outra API. Os 180 registros do cenário de drenagem são publicados em
segundos com timestamps ao longo de ~60 min: isso evita uma apresentação de
60 min e cabe na janela de leitura do `DrenagemService` (máximo de 200 registros
por consulta).

**Limitação conhecida (timestamps no futuro).** Sem `--inicio`, os timestamps
começam no instante atual e avançam pelo intervalo do cenário. No cenário de
drenagem a última leitura fica cerca de **1 h no futuro** (verificado:
`ultimaLeituraEm` 1 h à frente do relógio). A tendência não é afetada, porque a
idade dela usa o horário de recepção, mas o dashboard mostra "última atualização"
no futuro e, pela regra de ordenação cronológica (`COALESCE(timestampDispositivo,
recebidoEm)`), uma leitura real mais nova ficaria atrás dessas. Para o cenário
terminar "agora", comece 1 h no passado (Linux ou Git Bash):

```bash
--inicio=$(date -u -d '60 minutes ago' +%Y-%m-%dT%H:%M:%SZ)
```

### O que conferir

- `GET /api/jardins/J01/leituras` mostra `seq` crescente e `tipo=snapshot`.
- `GET /api/jardins/J01/tendencia` (SW-14, já na `develop`) retorna `SUBINDO`
  **enquanto o nível sobe**: chuva moderada até cerca de 40 passos e chuva
  intensa até cerca de 16. Para ver isso, use `--passos` menor que o padrão e
  consulte logo em seguida (o limite de idade do perfil `simulacao` é 15 s; depois
  dele a tendência vira `DESATUALIZADO`, ver `docs/tendencia-nivel.md`). Com os
  passos padrão os dois cenários de chuva terminam parados em 50 cm, e a
  tendência é `ESTAVEL`. Na drenagem lenta também é `ESTAVEL`: a queda é menor
  que a tolerância de 1,5 cm na janela de 5 leituras.
- Após o merge do SCRUM-30 (SW-15), a drenagem lenta é classificada como
  `DRENAGEM_LENTA`. O serviço ainda não tem endpoint, então a verificação é
  feita nos testes.

## Parte B — firmware/Wokwi

Roteiro derivado de `docs/validacao-sinalizacao.md` (SW-13). Use **somente
`pot4`**, que é a sonda HW-02: nível em cm = posição do potenciômetro × 100.

### Início e reset

1. Pressione `L` para limpar a fila.
2. Reinicie o ESP32 com **EN/RST** (ou pare e rode o Wokwi). `L` sozinho apaga
   a linha de boot mas **não zera o `seq`**.
3. Espere a linha de boot e o relógio. Pressione `i` e confirme `pendentes: 0`,
   `dizimados: 0`, `descartados: 0`.
4. Anote o `bootId` exibido no diagnóstico. Esse é o reset do cenário.

### Execução dos cenários

| Cenário | Ação no `pot4` | Resultado esperado no totem |
|---|---|---|
| Chuva moderada | boot em ~10 %, subir para 22 % | verde → amarelo, `estado=ENCHENDO`, buzzer calado |
| Chuva intensa | manter ~40 % e subir 1 cm a cada 5 s até 46 % | amarelo → vermelho fixo, `tendencia=SUBINDO`, buzzer com 3 pulsos a cada 2 s |
| Saturação | ~50 % (teto do gerador) | `estado=TRANSBORDANDO`, vermelho fixo; acima de 60 % vira `ALAGADO` (piscando) |
| Drenagem lenta | ~46 %, descer 0,1 cm a cada 20 s | `tendencia=ESTAVEL`, buzzer calado; queda lenta não acende tendência de descida |

### Fim

1. Pressione `i` e copie o bloco `--- DIAGNOSTICO ---` (inclusive `bootId`).
2. Pressione `o` para voltar online e cronometre até `pendentes: 0`.
3. `d` mostra a fila; `u` aponta outra URL de receptor.

## Pendências de integração

- O SW-15 (SCRUM-30) ainda não entrou na `develop`. Sem ele não há classificação
  de drenagem lenta; o gerador já produz a sequência, mas a regra está na
  branch do SCRUM-30.
- Os passos 3.14 (confirmar `42.5` no receptor com `pot4` em 42,5 cm) e 3.16
  (receptor fora do ar, sem som contínuo) continuam pendentes no Wokwi, como
  registrado em `docs/validacao-sinalizacao.md` e no review do #24.
- A `develop` já aceita `tipo`, `bootId` e `seq` (SW-11/SW-14), então o reset
  por novo `bootId` vale sem depender de trocar os timestamps.
- O #26 (dashboard) segue aberto e usa limites de fallback 15/30/45 cm, contra
  22/45/60 cm do firmware. Para a apresentação, conferir se o `estado` enviado
  pela API está sendo priorizado; essa divergência é tratada no próprio #26.
- PlatformIO não está disponível no ambiente onde este documento foi escrito;
  `pio run` e o Wokwi precisam ser executados pelo time.

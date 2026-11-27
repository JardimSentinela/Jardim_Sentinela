# Contrato de telemetria — Jardim Sentinela

> **Card:** [SCRUM-19] [SW-01] Definir contrato de telemetria
> **Pai:** Estruturar a frente de Arquitetura
> **Sprint:** SCRUM Sprint 1 (revisado na Sprint 2: SCRUM-43 [SW-09], SCRUM-44 [SW-10])
> **Última revisão:** 2026-11-27

## Por que este documento existe

O mesmo dado de telemetria vai passar por três pontas diferentes:

```
ESP32 (Wokwi)  ─┐
Simulador      ─┼─→  POST /api/leituras  ─→  API
Gateway        ─┘
```

Se cada ponta inventar seus próprios nomes de campo (`nivelAgua` num lado,
`nivelCm` no outro), a API rejeita dados válidos ou aceita dados errados em
silêncio. Este documento é a **fonte de verdade** para o *formato do dado*:
se algo aqui mudar, muda em todos os lugares ao mesmo tempo.

> **Há duas coisas diferentes neste documento.** A seção 1 descreve o **payload
> da API** (`POST /api/leituras`, implementado). A seção 2 descreve o **que o
> ESP32 grava no arquivo `/fila.ndjson`**. As duas são formatos diferentes: o
> arquivo do firmware tem campos extras que a API lê ou ignora (ver 3.3).
> O estado das decisões está na seção 3; **o que está marcado como pendente
> ainda não é comportamento da API**.

---

## 1. Payload da API — `POST /api/leituras`

> **Estado:** implementado em `develop` + PR #15 (ponte de compatibilidade,
> SCRUM-43). Fonte no código: `LeituraRequest`.

Um objeto JSON por leitura. Firmware e simulador enviam o **mesmo payload** ao
**mesmo endpoint**, sem tratamento por origem.

### 1.1 Campos da requisição

| Campo (firmware e simulador) | Alias também aceito | Tipo             | Unidade          | Obrigatório | Regra de validação        | Exemplo                  |
| ---------------------------- | ------------------- | ---------------- | ---------------- | ----------- | ------------------------- | ------------------------ |
| `idJardim`                   | `jardimId`          | string           | —                | sim         | não vazio                 | `"J01"`                  |
| `idSensor`                   | `sensorId`          | string           | —                | sim         | não vazio                 | `"S01"`                  |
| `nivelAgua`                  | `nivelCm`           | number (decimal) | centímetros      | sim         | não negativo              | `42.5`                   |
| `bateria`                    | —                   | number (inteiro) | escala 0-100 (%) | sim         | inteiro entre 0 e 100     | `87`                     |
| `timestamp`                  | —                   | string           | ISO-8601 em UTC  | **não**     | data/hora ISO-8601 válida | `"2026-11-12T12:00:00Z"` |

- Campos opcionais [SCRUM-28][SW-14]: `tipo` (`evento`|`snapshot`|`boot`), `bootId`
  (8 hex), `seq` (inteiro >= 0, até 4294967295) e `horaSincronizada` (booleano, [F1]).
  São validados quando presentes e guardados (não aparecem nas respostas).
  `tipo: "boot"` não entra na tendência nem no estado. `bootId`+`seq` identificam a
  leitura (idempotência); sem eles vale `jardimId + sensorId + timestamp`, como antes.
  Registros sem esses campos (simulador, antigos) continuam aceitos e elegíveis.
  Quando `horaSincronizada = false`, a API ordena a leitura por `recebidoEm` (ver 3.4).
- Campos ainda desconhecidos (`deltaTMs`, `lat`, `lon`) são **aceitos e descartados**
  (ver 3.3).
- O **nome oficial** do campo (`idJardim` × `jardimId`, etc.) não está fechado
  (ver 3.1). Enquanto isso, os dois nomes valem.

### 1.2 Exemplo aceito

```json
{
  "idJardim": "J01",
  "idSensor": "S01",
  "nivelAgua": 42.5,
  "bateria": 87,
  "timestamp": "2026-11-12T12:00:00Z"
}
```

### 1.3 Resposta e campos de saída

A resposta **não usa os nomes da requisição**: usa os nomes internos da API.

| Campo de saída         | Tipo            | Descrição                                                              |
| ---------------------- | --------------- | ---------------------------------------------------------------------- |
| `id`                   | number          | Identificador da linha no banco                                        |
| `jardimId`             | string          | Mesmo valor de `idJardim`                                              |
| `sensorId`             | string          | Mesmo valor de `idSensor`                                              |
| `nivelCm`              | number          | Mesmo valor de `nivelAgua` (cm)                                        |
| `bateria`              | number          | 0-100                                                                  |
| `recebidoEm`           | string ISO-8601 | Instante em que a **API** recebeu. Sempre preenchido pelo servidor     |
| `timestampDispositivo` | string ISO-8601 | O `timestamp` enviado pelo **dispositivo**; `null` se não enviado      |

> Na requisição o campo chama-se `timestamp`; na resposta, `timestampDispositivo`.
> É **o mesmo dado**, com nomes diferentes por direção (decisão 2026-11-14).

Endpoints de leitura (detalhes e exemplos em `docs/api-dashboard.md`):

| Endpoint                                  | Retorna                                                              |
| ----------------------------------------- | -------------------------------------------------------------------- |
| `GET /api/jardins/{id}`                   | Última leitura + `ultimaLeituraEm` + `totalLeituras`; 404 se o jardim nunca enviou dado |
| `GET /api/jardins/{id}/leituras?limite=N` | Histórico, mais recente primeiro; `limite` padrão 100, máximo 1000   |
| `GET /api/jardins/{id}/tendencia`         | [SCRUM-28] Tendência do nível (6 estados) pelas 5 leituras válidas mais recentes, ordenadas por `COALESCE(timestampDispositivo, recebidoEm)`; 404 se o jardim nunca enviou dado. Campos em `docs/dicionario-de-dados.md`, seção 3.1 |

### 1.4 Idempotência (reenvio)

Se o payload traz `timestamp`, a mesma combinação **`idJardim` + `idSensor` +
`timestamp`** é tratada como o **mesmo evento**: o reenvio devolve **200** com a
leitura já existente e **não duplica**. A primeira gravação devolve **201**.
Sem `timestamp`, não há como detectar duplicata: cada envio grava uma leitura nova.

### 1.5 Erros

| Situação                                | HTTP | Código (`erro`)    |
| --------------------------------------- | ---- | ------------------ |
| Campo ausente ou fora das regras de 1.1 | 400  | `PAYLOAD_INVALIDO` |
| Corpo não é JSON válido, ou tipo errado | 400  | `CORPO_ILEGIVEL`   |
| Jardim sem nenhuma leitura (`GET /api/jardins/{id}` e `/leituras`) | 404 | `JARDIM_NAO_ENCONTRADO` |
| Leitura inexistente (`GET /api/leituras/{id}`) | 404 | corpo vazio |

Formato do corpo de erro (400 e 404 de jardim): `status`, `erro`, `mensagem`,
`campos` (lista de `{campo, mensagem}`; vazia no 404) e `momento`.

### 1.6 Formato dos ids

**Padrão: `J01`, `J02`… para jardins (`^J\d{2}$`) e `S01`, `S02`… para sensores
(`^S\d{2}$`)**, mais o valor reservado `idSensor: "jardim"` para a unidade inteira
(snapshots e `boot`). Detalhes em `docs/dicionario-de-dados.md`. A API **não valida**
esse formato hoje (só exige não vazio): a validação será ligada **depois** que o
firmware migrar, para não rejeitá-lo. O firmware já envia `idJardim: "J01"`
(constante `JARDIM_ID`); os ids de sensor `sensor-01`..`sensor-04` (legado, não
`S01`..`S04`) seguem como estão e ainda precisam migrar: **legado a migrar**, não
padrão. A migração dos sensores é do time de hardware (dicionário, seção 6).

---

## 2. Formato gravado pelo firmware (ESP32 → `/fila.ndjson`)

> **Estado:** implementado no firmware `esp32/src/main_alt.cpp` (branch
> `feat/SCRUM-31-Emular-sensores-resistivos-T1592`).
> Uma linha JSON por registro, terminada em **LF** (`\n`), sem `\r`.

### 2.1 Campos

| Campo             | Tipo            | Unidade          | Presente em                        | Descrição                                                                    |
| ----------------- | --------------- | ---------------- | ---------------------------------- | ---------------------------------------------------------------------------- |
| `tipo`            | string          | —                | **todas**                          | `"evento"` (mudança de patamar), `"snapshot"` (heartbeat periódico), `"boot"` (primeira linha do boot) |
| `idJardim`        | string          | —                | **todas**                          | Identificador do jardim (`J01`, constante `JARDIM_ID`; antes `jardim-centro-01`) |
| `idSensor`        | string          | —                | **todas**                          | `sensor-01`..`sensor-03` (T1592) e `sensor-04` (sonda HW-02) nos **eventos**; `jardim` nos **snapshots** e na linha de **boot** (esses dois representam a unidade inteira) |
| `nivelAgua`       | number (1 casa) | centímetros       | **todas**                          | Nível atual do nível mais alto molhado (`0.0` quando nada está molhado)        |
| `bateria`         | number (int)    | escala 0-100     | **todas**                          | Carga da bateria — hoje é um valor **constante** (`100`); não há sensor de bateria no firmware |
| `timestamp`       | string          | ISO-8601 em UTC  | **todas**                          | Instante do **evento** (não o da gravação). Ver §2.3                            |
| `seq`             | number (int)    | —                | **todas**                          | Contador do boot: `0` na linha de boot, +1 a cada registro criado. Atribuído na **criação** do registro, então um registro perdido deixa **buraco visível** na sequência |
| `bootId`          | string (8 hex)  | —                | **todas**                          | Identificador do boot, novo a cada reinício (`esp_random()`). Separa um boot do anterior |
| `deltaTMs`        | number (int)    | milissegundos    | **só evento de SUBIDA de nível**   | Tempo entre patamares. **Omitido** nas quedas e nos snapshots/boot, para economizar bytes |
| `lat` / `lon`     | number (6 casas)| graus            | **só a linha de boot** (simulação)  | Coordenadas configuradas do jardim. **PLACEHOLDER** (`0.0`) — valor configurado, não medido; a equipe define o valor real |
| `horaSincronizada`| boolean         | —                | **todas**                          | `true` se o relógio estava sincronizado (NTP) quando o registro nasceu; `false` enquanto o horário é **estimado** |
| `estado`          | string          | —                | **só registros com classificação** | Estado físico da sonda: `NORMAL`, `ENCHENDO`, `TRANSBORDANDO` ou `ALAGADO`. Faixas e histerese **provisórias** (`esp32/src/modulos/estado_fisico.h`). Omitido em registro sem classificação (ex.: eventos T1592) |
| `tendenciaDispositivo` | string     | —                | **só registros com classificação** | Tendência calculada pelo firmware (`SUBINDO`, `ESTAVEL`, `DESCENDO`, `INSUFICIENTE`, `INDETERMINADO`, `DESATUALIZADO`). Só auditoria na API |
| `versaoRegra`     | string          | —                | **junto com `estado` ou `tendenciaDispositivo`** | Versão da regra que classificou (`v1-provisoria-2026-11`) |

### 2.2 Exemplos reais (gerados na simulação)

Linha de **boot** (primeira do arquivo, `seq 0`):

```json
{"tipo":"boot","idJardim":"J01","idSensor":"jardim","nivelAgua":0.0,"bateria":100,"timestamp":"2026-10-15T00:00:00Z","seq":0,"bootId":"00000032","lat":0.000000,"lon":0.000000,"horaSincronizada":false}
```

Linha de **snapshot** (heartbeat periódico):

```json
{"tipo":"snapshot","idJardim":"J01","idSensor":"jardim","nivelAgua":0.0,"bateria":100,"timestamp":"2026-10-15T00:00:00Z","seq":1,"bootId":"00000032","horaSincronizada":false}
```

Linha de **evento** (mudança de patamar, sem subida):

```json
{"tipo":"evento","idJardim":"J01","idSensor":"sensor-01","nivelAgua":5.0,"bateria":100,"timestamp":"2026-10-15T00:00:00Z","seq":2,"bootId":"00000032","horaSincronizada":false}
```

Linha de **evento** (subida de nível — única com `deltaTMs`):

```json
{"tipo":"evento","idJardim":"J01","idSensor":"sensor-02","nivelAgua":10.0,"bateria":100,"timestamp":"2026-10-15T00:00:01Z","seq":5,"bootId":"00000032","deltaTMs":100,"horaSincronizada":false}
```

Tamanhos médios: evento **189 B**, snapshot **188 B**, boot **214 B** (incluindo o LF), medidos
antes de `estado`, `tendenciaDispositivo` e `versaoRegra` (+~100 B quando presentes). A linha mais
longa (boot com todos os campos e `seq` de 32 bits) mede 328 B; o buffer é de 384 B.

O **nível** (`nivelAgua`) chega à API com a mesma casa decimal gravada na linha: o quadro v2 do
envio local carrega o nível em décimos de cm (42,5 cm = 425), sem arredondar para inteiro.

### 2.3 Hora do registro: `timestamp` e `horaSincronizada`

O `timestamp` é sempre o **instante do evento**, nunca o instante da gravação:

- **Com o relógio sincronizado** (NTP ligado): `timestamp` = hora atual menos o
  tempo já decorrido desde o evento (`millis() - tMs`). Um registro que ficou na
  fila não "envelhece" o horário.
- **Sem NTP** (situação atual): `timestamp` = `2026-10-15T00:00:00Z` + `millis()`
  do evento. É um horário **estimado**, e por isso `horaSincronizada` vai `false`.

Em ambos os casos o **intervalo real entre dois eventos é preservado**. Um reset
do ESP32 reinicia a base do horário estimado — por isso `horaSincronizada`
existe: uma linha com `false` nunca deve ser tratada como hora real.

> **Ponto de ligação do NTP:** no firmware, a função `iniciarSincronizacaoNtp()`
> (`esp32/src/main_alt.cpp`) é o lugar marcado onde entra `WiFi.begin()` e
> `configTime(0, 0, "pool.ntp.org", …)`.

### 2.4 O arquivo é uma **janela**, não um histórico

O arquivo tem teto de `MAX_BYTES_FILA` (64 KB na simulação). Ao encher, o
firmware **descarta os registros mais antigos** e **mantém os novos** (a
compactação reescreve o arquivo num temporário e o substitui por `rename`). O
conteúdo do arquivo é, portanto, uma **janela com os registros mais recentes** —
não o histórico completo desde o primeiro boot.

Consequências para quem lê:

- A primeira linha do arquivo pode ter `seq` > 0: o início foi descartado.
- O número de registros perdidos aparece no contador `descartados` (mostrado no
  monitor serial) e pode ser deduzido pelo salto de `seq`.
- O arquivo **nunca** termina com linha incompleta: se falta energia no meio de
  uma gravação, a linha truncada é removida no boot seguinte.

### 2.5 `lat`/`lon`: simulação × piloto

- **Simulação (agora):** `lat`/`lon` saem **só na linha de boot**, para não
  repetir as mesmas coordenadas em todas as linhas.
- **Piloto (previsto):** `lat`/`lon` vão em **todas as linhas**, para confirmar a
  localização do jardim a cada registro. O valor continua sendo um **PLACEHOLDER**
  até a equipe definir as coordenadas reais.

---

## 3. Decisões

### 3.0 Registro de decisões

| Data       | Decisão                                                                                       | Estado                         |
| ---------- | --------------------------------------------------------------------------------------------- | ------------------------------ |
| 2026-11-14 | `timestamp` é o nome na requisição; `timestampDispositivo` é o nome na resposta/banco         | decidido (a comunicar ao time) |
| 2026-11-14 | `timestamp` é **opcional** (mantém o comportamento da PR #15)                                 | decidido (a comunicar ao time) |
| 2026-11-14 | Ids no padrão `J01` / `S01`                                                                   | decidido (a comunicar ao time) |
| 2026-11-14 | Medição = `nivelAgua` em cm, por ora                                                          | decidido (a comunicar ao time) |
| 2026-11-14 | Nome oficial de id/sensor/nível: **manter a ponte** (dois nomes aceitos na requisição) e reavaliar depois da Sprint 3 (ver 3.1) | decidido (a comunicar ao time) |
| 2026-11-14 | Histórico e estado atual ordenam por `timestampDispositivo`, com `recebidoEm` quando for nulo (opção B, ver 3.4) | decidido; implementado (SW-11) |
| 2026-11-14 | Formato do id definido (`J01`, `S01`, `jardim`); **validação na API só depois** da migração do firmware (ver 1.6) | decidido; implementado (SW-11) |
| 2026-11-14 | Em payloads novos de requisição, usar `idJardim`/`idSensor`/`nivelAgua`/`timestamp` (ver dicionário, regra 4) | decidido (a comunicar ao time) |
| 2026-11-26 | `horaSincronizada = false` ordena por `recebidoEm` (F1); `tipo = boot` não é leitura atual (F2) | implementado |
| 2026-11-27 | `incluirBoot` no histórico (padrão `true`); dashboard usa `false` (PR #33) | implementado |

### 3.1 Nomes de campo: firmware × nome interno da API — ponte mantida por ora

O firmware e o simulador enviam `idJardim`, `idSensor`, `nivelAgua`. A API
guarda e responde `jardimId`, `sensorId`, `nivelCm`. Desde a PR #15, a API
aceita **os dois nomes na requisição** (`@JsonAlias` em `LeituraRequest`), então
**o payload do firmware é aceito** (a versão anterior deste documento dizia o
contrário). Isso foi uma ponte pragmática para fechar o SW-09; **nenhum nome
venceu**, e a decisão é manter a ponte e reavaliar depois da Sprint 3.
Consolidar em um só é uma refatoração de renomear. Até lá, quem
consome a **resposta** (dashboard) deve usar os nomes internos (1.3).

### 3.2 Nível × estado — nível adotado, por ora

`nivelAgua` (altura em cm do nível mais alto molhado) e um booleano `molhado`
são informações distintas. **O firmware atual registra `nivelAgua`** (cm), e a
decisão é manter assim por ora. Registrar também o estado por sensor continua
**em aberto**. O significado físico da medição (como a sonda calcula o cm) deve
ser **confirmado com o time de hardware**.

### 3.3 Campos do firmware: o que a API lê e o que ainda ignora

> **Atualização [SCRUM-28][SW-14] + [F1]:** `tipo`, `bootId`, `seq` e `horaSincronizada` passaram a
> ser lidos (ver 1.1 e 3.4). Falta decidir a detecção explícita de **perda** (buraco em `seq`). O
> texto abaixo vale para o que continua ignorado.

`deltaTMs`, `lat` e `lon` são desconhecidos para o DTO e **ignorados em silêncio** (o Jackson 3 do
Spring Boot 4 desliga `FAIL_ON_UNKNOWN_PROPERTIES` por padrão): o dado é aceito e descartado.

`tipo`, `bootId`, `seq` e `horaSincronizada` agora existem no DTO e são persistidos. Com `bootId` e
`seq` a idempotência usa `idJardim` + `bootId` + `seq`; sem eles, `idJardim` + `idSensor` +
`timestamp` (1.4). `tipo = boot` não entra na tendência, no estado nem na leitura atual (F2), e
`horaSincronizada = false` faz a ordenação usar `recebidoEm` (F1, ver 3.4).

Continua em aberto:
- detectar **perda** (buraco em `seq`) de forma explícita, além da idempotência já implementada;
- distinguir `evento`, `snapshot` e `boot` em relatórios e consultas além da exclusão do boot.

### 3.4 Qual hora manda — decidido e implementado: opção B

Cada leitura tem duas horas: `timestampDispositivo` (quando o sensor mediu) e
`recebidoEm` (quando a API recebeu). **Antes da SW-11, o histórico e o
"estado atual" ordenavam por `recebidoEm`.** Com o dispositivo offline, o firmware reenvia a fila
de uma vez; segundo os comentários de `esp32/src/modulos/drenagem.cpp`, a
drenagem envia o snapshot mais recente (âncora) e depois os eventos mais antigos.
Nesse caso:

- no gráfico, leituras de horas diferentes ficam coladas no instante do reenvio;
- o "estado atual" pode mostrar uma leitura **antiga** se ela chegar por último.

| Opção | Ordenação                                                        | Risco                                              |
| ----- | ---------------------------------------------------------------- | -------------------------------------------------- |
| A     | `recebidoEm` (como hoje)                                         | Errada após reconexão                              |
| B     | `timestampDispositivo`, com `recebidoEm` quando for nulo         | Relógio do dispositivo errado; mitigado usando `recebidoEm` quando `horaSincronizada = false` |
| C     | A API devolve as duas; o dashboard escolhe                       | Empurra a decisão para o front                     |

**Decisão (2026-11-14): opção B.** Histórico e "estado atual" passam a ordenar
por `timestampDispositivo`, usando `recebidoEm` quando ele for nulo. **Implementado
na SW-11** (histórico, estado atual e tendência ordenam pela mesma chave).

**F1 corrigido (26/11/2026):** quando `horaSincronizada = false`, a ordenação usa `recebidoEm` (hora
estimada não é confiável). Implementado em `LeituraRepository.CHAVE_CRONOLOGICA` e em
`CalculadoraTendencia.chaveCronologica`, com regressão em `DivergenciasF1F2Tests`.

**F2 corrigido (26/11/2026):** registros `tipo = boot` não são a leitura atual do detalhe nem da
lista (`JardimService.buscarJardim` e `LeituraRepository.findUltimaLeituraDeCadaJardim`).

---

## 4. Observações gerais

- **Unidade do nível:** centímetros. O nome do campo (`nivelAgua`) não carrega a
  unidade; ela está documentada aqui.
- **Formato do arquivo ≠ formato do rádio.** O formato do `/fila.ndjson` é o
  **formato de armazenamento local** (store-and-forward), pensado para ser
  reescrito, truncado e recuperado após falta de energia. Ele **não é
  necessariamente** o formato que irá pelo LoRa no piloto: o rádio tem limite de
  bytes por quadro e de energia, e o envio (com ACK) é responsabilidade da Pessoa 3.
  Decidir o formato do quadro LoRa é uma tarefa à parte — este documento
  descreve o arquivo, não o payload do rádio.
- **Resolução do `timestamp`:** 1 segundo (sem milissegundos), ISO-8601 UTC.
- **`bateria`:** hoje é constante no firmware; quando houver sensor, o valor
  passará a ser medido.
- **`timestamp` ausente:** a API não rejeita; `timestampDispositivo` fica `null`
  e vale só o `recebidoEm`.

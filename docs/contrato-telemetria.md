# Contrato de telemetria — Jardim Sentinela

> **Card:** [SCRUM-19] [SW-01] Definir contrato de telemetria
> **Pai:** Estruturar a frente de Arquitetura
> **Sprint:** SCRUM Sprint 1

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

> **Há duas coisas diferentes neste documento.** A seção "Formato" descreve o **payload
> da API** (a proposta do card SCRUM-19). A seção "Formato gravado pelo firmware"
> descreve o **que o ESP32 realmente grava no arquivo `/fila.ndjson`**. As duas
> ainda não são o mesmo formato — e a divergência está **registrada e aberta**,
> não resolvida (ver "Decisões em aberto").

---

## 1. Formato (payload da API — proposta do card SCRUM-19)

Um objeto JSON por leitura, enviado no corpo da requisição `POST /api/leituras`.

### Campos

| Campo       | Tipo               | Unidade          | Obrigatório | Descrição                                      | Exemplo                 |
| ----------- | ------------------ | ---------------- | ----------- | ---------------------------------------------- | ----------------------- |
| `idJardim`  | string             | —                | sim         | Identificador do jardim                        | `"jardim-centro-01"`    |
| `idSensor`  | string             | —                | sim         | Identificador do sensor que gerou a leitura    | `"sensor-01"`           |
| `nivelAgua` | number (decimal)   | centímetros       | sim         | Nível de água medido                           | `42.5`                  |
| `bateria`   | number (inteiro)   | escala 0-100 (%) | sim         | Carga da bateria da unidade de campo           | `87`                    |
| `timestamp` | string             | ISO-8601 em UTC  | sim         | Data/hora da leitura                           | `"2026-10-15T02:25:28Z"` |

### Exemplo aceito

```json
{
  "idJardim": "jardim-centro-01",
  "idSensor": "sensor-01",
  "nivelAgua": 42.5,
  "bateria": 87,
  "timestamp": "2026-10-15T02:25:28Z"
}
```

### Regras de validação (base para o SCRUM-22)

| Campo       | Regra                                                                 |
| ----------- | --------------------------------------------------------------------- |
| `idJardim`  | não vazio                                                             |
| `idSensor`  | não vazio                                                             |
| `nivelAgua` | número, não negativo                                                  |
| `bateria`   | inteiro entre 0 e 100                                                 |
| `timestamp` | data/hora válida em ISO-8601                                          |

Campos ausentes ou fora dessas regras caracterizam **payload inválido** e devem
ser rejeitados (ver critério de aceite do SCRUM-22).

---

## 2. Formato gravado pelo firmware (ESP32 → `/fila.ndjson`)

> **Estado:** implementado no firmware `esp32/src/main_alt.cpp` (branch
> `feat/SCRUM-31-Emular-sensores-resistivos-T1592`).
> Uma linha JSON por registro, terminada em **LF** (`\n`), sem `\r`.

### 2.1 Campos

| Campo             | Tipo            | Unidade          | Presente em                        | Descrição                                                                    |
| ----------------- | --------------- | ---------------- | ---------------------------------- | ---------------------------------------------------------------------------- |
| `tipo`            | string          | —                | **todas**                          | `"evento"` (mudança de patamar), `"snapshot"` (heartbeat periódico), `"boot"` (primeira linha do boot) |
| `idJardim`        | string          | —                | **todas**                          | Identificador do jardim (`jardim-centro-01`)                                  |
| `idSensor`        | string          | —                | **todas**                          | `sensor-01`..`sensor-03` nos **eventos**; `jardim` nos **snapshots** e na linha de **boot** (esses dois representam a unidade inteira) |
| `nivelAgua`       | number (1 casa) | centímetros       | **todas**                          | Nível atual do nível mais alto molhado (`0.0` quando nada está molhado)        |
| `bateria`         | number (int)    | escala 0-100     | **todas**                          | Carga da bateria — hoje é um valor **constante** (`100`); não há sensor de bateria no firmware |
| `timestamp`       | string          | ISO-8601 em UTC  | **todas**                          | Instante do **evento** (não o da gravação). Ver §2.3                            |
| `seq`             | number (int)    | —                | **todas**                          | Contador do boot: `0` na linha de boot, +1 a cada registro criado. Atribuído na **criação** do registro, então um registro perdido deixa **buraco visível** na sequência |
| `bootId`          | string (8 hex)  | —                | **todas**                          | Identificador do boot, novo a cada reinício (`esp_random()`). Separa um boot do anterior |
| `deltaTMs`        | number (int)    | milissegundos    | **só evento de SUBIDA de nível**   | Tempo entre patamares. **Omitido** nas quedas e nos snapshots/boot, para economizar bytes |
| `lat` / `lon`     | number (6 casas)| graus            | **só a linha de boot** (simulação)  | Coordenadas configuradas do jardim. **PLACEHOLDER** (`0.0`) — valor configurado, não medido; a equipe define o valor real |
| `horaSincronizada`| boolean         | —                | **todas**                          | `true` se o relógio estava sincronizado (NTP) quando o registro nasceu; `false` enquanto o horário é **estimado** |

### 2.2 Exemplos reais (gerados na simulação)

Linha de **boot** (primeira do arquivo, `seq 0`):

```json
{"tipo":"boot","idJardim":"jardim-centro-01","idSensor":"jardim","nivelAgua":0.0,"bateria":100,"timestamp":"2026-10-15T00:00:00Z","seq":0,"bootId":"00000032","lat":0.000000,"lon":0.000000,"horaSincronizada":false}
```

Linha de **snapshot** (heartbeat periódico):

```json
{"tipo":"snapshot","idJardim":"jardim-centro-01","idSensor":"jardim","nivelAgua":0.0,"bateria":100,"timestamp":"2026-10-15T00:00:00Z","seq":1,"bootId":"00000032","horaSincronizada":false}
```

Linha de **evento** (mudança de patamar, sem subida):

```json
{"tipo":"evento","idJardim":"jardim-centro-01","idSensor":"sensor-01","nivelAgua":5.0,"bateria":100,"timestamp":"2026-10-15T00:00:00Z","seq":2,"bootId":"00000032","horaSincronizada":false}
```

Linha de **evento** (subida de nível — única com `deltaTMs`):

```json
{"tipo":"evento","idJardim":"jardim-centro-01","idSensor":"sensor-02","nivelAgua":10.0,"bateria":100,"timestamp":"2026-10-15T00:00:01Z","seq":5,"bootId":"00000032","deltaTMs":100,"horaSincronizada":false}
```

Tamanhos médios: evento **189 B**, snapshot **188 B**, boot **214 B** (incluindo o LF).

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

## 3. Decisões em aberto (NÃO resolvidas por este documento)

As duas divergências abaixo **permanecem abertas**. A alteração do firmware e do
formato do arquivo **não as resolveu** — e não deve ser lida como se tivesse.

### 3.1 Divergência de **nomes** entre o firmware e a API

| Ponto        | Firmware (arquivo)          | API (DTO `LeituraRequest`) | Situação                      |
| ------------ | --------------------------- | -------------------------- | ----------------------------- |
| id do jardim | `idJardim`                  | `jardimId`                 | **ABERTO** — nomes divergentes |
| id do sensor | `idSensor`                  | `sensorId`                 | **ABERTO** — nomes divergentes |
| medição      | `nivelAgua` (número, cm)    | `nivelCm` (número, cm)     | **ABERTO** — nomes divergentes |
| hora         | `timestamp` (ISO-8601 UTC)  | não existe no DTO          | **ABERTO** — a API usa `Instant.now()` (hora do servidor) |
| bateria      | `bateria`                   | `bateria`                  | coincide no nome; no firmware é um valor **constante** |

**Consequência prática hoje:** o payload que o firmware grava **não é aceito** pela
API atual (400 Bad Request), porque `jardimId`, `sensorId` e `nivelCm` não são
recebidos. Alinhar os nomes é uma decisão **da equipe**, não deste trabalho.

### 3.2 Divergência conceitual: **nível × estado**

`nivelAgua` (altura em centímetros do nível mais alto molhado) e um booleano
`molhado` (seco/molhado por sensor) **não são o mesmo dado com nomes diferentes**:
são informações distintas. Escolher um lado significa descartar o outro, e isso
afeta o que o firmware precisa medir.

**O firmware atual registra `nivelAgua`** (valor em cm). A decisão de registrar
também o estado booleano por sensor continua **ABERTA**.

### 3.3 Consequência no lado da API (para quando for alinhar)

Os campos novos do firmware (`tipo`, `seq`, `bootId`, `deltaTMs`,
`horaSincronizada`, `lat`, `lon`) são **desconhecidos** para o DTO da API atual,
que os **ignora em silêncio** (o Jackson 3 do Spring Boot 4 desliga
`FAIL_ON_UNKNOWN_PROPERTIES` por padrão). Ou seja: mandar `tipo`/`seq`/`bootId`
hoje **não dá erro** — o dado é aceito e descartado.

Quando a equipe alinhar os nomes, vale decidir se a API passa a **usar**
`seq` + `bootId` para:

- detectar **perda** (buraco na sequência) e **retransmissão** (mesmo `seq` e
  mesmo `bootId` já recebidos) — hoje a API não tem proteção contra duplicatas;
- distinguir **evento**, **snapshot** e **boot**;
- confiar no `timestamp` do dispositivo apenas quando `horaSincronizada` for
  `true`.

---

## 4. Observações gerais

- **Unidade do nível:** adotada em centímetros. O nome do campo (`nivelAgua`)
  não carrega a unidade; ela está documentada aqui.
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

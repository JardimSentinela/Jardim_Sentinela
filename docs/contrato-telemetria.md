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
silêncio. Este documento é a **fonte da verdade**: se algo aqui mudar, muda em
todos os lugares ao mesmo tempo.

## Formato

Um objeto JSON por leitura, enviado no corpo da requisição `POST /api/leituras`.

### Campos

| Campo       | Tipo               | Obrigatório | Descrição                                      | Exemplo                 |
| ----------- | ------------------ | ----------- | ---------------------------------------------- | ----------------------- |
| `idJardim`  | string             | sim         | Identificador do jardim                        | `"jardim-centro-01"`    |
| `idSensor`  | string             | sim         | Identificador do sensor que gerou a leitura    | `"sensor-01"`           |
| `nivelAgua` | number (decimal)   | sim         | Nível de água medido, em centímetros           | `42.5`                  |
| `bateria`   | number (inteiro)   | sim         | Carga da bateria da unidade de campo, 0 a 100  | `87`                    |
| `timestamp` | string             | sim         | Data/hora da leitura, ISO-8601 em UTC          | `"2026-10-15T02:25:28Z"` |

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

## Decisão adiada (nada fechado por enquanto)

Este contrato **não está fechado**. Os nomes e tipos abaixo refletem a proposta
do card SCRUM-19, mas **não foram adotados como padrão**. A divergência entre
card, código e firmware será resolvida com a equipe. Nenhum dos três deve ser
tratado como fonte da verdade até lá.

## Divergências registradas (a resolver com a equipe)

| Ponto       | Card SCRUM-19 (proposta) | Código em `main`  | Firmware ESP32                                    | Situação             |
| ----------- | ------------------------ | ----------------- | ------------------------------------------------- | -------------------- |
| id do jardim | `idJardim`              | `jardimId`        | `jardimId`                                        | conflito de nome     |
| id do sensor | `idSensor`              | `sensorId`        | `sensorId`                                        | conflito de nome     |
| medição      | `nivelAgua` (número)     | `nivelCm` (número) | `molhado` (booleano)                             | **conflito de conceito** |
| bateria      | `bateria`                | `bateria`         | não existe                                        | lacuna no firmware   |
| tempo        | `timestamp` (ISO-8601)   | não existe        | `tMs` (millis desde o boot) + `bootId` e `seq`    | **conflito de conceito** |

**O ponto crítico é a medição.** `nivelAgua` (nível em centímetros) e `molhado`
(estado seco/molhado) **não são o mesmo dado com nomes diferentes** — são
informações distintas. Escolher um lado significa descartar o outro, e isso
afeta diretamente o que o firmware precisa medir. O ESP32 hoje produz
`molhado`, `bootId`, `seq` e `tMs` (`esp32/main_alt.cpp`, linha 112), e esse
firmware ainda está marcado como rascunho não definitivo.

## Outras observações

- **Unidade do nível:** adotada em centímetros. O nome do campo (`nivelAgua`)
  não carrega a unidade; ela está documentada aqui.
- **`timestamp`:** não existe no código atual. Se o ESP32 não tiver relógio
  confiável, avaliar se o horário deve ser preenchido pelo servidor.

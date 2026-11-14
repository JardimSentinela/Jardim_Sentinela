# Dicionário de dados — Jardim Sentinela

> **Para quê:** nomear cada dado uma vez só, para API, firmware, simulador,
> dashboard e agentes seguirem o mesmo padrão.
> **Última revisão:** 2026-11-14
> **Relação com outros documentos:** o formato dos payloads está em
> `docs/contrato-telemetria.md`; os exemplos de resposta, em `docs/api-dashboard.md`.
> Se este dicionário e o contrato divergirem, **corrija os dois no mesmo commit**.

## 1. Convenções

| Tema            | Regra                                                                                       |
| --------------- | ------------------------------------------------------------------------------------------- |
| Chaves de JSON  | camelCase, sem acento (`nivelAgua`, `timestampDispositivo`)                                 |
| Datas e horas   | ISO-8601 em UTC com `Z`, resolução de 1 segundo (`2026-11-12T12:00:00Z`)                    |
| Nível de água   | centímetros; o nome do campo não carrega a unidade                                          |
| Bateria         | inteiro de 0 a 100 (%)                                                                      |
| Id de jardim    | `J` + 2 dígitos: `J01`, `J02`… (regex `^J\d{2}$`)                                           |
| Id de sensor    | `S` + 2 dígitos: `S01`, `S02`… (regex `^S\d{2}$`), mais o valor reservado `jardim` (ver 1.1) |
| Valores legados | `jardim-centro-01`, `sensor-01`… existem no firmware e nos exemplos antigos: **legado a migrar** |
| Validação       | **Regra definida, validação na API ainda desligada** (ver seção 6)                          |

### 1.1 Valores de `idSensor`

| Valor            | Significa                                                        | Observação                                                       |
| ---------------- | ---------------------------------------------------------------- | ---------------------------------------------------------------- |
| `S01`, `S02`, `S03` | Sensores resistivos T1592 (degraus de 5, 10 e 15 cm)          | Hoje o firmware envia `sensor-01`..`sensor-03` (legado a migrar) |
| `S04`            | Sonda HW-02 (leitura contínua em cm)                             | **A confirmar com hardware:** deduzido do índice da sonda no firmware, que hoje sai como `sensor-04` |
| `jardim`         | Unidade inteira (usado nos snapshots e na linha de `boot`)       | Valor **reservado**: não segue o padrão `S` + 2 dígitos. Mantido como está no firmware |

## 2. Campos de leitura

"Requisição" = o que o firmware e o simulador enviam. "API" = o que a API guarda e devolve.

| Conceito          | Requisição         | Alias aceito na requisição | API (resposta/banco)   | Tipo            | Obrig. | Regra                      |
| ----------------- | ------------------ | -------------------------- | ---------------------- | --------------- | ------ | -------------------------- |
| Jardim            | `idJardim`         | `jardimId`                 | `jardimId`             | string          | sim    | não vazio                  |
| Sensor            | `idSensor`         | `sensorId`                 | `sensorId`             | string          | sim    | não vazio                  |
| Nível de água     | `nivelAgua`        | `nivelCm`                  | `nivelCm`              | decimal (cm)    | sim    | ≥ 0                        |
| Bateria           | `bateria`          | —                          | `bateria`              | inteiro (%)     | sim    | 0 a 100                    |
| Hora da medição   | `timestamp`        | —                          | `timestampDispositivo` | ISO-8601 UTC    | não    | data válida; `null` se ausente |
| Hora de recebimento | — (não enviar)   | —                          | `recebidoEm`           | ISO-8601 UTC    | —      | preenchido pelo servidor   |
| Identificador     | — (não enviar)     | —                          | `id`                   | número          | —      | gerado pelo banco          |

**`nivelAgua` é sempre em centímetros, qualquer que seja a fonte:** os sensores
resistivos dão degraus (5, 10 ou 15 cm) e a sonda dá leitura contínua. Qual fonte
vale no piloto real é decisão do **time de hardware**, não do dicionário.

**Atenção:** `timestamp` (requisição) e `timestampDispositivo` (API) são o **mesmo
dado**. Não crie um terceiro nome (`medidoEm`, `dataLeitura`…).

## 3. Campos de resposta de `GET /api/jardins/{id}`

**Definição de "leitura mais recente"** (vale para o estado atual e para o histórico):
a de maior `timestampDispositivo`; quando ele for nulo, usa-se `recebidoEm`
(ordenação por `COALESCE(timestampDispositivo, recebidoEm)`). **Estado: a implementar** (ver seção 6).

| Campo             | Tipo            | Descrição                                                    |
| ----------------- | --------------- | ------------------------------------------------------------ |
| `jardimId`        | string          | Id do jardim                                                 |
| `sensorId`        | string          | Sensor da última leitura                                     |
| `nivelCm`         | decimal (cm)    | Nível da última leitura                                      |
| `bateria`         | inteiro (%)     | Bateria da última leitura                                    |
| `ultimaLeituraEm` | ISO-8601 UTC    | Hora da leitura mais recente. **Alvo:** `timestampDispositivo` (ou `recebidoEm` se nulo). **Hoje:** devolve `recebidoEm` — ver seção 6 |
| `totalLeituras`   | inteiro         | Quantidade de leituras do jardim                             |

## 4. Campos do firmware que a API ignora

`tipo` (`evento`, `snapshot`, `boot`), `seq`, `bootId`, `deltaTMs`,
`horaSincronizada`, `lat`, `lon`. São aceitos e descartados. Definição completa em
`docs/contrato-telemetria.md`, seção 2.

## 5. Erros

| `erro`             | HTTP | Quando                                         |
| ------------------ | ---- | ---------------------------------------------- |
| `PAYLOAD_INVALIDO` | 400  | campo ausente ou fora da regra                 |
| `CORPO_ILEGIVEL`   | 400  | JSON inválido ou tipo errado                   |
| `JARDIM_NAO_ENCONTRADO` | 404 | jardim sem nenhuma leitura (`GET /api/jardins/{id}` e `/leituras`) |
| (corpo vazio)      | 404  | leitura inexistente (`GET /api/leituras/{id}`) |

Status de criação: **201** na primeira gravação; **200** no reenvio do mesmo
`idJardim` + `idSensor` + `timestamp` (idempotência).

## 6. Alvo × implementado

Este dicionário descreve o **alvo**. A coluna "Estado" diz o que o código já cumpre.
Cada linha "a implementar" vira item de teste (vermelho) e código na Sprint 3.

| Regra                                                                              | Estado            |
| ---------------------------------------------------------------------------------- | ----------------- |
| Requisição aceita `idJardim`/`idSensor`/`nivelAgua`/`timestamp` e os aliases       | implementado      |
| Idempotência por `idJardim` + `idSensor` + `timestamp` (201 / 200)                   | implementado      |
| Histórico e estado atual ordenados por `COALESCE(timestampDispositivo, recebidoEm)` | **a implementar** (hoje ordena por `recebidoEm`) |
| `ultimaLeituraEm` = hora da leitura mais recente (regra acima)                      | **a implementar** (hoje devolve `recebidoEm`)    |
| Validar `idJardim` com `^J\d{2}$` e `idSensor` com `^S\d{2}$` ou `jardim`          | **a implementar, só depois que o firmware migrar os ids** (ativar antes rejeita o firmware) |
| Migrar ids legados (`jardim-centro-01`, `sensor-0N`) no firmware, testes e exemplos | **a implementar** (firmware é do time de hardware) |
| Hora com `horaSincronizada:false` tratada como não confiável                        | **em aberto**     |
| Nome oficial único (sem alias)                                                      | **em aberto**: ponte mantida; reavaliar depois da Sprint 3 |
| Fonte de `nivelAgua` no piloto (resistivo × sonda) e `S04` para a sonda             | **a confirmar com hardware** |

## 7. Regras para quem altera o projeto (inclusive agentes)

1. Antes de criar um campo novo, procure aqui. Se o conceito existe, **use o nome existente**.
2. Para adicionar ou renomear um campo: atualize **este dicionário, o contrato e os testes no mesmo commit**.
3. Não crie nome novo para um conceito que já tem nome (ver o aviso do `timestamp`).
4. Em **payloads novos de requisição** (firmware, simulador, scripts de teste), use `idJardim`, `idSensor`, `nivelAgua` e `timestamp`. Os aliases (`jardimId`, `sensorId`, `nivelCm`) existem só por compatibilidade. Isso não define o nome oficial da resposta/banco, que segue em aberto.
5. Todo comportamento novo de contrato começa por **teste** (vermelho), depois implementação.
6. O que a seção 6 marca como "a implementar" ou "em aberto" **não pode ser assumido como verdadeiro no código**. Confira antes de depender.

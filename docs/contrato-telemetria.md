# Contrato de telemetria — Jardim Sentinela

> **Card:** [SCRUM-19] [SW-01] Definir contrato de telemetria
> **Pai:** Estruturar a frente de Arquitetura
> **Sprint:** SCRUM Sprint 1

## Por que este documento existe

O mesmo dado de telemetria vai passar por três pontas diferentes:

ESP32 (Wokwi)  ─┐
Simulador      ─┼─→  POST /api/leituras  ─→  API
Gateway        ─┘

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

{
  "idJardim": "jardim-centro-01",
  "idSensor": "sensor-01",
  "nivelAgua": 42.5,
  "bateria": 87,
  "timestamp": "2026-10-15T02:25:28Z"
}

### Regras de validação (base para o SCRUM-22)

| Campo       | Regra                                                                 |
| ----------- | ----------------------------------------------------------------------|
| `idJardim`  | não vazio                                                             |
| `idSensor`  | não vazio                                                             |
| `nivelAgua` | número, não negativo                                                  |
| `bateria`   | inteiro entre 0 e 100                                                 |
| `timestamp` | data/hora válida em ISO-8601                                          |

Campos ausentes ou fora dessas regras caracterizam **payload inválido** e devem
ser rejeitados (ver critério de aceite do SCRUM-22).

## Resolução parcial — ponte de compatibilidade (SCRUM-43 / [SW-09])

**[Atualizado — ver nota de revisão no fim do documento.]** A tabela de
divergências abaixo (mantida como histórico) estava desatualizada: o firmware
em `esp32/src/main_alt.cpp` **já emite** `idJardim`, `idSensor`, `nivelAgua`,
`bateria` e `timestamp` (função `persistir()`, linha ~179) — não mais
`molhado` nem `tMs`/`bootId`/`seq`. Essa escada de conceitos foi resolvida no
firmware antes de ser resolvida na API.

O que sobrou de divergência real, hoje, em `develop`, é só nome de campo entre
o firmware e o DTO `LeituraRequest` da API:

| Campo        | Firmware (`main_alt.cpp`) | API (`LeituraRequest`, antes) |
| ------------ | -------------------------- | ------------------------------ |
| id do jardim | `idJardim`                 | `jardimId`                      |
| id do sensor | `idSensor`                 | `sensorId`                      |
| medição      | `nivelAgua`                 | `nivelCm`                       |
| bateria      | `bateria`                   | `bateria` (já coincidia)        |
| tempo        | `timestamp`                 | não existia no DTO              |

**Solução implementada:** `LeituraRequest` passou a aceitar os dois nomes via
`@JsonAlias` (Jackson), e ganhou um campo `timestamp` opcional. O simulador
Java continua mandando `jardimId`/`nivelCm` sem timestamp e não muda uma
linha; o firmware manda `idJardim`/`nivelAgua`/`timestamp` e cai no mesmo
`POST /api/leituras`, sem `if` por origem no controller. Isso fecha o
critério de aceite do SW-09 ("ambas as fontes enviam o mesmo payload e são
processadas pelo mesmo endpoint sem código especial por origem") sem forçar
a equipe a escolher um nome vencedor hoje.

**O que isso NÃO resolve** (decisão da equipe continua em aberto):
- **Nome oficial do contrato.** `idJardim`/`nivelAgua` (firmware) e
  `jardimId`/`nivelCm` (API/banco) convivem via alias; ninguém venceu. Se a
  equipe quiser consolidar num nome só, é refatoração de renomear, não de
  reabrir a divergência.
- **Idempotência não usa `bootId`/`seq`.** Esses campos só existem no formato
  NDJSON mais rico da branch `feat/SCRUM-34-HW-03-...` (ainda não mergeada em
  `develop` — card HW-04, status "Validação" no Jira). A idempotência básica
  do SCRUM-44 [SW-10] usa `jardimId + sensorId + timestamp` porque é o que o
  firmware *de fato* envia em `develop` hoje. Quando HW-04 integrar e
  `bootId`/`seq` chegarem em `develop`, vale trocar a chave — eles são mais
  robustos que timestamp (não dependem do relógio do ESP32 estar certo).
- **Gateway/LoRa.** Este documento nunca cobriu o quadro do rádio, só o JSON
  do `POST /api/leituras`.

## Outras observações

- **Unidade do nível:** adotada em centímetros. O nome do campo (`nivelAgua`)
  não carrega a unidade; ela está documentada aqui.
- **`timestamp`:** agora existe tanto no firmware quanto no DTO (campo
  opcional). Quando ausente (hoje, o caso do simulador), a API usa o horário
  de recebimento (`recebidoEm`) como já fazia.

---

> **Nota de revisão (SCRUM-43/SCRUM-44, preparado para a sprint 2):** a seção
> "Decisão adiada" e a tabela de divergências originais citavam o firmware
> produzindo `molhado` (booleano) e nenhum campo de bateria/tempo. Isso não
> bate com o `esp32/src/main_alt.cpp` atual de `develop` — possivelmente a
> tabela foi escrita olhando uma versão anterior do firmware ou a versão da
> branch `feat/SCRUM-34-HW-03-...`, que tem um formato NDJSON mais rico e
> ainda não mergeado. **[Dado a verificar com quem escreveu a tabela
> original.]** O conteúdo acima reflete o que o código em `develop` realmente
> faz hoje.

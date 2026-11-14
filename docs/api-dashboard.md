# API para o dashboard gerado — referência rápida

> **Card:** [SCRUM-20] [SW-11] Preparar API para o dashboard gerado
> **Sprint:** SCRUM Sprint 2
> **Para quem:** Adrielly (DS-01/DS-02 — wireframe e dashboard gerado a partir do Figma)

Objetivo do card: o dashboard gerado consegue consumir a API **sem
retrabalho estrutural**. Isso aqui é a referência de endpoints + exemplos
que o código gerado (ou qualquer mock) pode usar hoje, em `develop`.

## CORS

Liberado para `/api/**`, métodos `GET`/`POST`, qualquer header. Origem
controlada pela property `jardimsentinela.cors.allowed-origins`
(`application.properties`), hoje em `*` para não travar o preview do
dashboard. **Antes de publicar fora do ambiente de desenvolvimento**, trocar
por uma lista fechada com a URL real do dashboard — "*" não deve ir pra
produção (ver `config/WebConfig.java`).

## Endpoints

### `GET /api/jardins/{id}` — estado atual de um jardim

Retorna a leitura mais recente do jardim + total de leituras já recebidas.

```bash
curl http://localhost:8080/api/jardins/J01
```

```json
{
  "jardimId": "J01",
  "sensorId": "S01",
  "nivelCm": 18.5,
  "bateria": 97,
  "ultimaLeituraEm": "2026-11-04T14:02:10.123Z",
  "totalLeituras": 42
}
```

Jardim sem nenhuma leitura ainda → **404**, corpo vazio. O dashboard precisa
tratar esse caso (jardim cadastrado mas ainda sem dado é diferente de erro).

### `GET /api/jardins/{id}/leituras?limite=N` — histórico

Mais recente primeiro. `limite` é opcional (padrão 100, máximo 1000 —
valores fora da faixa são ajustados, nunca dá erro por causa do `limite`).

```bash
curl "http://localhost:8080/api/jardins/J01/leituras?limite=5"
```

```json
[
  { "id": 42, "jardimId": "J01", "sensorId": "S01", "nivelCm": 18.5, "bateria": 97,
    "recebidoEm": "2026-11-04T14:02:10.123Z", "timestampDispositivo": "2026-11-04T14:02:09Z" },
  { "id": 41, "jardimId": "J01", "sensorId": "S01", "nivelCm": 18.1, "bateria": 97,
    "recebidoEm": "2026-11-04T14:01:40.001Z", "timestampDispositivo": null }
]
```

`timestampDispositivo` vem `null` quando a origem não manda `timestamp`
(hoje, o simulador). Jardim inexistente → **404**, array vazio não é o
mesmo caso.

### `GET /api/leituras` — todas as leituras (sem filtro por jardim)

Lista tudo que já chegou. Sem paginação ainda — para poucos jardins (J01–J04,
ver SW-08) tudo bem; se o dashboard for listar leitura crua direto daqui
com volume maior, isso é algo a revisitar, não assumir que fica rápido pra
sempre.

### `GET /api/leituras/{id}` — uma leitura específica por id numérico

404 se não existir.

### `POST /api/leituras` — ingestão (não é consumida pelo dashboard, mas documentada aqui porque é a mesma API)

Aceita tanto `{"jardimId","sensorId","nivelCm","bateria"}` (simulador) quanto
`{"idJardim","idSensor","nivelAgua","bateria","timestamp"}` (firmware atual)
— ver `docs/contrato-telemetria.md`. Retorna **201** com a leitura criada, ou
**200** com a leitura já existente se for uma retransmissão (mesmo
jardim+sensor+timestamp).

Erro de validação (campo ausente/inválido):

```json
{
  "status": 400,
  "erro": "PAYLOAD_INVALIDO",
  "mensagem": "Um ou mais campos não passaram na validação.",
  "campos": [{ "campo": "jardimId", "mensagem": "não deve estar em branco" }],
  "momento": "2026-11-04T14:05:00Z"
}
```

## Mocks para o Designer

Se o fluxo do Figma→código precisar de dado antes da API estar de pé numa
máquina acessível, os três JSONs de resposta acima (`GET /api/jardins/{id}`,
`GET /api/jardins/{id}/leituras`, erro de validação) são exemplos reais —
produzidos pelos testes automatizados (`JardimControllerTests`,
`LeituraIngestaoTests`), não inventados agora. Dá pra colar direto num mock
de API (MSW, json-server, etc.) sem esperar o backend local subir.

## Dependências deste card (ver Jira)

SW-06 e SW-07 (os dois endpoints de consulta) e DS-01 (wireframe) — os três
já estavam prontos em `develop` antes deste card; o board do Jira ainda
marcava SW-06 como "A fazer" no momento desta análise, o que parece
desatualizado frente ao código — vale confirmar com o Arthur e ajustar o
status no board.

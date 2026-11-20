# API para o dashboard gerado — referência rápida

> **Card:** [SCRUM-20] [SW-11] Preparar API para o dashboard gerado
> **Sprint:** SCRUM Sprint 2
> **Para quem:** Adrielly (DS-01/DS-02 — wireframe e dashboard gerado a partir do Figma)

Objetivo do card: o dashboard gerado consegue consumir a API **sem
retrabalho estrutural**. Isso aqui é a referência de endpoints + exemplos
que o código gerado (ou qualquer mock) pode usar hoje, em `develop`.

## CORS

Liberado para `/api/**`, métodos `GET`/`POST`, os headers `Content-Type` e `Accept`, **somente para
uma lista fechada de origens**: `http://localhost:5173` (dev do Vite) e
`http://localhost:4173` (preview do build). Origem fora da lista não recebe
`Access-Control-Allow-Origin` (o navegador bloqueia a leitura; o preflight
também é recusado). Não há mais `*`.

A lista vem da propriedade `jardimsentinela.cors.allowed-origins`
(`application.properties`), separada por vírgula. Quando houver deploy do
dashboard, acrescentar a URL dele nessa propriedade (ver `config/WebConfig.java`
e `docs/decisoes-tecnicas.md` §6.6).

## Pendências conhecidas (dívida técnica, fora do escopo da SW-11)

- `GET /teste` continua exposto (herdado; usado por `scripts/smoke-test-integracao.sh`
  e `docs/validacao-wokwi.md`). Remover ou restringir exige ajustar esses dois usos
  e é decisão da equipe.
- Log de SQL: `spring.jpa.show-sql` vem **desligado**; para depurar localmente use
  `SPRING_JPA_SHOW_SQL=true`.

## Ordenação (leitura mais recente)

Histórico e estado atual ordenam pela chave `COALESCE(timestampDispositivo,
recebidoEm)`, da mais recente para a mais antiga, com desempate por `id`
decrescente. Assim, leituras retransmitidas pela fila do dispositivo ficam na
ordem em que o evento aconteceu, não na ordem em que chegaram. Por isso
`ultimaLeituraEm` é a data (pela mesma chave) da leitura mais recente.

## Endpoints

### `GET /api/jardins` — lista de jardins com resumo

Um item por jardim que já enviou ao menos uma leitura, ordenado por `jardimId`.
Jardim sem leitura **não aparece** (sem dados, a resposta é `[]`, status 200).
Cada item traz a leitura mais recente segundo a chave de ordenação acima.
A lista é montada com 2 queries fixas (última leitura por jardim + contagem
agrupada), independentemente do número de jardins (coberto por
`ListaJardinsQueriesTests`).

```bash
curl http://localhost:8080/api/jardins
```

```json
[
  { "jardimId": "J01", "nivelCm": 24.0, "estado": "ENCHENDO",
    "ultimaLeituraEm": "2026-11-19T10:00:00Z", "totalLeituras": 1 },
  { "jardimId": "J02", "nivelCm": 10.5, "estado": null,
    "ultimaLeituraEm": "2026-11-19T10:00:05Z", "totalLeituras": 1 }
]
```

`estado` é `null` quando o registro não traz classificação (ver abaixo). A lista
mostra todos os ids que já enviaram dados, inclusive ids legados do firmware; a
validação de formato (`^J\d{2}$`) ainda não está ligada.

### `GET /api/jardins/{id}` — estado atual de um jardim

Retorna a leitura mais recente do jardim + total de leituras já recebidas.

```bash
curl http://localhost:8080/api/jardins/J01
```

```json
{
  "jardimId": "J01",
  "sensorId": "S01",
  "nivelCm": 24.0,
  "bateria": 100,
  "ultimaLeituraEm": "2026-11-19T10:00:00Z",
  "totalLeituras": 1,
  "estado": "ENCHENDO",
  "tendencia": null
}
```

- `estado`: vem do registro mais recente, como o dispositivo enviou (`NORMAL`,
  `ENCHENDO`, `TRANSBORDANDO` ou `ALAGADO`). A API **não recalcula**. É `null`
  em registros antigos, sem classificação.
- `tendencia`: será calculada pela API (SW-14). O campo já existe, mas hoje
  vem sempre `null`.

Jardim sem nenhuma leitura ainda → **404** com corpo JSON padronizado
(`erro: "JARDIM_NAO_ENCONTRADO"`, ver "Erros" abaixo). O dashboard precisa
tratar esse caso (jardim cadastrado mas ainda sem dado é diferente de erro).

### `GET /api/jardins/{id}/leituras?limite=N` — histórico

Mais recente primeiro (chave de ordenação acima). `limite` é opcional (padrão 100, máximo 1000 —
valores fora da faixa são ajustados, nunca dá erro por causa do `limite`).

```bash
curl "http://localhost:8080/api/jardins/J01/leituras?limite=5"
```

```json
[
  { "id": 1, "jardimId": "J01", "sensorId": "S01", "nivelCm": 24.0, "bateria": 100,
    "recebidoEm": "2026-11-19T05:34:46.338720Z", "timestampDispositivo": "2026-11-19T10:00:00Z",
    "estado": "ENCHENDO", "tendenciaDispositivo": "SUBINDO", "versaoRegra": "v1-provisoria-2026-11" }
]
```

`timestampDispositivo` vem `null` quando a origem não manda `timestamp`
(hoje, o simulador); nesse caso a ordenação usa `recebidoEm`. `estado`, `tendenciaDispositivo` e `versaoRegra` vêm `null` em registros sem classificação. Jardim sem nenhuma leitura → **404** com o mesmo corpo
`JARDIM_NAO_ENCONTRADO` do endpoint acima; array vazio não é o mesmo caso.

### `GET /api/leituras` — todas as leituras (sem filtro por jardim)

Lista tudo que já chegou. Sem paginação ainda — para poucos jardins (J01–J04,
ver SW-08) tudo bem; se o dashboard for listar leitura crua direto daqui
com volume maior, isso é algo a revisitar, não assumir que fica rápido pra
sempre.

### `GET /api/leituras/{id}` — uma leitura específica por id numérico

404 se não existir, **com corpo vazio** (diferente dos endpoints de
`/api/jardins`, que devolvem o corpo JSON de erro).

### `POST /api/leituras` — ingestão (não é consumida pelo dashboard, mas documentada aqui porque é a mesma API)

Aceita tanto `{"jardimId","sensorId","nivelCm","bateria"}` (simulador) quanto
`{"idJardim","idSensor","nivelAgua","bateria","timestamp"}` (firmware atual)
— ver `docs/contrato-telemetria.md`. Retorna **201** com a leitura criada, ou
**200** com a leitura já existente se for uma retransmissão (mesmo
jardim+sensor+timestamp).

Campos **opcionais** novos, enviados pelo dispositivo:

| Campo | Valores aceitos |
|---|---|
| `estado` | `NORMAL`, `ENCHENDO`, `TRANSBORDANDO`, `ALAGADO` |
| `tendenciaDispositivo` | `SUBINDO`, `ESTAVEL`, `DESCENDO`, `INSUFICIENTE`, `INDETERMINADO`, `DESATUALIZADO` |
| `versaoRegra` | texto, até 64 caracteres (ex.: `v1-provisoria-2026-11`) |

Se vierem, são **guardados como vieram e não são recalculados**. Se não vierem,
ficam `null`. Valor fora da lista (ou `versaoRegra` acima de 64 caracteres)
retorna **400** `PAYLOAD_INVALIDO`.

Exemplo de resposta (201) para um envio com os campos novos:

```json
{
  "id": 1, "jardimId": "J01", "sensorId": "S01", "nivelCm": 24.0, "bateria": 100,
  "recebidoEm": "2026-11-19T05:34:46.338719902Z", "timestampDispositivo": "2026-11-19T10:00:00Z",
  "estado": "ENCHENDO", "tendenciaDispositivo": "SUBINDO", "versaoRegra": "v1-provisoria-2026-11"
}
```

Sem os campos novos, a resposta traz `"estado": null`, `"tendenciaDispositivo": null`
e `"versaoRegra": null`.

Exemplo de erro por `estado` fora da lista (`"estado":"SECO"`):

```json
{
  "status": 400,
  "erro": "PAYLOAD_INVALIDO",
  "mensagem": "Um ou mais campos não passaram na validação.",
  "campos": [{ "campo": "estado", "mensagem": "deve ser um de NORMAL, ENCHENDO, TRANSBORDANDO, ALAGADO" }],
  "momento": "2026-11-19T05:34:46.427200299Z"
}
```

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

Jardim sem nenhuma leitura (`GET /api/jardins/{id}` e
`GET /api/jardins/{id}/leituras`), **404**:

```json
{
  "status": 404,
  "erro": "JARDIM_NAO_ENCONTRADO",
  "mensagem": "Não existe leitura registrada para o jardim 'J99'.",
  "campos": [],
  "momento": "2026-11-04T14:05:00Z"
}
```

## Mocks para o Designer

Se o fluxo do Figma→código precisar de dado antes da API estar de pé numa
máquina acessível, os JSONs de resposta acima (`GET /api/jardins`, `GET /api/jardins/{id}`,
`GET /api/jardins/{id}/leituras`, erros) são exemplos reais —
produzidos pelos testes automatizados (`JardimControllerTests`, `JardimOrdenacaoTests`,
`ListaJardinsTests`, `LeituraCamposEstadoTests`, `LeituraIngestaoTests`), não inventados agora. Dá pra colar direto num mock
de API (MSW, json-server, etc.) sem esperar o backend local subir.

## Dependências deste card (ver Jira)

SW-06 e SW-07 (os dois endpoints de consulta) e DS-01 (wireframe) — os três
já estavam prontos em `develop` antes deste card; o board do Jira ainda
marcava SW-06 como "A fazer" no momento desta análise, o que parece
desatualizado frente ao código — vale confirmar com o Arthur e ajustar o
status no board.

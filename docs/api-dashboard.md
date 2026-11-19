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

Jardim sem nenhuma leitura ainda → **404** com corpo JSON padronizado
(`erro: "JARDIM_NAO_ENCONTRADO"`, ver "Erros" abaixo). O dashboard precisa
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
(hoje, o simulador). Jardim sem nenhuma leitura → **404** com o mesmo corpo
`JARDIM_NAO_ENCONTRADO` do endpoint acima; array vazio não é o mesmo caso.

### `GET /api/jardins/{id}/tendencia` — o jardim está enchendo, estável ou drenando?

[SCRUM-28][SW-14] Endpoint **novo e aditivo**: não altera `GET /api/jardins/{id}` nem
`GET /api/jardins/{id}/leituras`. Calcula a direção da variação do nível a partir das
**5 leituras válidas mais recentes**: compara a última com a primeira, dentro de uma
tolerância. Detalhes e decisões em `docs/tendencia-nivel.md`.

```bash
curl http://localhost:8080/api/jardins/J01/tendencia
```

```json
{
  "jardimId": "J01",
  "tendencia": "ENCHENDO",
  "leiturasValidas": 5,
  "leiturasDescartadas": 0,
  "janelaLeituras": 5,
  "toleranciaCm": 1.5,
  "confiabilidadeCronologia": "NAO_VALIDADA",
  "semTimestampDispositivoNaJanela": false,
  "multiplosSensorId": false,
  "ultimaRecepcaoEm": "2026-11-19T12:00:05.123Z"
}
```

| Campo | Significado |
|---|---|
| `jardimId` | Jardim consultado |
| `tendencia` | Um dos 6 estados (tabela abaixo) |
| `leiturasValidas` | Leituras usadas na janela (no máximo 5; mostra "3 de 5" no `INSUFICIENTE`) |
| `leiturasDescartadas` | Leituras inválidas (nível nulo, `NaN`, infinito, negativo, ou sem nenhuma data) **examinadas** ao montar a janela. Não é o total de inválidas no banco |
| `janelaLeituras` | Tamanho da janela (5) |
| `toleranciaCm` | Tolerância usada, em cm. Vem da configuração do backend, **não** da requisição |
| `confiabilidadeCronologia` | Sempre `"NAO_VALIDADA"` neste MVP (ver avisos) |
| `semTimestampDispositivoNaJanela` | `true` se alguma leitura da janela não tem `timestampDispositivo` (vale o `recebidoEm`). Sinal informativo |
| `multiplosSensorId` | `true` se a janela mistura mais de um `sensorId`. Sinal informativo |
| `ultimaRecepcaoEm` | Maior `recebidoEm` entre as leituras válidas: quando a **API** recebeu algo pela última vez. Pode ser `null` |

Os 6 valores de `tendencia`:

| Valor | Quando | Sugestão para o dashboard |
|---|---|---|
| `ENCHENDO` | A variação líquida da janela passou da tolerância, para cima | Nível subindo |
| `ESTAVEL` | A variação líquida ficou dentro da tolerância (inclusive no limite) | Sem tendência clara. **Não** mostrar como "nível parado" (ver avisos) |
| `DRENANDO` | A variação líquida passou da tolerância, para baixo | Nível descendo |
| `INSUFICIENTE` | Menos de 5 leituras válidas | "Poucos dados" (`leiturasValidas` diz quantos) |
| `INDETERMINADO` | Há leituras com o mesmo horário e as ordens possíveis levariam a classificações diferentes | "Não dá para determinar" |
| `DESATUALIZADO` | A última recepção é mais antiga que o limite de idade (30 s por padrão) | "Sem dados recentes" |

Quando mais de uma situação vale, a precedência é: `INSUFICIENTE`, depois
`DESATUALIZADO`, depois `INDETERMINADO`, depois a tendência. **Os 6 estados respondem
HTTP 200.** Jardim que nunca enviou nenhuma leitura → **404** com o mesmo corpo
`JARDIM_NAO_ENCONTRADO` dos outros endpoints; jardim com leituras, mesmo que todas
inválidas, responde 200 (`INSUFICIENTE`).

**Avisos que o dashboard precisa respeitar:**

- **Não é previsão de enchente nem alerta oficial da Defesa Civil.** Só diz a direção da
  variação observada do nível.
- `ESTAVEL` significa que a diferença **entre a primeira e a última** leitura ficou
  dentro da tolerância. O nível pode ter oscilado bastante no meio (`10 → 15 → 10`
  é `ESTAVEL`).
- **Valores experimentais:** a janela (5), a tolerância (1,5 cm) e a idade máxima (30 s)
  vêm de experimentos com o **simulador**. **Não estão calibrados para o hardware real.**
- `ultimaRecepcaoEm` mede o silêncio do dispositivo (quando a API **recebeu**), não a idade
  real da medição: uma retransmissão de leituras antigas tem `ultimaRecepcaoEm` recente.
  `null` quer dizer "atualidade desconhecida" (nenhuma leitura válida com `recebidoEm`):
  **nunca** exibir como "dado atual".
- `confiabilidadeCronologia: "NAO_VALIDADA"`: a ordem das leituras usa o
  `timestampDispositivo` (ou o `recebidoEm` se faltar), mas a API ainda não persiste
  `bootId`, `seq` e `horaSincronizada`, então não consegue validar a cronologia (um
  reinício do ESP32 pode inverter a ordem real). Os sinais informativos **não** provam
  confiabilidade.
- A ordem da tendência (pelo horário da medição) **pode divergir** da de
  `GET /api/jardins/{id}/leituras` (por `recebidoEm`) até as duas serem unificadas.
- Trate valores de `tendencia` desconhecidos de forma segura: novos estados podem surgir.

**Configuração** (sem alterar código; `application.properties`):
`jardimsentinela.tendencia.tolerancia-cm` (padrão `1.5`),
`jardimsentinela.tendencia.idade-maxima` (padrão `30s`) e
`jardimsentinela.tendencia.tamanho-pagina` (padrão `50`; só muda o número de consultas,
não o resultado).

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

Jardim sem nenhuma leitura (`GET /api/jardins/{id}`,
`GET /api/jardins/{id}/leituras` e `GET /api/jardins/{id}/tendencia`), **404**:

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

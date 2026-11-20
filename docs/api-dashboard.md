# API para o dashboard gerado — referência rápida

> **Card:** [SCRUM-20] [SW-11] Preparar API para o dashboard gerado
> **Sprint:** SCRUM Sprint 2
> **Para quem:** Adrielly (DS-01/DS-02 — wireframe e dashboard gerado a partir do Figma)

Objetivo do card: o dashboard gerado consegue consumir a API **sem
retrabalho estrutural**. Isso aqui é a referência de endpoints + exemplos
que o código gerado (ou qualquer mock) pode usar hoje, em `develop`.

## CORS

Liberado para `/api/**`, métodos `GET`/`POST`, qualquer header, **somente para
uma lista fechada de origens**: `http://localhost:5173` (dev do Vite) e
`http://localhost:4173` (preview do build). Origem fora da lista não recebe
`Access-Control-Allow-Origin` (o navegador bloqueia a leitura; o preflight
também é recusado). Não há mais `*`.

A lista vem da propriedade `jardimsentinela.cors.allowed-origins`
(`application.properties`), separada por vírgula. Quando houver deploy do
dashboard, acrescentar a URL dele nessa propriedade (ver `config/WebConfig.java`
e `docs/decisoes-tecnicas.md` §6.6).

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
  "tendencia": "INSUFICIENTE"
}
```

- `estado`: vem do registro mais recente, como o dispositivo enviou (`NORMAL`,
  `ENCHENDO`, `TRANSBORDANDO` ou `ALAGADO`). A API **não recalcula**. É `null`
  em registros antigos, sem classificação.
- `tendencia`: calculada pela API (SW-14), com a **mesma regra** de
  `GET /api/jardins/{id}/tendencia`: `SUBINDO`, `ESTAVEL`, `DESCENDO`, `INSUFICIENTE`,
  `INDETERMINADO` ou `DESATUALIZADO`. No exemplo, a única leitura é menos que as 5 da
  janela. Não é gravada no banco: é calculada a cada consulta. Não é alerta oficial.

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

### `GET /api/jardins/{id}/tendencia` — o nível está subindo, estável ou descendo?

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
  "tendencia": "SUBINDO",
  "leiturasValidas": 5,
  "leiturasDescartadas": 0,
  "janelaLeituras": 5,
  "toleranciaCm": 1.5,
  "idadeMaximaSegundos": 180,
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
| `idadeMaximaSegundos` | Idade máxima usada para `DESATUALIZADO`, em segundos (180 por padrão). Vem da configuração. PROVISÓRIO |
| `confiabilidadeCronologia` | Sempre `"NAO_VALIDADA"` neste MVP (ver avisos) |
| `semTimestampDispositivoNaJanela` | `true` se alguma leitura da janela não tem `timestampDispositivo` (vale o `recebidoEm`). Sinal informativo |
| `multiplosSensorId` | `true` se a janela mistura mais de um `sensorId`. Sinal informativo |
| `ultimaRecepcaoEm` | Maior `recebidoEm` entre as leituras válidas: quando a **API** recebeu algo pela última vez. Pode ser `null` |

Os 6 valores de `tendencia`:

| Valor | Quando | Sugestão para o dashboard |
|---|---|---|
| `SUBINDO` | A variação líquida da janela passou da tolerância, para cima | Nível subindo |
| `ESTAVEL` | A variação líquida ficou dentro da tolerância (inclusive no limite) | Sem tendência clara. **Não** mostrar como "nível parado" (ver avisos) |
| `DESCENDO` | A variação líquida passou da tolerância, para baixo | Nível descendo |
| `INSUFICIENTE` | Menos de 5 leituras válidas | "Poucos dados" (`leiturasValidas` diz quantos) |
| `INDETERMINADO` | Há leituras com o mesmo horário e as ordens possíveis levariam a classificações diferentes | "Não dá para determinar" |
| `DESATUALIZADO` | A última recepção é mais antiga que o limite de idade (180 s por padrão; 15 s no perfil `simulacao`) | "Sem dados recentes" |

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
- **Valores PROVISÓRIOS** (ver `docs/decisoes-tecnicas.md` §5.2): a janela (5), a tolerância
  (1,5 cm) e a idade máxima (180 s = 3 vezes o snapshot de 60 s do hardware; 15 s na
  simulação, com snapshot de 5 s). **Não estão calibrados com dados de campo.**
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
`jardimsentinela.tendencia.idade-maxima` (padrão `180s`; o perfil `simulacao`, com
`spring.profiles.active=simulacao`, usa `15s`) e
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

# Integração do dashboard com a API

Este documento registra o que o frontend consome hoje, como os campos da API
são mapeados e **o que ainda falta no backend** para o critério de aceite
funcionar de ponta a ponta com dados reais.

## Dependência de backend (pendência)

O contrato completo usado por esta integração existe na branch
`feat/SCRUM-28-tendencia-api`, que **ainda não foi integrada a `develop`**.

Em `develop`, hoje:

| Recurso | Situação em `develop` |
|---|---|
| `GET /api/jardins` (lista de resumos) | **não existe** |
| `GET /api/jardins/{id}` com `estado` | **não existe** (o DTO não tem o campo) |
| `GET /api/jardins/{id}` com `tendencia` | **não existe** |
| `GET /api/jardins/{id}/tendencia` | **não existe** |

Consequência: o frontend já está pronto para o contrato documentado, mas, rodando
contra `develop`, a **tendência aparecerá como "Indisponível"** e o `estado`
será derivado do nível (sem classificação vinda do dispositivo). Para o
critério de aceite — J01–J04 com nível, estado, tendência e última atualização
**da API** — é necessário integrar a branch `feat/SCRUM-28-tendencia-api` (ou
equivalente) em `develop`.

Este trabalho **não cria nem altera o backend**; a pendência acima é de
integração da API.

## Endpoints consumidos

| Endpoint | Uso no dashboard | Observação |
|---|---|---|
| `GET /api/jardins/{id}` | card de status/dados do jardim selecionado e visão J01–J04 | é o endpoint que traz `estado` e `tendencia` |
| `GET /api/jardins/{id}/leituras?limite=N` | gráfico de histórico | leituras reais da API |
| `GET /api/jardins` | disponível em `fetchJardins()` | a visão J01–J04 usa o detalhe porque a lista não traz `tendencia` |
| `GET /api/leituras` | disponível em `fetchTodasLeituras()` | não é usado na tela principal |

`GET /api/jardins/{id}` é chamado para **J01, J02, J03 e J04 em paralelo**, a
cada 5 segundos. Uma falha em um jardim não impede a exibição dos outros: cada
card tem seu próprio estado de erro.

## Configuração da URL

A URL base vem de `VITE_API_BASE_URL` (padrão `http://localhost:8080`). O
arquivo `.env.example` documenta a variável; para sobrescrever localmente, crie
`.env.local`:

```bash
VITE_API_BASE_URL=http://localhost:8080
```

O backend precisa liberar a origem do dashboard no CORS. Na branch de tendência,
as origens liberadas são `http://localhost:5173` (dev) e
`http://localhost:4173` (preview do build).

## Mapeamento de respostas

### Estado

| API (`estado`) | Dashboard (`NivelAlerta`) |
|---|---|
| `NORMAL` | `SECO` |
| `ENCHENDO` | `ENCHENDO` |
| `TRANSBORDANDO` | `TRANSBORDANDO` |
| `ALAGADO` | `ALAGADO` |
| `null`/ausente | derivado da faixa de `nivelCm` |

O `estado` enviado pela API tem prioridade sobre a faixa calculada pelo nível.
A derivação local só existe para registros antigos sem classificação.

### Tendência

`tendencia` é exibida como veio da API: `SUBINDO`, `ESTAVEL`, `DESCENDO`,
`INSUFICIENTE`, `INDETERMINADO` ou `DESATUALIZADO`.

- `null`, ausente ou valor desconhecido → **"Indisponível"**. O frontend **não**
  calcula tendência localmente para substituir a API.
- `ESTAVEL` é descrito como variação dentro da tolerância, **não** como nível
  parado.
- A tendência **não é previsão de enchente nem alerta oficial**.

### Última atualização

`ultimaLeituraEm` é formatado em data/hora local (`pt-BR`) nos cards e no
detalhe.

## Loading e erro

- Enquanto a primeira consulta não termina, a visão J01–J04 mostra esqueletos
  de carregamento e o detalhe mostra os cards em skeleton.
- Em erro de rede/timeout (`ApiError` com `codigo: "REDE"` ou `"TIMEOUT"`), a
  interface mostra aviso com **"Tentar novamente"**; não há mais fallback
  silencioso para mock.
- Jardim sem leitura (`404` / `JARDIM_NAO_ENCONTRADO`) aparece como
  **"Sem leituras recebidas"** na visão de rede, sem derrubar os demais.

## O que continua mockado

Somente o **painel COP**, porque ainda não há endpoint correspondente na API.
Jardins, estado, tendência e histórico não usam mais mock.

## Testes

```bash
cd frontend
npm test        # Vitest + Testing Library (27 testes)
npm run lint
npm run build
```

Os testes cobrem:

- mapeamento `NORMAL` → `SECO` e fallback pelo nível quando `estado` é nulo;
- normalização das seis tendências e tratamento de valor desconhecido;
- cliente HTTP (URLs, 404 com código, timeout, erro de rede e ausência de
  fallback para mock);
- hooks de detalhe e da visão J01–J04 (sucesso, falha parcial e falha total);
- renderização de J01–J04 com nível, estado, tendência e última atualização;
- fluxo da `App` com a API mockada, incluindo estado/tendência do jardim em foco
  e erro acionável quando a API está fora do ar.

A validação visual foi feita com um servidor temporário que segue este contrato
(fora do repositório) e o Vite, já que o ambiente não possui Java/Maven para
subir a API Spring Boot real.

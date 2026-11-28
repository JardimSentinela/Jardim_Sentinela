# Integração do dashboard com a API

Este documento registra o que o frontend consome hoje, como os campos da API
são mapeados e qual contrato é usado para o critério de aceite funcionar de
ponta a ponta com dados reais.

## Dependência de backend

O contrato completo usado por esta integração já está em `develop`: a API expõe
`estado` e `tendencia` em `GET /api/jardins/{id}`, além de
`GET /api/jardins/{id}/tendencia`. Rodando esta branch com a API Spring Boot, a
interface recebe os campos reais, sem fallback silencioso para mock.

O frontend não cria nem altera o backend. Se a API estiver em uma revisão
anterior ao contrato, a tendência aparece como "Indisponível" e o `estado` é
derivado do nível; isso é apenas compatibilidade, não o cenário esperado.

## Endpoints consumidos

| Endpoint | Uso no dashboard | Observação |
|---|---|---|
| `GET /api/jardins/{id}` | card de status/dados do jardim selecionado e visão J01–J04 | é o endpoint que traz `estado` e `tendencia` |
| `GET /api/jardins/{id}/leituras?limite=N&incluirBoot=false` | gráfico de histórico | o dashboard exclui o registro `boot`; o padrão do backend continua `true` |
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

O backend precisa liberar a origem do dashboard no CORS. As origens liberadas
são `http://localhost:5173` (dev) e `http://localhost:4173` (preview do build).

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
  interface mostra aviso com **"Tentar novamente"**; não há fallback silencioso
  para mock.
- Uma falha transitória **preserva o último dado válido**: nível, estado,
  tendência e gráfico continuam na tela com o aviso de que a atualização falhou.
  Os dados só são limpos ao trocar de jardim.
- O erro do detalhe (`erroJardim`) e o erro do histórico (`erroHistorico`) são
  separados. Uma falha no histórico não esconde o status do jardim; uma falha no
  detalhe não esconde o histórico já carregado. O badge de conexão acompanha o
  detalhe.
- Jardim sem leitura (`404` / `JARDIM_NAO_ENCONTRADO`) é **estado vazio**, não
  erro: o detalhe mostra **"Sem leituras recebidas"** e o gráfico mostra
  **"Sem leituras para exibir"**, sem banner vermelho. Na visão de rede, o card
  do jardim também mostra "Sem leituras recebidas", sem derrubar os demais.
- O histórico do dashboard pede `incluirBoot=false`: o registro de
  inicialização (`tipo: "boot"`) tem nível fictício e não entra no gráfico nem
  no CSV. O padrão do endpoint continua `true` para não quebrar outros
  consumidores.
- O dashboard carrega até **100** leituras (padrão da API). A janela
  **"Todas as leituras carregadas"** mostra o que foi carregado, e o cabeçalho
  diferencia "X de Y" quando a janela de 10/20 limita a exibição.
- Requisições antigas são invalidadas por um contador: alternar J01 → J02 → J01
  não deixa uma resposta atrasada de J01 sobrescrever a consulta mais recente.

## O que continua mockado

Somente o **painel COP**, porque ainda não há endpoint correspondente na API.
Jardins, estado, tendência e histórico não usam mais mock.

## Testes

```bash
cd frontend
npm test        # Vitest + Testing Library (50 testes)
npm run lint
npm run build
```

Os testes cobrem:

- mapeamento `NORMAL` → `SECO` e fallback pelo nível quando `estado` é nulo;
- normalização das seis tendências e tratamento de valor desconhecido;
- cliente HTTP (URLs, 404 com código, `incluirBoot=false`, timeout, erro de rede
  e ausência de fallback para mock);
- hooks de detalhe e da visão J01–J04 (sucesso, falha parcial e falha total);
- `useJardimData`: sucesso → falha → mantém jardim e histórico; 404 como
  `semLeituras`; erros de detalhe e histórico separados; corrida J01 → J02 →
  J01;
- renderização de J01–J04 com nível, estado, tendência e última atualização;
- fluxo da `App` com a API mockada, incluindo estado/tendência do jardim em foco,
  conteúdo do gráfico ao trocar para J02, 404 como estado vazio e erro acionável
  quando a API está fora do ar;
- `HistoricoChart`: ordem cronológica no CSV, domínio do eixo Y acima de 50 cm e
  janela "Todas" com mais de 30 leituras.

No backend, `JardimControllerTests` cobre o histórico com e sem `boot` e o caso
de jardim que só tem boot (lista vazia, não 404).

A validação automatizada desta branch está registrada em
`docs/ajustes-sw-18.md`, com os comandos para subir a API real e o dashboard e o
checklist manual.

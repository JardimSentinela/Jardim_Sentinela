# SW-14 — Identificar tendência de nível (SCRUM-28)

> **Estado deste documento (atualizado em 2026-11-19):** registra as **decisões de
> domínio e técnicas fechadas**. A implementação está **em andamento**. **Implementado e
> testado:** o enum `Tendencia`, os records `LeituraNivel` e `ResultadoTendencia` e a
> **função pura `CalculadoraTendencia`** (validação, janela, classificação, empates,
> atualidade e precedência), desenvolvida com TDD (ver seção 14). **Ainda planejado, não
> implementado:** a consulta no repositório, o serviço, o controller, o DTO, as
> propriedades de configuração, o `Clock` e o endpoint (seções 11 e 12). O andamento está
> em [`SCRUM-28-handoff.md`](SCRUM-28-handoff.md).

Legenda de certeza usada neste documento: **[Certo]** confirmado no código ou nos
dados, **[Provável]** inferência técnica, **[Suposição]** proposta ainda não validada,
**[Dado a verificar]** pendência.

---

## 1. Objetivo e escopo

**História:** como operador, quero saber se o jardim está enchendo, estável ou
drenando.

**Critério de aceite:** o sistema calcula a tendência a partir de leituras
consecutivas e retorna resultado consistente.

**Dependência:** SW-07 (SCRUM-40), histórico de leituras, já concluída.

**Fora do escopo:**

- Tendência **não** é previsão de enchente nem alerta oficial da Defesa Civil.
- A **SW-13** (classificação do estado físico do jardim) permanece independente e não
  é alterada. [Certo] Não há implementação da SW-13 no repositório.
- A ingestão (`POST /api/leituras`), o simulador e o firmware não são alterados.

---

## 2. Resumo das decisões

| # | Decisão | Valor |
|---|---|---|
| D1 | Algoritmo | Variação líquida: `última − primeira` da janela, comparada à tolerância |
| D2 | Tamanho da janela | **N = 5 leituras** (4 intervalos) |
| D3 | Tolerância | **1,5 cm**, absoluta, configurável, recebida como argumento pela função pura |
| D4 | Limite | `ESTAVEL` quando `|variação| <= tolerância` (inclusivo), comparação exata com `BigDecimal` |
| D5 | Estados | 6: `ENCHENDO`, `ESTAVEL`, `DRENANDO`, `INSUFICIENTE`, `INDETERMINADO`, `DESATUALIZADO` |
| D6 | Precedência | `INSUFICIENTE → DESATUALIZADO → INDETERMINADO → tendência calculada` |
| D7 | Cronologia (C1) | `timestampDispositivo`, com `recebidoEm` quando ausente; **nunca** afirmada como validada |
| D8 | Empates (A3) | Empate = conjunto de possibilidades; sem desempate por `id` ou por nível |
| D9 | Série (F1) | Uma série por jardim, todas as leituras, sem tendência por sensor |
| D10 | Leitura válida | `nivelCm` não nulo, finito e `>= 0`, **e** com alguma referência temporal (`timestampDispositivo` ou `recebidoEm`); inválidas são descartadas e contadas |
| D11 | Histórico insuficiente | Menos de 5 leituras válidas → `INSUFICIENTE` (HTTP 200); 404 só sem nenhuma leitura |
| D12 | Idade | Limite provisório de **30 s**, configurável, sobre `recebidoEm`; exatamente 30 s ainda é atual |
| D13 | Endpoint | `GET /api/jardins/{id}/tendencia`, DTO próprio (planejado) |
| D14 | Fonte dos dados (P3) | Consulta nova só para a SW-14; SW-06 e SW-07 inalteradas; divergência documentada |
| D15 | Testes | Séries construídas à mão; simulador só para observação e regressão; **TDD (RED → GREEN → REFACTOR) obrigatório** |

**Todos os parâmetros numéricos (N, tolerância, idade) são experimentais, definidos a
partir do simulador, e não estão calibrados para o hardware real.**

---

## 3. Algoritmo: variação líquida (D1, D2, D3, D4)

Dada uma janela de N = 5 leituras em ordem cronológica `n1 … n5`:

```
variação = n5 − n1            (positivo = o nível subiu)

variação >  +tolerância  →  ENCHENDO
variação <  −tolerância  →  DRENANDO
caso contrário           →  ESTAVEL     (|variação| <= tolerância)
```

- Só a **primeira e a última** leitura da janela entram na conta. As três do meio não
  influenciam o resultado.
- A janela é por **quantidade de leituras** (5 leituras = 4 intervalos), não por tempo.
- O limite é **inclusivo**: variação exatamente igual à tolerância é `ESTAVEL`.
- A comparação usa `BigDecimal.valueOf(double)`. Em `double`, `1,1 − 1,0` dá
  `0,10000000000000009`, e uma variação "exatamente igual à tolerância" poderia ser
  classificada como acima dela. `BigDecimal.valueOf` usa a representação decimal
  textual do `double`, sem assumir uma quantidade fixa de casas e sem arredondar.

### O que `ESTAVEL` significa (e o que não significa)

`ESTAVEL` quer dizer que a **variação líquida** entre a primeira e a última leitura
ficou dentro da tolerância. **Não** quer dizer que o nível permaneceu constante durante
a janela. Exemplo: `10 → 15 → 10 → 15 → 10` tem variação líquida 0 e é `ESTAVEL`,
embora o nível tenha oscilado 5 cm. O dashboard **não deve** apresentar `ESTAVEL` como
"nível parado".

Uma medida complementar de oscilação (`máximo − mínimo` da janela) é uma **evolução
futura**, sem estado novo associado (ver seção 15).

### Abordagens descartadas para o MVP

Movimento consistente (variantes em que todas as variações entre leituras vizinhas
precisam apontar na mesma direção): descartado por exigir um estado de "oscilando" no
enum, dois usos da tolerância e muito mais casos de teste.

---

## 4. Parâmetros

| Parâmetro | Valor | Como é definido | Estado |
|---|---|---|---|
| Janela N | 5 leituras | constante `CalculadoraTendencia.TAMANHO_JANELA` | **implementado** |
| Tolerância | 1,5 cm | propriedade de configuração; recebida como argumento pela função pura | argumento da função **implementado** (rejeita valor negativo, `NaN` ou infinito); propriedade planejada |
| Idade máxima | 30 s | propriedade de configuração; recebida como argumento pela função pura (junto com o "agora") | argumento da função **implementado** (rejeita valor negativo); propriedade planejada |

Os **nomes das propriedades** ainda não foram definidos [Suposição]: seguir o padrão
`jardimsentinela.*` já usado em `application.properties` (por exemplo
`jardimsentinela.cors.allowed-origins`). A tolerância **não** será um parâmetro da
requisição, para que os clientes não escolham o limiar.

---

## 5. Os seis estados (D5) e a precedência (D6)

| Estado | Definição |
|---|---|
| `ENCHENDO` | A variação líquida da janela passou da tolerância, para cima |
| `ESTAVEL` | `|variação líquida| <= tolerância` (não implica nível constante) |
| `DRENANDO` | A variação líquida da janela passou da tolerância, para baixo |
| `INSUFICIENTE` | Menos de 5 leituras válidas: não há o que calcular |
| `INDETERMINADO` | A ordem das leituras nas pontas da janela não é definida pelos dados (empate no horário) e as ordens possíveis dão classificações diferentes |
| `DESATUALIZADO` | A leitura recebida mais recentemente é mais antiga que o limite de idade |

**Precedência** quando mais de uma situação vale ao mesmo tempo:

```
INSUFICIENTE → DESATUALIZADO → INDETERMINADO → ENCHENDO / ESTAVEL / DRENANDO
```

Sem 5 leituras válidas nada mais se calcula; se os dados estão velhos, o empate
deixa de importar.

**Um único campo** carrega classificação e situação do cálculo, então não existe
combinação inválida. Contrapartida: **novos estados futuros** podem aparecer; os
clientes devem tratar valores desconhecidos de forma segura.

**Implementado** (pacote `br.edu.cesar.jardimsentinela.tendencia`): o enum `Tendencia`
com os seis valores e a documentação acima; o record `LeituraNivel` (entrada da função
pura, sem `id`, independente da entidade JPA `Leitura`); o record `ResultadoTendencia`
(`tendencia`, `leiturasValidas`, `leiturasDescartadas`, `semTimestampDispositivoNaJanela`,
`multiplosSensorId`, `ultimaRecepcaoEm`); e a função pura `CalculadoraTendencia.calcular`.
A conversão `Leitura` → `LeituraNivel` ficará no serviço (Etapa 6) e não pode descartar,
arredondar ou substituir valores inválidos.

---

## 6. Leituras válidas e histórico insuficiente (D10, D11)

**Leitura válida:** `nivelCm` não nulo, finito (não `NaN`, não infinito) e `>= 0`, **e**
com ao menos uma referência temporal: `timestampDispositivo` ou `recebidoEm`.

- Leituras inválidas são **descartadas e contabilizadas**, sem conversão silenciosa.
- **Regra acrescentada (2026-11-19):** uma leitura com `timestampDispositivo` **e**
  `recebidoEm` ambos nulos é inválida: não há como posicioná-la cronologicamente, então
  não pode participar do cálculo. **Não** se converte a ausência em data artificial.
  [Certo] Pela ingestão, `recebidoEm` é sempre preenchido pelo servidor; a regra protege
  contra dados anômalos.
- `leiturasDescartadas` conta as leituras inválidas **examinadas** durante a recuperação
  e o cálculo; **não** é o total de registros inválidos existentes no banco.
- `leiturasValidas` é a quantidade de leituras efetivamente **usadas na janela**, no
  máximo N = 5 (com menos de 5 válidas, é a quantidade disponível). Em empates, a análise
  de possibilidades pode considerar mais candidatos sem aumentar esta contagem. Isso
  fecha a pendência da semântica de `leiturasValidas` para a função pura.
- Os sinais informativos (`semTimestampDispositivoNaJanela`, `multiplosSensorId`) são
  calculados sobre os **candidatos** da janela (incluindo todos os membros de um grupo
  empatado no corte) e, com menos de 5 válidas, sobre as válidas disponíveis (janela
  parcial). Leituras descartadas não ligam sinal algum.
- `ultimaRecepcaoEm` é o maior `recebidoEm` entre as leituras **válidas** e é `null`
  quando não há nenhuma (ou nenhuma tem `recebidoEm`); não se inventa data.
- [Certo] Pela ingestão atual, nulo e negativo já não chegam à API (`@NotNull`,
  `@PositiveOrZero` em `LeituraRequest`) e `NaN` não é JSON padrão. A função pura se
  defende mesmo assim, por causa de dados legados e de outras origens.
- O descarte acontece **antes** de montar a janela: a janela é formada pelas 5 leituras
  **válidas** mais recentes.

**Histórico insuficiente:**

| Situação | Resposta |
|---|---|
| Jardim **sem nenhuma leitura** registrada | `404 JARDIM_NAO_ENCONTRADO` (comportamento existente da SW-07/SW-38 preservado) |
| Jardim com leituras, mas **menos de 5 válidas** | `200` com `tendencia = INSUFICIENTE` |

Alternativas descartadas: resultado ausente (o dashboard não distinguiria de erro e
colide com o 404), erro (é situação normal em jardim novo) e **cálculo com menos
leituras** (a tolerância foi calibrada para 4 intervalos; com menos, o mesmo limiar
significaria outra coisa).

---

## 7. Cronologia das leituras (C1, D7)

### 7.1 O problema

Cada leitura tem duas horas: `timestampDispositivo` (quando o sensor mediu) e
`recebidoEm` (quando a API recebeu, relógio do servidor). Com o dispositivo offline, o
firmware reenvia a fila de uma vez; segundo `esp32/src/modulos/drenagem.cpp` [Certo],
a drenagem envia primeiro o snapshot mais recente (âncora) e depois os eventos em FIFO,
do mais antigo para o mais novo. Logo, **a leitura recebida por último não é
necessariamente a mais recente medida**.

Exemplo (jardim drenando de verdade; medições `20 → 16 → 12 → 8`, chegada `8, 20, 16,
12`): ordenar por recebimento dá `12 − 8 = +4` (`ENCHENDO`); a ordem correta dá
`8 − 20 = −12` (`DRENANDO`).

Por isso **não basta** buscar as N últimas por recebimento e reordenar depois: isso pode
selecionar as medições erradas. **Ordenar primeiro pela chave correta e só depois
limitar.**

### 7.2 Decisão

- **Chave cronológica:** `timestampDispositivo`, usando `recebidoEm` quando o primeiro
  for nulo (contrato de telemetria, seção 3.4, opção B).
- **Nunca afirmar que a cronologia foi validada.** A confiabilidade é sempre
  **"não validada"**, porque os dados atualmente persistidos não permitem validar.
- Existir um timestamp **não** significa que ele seja confiável.
- `recebidoEm` como alternativa **não** recupera a cronologia de eventos retransmitidos.
- **Não** usar a comparação entre a ordem de chegada e a ordem de medição como
  indicador: coincidir não prova que os horários são confiáveis, e divergir pode ser
  retransmissão legítima.

### 7.3 Por que a API não consegue validar hoje

- [Certo] O firmware envia `seq`, `bootId` e `horaSincronizada` no JSON reconstruído
  (`gateway.cpp`, `montarJsonGateway`); no envio real, `horaSincronizada` vem da linha
  original, isto é, do estado do relógio **no momento em que o registro nasceu**.
- [Certo] A API **descarta** esses campos em silêncio: `LeituraRequest` não os declara e
  a entidade `Leitura` não os persiste.
- [Certo] Sem NTP, o firmware usa `2026-10-15T00:00:00Z + millis()`. Um reinício do
  ESP32 reinicia essa base, e uma leitura **mais nova** pode parecer **mais antiga**.
- [Provável] A API não tem como detectar isso com os dados atuais.
- [Dado a verificar] **Validação ponta a ponta:** confirmar que o corpo HTTP enviado
  pelo ESP32 é exatamente esse JSON, que a API o recebe e descarta esses campos, e o
  valor real da flag com e sem NTP. Há ainda um campo `origem` (por exemplo
  `simulacao`) no JSON do gateway que não consta na lista da seção 3.3 do contrato.

### 7.4 Sinais informativos (não são prova de confiabilidade)

O resultado planejado registra dois booleanos, com significado **apenas** o que dizem:

- existe leitura **sem** `timestampDispositivo` na janela;
- existe **mais de um** `sensorId` na janela.

---

## 8. Tratamento de empates (A3, D8)

Duas leituras **empatadas** são aquelas com a **mesma chave cronológica**. A SW-14 trata
um grupo empatado como um **conjunto de possibilidades**:

- **Não** desempata por `id` (ordem de gravação) nem por nível. Esses critérios
  inventariam uma ordem; a ordem de gravação não representa necessariamente a ordem
  real das medições. Média dos empatados também foi descartada (cria um valor que
  ninguém mediu).
- Determinismo significa que o **mesmo conjunto** de leituras produz sempre o **mesmo
  resultado**, independente da ordem em que o banco as devolve. A função pura **ordena
  a entrada por conta própria**.
- Calcula-se a **faixa de variação possível** (mínima e máxima). Como o estado é
  monótono na variação, basta comparar o estado das duas extremidades: se coincidem, o
  resultado é calculável e **não depende de nenhuma ordem inventada**; se diferem, o
  resultado é `INDETERMINADO`.

### 8.1 Cuidados incorporados

1. **Empate atravessando o corte.** A seleção das 5 leituras não descarta antecipadamente
   registros empatados no limite da janela. Se o corte atravessa um grupo, **todos os
   membros** são candidatos; o número de membros incluídos é fixo (5 menos as
   estritamente mais novas) e qualquer subconjunto desse tamanho é uma possibilidade.
2. **Combinações realmente possíveis.** A primeira e a última leitura têm de ser
   leituras **distintas**. Quando vêm de grupos diferentes, as escolhas são
   independentes. Quando a janela inteira cabe num único grupo, a variação é calculada
   entre pares ordenados de leituras distintas. [Provável] Nessa configuração a faixa
   é simétrica (`−D` a `+D`, com `D` = maior nível − menor nível do grupo) e incluir ou
   não o par "mesma leitura" não muda o resultado; **o teste de oráculo por força
   bruta** (seção 14) valida isso.

### 8.2 Exemplos (N = 5, tolerância 1,5 cm)

- **Empate numa ponta, estados diferentes → `INDETERMINADO`:** leituras `10,0 (t0)`,
  `11,0 (t1)`, `12,0 (t2)`, `14,0 (t3)` e `10,5 (t3)`. A última pode ser 14,0
  (variação +4,0, `ENCHENDO`) ou 10,5 (variação +0,5, `ESTAVEL`).
- **Empate numa ponta, mesmo estado → calculável:** a última pode ser 14,0 ou 13,0;
  variações +4,0 e +3,0, ambas `ENCHENDO`.
- **Empate só no meio → calculável:** não afeta a diferença entre as pontas.

### 8.3 Escopo

Neste MVP só se consideram as ambiguidades **decorrentes de empates na chave
cronológica**. Mistura de relógios e timestamp no futuro **não bloqueiam** o cálculo
nem geram `INDETERMINADO`: ficam na categoria "calculável, cronologia não validada". Uma
análise de sensibilidade (recalcular sem as leituras suspeitas) fica como evolução.

Empates entre leituras **sem** timestamp (chave `recebidoEm`) recebem o mesmo
tratamento.

---

## 9. Série única por jardim (F1, D9)

- Todas as leituras do jardim formam **uma série**. A tendência é do jardim como um
  todo, **não** por sensor.
- [Certo] Pelo firmware (`amostragem.cpp`), os eventos dos sensores de degrau gravam o
  nível do **conjunto**, e o `idSensor` indica qual sensor mudou. O evento da sonda
  grava `sonda.nivelCm`. O snapshot usa o nível da sonda quando existe e, senão, o dos
  degraus. Snapshots e `boot` usam `idSensor: "jardim"` (confirmado em `gateway.cpp`).
- **Não se assume** que medições de sensores diferentes sejam equivalentes só por
  estarem em centímetros. Para o **simulador** (um sensor por jardim), F1 é adequada.
  Para o **hardware real**, a comparabilidade precisa de validação.
- Os sensores resistivos T1592 têm degraus de 5, 10 e 15 cm; a sonda HW-02 é contínua.
  A diferença de resolução **não** será tratada aumentando a tolerância (isso poderia
  ocultar variações reais).
- [Provável] Nos degraus, `0.0` significa "abaixo do primeiro degrau", e o nível satura
  em 15 cm; acima disso a água pode subir sem o valor mudar.
- [Dado a verificar] Configuração dos sensores no piloto, calibração e comparabilidade
  entre as fontes físicas (pendências com o time de hardware).

---

## 10. Idade e `DESATUALIZADO` (D12)

- A função pura recebe **"agora" como argumento** (relógio injetado). Sem isso ela
  deixaria de ser pura e testável.
- **Referência:** `recebidoEm` (relógio do servidor), **não** `timestampDispositivo`.
  Com o horário estimado do firmware (base `2026-10-15`), a idade por
  `timestampDispositivo` daria semanas e o resultado seria sempre "velho".
- A idade é calculada sobre a leitura de **maior `recebidoEm` entre todas as leituras
  válidas buscadas**, e não só sobre a janela, para não depender do empate no corte.
- **Limite:** 30 s, configurável. Idade **menor ou igual** a 30 s é atual; **acima**
  de 30 s é `DESATUALIZADO`.
- **O que isso mede:** o tempo desde a **última recepção** (silêncio do dispositivo).
  **Não** é a idade real da medição. Numa retransmissão de leituras antigas, o
  `recebidoEm` é novo, então a medição pode ser antiga mesmo com a recepção recente.
  Isso se soma à cronologia não validada (seção 7).
- **Decisão (2026-11-19): sem `recebidoEm`, a idade não é avaliável.** Se nenhuma
  leitura válida tiver `recebidoEm` (só `timestampDispositivo`), não há última recepção:
  `ultimaRecepcaoEm = null` e a função **não** declara `DESATUALIZADO`. Mas **também não
  afirma que o dado está atualizado**: `ultimaRecepcaoEm = null` significa "atualidade
  desconhecida", e quem consome o resultado não deve apresentá-lo como "dado atual". A
  função continua classificando a tendência normalmente. Só ocorre com dados anômalos,
  pois o servidor sempre preenche `recebidoEm`. É uma **limitação conhecida**, e o escopo
  desta exceção não será ampliado agora. Uma recepção **posterior** ao "agora"
  (diferença entre relógios) dá idade negativa e é considerada atual.
- Os 30 s foram escolhidos só para o **simulador** [Suposição]: o intervalo mais lento
  é 5 s e a janela de 5 leituras cobre até 20 s. Para o hardware, a periodicidade do
  snapshot é desconhecida: [Dado a verificar].

---

## 11. Contrato planejado do endpoint (D13) — **não implementado**

`GET /api/jardins/{id}/tendencia`

- Endpoint **novo e aditivo**: não altera `GET /api/jardins/{id}` nem
  `GET /api/jardins/{id}/leituras`, e não se mistura com a SW-13.
- O caminho e os campos são decisões deste documento; **não existem ainda no código**.

### Códigos HTTP

| Situação | Resposta |
|---|---|
| Qualquer um dos 6 estados | `200` |
| Jardim sem nenhuma leitura | `404`, corpo padrão do `TratadorDeErros` (`JARDIM_NAO_ENCONTRADO`) |

### Campos

| Campo | Para quê |
|---|---|
| `jardimId` | Identifica o jardim |
| `tendencia` | Um dos 6 valores do enum |
| `leiturasValidas` | Quantas leituras válidas foram consideradas (mostra "3 de 5" no `INSUFICIENTE`) |
| `leiturasDescartadas` | Quantas leituras inválidas foram descartadas |
| `janelaLeituras` | Tamanho da janela (5) |
| `toleranciaCm` | Tolerância usada (transparência sobre o parâmetro experimental) |
| `confiabilidadeCronologia` | Constante `"NAO_VALIDADA"` no MVP |
| `semTimestampDispositivoNaJanela` | Sinal informativo (seção 7.4) |
| `multiplosSensorId` | Sinal informativo (seção 7.4) |
| `ultimaRecepcaoEm` | `recebidoEm` da leitura mais recente recebida (tempo desde a última **recepção**) |

**Fora do MVP:** `variacaoLiquidaCm`. Com empates ela seria uma faixa e confundiria o
dashboard. A tolerância vem da configuração, **não** da requisição.

### Exemplo ilustrativo (formato planejado)

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
  "ultimaRecepcaoEm": "2026-11-17T12:00:09.123Z"
}
```

[Dado a verificar] Semântica exata de `leiturasValidas` quando há mais leituras do que
a busca traz (a consulta é limitada). Definir na Etapa 6.

---

## 12. Fonte dos dados (P3, D14) e cuidados obrigatórios da consulta

**Decisão (P3):** uma **consulta nova**, só para a SW-14, ordenada por
`COALESCE(timestampDispositivo, recebidoEm) DESC`, com a regra da chave cronológica
**em um único lugar**. SW-06 e SW-07 **não mudam** neste trabalho.

**Divergência temporária (documentada):** hoje o histórico e o estado atual (SW-06,
SW-07) ordenam por `recebidoEm DESC, id DESC`; a seção 3.4 do contrato decidiu migrar
para `timestampDispositivo` e marcou a implementação como pendente. Enquanto a
unificação não ocorre, o **histórico** e a **tendência** podem divergir na ordenação
visível ao dashboard. É preciso registrar o ticket de acompanhamento "alinhar SW-06 e
SW-07 ao contrato 3.4", com testes de regressão de ordenação (por exemplo
`TelemetriaIntercambiavelTests`, que verifica a ordem do histórico).

### Cuidados obrigatórios (fixados antes de implementar a consulta)

1. **Não usar margem arbitrária de registros para tratar empates no corte.** A consulta
   precisa recuperar **todos os candidatos relevantes do grupo empatado**, sem permitir
   que a paginação produza uma classificação incorreta. Se um grupo empatado no corte
   se estende além do que foi buscado, não se pode adivinhar o resto do grupo.
2. **Não concluir que o histórico é insuficiente só porque há leituras inválidas entre
   as primeiras N recuperadas.** É preciso **procurar as próximas leituras válidas**,
   respeitando a ordenação e os limites de consulta.

Consequência de projeto: a função pura recebe leituras já suficientes para o corte e
**não** sabe se faltou buscar mais; **quem decide quando parar de buscar é o serviço**.
A estratégia exata da consulta (busca em etapas, limites) será projetada na Etapa 6.
[Provável] O `COALESCE` na ordenação com `Pageable` em JPQL funciona; será validado em
teste de integração.

---

## 13. Experimentos de N e T

### 13.1 Metodologia

Análise **reproduzível e descartável**, fora do repositório (os scripts não são
versionados; ver `SCRUM-28-handoff.md`). Usou o `GeradorDadosJardim` **real**, compilado
do código-fonte sem alterá-lo.

- **Modelo do simulador [Certo]:** passeio aleatório; a cada leitura,
  `nível += tendência + sorteio entre −passo e +passo`, limitado entre 0 e 50 cm e
  gravado com 1 casa decimal. Parâmetros do `main` do gerador:

| Jardim | Intervalo | Nível inicial | Ruído (±) | Tendência por leitura | Papel |
|---|---|---|---|---|---|
| J01 | 2 s | 18,0 | 1,5 | 0 | estável |
| J02 | 3 s | 10,0 | 1,0 | +1,2 | enchendo |
| J03 | 4 s | 40,0 | 1,0 | −1,0 | drenando |
| J04 | 5 s | 25,0 | 3,0 | 0 | oscilando |

- **Três fontes, mantidas separadas:** (1) **teórico**: fórmula exata da soma de
  uniformes (Irwin–Hall), sem o limite 0–50 e sem arredondamento; (2) **Monte Carlo**:
  20.000 corridas (sementes 1 a 20000) com o gerador real; (3) **sementes oficiais**
  1 a 4 do `main`, uma corrida determinística.
- Em cada corrida, as **25 primeiras leituras** (antes de qualquer saturação em 0 ou 50
  cm), com todas as janelas deslizantes de N leituras.
- A comparação usa aritmética em **décimos de cm inteiros**, para não misturar com o
  problema de ponto flutuante.
- O resultado teórico e o observado concordam; o observado fica 1 a 2 pontos
  percentuais acima no `ESTAVEL` porque, na grade de 0,1 cm, variações exatamente
  iguais a ±T acontecem e contam como `ESTAVEL` [Provável].
- Erro de amostragem do Monte Carlo: até ~0,35 ponto percentual [Provável].

### 13.2 Resultados observados (Monte Carlo, N × T)

Frações de janelas classificadas como o rótulo esperado para cada jardim. "Troca" é a
fração de pares de janelas consecutivas com classificação diferente ("pisca-pisca").

| N | T (cm) | J01 ESTAVEL | J02 ENCHENDO | J03 DRENANDO | Troca J01 | Troca máx. (J02, J03) |
|---|---|---|---|---|---|---|
| 3 | 0,5 | 33,3% | 99,7% | 96,2% | 52,4% | 6,2% |
| 3 | 1,0 | 57,8% | 94,7% | 86,2% | 47,0% | 17,9% |
| 3 | 1,5 | 76,7% | 83,4% | 70,0% | 31,7% | 29,1% |
| 3 | 2,0 | 90,0% | 66,0% | 47,6% | 15,9% | 33,3% |
| 3 | 3,0 | 100%* | 22,8% | 11,3% | 0%* | 25,1% |
| **5** | 0,5 | 24,1% | ~100% | 99,98% | 38,3% | 0,04% |
| **5** | 1,0 | 44,2% | ~100% | 99,7% | 38,4% | 0,5% |
| **5** | **1,5** | **61,5%** | **99,9%** | **98,5%** | **31,7%** | **2,1%** |
| **5** | 2,0 | 75,3% | 99,4% | 95,4% | 23,4% | 5,4% |
| **5** | 3,0 | 92,2% | 93,3% | 78,8% | 9,5% | 16,8% |
| 7 | 0,5 | 20,0% | ~100% | ~100% | 31,7% | 0% |
| 7 | 1,0 | 37,1% | ~100% | 99,99% | 33,1% | 0,01% |
| 7 | 1,5 | 52,6% | ~100% | 99,97% | 29,3% | 0,04% |
| 7 | 2,0 | 65,8% | ~100% | 99,8% | 24,0% | 0,2% |
| 7 | 3,0 | 84,6% | 99,9% | 98,3% | 13,5% | 2,0% |

\* Artefato do simulador: o ruído é limitado a ±1,5 por passo, então com 2 passos a
variação máxima é exatamente 3,0 cm, e `<=` conta como `ESTAVEL`. Ruído real não tem
esse teto.

**"~100%" é o que foi observado, não uma garantia.** Por exemplo, para N = 5 e T = 1,0
o J02 teve zero janelas `ESTAVEL` em cerca de 420 mil, mas essas janelas são
correlacionadas; a conta teórica dá cerca de 0,0004% de chance, pequena e não nula.

Menor deriva por leitura detectada como `ENCHENDO` em pelo menos 90% das janelas
(**teórico**, ruído ±1,0 como J02 e J03, sem limite 0–50), em cm por leitura:

| N | T = 0,5 | T = 1,0 | T = 1,5 | T = 2,0 | T = 3,0 |
|---|---|---|---|---|---|
| 3 | 0,80 | 1,05 | 1,30 | 1,55 | 2,05 |
| 5 | 0,50 | 0,63 | **0,75** | 0,88 | 1,13 |
| 7 | 0,39 | 0,47 | 0,56 | 0,64 | 0,81 |

Com ruído maior que ±1,0, esses limiares crescem.

### 13.3 Achados

1. **Com T fixo, N maior piora o J01:** a dispersão acumulada de um passeio aleatório
   cresce com N mesmo com tendência zero. Com T = 1,0, o `ESTAVEL` do J01 cai de 58%
   (N = 3) para 31% (N = 10).
2. **J02 e J03 melhoram rápido com N:** com N = 5 já chegam a ~100% (T = 1,0).
3. **T e N atuam juntos como limiar por intervalo:** `T / (N − 1)` é a variação média
   por intervalo a partir da qual se declara uma tendência. Com o mesmo limiar de
   0,5 cm por intervalo, N maior melhora todas as métricas (por exemplo,
   N = 3, T = 1,0: J01 57,8%; N = 5, T = 2,0: 75,3%; N = 7, T = 3,0: 84,6%), à custa
   de exigir mais histórico, cobrir mais tempo e reagir mais devagar.
4. **A "persona" do simulador não é a verdade de cada janela.** "Estável" no simulador
   significa **tendência configurada zero**, não nível parado: na sequência oficial
   (semente 1) o J01 foi de 17,0 para 22,3 cm e voltou a ~18,7. A fração "correta" do
   J01 mede quanto a tolerância cobre o ruído, não um erro do algoritmo.
5. **Um T único não serve a todos os jardins:** o ruído do J04 (±3,0) o mantém
   oscilando em qualquer par testado. É a limitação da abordagem de variação líquida.

### 13.4 Justificativa de N = 5 e T = 1,5 cm

**Prioridade declarada:** preservar a sensibilidade a mudanças reais de nível, sem
exigir um histórico muito grande.

- **N = 5** já leva J02 e J03 a ~100% com T = 1,0 e exige só 5 leituras.
- **T = 1,5 cm** mantém J02 em 99,9% e J03 em 98,5%, com pisca-pisca de no máximo 2,1%
  entre os dois, e detecta derivas a partir de ~0,75 cm/leitura (com ruído ±1,0).
- **T = 1,0** teria o J01 em apenas 44% de `ESTAVEL` e 38% de troca.
- **T = 2,0** subiria o J01 para 75% de `ESTAVEL`, mas derrubaria o J03 para 95,4% e
  pioraria a detecção de mudanças reais (menor deriva sobe para 0,88 cm/leitura).
- **T = 3,0** (melhor para "estabilizar" o J01) é calibrado só no ruído do simulador,
  oculta variações de até 3 cm e é arriscado para hardware com degraus de 5 cm e ruído
  desconhecido.
- **Não** se aumenta T para reproduzir o rótulo `ESTAVEL` do J01; aceita-se que ele
  oscile na classificação.
- **Alternativa registrada para reavaliação futura:** **N = 7 e T = 3,0 cm**. É melhor
  em todas as métricas medidas no simulador, mas exige 7 leituras, cobre mais tempo e
  reage mais devagar [Provável, não medido]. **Não foi adotada agora.**

### 13.5 Limites desta análise

- Só as 25 primeiras leituras por corrida. O simulador limita o nível entre 0 e 50 cm:
  [Provável] o J02 satura em 50 cm depois de ~33 leituras (~100 s) e o J03 chega a 0
  depois de ~40 leituras (~160 s); depois disso ficam parados e viram `ESTAVEL`. É
  comportamento atual do simulador, não alterado.
- Janelas deslizantes são correlacionadas entre si; as frações marginais são válidas,
  e o "troca de rótulo" depende da sobreposição.
- **Não** mediu o tempo de reação quando a tendência começa ou termina.
- **Não** exercitou empates de horário (timestamps sintéticos sempre distintos).
- Nada disto vale para o hardware real: não há dados de ruído nem calibração.

---

## 14. Estratégia de testes (D15)

- **Unitários** da função pura: sem Spring, sem banco, com **séries construídas à
  mão**. Não se valida o algoritmo contra o rótulo "persona" do simulador.
- **Integração** (MockMvc, H2 em memória, como os testes existentes) da recuperação e
  da exposição.
- **Regressão:** rodar a suíte existente completa, sem alterá-la.
- O **simulador** serve só para observação e regressão com sementes fixas.
- **TDD obrigatório (decisão de 2026-11-19):** cada comportamento novo segue
  **RED** (escrever o teste e vê-lo falhar pelo motivo esperado), **GREEN** (o mínimo
  para passar) e **REFACTOR** (melhorar com a suíte verde). Os testes **não** são escritos
  depois de uma calculadora pronta; entram junto com a implementação, um comportamento por
  ciclo. Isso vale também para a integração (Etapa 6: testes de repositório e de MockMvc
  antes da implementação correspondente). A Etapa 5 deixou de ser "escrever todos os
  testes" e passou a ser **revisão de cobertura, casos de borda e execução da suíte
  completa**.

Cenários aprovados (N = 5, tolerância 1,5 cm, níveis em ordem cronológica):

| # | Cenário | Esperado |
|---|---|---|
| 1 | `10,11,12,13,14` | `ENCHENDO` (+4) |
| 2 | `14,13,12,11,10` | `DRENANDO` (−4) |
| 3 | `10,10.4,9.8,10.3,10.6` | `ESTAVEL` (+0,6) |
| 4 | cinco leituras iguais | `ESTAVEL` (0) |
| 5 | última − primeira = **+1,5** e **−1,5** | `ESTAVEL` nos dois |
| 6 | **+1,6** e **−1,6** | `ENCHENDO` e `DRENANDO` |
| 7 | tolerância 0,1 com `1,0 → 1,1` (a falha do `double`) | `ESTAVEL` |
| 8 | `10,15,10,15,10` | `ESTAVEL` (documenta a limitação) |
| 9 | `10,20,0,30,11` (só as pontas contam) | `ESTAVEL` (+1) |
| 10 | vazio, 1 leitura, 4 leituras | `INSUFICIENTE` |
| 11 | 6 leituras, a mais antiga é outlier | usa só as 5 mais recentes |
| 12 | mesma série embaralhada | mesmo resultado (determinismo) |
| 13 | ordem de chegada ≠ ordem de medição (retransmissão) | usa a ordem de medição |
| 14 | empate no meio | calculável |
| 15 | empate na ponta, estados diferentes (`…,14.0` e `10.5` no mesmo instante) | `INDETERMINADO` |
| 16 | empate na ponta, mesmo estado | calculável |
| 17 | empate **atravessando o corte**, estados diferentes (`10.0` e `11.0` no mesmo instante, última `12.0`) | `INDETERMINADO` |
| 18 | empate atravessando o corte, mesmo estado | calculável |
| 19 | todas no mesmo instante, níveis diferentes / todos iguais | `INDETERMINADO` / `ESTAVEL` |
| 20 | nulo, `NaN`, infinito e negativo | descartados e contados |
| 21 | leitura sem `timestampDispositivo` | usa `recebidoEm` e liga o sinal |
| 22 | mais de um `sensorId` | sinal ligado |
| 23 | idade acima do limite / exatamente no limite | `DESATUALIZADO` / atual |
| 24 | precedência (`INSUFICIENTE` > `DESATUALIZADO` > `INDETERMINADO`) | conforme a ordem da seção 5 |
| 25 | **oráculo por força bruta** em séries pequenas com empates e corte | igual à enumeração de todas as possibilidades |

Integração: um caso por estado do enum, o 404 do jardim sem leitura, o formato do JSON,
retransmissão via POST com timestamps fora de ordem, leituras sem timestamp,
`confiabilidadeCronologia` constante, e a idade com um `Clock` controlável.

### Cobertura dos 25 cenários pela função pura (estado atual)

Testes em `api-jardim-sentinela/src/test/java/br/edu/cesar/jardimsentinela/tendencia/`
(classes `CalculadoraTendencia*Test`; construtores de dados em `LeiturasDeTeste`). Os 25
cenários estão cobertos no nível unitário; os de integração (acima) dependem da Etapa 6.

| Cenários | Classe de teste |
|---|---|
| 1 a 9 (classificação, limites inclusivos, `BigDecimal`, oscilação, só as pontas) | `CalculadoraTendenciaClassificacaoTest` |
| 10 (vazio, 1 e 4 leituras), 20 (inválidas), 22 (múltiplos sensores) | `CalculadoraTendenciaTest` (e `...CronologiaTest` para as inválidas intercaladas) |
| 11, 12, 13, 21 (janela, embaralhado, retransmissão, sem timestamp) | `CalculadoraTendenciaCronologiaTest` |
| 14 a 19 (empates, corte, todas no mesmo instante) | `CalculadoraTendenciaEmpatesTest` |
| 23 e 24 (idade e precedência) | `CalculadoraTendenciaAtualidadeTest` |
| 25 (oráculo por força bruta: 3000 séries sorteadas com semente fixa, 5 a 7 leituras, 4 horários) | `CalculadoraTendenciaOraculoTest` |

Complementos além dos 25: leitura sem nenhuma referência temporal, tolerância e limite de
idade inválidos, última recepção só sobre válidas, sinais sobre os candidatos do corte.
Verificação de força: um teste de mutação manual (alterar uma regra por vez) foi
detectado pela suíte em 6 de 6 mutações experimentadas.

**Os testes passando não bastam como critério de aceite:** é preciso conferir que eles
representam as decisões de domínio deste documento.

---

## 15. Limitações conhecidas e evoluções futuras

### Limitações conhecidas

1. **Parâmetros experimentais** (N, tolerância, idade) definidos a partir do simulador;
   **não calibrados** para o hardware real e **não** usados para mascarar diferenças de
   resolução entre sensores.
2. **`ESTAVEL` não significa nível constante.** Oscilação grande com variação líquida
   dentro da tolerância é `ESTAVEL`.
3. **Cronologia não validada.** A API ainda descarta `bootId`, `seq` e
   `horaSincronizada`. Horário estimado do firmware e reinício do ESP32 podem inverter a
   ordem real sem aviso.
4. **A idade mede a última recepção**, não a idade real da medição.
5. **Janela por quantidade de leituras:** o tempo coberto varia por jardim (no
   simulador, de 8 a 20 s) e, no hardware real, pode cobrir horas.
6. **Idempotência (SCRUM-44):** a chave `jardimId + sensorId + timestampDispositivo`,
   com resolução de 1 segundo, **pode descartar eventos legítimos** do mesmo sensor no
   mesmo segundo (a API devolve 200 com o registro existente). Se o evento descartado
   fosse uma ponta da janela, a tendência seria calculada com dados incompletos, sem
   detecção. A ingestão **não será alterada** neste trabalho.
7. **Possível duplicata em corrida:** [Provável] a checagem da SW-10 consulta e depois
   grava, sem restrição de unicidade na entidade; requisições simultâneas idênticas
   podem gerar duplicatas. Elas têm o mesmo horário e o mesmo nível, então o tratamento
   de empates as tolera.
8. **Série única por jardim:** pode misturar fontes de resolução diferente (degraus de
   5 cm e sonda contínua); a comparabilidade no hardware está por validar.
9. **Divergência temporária de ordenação** entre a tendência (`timestampDispositivo`) e
   o histórico/estado atual (`recebidoEm`), até a unificação (seção 12).
10. **Saturação do simulador** em 0 e 50 cm e saturação em 15 cm dos degraus (seção 9).
11. A medida de tendência **não** é previsão de enchente.
12. **Atualidade desconhecida sem `recebidoEm`:** com leituras válidas sem nenhum
    `recebidoEm`, a idade não pode ser medida; o resultado traz `ultimaRecepcaoEm = null`
    e não afirma que o dado está atual (seção 10).

### Evoluções futuras

- **Persistir `bootId`, `seq` e `horaSincronizada`** (Alternativa 2): permitiria
  ordenar dentro do mesmo boot por `seq`, detectar retransmissão e perda, e confiar no
  `timestamp` só quando `horaSincronizada` for `true`, o que abriria caminho para uma
  cronologia **validada**. Mexe em entidade, DTO, contrato e banco e depende de decisão
  do time (contrato, seção 3.3). Observação: `bootId` é aleatório e **não** indica
  qual boot veio antes.
- **Unificar a ordenação** de SW-06 e SW-07 com a SW-14 (contrato, seção 3.4).
- **Medida de oscilação** (`máximo − mínimo`) sem necessariamente criar estado novo.
- **Análise de sensibilidade** para relógios mistos e timestamps no futuro.
- **Calibração dos parâmetros** no hardware: coletar leituras com o nível parado,
  medir a dispersão e escolher a tolerância acima do ruído observado, com margem.
- **Reavaliar N = 7 e T = 3,0 cm.**
- **Tolerância por jardim ou por tipo de fonte**, se um valor único não servir.
- **Janela por tempo**, ou combinada com a quantidade, para hardware de eventos
  assíncronos.

---

## 16. Pendências [Dado a verificar]

1. Branch-base oficial: `develop` foi usada como referência técnica; `main` está 56
   commits atrás.
2. Validação ponta a ponta do envio do firmware (seção 7.3), incluindo o campo `origem`.
3. Configuração dos sensores no piloto, calibração e comparabilidade (seção 9).
4. Periodicidade real do snapshot no hardware, para calibrar a idade máxima.
5. Estratégia da consulta (Etapa 6). A semântica de `leiturasValidas` para a função pura
   está fechada (seção 6); falta definir como o serviço informa a contagem quando a
   consulta traz mais leituras do que a janela.
6. Nomes das propriedades de configuração (tolerância e idade máxima).
7. ~~Comportamento sem nenhum `recebidoEm` nas leituras válidas~~ **Fechado em
   2026-11-19** (seção 10): `ultimaRecepcaoEm = null`, sem `DESATUALIZADO` e sem afirmar
   que o dado está atual. Na Etapa 6, o DTO deve manter o `null` visível, sem preenchê-lo.

## 17. Referências

- `docs/contrato-telemetria.md`: seções 2.3 (hora do registro), 3.3 (campos ignorados) e
  3.4 (qual hora manda).
- `docs/dicionario-de-dados.md`: sensores, unidades e a fonte de `nivelAgua`.
- `docs/api-dashboard.md`: endpoints consumidos pelo dashboard (receberá a seção do novo
  endpoint na Etapa 7).
- Código: `JardimController.buscarHistorico`, `JardimService.buscarHistorico`,
  `LeituraRepository`, `Leitura`, `LeituraRequest`, `GeradorDadosJardim`,
  `esp32/src/modulos/drenagem.cpp`, `gateway.cpp`, `relogio.cpp`, `amostragem.cpp`.

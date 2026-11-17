# SW-14 / SCRUM-28 — Documento de continuidade (handoff)

> Escrito em 2026-11-17 para continuar o desenvolvimento em **outro computador**, sem
> depender do histórico da conversa em que as decisões foram tomadas.
>
> As **decisões** estão em [`tendencia-nivel.md`](tendencia-nivel.md). Este documento
> diz **onde o trabalho parou** e **como retomar**.

---

## 1. Objetivo e critério de aceite

**SW-14 — Identificar tendência de nível** (Jira **SCRUM-28**).

- **História:** como operador, quero saber se o jardim está enchendo, estável ou
  drenando.
- **Critério de aceite:** o sistema calcula a tendência a partir de leituras
  consecutivas e retorna resultado consistente.
- **Dependência:** SW-07 (SCRUM-40), histórico de leituras, já concluída.
- **Não confundir:** tendência de nível não é previsão de enchente nem alerta oficial.
  A **SW-13** (classificação do estado físico) é independente e **não deve ser
  alterada**.

## 2. Repositório, branch e ponto de partida

| Item | Valor |
|---|---|
| Remoto (`origin`) | `https://github.com/RiosGabri/Jardim_Sentinela.git` |
| Branch de trabalho | `feat/SCRUM-28-identificar-tendencia-nivel` |
| Criada a partir de | `develop` em `1652df3` (Merge pull request #18, `feat/API-simulador-intercalável`) |
| Base de trabalho | `develop` (referência técnica provisória; ver pendências) |

[Dado a verificar] Os históricos de PR apontam para `JardimSentinela/Jardim_Sentinela`,
enquanto o `origin` local aponta para `RiosGabri/Jardim_Sentinela`. Confirme que a
organização e o repositório estão corretos e que a sua conta tem permissão de escrita.

## 3. Estado da implementação (implementado × planejado)

| Item | Estado |
|---|---|
| Enum `Tendencia` (6 valores, com documentação) | **IMPLEMENTADO** e compilado |
| `docs/tendencia-nivel.md` (decisões) | **ESCRITO** |
| `docs/SCRUM-28-handoff.md` (este arquivo) | **ESCRITO** |
| Record `ResultadoTendencia` | **PLANEJADO** (próximo passo) |
| Função pura `CalculadoraTendencia` | **PLANEJADO** |
| Testes unitários da função pura (25 cenários) | **PLANEJADO** |
| Consulta nova em `LeituraRepository` | **PLANEJADO** |
| `TendenciaService`, `TendenciaController`, `TendenciaResponse` | **PLANEJADO** |
| Propriedades de configuração (tolerância, idade máxima) e bean `Clock` | **PLANEJADO** |
| Testes de integração do endpoint | **PLANEJADO** |
| Seção do endpoint em `docs/api-dashboard.md` | **PLANEJADO** |
| Endpoint `GET /api/jardins/{id}/tendencia` | **NÃO EXISTE ainda** |

**Único código novo da SW-14 até aqui:**
`api-jardim-sentinela/src/main/java/br/edu/cesar/jardimsentinela/tendencia/Tendencia.java`.
Nenhum arquivo existente foi modificado.

## 4. Comandos

Todos a partir de `api-jardim-sentinela/` (no Windows PowerShell use `.\mvnw.cmd` no
lugar de `./mvnw`):

```bash
./mvnw -q -DskipTests compile        # compilar
./mvnw test                          # toda a suíte
./mvnw test -Dtest=NomeDaClasse      # uma classe de teste
```

Requisitos: um JDK (o `pom.xml` declara `java.version` 17; a verificação deste
documento rodou com JDK 26 sob essa configuração). O Maven Wrapper já vem no
repositório. O banco H2 de desenvolvimento (`api-jardim-sentinela/data/`) **não** é
versionado e é recriado automaticamente (`ddl-auto=update`); os testes usam H2 em
memória (`src/test/resources/application.properties`).

## 5. O que foi efetivamente compilado e testado

Em 2026-11-17, nesta máquina, com a branch acima:

- `./mvnw -q -DskipTests compile`: **sucesso**; `Tendencia.class` gerado.
- `./mvnw test`: **19 testes executados, 0 falhas, 0 erros, 0 ignorados, `BUILD
  SUCCESS`** (tempo total ~22 s). Classes: `JardimControllerTests` (4),
  `LeituraIngestaoTests` (5), `TelemetriaIntercambiavelTests` (6), `WebConfigCorsTests`
  (2) e os demais testes existentes (`ApiJardimSentinelaApplicationTests`,
  `GeradorDadosJardimTests`).

**Importante:** essa suíte é a **existente**. **Não há ainda nenhum teste da SW-14**, e
o enum não tem lógica a testar. Nada do plano de testes da seção 7 foi escrito.

## 6. Onde paramos

**Etapa 4, passo 1 concluído — enum criado.**
**Próximo passo: o record `ResultadoTendencia`.**

Modo de trabalho combinado (também no prompt de retomada): a pessoa conduz as decisões
de domínio; o assistente age como professor e par de programação. Etapas pequenas,
**um passo por vez**, conceito explicado **antes** de usar, e a pergunta literal
**"Faz sentido até aqui?"** depois de cada bloco de código não trivial.

## 7. Próximas etapas, na ordem

**Etapa 4 — núcleo (função pura).**

1. ✅ Enum `Tendencia`.
2. **Record `ResultadoTendencia`** — a estrutura devolvida pela função pura. Explicar
   `record` antes de usar. Campos mínimos planejados: `tendencia`, `leiturasValidas`,
   `leiturasDescartadas`, os dois sinais informativos (sem `timestampDispositivo` na
   janela; mais de um `sensorId`) e a data da última recepção. **A lista exata de campos
   é fechada neste passo.**
3. **`CalculadoraTendencia`** (função pura, sem Spring, sem banco, sem relógio global),
   em partes pequenas, cada uma com testes antes de seguir:
   - 3a. filtro de leituras válidas e ordenação por chave cronológica (conceitos:
     `Comparator`, listas e ordenação);
   - 3b. seleção da janela com empates e corte (conjunto de possibilidades);
   - 3c. variação líquida, faixa mínima e máxima e classificação, com `BigDecimal`;
   - 3d. idade (relógio injetado como argumento) e precedência dos estados.

**Etapa 5 — testes unitários.** Os 25 cenários aprovados (tabela em
`tendencia-nivel.md`, seção 14), cada um explicado antes de implementar, incluindo o
**oráculo por força bruta**. Séries construídas à mão. Sem Spring e sem banco.

**Etapa 6 — integração.** Consulta nova em `LeituraRepository`, `TendenciaService`
(com `Clock` e configuração), `TendenciaController`, `TendenciaResponse`, propriedades,
testes de integração (MockMvc, H2 em memória). **Aqui valem os dois cuidados
obrigatórios da consulta** (seção 9).

**Etapa 7 — validação final.** Suíte completa, revisão do diff, seção do endpoint em
`docs/api-dashboard.md`, confirmação de que SW-06, SW-07 e a SW-13 não foram alteradas,
e o ticket de acompanhamento para unificar a ordenação.

Cada etapa só avança com **autorização explícita**. Commit, push, merge e PR também só
com autorização explícita.

## 8. Decisões já fechadas (não reabrir)

Resumo; o texto completo está em `tendencia-nivel.md`: variação líquida; **N = 5**;
tolerância **1,5 cm**, configurável, argumento da função pura; limite inclusivo com
`BigDecimal`; 6 estados e precedência `INSUFICIENTE → DESATUALIZADO → INDETERMINADO →
tendência`; cronologia por `timestampDispositivo` com `recebidoEm` como alternativa e
**nunca validada**; empates como conjunto de possibilidades (A3), sem desempate por
`id` ou nível; série única por jardim (F1); leitura válida = não nula, finita e
`>= 0`, inválidas descartadas e contadas; `INSUFICIENTE` com menos de 5 válidas, 404 só
sem nenhuma leitura; idade de 30 s sobre `recebidoEm`, exatamente 30 s é atual;
endpoint `GET /api/jardins/{id}/tendencia` com campo `ultimaRecepcaoEm`; consulta nova
(P3) sem mudar SW-06 e SW-07.

Só reabra uma decisão diante de um **bloqueio concreto** que impeça a implementação
correta.

## 9. Pendências técnicas e decisões ainda não implementadas

**Cuidados obrigatórios da futura consulta (Etapa 6):**

1. **Não usar margem arbitrária** para tratar empates no limite das N leituras: recuperar
   **todos os candidatos do grupo empatado**, sem deixar a paginação produzir
   classificação incorreta.
2. **Não concluir histórico insuficiente** só porque há leituras inválidas entre as
   primeiras N recuperadas: continuar buscando as próximas válidas, respeitando a
   ordenação e os limites.

**Pendências de projeto:**

- A função pura recebe a entidade `Leitura` diretamente ou um tipo de entrada próprio?
  [Dado a verificar] `Leitura` tem construtor público e não depende do banco para ser
  instanciada em testes, mas acoplaria o cálculo à entidade JPA. Decidir no passo 3.
- Semântica exata de `leiturasValidas` quando há mais leituras do que a busca traz.
- Nomes das propriedades de configuração (sugestão: prefixo `jardimsentinela.tendencia.`).
- Estratégia concreta da consulta e validação do `COALESCE` em ordenação com paginação.
- Bean `Clock` para testar a idade de forma determinística.
- Branch-base oficial (`develop` foi usada provisoriamente).
- Ticket de acompanhamento: alinhar SW-06 e SW-07 ao contrato 3.4 (ordenação).
- [Dado a verificar] Validação ponta a ponta do envio do firmware
  (`seq`, `bootId`, `horaSincronizada`, `origem`).
- [Dado a verificar] Configuração dos sensores do piloto, calibração e periodicidade do
  snapshot no hardware.

## 10. Arquivos que ficam apenas na máquina original

Estes arquivos **não** vão para o GitHub e **não** são necessários para continuar:

| Arquivo (pasta temporária `scratchpad` do Claude Code, fora do repositório) | O que é |
|---|---|
| `AnaliseTendencia.java` | Análise descartável de N com o gerador real (teórico, Monte Carlo e sementes oficiais) |
| `AnaliseGrade.java` | Análise descartável da grade N × T |
| `resultado.txt` | Saída bruta da análise de N |
| `resultado_grade.txt` | Saída bruta da grade N × T |
| `out/` | Classes compiladas desses scripts |

O **resumo completo dos experimentos**, a metodologia (modelo do simulador, sementes
1 a 20000, 25 leituras iniciais, aritmética em décimos de cm) e as tabelas estão na
seção 13 de `tendencia-nivel.md`. Os scripts são descartáveis; se for preciso reproduzir
os números, a metodologia descrita permite reescrevê-los.

Também ficam só na máquina original, por serem ignorados ou locais: `.vscode/` (na raiz
do repositório, não versionado), `api-jardim-sentinela/data/` (banco H2 local, ignorado
pelo `.gitignore`), `api-jardim-sentinela/target/` (build) e o **contexto da conversa**
com o assistente.

## 11. Como continuar em outro computador

1. **Pré-requisitos:** Git, um JDK (veja a seção 4) e o Claude Code instalado e
   autenticado.
2. **Obter a branch** (depois que ela estiver no GitHub):

   ```bash
   git clone https://github.com/RiosGabri/Jardim_Sentinela.git
   cd Jardim_Sentinela
   git fetch origin
   git switch feat/SCRUM-28-identificar-tendencia-nivel
   ```

   Se o repositório já estiver clonado: `git fetch origin` e depois o `git switch`.
3. **Conferir que está na branch correta:**

   ```bash
   git branch --show-current     # deve imprimir: feat/SCRUM-28-identificar-tendencia-nivel
   git log --oneline -3          # o commit de baixo deve ser 1652df3 (Merge PR #18)
   git status -sb                # deve estar limpo
   ls api-jardim-sentinela/src/main/java/br/edu/cesar/jardimsentinela/tendencia
   ls docs/tendencia-nivel.md docs/SCRUM-28-handoff.md
   ```

4. **Conferir o build:**

   ```bash
   cd api-jardim-sentinela
   ./mvnw test        # espera-se: 19 testes, 0 falhas (suíte existente)
   ```

5. Abrir o Claude Code **na raiz do repositório** e colar o prompt abaixo.
6. **Primeira tarefa de programação na retomada:** o record `ResultadoTendencia`
   (Etapa 4, passo 2), depois de o assistente explicar o conceito de `record`.

## 12. Prompt de retomada (copie e cole no Claude Code)

```text
Estou retomando a SW-14 (SCRUM-28, "Identificar tendência de nível") do projeto
Jardim Sentinela em outro computador. Você atua como meu professor e par de
programação: eu conduzo as decisões de domínio; você me ensina os conceitos de
código que eu ainda não domino.

Antes de qualquer coisa, leia na íntegra:
  1. docs/SCRUM-28-handoff.md  (onde paramos, comandos, próximas etapas)
  2. docs/tendencia-nivel.md   (todas as decisões fechadas e os 25 cenários de teste)

Depois confira o ambiente SEM alterar nada: git branch --show-current (deve ser
feat/SCRUM-28-identificar-tendencia-nivel), git status -sb, git log --oneline -3, e
rode "cd api-jardim-sentinela && ./mvnw test" (espera-se 19 testes, 0 falhas, que são
da suíte existente; a SW-14 ainda não tem testes). Me mostre o resultado.

Estado: Etapa 4, passo 1 concluído (enum Tendencia criado). Próximo passo: o record
ResultadoTendencia.

Regras de trabalho:
- As decisões de domínio estão FECHADAS; não as reabra, salvo bloqueio concreto.
- Um passo por vez. Antes de usar um conceito novo (record, Comparator, BigDecimal,
  Clock, @Query...), explique-o com um exemplo mínimo fora do repo e depois aplique.
- Depois de cada bloco de código não trivial, pergunte literalmente "Faz sentido até
  aqui?" e espere minha resposta.
- Não confunda funcionalidade planejada com implementada. Não invente classes, campos
  ou endpoints sem verificar o código e justificar.
- Classifique incertezas com [Certo], [Provável], [Suposição] ou [Dado a verificar].
- Ao mostrar um erro: causa provável -> correção mínima -> porquê.
- Não altere SW-06, SW-07 nem a SW-13. Não rode comandos destrutivos (reset --hard,
  clean, rm, force push) sem me avisar antes.
- NÃO faça commit, push, merge nem PR sem minha autorização explícita.
- Lembre dos dois cuidados obrigatórios da consulta (seção 9 do handoff) para a
  Etapa 6.

Comece conferindo o ambiente e, em seguida, me explique o que é um "record" em Java
antes de criarmos o ResultadoTendencia.
```

# Relatório do ensaio do SW-19 — 26/11/2026 (develop atual e develop + PR #33)

> **O que é este documento:** o resultado de um ensaio do `docs/roteiro-teste-integrado-sw19.md`,
> executado por um assistente de IA, a pedido da Júlia Tenório Calado, num container Linux, com
> Chromium headless. **Não é a execução oficial do time e não fecha o SCRUM-39 sozinho.**
> Bloco D (Wokwi) **não foi executado**. Hardware real nunca foi usado.

## 1. Versões testadas

| Rodada | Código | SHA |
|---|---|---|
| 1 e 2 | `develop` pura (já com #32, #34 e #35) | `1bd7b5f` |
| 3 | `develop` + PR #33 (SW-18) mesclado localmente, sem conflito | `e04ae3c` (develop `1bd7b5f` + #33 `b5cb1fe`) |

Ambiente: Linux 6.18, Java 21, Node 22, Python 3.13, H2 em memória (e em arquivo nos testes B5 e C5),
perfil `simulacao`.

## 2. Resultado em uma tabela

PASSOU = bateu com o esperado do roteiro. **RESSALVA** = o comportamento está certo, mas o critério do
roteiro, lido ao pé da letra, não fecha (explicado na seção 4).

| Item | `develop` pura | `develop` + #33 |
|---|---|---|
| A1 testes da API | PASSOU (204) | PASSOU (206) |
| A2 lint / A4 build | PASSOU | PASSOU |
| A3 testes do frontend (≥ 50 em 7 arquivos) | **FALHOU** (27 em 6) | PASSOU (50 em 7) |
| A5 firmware 14/14 / A6 receptor | PASSOU | — (não mudou) |
| A7 CI verde no SHA | PASSOU (run da `develop` `1bd7b5f`, 3 jobs) | — |
| B1 banco vazio (controle) | PASSOU | PASSOU |
| B2 payload inválido rejeitado, nada gravado (controle) | PASSOU | PASSOU |
| B3/B3a/B3b cenário → API → `nivelCm`, `totalLeituras`, `SUBINDO` | PASSOU (21 × 201; 29,9; 21; SUBINDO) | PASSOU |
| B4 idempotência | PASSOU (21 × 200, total 21) | PASSOU |
| B5 persistência (H2 em arquivo, reiniciando a API) | PASSOU (29,9 / 21 antes e depois) | PASSOU |
| B6 / B7 / B8 (chuva intensa, saturação, drenagem) | PASSOU (31, 31 e 181 × 201) | PASSOU |
| B8a testes da regra de drenagem | PASSOU (9 testes) | — |
| B9 boot fora do histórico (`incluirBoot=false`) | **FALHOU** (parâmetro ainda não existe) | RESSALVA (32 → 30 leituras, 0 com 0.0) |
| B10 jardim inexistente → 404 | PASSOU | PASSOU |
| B11 smoke test | PASSOU (69/69, 2 achados F1/F2; `--estrito` sai 1) | — |
| C1 dashboard = API (J01–J03) | PASSOU | PASSOU |
| C1b J04 aparece sozinho com a página aberta | PASSOU | PASSOU |
| C2 polling sem recarregar | PASSOU (29,9 → 20,2 cm em ≤ 12 s) | PASSOU |
| C3 histórico chamado com `incluirBoot=false` | **FALHOU** (`?limite=30`) | PASSOU (`?limite=100&incluirBoot=false`) |
| C4 sem 0 cm no gráfico e no CSV | PASSOU (**prova fraca**, ver 4.2) | PASSOU (prova fraca) |
| C5 API fora: dados mantidos + aviso; volta sem recarregar | **FALHOU** (dados mantidos, **sem o aviso**) | PASSOU |
| C6 jardim sem leitura = estado vazio, sem banner | **FALHOU** (aparece banner de erro no detalhe) | PASSOU |
| C7 zero erros de console | RESSALVA | RESSALVA |
| C8 trocas rápidas J01→J02→J01 | PASSOU | PASSOU |
| D1–D3 Wokwi | **não executado** | **não executado** |

## 3. O que isso significa para fechar o SW-19

- Na `develop` **sem** o #33, **cinco itens do roteiro falham** (A3, B9, C3, C5, C6) porque dependem
  do PR #33. Com o #33 mesclado, todos passam, com as ressalvas abaixo.
- **Conclusão:** o SW-19 depende de o **#33 ser mergeado**. O fluxo simulador → API → banco →
  dashboard já funciona (blocos A, B e o núcleo de C), e o que o #33 acrescenta é o tratamento de
  falha, de estado vazio e de boot que o roteiro exige.
- **O SW-19 só fecha com a execução do time**, que também precisa decidir sobre o bloco D (Wokwi).

## 4. Ressalvas e problemas do próprio roteiro (para corrigir)

### 4.1 B9 — a contagem esperada ficou errada no meu ensaio
O roteiro espera "a 1ª lista tem 1 registro a mais". No ensaio houve **2**, porque o experimento C2r
(abaixo) criou um segundo registro de boot **antes** do B9. O comportamento está certo: com
`incluirBoot=false`, os 2 boots saem e nenhum registro com 0.0 resta. **No roteiro, o B9 deveria
dizer "a diferença é o número de boots já enviados".**

### 4.2 C4 — passa por um motivo fraco
O boot (0 cm) é o registro **mais antigo**. Como o jardim tem mais de 20 leituras, ele fica fora das
"Últimas 20" mesmo sem o #33. Para o C4 provar algo, o jardim precisa ter **menos leituras que a
janela**. **Correção aplicada no roteiro e validada:** usar um jardim com poucas leituras. No J04 com 1 boot + 5
snapshots (6 registros), o CSV exportado pelo dashboard teve **6 linhas, uma com nível 0, na `develop`
pura (`8cea007`)**, e **5 linhas, nenhuma com 0, com o #33 (`b288eb3`)**. Agora o teste consegue falhar.

### 4.3 C7 — "zero erros" é rígido demais
O navegador registra um erro de console para cada resposta 404, e o 404 do J04 sem leituras é o
**contrato** (estado vazio). Foram 4 erros, **todos "404"**, e nenhum outro. **Correção sugerida:**
"zero erros, exceto os 404 de jardins sem leitura".

### 4.4 C2 — risco de falso FALHOU por timestamps no futuro (**comprovado**)
Todo cenário sem `--inicio` publica timestamps até cerca de 1 min à frente. Uma nova rodada no mesmo
jardim **logo depois** fica escondida: `nivelCm` ficou em 29,9 antes e depois, apesar de a 2ª rodada
terminar em 20,2. O C2 só funciona se for feito **depois de ~1 min** ou com `--inicio` no passado.
**Neste ensaio os dados-base do bloco C foram publicados com `--inicio` de 5 minutos atrás**,
exatamente por isso. **Correção sugerida:** o roteiro avisar isso no C2.

### 4.5 A3 — o limiar depende do #33
"≥ 50 testes em 7 arquivos" só vale com o #33. Na `develop` pura são 27 em 6.

## 5. O que este ensaio **não** prova
- Wokwi, firmware real e hardware (bloco D): **não executados**.
- Execução por pessoas do time, em Windows ou em outra máquina: **não feita**. O smoke no Git Bash
  do Windows segue **[Dado a verificar]**.
- Visual do dashboard em tema escuro, mobile e acessibilidade: **não avaliados**.
- O C2 foi medido uma vez; o C8 e o C5 uma vez por rodada.

## 6. Evidências
Os logs, o CSV exportado e as capturas de tela do ensaio **não estão versionados** (ficaram na sessão
de trabalho de quem rodou). O que importa está nas tabelas acima, com os valores observados. Quem
repetir o ensaio deve anexar os próprios logs ao registro da seção F do roteiro.

Para repetir: `docs/roteiro-teste-integrado-sw19.md`. Os comandos dos blocos A e B funcionam como
estão. No bloco C, siga as observações do C2 (horário dos dados-base) e do C4 (jardim com poucas
leituras).

## 7. Correções já aplicadas no roteiro (mesma entrega deste relatório)
A3, B9, C2, C3, C4, C5, C6 e C7 foram reescritos, e uma seção "Dependência do PR #33" foi
acrescentada no topo do roteiro.

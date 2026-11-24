# Relatório de integração — SCRUM-21 [SW-12] — rodada de 23/11/2026

> **Responsável:** Rafael Carrilho. **Card:** SCRUM-21 / SW-12 — Validar
> integração da equipe completa.
> **Critério de aceite (Jira e checklist, idênticos):** Wokwi e simulador
> alimentam a API; dados persistem; endpoints respondem; a interface gerada
> consegue consumir dados de teste.
> **Escopo deste documento:** o que foi **executado** nesta rodada, com versão,
> comando e resultado. Nada aqui é herdado de rodadas anteriores sem estar
> marcado. O histórico da Sprint 2 continua em
> [`checklist-integracao-sprint2.md`](checklist-integracao-sprint2.md) (seções 0 a 5).

Classificação: **[Certo]** evidência executada · **[Provável]** inferência forte ·
**[Dado a verificar]** depende de algo que não foi possível executar.

---

## 1. Parecer

| Critério | Situação |
|---|---|
| Simulador alimenta a API | **Validado** |
| Dados persistem | **Validado** (H2 em arquivo, sobrevive a reinício) |
| Endpoints respondem | **Validado** (smoke novo, 69 asserções) |
| Interface consome dados de teste | **Validado** (PR #26 + API real, no navegador) |
| Wokwi alimenta a API | **Validado em 24/11/2026** com o firmware atual: 3.14 **PASSOU**; 3.16 **VALIDADO COM RESSALVAS** (§6.4) |

**Situação em 24/11/2026:** do ponto de vista técnico, os cinco critérios de
aceite têm evidência executada. O Wokwi foi rodado de verdade contra a API
Spring Boot (§6.4): o cenário 3.14 passou e o 3.16 foi validado **com ressalvas**
(buzzer sem verificação objetiva; piscar do vermelho não capturado dentro da
janela exata do bloqueio; só simulador, nunca hardware real).
**Não há impedimento técnico** para entregar a SW-12. O que ainda depende do
grupo são as decisões de domínio sobre **F1, F2 e F3** (§4), que não foram
implementadas aqui. Os testes automatizados e o CI verde não substituem essas
decisões.

---

## 2. Versões testadas

| Componente | Referência | SHA |
|---|---|---|
| `develop` (contém #19, #22, #23, #24, #28) | `origin/develop` | `d13d021` |
| Dashboard conectado à API | PR #26, HEAD | `8a0bd3f` |
| Ancestralidade | `git merge-base --is-ancestor d13d021 8a0bd3f` = sim; 8 commits próprios do #26 | só em `frontend/` |
| Backend e firmware | `git diff 22e0617 8a0bd3f -- api-jardim-sentinela` e `git diff cdd6ab1 8a0bd3f -- esp32` vazios | idênticos às versões testadas na rodada anterior (antes dos merges: PR #23 em `22e0617`, PR #24 em `cdd6ab1`) |

PRs ainda abertos em 23/11: #25 (develop → main, com conflitos), #26, #30
(SW-15) e #31 (SW-16). Os dois últimos **não** foram testados aqui e podem
alterar o contrato (eventos T1592, simulador).

Ambiente: container Linux, Java 21, Node 22, Python 3.13, g++, Chromium
(Playwright). Nessa rodada (23/11) Wokwi, PlatformIO e hardware estavam **indisponíveis**; o Wokwi foi executado depois, em 24/11, na máquina do responsável (§6.4).

### CI (consultado, não repetido)

`.github/workflows/ci.yml` roda `./mvnw test` (JDK 17), `npm ci`/lint/`npm test`/
build do frontend e `run_tests.py` + `test_receber.py` do firmware. Execução nº
14 (#26, `8a0bd3f`) e nº 12 (`develop`, `d13d021`): sucesso. O CI **não** sobe a
API, não abre o navegador, não roda o smoke HTTP, não usa Wokwi nem hardware.

---

## 3. Execuções (HEAD `8a0bd3f`)

| # | Teste | Comando | Resultado |
|---|---|---|---|
| 1 | API | `cd api-jardim-sentinela && ./mvnw -B -ntp test` | **180/180**, BUILD SUCCESS |
| 2 | Frontend | `cd frontend && npm ci && npm test && npm run lint && npm run build` | **27/27**, lint e build OK |
| 3 | Firmware (host) | `cd esp32 && python3 test/host/run_tests.py --verbose` | **14/14 grupos** |
| 4 | Firmware (receptor) | `python3 test/receiver/test_receber.py` | OK |
| 5 | Smoke **original** | `bash scripts/smoke-test-integracao.sh` (versão da `develop`) | 9 OK, **2 FALHAS** (passos 5 e 6) |
| 6 | Smoke **novo** | `bash scripts/smoke-test-integracao.sh` | **69/69**, 2 achados (F1, F2) |
| 7 | Smoke novo sem `jq` | idem, com `python3` no lugar | **69/69** |
| 8 | Smoke novo `--estrito` | idem | sai com 1 (achados contam como falha) |
| 9 | E2E navegador | API (H2 em memória) + simulador J01–J04 + `vite` + Chromium | §5 |
| 10 | Persistência | H2 em arquivo temporário; POST, parar API, subir, GET | 42,5 cm mantido; retransmissão `bootId+seq` = 200, total 1 |
| 11 | JSON do gateway → API | código real do gateway (C++) gerando JSON; POST | 201; reenvio 200; **42,5 preservado** |
| 12 | Sintaxe e lint do smoke | `bash -n` e `shellcheck` (v0.11) | sem erros e sem avisos |
| 13 | Autoteste da validação de URL | `bash scripts/smoke-test-integracao.sh --autoteste-url` | **45/45** URLs classificadas corretamente (sem rede) |
| 14 | Bloqueio **antes** de qualquer requisição | script com um `curl` falso no `PATH` que registra cada chamada, URLs com `@`, `#`, caminho, `file://`, host remoto, também com `SMOKE_PERMITIR_REMOTO=1` | **0 chamadas** de `curl` em todas; URL válida (controle) chegou ao `curl` |
| 15 | Controle negativo | smoke contra um servidor falso que sempre responde 200/201 | **52 falhas**, exit 1: o script não passa em branco |
| 16 | Interpretador de JSON quebrado | `python3` que falha (como o atalho da Microsoft Store), sem `jq` | erro claro (exit 2); com `python` funcional, usa-o: 69/69 |
| 17 | Timeout | servidor que aceita e não responde, `SMOKE_TIMEOUT=2` | termina em ~2 s (exit 2) |

Os números 1 a 3 coincidem com os da rodada anterior, antes dos merges (180/180,
27/27, 14/14), o que é esperado porque as árvores são idênticas. O teste 4 não
tinha sido executado antes. Os testes 12 a 17 foram executados em Linux; o
comportamento no Git Bash do Windows é **[Dado a verificar]** (§9).

### 3.1 Smoke test: o que mudou e por quê

O script original usava `TS="2026-11-04T14:00:00Z"` e esperava que essa leitura
fosse a "atual" depois de outra **sem timestamp**, recebida em 23/11. Pelo
contrato §3.4 a leitura mais recente é a de maior
`COALESCE(timestampDispositivo, recebidoEm)`; logo 23/11 vence 04/11. **A API
está correta; a expectativa do script estava errada.**

Mudanças em `scripts/smoke-test-integracao.sh`:

- timestamps **explícitos e ordenados**, independentes do relógio e da velocidade;
- bloco **R** (sem timestamp) separado: confere que os dois `recebidoEm` são
  distintos antes de afirmar a ordem; senão, "inconclusivo";
- leitura **semântica** de JSON (`jq` ou `python3`), sem depender da ordem dos
  campos nem de `20` × `20.0`;
- ids únicos por execução (`SW12-…`); os payloads inválidos também usam um id
  isolado, nunca J01–J04; arquivos temporários com `mktemp` (removidos ao sair);
- **validação rigorosa da URL antes de qualquer requisição:** só
  `http(s)://host[:porta]`; recusa credenciais embutidas (`@`), caminho, query,
  fragmento, espaços, barra invertida, porta vazia ou fora de 1–65535 e host
  malformado. Só aceita `localhost`, `127.0.0.1` e `[::1]`; outro host exige
  `SMOKE_PERMITIR_REMOTO=1`, e mesmo assim a URL precisa ser válida. O modo
  `--autoteste-url` verifica isso sem rede (testes 13 e 14);
- timeout em toda requisição (`SMOKE_TIMEOUT`, padrão 15 s);
- o interpretador de JSON é testado antes do uso (`jq`, depois `python3`, depois
  `python`); se nenhum funcionar, o script para com uma mensagem clara;
- aviso destacado de que o script **grava** dados e exige banco descartável
  (11 jardins `SW12-…` e cerca de 40 leituras por execução);
- novos blocos: estado de reserva e boot (F), tendência (N), lista × detalhe
  (L), erros (E), CORS com preflight (C);
- **F1 e F2 são "achados"**, não falhas, porque dependem de decisão de domínio
  (§4); `--estrito` os transforma em falha.

---

## 4. Problemas — consolidados em uma única rodada

Todos foram **verificados novamente** no HEAD. Nenhum foi corrigido.

### F1 — Reinício com horário estimado · persiste · **Média** · decisão de domínio em aberto

- **Evidência:** bloco S do smoke. Boot A termina em 12 cm às `2026-10-15T00:11:00Z`;
  o boot B (depois, no tempo real) lê 30 cm às `2026-10-15T00:00:05Z`
  (`horaSincronizada:false` recomeça em 15/10). `GET /api/jardins/{id}` devolve
  **12 cm**.
- **Impacto:** com a hora estimada, o dashboard mostra a leitura do boot
  antigo, e a tendência mistura os dois boots.
- **Atenuante [Provável]:** o firmware do #24 liga NTP e usa o header `Date`
  como alternativa, então só ocorre sem sincronização. Não verificado no Wokwi.
- **Responsável:** API (SW-14, Júlia) + decisão do grupo. Contrato §3.4 já
  registra "em aberto: tratar `horaSincronizada:false`".
- **Correção mínima sugerida:** o grupo decide a regra (por exemplo, ordenar por
  `recebidoEm` quando `horaSincronizada=false`); depois, um teste que reproduza a
  sequência acima.

### F2 — `boot` como leitura atual · persiste · **Média** · decisão de contrato

- **Evidência:** bloco S do smoke. Após `tipo:"boot"` (nível 0,0, `sensorId:"jardim"`)
  com timestamp mais novo, `GET /api/jardins/{id}` devolve `nivelCm: 0.0`.
- **Impacto [Suposição]:** se um registro `boot` tiver o timestamp mais novo do
  jardim, o dashboard exibirá 0 cm fictício até o próximo snapshot. Isso foi
  **reproduzido enviando o registro por POST**. **Não foi medido** se o firmware
  chega a gerar esse caso: o boot é gravado no início do boot, possivelmente
  antes de o relógio sincronizar (hora estimada, situação do F1); nesse caso o
  boot teria timestamp antigo e **não** apareceria como leitura atual. O boot
  **já** é excluído da tendência e do campo `estado`; o dicionário diz que
  `totalLeituras` inclui o boot e que `nivelCm` é "o da última leitura", então
  o comportamento segue o texto.
- **Responsável:** API (Júlia).
- **Correção mínima sugerida:** ignorar `tipo=boot` na consulta da "última
  leitura" (detalhe e lista) e ajustar o dicionário; `totalLeituras` continua
  contando tudo. Um teste de repositório.

### F3 — Limiares divergentes · persiste · **Média** (alta se a apresentação usar o simulador) · decisão de domínio

- **Evidência (reproduzível):** classificar 0 a 65 cm (passo 0,5) com o código
  real dos dois lados.
  Firmware: `classificarEstado()` de `esp32/src/modulos/estado_fisico.cpp`
  (22 / 45 / 60 cm, histerese de 2 cm). Frontend: `calcularStatusAlerta()` de
  `frontend/src/types/jardim.ts` (15 / 30 / 45 cm). **74 de 131 níveis divergem**,
  em três faixas: 15–21,5 cm (firmware NORMAL, frontend ENCHENDO), 30–44,5 cm
  (ENCHENDO × TRANSBORDANDO) e 45–59,5 cm (TRANSBORDANDO × ALAGADO).

  ```bash
  # firmware (g++): laço de 0 a 65 cm chamando classificarEstado(n, anterior)
  g++ -std=c++17 -I esp32/src/modulos fw.cpp esp32/src/modulos/estado_fisico.cpp -o fw
  # frontend: empacotar a função real e varrer os mesmos níveis
  frontend/node_modules/.bin/esbuild entrada.ts --bundle --platform=node --format=cjs --outfile=front.cjs
  ```

- **Quando aparece:** só quando a API devolve `estado: null`. O simulador
  **nunca** envia `estado`, então todos os cards do teste mostraram estado
  calculado no frontend (por exemplo J01 com 20,9 cm aparece "Enchendo"; o
  firmware diria NORMAL). Com `estado` do dispositivo, a tela usa o da API
  (testado: `estado=TRANSBORDANDO` com 20 cm apareceu "Transbordando").
- **Impacto:** o dashboard e o totem podem discordar para o mesmo nível. O texto
  do card ("Sensor do totem físico … identifica nível em elevação") afirma algo
  sobre o totem mesmo quando o estado foi calculado localmente.
- **Responsável:** frontend (Larissa) e simulador/API (Júlia, Arthur). Os limiares
  do firmware estão marcados como **PROVISÓRIOS** em comentário de
  `esp32/src/modulos/estado_fisico.h` (o comentário remete a
  `docs/decisoes-tecnicas.md` §3, arquivo que não existe no repositório; ver §7)
  e os do frontend são "faixas do protótipo": **o grupo precisa dizer quais valem**.
- **Correção mínima sugerida:** com `estado` nulo, mostrar "Sem classificação" em
  vez de derivar; ou o simulador enviar `estado` com os limiares do firmware;
  depois alinhar as constantes.

### F4 — 404 mostrado como "API indisponível" e dado antigo mantido · persiste · **Baixa**

- **Evidência:** E2E fase 3. Com a API no ar e **vazia** (20 respostas 404), o
  banner diz "API indisponível" e os cards continuam com as leituras antigas.
  Na carga inicial com a API vazia, os cards mostram "Sem leituras recebidas"
  (verificado na rodada anterior, antes dos merges; o código do frontend é o
  mesmo, mas esta carga inicial não foi repetida no HEAD novo).
- **Responsável:** frontend (Larissa).
- **Correção mínima sugerida:** tratar `JARDIM_NAO_ENCONTRADO` como "sem leituras":
  limpar o dado e não mostrar o banner de indisponibilidade.

### F5 — Polling duplicado do jardim em foco · persiste · **Baixa**

- **Evidência:** E2E. `GET /api/jardins/J01` fez **6** requisições em 15 s (o
  esperado seria 3): `useJardinsRede` e `useJardimData` consultam o mesmo jardim.
- **Responsável:** frontend. **Correção:** reaproveitar os dados da rede para o
  jardim em foco, ou um único polling.

### F6 — Rótulos do gráfico · persiste · **Baixa**

- **Evidência:** captura do HEAD: rótulos "19 cm", "21 cm" enquanto o card mostra
  20,9 cm; eixo X repete "21:34". O card e a API preservam o decimal.
- **Responsável:** frontend. **Correção:** uma casa decimal nos rótulos e
  `HH:mm:ss` no eixo.

### Divergências de documentação (não bloqueiam)

Ver §7.

---

## 5. Integração real no navegador (HEAD `8a0bd3f`)

Montagem: API do mesmo HEAD (H2 em memória, perfil `simulacao`), simulador
`GeradorDadosJardim` (J01–J04), `vite` com `VITE_API_BASE_URL=http://localhost:8080`,
Chromium via Playwright.

| Verificação | Resultado |
|---|---|
| J01–J04 com nível, estado, tendência e última atualização | OK |
| Nível exibido = um dos últimos valores devolvidos pela API (4 jardins) | OK |
| `estado` do dispositivo prevalece sobre o cálculo local | OK (fase 5) |
| Tendência exibida = `tendencia` da API | OK |
| CORS (`Access-Control-Allow-Origin: http://localhost:5173`) em 200 e 404 | OK |
| Histórico/gráfico com leituras reais | OK (até 20 leituras) |
| API fora: aviso + último dado mantido | OK |
| API de volta + simulador: recupera **sem recarregar** | OK |
| 404 real (API vazia): comportamento | ver F4 |
| Polling a cada 5 s | OK; ver F5 |

Os arquivos brutos (JSON do E2E e capturas de tela) ficam fora do repositório.

---

## 6. Wokwi: execução real (24/11) e histórico das pendências

### 6.1 O que existe de evidência (não reproduzida aqui)

- 14/11: `validacao-wokwi.md` §3 registra Wokwi real postando na API via Private
  Gateway (firmware **antigo**, antes do #24).
- 19/11: `validacao-sinalizacao.md` registra a sinalização do totem com receptor
  temporário (webhook.site). A API local **não** foi usada.
- Pendentes até 23/11: **3.14** (nível 42,5 cm após a correção do codec de 20/11) e **3.16**
  (receptor fora do ar). **Executados em 24/11: ver §6.4.**

### 6.2 O que o ambiente da rodada de 23/11 (Linux) permitia

| Etapa | Estado |
|---|---|
| Testes de host (g++) | executado (14/14) |
| Firmware compilado para ESP32 (`pio run`) | **bloqueado:** `pio` ausente; na rodada anterior, `pio run` falhou ao baixar a plataforma `espressif32` (`HTTPClientError`) |
| Simulação no Wokwi | **bloqueado:** `wokwi-cli` e token ausentes; `wokwi.com` sem resposta |
| Wokwi → API Spring Boot | **não executado** (exige Private Gateway) |

**Acesso externo, registrado com exatidão:** nenhum dado do projeto foi enviado a
serviços externos. Houve apenas **sondagens de conectividade** (uma requisição
simples, sem corpo, a cada host): `wokwi.com`, `registry.platformio.org`,
`api.registry.platformio.org` e `dl.platformio.org`. Todas ficaram **sem resposta
(código 000)**; a causa (rede do ambiente, política de acesso ou outra) **não foi
determinada**. Na rodada anterior também houve a instalação do PlatformIO por
`pip` em ambiente isolado e uma tentativa de `pio run`, que falhou no download da
plataforma. Dependências de desenvolvimento do Maven e do npm foram baixadas
normalmente.

### 6.3 Passos usados (roteiro de 23/11; executados em 24/11, ver §6.4)

1. API local, banco descartável:

   ```bash
   cd api-jardim-sentinela
   SPRING_DATASOURCE_URL='jdbc:h2:mem:wokwi;DB_CLOSE_DELAY=-1' SPRING_H2_CONSOLE_ENABLED=false ./mvnw spring-boot:run
   ```
2. `esp32/src/secrets.h` (ignorado pelo git): `WIFI_SSID "Wokwi-GUEST"`,
   `WIFI_SENHA ""`, `URL_RECEPTOR "http://host.wokwi.internal:8080/api/leituras"`.
3. `cd esp32 && pio run` e conferir
   `strings -a .pio/build/esp32dev/firmware.merged.bin | grep host.wokwi.internal`.
4. Wokwi com **Private Gateway**; **parar e iniciar** a simulação (EN/RST não
   recarrega o binário).
5. **Cenário 3.14:** `pot4` em 42,5 cm. Registrar o `POST` (201) e confirmar:
   `curl -s localhost:8080/api/jardins/J01 | jq '{nivelCm,estado}'` com
   `nivelCm` = **42.5** (e **não** 43.0), `idJardim` J01 e `seq` crescente.
6. **Cenário 3.16:** com a simulação em `pot4` ≈ 65 % (ALAGADO), **parar a API**.
   Registrar que o vermelho continua piscando e o buzzer segue em pulsos
   (nunca contínuo) durante os POSTs que falham; depois em ≈ 46 % com `SUBINDO`.
   Subir a API de novo e registrar a retransmissão (200 nos já recebidos, sem
   duplicar).
7. Rodar `scripts/smoke-test-integracao.sh` e o simulador na mesma API.
8. Registrar em `validacao-sinalizacao.md` (3.14 e 3.16): data, SHA, `wokwi-cli`
   usado, e os logs. Só então marcar como executado.

### 6.4 Execução real em 24/11/2026 (máquina do responsável)

Evidência detalhada, com marcação de origem, em
[`evidencias-sw12-wokwi-2026-11-24.md`](evidencias-sw12-wokwi-2026-11-24.md).
Legenda: **observado** (visto na tela durante a execução) · **log** (serial/API) ·
**relatado** (dito por Rafael) · **código** (inferido da leitura do firmware).

**Ambiente:** clone isolado da `develop` (HEAD `5a36b2d`, inclui o PR #26);
PlatformIO 6.2.0 (`esp32dev`, `-DSIMULATION`), build SUCCESS; extensão Wokwi para
VS Code 4.1.0 (Community License) com o Private IoT Gateway embutido
(`host.wokwi.internal`); API Spring Boot 4.1.1 com H2 em memória, JDK 25.0.4.
`secrets.h` com valores de teste (ignorado pelo git). Nenhum código nem
configuração versionados foi alterado para os testes.

| Cenário | Resultado | Evidência |
|---|---|---|
| **3.14** nível 42,5 cm sem arredondar para 43 | **PASSOU** | log: `HW-02 ADC: 1741 \| Nivel: 42.52 cm`; API: `nivelCm":42.5` (ex.: id 52), nunca 43 |
| **3.16** totem sem receptor | **VALIDADO COM RESSALVAS** | ver abaixo |

**3.16 — o que está comprovado**

- **Observado/log, ALAGADO (65 cm, API fora):** `TOTEM: estado=ALAGADO
  tendencia=SUBINDO som=permitido`; vermelho piscando; leitura e serial seguem.
- **Log, subida 40,27 → 46,13 cm:** `ENCHENDO/SUBINDO som=calado` abaixo de 45 cm;
  `TRANSBORDANDO/SUBINDO som=permitido` ao passar de 45 cm; depois `ESTAVEL`.
  **Observado:** vermelho fixo em TRANSBORDANDO (sem piscar).
- **Log, timeout:** com um listener TCP temporário no lugar da API (aceita a
  conexão e nunca responde): conexão aceita às 02:20:13 (**relatado**, print do
  listener) e, no serial, `HTTP: falha -1`, `ENVIO: rede/timeout; falha 1,
  proxima em 2 s`, `ENVIO: lote 0/1 2xx (rede/timeout)`. O POST ficou sem
  resposta até estourar o timeout (30 s simulados no `-DSIMULATION`) e nunca
  recebeu 2xx.
- **Código:** o totem roda em tarefa FreeRTOS própria (`iniciarTarefaTotem`, 20 ms),
  e há checagem estática em `run_tests.py` que falha se o `loop()` voltar a chamar
  `atualizarTotem()`. É isso que sustenta que o POST bloqueante não trava o LED.

**3.16 — ressalvas (não confirmadas)**

1. **Buzzer:** há apenas o **relato** de Rafael de que o buzzer tocou, com ritmo, e
   que o ritmo parecia acelerar (hipótese não confirmada: a velocidade da simulação
   variou de 19% a 61% e o áudio acompanha). Cadência de 3 pulsos a cada 2 s e
   "nunca contínuo" **não** foram verificados de forma objetiva; só o log `som=`.
2. **Vermelho piscando dentro da janela exata do bloqueio:** não há sequência de
   quadros mostrando a alternância durante os 30 s. As capturas de 1 s ficaram
   alinhadas ao período do pisca com a simulação em velocidade variável. O piscar
   foi observado antes e depois; durante, é **inferido pelo código**.
3. **Ponto de 46% do roteiro:** a linha `ESTAVEL som=calado` exatamente em 46 cm não
   foi localizada no serial; `som=calado` em `ESTAVEL` aparece em 54–57 cm.
4. **Só simulador:** não existe hardware real até agora. As correções de buzzer e
   totem estão validadas apenas no Wokwi.
5. **Tipos de falha:** o timeout completo foi provado com o listener (conexão aceita
   sem resposta). Com a API simplesmente desligada, o log registrou
   `ENVIO: lote 0/1 2xx (rede/timeout)`; que essa falha seja rápida (recusa de
   conexão) é **suposição**, não foi cronometrada.

---

## 7. Documentação: histórica × operacional

| Documento | Situação | Ação sugerida |
|---|---|---|
| `checklist-integracao-sprint2.md` (§0–5) | **histórico** de 14/11 | mantido; seção 6 acrescentada (SW-12) |
| `relatorio-integracao-sw12.md` | **operacional** (este) | SW-12 |
| `frontend/INTEGRACAO-API.md` (linhas 10–21) | **desatualizado:** diz que `GET /api/jardins`, `estado` e `tendencia` "não existem" na `develop`; já existem (#22, #23) | **Larissa** (#26) |
| `contrato-telemetria.md` §1.1 | tabela de campos **sem** `estado`, `tendenciaDispositivo`, `versaoRegra` (o DTO e o dicionário têm) | **Júlia** |
| `contrato-telemetria.md` §2 | cita a branch `feat/SCRUM-31-…` | **Júlia** |
| `relatorio-tarefa-c.md` | diz "PR Aberto"; o #24 está integrado | **Júlia** |
| `validacao-sinalizacao.md` | roteiro de 19/11; 3.14 e 3.16 ainda marcados como pendentes, **mas executados em 24/11 (§6.4)** | **Júlia** (registrar os resultados do §6.4 na tabela dela) |
| `validacao-wokwi.md` | histórico de 14/11 (firmware antigo) | manter como histórico |
| Referências a `docs/decisoes-tecnicas.md` | **arquivo inexistente:** 31 ocorrências em `develop` (código e testes do firmware, `docs/api-dashboard.md`, `docs/tendencia-nivel.md`, `docs/relatorio-tarefa-c.md`); nenhum commit em nenhuma ref consultada o criou, e não há arquivo de nome parecido | **Júlia** (autora das referências): publicar o documento ou ajustar as referências. A SW-12 **não** substitui a referência por outra |

Nesta entrega só foram alterados os arquivos da SW-12: o smoke test, o checklist
e este relatório. As demais atualizações ficam como proposta aos responsáveis.

---

## 8. Como reproduzir

**Pré-requisitos** (versões usadas nesta rodada; o CI usa JDK 17, Node 22 e
Python 3.12): JDK compatível com `api-jardim-sentinela/pom.xml`, Node 22 com
npm, Python 3, `g++` (testes de host do firmware), `bash` e `curl`, e `jq` **ou**
`python3`/`python` para o smoke. Portas livres: 8080 (API) e 5173 (dashboard).
No Windows, use o Git Bash e mantenha o fim de linha do script em LF.

```bash
# 0) testes automatizados (a partir da raiz do repositório)
(cd api-jardim-sentinela && ./mvnw -B -ntp test)
(cd frontend && npm ci && npm test && npm run lint && npm run build)
(cd esp32 && python3 test/host/run_tests.py --verbose && python3 test/receiver/test_receber.py)

# 1) API com banco DESCARTÁVEL (compila e sobe; deixe este terminal aberto)
cd api-jardim-sentinela
SPRING_DATASOURCE_URL='jdbc:h2:mem:smoke;DB_CLOSE_DELAY=-1' SPRING_H2_CONSOLE_ENABLED=false \
SPRING_PROFILES_ACTIVE=simulacao ./mvnw spring-boot:run

# 2) outro terminal, raiz do repositório: validação da URL e smoke test
bash scripts/smoke-test-integracao.sh --autoteste-url   # sem rede
bash scripts/smoke-test-integracao.sh                   # achados F1 e F2 informativos
bash scripts/smoke-test-integracao.sh --estrito         # achados viram falha (hoje: exit 1)

# 3) simulador J01–J04 (60 s); exige as classes compiladas pelo passo 1
java -cp api-jardim-sentinela/target/classes br.edu.cesar.jardimsentinela.gerador.GeradorDadosJardim http://localhost:8080/api/leituras 60

# 4) dashboard apontado para a API (já com `npm ci` feito no passo 0)
cd frontend && VITE_API_BASE_URL=http://localhost:8080 npm run dev   # http://localhost:5173
```

O smoke **grava** dados (11 jardins `SW12-…`): rode-o só contra um banco
descartável. Outro host exige `SMOKE_PERMITIR_REMOTO=1`; o tempo máximo de cada
requisição é `SMOKE_TIMEOUT` (padrão 15 s).

---

## 9. O que não foi executado ou não está coberto

| Item | Situação |
|---|---|
| Wokwi → API Spring Boot real, firmware do #24 (3.14 e 3.16) | **Executado em 24/11** (§6.4): 3.14 passou; 3.16 com ressalvas (buzzer, piscar na janela do bloqueio, só simulador) |
| Firmware compilado para o ESP32 (`pio run`) | **Executado em 24/11** (build SUCCESS, PlatformIO 6.2.0) |
| Reinício real do ESP32 e retransmissão da fila feita pelo firmware | **Não executado.** Só o lado da API foi exercitado (idempotência por `bootId+seq`) |
| NTP e `Date` como fonte de hora no firmware (atenuante do F1) | **Não verificado** |
| Hardware real (ESP32 físico) | **Nunca executado:** só simulador |
| Painel COP | **Continua mockado** (não integrado; fora do escopo desta rodada) |
| PRs #30 (SW-15) e #31 (SW-16) | **Não testados** |
| Carga inicial do dashboard com a API vazia, no HEAD novo | **Não repetida** (ver F4) |
| Smoke e fallback `python` no Git Bash do Windows | **[Dado a verificar]:** testado só em Linux |
| CI | Consultado, **não repetido** localmente |

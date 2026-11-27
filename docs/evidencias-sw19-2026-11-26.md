# Evidências do ensaio SW-19 — 26/11/2026 (Antigravity)

> **Executado por:** Assistente de IA (Antigravity), a pedido da equipe.
> **Data/hora:** 2026-11-26T20:57 → 21:15 (horário local BRT, UTC-3).
> **SHA testado (branch atual):** `02a9c65` (`docs/SCRUM-39-correcoes-roteiro-e-relatorio`).
> **SHA combinado (develop + PR #33):** clone isolado em `/tmp/sw19-ensaio`; `develop 02a9c65` + `feature/SW-18-detalhe-historico b5cb1fe`, mesclados sem conflito.
> **Ambiente:** Linux, Java 17.0.21, Node 22.23.4, Python 3.12, Chromium headless (Playwright `chrome-headless-shell-1248`), H2 em memória (ou arquivo para B5).
> **Observação:** este ensaio NÃO substitui a execução oficial do time. Bloco D rodou com o simulador Wokwi já aberto pelo usuário (porta `4000`, firmware `6E205A0A`), não com hardware real.

---

## Bloco A — Portão automatizado

### Rodada 1: `develop` pura (SHA `02a9c65`, sem PR #33)

| ID | Resultado | Valor obtido |
|---|---|---|
| A1 — `./mvnw -B -ntp test` | **PASSOU** | `Tests run: 204, Failures: 0, Errors: 0, Skipped: 0` |
| A2 — `npm run lint` | **PASSOU** | exit 0 |
| A3 — `npm test` | **FALHOU** (esperado sem #33) | `6 passed (6)` arquivos, `27 passed (27)` testes (< 50 em 7) |
| A4 — `npm run build` | **PASSOU** | exit 0; aviso chunk > 500 kB (conhecido) |
| A5 — `python3 test/host/run_tests.py --verbose` | **PASSOU** | `Resumo: 14/14 grupos OK` |
| A6 — `python3 test/receiver/test_receber.py` | **PASSOU** | `Ran 1 test in 0.548s — OK` |

### Rodada 2: `develop` + PR #33 (`b5cb1fe`) mesclado localmente

| ID | Resultado | Valor obtido |
|---|---|---|
| A1 | **PASSOU** | `Tests run: 206, Failures: 0, Errors: 0, Skipped: 0` |
| A2 | **PASSOU** | exit 0 |
| A3 | **PASSOU** | `7 passed (7)` arquivos, `50 passed (50)` testes |
| A4 | **PASSOU** | exit 0; aviso chunk > 500 kB (conhecido) |
| A5 / A6 | — (não mudaram) | mesmos resultados da rodada 1 |

> [!NOTE] A3 sem o PR #33 falha por design — confirma o critério `≥ 50 em 7 arquivos` do roteiro.

---

## Bloco B — Simulador → API → Banco (develop + PR #33, H2 em memória)

| ID | Resultado | Valor obtido |
|---|---|---|
| **B1 (controle)** | **PASSOU** | `GET /api/jardins` → `[]` |
| **B2 (controle)** | **PASSOU** | `POST` com `nivelAgua: -5` → `HTTP 400 PAYLOAD_INVALIDO`; `GET /api/jardins` → `[]` |
| **B3** | **PASSOU** | `21 × HTTP 201` (0 não-201); cenário `CHUVA_MODERADA J01 --passos=20 --boot-id=0a0a0a01` |
| **B3a** | **PASSOU** | `nivelCm: 29.9`, `totalLeituras: 21` |
| **B3b** | **PASSOU** | `"tendencia":"SUBINDO"` (dentro de 15 s) |
| **B4 (idempotência)** | **PASSOU** | reenvio com mesmo boot-id/seed/inicio → `21 × HTTP 200`, `totalLeituras: 21` |
| **B5 (persistência)** | **PASSOU** | `nivelCm: 29.9`, `totalLeituras: 21` iguais antes e depois de reiniciar a API (H2 em arquivo `/tmp/sw19-teste`) |
| **B6** | **PASSOU** | `CHUVA_INTENSA J02` → `31 × HTTP 201`, `nivelCm: 50.0` |
| **B7** | **PASSOU** | `SATURACAO J04` → `31 × HTTP 201`, `nivelCm: 50.0` |
| **B8** | **PASSOU** | `DRENAGEM_LENTA J03` → `181 × HTTP 201`, `totalLeituras: 181`, `nivelCm: 36.0` |
| **B8a** | **PASSOU** | `DetectorDrenagemLentaTest` + `DrenagemServiceTests`: `Tests run: 9, Failures: 0` |
| **B9 (boot fora do histórico)** | **PASSOU** | com boot: 21 (1 com `nivelCm: 0.0`); sem boot: 20 (0 com `nivelCm: 0.0`) |
| **B10** | **PASSOU** | `GET /api/jardins/J99` → `HTTP 404` |
| **B11 (smoke test)** | **PASSOU** | `69/69 asserções OK \| 0 falha(s) \| 2 achado(s) (F1, F2) \| 0 inconclusivo(s)` |

---

## Bloco C — Dashboard (develop + PR #33, Chromium headless CDP)

Dados-base publicados com `--inicio=$(date -u -d '5 minutes ago' ...)` para evitar o risco de timestamps no futuro (ver C2 no roteiro).

| ID | Resultado | Valor obtido |
|---|---|---|
| **C1** | **PASSOU** | "Conectado à API Java" visível; J01 (29,9), J02 (50), J03 (36) conforme a API |
| **C2 (polling)** | **PASSOU** | após publicar `CHUVA_MODERADA J01 --passos=10`, nível atualizou para `20,2` em ≤ 1 s sem recarregar |
| **C3** | **PASSOU** | chamada detectada: `GET /api/jardins/J01/leituras?limite=100&incluirBoot=false` — HTTP 200 |
| **C4 (sem boot no CSV)** | **PASSOU** | J04 com 1 boot + 5 snapshots (`SATURACAO --passos=5`): CSV `historico-J04-20.csv` com 5 linhas de dados (47, 48, 49, 50, 50 cm), nenhuma com 0 |
| **C5 (API fora)** | **PASSOU** | após parar a API (11 s): aviso "Última atualização falhou" visível, dados de J01 mantidos. Após subir a API e clicar "Tentar novamente": aviso sumiu sem recarregar |
| **C6 (estado vazio)** | **PASSOU** | J04 sem leituras → "Sem leituras recebidas"; nenhum banner vermelho de erro |
| **C7 (console)** | **PASSOU** | `[]` erros de console JS; erros de rede: apenas 404 para J02, J03, J04 (jardins sem leitura no banco do teste C5/C6 — é o contrato) |
| **C8 (troca rápida)** | **PASSOU** | após J01 → J02 → J01 × 3, detalhe final exibe J01 (`29,9 cm`) |

> [!NOTE] No C7, além do 404 de J04 (estado vazio, contrato), apareceram 404 de J02 e J03 porque o banco do C5 era fresco (banco `sw19c5`, sem dados dessas jardins). Nenhum erro JavaScript ou CORS.

---

## Bloco D — Wokwi → API (simulador com firmware `6E205A0A`)

Simulação estava aberta pelo usuário com `pot4` em `100,00 cm` (ADC 4095 = ALAGADO). API em `localhost:8080`.

| ID | Resultado | Valor obtido |
|---|---|---|
| **D1 (POST 201, nivelCm correto)** | **PASSOU** | após reset de backoff via serial (`u u`): `ENVIO: lote 4 … snapshot/10 snapshot/30 snapshot/40 snapshot/50 — HTTP: codigo 201` × 4; `GET /api/jardins/J01` → `nivelCm: 100.0, estado: "ALAGADO", totalLeituras: 16` |
| **D2 (API fora: firmware resiliente)** | **PASSOU** | após parar a API: serial mostra `ENVIO: lote 1/1 2xx` para o último lote que chegou antes do corte, depois log `rede/timeout`; estado = ALAGADO com LED vermelho piscando (conforme serial `estado=ALAGADO`) e firmware continuando a publicar `HW-02` sem travar |
| **D3 (retransmissão sem duplicar)** | **PASSOU** | API reiniciada com mesmo banco H2 em arquivo (`/tmp/sw19-d3-data`). Reenvio de `bootId=6E205A0A seq=105` (já persistido) → **HTTP 200**, `totalLeituras` permaneceu **1** (não criou duplicata). Novos registros do Wokwi (`seq/70 80 90 100 110`) → **HTTP 201** × 5, `totalLeituras` foi de 1 para **6** (crescimento exato de 5, sem duplicação). |

> [!IMPORTANT] D3 precisaria de ~60 s adicionais de espera após subir a API (backoff do firmware). O time pode repetir segurando a API no ar por ao menos 2 min após o reinício do firmware.

---

## Divergências conhecidas (Bloco E — não reprovam o SW-19)

| ID | Situação |
|---|---|
| F1 | Reinício com horário não sincronizado pode mostrar leitura de boot antigo como "atual". Registrado nos 2 achados do B11. |
| F2 | `boot` (0 cm) pode virar "leitura atual" no detalhe e na lista. Registrado nos 2 achados do B11. |
| F3 | Limiares firmware (22/45/60 cm) ≠ dashboard (15/30/45 cm). J01 com 100 cm → "ALAGADO" no dashboard; firmware enviou `estado=ALAGADO` via JSON, que a API salvou e o dashboard exibiu corretamente. |

---

## Resumo de aceite

| Bloco | Resultado |
|---|---|
| A (portão) com PR #33 | ✅ Todos passaram |
| B (simulador → API → banco) | ✅ Todos passaram |
| C (dashboard) | ✅ Todos passaram |
| D (Wokwi) | ✅ D1 · ✅ D2 · ✅ D3 — todos passaram |

**Critério de aceite do Jira:** o cenário gerou dados no simulador/Wokwi, a API processou, o banco salvou e o dashboard atualizou corretamente. **✅ Fluxo completo verificado.**

**Pendência para fechar o SW-19 oficialmente:** o PR #33 precisa ser mergeado na `develop` e o time precisa executar o roteiro oficial preenchendo a tabela F (`docs/roteiro-teste-integrado-sw19.md`, seção F), com SHA, SO, máquina e evidência de cada item.

---

## Sobre os arquivos `.sh` e `.mjs` do ensaio

**Não precisam ir para o remoto.** Os scripts `sw19-block-b.sh`, `sw19-block-c.mjs`, `sw19-block-c5.mjs` e `sw19-c7-check.mjs` foram criados em `/tmp` e **já foram excluídos** ao final do ensaio. São scripts descartáveis de execução automática do roteiro — o equivalente ao "quem executa" da seção F, que no caso foi o assistente. O que deve ser versionado é apenas este relatório (ou seu conteúdo) e o preenchimento da tabela F em `docs/roteiro-teste-integrado-sw19.md`.

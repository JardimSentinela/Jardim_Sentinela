# Checklist de integração — Sprint 2 ([SCRUM-21] [SW-12])

> **Lead:** Rafael Carrilho. **Participam:** Arthur Vitorino, Julia Tenorio,
> Gabriel Rios, Larissa, Dandara, Adrielly.
> **Critério de aceite do card:** Wokwi e simulador alimentam a API; dados
> persistem; endpoints respondem; a interface gerada consegue consumir dados
> de teste.
> **Dependências:** SW-06, SW-07, SW-08, SW-09, SW-11, DS-02.

> **Atualização de 23/11/2026:** as seções 0 a 5 abaixo são o **registro
> histórico da Sprint 2 (14/11)** e foram mantidas como estavam. A rodada de
> validação sobre a `develop` atual e o dashboard do PR #26 está na
> **seção 6**; o relatório técnico completo está em
> [`relatorio-integracao-sw12.md`](relatorio-integracao-sw12.md).

Este documento mapeia as 5 subtarefas sugeridas do card ao estado real do
código em `develop` (analisado via Jira + repositório GitHub, sem acesso ao
computador local de ninguém — qualquer item marcado **[Dado a verificar]**
precisa de confirmação rodando de verdade).

**Escopo:** SCRUM-38 (SW-06) fica de fora desta entrega de propósito —
é task do Arthur, que já sinalizou que pode estar pronta (possivelmente feita
por outra pessoa do time) e pediu pra ninguém mexer em `develop` antes dele
confirmar. O código já existe (ver item 5 abaixo), mas a confirmação e
qualquer ajuste ficam com ele.

## 0. Fluxo TDD aplicado nesta entrega

A partir desta rodada, os testes foram escritos **antes** da implementação
(ciclo vermelho → verde), pra quem for aplicar os commits poder reproduzir o
ciclo: rodar `./mvnw test` só com o teste (vê falhar), depois aplicar o código
(vê passar). Dois arquivos de teste novos:

- `LeituraIngestaoTests.java` — cobre SW-09 (`@JsonAlias`) e SW-10
  (idempotência + erro padronizado). Sem a implementação, a 2ª e a 3ª
  asserção de cada teste devem falhar (DTO antigo rejeita os nomes do
  firmware; sem `TratadorDeErros`, o corpo do erro não tem o formato
  esperado).
- `WebConfigCorsTests.java` (novo) — cobre SW-11 (CORS). Sem `WebConfig.java`,
  a API não manda nenhum cabeçalho `Access-Control-*`, então as duas
  asserções de `header().exists(...)` devem falhar.

**Atualização:** Já rodei. Os 12 testes automatizados (`LeituraIngestaoTests`,
`WebConfigCorsTests`) passam (BUILD SUCCESS). O smoke test ponta a ponta
também rodou contra a API real; 1 checagem falhou por bug no próprio script
(comparação truncada contra JSON com ordem de campo alfabética), corrigido
nesta mesma entrega — não era um problema na API.

## 1. Teste ponta a ponta parcial

**Script pronto:** `scripts/smoke-test-integracao.sh` (requer a API rodando
em `localhost:8080`). Cobre: API de pé, ingestão pelos dois vocabulários,
idempotência, consulta de estado atual, histórico, erro de validação, 404,
e cabeçalho CORS presente.

```bash
cd api-jardim-sentinela
./mvnw test                               # confirma os testes automatizados
./mvnw spring-boot:run &                  # sobe a API
../scripts/smoke-test-integracao.sh    # roda o smoke test
```

**Atualização:** Rodado contra a API real (H2 local), 9 etapas, todas
passando após a correção do script (ver seção 0).

## 2. Correções de contrato

Feito nesta entrega (ver `docs/contrato-telemetria.md`, seção "Resolução
parcial"): `LeituraRequest` aceita os dois vocabulários (`jardimId`/`nivelCm`
do simulador e `idJardim`/`nivelAgua`/`timestamp` do firmware atual) no
mesmo endpoint, via `@JsonAlias`. Isso fecha SW-09 sem esperar a equipe
decidir o nome oficial.

**Ainda em aberto (decisão da equipe, não é código):**
- Nome final do contrato (ver "O que isso NÃO resolve" no doc).
- `bootId`/`seq` do formato NDJSON mais rico só existem na branch
  `feat/SCRUM-34-HW-03-...`, que ainda não foi mergeada em `develop` — o
  card **HW-04** ([SCRUM-36], status "Validação" no Jira) é quem traz isso.
  Até lá, a idempotência usa `timestamp`.

## 3. Validar exemplos JSON

Exemplos reais (não inventados — vêm dos testes automatizados e do código
de `persistir()` no firmware), com os dois vocabulários, estão em
`docs/api-dashboard.md` e nos testes `LeituraIngestaoTests`. Resumo:

| Origem | Exemplo | Endpoint | Resultado |
|---|---|---|---|
| Simulador | `{"jardimId":"J01","sensorId":"S01","nivelCm":18.5,"bateria":97}` | `POST /api/leituras` | 201 |
| Firmware (formato de `main_alt.cpp`, ids no padrão novo) | `{"idJardim":"J01","idSensor":"S02","nivelAgua":10.0,"bateria":100,"timestamp":"2026-10-15T00:00:01Z"}` | `POST /api/leituras` | 201 |
| Qualquer um, campo faltando | `{"sensorId":"S01","nivelCm":1,"bateria":1}` | `POST /api/leituras` | 400, `PAYLOAD_INVALIDO` |

## 4. Testar consumo inicial pelo código gerado

> **[Superado em 23/11/2026]** O dashboard existe (PR #19 na `develop`; PR #26
> conectado à API) e foi exercitado no navegador contra a API real. Ver seção 6.
> O texto abaixo é o registro de 14/11.

**Bloqueado (em 14/11):** DS-02 ("Finalizar dashboard e gerar código-base") está "Em
andamento" com a Adrielly — ainda não existe código gerado pra testar
consumindo a API de verdade. O que dá pra adiantar sem o código gerado
existir:

- `docs/api-dashboard.md` tem os 3 formatos de resposta reais (estado atual,
  histórico, erro) prontos pra virar mock (MSW/json-server) — a Adrielly ou
  quem for integrar o front pode trabalhar contra o mock sem esperar o
  backend local subir.
- CORS já configurado (`WebConfig.java`) pra quando o código gerado rodar
  num host diferente da API.

Quando o código gerado existir, falta: apontar a `baseUrl` dele pra
`localhost:8080` (ou onde a API estiver rodando) e repetir as chamadas do
`api-dashboard.md` a partir do próprio código gerado, não só via `curl`.

## 5. Checklist de integração (resumo executivo)

| Item | Card Jira | Status real no código | Status no Jira | Observação |
|---|---|---|---|---|
| Contrato definido | SCRUM-19 | Proposta existe, documentada | Concluído | — |
| Validação de payload | SCRUM-22 | Implementada (`LeituraRequest`) | Concluído | — |
| Persistência | SCRUM-23 | Implementada (JPA/H2) | Concluído | — |
| Tratamento de erro padronizado | SCRUM-44 | **Só em `main`, faltava em `develop`** | A fazer | Trazido nesta entrega |
| Consultar estado atual | SCRUM-38 | **Já implementado e testado** | A fazer | Deixado fora desta entrega — é do Arthur, que já está de olho nisso |
| Consultar histórico | SCRUM-40 | Implementado e testado | Concluído | — |
| Simular múltiplos jardins | SCRUM-42 | **Já implementado** (J01–J04 no `GeradorDadosJardim`) | A fazer | Jira parece desatualizado — confirmar com Julia |
| Wokwi/simulador intercambiáveis | SCRUM-43 | Resolvido nesta entrega (ponte `@JsonAlias`) | A fazer | — |
| API pronta pro dashboard | SCRUM-20 | CORS + docs entregues nesta entrega | A fazer | Falta testar com código gerado de verdade (bloqueado por DS-02) |
| Dashboard gerado | SCRUM-24 | Em andamento (Adrielly) | Em andamento | Bloqueia o item 4 acima |
| Integração hardware (NTP/offline/backoff/LoRa) | SCRUM-34/36 | Implementado numa branch separada, não mergeada | Concluído/Validação | Não mexi nisso — fora do escopo "só software" de hoje |

**Para a reunião com a equipe, os pontos que precisam de decisão humana (não
são meus pra decidir):**
1. SCRUM-38 e SCRUM-42 parecem prontos no código mas abertos no Jira —
   alguém confirma e atualiza o board?
2. Nome final do campo de nível/id (ver contrato) — ou seguir com a ponte de
   aliases indefinidamente, que também é uma opção válida.
3. `main` tem um commit (PR#11, Arthur) que nunca chegou em `develop` — o
   fluxo do time é mergear em `develop` e só promover pra `main` "em última
   instância"; isso inverteu. Só percebi o `TratadorDeErros.java` porque
   fui atrás; pode haver mais coisa de main p/ conferir.

---

## 6. Rodada de 23/11/2026 — `develop` atual + dashboard do PR #26

> Registro novo. Não substitui as seções 0 a 5 (histórico de 14/11).
> Detalhes, comandos, achados e passos para o Wokwi:
> [`relatorio-integracao-sw12.md`](relatorio-integracao-sw12.md).

### 6.1 Versões testadas

| Componente | Referência | SHA |
|---|---|---|
| `develop` (com #19, #22, #23, #24, #28) | `origin/develop` | `d13d021` |
| Dashboard conectado à API | PR #26 (HEAD) | `8a0bd3f` |
| Relação | `develop` é ancestral do #26; o #26 tem 8 commits próprios, só em `frontend/` | — |
| Backend e firmware | `api-jardim-sentinela/` e `esp32/` no #26 são idênticos à `develop` | — |

Ambiente: container Linux, Java 21, Node 22, Python 3.13, g++, Chromium
(Playwright). Bancos H2 descartáveis (memória; arquivo temporário só no teste de
persistência). Nessa rodada de 23/11 Wokwi, PlatformIO e hardware **não foram executados** (sondagens
sem resposta). O Wokwi foi executado em **24/11/2026** na máquina do responsável: ver 6.4.

### 6.2 Execuções (HEAD `8a0bd3f`)

| Teste | Resultado | Observação |
|---|---|---|
| API: `./mvnw -B -ntp test` | **180/180** | CI (execução nº 14 do #26) também verde |
| Frontend: `npm ci`, `npm test`, `npm run lint`, `npm run build` | **27/27**, lint e build OK | testes com API mockada; CI verde |
| Firmware (host): `python3 test/host/run_tests.py --verbose` | **14/14 grupos** | não é Wokwi nem ESP32 |
| Firmware (receptor): `python3 test/receiver/test_receber.py` | **OK** | não é Wokwi |
| Smoke **original** (timestamp fixo de 04/11) | 9 OK, **2 FALHAS** (passos 5 e 6) | expectativa do script contraria o contrato §3.4 |
| Smoke **novo** (`scripts/smoke-test-integracao.sh`) | **69/69** (com `jq` e com `python3`), 2 achados (F1, F2) | `--estrito` falha pelos achados |
| Validação de URL do smoke (`--autoteste-url`) | **45/45** | URLs com `@`, caminho, `file://` etc. recusadas; **0 chamadas** de `curl` antes da recusa |
| Controle negativo do smoke (servidor falso) | 52 falhas, exit 1 | o script não passa em branco |
| Simulador J01–J04 → API → dashboard, no navegador | OK | ver 6.3 |
| Persistência após reiniciar a API (H2 em arquivo) | OK | leitura mantida; retransmissão `bootId+seq` devolve 200 |
| JSON real do gateway do firmware → API (42,5 cm) | OK | 201; reenvio 200; 42,5 preservado |

### 6.3 Integração real (separada por tipo)

- **Componentes isolados:** API, frontend e firmware (host), tabela acima.
- **Frontend ↔ backend reais (navegador):** J01–J04 com nível, estado,
  tendência e última atualização vindos da API; o nível exibido bate com as
  últimas leituras da API em todos os jardins; `estado` enviado pelo dispositivo
  tem prioridade sobre o cálculo local; CORS presente em todas as respostas;
  API fora → aviso e último dado mantido; API de volta → recupera sem recarregar.
- **Simulador:** J01–J04 aceitos (HTTP 201) e persistidos.
- **Firmware → API:** apenas o JSON gerado pelo código real do gateway
  (harness em C++), postado na API. **Não** é Wokwi.
- **Wokwi → API Spring Boot real (firmware atual):** **não executado.**

### 6.4 Wokwi em 24/11/2026 (substitui os bloqueios de 23/11)

Bloqueios de 23/11 (`wokwi-cli`, `pio`, token, rede do ambiente Linux) deixaram de
valer: a execução foi na máquina do responsável, com PlatformIO e extensão Wokwi para
VS Code (Private Gateway embutido). Detalhes e origem de cada evidência em
[`evidencias-sw12-wokwi-2026-11-24.md`](evidencias-sw12-wokwi-2026-11-24.md) e
`relatorio-integracao-sw12.md` §6.4.

| Cenário | Resultado |
|---|---|
| 3.14 nível 42,5 cm sem arredondar para 43 | **PASSOU** (log `42.52 cm`; API `nivelCm: 42.5`) |
| 3.16 totem sem receptor | **VALIDADO COM RESSALVAS** |

Ressalvas do 3.16: buzzer só com relato auditivo e log `som=` (cadência não
verificada); piscar do vermelho não capturado dentro da janela exata do bloqueio
(inferido pelo código: tarefa FreeRTOS própria); só simulador, nunca hardware real.

### 6.5 Critérios de aceite

| Critério | Situação em 23/11/2026 |
|---|---|
| Simulador alimenta a API | **Validado** |
| Dados persistem | **Validado** |
| Endpoints respondem | **Validado** (smoke novo: 69/69) |
| Interface consome dados de teste | **Validado** (#26 + API real, no navegador) |
| Wokwi alimenta a API | **Validado em 24/11** (3.14 passou; 3.16 com ressalvas, ver 6.4) |

### 6.6 Achados abertos (resumo)

F1 reinício com hora estimada, F2 `boot` como leitura atual, F3 limiares de
estado divergentes (frontend × firmware), F4 aviso "API indisponível" em 404 e
dado antigo mantido, F5 polling duplicado, F6 rótulos do gráfico. Evidência
reproduzível, impacto, prioridade e responsável em
`relatorio-integracao-sw12.md` §4.

# Decisões técnicas — Jardim Sentinela

> **Leia isto primeiro.** O arquivo original `docs/decisoes-tecnicas.md` **nunca foi versionado**:
> não há commit dele em nenhuma branch, mas ~31 comentários, testes e docs apontam para as suas
> seções (§3, §3.1, §4, §4.1, §4.2, §5.1, §5.2, §6.2, §6.3, §6.6, §8). Este texto é uma
> **reconstrução de 26/11/2026**, feita a partir do que o código, os testes e os documentos do
> repositório afirmam. **Não é o original.**
>
> - A numeração segue as referências que já existem no código, para elas voltarem a apontar para
>   algo.
> - **Só entra aqui o que o repositório sustenta.** Se algo estava no original e não deixou rastro
>   no código ou nos docs, não está aqui. Se o original ainda existir (Notion, Drive de alguém,
>   conversa antiga), ele deve ser comparado com este texto, e este arquivo corrigido.
> - Cada decisão tem **Fonte** (onde o repositório a registra) e **Status**.
> - **Não é alerta oficial.** O Jardim Sentinela não substitui Defesa Civil, APAC ou CEMADEN
>   (README). Estado e tendência descrevem o jardim, não a emergência da comunidade.

**Legenda de status:** **Em vigor** (implementado e coberto por teste) · **Provisório** (implementado, mas o
próprio código diz que será revisado) · **A confirmar** (o grupo ainda precisa decidir) ·
**Em aberto** (sem decisão e sem implementação).

**Revisão:** pendente do grupo. Autoria da reconstrução: assistente de IA, a pedido da Júlia
Tenório Calado.

---

## Resumo das pendências (tudo o que ainda precisa de decisão do grupo)

| Item | Onde | Quem decide (segundo os PRs/relatórios) |
|---|---|---|
| Limiares do estado físico (22/45/60 cm) e histerese de 2 cm | §3, §3.1 | grupo |
| Padrão sonoro e piscar do ALAGADO | §4.1, §4.2 | grupo |
| Regra do som em `TRANSBORDANDO` + tendência `INSUFICIENTE` | §4.3 | grupo (relatório da Tarefa C, §5.2.1) |
| Janela, tolerância e idade máxima da tendência | §5.2 | revisar com dados de campo |
| Relógio da tendência no firmware: `tMs` do boot × `timestamp` absoluto | §5.2 | grupo |
| Estado vindo da sonda HW-02 (T1592 só valida protocolo) | §8 | grupo confirmar |
| Drenagem lenta: expor na API? Eventos T1592 entram? (**issue #29**) | Anexo A | grupo |

---

## §3 Estado físico do jardim

**O que é:** o nível de água classificado em quatro estados: `NORMAL`, `ENCHENDO`, `TRANSBORDANDO`, `ALAGADO`.
É o **nível de água do jardim físico**, não o estado de emergência.

| Estado | Começa em (nível, cm) |
|---|---|
| `NORMAL` | abaixo de 22 |
| `ENCHENDO` | 22 |
| `TRANSBORDANDO` | 45 |
| `ALAGADO` | 60 |

- O estado sobe ao **atingir** o limite (inclusive).
- Nível inválido (NaN, infinito ou negativo) **mantém o estado anterior**.
- **Fonte:** `esp32/src/modulos/estado_fisico.h` e `.cpp`; `esp32/test/host/test_estado_fisico.cpp`;
  `esp32/test/vetores/estado.csv`.
- **Status:** **Provisório.** O próprio código diz "PROVISORIO, ver §3".

### §3.1 Histerese

- O estado só **desce** quando o nível cai abaixo de `limite − 2 cm`. Isso evita que o estado
  "pisque" quando o nível oscila perto de um limite.
- O relatório da Tarefa C (`docs/relatorio-tarefa-c.md` §5.2.1) registra que "o projeto trata o
  **alarme falso** como o principal risco" e atribui esse ponto à §3.1. Isso explica a histerese,
  mas é uma **citação** do relatório; o texto original não foi encontrado.
- **Fonte:** `estado_fisico.h` (`FOLGA_HISTERESE_CM = 2.0`).
- **Status:** **A confirmar** (o código diz "a confirmar").

### §3.2 Alinhamento com o frontend (F3) — resolvido

O frontend, quando a API devolve `estado: null`, calcula o estado com `calcularStatusAlerta()` em
`frontend/src/types/jardim.ts`. Antes, usava faixas diferentes do firmware (15/30/45 cm); a partir de
26/11/2026 usa as **mesmas faixas** (22/45/60 cm), com o `NORMAL` do firmware mapeado para `SECO` no
dashboard. Quando o firmware envia `estado`, ele continua tendo prioridade sobre o cálculo local.

- **Fonte:** `frontend/src/types/jardim.ts` (`LIMITES_ALERTA_CM`), `frontend/src/types/jardim.test.ts`;
  `esp32/src/modulos/estado_fisico.h`; ensaio de 26/11/2026.
- **Status:** **Em vigor.** A divergência F3 foi corrigida na
  `docs/SCRUM-39-correcoes-roteiro-e-relatorio`.

---

## §4 Sinalização do totem

O totem usa três LEDs e um buzzer, e **funciona sem internet**: depende só do estado físico e da
tendência locais. O som indica que as pessoas devem **se afastar da área do jardim**. Não é sirene
nem alerta oficial; o padrão nunca é contínuo, para ser distinto de qualquer sirene.

| Estado | LED |
|---|---|
| `NORMAL` | verde fixo |
| `ENCHENDO` | amarelo fixo |
| `TRANSBORDANDO` | vermelho fixo |
| `ALAGADO` | vermelho piscando (§4.1) |
| sem leitura da sonda (`0xFF`) | tudo apagado e calado |

- **Pinos:** verde D13, vermelho D14, buzzer D23, amarelo GPIO16 (rótulo `RX2` no Wokwi), em vez de
  D12, que é pino de strapping (`docs/relatorio-tarefa-c.md` §5.3).
- **Fonte:** `esp32/src/modulos/sinalizacao.h` e `.cpp`, `totem.cpp`; `test_sinalizacao.cpp`.
- **Status:** **Em vigor** para o mapa estado → LED. **Provisório** para §4.1 e §4.2.

### §4.1 Piscar do ALAGADO
- O vermelho fica aceso na primeira metade de cada período de **1000 ms**.
- **Status:** **Provisório** ("a definir na implementação").

### §4.2 Padrão sonoro
- Rajada de **3 pulsos de 150 ms**, com 150 ms de pausa entre eles, repetida a cada **2000 ms**
  (pulsos em `[0,150)`, `[300,450)`, `[600,750)` de cada ciclo). Frequência do buzzer: 2 kHz.
- Sem estado interno: o padrão depende só do relógio do dispositivo. Se o som começar no meio do
  ciclo, o primeiro pulso pode sair parcial.
- **Status:** **Provisório** ("a confirmar").

### §4.3 Quando o buzzer pode soar

| Estado | Tendência | Buzzer |
|---|---|---|
| `ALAGADO` | qualquer | permitido |
| `TRANSBORDANDO` | `SUBINDO` | permitido (regra normal) |
| `TRANSBORDANDO` | `INSUFICIENTE` ou inválida (`0xFF`) | permitido (**regra provisória**, 19/11/2026) |
| `TRANSBORDANDO` | `ESTAVEL`, `DESCENDO`, `INDETERMINADO`, `DESATUALIZADO` | calado |
| `NORMAL`, `ENCHENDO` | qualquer | calado |

- **Motivo da regra provisória:** depois de um reinício a tendência fica `INSUFICIENTE` por cerca de
  4 min no hardware (5 snapshots de 60 s); antes, a regra deixava `TRANSBORDANDO` calado nesse período
  mesmo com a água subindo.
- **Custo conhecido:** a cada reinício com o jardim em `TRANSBORDANDO`, o buzzer soa até a janela
  encher, mesmo com o nível estável ou caindo. O grupo precisa pesar esse custo (alarme falso)
  contra o silêncio logo após o reinício.
- **Fonte:** `sinalizacao.cpp` (`somPermitido`, único lugar da regra); `relatorio-tarefa-c.md` §5.2.1.
- **Status:** **A confirmar.**

---

## §5 Regras versionadas e tendência

### §5.1 Versão da regra
- Cada registro com estado ou tendência carrega a **versão da regra** que o classificou:
  `v1-provisoria-2026-11` (firmware). A constante vem do firmware e não viaja no quadro binário; só sai
  no JSON quando há estado ou tendência.
- A API guarda `estado`, `tendenciaDispositivo` e `versaoRegra` **como vieram**, para auditoria, e
  **nunca os recalcula** (§6.3).
- **Fonte:** `estado_fisico.h` (`VERSAO_REGRA`); `LeituraRequest.java`; `test_estado_fisico.cpp`.
- **Status:** **Em vigor.** O sufixo "provisoria" acompanha o status das regras (§3, §5.2).

### §5.2 Tendência do nível

**O que é:** a direção recente do nível. **Não é previsão e não é alerta.** Seis valores: `SUBINDO`,
`ESTAVEL`, `DESCENDO`, `INSUFICIENTE`, `INDETERMINADO`, `DESATUALIZADO`.

| Parâmetro | Valor | Observação |
|---|---|---|
| Janela | **5** leituras válidas (4 intervalos) | constante do código; leitura válida = nível finito e `>= 0` |
| Variação | `última − primeira` da janela | |
| Tolerância | **1,5 cm** | `\|variação\| <= 1,5` é `ESTAVEL` (limite inclusivo) |
| Idade máxima | **3 × o intervalo de snapshot** | 180 s no hardware (snapshot de 60 s); 15 s na simulação (snapshot de 5 s). Idade igual ao máximo **não** é velha |
| Precedência | `INSUFICIENTE` → `DESATUALIZADO` → `INDETERMINADO` → direção | |

- A tendência usa **só leituras de snapshot** (intervalo regular), não cada leitura da sonda (que lê
  a cada 500 ms) nem os eventos irregulares.
- **Atenção:** a tolerância é em cm, e a janela dura tempos diferentes. 1,5 cm em 240 s no hardware
  é 0,375 cm/min; na simulação, 1,5 cm em 20 s é 4,5 cm/min. A simulação é cerca de 12 vezes mais
  sensível. Não comparar os dois sem considerar isso.
- **Firmware** (`tendencia_local.h`): só entram leituras do boot atual, ordenadas por `seq`; a idade
  usa o relógio do próprio dispositivo (`tMs`) dentro do boot. Não aloca memória.
- **API** (`GET /api/jardins/{id}/tendencia`, `TendenciaProperties`): a janela é montada pela
  cronologia do §6.2; a idade conta desde a última **recepção** (`recebidoEm`), não desde a medição;
  o `boot` fica fora; configurável por `jardimsentinela.tendencia.*`. As decisões D1 a D15 da API,
  com a justificativa de N = 5 e T = 1,5 cm, estão em `docs/tendencia-nivel.md`.
- **Divergência de relógio (ponto em aberto):** o firmware mede a idade com `tMs`, a API com
  `recebidoEm`. O grupo ainda não decidiu se o firmware deve usar o `timestamp` absoluto quando o
  relógio for válido (relatório da Tarefa C, §5.2, item 2).
- **Fonte:** `tendencia_local.h`, `TendenciaProperties.java`, `application.properties`,
  `application-simulacao.properties`, `docs/tendencia-nivel.md`, `docs/api-dashboard.md`.
- **Status:** **Provisório.** "Não estão calibrados com dados de campo" (`api-dashboard.md`).

---

## §6 API

### §6.2 Ordenação cronológica (opção B)
- Histórico, estado atual e tendência ordenam por `COALESCE(timestampDispositivo, recebidoEm)`,
  do mais recente para o mais antigo, com desempate por `id` decrescente.
- **[F1] Exceção:** quando `horaSincronizada = false`, a chave passa a ser `recebidoEm`, porque o
  `timestampDispositivo` é estimado e não serve para ordenar.
- Em `GET /api/jardins` a lista usa a mesma regra, em 2 consultas fixas (sem N+1).
- **[F2] Registros `tipo = boot`** não são a leitura atual do detalhe nem da lista. Se o jardim só
  tiver `boot`, o detalhe ainda o mostra como fallback; a lista não exibe o jardim.
- **Fonte:** `LeituraRepository.java` (`CHAVE_CRONOLOGICA`); `DivergenciasF1F2Tests`;
  `JardimOrdenacaoTests`.
- **Status:** **Em vigor (F1 e F2 corrigidos em 26/11/2026).** Os dois problemas foram reproduzidos
  pelo smoke test do SW-12 (`docs/relatorio-integracao-sw12.md`) e corrigidos na
  `docs/SCRUM-39-correcoes-roteiro-e-relatorio`.

### §6.3 Campos opcionais gravados como vieram
- `estado`, `tendenciaDispositivo`, `versaoRegra`, `tipo` (`evento|snapshot|boot`), `bootId` (**8
  caracteres hexadecimais**) e `seq` são opcionais no `POST /api/leituras`. A API valida o formato,
  **guarda como veio e nunca recalcula**.
- **Fonte:** `LeituraRequest.java`; `LeituraCamposEstadoTests`; `docs/dicionario-de-dados.md`.
- **Status:** **Em vigor.**

### §6.6 CORS
- **Lista fechada** de origens: `http://localhost:5173` (servidor de dev do Vite) e
  `http://localhost:4173` (preview do build). **Nunca `*`:** com ele, qualquer página aberta no
  navegador de quem roda a API poderia enviar dados a ela, inclusive por POST.
- Métodos: `GET` e `POST`. Headers: só `Content-Type` e `Accept`. A origem de produção entra pela
  propriedade `jardimsentinela.cors.allowed-origins` quando houver deploy.
- Consequência prática: abrir o dashboard em `http://localhost:5173`, não em `127.0.0.1`.
- **Fonte:** `WebConfig.java`, `application.properties`, `WebConfigCorsTests`.
- **Status:** **Em vigor.**

---

## §8 Fonte do estado: a sonda HW-02

- O **estado** do totem vem da **sonda HW-02** (nível contínuo, 0 a 100 cm). Os sensores resistivos
  T1592 só validam o protocolo.
- A tendência usa as leituras de **snapshot**. Eventos T1592 e o registro de `boot` levam `0xFF`
  ("sem classificação").
- **Fonte:** `esp32/test/host/test_classificador_sonda.cpp`; `relatorio-tarefa-c.md` §5.2 (item 1).
- **Status:** **A confirmar.** O relatório registra "decidido pela Júlia; grupo precisa confirmar".
- **Observação:** o conteúdo exato da §8 original não foi encontrado; o texto acima vem do
  comentário do teste e do relatório. Pode estar incompleto.

---

## Anexo A — Decisões posteriores às seções originais

> Estas decisões **não** têm uma seção citada no código. Entram aqui para ficarem num lugar só.

### A.1 Drenagem lenta (SW-15, SCRUM-30)
- **O que é:** regra **experimental** para perceber possível necessidade de manutenção depois da
  chuva. Janela de **60 minutos** que termina na leitura mais recente; taxa = queda entre a primeira e
  a última leitura ÷ minutos entre elas; abaixo de **0,5 cm/min** é `DRENAGEM_LENTA`; exatamente no
  limite já é drenagem normal (comparação estrita). Resultados: `DRENAGEM_LENTA`, `DRENAGEM_NORMAL`,
  `SEM_QUEDA`, `INSUFICIENTE`. Versão: `v1-provisoria-drenagem-2026-11`.
- **Escopo entregue:** regra pura + serviço + testes + documento. **Nenhum endpoint e nenhuma tela usam
  isso ainda**, e isso não era critério de aceite da task.
- **Em aberto (issue #29):** se a API considera **só snapshots** ou também **eventos T1592**. Hoje a
  regra aceita os dois, por decisão provisória, e o ponto de ajuste é `DrenagemService`.
- **Fonte:** `DetectorDrenagemLenta.java`, `DrenagemService.java`, `docs/drenagem-lenta.md`.
- **Status:** **Provisório.** O `drenagem.cpp` do firmware **não** faz isto: ele drena a fila de
  registros offline.

### A.2 `bootId` com 8 caracteres hexadecimais
- A API só aceita `bootId` com exatamente 8 hexadecimais (`[0-9A-Fa-f]{8}`), o mesmo formato que o
  firmware gera. O runner dos cenários controlados (SW-16) deve gerar e aceitar só esse formato.
- **Fonte:** `LeituraRequest.java`; `ExecutaCenario.java`; `docs/cenarios-controlados.md`.
- **Status:** **Em vigor.**

### A.3 Histórico sem `boot` para o dashboard (SW-18)
- Implementado pelo PR #33 (mergeado na `develop` em 27/11/2026): `incluirBoot` no
  `GET /api/jardins/{id}/leituras` (padrão `true`, para não mudar o contrato) e o dashboard passa
  `false`, para o gráfico e o CSV não mostrarem o 0 cm fictício do boot.
- O detalhe e a lista **não** dependem desse parâmetro; a exclusão do boot como leitura atual é o F2
  (§6.2).
- **Status:** **Em vigor.**

---

## Histórico deste documento

| Data | O que mudou |
|---|---|
| 26/11/2026 | Reconstrução a partir do repositório (código, testes e docs) porque o original nunca foi versionado. Pendente de revisão do grupo e de comparação com o original, se ele existir. |
| 27/11/2026 | F1, F2 e F3 marcados como resolvidos; `horaSincronizada` passa a ser persistido e usado na ordenação; PR #33 (`incluirBoot`) mergeado na `develop`. |

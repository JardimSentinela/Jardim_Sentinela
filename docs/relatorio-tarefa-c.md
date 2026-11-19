# Relatório — Tarefa C: estado físico, tendência local e sinalização do totem

> **Cards:** [SW-13] Classificar estado físico do jardim (SCRUM-26) e parte de [SW-14] (tendência local).
> **Branch:** `feat/SCRUM-26-estado-totem` (a partir de `origin/develop`). **PR Aberto.**
> **Autoria dos commits:** Júlia Tenório Calado `<jtc@cesar.school>`.
> **Roteiro da simulação:** [`docs/validacao-sinalizacao.md`](validacao-sinalizacao.md).
>
> Este relatório existe para que o conteúdo não dependa da sessão que o produziu.

---

## 1. Resumo

O firmware agora classifica o estado do jardim (`NORMAL`, `ENCHENDO`, `TRANSBORDANDO`,
`ALAGADO`), calcula a tendência local (`SUBINDO`, `ESTAVEL`, `DESCENDO`, …), grava as duas
no registro, envia à API e aciona LEDs e buzzer. Tudo funciona sem internet. A lógica é
código C++ puro, testado no host (14 grupos, todos passando).

**O que não foi verificado:** o firmware **não foi compilado para o ESP32** e o **Wokwi não
foi executado**. Ver §7.

## 2. Commits (17)

| Hash | Tipo | Assunto |
|---|---|---|
| `003f0d0` | test | teste vermelho do estado físico com histerese |
| `90c1fe9` | test | piso de casos do CSV de estado passa para 30 |
| `99d90ec` | feat | classifica o estado físico com histerese de 2 cm |
| `a5a0c32` | test | teste vermelho da tendência local |
| `dca235c` | test | tendência local: boot atual informado e casos de reinício |
| `0f60630` | feat | calcula a tendência local do nível |
| `62c5b0a` | test | teste vermelho do codec v2 com estado e tendência |
| `e80031f` | test | linha antiga, rejeição por versão e tamanho da linha |
| `f82d741` | feat | quadro v2, contador de rejeição por versão |
| `9336acb` | test | teste vermelho da sinalização do totem |
| `d37dc6d` | feat | decide LEDs e buzzer em módulo puro |
| `08cff99` | test | teste vermelho do `seq` de 32 bits |
| `e0d627d` | feat | alarga o `seq` do quadro para 32 bits |
| `ea3efdd` | test | estado e tendência na linha gravada, J01, classificador da sonda |
| `0fbca2a` | feat | grava e envia estado e tendência, id J01, estado da sonda |
| `858337c` | test | teste vermelho da conversão de bytes do totem |
| `18a8010` | feat | liga LEDs e buzzer do totem no Wokwi |

O commit com este relatório e o roteiro é o 18º (`docs`).

## 3. O que foi implementado

| Módulo (`esp32/src/modulos/`) | Função |
|---|---|
| `estado_fisico` | Faixas 22 / 45 / 60 cm e histerese de 2 cm. Entrada inválida (NaN, infinito, negativa) mantém o estado |
| `tendencia_local` | Janela de 5 leituras válidas do **boot atual**, ordenadas por `seq`; tolerância 1,5 cm; `DESATUALIZADO` pelo `tMs`; histórico de tamanho fixo, sem alocação |
| `classificador_sonda` | Junta estado (da sonda, com histerese) e tendência (dos snapshots) |
| `sinalizacao` / `totem` | Decisão de LEDs e buzzer (pura, testada) e ligação dos pinos |
| `evento.h` | `Evento` e `TipoRegistro` num header puro, para os testes usarem a mesma struct |
| `codec_quadro.h` | Quadro **v2**: +2 bytes por registro (estado, tendência), `seqBase` de 32 bits |
| `gateway.cpp`, `linha.cpp` | Leitura e escrita dos campos novos; linha antiga continua válida |

**Vetores compartilhados** (`esp32/test/vetores/`): `estado.csv` (34 casos) e `tendencia.csv`
(33 casos). A API (Tarefa B) deve ler os mesmos arquivos.

## 4. Contratos alterados

- **Payload `POST /api/leituras`:** campos opcionais novos `estado`, `tendenciaDispositivo`,
  `versaoRegra` (decisões §6.3). Saem **depois** de `bootId`/`deltaTMs`/`lat`/`lon` e **antes**
  de `horaSincronizada`. Só saem se o registro tem classificação.
- **`idJardim`:** `J01` (constante única `JARDIM_ID`). Linhas antigas com `jardim-centro-01`
  saem com `J01` (migração silenciosa).
- **Quadro v2** (versão `0x02`): registro de 9 bytes, cabeçalho de 16. Constantes: `MAX_BYTES` 70, `MAX_COM_EXTRAS` 148
  (limite folgado; o pior caso real é 147). O quadro **não é persistido**: é montado do NDJSON no envio, então não há
  quadro v1 pendente.
- `TAMANHO_LINHA`: 320 → **384** (a linha mais longa chega a 351 bytes com os campos novos).
- `docs/contrato-telemetria.md` e `docs/dicionario-de-dados.md` **não foram atualizados** nesta tarefa
  (as decisões pedem corrigir os três no mesmo commit). Fica para a Tarefa F ou para a Júlia.

## 5. Decisões

### 5.1 Decisão nova, confirmada pela Júlia

**A tendência usa só as leituras de snapshot. O estado continua vindo da sonda. Os T1592 e o registro de boot levam `0xFF` (sem classificação).**

Justificativa do espaçamento:

- Os snapshots chegam em **intervalo regular** (5 s na simulação, 60 s no hardware). Cinco snapshots
  cobrem 4 intervalos: **20 s** na simulação e **4 min** no hardware, alinhado com a idade máxima
  de 3 × snapshot (§5.2).
- A sonda lê a cada **500 ms**: cinco leituras cobririam 2,5 s, quase só ruído de ADC.
- Os eventos da sonda (variação ≥ 1 cm) são **irregulares**: numa chuva forte cinco eventos acontecem em
  segundos (tendência `SUBINDO` quase instantânea); com o nível parado, não chegam a se formar e a tendência
  ficaria `DESATUALIZADO` sem motivo.
- Com espaçamento regular, "variação > 1,5 cm" tem um significado estável como taxa mínima.
  **Atenção:** a tolerância é em cm, e a janela tem tempos diferentes. No hardware, 1,5 cm em 240 s é
  **0,375 cm/min**; na simulação, 1,5 cm em 20 s é **4,5 cm/min**. A simulação é cerca de 12 vezes mais
  sensível. Não comparar os dois sem considerar isso.

### 5.2 Mudanças em relação ao `decisoes-tecnicas.md` (a confirmar pelo grupo)

| # | Mudança | Estado |
|---|---|---|
| 1 | **§8:** o estado vem da **sonda HW-02** (0–100 cm, contínua). Os T1592 só validam o protocolo | Decidido pela Júlia; **grupo precisa confirmar** |
| 2 | **§5.2 (relógio):** dentro do boot, o `tMs` é o relógio usado para a idade. O `timestamp` absoluto **não** é usado. Pergunta aberta: usar o absoluto quando o relógio for válido? | Júlia aceitou o `tMs`; **grupo precisa confirmar** |

### 5.2.1 Regra provisória do som (decidida em 2026-11-19, **a confirmar pelo grupo**)

| Estado | Regra do buzzer |
|---|---|
| `ALAGADO` | soa **sempre** (não mudou) |
| `TRANSBORDANDO` | soa com tendência `SUBINDO` (regra normal, **não mudou**) **ou** enquanto a tendência for `INSUFICIENTE` (**regra provisória nova**) |
| `NORMAL`, `ENCHENDO` | calado |

- **Motivo:** depois de um reinício a tendência fica `INSUFICIENTE` por cerca de 4 min no hardware (5 snapshots), e antes a regra
  deixava o `TRANSBORDANDO` calado nesse período, mesmo com a água subindo.
- **Continuam calados no `TRANSBORDANDO`:** `ESTAVEL`, `DESCENDO`, `INDETERMINADO` e `DESATUALIZADO`.
  Tendência inválida (`0xFF`) conta como `INSUFICIENTE`, então também soa.
- **Custo conhecido:** a cada reinício com o jardim em `TRANSBORDANDO`, o buzzer soa até a janela encher, mesmo com o nível
  estável ou caindo. O projeto trata o alarme falso como o principal risco (§3.1 das decisões), então o grupo precisa pesar
  esse custo contra o silêncio logo após o reinício.
- **Pendência:** confirmação do grupo. Até lá, a regra é **provisória** e fica em um único lugar (`somPermitido`, `sinalizacao.cpp`).
- **Código e testes:** `sinalizacao.cpp`/`.h` e `test_sinalizacao.cpp` (commits `test:` e `feat:` de 2026-11-19, regra provisória do som).

### 5.3 Decisões de implementação (não estavam nas decisões técnicas)

1. **Boot atual é parâmetro** (`bootIdAtual`), não o boot do último item: um retardatário de boot antigo não vira o atual. `seq` e `tMs` não são comparáveis entre boots.
2. **Idade conta a última leitura válida** do boot atual, não uma inválida mais recente.
3. **Entrada inválida** (NaN, infinito, negativa): `estado_fisico` mantém o estado; `tendencia_local` ignora a leitura.
4. **Nome desconhecido** em `estado` ou `tendenciaDispositivo` invalida a linha (não vira `0xFF` em silêncio).
5. **`versaoRegra` só sai quando há estado ou tendência**, e é sempre a constante do firmware (não viaja no quadro).
6. **Estado ou tendência fora da lista**, ao gravar a linha, **não são gravados**.
7. **Sem leitura da sonda**, o totem apaga tudo e não soa. Tendência inválida conta como `INSUFICIENTE`.
8. **Quadro v2 rejeita qualquer versão ≠ `0x02`**, e a rejeição por versão é contada (`rejeitadosVersao`, comando `i`) e logada.
9. **Padrão sonoro** (3 pulsos de 150 ms a cada 2 s), **piscar do ALAGADO** (1 s) e **frequência do buzzer** (2 kHz) são **provisórios**.
10. **Pino do amarelo:** GPIO16 (rótulo `RX2` no Wokwi), em vez de D12 (pino de strapping). LEDs: verde D13, vermelho D14; buzzer D23.

## 6. Riscos e limitações

| Risco | Detalhe |
|---|---|
| **Reinício em `TRANSBORDANDO`** | Após um boot a tendência é `INSUFICIENTE` por ~4 min (hardware). Pela regra provisória de 2026-11-19 (§5.2.1), o `TRANSBORDANDO` **soa** nesse período. O custo é alarme possível com o nível estável ou caindo. **O grupo deve confirmar** |
| **`TRANSBORDANDO` com leitura desatualizada** | Tendência `DESATUALIZADO` **não soa** (a regra provisória cobre só `INSUFICIENTE`) |
| **`versaoRegra` constante** | Se a regra mudar (v2) com linhas da v1 pendentes na fila, elas saem carimbadas com a v2 |
| **Migração silenciosa do id** | Linha pendente com `jardim-centro-01` sai como `J01` |
| **Som parcial** | A saída não tem estado interno: se o som começar no meio de um ciclo, o primeiro pulso pode sair parcial |
| **Histerese e padrão sonoro** | Ainda "a confirmar" pelo grupo (§3.1 e §4.2) |

## 7. Verificação e pendências

**Executado:** `python3 test/host/run_tests.py` → **14/14 grupos OK**, 0 warnings nos testes novos.
Verificações por grupo: codec 65, compactação 55, latência 20, drenagem 28, url 6, backoff 31, sonda 23,
estado físico 52, tendência local 69, gateway 40, sinalização 76, classificador da sonda 19, linha 24.
Fuzz da inserção na janela de tendência: 200.000 casos, 0 divergências (executado fora do repositório, não versionado).

**NÃO executado (pendências):**

1. **Compilação para o ESP32.** Não há PlatformIO no ambiente. Só foi feita uma checagem de **sintaxe** com
   `-std=gnu++11` e stubs mínimos (fora do repositório), que passou em `amostragem`, `gateway`, `linha`, `estado`,
   `diagnostico`, `drenagem`, `totem` e `main_alt`. `armazenamento` e `cursor` só falham por limitação dos stubs,
   em código não alterado. **Isso não substitui `pio run`.** O padrão C++ do core Arduino-ESP32 não foi confirmado
   ([Suposição]: `gnu++11` no core 2.x), por isso `Evento` não tem valor padrão nos membros.
2. **Wokwi não executado.** O roteiro está em `docs/validacao-sinalizacao.md`, todo `PENDENTE`.
3. **Conferência do rótulo `RX2`** do LED amarelo no Wokwi ([Provável], não verificado).
4. **API (Tarefa B):** escolher uma regra explícita para o **boot atual** (por exemplo, o `bootId` da leitura válida mais
   recente por `recebidoEm`) e testar o caso do retardatário. Sem isso, `tendencia.csv` não vale para a API.
5. **Docs de contrato** (`contrato-telemetria.md`, `dicionario-de-dados.md`) sem os campos novos.
6. Confirmações do grupo: §5.2 deste relatório (itens 1 e 2), **a regra provisória do som com tendência `INSUFICIENTE` (§5.2.1)**, histerese e padrão sonoro.

## 8. Achados durante a tarefa

- **Bug no decodificador (existia na base):** a checagem de limite `cabe(p, 3, tam)` lia **4** bytes (nível, bateria e
  `deltaT` de 2 bytes). Foi **corrigido na mesma branch**, no commit `f82d741`; agora é `cabe(p, 6, tam)`
  (com os 2 bytes novos).
- **`seq` de 16 bits (existia na base):** o `seqBase` do quadro era `uint16_t`, então depois de 65.535 registros no boot o `seq`
  do JSON voltava a zero e o par `(bootId, seq)` se repetia na API. Corrigido nos commits `08cff99` (teste) e `e0d627d`:
  `seqBase` de 32 bits, **+2 bytes por quadro** e **0 bytes no JSON**. O `tendencia_local` nunca foi afetado: usa `seq` de 32 bits.
- **`MAX_COM_EXTRAS`:** é um limite folgado (o primeiro registro nunca tem gap). Real: 147 bytes; limite: 148.

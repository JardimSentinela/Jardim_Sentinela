# Plano — Persistência e formato do `/fila.ndjson` (store-and-forward)

> **Status:** Etapa 1 (plano) **aprovada** e **Etapa 2 implementada** — nenhum código
> alterado na Etapa 1; a implementação está em `esp32/src/main_alt.cpp` e
> `docs/contrato-telemetria.md`, na branch da história.
> **Decisões da aprovação:** boot criado dentro de `tarefaAmostragem`; `bootId` de
> 8 hex; `lat`/`lon` só na linha de boot (documentado para o piloto); descarte
> antigo em blocos de ~25% com compactação por temporário + `rename`;
> `MAX_BYTES_FILA` como constante única; nomes dos campos JSON centralizados em
> constantes; Opção A no item 4 (cálculo de `sensAtivo` mantido).
> **Autorização:** Pessoa 1 (simulação dos sensores resistivos) autorizou a correção da
> parte de persistência e formato dos dados.
> **Branch de trabalho:** `feat/SCRUM-31-Emular-sensores-resistivos-T1592` — a alteração
> faz parte da **mesma história** (SCRUM-31), portanto segue na própria branch da
> história, sem branch nova.
> **Arquivo alvo:** `esp32/src/main_alt.cpp` (é o compilado pelo PlatformIO;
> `esp32/src/sketch.ino.bak` é rascunho antigo e **não** entra no build).

---

## 1. Contexto e regras respeitadas

A integração do envio à API (HTTP no Wokwi, LoRa no hardware real) vai exigir que
`/fila.ndjson` funcione como **buffer store-and-forward confiável**. Hoje ele é
append-only e, ao atingir 64 KB, **descarta o registro novo e fica mudo**.

### Regras dados pelo solicitante

| Regra | Como o plano a cumpre |
|---|---|
| Não alterar lógica de amostragem, histerese, debounce, cálculo de patamares nem pinos | `lerSensor`, `nivelDe` e o miolo de `tarefaAmostragem` **permanecem intactos**. O único acréscimo na tarefa é a **criação** de registros (campos novos no `struct Evento`) |
| Não renomear campos existentes (`idJardim`, `idSensor`, `nivelAgua`, `bateria`, `timestamp`) | Nenhum desses nomes muda. Campos novos são **adicionados**. A troca `bool snapshot` → `TipoRegistro tipo` é **interna ao `struct Evento`**, não afeta o JSON |
| Manter a convenção `SIMULATION` / `#ifdef SIMULATION` | Preservada integralmente |
| Branch própria, sem mexer na `main` | A alteração integra a **mesma história (SCRUM-31)**, logo segue na branch `feat/SCRUM-31-Emular-sensores-resistivos-T1592`. A `main` **não** é alterada |
| Comentário curto em cada função criada/alterada | Previsto em todas as etapas de §A |
| Apresentar plano e esperar aprovação antes de editar | Este documento; a Etapa 2 só começa após aprovação |

### Observação sobre a branch de base

A `main` **não** contém o firmware vigente. Ela tem `esp32/main_alt.cpp` (versão
antiga, 179 linhas, formato `molhado`/`bootId`/`seq`/`tMs`/`jardimId`/`sensorId`),
enquanto o arquivo atual — `esp32/src/main_alt.cpp`, 224 linhas, formato
`snapshot`/`nivelAgua`/`deltaTMs`/`timestamp` — só existe na branch
`feat/SCRUM-31-Emular-sensores-resistivos-T1592`, que é a branch desta história.

**Consequência prática:** todo o trabalho de persistência e formato deve ser feito
sobre `esp32/src/main_alt.cpp` **na branch SCRUM-31**. Se as alterações fossem
aplicadas sobre a `main`, elas atingiriam o arquivo antigo (caminho e conteúdo
diferentes) e seriam perdidas no merge.

---

## 2. Fatos técnicos verificados (core Arduino-ESP32 4.20017)

Verificados no pacote instalado, porque mudam o desenho do item 9:

| Fato | Onde | Consequência |
|---|---|---|
| `FS::rename(from, to)` existe | `libraries/FS/src/FS.h:105` | compaction via "grava temporário + renomeia" é possível |
| **`truncate()` não existe** em `FS.h`, `File` nem LittleFS | `libraries/FS/src/`, `libraries/LittleFS/src/` | compaction *in-place* (cortar o início do arquivo) é **impossível** |
| `File::seek(pos, SeekEnd)` existe | `libraries/FS/src/FS.h:66` | permite ler o arquivo de trás para frente e achar fronteiras de linha |
| `configTime()` existe | `cores/esp32/esp32-hal-time.c:48` | o ponto de ligação do NTP já tem API pronta |
| Partição LittleFS (`spiffs` no `default.csv`) = `0x160000` | `tools/partitions/default.csv` | **1.375.488 bytes (~1,31 MB)** — folga para 2 arquivos de 64 KB |
| Jackson 3.1.5: `FAIL_ON_UNKNOWN_PROPERTIES` com `_defaultState = false` | `tools.jackson.databind.DeserializationFeature` (bytecode) | a API hoje **ignora** campos desconhecidos em silêncio |
| `Print::println()` escreve `"\r\n"` | `cores/esp32/Print.cpp:169-172` | origem do CRLF atual; `print()` + `"\n"` resolve |

---

## 3. O que muda, item a item

### Item 1 — campo `tipo` (evento / snapshot / boot)

| Onde | Mudança | Por quê |
|---|---|---|
| `struct Evento` (41-47) | `bool snapshot` → `TipoRegistro tipo`; novo `enum class TipoRegistro : uint8_t { Evento, Snapshot, Boot };` acima da struct | 3 tipos não cabem em `bool`. Nenhum campo JSON existente é tocado |
| `tarefaAmostragem` (125, 135) | `Evento e{false,…}` → `{TipoRegistro::Evento,…}`; `Evento s{true,…}` → `{TipoRegistro::Snapshot,…}` | apenas tipagem; histerese/debounce/patamares intactos |
| `persistir` (161-166) | `"tipo"` como **primeira** chave; linha montada em 3 variantes | permite `grep '"tipo":"evento"'` e contar por tipo |
| nova função | `static const char* nomeTipo(TipoRegistro)` → `"evento"` / `"snapshot"` / `"boot"` | um único lugar para os nomes |

**Implementação:** a linha passa a ser montada em partes — parte fixa (tipo + 5
campos obrigatórios + `seq` + `bootId` + `horaSincronizada`) e **partes opcionais**
(`deltaTMs`, `lat`/`lon`) inseridas **antes** do `}` final.

---

### Item 2 — campos `seq` e `bootId`

| Onde | Mudança | Por quê |
|---|---|---|
| novos globais (perto de 49-50) | `static char g_bootId[9];` (8 hex + NUL) e `std::atomic<uint32_t> g_seq{0};` | o `seq` nasce na tarefa e é gravado no `loop()`; `std::atomic` segue o estilo do `descartados` (linha 50) e `<atomic>` já está incluso |
| `setup()` (antes de 207) | `g_bootId` ← `esp_random()` (8 hex); `g_seq = 0` | `bootId` novo a cada boot; `seq` recomeça em 0 |
| `struct Evento` (41-47) | **novo** `uint32_t seq;` | o `seq` viaja junto do registro até o arquivo |
| `tarefaAmostragem` (125, 135) e criação do boot | `seq = g_seq++` **no momento da criação** (não no `persistir`) | requisito: registro descartado (fila cheia / arquivo cheio) deixa **buraco visível** na sequência |
| `persistir` | `"seq":N` e `"bootId":"XXXXXXXX"` em **toda** linha | o receptor passa a detectar perda e separar boots |

**Atribuição do `bootId`:** 8 hex (32 bits) — colisão desprezível. A alternativa de
4 hex economiza 4 bytes/linha, mas dá 1 chance em 65.536 de repetir um `bootId`
entre boots, o que confundiria o receptor. **Recomendação: 8.**

---

### Item 3 — `deltaTMs` apenas nos eventos de subida

| Onde | Mudança | Por quê |
|---|---|---|
| `tarefaAmostragem` (114-123) | **nenhuma** | o cálculo já zera `dt` nas quedas e no primeiro evento — a lógica de subida **já** está correta |
| `persistir` (161-166) | incluir `"deltaTMs":N` **somente** quando `tipo == Evento && e.deltaTMs > 0` | economiza ~16 bytes nas demais linhas |
| debug `[LIMIAR]` (170-171) | imprimir `Delta t` **só quando > 0**, alinhando o console ao arquivo | o critério de aceite é "o valor gravado coincide com o da linha de debug"; assim console e arquivo dizem exatamente a mesma coisa |

---

### Item 4 — `idSensor = "jardim"` em snapshot e boot

| Onde | Mudança | Por quê |
|---|---|---|
| `persistir` (158-159) | `idSensor` = `"jardim"` quando `tipo` é `Snapshot` ou `Boot`; senão `sensor-%02u` | resolve o defeito atual (snapshot sem nada molhado saía como `sensor-01`) |
| `tarefaAmostragem` (131-134) | **Opção A (recomendada):** manter o cálculo de `sensAtivo` e apenas ignorá-lo no `persistir` para snapshot/boot — **zero** alteração na lógica de amostragem. **Opção B:** remover o cálculo (fica código morto) | respeita ao pé da letra "não altere a lógica de amostragem" |

O `nivelAgua` do snapshot continua sendo o nível atual (`0.0` quando nada está molhado),
pois vem de `nivelDe(estado)` (79-83), que não muda.

---

### Item 5 — hora do evento correta

| Onde | Mudança | Por quê |
|---|---|---|
| nova constante | `constexpr time_t EPOCH_MINIMO_VALIDO = 1700000000;` | distingue "relógio real" de "sem sync" (hoje o código usa `now < 100000`) |
| `formatarTimestamp` (54-63) | reescrita do núcleo: se o relógio for válido → `t = now - (millis() - ms)/1000` (**instante do evento**); senão → fallback `MARCO_FALLBACK + ms/1000` | hoje, com NTP, o `ms` é **ignorado** e sai a hora da *escrita*; com a fórmula, um backlog não falsifica o instante |
| nova função | `static bool relogioValido()` | usado em 2 lugares (timestamp + `horaSincronizada`) |
| nova função **marcada** | `static void iniciarSincronizacaoNtp()` — **placeholder vazio com comentário** indicando exatamente onde entram `WiFi.begin()` + `configTime(0, 0, "pool.ntp.org", …)` (a API `configTime` já existe no core) | é o **ponto claro de onde o NTP será ligado** |
| `setup()` (após 205) | chamada a `iniciarSincronizacaoNtp()` | hoje é no-op; quando o Wi-Fi for ligado, basta preencher a função |
| `struct Evento` | **novo** `bool relogioValido;` capturado **na criação** do registro | garante que registro criado antes do sync e gravado depois ainda saia com `horaSincronizada:false` e hora de fallback (ver item 6) |

**Por que satisfaz "3 s de intervalo" nos dois regimes:** com relógio válido, os dois
eventos dão `now - (millis()-tMs)` e a diferença de `(millis()-tMs)` é o intervalo
real; sem relógio, ambos usam `tMs/1000` a partir da mesma base — o intervalo real é
preservado. `millis() - ms` é aritmética sem sinal, segura no estouro de 2³² ms
(49,7 dias).

**Observação de semântica:** no regime com NTP o timestamp passa a ser "instante do
evento", não "instante da escrita" — que é exatamente o que o item 5 pede.

---

### Item 6 — campo `horaSincronizada` em toda linha

| Onde | Mudança |
|---|---|
| `persistir` | `"horaSincronizada":true|false` em **todas** as linhas (evento, snapshot, boot), lendo `e.relogioValido` capturado na criação |

Um reset não pode apresentar hora de fallback como se fosse real: como o sinal é
capturado na **criação** do registro, ele reflete o estado do relógio naquele
instante, não no momento da gravação.

---

### Item 7 — mensagem de boot

| Onde | Mudança | Por quê |
|---|---|---|
| novas constantes (perto de 28) | `constexpr double LATITUDE = 0.0;` e `LONGITUDE = 0.0;` com o comentário **"PLACEHOLDER — valor configurado, não medido; a equipe define o valor real"** | cumpre o requisito sem inventar uma coordenada "real" |
| `tarefaAmostragem`, **imediatamente após** a leitura inicial do estado (96-98) e **antes** do `for(;;)` | cria o registro `Boot` com `seq = g_seq++` (= **0**), `nivelAgua = nivelDe(estado)`, `idSensor = "jardim"`, `bateria = BATERIA_PADRAO`, `timestamp`, `bootId`, `lat`, `lon`, `horaSincronizada:false`, e chama `enviar(...)` | **é aqui que o nível do boot é conhecido com exatidão** (mesma leitura usada pela tarefa) e, como a fila é FIFO, a linha de boot é **necessariamente a primeira gravada**, antes de qualquer evento/snapshot |
| alternativa | criar em `setup()` (literalmente como o item 7 pede) → exigiria uma **segunda** leitura dos sensores via `lerSensor()`, com risco de divergir do estado inicial da tarefa por causa do ruído do `SIMULATION` | por isso a recomendação acima |

A linha de boot carrega também os 5 campos obrigatórios do contrato
(`idJardim`, `idSensor`, `nivelAgua`, `bateria`, `timestamp`), para passar na
validação atual da API.

---

### Item 8 — terminador LF em vez de CRLF

| Onde | Mudança |
|---|---|
| `persistir` (166) | `f.println(linha);` → `f.print(linha); f.print("\n");` (ou `f.printf("%s\n", linha)`) |

`Print::print` não acrescenta nada; o `\n` é gravado cru.
**Bônus:** `despejarFila()` (180) usa `readStringUntil('\n')` — hoje, com CRLF, ele
devolve o `\r` junto e o console mostra um CR espúrio; com LF isso some.

---

### Item 9 — arquivo cheio: descartar os **antigos**, manter os novos

Estratégia recomendada (detalhada em §4):

| Onde | Mudança | Por quê |
|---|---|---|
| nova função | `static void compactarFila(File& f, size_t bytesALiberar)` — lê o arquivo de trás para frente, acha o offset da primeira linha da cauda a manter, escreve a cauda **+ a linha nova** em `/fila.tmp` (só linhas completas, com LF), fecha, e faz `LittleFS.rename("/fila.tmp", ARQ_FILA)` | o `rename` é a **única** forma de substituir o arquivo (não há `truncate`); o original só é substituído **depois** que a cópia está completa |
| `persistir` (149-153) | em vez de "se `size > MAX` → descarta o novo": se não couber → **compactar** (descartando os mais antigos) e **gravar o novo** | inverte a política: o dispositivo nunca fica mudo |
| `descartados` | **+1 por registro antigo removido**, mantendo os incrementos já existentes (fila cheia, erro de abertura) | requisito explícito do item 9 |
| nova função | `static void recuperarLinhaParcial()` — verifica se o arquivo termina com `'\n'`; se não, a última linha (truncada por falta de energia) é descartada e o arquivo é reescrito sem ela (mesmo mecanismo temp+rename) | **garante que o arquivo nunca termine com linha incompleta**, inclusive após reset no meio de uma escrita |
| chamadas | `recuperarLinhaParcial()` no `setup()` (logo após `LittleFS.begin`) e antes de cada append | o arquivo fica sempre íntegro |

**Efeito colateral a documentar:** como o descarte é do mais **antigo**, o arquivo
pode **começar** com `seq > 0` (ex.: começar em `seq=250`). O receptor não vê um
buraco *dentro* do arquivo, mas vê que o início foi perdido — é para isso que
servem `descartados`, `bootId` e `seq`. É a contrapartida direta de "descartar os
antigos".

**Granularidade:** sugiro descartar em **blocos** (liberar ~25% do limite = 16 KB
por compactação), para que a reescrita aconteça ~1 vez a cada 75-80 anexos, em vez
de 1 vez por anexo.

---

### Item 10 — `docs/contrato-telemetria.md`

| Seção | Mudança |
|---|---|
| nova seção **"Formato atual gravado pelo firmware (ESP32)"** | tabela com os 3 tipos de linha e **quais campos são opcionais por tipo** (`deltaTMs` só em evento com subida; `lat`/`lon` só no boot); semântica de `seq`/`bootId`/`horaSincronizada`; aviso de que o timestamp sem NTP é *fabricado* (`2026-10-15T00:00:00Z` + `millis()`) |
| tabela **"Divergências registradas"** (70-76) | remover/atualizar a coluna "Firmware ESP32" (está desatualizada: descreve `molhado`/`mascara`/`bootId`/`seq`/`tMs` do firmware antigo); manter as divergências **reais** contra a API (`jardimId`/`sensorId`/`nivelCm`, sem `timestamp`) |
| "Decisão adiada" (61-66) | registrar explicitamente que **continua ABERTA**: a divergência de **nomes** (`idJardim` vs `jardimId`, `idSensor` vs `sensorId`, `nivelAgua` vs `nivelCm`) e a divergência conceitual **nível × estado** — **não decididas nesta tarefa** |
| "Outras observações" (85-90) | corrigir "timestamp: não existe no código atual"; documentar `horaSincronizada` e o `deltaTMs` |

---

## 4. Alternativas para o item 9

### Alternativa 1 — Reescrever o arquivo (temp + rename) — **RECOMENDADA**

*Como funciona:* quando a linha nova não cabe, lê-se o arquivo de trás para frente
até achar a fronteira da linha cuja cauda cabe no espaço desejado; escreve-se essa
cauda + a linha nova em `/fila.tmp`; fecha-se; `rename("/fila.tmp", "/fila.ndjson")`.

| | |
|---|---|
| **Vantagens** | arquivo único (o receptor lê 1 arquivo, sem lógica de rotação); o caminho normal de gravação continua sendo **append simples**; **nenhuma linha cortada** (opera em fronteiras de linha); o `rename` só acontece **depois** que a cópia está completa e fechada → falha de energia no meio deixa o arquivo original intacto; temporário órfão é limpo no boot |
| **Desvantagens** | custo O(tamanho do arquivo) por compactação: leitura + escrita de ~48-64 KB na flash (wear); precisa de RAM para a cauda (limitada a ~16-32 KB por bloco — ok no ESP32); latência da compactação (~centenas de ms) **bloqueia o `loop()`**, e o `loop()` é onde a fila é drenada → pode aumentar o descarte por fila cheia durante a compactação |
| **Risco de perda de dados se faltar energia** | **BAIXO** — a perda é só a intencionada (os antigos descartados). Falha durante a escrita do temp → original intacto + temp órfão. Falha entre a escrita do temp e o rename → original intacto. Falha durante o rename → o LittleFS faz operações de metadata; o pior caso é ficar com o antigo ou o novo, e **nenhum dos dois truncado**, porque os dois foram escritos por completo |

### Alternativa 2 — Dois arquivos alternados (A/B)

*Como funciona:* `/fila_a.ndjson` e `/fila_b.ndjson`. Escreve-se sempre no arquivo
"ativo"; quando ele enche, troca-se para o outro e o antigo (ainda não confirmado
pelo servidor) é removido.

| | |
|---|---|
| **Vantagens** | **nenhuma reescrita** → sem wear extra e sem latência; escrita O(1); descarte barato (apagar o arquivo velho inteiro) |
| **Desvantagens** | o receptor precisa ler **dois** arquivos e **saber a ordem** (o `bootId`+`seq` resolve, mas o cliente precisa da lógica); ocupa 2× o espaço (128 KB de 1,31 MB — irrelevante); **não há descarte parcial**: ao alternar perde-se o arquivo inteiro, não só as linhas mais velhas → granularidade pior; risco maior de perder dados não-confirmados, porque o arquivo inteiro é jogado fora |
| **Risco de perda de dados se faltar energia** | **MÉDIO** — se a energia cai entre "terminou de gravar no B" e "apagou o A", ambos sobrevivem (ok); se cai no meio de uma escrita em B, o B pode ter a última linha truncada → precisa do mesmo mecanismo de recuperação de linha parcial; e o A pode ser apagado **antes** de o receptor confirmar → perda de dados não-transmitidos |

### Alternativa 3 — Arquivos segmentados em anel (N segmentos de tamanho fixo)

*Como funciona:* `/fila_00.ndjson` … `/fila_03.ndjson`, cada um de 16 KB. Escreve-se
sequencialmente; ao chegar no último, apaga-se o primeiro e recomeça-se nele.

| | |
|---|---|
| **Vantagens** | append O(1), sem reescrita; cada segmento é autocontido (o receptor lê em ordem); descarte barato (apagar 1 segmento) |
| **Desvantagens** | o receptor precisa ler N arquivos e conhecer a ordem/rotação; um segmento pode ser **reaproveitado** enquanto o anterior ainda não foi confirmado → mesma perda de dados não-confirmados da Alt. 2; mais arquivos = mais metadados no LittleFS; lógica mais complexa que a Alt. 2 |
| **Risco de perda de dados se faltar energia** | **MÉDIO** — mesmo problema da Alt. 2 (perda do segmento não confirmado) + linha parcial possível no segmento em escrita |

### Alternativa 4 — Recorte *in-place* do início do arquivo — **DESCARTADA**

Seria a mais barata (nada de reescrever), mas **não existe**: nem `File::truncate()`
nem `ftruncate` estão expostos pelo `FS`/`LittleFS` do core (`FS.h` só tem
`open/remove/rename/mkdir/rmdir`). Só seria possível via chamada direta à VFS do
ESP-IDF — **não recomendado** (quebra o `LittleFS` do Arduino e complica a
portabilidade).

### Recomendação

**Alternativa 1 (temp + rename), com descarte em blocos de ~25%.**

Motivos: (a) mantém o receptor simples — **um arquivo só**; (b) o descarte é
**granular** (some o necessário, não o arquivo inteiro), que é o que o item 9 pede;
(c) a operação de escrita é a mais barata possível (append); (d) já garante que o
arquivo **nunca** fique com linha truncada; (e) o custo de wear é amortizado pelo
descarte em blocos.

**Ressalva:** a compactação bloqueia o `loop()` por um instante — o bloco será
dimensionado (16 KB) e o comportamento explicitado em comentários no código e no
relatório da Etapa 2.

---

## 5. Tamanho das linhas e capacidade no limite atual

Medido com o formatador proposto (LF, `tipo` primeiro, `bootId` de 8 hex):

| Tipo de linha | Bytes (com LF) | Registros em 64 KB | Exemplo |
|---|---|---|---|
| **evento com `deltaTMs`** | **206** | **318** | `{"tipo":"evento","idJardim":"jardim-centro-01","idSensor":"sensor-01","nivelAgua":5.0,"bateria":100,"timestamp":"2026-10-15T00:00:12Z","seq":12,"bootId":"A1B2C3D4","deltaTMs":3000,"horaSincronizada":false}` |
| **evento sem `deltaTMs`** | **190** | **344** | `{"tipo":"evento",…,"seq":13,"bootId":"A1B2C3D4","horaSincronizada":false}` |
| **snapshot** | **190** | **344** | `{"tipo":"snapshot",…,"idSensor":"jardim",…,"seq":14,"bootId":"A1B2C3D4","horaSincronizada":false}` |
| **boot** (com `lat`/`lon`) | **205** | **319** | `{"tipo":"boot",…,"seq":0,"bootId":"A1B2C3D4","lat":0.0,"lon":0.0,"horaSincronizada":false}` |

**Comparação com o formato atual:** hoje a linha de evento tem **121 bytes**
(119 + CRLF) → **541 registros**. Com LF e os campos novos: **190-206 bytes** →
**~318-344 registros**.

| Cenário | Hoje | Depois |
|---|---|---|
| Só eventos (com delta) | 541 | 318 |
| Só snapshots | 541 | 344 |
| Mix 1 boot + N pares (evento+snapshot) | — | **329 registros / 65.149 bytes** |

**Ganho de ~40% no número de linhas é o preço dos metadados** (`tipo`, `seq`,
`bootId`, `horaSincronizada` ≈ +70 bytes por linha). Duas alavancas de compensação,
se quiserem: `bootId` de 4 hex (−4 bytes/linha) e `lat`/`lon` só no boot (já é assim).
Um campo `"ts"` mais curto economizaria ~10 bytes/linha, mas **renomearia**
`timestamp`, que está proibido — portanto não é opção.

**Nota sobre a capacidade efetiva:** com a política nova (descartar antigos), o
limite deixa de ser "capacidade" e passa a ser **janela de retenção**: o arquivo
sempre guarda os ~320 registros **mais recentes**, e o receptor pode ler uma janela
*rolling* em vez de um histórico completo.

---

## 6. Riscos de compatibilidade com a API atual

**Constatação que muda o enquadramento:** a API **não** valida
`idJardim`/`idSensor`/`nivelAgua`/`timestamp`. O DTO real
(`api-jardim-sentinela/.../dto/LeituraRequest.java:10-13`) valida `jardimId`,
`sensorId`, `nivelCm`, `bateria` — **4 campos, sem `timestamp`**. Consequência
verificada empiricamente: o payload real do firmware hoje retorna **400 Bad Request**.

| # | Risco | Gravidade | Mitigação / o que fazer |
|---|---|---|---|
| 1 | **Os campos novos são "desconhecidos" para a API e serão descartados em silêncio** — `tipo`, `seq`, `bootId`, `deltaTMs`, `horaSincronizada`, `lat`, `lon`. O Jackson 3 (Boot 4.1.1) tem `FAIL_ON_UNKNOWN_PROPERTIES` **desligado por default** e **não há** `spring.jackson.*` no `application.properties` | **Alta** (silencioso) | Nada quebra o 201, mas a API perderia `tipo`/`seq`/`bootId` — justamente o que habilita dedupe, heartbeat e detecção de perda. **Decisão da equipe:** aceitar `tipo`/`seq`/`bootId` no DTO da API, ou continuar ignorando. O firmware já fica pronto para quando a API aceitar |
| 2 | **A divergência de nomes é pré-existente e não é desta tarefa**: o firmware grava `idJardim`/`idSensor`/`nivelAgua` (contrato); a API exige `jardimId`/`sensorId`/`nivelCm` → **400 hoje** | **Alta** | **Fora do escopo desta tarefa** (é a divergência de nomes deixada em aberto). Registrando: enquanto ela não for resolvida, a integração HTTP receberá 400 em toda linha — **inclusive na linha de boot nova** |
| 3 | **`idSensor = "jardim"`** em snapshot/boot | Média | Passa no `@NotBlank` atual. Mas se a API passar a validar o **formato** do `idSensor` (ex.: `sensor-\d{2}`), "jardim" passa a falhar. Sinalizar à equipe junto com a decisão de nomes |
| 4 | **Linha de boot com `nivelAgua` = nível lido no boot** (0.0/5.0/10.0/15.0) e `bateria` = 100 | Baixa | Passa em `@PositiveOrZero` e `@Min(0)@Max(100)`. Registro apenas que `bateria` continua sendo constante (não há sensor de bateria) |
| 5 | **`timestamp` com hora de fallback** (`2026-10-15T00:00:00Z` + millis) | Média | É ISO-8601 válido (passaria na validação do contrato), mas é **instante falso**. Se a API passar a exigir "timestamp próximo de agora", as linhas com `horaSincronizada:false` serão rejeitadas — é para isso que o campo `horaSincronizada` existe: dá ao receptor o sinal para tratar essas linhas como *estimativas* |
| 6 | **`timestamp` em `"%Y-%m-%dT%H:%M:%SZ"`** (segundo de resolução, sem milissegundos) | Baixa | É ISO-8601 válido. Registrar que a resolução é de 1 s |
| 7 | **Sem proteção contra duplicatas na API** (nenhuma constraint de unicidade verificada) | Média | Com `seq`+`bootId` o firmware dá material para o receptor detectar retransmissão, mas a API precisaria usar esses campos. Registrado como **aberto** |
| 8 | **Mudança de `idSensor` dos snapshots** (de `sensor-0X` para `jardim`) | Média | Quebra qualquer consumidor que contasse snapshots pelo `idSensor`. Como o formato atual só é consumido pelo próprio projeto (e a API hoje rejeita tudo), o risco é baixo — mas precisa constar no doc |

**Resumo:** os itens 1-10 **não aumentam** o risco de compatibilidade — nenhum campo
novo é obrigatório para a API, e todos são ignorados silenciosamente (risco 1). O 400
atual é **pré-existente** (risco 2) e pertence à divergência de nomes em aberto. O
que o firmware ganha é **capacidade de diagnóstico** (`tipo`, `seq`, `bootId`,
`horaSincronizada`) que a API ainda não consome.

---

## 7. Perguntas em aberto (bloqueiam o início da Etapa 2)

1. **Linha de boot: onde criar?** Recomendo **dentro da `tarefaAmostragem`**, logo
   após a leitura inicial (96-98) — garante nível exato e "boot primeiro" sem segunda
   leitura. O item 7 diz "no setup()"; se preferir o `setup()` literal, crio uma
   segunda leitura dos sensores (com possível divergência por causa do ruído do
   `SIMULATION`). **Posso seguir sua letra (setup) ou a recomendação?**
2. **`bootId`: 8 hex (recomendado) ou 4 hex** (economiza 4 bytes/linha)?
3. **`lat`/`lon`: só na linha de boot** (como o item 7 diz) **ou em todas as linhas**
   (+~37 bytes/linha)?
4. **Descarte em blocos de ~25% (16 KB) por compactação** — ok, ou prefere descartar
   o mínimo exato a cada vez (compacta quase todo append: mais wear e mais latência)?

---

## 8. Entregáveis da Etapa 2 (após aprovação)

- **(a)** Resumo do que mudou em **cada função** (com o comentário de documentação já
  embutido no código).
- **(b)** **Exemplos reais** de uma linha de cada tipo (`boot`, `evento` com e sem
  `deltaTMs`, `snapshot`) geradas na simulação.
- **(c)** **Roteiro de teste passo a passo** para repetir no Wokwi e validar **cada
  critério de aceite**, incluindo: reset do ESP32, arquivo cheio e relógio sem
  sincronização.

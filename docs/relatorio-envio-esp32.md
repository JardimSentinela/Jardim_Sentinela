# Relatório — camada de envio e store-and-forward (ESP32)

> **Arquivo compilado:** `esp32/src/main_alt.cpp`
> **Branch:** `feat/SCRUM-34-HW-03-Emular-LoRa-e-resiliência-de-conectividade`
> **Build:** PlatformIO, env `esp32dev`, `-DSIMULATION`, LittleFS.
> Este documento é atualizado a cada incremento.

## Decisões tomadas

| Tema | Decisão |
|---|---|
| `delay(20)` no fim do `loop()` | **Removido** (opção C). O loop não bloqueia; a amostragem (prioridade 3) continua preemptando. |
| "Boot é a primeira linha do arquivo" | Interpretado como **primeira linha gerada neste boot** (`seq 0`), não a primeira linha física após um reboot. |
| Branch de trabalho | SCRUM-34, sem branch nova; docs alheios não são editados. |
| `.gitignore` | Pode receber `secrets.h` no INC 2 (autorizado). |
| Persistência (SCRUM-31) | Já presente neste branch; **não reimplementada**. |

---

## INC 0 — Reconhecimento (sem edições)

- Etapa 2 da persistência **aplicada**: `tipo`, `seq`, `bootId`, `horaSincronizada`,
  `deltaTMs`, compactação 25% com `temp + rename`, LF.
- `char linha[320]` **cabe** o pior caso: 242 B (evento com `deltaTMs` e `origem`) + LF.
- O arquivo termina em LF (`print(linha)` + `print("\n")`).
- Build limpo passava: RAM 7,0% (22.960 B), Flash 26,0% (340.601 B), sem warnings.
- `sketch.ino.bak` não entra no build. Nenhum segredo versionado naquele momento.

## INC 1 — Costura e diagnóstico base

### Mudanças em `esp32/src/main_alt.cpp` (+87 / −6)

| Item | Detalhe |
|---|---|
| `origem` | Novo campo em todas as linhas: `"simulacao"` com `-DSIMULATION`, `"campo"` no ramo LoRa. |
| `enviarRegistro(const Evento&)` | Stub que só retorna `false` e avisa uma vez no console. |
| `loop()` | Chama `enviarRegistro()` no fim, com o último registro drenado; `delay(20)` removido. |
| `'i'` | `imprimirDiagnostico()`: bytes, linhas, pendentes, tentativas, confirmados, falhas, descartados, bootId, relógio. |
| Boot | `logarEstadoBoot()`: `filaExiste`, `bytes`, `bootId`, `relogio`. |
| Contadores | `tentativas`, `confirmados`, `falhas` em `g_envio`; começam em zero. |

### O que não mudou

`tarefaAmostragem`, `lerSensor`, `nivelDe`, histerese, debounce, patamares, pinos,
períodos, `persistir`, compactação, recuperação de linha parcial, `'d'` e `'L'`.

### Evidência (build real)

```
RAM:   [=         ]   7.0% (used 22960 bytes from 327680 bytes)
Flash: [===       ]  26.1% (used 341725 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

Formato verificado fora do dispositivo (mesma composição de `montarLinha`):

```
boot:     235 bytes (com LF) -> JSON valido
snapshot: 219 bytes (com LF) -> JSON valido
evento:   242 bytes (com LF) -> JSON valido
```

### Observações

- `pendentes` era derivado do arquivo; sem cursor (INC 4) ele era igual a `linhas`.
  A partir do INC 4, `pendentes` é contado a partir do cursor confirmado.
- Loop em busy-wait: `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1` não está setado e
  `loopTaskWDTEnabled=false`, então não há panic de watchdog. Custo: CPU do core 1
  em 100% e consumo maior no hardware real.

## INC 2 — Transporte HTTPS

### Mudanças

| Arquivo | O que mudou |
|---|---|
| `esp32/src/main_alt.cpp` (+118 / −7) | `WiFi.begin()` sem esperar; máquina de estados `WIFI_OCIOSO/CONECTANDO/CONECTADO` com timeout de 15 s e nova tentativa a cada 30 s; `postarRegistro()` com `HTTPClient` + `WiFiClientSecure` e timeout de 8 s; `enviarRegistro()` agora conta tentativas/confirmados/falhas; log do código HTTP. |
| `esp32/src/secrets.h` (ignorado) | `WIFI_SSID`, `WIFI_SENHA`, `URL_RECEPTOR` reais. |
| `esp32/src/secrets.example.h` (versionado) | Placeholders. |
| `esp32/.gitignore` | `secrets.h` ignorado. |

- Payload montado com o **mesmo** `montarLinha()` do arquivo (buffer fixo, sem `String`).
- Wi-Fi inicia no `setup()` e é acompanhado por `atualizarWifi()` no `loop()`; nenhum
  `delay()` e nenhuma espera bloqueante da conexão.
- `setInsecure()` está marcado com comentário de limitação (gateway do Wokwi não
  entrega a cadeia de certificados).

### O que não mudou

Amostragem, histerese, debounce, patamares, pinos, períodos, `persistir`,
compactação, recuperação de linha parcial, `'d'` e `'L'`.

### Evidência

Build real:

```
RAM:   [=         ]  14.5% (used 47436 bytes from 327680 bytes)
Flash: [=======   ]  74.2% (used 971933 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

**POST real do ESP32 observado** no Webhook.site (execução do usuário no Wokwi):

```
49 requisicoes com User-Agent ESP32HTTPClient
IP de origem do navegador; intervalo de ~5 s (snapshots)
tamanhos: 209 e 210 bytes
corpo (exemplo):
{"tipo":"snapshot","idJardim":"jardim-centro-01","idSensor":"jardim","nivelAgua":15.0,
"bateria":100,"timestamp":"2026-10-15T00:00:11Z","seq":3,"bootId":"6F8DE646",
"horaSincronizada":false,"origem":"simulacao"}
```

**Sobre o HTTP 429:** não é falha do firmware. O token **anônimo** do Webhook.site
aceita no máximo **50 requisições**; as 49 do ESP32 + 1 teste manual esgotaram a
cota e o receptor passou a responder 429 (armazenando a requisição mesmo assim).
Foi criado um token novo para o teste de 2xx. Para execuções longas (INC 7/8),
usar um token de conta Webhook.site ou um receptor sem cota (ex.: `httpbin.org`,
`ntfy.sh`), já verificados com HTTP 200.

**200 confirmado** pelo usuário no Wokwi após a troca do token.

### Como testar no Wokwi

1. Preencha `esp32/src/secrets.h` (SSID `Wokwi-GUEST`, senha vazia, URL do Webhook.site).
2. Build/upload e abra o monitor serial em 115200.
3. Observe `WIFI: conectando...` seguido de `WIFI: conectado em ... ms`.
4. A cada snapshot (5 s) deve aparecer `HTTP: codigo 200, ... bytes`.
5. Confira o corpo no Webhook.site.
6. Para testar o timeout: use uma rede inválida; o log deve mostrar
   `WIFI: timeout de 15 s` e o firmware continua gravando snapshots.

> **Cota do Webhook.site:** token anônimo aceita **50 requisições** e depois passa a
> responder **429** (o corpo ainda é armazenado). Com snapshots a cada 5 s, isso dá
> ~4 minutos. Para testes longos, usar token de conta ou receptor sem cota.
> Um 429 **não** é falha do firmware: o INC 5 é que vai tratar esse código.


## INC 3 — Hora

### Mudanças

| Item | Detalhe |
|---|---|
| `iniciarSincronizacaoNtp()` | Agora chama `configTime(0, 0, "pool.ntp.org", "time.google.com")` **sem esperar**; é chamada quando o Wi‑Fi conecta. `NTP_TIMEOUT_MS = 10000`. |
| `parsearDataHttp()` | Converte o header `Date` (RFC 7231) em epoch UTC sem depender de `timegm`/TZ (algoritmo civil). |
| `postarRegistro()` | No primeiro 2xx sem hora confiável, guarda o `Date` em `g_epochHttp` (não mexe no relógio ainda). |
| `atualizarHora()` | Se `time()` ficar válido → `fonte=ntp`; se o NTP estourar o timeout e houver `Date` → `settimeofday` + `fonte=http`; sem nenhum → `fonte=fallback`. |
| `setup()` | Não inicia mais o NTP antes da rede; o relógio segue inválido até sincronizar. |
| Topo do arquivo | `#define SIMULATION` protegido por `#ifndef` (some o warning de redefinição). |

`horaSincronizada` continua capturado **na criação** do registro (`relogioValido()`);
o timestamp nunca é recalculado para outro regime depois. Registros criados antes
do sync ficam com `false` e hora de fallback; depois, `true` e epoch real.

### O que não mudou

Amostragem, histerese, períodos, `persistir`, compactação, formato do arquivo,
`'d'`, `'L'` e o transporte do INC 2.

### Evidência

Build sem warnings:

```
RAM:   [=         ]  14.5% (used 47476 bytes from 327680 bytes)
Flash: [========  ]  75.7% (used 992209 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

Log real no Wokwi (fonte NTP venceu):

```
HORA: fonte=ntp epoch=1792617635
```

Teste do parser (função extraída do firmware e compilada no host):

```
Wed, 21 Oct 2026 20:39:43 GMT -> ok epoch=1792615183
Thu, 01 Jan 1970 00:00:00 GMT -> ok epoch=0
Tue, 29 Feb 2028 23:59:59 GMT -> ok epoch=1835481599
Sat, 01 Mar 2025 00:00:00 GMT -> ok epoch=1740787200
texto invalido -> rejeitou (ok)
4/4 casos corretos
```

### Como testar

1. Wokwi com Wi‑Fi: esperar `HORA: ntp iniciado (timeout de 10 s)` e depois
   `HORA: fonte=ntp epoch=17...` (epoch > 1.700.000.000).
2. Se o NTP não responder: após ~10 s aparece
   `HORA: fonte=fallback (sem sync em 10 s; aguardando Date)` e, no 2xx seguinte,
   `HORA: Date capturado (...)` + `HORA: fonte=http epoch=...`.
3. Sem rede (SSID inválido): só `fonte=fallback`.
4. `d`: a linha de boot (criada antes do sync) mantém `"horaSincronizada":false`
   e timestamp `2026-10-15T...`; snapshots criados depois do sync saem
   `"horaSincronizada":true` com a data real.
5. `i`: `relogio: valido` após a sincronização.

### Riscos

- Depende de o gateway do Wokwi encaminhar UDP/123; se não, o caminho `http` entra.
- O `Date` tem resolução de 1 s e vem do servidor; se o servidor estiver errado,
  o firmware confia nele (é a única fonte disponível sem NTP).
- Só o **primeiro** 2xx fornece o `Date`.
- `settimeofday` pode causar um salto no relógio; registros antigos não são
  reescritos porque carregam `relogioValido` e `tMs` da criação.

## INC 4 — Modo offline e cursor de confirmação

### Mudanças em `esp32/src/main_alt.cpp` (+193 / −36; líquido +157)

- Comando `'o'` alterna o modo offline lógico (`g_offline`). Com ele ativo,
  `enviarRegistro` devolve `false` na hora e nenhuma conexão é aberta; o arquivo
  continua crescendo normalmente.
- Cursor de confirmação `(ackBootId, ackSeq)` persistido na NVS
  (`Preferences`, namespace `envio`), carregado no `setup()` e gravado
  **somente depois de um 2xx**. Falha de energia entre o 2xx e a gravação
  resulta em reenvio (duplicata), nunca em perda.
- A cada tentativa, o firmware varre `/fila.ndjson`, pula até a linha do cursor
  e envia **a linha seguinte**. Linhas confirmadas não são apagadas nem
  reescritas; o cursor é identidade `(bootId, seq)`, não offset de byte.
- O POST passou a enviar a linha já gravada (`postarLinha`), sem remontar o
  JSON. Isso preserva exatamente o timestamp e o `horaSincronizada` da criação
  (o `montarPayload` do INC 2 foi removido).
- Cursor não encontrado no arquivo (compactação ou `'L'`): reancora no início,
  incrementa `reancoragens` e loga `ENVIO: cursor nao encontrado; reancorando
  no inicio`. Pode duplicar, mas não perde.
- `'i'` passou a mostrar `cursor`, `pendentes` (recontados a partir do cursor),
  `reancoragens` e `offline`.
- Falha de envio impõe uma pausa fixa de 5 s (`PAUSA_FALHA_MS`), provisória até
  o backoff exponencial do INC 5. Sem ela, um 429 do receptor viraria laço
  apertado de tentativas.

### O que não mudou

Amostragem, histerese, períodos, limiares, pinos, `persistir`, compactação,
formato do arquivo, `'d'`, `'L'`, branch LoRa do `SIMULATION` e o parser de hora.

### Evidência

Build limpo:

```
RAM:   [=         ]  14.5% (used 47556 bytes from 327680 bytes)
Flash: [========  ]  76.1% (used 997405 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

Teste de host com as funções **extraídas do firmware** (`lerLinha`,
`extrairChave`, `contarPendentes`, `buscarPendente`), simulando o arquivo:

```
arquivo vazio                          alvo=-1 pendentes=0 OK
sem cursor -> primeiro (0), 5 pend.    alvo=0 pendentes=5 OK
cursor A/1 -> proximo (2), 3 pend.     alvo=2 pendentes=3 OK
cursor na ultima -> nada, 0 pend.      alvo=-1 pendentes=0 OK
cursor sumido -> reancora (0), 5 pend. alvo=0 pendentes=5 OK
  reancoragens: 0 -> 1 OK
linha truncada no fim e ignorada       alvo=0 pendentes=5 OK
TODOS OS CASOS OK
```

### Como testar no Wokwi

1. `'o'` → `ENVIO: modo offline (nao envia)`. Esperar ~2 min: o Webhook não
   recebe nada e `'i'` mostra `bytes`/`pendentes` crescendo.
2. `'o'` de novo → `ENVIO: modo online`. O backlog é drenado em ordem; `'i'`
   mostra `cursor` avançando e `pendentes: 0` ao final.
3. Reset (botão EN, sem parar a simulação) → `BOOT: ... cursor=...` e
   `ENVIO: urlConfigurada=sim cursor=...`. O que já tinha 2xx não é reenviado;
   só as linhas novas (e as que estavam pendentes) saem.
4. Para exercitar a reancoragem: `'L'` apaga o arquivo mas preserva o cursor;
   no próximo envio aparece `reancorando no inicio` e `reancoragens` sobe.

Parar e reiniciar a simulação pode reinicializar a flash (LittleFS e NVS);
para o teste de reset, usar o botão de reset dentro da sessão.

### Riscos

- Uma gravação na NVS por 2xx: no dreno de um backlog grande há muitas
  escritas; aceitável para o volume atual, mas é desgaste de flash a observar.
- A varredura é O(arquivo) por registro → O(n²) no dreno. Com o teto de 64 KB
  isso é irrelevante no Wokwi; o INC 7 pode manter um offset em memória.
- Se a compactação remover a linha do cursor e houver linhas não confirmadas
  anteriores a ela, a reancoragem reenvia (duplicata) — não há perda.
  `perdidosNãoConfirmados` fica para o INC 8, junto da contabilidade de perdas.
- `'L'` não zera o cursor de propósito: a reancoragem cobre o caso sem apagar
  a NVS.
- O tamanho líquido ficou em +157 (levemente acima do teto de ~150) porque a
  varredura, a NVS e o modo offline formam uma unidade testável; dividir
  deixaria o cursor sem uso em uma das metades.

## INC 5 — Classificação da resposta e backoff

### Mudanças em `esp32/src/main_alt.cpp` (+79 / −12)

- `postarLinha` passou a devolver o **código HTTP** (ou negativo para falha de
  configuração/transporte) em vez de `bool`. A linha enviada e o cursor não
  mudam: falha sempre deixa o registro pendente.
- `enviarRegistro` devolve o código por parâmetro; offline continua retornando
  `false` na hora, sem sequer abrir conexão.
- **Backoff exponencial** para falhas transitórias (timeout, rede, 5xx e demais
  códigos): 2, 4, 8, 16, 32, 60, 60... segundos, sem jitter.
- **429** é tratado à parte:
  - com `Retry-After` em segundos: usa o valor (piso de 5 s, teto de 300 s);
  - sem header (ou com data HTTP): 30, 60, 120, 240, 300, 300... segundos.
  - O cursor **não avança**, porque o receptor pode ter gravado o corpo mesmo
    assim (é o caso do Webhook.site). O reenvio posterior pode duplicar.
- Sucesso zera o contador de falhas consecutivas e o `ultimoErro`; se havia
  falhas, loga `ENVIO: recuperado apos N falhas`.
- `'i'` ganhou `ultimoErro` (ex.: `http 429 (limite)`, `rede/timeout`,
  `http 500`) e `proximoEnvioEm` (segundos até a próxima tentativa).
- O teto de 60 s também vale para 4xx que não sejam 429: a linha continua na
  cabeça da fila e é repetida. Não há descarte silencioso neste incremento.

### O que não mudou

Amostragem, histerese, períodos, limiares, pinos, `persistir`, compactação,
formato do arquivo, `'d'`, `'L'`, cursor na NVS, modo offline e a branch LoRa.

### Evidência

Build limpo:

```
RAM:   [=         ]  14.5% (used 47596 bytes from 327680 bytes)
Flash: [========  ]  76.2% (used 998133 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

Backoff extraído do firmware e testado no host:

```
backoff falhas=1 ->  2000 ms OK
backoff falhas=2 ->  4000 ms OK
backoff falhas=3 ->  8000 ms OK
backoff falhas=4 -> 16000 ms OK
backoff falhas=5 -> 32000 ms OK
backoff falhas=6 -> 60000 ms OK
backoff falhas=100 -> 60000 ms OK (sem overflow)
429     falhas=1 ->  30000 ms OK
429     falhas=2 ->  60000 ms OK
429     falhas=3 -> 120000 ms OK
429     falhas=4 -> 240000 ms OK
429     falhas=5 -> 300000 ms OK
429     falhas=100 -> 300000 ms OK (sem overflow)
TODOS OS CASOS OK
```

Classificação/agendamento com as funções reais (`agendarNovaTentativa`):

```
500 1a falha -> 2 s                        espera=  2000 ms erro=http 500                 OK
500 2a falha -> 4 s (encadeada)            espera=  4000 ms OK
429 sem header -> 30 s                     espera= 30000 ms erro=http 429 (limite)        OK
429 Retry-After 7 -> 7 s                   espera=  7000 ms erro=http 429 (Retry-After)   OK
429 Retry-After 1 -> piso 5 s              espera=  5000 ms erro=http 429 (Retry-After)   OK
429 Retry-After 999999 -> teto 300 s       espera=300000 ms erro=http 429 (Retry-After)   OK
timeout (-1) -> 2 s                        espera=  2000 ms erro=rede/timeout             OK
URL vazia (-2) -> 60 s                     espera= 60000 ms erro=config: URL vazia        OK
400 -> backoff 2 s                         espera=  2000 ms erro=http 400                 OK
503 -> 2 s                                 espera=  2000 ms erro=http 503                 OK
TODOS OS CASOS OK
```

### Como testar no Wokwi

1. Com o token do Webhook.site esgotado (429): no primeiro envio aparece
   `HTTP: codigo 429` e `ENVIO: http 429 (limite); falha 1, proxima em 30 s`.
   As tentativas seguintes espaçam 60, 120, 240 e 300 s; o `'i'` mostra
   `ultimoErro: http 429 (limite)` e `proximoEnvioEm: N s`.
   O importante é que o firmware **para de martelar** o receptor.
2. Trocar `URL_RECEPTOR` em `secrets.h` por um endpoint saudável (ou esperar a
   cota): o próximo 2xx loga `ENVIO: recuperado apos N falhas`, o cursor avança
   e `'i'` volta a `ultimoErro: nenhum`.
3. Para ver o backoff 2/4/8 sem esperar a cota: apontar temporariamente para
   `https://httpbin.org/status/500` e observar os intervalos; depois restaurar.
4. Para 429 com `Retry-After`, um endpoint que devolva o header (ex.:
   `httpbin` não fornece) — o caso sem header já cobre o caminho padrão.

### Riscos

- O `Retry-After` em formato de data HTTP é ignorado (vira 0) e cai no backoff;
  apenas o formato em segundos é interpretado.
- O teto de 300 s vale tanto para `Retry-After` quanto para o 429 sem header;
  se o receptor pedir mais, o dispositivo tenta antes do pedido.
- 4xx permanente (400/401/403) mantém a fila parada naquela linha, com
  tentativas a cada 60 s. É conservador (não perde), mas não drena o restante;
  se o contrato do receptor definir descarte, isso entra em incremento próprio.
- Sem jitter: vários dispositivos com a mesma falha tendem a sincronizar as
  tentativas. Com um jardim isso não é problema; jitter pode ser adicionado
  depois sem mudar a interface.
- `falhasConsecutivas` não zera quando o Wi-Fi cai; ao reconectar, o backoff
  continua de onde parou (no máximo 60 s).

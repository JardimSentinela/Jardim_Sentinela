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
- 4xx que não seja 429 **para** a fila naquela linha: o cursor não avança e a
  nova tentativa fica para 5 min depois (ajustado no INC 6B para cumprir o
  INC 5). Não há descarte silencioso.

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
  tentativas a cada 5 min (o `'e'` do INC 6B permite exercitar isso). É
  conservador (não perde), mas não drena o restante; se o contrato do receptor
  definir descarte, isso entra em incremento próprio.
- Sem jitter: vários dispositivos com a mesma falha tendem a sincronizar as
  tentativas. Com um jardim isso não é problema; jitter pode ser adicionado
  depois sem mudar a interface.
- `falhasConsecutivas` não zera quando o Wi-Fi cai; ao reconectar, o backoff
  continua de onde parou (no máximo 60 s).

## INC 6 — Codec binário v1

O incremento completo (codec + gateway + `'t'`) passaria de ~350 linhas, então foi
dividido:

- **6A (esta entrega):** header `codec_quadro.h` com encode/decode e validação,
  mais o teste de round-trip/layout no host.
- **6B (próxima):** comando `'t'` no firmware, gateway local que decodifica o
  quadro e posta o JSON, e comparação campo a campo com a linha do arquivo.

### 6A — Mudanças

- **Novo `esp32/src/codec_quadro.h`** (header-only, sem dependência de Arduino):
  - `codec::Quadro` e `codec::Registro` com os campos do quadro v1.
  - `tamanho()` valida e calcula o tamanho exato (14 B de cabeçalho + 7 B por
    registro + 1 B de gap + 4 B de timestamp absoluto + 8 B de lat/lon no boot).
  - `codificar()` escreve little-endian e devolve 0 se não couber ou for inválido.
  - `decodificar()` rejeita versão, `n` fora de 1..6, tipo inválido, bateria
    > 100, gap no primeiro registro, quadro truncado ou com byte sobrando.
  - `seqBase` é o `seq` do primeiro registro; cada registro seguinte carrega
    `gap` (quantos `seq` foram pulados) quando necessário.
  - `dTs` é a diferença em segundos para `baseTs`; se negativa ou > 65534, usa
    `0xFFFF` + u32 absoluto.
- **`esp32/src/main_alt.cpp`:** apenas o `#include "codec_quadro.h"` (1 linha),
  para o toolchain do ESP32 compilar o header. Nenhum comportamento mudou.

### O que NÃO mudou

Amostragem, histerese, períodos, limiares, pinos, persistência, compactação,
transporte HTTPS, cursor, offline, backoff, `'d'`, `'L'`, `'i'`, `'o'` e o ramo
LoRa. O firmware não usa o codec ainda; o envio continua postando a linha.

### Evidência

Build do firmware com o header incluído:

```
RAM:   [=         ]  14.5% (used 47596 bytes from 327680 bytes)
Flash: [========  ]  76.2% (used 998133 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

Teste de host (`g++ -std=c++17 -Wall -Wextra`), com o header real:

```
evento (5 cm, delta 3,2 s)         21 bytes  OK
  hex (21 bytes):
    01 01 00 01 07 00 46 E6 8D 6F A3 2C D9 6A 00 00
    00 05 64 20 00
snapshot (10 cm, sem delta)        21 bytes  OK
boot (coords 0,0)                  29 bytes  OK
layout do cabecalho/registro       OK
3 registros: gap=2 e ts absoluto   40 bytes  OK
6 registros (56 bytes)             56 bytes  OK
pior caso: 6 boots com extras     133 bytes  OK
boot com lat/lon negativas         29 bytes  OK
rejeita versao/n/tam/bateria/gap   OK
TODOS OS CASOS OK
```

O hexdump do evento confere com o layout: versão `01`, flags `01` (simulada),
`jardimIdx=00`, `n=01`, `seqBase=0007`, `bootId=6F8DE646` (LE `46 E6 8D 6F`),
`baseTs=6AD92CA3` (LE `A3 2C D9 6A`), meta `00` (evento, sensor 0), `dTs=0000`,
`nivel=05`, `bateria=64`, `deltaT=0020` (32 × 0,1 s).

### Como testar (6A)

Não há comando novo no firmware ainda. O teste é o host acima; para repetir:

```
g++ -std=c++17 -Wall -Wextra -o teste_codec teste_codec.cpp && ./teste_codec
```

### Critério de aceite do 6A

Round-trip idêntico nos três tipos, layout little-endian conferido byte a byte,
limite de 6 registros (56 B) e rejeição de quadros inválidos: **passou**.

### Riscos e lacunas registradas

- **`horaSincronizada` não existe no quadro v1.** O layout especificado não tem
  bit para ele. No 6B, o gateway da simulação vai usar o valor da linha de
  origem como metadado fora da banda, só para o JSON postado ficar idêntico ao
  arquivo. No gateway real (LoRa), esse campo precisará ser inferido/definido
  ou entrar num quadro v2; não inventei um bit novo.
- `seqBase` é u16; um boot com mais de 65.535 registros truncaria a identidade.
  Com snapshot de 5 s isso levaria ~91 h; registrado como limite do v1.
- `flags` é global ao quadro: todos os registros compartilham `origem` e
  `coordenadas`. No uso atual todos são da mesma origem; um lote misto exigiria
  separar em quadros.
- O decoder exige que o buffer termine exatamente no último registro; é
  intencional para pegar erro de tamanho, mas rejeita padding futuro.

### 6B — Gateway local, `'t'` e `'e'`

### Mudanças em `esp32/src/main_alt.cpp` (+350 / −1)

- `parsearIso` / `formatarIso`: conversão entre `YYYY-MM-DDTHH:MM:SSZ` e epoch
  UTC, sem depender de `timegm`.
- `valorCampo` / `campoTxt` / `campoU32` / `campoDouble`: leitura mínima dos
  campos da linha NDJSON (sem biblioteca JSON).
- `montarQuadroDaLinha`: linha do arquivo → `codec::Quadro` com `n=1`
  (`idSensor`, `nivelAgua`, `bateria`, `seq`, `bootId`, `timestamp`,
  `deltaTMs`, `horaSincronizada`, `origem`, `lat/lon`).
- `montarJsonGateway`: quadro decodificado → JSON na **mesma ordem de campos**
  do arquivo. `horaSincronizada` vem da linha original (metadado fora da banda,
  porque o quadro v1 não o carrega).
- `gatewaySimulado`: monta o quadro, codifica, **decodifica** e devolve o JSON.
  No ramo `SIMULATION`, `enviarRegistro` posta esse JSON; o ramo `#else` ficou
  com o comentário “NÃO COMPILADO NO WOKWI — validar no hardware”.
- Comando **`'t'`**: round-trip de evento/snapshot/boot, comparação campo a
  campo, comparação do JSON do gateway com o do arquivo e hexdump com tamanho.
- Comando **`'e'`**: cicla `ok → 500 → 400 → timeout`. A resposta forçada é
  devolvida por `postarLinha` sem tocar na rede; trocar de modo zera a espera e
  o contador de falhas para o teste não ficar preso.
- **4xx (não 429) agora retenta em 5 min** (era 60 s): fecha a exigência do
  INC 5 de “parar, preservar e registrar `ultimoErro`”.
- `-4` (linha que o codec não conseguiu interpretar) entra no backoff como
  `codec: linha invalida`, com 60 s.

### O que NÃO mudou

Amostragem, histerese, períodos, limiares, pinos, persistência, compactação,
formato do arquivo, cursor, offline, 429/Retry-After, `'d'`, `'L'`, `'i'`,
`'o'` e a branch LoRa.

### Evidência

Build:

```
RAM:   [=         ]  14.5% (used 47596 bytes from 327680 bytes)
Flash: [========  ]  76.6% (used 1003469 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

Teste de host com as funções **reais extraídas do firmware** (`montarLinha`,
`montarQuadroDaLinha`, `montarJsonGateway`, `testarCodec`):

```
CODEC evento  : OK (21 bytes) JSON OK
  hex (21 bytes):
    01 01 00 01 07 00 46 E6 8D 6F 43 23 D9 6A 00 00
    00 05 64 20 00
CODEC snapshot: OK (21 bytes) JSON OK
CODEC boot    : OK (29 bytes) JSON OK
linha real -> gateway              OK
linha truncada rejeitada           OK
TODOS OS CASOS OK
```

O caso “linha real” pega a linha completa do arquivo (evento com `deltaTMs` e
`origem`), converte para quadro, decodifica e compara o JSON resultante com a
linha original usando `strcmp`: idêntico.

Backoff após o ajuste do 4xx:

```
400 (contrato) -> 5 min                    300000 ms  http 400 (contrato)        OK
401 (contrato) -> 5 min                    300000 ms  http 401 (contrato)        OK
404 (contrato) -> 5 min                    300000 ms  http 404 (contrato)        OK
codec invalido (-4) -> 60 s                 60000 ms  codec: linha invalida      OK
500 -> 2 s                                   2000 ms  http 500                   OK
timeout (-1) -> 2 s                          2000 ms  rede/timeout               OK
TODOS OS CASOS OK
```

### Como testar no Wokwi

1. `'t'` no Serial:
   ```
   --- TESTE CODEC ---
   CODEC evento  : OK (21 bytes) JSON OK
   ...
   CODEC: TODOS OK
   ```
   O hexdump de 21 B do evento e o de 29 B do boot devem aparecer.
2. `'e'` uma vez → `resposta forcada = 500`. Na próxima tentativa:
   `HTTP: forcado 500`, `ENVIO: http 500; ... proxima em 2 s`, depois 4, 8...
3. `'e'` de novo → `400`: `ENVIO: http 400 (contrato); ... proxima em 300 s`.
   O cursor **não** avança e a linha continua no arquivo.
4. `'e'` de novo → `timeout` (volta imediatamente, sem esperar 8 s) e, de novo,
   `ok` para voltar ao POST real.
5. Com `ok`, comparar o corpo recebido no Webhook com a última linha do `'d'`:
   para snapshots/eventos com `deltaTMs` múltiplo de 100 ms, o JSON é idêntico.
6. `'i'` deve mostrar `ultimoErro` e `proximoEnvioEm` coerentes com o modo.

### Critério de aceite do 6B

`'t'` imprime OK nos três tipos com hexdump; o JSON do gateway é idêntico ao da
linha no teste de host; `'e'` exercita ok/500/400/timeout; 400 não avança o
cursor e retenta em 5 min: **passou no host; falta a confirmação visual no
Wokwi**.

### Riscos e lacunas

- **`horaSincronizada` não está no quadro v1**: o gateway da simulação usa o
  valor da linha. No gateway real, o campo precisa ser inferido/definido ou
  entrar num v2.
- **`deltaTMs` é quantizado em 0,1 s** no quadro; um evento com 3175 ms chega
  como 3100 ms. O arquivo preserva o valor original. O teste `'t'` usa 3200 ms
  para a comparação exata.
- **`seqBase` é u16**: acima de 65.535 o JSON postado fica com o seq truncado
  (o cursor continua usando o seq original).
- **`flags` é global**: todos os registros do quadro compartilham origem e
  coordenadas; o lote de 6 do INC 7 precisa respeitar isso.
- **Linhas antigas** (anteriores ao `tipo`/`seq`/`bootId`/`origem`) não parseiam
  e param a fila em `codec: linha invalida` (60 s). O arquivo atual não tem
  essas linhas; a recuperação de linha truncada do INC 8 cobre outro caso.
- **A resposta forçada do `'e'` não é persistida** e zera o backoff ao trocar de
  modo; é uma ferramenta de teste.
- O gateway só aceita `jardimIdx=0` (mapa 0 → `jardim-centro-01`) e `n=1`; o
  lote entra no INC 7.
- O diff do 6B ficou em +350; o bundle 6A+6B foi aprovado depois do aviso de que
  o INC 6 completo passaria do teto.

---

## INC 7A — Lote de até 6 e prefixo 2xx

O INC 7 tem duas partes independentes. Esta entrega é o **7A**: transporte em
lote e confirmação do maior prefixo 2xx. A **ordem de drenagem** (snapshot mais
recente → boot → eventos FIFO → snapshots dizimados) e a dizimação ficam para o
**7B**, porque mexem em como o cursor representa registros confirmados fora de
ordem; o cursor continua conservador.

### Mudanças em `esp32/src/main_alt.cpp` (+156 / −106)

- `LoteLinhas` + `pularAteCursor` + `coletarLote` + `buscarLote`: coleta até 6
  linhas **consecutivas** depois do cursor, todas do **mesmo boot**, com `seq`
  crescente e gap ≤ 255. Se o cursor sumiu, reancora no início (como antes).
- `tentarEnviarLote` (substitui `tentarEnviarPendente`): o `LoteLinhas` é
  `static` (≈2 KB em BSS) para não somar à pilha do `WiFiClientSecure`;
  monta um quadro com
  `n` registros (1 a 6), codifica, decodifica no gateway local e faz **um POST
  por registro**. O primeiro erro interrompe o lote.
- Confirmação do **maior prefixo 2xx**: se os K primeiros registros voltarem
  2xx, `confirmarCursor` avança para o K-ésimo e grava a NVS **uma vez**,
  somando K em `confirmados`. Se K = 0, o cursor não muda.
- Se K > 0 e o próximo falhou, o prefixo é confirmado e o backoff do erro é
  agendado. O registro que falhou continua na fila.
- O lote não mistura boots nem origem (`simulacao`/`campo`); a flag de
  coordenadas é agregada (liga se algum registro for boot). Coordenadas só
  aparecem no JSON do boot.
- Logs: `ENVIO: lote K/N 2xx` ou `ENVIO: lote K/N 2xx (erro)`.
- `enviarRegistro`, `gatewaySimulado`, `buscarPendente` e
  `tentarEnviarPendente` foram removidos; o caminho agora é único.
- O ramo `#else` (LoRa) ficou com o comentário de hardware: enviar o quadro e
  confirmar o prefixo que o gateway reportar.

### O que NÃO mudou

Amostragem, histerese, períodos, limiares, pinos, persistência, compactação,
formato do arquivo, cursor (continua `(bootId, seq)` do último confirmado em
ordem de arquivo), offline, 429/Retry-After, 4xx em 5 min, `'d'`, `'L'`,
`'i'`, `'o'`, `'e'`, `'t'` e o codec.

### Evidência

Build:

```
RAM:   [==        ]  15.2% (used 49692 bytes from 327680 bytes)
Flash: [========  ]  76.6% (used 1003977 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

Seleção do lote (funções reais extraídas do firmware, `File` simulado):

```
sem cursor -> primeiros 6              n=6 seq=0..5 reanc=0  OK
cursor no 0 -> 1..6                    n=6 seq=1..6 reanc=0  OK
cursor no 2 -> 3..7 (5 restantes)      n=5 seq=3..7 reanc=0  OK
cursor no ultimo -> nada               n=0 seq=0..0 reanc=0  OK
cursor sumiu -> reancora no inicio     n=6 seq=0..5 reanc=1  OK
troca de boot -> para antes do boot B  n=4 seq=0..3 reanc=0  OK
gap 299 (>255) -> para                 n=1 seq=0..0 reanc=0  OK
gap 255 (cabe em u8) -> 2              n=2 seq=0..256 reanc=0  OK
seq repetido -> para                   n=1 seq=0..0 reanc=0  OK
limite max=3                           n=3  OK
TODOS OS CASOS OK
```

Prefixo 2xx (transporte simulado, JSON real do gateway):

```
6x 2xx -> confirma 6                     posts=6 conf=6 seq=10 agenda=0  OK
2x 2xx + 429 -> prefixo 2                posts=3 conf=2 seq=6 agenda=429  OK
400 no primeiro -> prefixo 0             posts=1 conf=0 seq=0 agenda=400  OK
2xx + timeout -> prefixo 1               posts=2 conf=1 seq=5 agenda=-1  OK
so 3 linhas -> n=3                       posts=3 conf=3 seq=7 agenda=0  OK
sem pendentes -> nao posta               posts=0 conf=0 seq=0 agenda=0  OK
linha invalida no meio -> n=2            posts=2 conf=2 seq=6 agenda=0  OK
boot+evento+snapshot (coords so no boot) posts=3 conf=3  OK
posts do caso 3 (ordem): 5 6 7
TODOS OS CASOS OK
```

Quadro com 3 registros: 35 bytes, e o JSON de cada índice é idêntico à linha de
origem (evento, snapshot, evento), incluindo os `seq` 5, 6 e 7.

### Como testar no Wokwi

1. Deixe acumular fila (`'o'` offline por alguns minutos) e volte online.
2. O log deve mostrar lotes de 6: `ENVIO: lote 6/6 2xx`, depois o próximo lote,
   até `pendentes=0`. Cada lote faz 6 POSTs (o Webhook recebe 6 corpos).
3. `'d'` lista o arquivo; `'i'` mostra `confirmados` e `pendentes`. O cursor
   avança em blocos de 6.
4. `'e'` uma vez (500): o próximo lote para no primeiro POST,
   `ENVIO: lote 0/6 2xx (http 500)`, cursor parado, backoff 2 s.
5. Para ver um prefixo parcial sem depender do receptor, o teste de host cobre
   2x 2xx + 429; no Wokwi o 429/erro no meio é difícil de forçar.
6. Meça o tempo de drenagem: `N` pendentes / 6 × tempo de um POST.

### Critério de aceite do 7A

Lote de até 6, sem misturar boot/origem, confirmação do maior prefixo 2xx e
cursor avançando só até o último confirmado: **passou no host**. Falta observar
os lotes no Wokwi. A **ordem** do INC 7 ainda não está implementada (7B).

### Riscos e lacunas

- **6 POSTs seguidos bloqueiam o loop**: no pior caso (timeout de 8 s) são ~48 s
  sem amostrar. O `delay()` não existe, mas a chamada HTTP é síncrona; uma fila
  de envio em outra task resolveria. O lote de 1 do INC 5 já tinha esse risco,
  menor.
- **Sem jitter** no backoff (pendente desde o INC 5).
- **Sem ordem/dizimação**: o 7A envia em ordem de arquivo. O snapshot mais
  recente, o boot e a dizimação entram no 7B.
- **Só o mesmo boot**: um quadro tem `bootId` único; ao trocar de boot, o lote
  para antes. Isso é correto para o formato v1.
- **Gap > 255** ou `seq` fora de ordem/duplicado: o lote para antes; a próxima
  tentativa começa naquela linha. Gap maior que 255 exige um quadro só para o
  salto (não implementado; o normal é 1 em 1).
- **`seqBase` u16**: após 65.535 registros no mesmo boot o `seq` do JSON trunca
  (cursor continua com o original). Limite do v1.
- **Linha inválida no meio**: o prefixo válido é confirmado e a inválida para o
  próximo lote em `codec: linha invalida` (60 s).
- **Prefixo só é observável na simulação**: no ramo LoRa quem sabe quantos
  registros o gateway postou é o gateway; o firmware precisaria receber esse
  número no ack.

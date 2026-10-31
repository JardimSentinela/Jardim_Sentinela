# Relatório — camada de envio e store-and-forward (ESP32)

> **Arquivo compilado:** `esp32/src/main_alt.cpp` (ponto de entrada) + módulos em `esp32/src/modulos/`
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
- Cursor não encontrado no arquivo (compactação): reancora no início,
  incrementa `reancoragens` e loga `ENVIO: cursor nao encontrado; reancorando
  no inicio`. Pode duplicar, mas não perde. No 7B, `'L'` preserva o cursor na
  NVS; como a chave sumiu do arquivo, o próximo envio reancora.
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
4. Para exercitar a reancoragem: no INC 4, `'L'` apagava o arquivo mas
   preservava o cursor. **A partir do 7B**, `'L'` zera conjunto e âncora; o
   cursor na NVS é preservado (como no INC 4), e o próximo envio reancora
   porque a chave sumiu.

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
- No INC 4, `'L'` não zerava o cursor de propósito. O 7B mantém isso: `'L'`
  preserva o cursor na NVS, mas zera conjunto e âncora; o próximo envio
  reancora porque a chave sumiu.
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
os lotes no Wokwi. A **ordem** do INC 7 foi implementada no 7B (abaixo).

### Riscos e lacunas

- **6 POSTs seguidos bloqueiam o loop**: no pior caso (timeout de 8 s) são ~48 s
  sem amostrar. O `delay()` não existe, mas a chamada HTTP é síncrona; uma fila
  de envio em outra task resolveria. O lote de 1 do INC 5 já tinha esse risco,
  menor.
- **Sem jitter** no backoff (pendente desde o INC 5).
- **Sem ordem/dizimação no 7A**: o 7A envia em ordem de arquivo; o 7B (abaixo)
  implementa a ordem e a dizimação.
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

---

## INC 7B — Ordem de drenagem, âncora e dizimação

O 7B fecha o INC 7. Ele mantém o transporte em lote e o prefixo 2xx do 7A e
muda **quais** registros entram em cada lote e **como** o cursor representa
confirmações fora de ordem.

### Mudanças em `esp32/src/main_alt.cpp` (+328 / −41)

> O diff ficou acima da meta de ~150 linhas. A âncora, o conjunto de chaves e o
> watermark são acoplados; uma divisão em 7B1/7B2 exigiria duas versões do
> cursor. Se preferir, ainda posso separar em commits/entregas menores.

| Item | Detalhe |
|---|---|
| Ordem | (1) snapshot mais recente ainda não confirmado — **âncora**, sozinho; (2) boot do boot corrente + eventos dele; (3) eventos FIFO de qualquer boot; (4) backfill a partir do cursor: boot antigo (contexto/coordenadas) + snapshots mantidos do mesmo boot. |
| Âncora | `g_ancoraEnviada` garante **uma** âncora por drenagem. É enviada sozinha porque os registros do quadro precisam de `seq` crescente e a âncora é o mais novo. É resetada no boot, ao voltar de offline e quando a fila esvazia. |
| Eventos furam a fila | `buscarEventos` é consultado antes do backfill em toda rodada; um evento novo não espera os snapshots antigos. |
| Dizimação | Snapshots de backfill com `seq % 10 == 0` são mantidos; os demais são descartados pela política e contados em `dizimados`. O cursor pode passar por eles. Não é perda: é descarte deliberado, separado de `descartados` (compactação do arquivo). |
| Cursor | Continua sendo `(bootId, seq)` do último confirmado **em ordem de arquivo**, gravado na NVS uma vez por avanço. Confirmações fora de ordem ficam num conjunto de 32 chaves em RAM. `avancarCursorResolvido` anda pelo prefixo já resolvido e pelos snapshots dizimados; para no primeiro registro que ainda precisa de 2xx. |
| Conjunto em RAM | `g_resolvidos[32]` (bootId + seq), com remoção ao ser alcançado. Se encher, a chave mais antiga sai: o registro pode ser reenviado (duplicata), nunca perdido. Reboot perde o conjunto: idem, duplicata. |
| Backfill | `buscarBackfill` inclui o boot antigo e os snapshots mantidos; não mistura dois boots no mesmo quadro. Se o gap entre o boot e o primeiro snapshot passar de 255, o boot vai sozinho e o snapshot no lote seguinte. |
| Reancoragem | Se a chave do cursor sumiu (compactação ou `'L'`), `abrirAposCursor` invalida o cursor (`g_ackValido=false`), limpa o conjunto e a âncora e relê desde o início. `selecionarLote` valida o cursor antes de escolher a ordem, para não dizimar a âncora nova no meio do processo. |
| `'L'` | Continua apagando o arquivo e preservando o cursor na NVS (como no INC 4); agora também zera o conjunto e a âncora. O próximo envio não acha a chave e reancora no início; a linha nova ancora. |
| Flags/coords | O boot entra no backfill para levar as coordenadas; a flag de coordenadas continua global ao quadro e só o JSON do boot as mostra. |
| Log | Além de `ENVIO: lote K/N 2xx`, cada lote imprime `ENVIO: lote N [ms]: tipo/seq ...`, na ordem em que os POSTs serão feitos. |
| `'i'` | Novos campos: `dizimados`, `resolvidosFora` (tamanho do conjunto) e `ancora` (`enviada`/`pendente`). |
| Removidos | `coletarLote`, `buscarLote` e `confirmarCursor` (substituídos por `selecionarLote`, pelo conjunto de resolvidos e por `avancarCursorResolvido`). |

### O que NÃO mudou

Amostragem, histerese, debounce, períodos, limiares, pinos, persistência,
compactação, formato do arquivo, `'d'`, `'o'`, `'e'`, `'t'`, offline,
backoff, 429/Retry-After, 4xx em 5 min, codec v1 e o contrato de telemetria.
Não há `delay()` no caminho de envio nem `String` no payload.

O `'L'` continua apagando o arquivo e preservando o cursor na NVS (como no
INC 4); agora também zera o conjunto e a âncora. O próximo envio reancora
porque a chave sumiu.

### Evidência de host

As funções reais de seleção/cursor foram extraídas de `main_alt.cpp` e
compiladas com `File` simulado (`/tmp/opencode/teste_drenagem.cpp` +
`funcs_drenagem.inc`). Cada cenário simula 2xx de todos os registros do lote e
chama `avancarCursorResolvido`, como o firmware faz:

```
--- mesmo boot, cursor no boot ---
lote 1: snapshot/61   ancora=sim
lote 2: evento/10 evento/20   ancora=nao
lote 3: snapshot/30 snapshot/40 snapshot/50 snapshot/60   ancora=nao
(fim)
cursor=AAAA0001/61 dizimados=54 resolvidos=0 nvs=3

--- mesmo boot, sem cursor ---
lote 1: snapshot/61   ancora=sim
lote 2: boot/0 evento/10 evento/20   ancora=nao
lote 3: snapshot/30 snapshot/40 snapshot/50 snapshot/60   ancora=nao
(fim)
cursor=AAAA0001/61 dizimados=54 resolvidos=0 nvs=2

--- boot antigo + boot corrente, sem cursor ---
lote 1: snapshot/3   ancora=sim
lote 2: boot/0 evento/4   ancora=nao
lote 3: evento/6   ancora=nao
lote 4: boot/0   ancora=nao
(fim)
cursor=BBBB0002/4 dizimados=7 resolvidos=0 nvs=1

--- snapshots 1..30, dizimacao 1/10 ---
lote 1: snapshot/30   ancora=sim
lote 2: snapshot/10 snapshot/20   ancora=nao
(fim)
cursor=AAAA0001/30 dizimados=27 resolvidos=0 nvs=2

--- backfill de boot antigo com snapshots mantidos ---
lote 1: snapshot/3   ancora=sim
lote 2: boot/0 evento/4   ancora=nao
lote 3: evento/25   ancora=nao
lote 4: boot/0 snapshot/10 snapshot/20 snapshot/30   ancora=nao
(fim)
cursor=BBBB0002/4 dizimados=2 resolvidos=0 nvs=1
```

Reancoragem (cursor antigo sumiu enquanto `g_ancoraEnviada=true`):

```
ENVIO: cursor nao encontrado; reancorando no inicio
--- reancora com ancora antiga ---
lote n=1 seq=15 ancora=sim cursorValido=nao -> OK
```

Sem invalidar o cursor, o snapshot 15 (não múltiplo de 10) seria dizimado antes
de ser escolhido como âncora. O teste cobre exatamente esse caso.

O caso de prefixo parcial confirma só o primeiro registro de um lote de boot +
três eventos; a rodada seguinte começa no segundo evento, sem pular o que
falhou:

```
--- prefixo parcial ---
lote inicial n=4
apos confirmar so o primeiro: proximo n=3 (seq=1)
prefixo parcial OK
TODOS OS CENARIOS OK
```

Observações da evidência:

- **Âncora**: sempre o primeiro lote, com `n=1`, e `ancora=sim`.
- **Ordem**: eventos antes dos snapshots; o boot corrente antes dos eventos.
- **Dizimação**: no cenário 1, 54 snapshots foram descartados (1..9, 11..19,
  21..29, 31..39, 41..49, 51..59); 30/40/50/60 foram mantidos.
- **Watermark**: o cursor terminou em 61 mesmo com a âncora confirmada antes;
  `resolvidosFora=0` no fim, ou seja, o conjunto foi drenado.
- **Reancoragem**: com cursor antigo + `g_ancoraEnviada=true`, a âncora nova
  (15) foi escolhida e não dizimada; o cursor inválido foi esquecido
  (`cursorValido=nao`).
- **NVS**: no máximo uma gravação por avanço do cursor, não uma por registro.
- **Boot antigo**: no cenário 5, o boot antigo foi junto com seus snapshots
  mantidos (10/20/30), mostrando que o backfill não para no boot.
- **Sem travamento**: um boot antigo nunca deixa a fila presa; ele é incluído no
  backfill e, quando não cabe com o próximo snapshot, vai sozinho.

Build final:

```
RAM:   [==        ]  15.3% (used 50212 bytes from 327680 bytes)
Flash: [========  ]  76.8% (used 1006665 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

### Como testar no hardware/Wokwi

1. `'L'` para limpar a fila e `'o'` para ficar offline. Deixe passar **5 min**
   (vários snapshots; provoque pelo menos um evento, se possível). Anote o que
   `'d'` mostra.
2. `'o'` para voltar online. A primeira linha deve ser a âncora:
   `ENVIO: lote 1 [<ms>]: snapshot/<seq mais novo>` seguida de
   `ENVIO: lote 1/1 2xx`.
3. Se o boot corrente ainda não foi confirmado, o próximo lote deve começar por
   `boot/0`; depois os eventos, depois os snapshots mantidos. A ordem aparece
   no log `ENVIO: lote N [ms]: ...`.
4. Deixe drenar até `pendentes=0`. Meça o tempo do primeiro `[ms]` ao último
   `[ms]`; com lotes de até 6 e POSTs sequenciais, o tempo é aproximadamente
   `n_lotes × latência_do_POST`.
5. `'i'`: `dizimados` deve ser aproximadamente o número de snapshots
   não múltiplos de 10 que ficaram para trás; `resolvidosFora` deve voltar a 0;
   `ancora` volta a `pendente` quando a fila esvazia.
6. **Duplicatas**: no receptor, agrupe por `(bootId, seq)`. O normal é cada
   chave aparecer uma vez; após reboot no meio de uma drenagem, ou se um lote
   falhou e foi repetido, a mesma chave pode aparecer de novo. O critério é
   **nenhuma chave perdida** e eventos nunca descartados; duplicata é aceita.
7. **Falha no primeiro POST**: `'e'` uma vez (500). O lote deve mostrar
   `ENVIO: lote 0/N 2xx (http 500)`, o cursor não avança e a âncora continua
   pendente. O próximo ciclo reancora.
8. **Prefixo parcial**: difícil de forçar no Wokwi; o teste de host cobre
   2×2xx + 429. No hardware, o 429/erro no meio depende do receptor.
9. **Offline → online**: o `g_ancoraEnviada` é resetado ao voltar online; a
   primeira drenagem ancora de novo, mesmo que a fila já tivesse sido drenada
   antes.
10. **`'L'` no meio**: com fila pendente, `'L'`; `'d'` deve ficar vazio e o
    próximo envio deve logar `reancorando no inicio` (o cursor antigo não
    existe mais no arquivo). Grave uma nova amostra e volte online: ela deve
    ser a âncora (`n=1`), sem ser dizimada pelo estado anterior.

### Critério de aceite do INC 7 (7A + 7B)

- Lote de até 6, mesmo boot, `seq` crescente e gap ≤ 255.
- Prefixo 2xx confirmado; o cursor só avança por 2xx ou por snapshot dizimado
  pela política.
- Ordem observável no log: âncora → boot corrente → eventos FIFO → backfill.
- Um evento novo não espera o backfill.
- Nenhum evento é descartado; snapshots fora da política são contados em
  `dizimados`.
- Duplicatas por `(bootId, seq)` são possíveis após reboot/falha, mas nenhuma
  chave é perdida.
- `'L'` ou cursor perdido: conjunto e âncora limpos; o próximo envio reancora
  e a linha nova ancora, sem dizimar um snapshot novo antes da âncora.
- Build limpo e testes de host passando.

O host passou; falta a observação no hardware/Wokwi.

### Riscos e lacunas do 7B

- **6 POSTs síncronos** continuam bloqueando o loop (até ~48 s com timeout de
  8 s). O `delay()` não existe, mas a latência HTTP é real; uma task de envio
  resolveria.
- **Âncora uma vez por drenagem**: snapshots novos que chegam durante uma
  drenagem longa não são âncora; entram no backfill e podem ser dizimados. Se a
  âncora falhar, ela é tentada de novo na próxima rodada.
- **Dizimação por `seq % 10`**: é determinística pelo `seq`, não pelo tempo. Em
  sequências curtas (poucos snapshots), pode manter poucos ou nenhum snapshot
  além da âncora.
- **Conjunto de 32 chaves**: se houver mais de 32 confirmações fora de ordem
  antes de o cursor alcançá-las, a mais antiga sai do conjunto e pode ser
  reenviada. Duplicata, não perda.
- **Conjunto em RAM**: reboot perde as confirmações fora de ordem; o cursor
  fica atrás e reenvia. Duplicata, não perda.
- **Reancoragem por compactação**: se a compactação remover a linha do cursor,
  o cursor é invalidado e o arquivo é relido do início. Registros já confirmados
  podem ser reenviados (duplicata), nunca perdidos; a validação no início de
  `selecionarLote` evita escolher o backfill antes da âncora nesse caso.
- **`'L'` e cursor**: o cursor na NVS é preservado de propósito (INC 4); como a
  chave não existe no arquivo apagado, o próximo envio reancora. O conjunto e a
  âncora são zerados; não use `'L'` esperando continuar a drenagem antiga.
- **Boot antigo + evento**: um evento FIFO pode ser postado antes do boot
  antigo (o evento é prioridade). O JSON tem `bootId`; o receptor deve
  correlacionar. Se o contrato exigir o boot antes do evento, essa ordem
  precisa mudar.
- **`contarPendentes`** conta linhas após o cursor, incluindo snapshots que
  serão dizimados; durante a drenagem ele pode superestimar. No fim, 0.
- **Linha inválida/truncada no meio**: `avancarCursorResolvido` e
  `buscarBackfill` param nela, então os registros seguintes ficam bloqueados
  até a recuperação do INC 8. Uma linha truncada no fim (queda de energia)
  também para a drenagem; é o caso já previsto para o INC 8.
- **NVS após 2xx**: se faltar energia entre o 2xx e a gravação do cursor, o
  registro é reenviado no próximo boot. Duplicata.
- **`seqBase` u16**: após 65.535 registros no mesmo boot, o `seq` do JSON
  trunca; limite do codec v1.
- **Sem jitter** no backoff (pendente desde o INC 5).

### Próximo

O INC 8 foi dividido em **8A** (ganchos `'x'`/`'f'` e contabilidade de perdas,
abaixo), **8B** (latência criação→2xx, p50/p95) e **8C** (protocolo de 20 min
offline e tabela dos 12 critérios). O 8A está implementado e testado no host;
falta a observação no hardware/Wokwi.

> Sem commit, merge ou push nesta etapa.

---

## INC 8A — Ganchos de arquivo e contabilidade de perdas

### Objetivo

Dar meios de injetar uma linha truncada (`'x'`), forçar a compactação (`'f'`) e
contar quantos registros **não confirmados** a compactação removeu
(`perdidosNaoConfirmados`), sem mudar a política de drenagem do 7B.

### Mudanças em `esp32/src/main_alt.cpp` (+120 / −6 sobre o 7B)

| Item | Detalhe |
|---|---|
| `'x'` — `injetarLinhaTruncada()` | Abre a fila em `"a"`, grava 81 bytes de um JSON sem fechamento e **sem LF**, fecha e liga `arquivoPodeTerLinhaParcial`. O próximo `persistir()` ou o próximo boot chama `recuperarLinhaParcial()` e remove só a cauda. |
| `'f'` — `forcarCompactacao()` | Se houver cauda parcial, repara antes. Chama `compactarFila(true)`, que mantém 3/4 do tamanho **atual** (não espera o arquivo chegar a 64 KB) e loga bytes antes/depois. |
| `compactarFila(bool forcar)` | Assinatura ganhou o parâmetro (default `false`, preservando o `persistir()`). No modo normal continua mirando 48 KB e só age acima disso; no forçado mira `total * 3 / 4`. |
| `offsetDoCursor(File&)` | Devolve o offset do início da linha do cursor `(bootId, seq)`, ou `(size_t)-1` se a chave sumiu. Usa `f.position()` antes/depois de `lerLinha`, sem contar offset de byte como identidade. |
| `contarPerdidosNoPrefixo(File&, size_t)` | Varre `[0, ate)`: linha antes/na chave do cursor = confirmada; depois dela, só é confirmada se estiver em `g_resolvidos` (2xx fora de ordem). Sem cursor no arquivo, conta o prefixo inteiro (conservador). |
| `g_envio.perdidos` | Novo contador `perdidosNaoConfirmados`; incrementado **somente depois** de `reescreverTrecho()` dar certo. `'i'` passa a imprimi-lo; `'L'` zera junto com `descartados`. |
| `'i'` | Nova linha `perdidosNaoConfirmados: N` depois de `descartados`. |
| Contabilidade | `descartados` continua contando **todas** as linhas antigas removidas; `perdidosNaoConfirmados` é o subconjunto que não tinha confirmação (cursor ou conjunto fora de ordem). |

### O que NÃO mudou

Amostragem, histerese, debounce, patamares, pinos, períodos, `montarLinha`,
formato NDJSON, `persistir` no caminho normal, cursor na NVS, ordem do 7B
(âncora → boot corrente → eventos FIFO → backfill dizimado), codec v1,
`'d'`, `'L'`, `'o'`, `'e'`, `'t'`, `'i'` (só ganhou uma linha) e o ramo LoRa do
`SIMULATION`.

### Como testar no hardware/Wokwi

1. Faça o build/upload e abra o monitor em 115200.
2. `'L'` para começar com a fila vazia; espere o boot e pelo menos um snapshot
   (o arquivo precisa ter uma linha completa antes do `'x'`).
3. **Linha truncada:** pressione `'x'`. Deve aparecer
   `TESTE: linha truncada injetada (81 bytes, sem LF)`.
   - Resete o ESP32 **em até 5 s** (antes do próximo snapshot). No boot deve
     aparecer `AVISO: linha parcial removida (81 bytes)` e o `'d'` deve mostrar
     as linhas completas anteriores, sem a cauda.
   - Se um snapshot chegar antes do reset, o próprio `persistir()` remove a
     cauda; nesse caso repita o `'x'` para observar o caminho do boot.
4. **Compactação forçada:** com pelo menos 4 linhas no arquivo, pressione `'f'`.
   Deve aparecer `--- COMPACTACAO FORCADA ---`, uma linha
   `Fila compactada (forcada): N registro(s) ...` e
   `TESTE: compactacao forcada ok (X -> Y bytes)`. O `'d'`/`'i'` devem mostrar
   menos bytes/linhas, e `descartados`/`perdidosNaoConfirmados` devem subir.
5. **Cursor removido:** com o modo offline (`'o'`), deixe o cursor numa linha
   antiga e force `'f'` até essa linha sair. Ao voltar online, o próximo envio
   deve logar `ENVIO: cursor nao encontrado; reancorando no inicio`; duplicatas
   são aceitáveis, perda não.
6. **Contabilidade:** `'i'` deve mostrar
   `perdidosNaoConfirmados <= descartados`. `'L'` zera os dois.
7. Se o arquivo tiver só uma linha completa, `'f'` não remove nada (não há como
   cortar 25% sem quebrar a única linha); o comando loga `sem efeito`.

### Evidência

Build limpo:

```
RAM:   [==        ]  15.3% (used 50212 bytes from 327680 bytes)
Flash: [========  ]  76.9% (used 1008041 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

Teste de host com as funções **extraídas do firmware** (`lerLinha`,
`extrairChave`, `indiceResolvido`, `offsetDoCursor`,
`contarPerdidosNoPrefixo`, `inicioDaLinhaSeguinte`, `reescreverTrecho`,
`recuperarLinhaParcial`, `compactarFila`, `forcarCompactacao`,
`injetarLinhaTruncada`) e um LittleFS em memória. Doze cenários, todos OK:

```
== 1. forcada, sem cursor ==
Fila compactada (forcada): 3 registro(s) antigo(s) descartado(s), 3 nao confirmado(s) (total 3/3)
OK   descartados=3
OK   perdidos=3
== 2. cursor no seq 1, removido junto ==
Fila compactada (forcada): 3 registro(s) antigo(s) descartado(s), 1 nao confirmado(s) (total 3/1)
OK   perdidos=1 (so o seq 2)
== 3. cursor no seq 2, ultimo removido ==
OK   perdidos=0
== 4. cursor no seq 8, preservado ==
OK   perdidos=0
OK   cursor ainda encontrado
== 5. cursor ausente do arquivo ==
OK   perdidos=3
== 6. cursor 1 e seq 2 resolvido fora de ordem ==
OK   perdidos=0
== 7. cursor 0, seq 1 resolvido, seq 2 pendente ==
OK   perdidos=1 (seq 2)
== 8. compactacao normal em arquivo pequeno nao mexe ==
OK   arquivo intacto
== 9. injecao de linha truncada + recuperacao ==
TESTE: linha truncada injetada (81 bytes, sem LF); resete para o boot recuperar
AVISO: linha parcial removida (81 bytes)
OK   recuperou exatamente as 2 linhas
== 10. duas compactacoes forcadas acumulam ==
OK   descartados=5 no total
OK   perdidos=3 no total
== 11. forcada em 2 linhas remove a mais antiga ==
OK   descartados=1
== 12. comando 'f' repara a cauda antes de compactar ==
AVISO: linha parcial removida (81 bytes)
Fila compactada (forcada): 3 registro(s) antigo(s) descartado(s), 3 nao confirmado(s) (total 3/3)
TESTE: compactacao forcada ok (400 -> 280 bytes)
TODOS OS CENARIOS OK
```

O método do host é o mesmo dos incrementos anteriores: as funções reais são
copiadas de `main_alt.cpp` para um `.inc` em `/tmp/opencode`, com `File`,
`LittleFS` e `Serial` simulados; o firmware não ganhou arquivo de teste.

### Critério de aceite do 8A

- `'x'` injeta uma linha JSON truncada sem LF; a recuperação remove apenas a
  cauda e preserva as linhas completas: **passou no host; falta o reset no
  hardware**.
- `'f'` compacta mesmo abaixo de 64 KB, mantendo 3/4 do tamanho atual, e loga
  bytes antes/depois: **passou no host; falta a observação no Wokwi**.
- `perdidosNaoConfirmados` conta só o que estava depois do cursor e fora do
  conjunto de resolvidos; não conta linhas confirmadas: **passou no host**.
- `'L'` zera `descartados` e `perdidosNaoConfirmados`: **passou no host**
  (reset explícito do teste).
- Build limpo: **passou**.

### Riscos e limitações do 8A

- **Janela do `'x'`**: o próximo snapshot (5 s na simulação) chama `persistir()`
  e repara a cauda antes do reset. O teste do caminho de boot exige reset
  rápido; se perder a janela, o arquivo continua íntegro e o `'x'` pode ser
  repetido.
- **Arquivo só com a linha injetada**: `recuperarLinhaParcial()` calcula
  `corte = 0` e `reescreverTrecho(0, 0)` não trunca. Neste firmware a linha de
  boot sempre existe antes do `'x'`, então o caso não ocorre na operação
  normal; fica registrado como limite do gancho de teste.
- **Contabilidade após reboot**: o conjunto de confirmados fora de ordem vive
  na RAM. Se um reboot acontecer antes de o cursor alcançá-los e a compactação
  remover essas linhas, elas entram em `perdidosNaoConfirmados` mesmo já tendo
  recebido 2xx (o receptor tem o dado; o firmware só não lembra). É
  conservador: pode superestimar perdas, nunca esconder uma perda real.
- **`perdidos` é cumulativo no boot** e só é zerado por `'L'`; a soma
  `gerados = retidos + descartados` deve ser lida com `dizimados` separado
  (política do 7B) e com `perdidosNaoConfirmados` como subconjunto de
  `descartados`.
- **Custo**: `contarPerdidosNoPrefixo()` é mais uma varredura O(arquivo), mas
  só roda quando a compactação acontece (arquivo cheio ou `'f'`). A compactação
  continua bloqueando o `loop()` durante a cópia, como antes.
- **Uma linha**: `'f'` em arquivo com uma única linha completa não tem efeito
  (não há corte possível sem quebrar a linha); em duas linhas, remove a mais
  antiga.

### Próximo

**8B**: implementado na seção seguinte (latência criação→2xx, p50/p95 no
`'i'`). **8C**: implementado mais abaixo; falta executar o protocolo de 20 min
offline e fechar a tabela dos 12 critérios finais.

> Sem commit, merge ou push nesta etapa.

---

## INC 8B — Latência criação→2xx

### Objetivo

Medir, por epoch, o tempo entre a criação do registro e o seu 2xx, e mostrar
p50/p95 no `'i'`, restringindo a amostra ao **boot corrente** com
`horaSincronizada:true`.

### Mudanças em `esp32/src/main_alt.cpp` (+73 / −1, já sobre o 8A commitado)

| Item | Detalhe |
|---|---|
| `LATENCIAS_MAX = 128` | Anel fixo de 128 amostras (`g_latSeg`), sem alocação dinâmica. Guarda as confirmações mais recentes. |
| `g_latN`, `g_latProx` | Quantidade preenchida e próxima posição do anel. Ao encher, a amostra mais antiga é sobrescrita. |
| `g_latDesconhecidas` | Confirmações sem medição: `horaSincronizada:false`, relógio inválido no 2xx ou timestamp inválido. |
| `g_latForaBoot` | Confirmações de outro `bootId` (boot antigo): ficam fora do p50/p95 por causa do critério 10. |
| `registrarLatencia(segundos)` | Insere no anel e avança `g_latProx`; não bloqueia e não usa `String`. |
| `registrarLatenciaDoRegistro(r, horaSinc, boot)` | Filtra: `horaSinc=false` → desconhecida; `boot != g_bootId` → fora do boot; relógio/timestamp inválidos → desconhecida; senão calcula `max(0, agora_epoch − r.ts)`. |
| `percentilLatencia(p)` | Copia as amostras ativas, ordena por inserção e usa posto mais próximo (nearest-rank). `p50`/`p95` em segundos. |
| `'i'` | Nova linha `latencia: p50=... s p95=... s amostras=N desconhecidas=D foraBoot=F`; sem amostras, imprime `latencia: desconhecida (...)`. |
| `'L'` | Chama `zerarLatencia()`: o teste de 20 min começa com p50/p95 limpos. |
| `tentarEnviarLote()` | A cada 2xx, chama `registrarLatenciaDoRegistro(d.regs[i], horaSinc[i], lote.boot[i])`, antes de contabilizar o confirmado. |

### O que NÃO mudou

Amostragem, histerese, debounce, patamares, pinos, períodos, formato NDJSON,
cursor NVS, ordem do 7B (âncora → boot corrente → eventos FIFO → backfill
dizimado), codec v1, `'x'`/`'f'` do 8A, `'d'`, `'L'`, `'o'`, `'e'`, `'t'` e o
ramo LoRa do `SIMULATION`.

### Como testar no hardware/Wokwi

1. Build/upload; abra o monitor em 115200. Confirme que o NTP venceu
   (`HORA: fonte=ntp ...` ou `fonte=http ...`).
2. Faça uma drenagem normal e pressione `'i'`:
   `latencia: p50=X s p95=Y s amostras=N desconhecidas=D foraBoot=F`.
3. **Backlog:** `'o'` offline por ~2 min, `'o'` online e deixe drenar. O p50/p95
   deve subir porque a latência inclui o tempo parado; os snapshots dizimados
   não entram, só os efetivamente confirmados.
4. **Falha:** `'e'` uma vez (500), depois `'e'` até `ok`. Os 500/400/timeout não
   geram amostra; o `'i'` deve manter `amostras` e `desconhecidas` coerentes.
5. **`'L'`:** zera `latencia`, `descartados` e `perdidosNaoConfirmados`; use
   antes do protocolo de 20 min do 8C.
6. **Boot antigo:** após um reset, as linhas do boot anterior que ainda
   drenarem entram em `foraBoot`, não no p50/p95.
7. **Hora não sincronizada:** as primeiras linhas do boot, criadas antes do
   NTP/Date, entram em `desconhecidas`.

### Evidência

Build limpo:

```
RAM:   [==        ]  15.5% (used 50740 bytes from 327680 bytes)
Flash: [========  ]  76.9% (used 1008577 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

Teste de host com as funções **extraídas do firmware**
(`zerarLatencia`, `registrarLatencia`, `registrarLatenciaDoRegistro`,
`percentilLatencia`) e `time()` controlado:

```
== 1. sem amostras ==
OK   n=0
OK   p50=0
OK   p95=0
== 2. 1..100: p50/p95 nearest-rank ==
OK   p50=50
OK   p95=95
== 3. anel de 128: ultimas amostras ==
OK   p50=136
OK   p95=194
== 4. filtros de boot e horaSincronizada ==
OK   horaSinc=false -> desconhecida
OK   boot antigo -> foraBoot
OK   boot corrente: 100 s
OK   timestamp futuro -> 0 s
OK   relogio invalido no 2xx -> desconhecida
OK   timestamp invalido -> desconhecida
== 5. zerarLatencia limpa tudo ==
OK   contadores zerados
== 6. duas amostras ==
OK   p50=10
OK   p95=20
== 7. 128 amostras iguais ==
OK   p50=42
OK   p95=42
TODOS OS CENARIOS OK
```

O teste confirma o posto mais próximo: para 100 amostras `1..100`, p50=50 e
p95=95; depois de 200 inserções no anel de 128, sobram `73..200`, com p50=136
e p95=194.

### Critério de aceite do 8B

- p50/p95 calculados por posto mais próximo e limitados ao anel: **passou no
  host**.
- Só entram amostras do boot corrente com `horaSincronizada:true`; os demais
  são contados como desconhecidos/fora do boot: **passou no host**.
- `'i'` imprime a linha de latência: **implementado; falta a leitura no
  hardware**.
- `'L'` zera a medição: **passou no host** para `zerarLatencia()`; falta a
  confirmação no Wokwi.
- Build limpo: **passou**.

O p50/p95 real do protocolo de 20 min fica para o ensaio descrito no **8C**,
que também precisa de um receptor com 2xx disponível (a cota do Webhook.site
continua esgotada).

### Riscos e limitações do 8B

- **Resolução de 1 s**: o timestamp do arquivo é epoch em segundos; POSTs
  rápidos aparecem como 0 s. A latência é `agora − timestamp`, não inclui
  subsegundo.
- **Anel de 128**: p50/p95 são das últimas 128 confirmações, não da vida toda.
  No teste de 20 min, a política do 7B envia poucas dezenas, então todas cabem;
  em execuções longas o anel passa a ser uma janela móvel.
- **Latência de backlog**: inclui o tempo offline e a espera dos POSTs
  anteriores do mesmo lote; é exatamente "criação→2xx", não "tempo de rede".
- **Duplicatas**: um registro reenviado após reboot/falha gera nova amostra;
  retries podem puxar o p50/p95 para cima.
- **Boot antigo**: linhas de `bootId` anterior são excluídas do p50/p95
  (`foraBoot`), mesmo com `horaSincronizada:true`, para cumprir o critério 10.
- **`horaSincronizada:false`**: o timestamp de fallback é grande e parecido com
  epoch, mas não é hora real; por isso essas linhas nunca entram na medição.
- **Ajuste de relógio**: se o relógio recuar, `agora < timestamp` vira 0 s;
  não há latência negativa.
- **`'L'`**: zera a medição, mas não zera `g_envio.confirmados`/`tentativas`
  (comportamento herdado do 8A); a linha de latência é independente.
- **Ramo LoRa**: a medição está no caminho `SIMULATION`, onde o gateway local
  devolve o 2xx. No rádio real, o gateway precisaria reportar o ack para o
  firmware; isso continua fora do escopo.

### Próximo

**8C**: implementado na seção seguinte, incluindo o `'u'` (URL inválida), o
protocolo de 20 min e a tabela dos 12 critérios finais.

> Sem commit, merge ou push nesta etapa.

---

## INC 8C — `'u'`, protocolo de 20 min e fechamento dos 12 critérios

### Objetivo

Fechar a camada de envio: adicionar o comando **`'u'`** (URL inválida), deixar
um protocolo reproduzível de **20 min offline**, registrar a evidência possível
no host e preencher a tabela dos **12 critérios finais**, separando o que foi
provado do que ainda depende de execução no hardware/Wokwi.

### Mudanças em `esp32/src/main_alt.cpp` (+20 / −2, sobre o 8B commitado)

| Item | Detalhe |
|---|---|
| `URL_INVALIDA` | `https://url-invalida.invalid/` — domínio `.invalid` (RFC 2606), que não resolve. Não substitui `secrets.h`. |
| `g_urlInvalida` | Estado do comando `'u'`; começa `false`. |
| `postarLinha()` | Escolhe `g_urlInvalida ? URL_INVALIDA : URL_RECEPTOR`; o resto do caminho HTTP/TLS não mudou. |
| `alternarUrlInvalida()` | Alterna a URL e zera `g_proximaTentativaMs`/`g_falhasConsecutivas`, para o teste não ficar preso no backoff anterior. |
| `'u'` no `loop()` | Chama `alternarUrlInvalida()`. |
| `'i'` | Nova linha `urlTeste: normal|invalida`. |

### O que NÃO mudou

Amostragem, histerese, debounce, patamares, pinos, períodos, formato NDJSON,
cursor NVS, ordem do 7B, codec v1, `'x'`/`'f'`, `'d'`, `'L'`, `'o'`, `'e'`,
`'t'`, `SIMULATION` e o ramo LoRa.

### Comando `'u'` — como testar

1. Deixe pelo menos uma linha pendente, o Wi-Fi conectado e confirme que
   `'e'` está em `ok` (a resposta forçada tem precedência sobre a URL).
2. Pressione `'u'`; deve aparecer:
   `ENVIO: URL de teste = invalida (dominio .invalid)`.
3. A próxima tentativa deve falhar no DNS/transporte, sem avançar o cursor:
   `HTTP: falha -1` (ou código negativo equivalente) e
   `ENVIO: ... proxima em ... s`.
4. Pressione `'i'`: `urlTeste: invalida`, `tentativas`/`falhas` incrementados,
   `cursor` inalterado, `pendentes` preservados.
5. Pressione `'u'` de novo: `ENVIO: URL de teste = normal`; o backoff é zerado
   e a próxima tentativa usa a URL real.
6. Repita com `'e'` em `500`, `400` e `timeout` para cobrir os caminhos que o
   DNS inválido não exercita.

O comando usa `.invalid` de propósito: ele testa **DNS/transporte reais** sem
tocar na URL de produção e sem gravar segredo em arquivo versionado. O caminho
TLS continua com `setInsecure()` no Wokwi, como registrado no INC 2.

### Protocolo de 20 min offline

Pré-requisitos: receptor com 2xx disponível (ou o receptor local em
`esp32/test/receiver/`); monitor serial capturado em arquivo; acesso à
exportação de requisições do receptor.

1. **Reset limpo:** pressione `'L'` e reinicie o ESP32. O `'L'` apaga a fila e
   zera `descartados`, `perdidosNaoConfirmados` e a medição de latência; o reset
   gera um boot novo com `seq 0` e zera os demais contadores. Sem o reset, a
   linha de boot some e o `seq` continua de onde parou, deslocando a equação
   `gerados = seq_max + 1`.
2. Espere a linha de boot e o relógio válido (`HORA: fonte=ntp` ou `http`).
   No `'i'`, confirme `urlTeste: normal` e `offline: nao`; se necessário,
   pressione `'u'`/`'o'` para voltar ao modo normal. Anote `t0` e o primeiro
   `'i'`.
3. Pressione `'o'` para ficar **offline**. O arquivo continua crescendo a cada
   snapshot de 5 s e a cada evento.
4. Aguarde **20 min** (240 snapshots previstos, além dos eventos). Não use
   `'x'`, `'f'` nem `'L'` durante a janela.
5. Ao final, pressione `'i'` e registre: `bytes`, `linhas`, `pendentes`,
   `descartados`, `perdidosNaoConfirmados`, `dizimados`, `latencia`.
6. Pressione `'o'` para voltar online e cronometre a drenagem. Considere o fim
   quando `pendentes: 0` e não houver novo `ENVIO: lote`.
7. Exporte as requisições do receptor e agrupe por `(bootId, seq)`:
   - **retidos** = chaves únicas recebidas;
   - **duplicatas** = requisições com a mesma chave;
   - **faltantes** = chaves esperadas que não chegaram.
8. Confira a contabilidade:

```text
gerados (seq_max + 1)
  = retidos únicos
  + pendentes no fim
  + descartados (fila cheia, falha de persistência, compactação)
  + dizimados (snapshots ignorados pela política do 7B)
```

`perdidosNaoConfirmados` é **subconjunto** de `descartados` (só o que foi
removido pela compactação depois do cursor e não estava no conjunto de
resolvidos); não some esse valor de novo. `dizimados` também não é perda: é a
política de backfill que manda 1 snapshot a cada 10.

### Evidência de host — seca de 20 min

Esta verificação agora é automatizada pela suíte versionada em
`esp32/test/host/`. Ela extrai as funções reais do firmware, compila no host e
não espera 20 min reais:

```bash
cd esp32
python3 test/host/run_tests.py
```

A seca monta um arquivo com **241 registros**: 1 boot (`seq 0`), 237 snapshots e
3 eventos (`seq 10/50/100`). As funções de seleção do 7B são **extraídas de
`src/main_alt.cpp`** e executadas com um `File`/`LittleFS` fake; todos os lotes
são confirmados com 2xx.

```text
[drenagem_20min] == 1. 20 min offline, todos os lotes com 2xx ==
OK   normal: 25 registros enviados
OK   normal: 25 chaves unicas
OK   normal: sem duplicatas
OK   normal: 216 snapshots dizimados
OK   normal: cursor final em 240
OK   normal: 6 rodadas
OK   normal: ancora 240 e o primeiro enviado
OK   normal: boot e eventos 10/50/100 enviados
OK   normal: todos os mantidos foram enviados
contabilidade: 241 gerados = 25 enviados + 216 dizimados
== 2. primeiro lote falha (500/timeout), depois recupera ==
OK   falha ocorreu na ancora 240
OK   falha nao avancou cursor nem dizimou nada
OK   retry: 25 registros enviados
OK   retry: 7 rodadas
OK   retry entregou exatamente o mesmo conjunto, sem perda
== 3. perda do cursor depois de drenar: duplicatas por chave ==
OK   reenvio manteve 25 registros e 216 dizimados
OK   as 25 chaves (bootId/seq) identificam as duplicatas
TODOS OS CENARIOS OK
```

Isso cobre a **ordem** (âncora 240 primeiro; boot e eventos do boot corrente
depois; backfill dizimado por último), a **cobertura** dos mantidos e a
**contabilidade** `241 = 25 + 216`, sem duplicatas no caminho normal. Também
simula **falha total no primeiro lote**: o cursor não avança, o mesmo conjunto é
retentado e chega completo. Por fim, perder o cursor depois de uma drenagem
bem-sucedida reproduz 25 reenvios; as chaves `(bootId, seq)` identificam todas
as duplicatas. Nesta seca não houve fila cheia nem compactação, então
`descartados = 0`; a equação geral inclui esse termo e `pendentes`. A seca não
substitui a execução real: não mede rede, tempo de drenagem nem p50/p95.

### Teste de host do `'u'`

O caso abaixo é automatizado por `esp32/test/host/test_url.cpp`, que extrai a
função real do firmware e usa um `Serial` fake:

```text
ENVIO: URL de teste = invalida (dominio .invalid)
OK   primeiro 'u' ativa URL invalida
OK   primeiro 'u' zera a espera
OK   primeiro 'u' zera as falhas
ENVIO: URL de teste = normal
OK   segundo 'u' volta para a URL normal
OK   segundo 'u' zera a espera
OK   segundo 'u' zera as falhas
TODOS OS CENARIOS OK
```

O domínio de teste também foi consultado no host: `url-invalida.invalid` não
resolve (`Name or service not known`), como esperado.

### Suíte automatizada completa

`esp32/test/host/` reúne os testes de lógica executáveis sem placa. O comando
`python3 test/host/run_tests.py --verbose` compila seis binários, roda as
checagens estáticas e imprime cada verificação; a saída resumida é:

```text
[codec] TODOS OS CENARIOS OK
[compactacao] TODOS OS CENARIOS OK
[latencia] TODOS OS CENARIOS OK
[drenagem_20min] TODOS OS CENARIOS OK
[url] TODOS OS CENARIOS OK
[backoff] TODOS OS CENARIOS OK
== checagens estaticas ==
OK   sem chamada delay( no firmware
OK   secrets.h esta no .gitignore
OK   secrets.h nao esta versionado
OK   secrets.example.h esta versionado

Resumo: 7/7 grupos OK
```

A cobertura por arquivo está em `esp32/test/host/README.md`; em resumo: codec
v1, compactação/`'x'`/`'f'`, p50/p95, seca de 20 min com retry, `'u'`, backoff
e as checagens estáticas de segredo/`delay()`. É evidência de **lógica**, não
substitui o ensaio de rede nem mede DNS/TLS do ESP32.

### Build

```text
RAM:   [==        ]  15.5% (used 50740 bytes from 327680 bytes)
Flash: [========  ]  77.0% (used 1008773 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

### Tabela dos 12 critérios finais

| # | Critério | Evidência / status | Limitação ou observação |
|---|---|---|---|
| 1 | Exatamente 1 boot, `seq 0`, lat/lon placeholder, `origem=simulacao`, primeira linha do arquivo | **Atendido com a decisão do INC 1.** A tarefa de amostragem gera um boot no início; `seq=0`; na simulação lat/lon só vão no boot; `origem` é `simulacao`. | Após reboot com fila existente, o boot novo é a primeira linha **gerada neste boot**, não a primeira linha física. Em arquivo novo (após `'L'`), é a primeira física. |
| 2 | `seq` estritamente crescente por boot; buracos só onde houve descarte contabilizado | **Atendido no código.** `seq` é incrementado por registro; falha de `xQueueSend` e falha de persistência incrementam `descartados`; `seq` não é reutilizado. | `descartados` agrega fila + persistência + compactação; não há contador separado por buraco. `dizimados` não cria buraco de `seq` (só não envia). |
| 3 | Toda linha termina em LF, nenhuma truncada, JSON válido | **Atendido no host; falta varredura final no device.** `persistir()` grava linha + LF; `recuperarLinhaParcial()` remove só a cauda; o teste 8A injeta/recupera 81 bytes; as amostras do INC 1 validaram os 3 tipos. | Linha inválida no **meio** do arquivo ainda pode travar a drenagem; o firmware só repara a cauda. Validar o dump com `jq`/Python após o ensaio. |
| 4 | Codec round-trip idêntico campo a campo nos 3 tipos | **Atendido.** Testes de host do 6A/6B e `'t'` no device; codec v1 codifica/decodifica evento, snapshot e boot. | O ramo LoRa não é compilado no Wokwi; o round-trip foi provado no caminho `SIMULATION`/gateway. |
| 5 | 20 min offline: `gerados = retidos + descartados`; retidos chegam; duplicatas identificáveis | **Parcial: política/contabilidade, retry e duplicatas no host; execução de 20 min no device pendente.** `test_drenagem_20min.cpp`: 241 = 25 + 216, 0 duplicatas, âncora 240, boot/eventos e todos os mantidos enviados; falha total no primeiro lote retenta o mesmo conjunto; perda de cursor gera 25 reenvios identificáveis por `(bootId, seq)`. 8A/7A cobrem perdas por compactação e prefixo 2xx. | A igualdade real precisa do termo `dizimados` e de `pendentes`: `gerados = retidos + pendentes + descartados + dizimados`. A cota do receptor continua bloqueando o ensaio de rede. |
| 6 | 5xx/timeout preservam dados e mostram backoff com timestamp | **Atendido no host/testes anteriores; `'u'` agora cobre DNS real.** `'e'` 500/timeout retorna sem confirmar; `agendarNovaTentativa()` mantém o cursor e aplica backoff; `test_backoff.cpp` cobre 500/503, timeout, 429 e Retry-After; cada tentativa loga `ENVIO: lote ... [ms]` e `proximoEnvioEm`. | O log da falha em si não repete `[ms]`; o intervalo é comprovado pela diferença entre os `[ms]` dos lotes. `'u'` no hardware ainda precisa ser observado. |
| 7 | 4xx para o envio, preserva dados e mostra erro | **Atendido.** `'e'` 400; `agendarNovaTentativa()` mantém a fila, o cursor não avança e `ultimoErro` aparece no `'i'`; `test_backoff.cpp` confirma 400 → 5 min e o teste de prefixo do 7A confirma que só o prefixo 2xx avança. | O 400 usa a pausa longa (5 min); para repetir rápido, troque `'e'` ou reinicie. |
| 8 | Compactação nunca passa de 64 KB + uma linha; antigos saem, novos ficam; cursor reancora se preciso | **Atendido no host; observação no device pendente.** `persistir()` só anexa se `size + linha <= 64 KB`; se não couber após compactar, descarta o novo; `compactarFila()` mantém 3/4 (normal: 48 KB; `'f'`: 3/4 do tamanho atual); `test_compactacao.cpp` cobre compactação normal acima de 64 KB, forçada, cursor, 2 linhas, `'x'` e `'f'`; `'x'` pode deixar 64 KB + 81 B; o cursor reancora se a chave sumiu. | A compactação bloqueia o `loop()` durante a cópia; reancorar pode duplicar registros (nunca perder). `'x'` é gancho de teste, não operação normal. |
| 9 | Linha parcial injetada some no próximo boot sem afetar o resto | **Atendido no host; reset no device pendente.** `'x'` grava 81 B sem LF; `recuperarLinhaParcial()` no boot/`persistir` remove só a cauda; `test_compactacao.cpp` confirma bytes e linhas completas preservadas e reparo no `'f'`. | Se o reset demorar, o próximo snapshot repara antes; para testar o caminho de boot, resetar em até 5 s. Arquivo só com a linha injetada é caso-limite. |
| 10 | Latência criação→2xx (p50/p95) só para registros do boot corrente | **Implementado e testado no host; números reais pendentes.** 8B mede `agora_epoch − timestamp_epoch` no 2xx; só entra `horaSincronizada:true` e `bootId == g_bootId`; antigos vão para `foraBoot`; `test_latencia.cpp` valida p50/p95, anel de 128 e filtros; `'i'` imprime p50/p95, amostras e desconhecidas. | Resolução de 1 s; anel das últimas 128 confirmações; duplicatas contam de novo; o p50/p95 do ensaio de 20 min ainda não foi medido. |
| 11 | Nenhum segredo em arquivo versionado | **Atendido e automatizado.** `run_tests.py` confirma via `git check-ignore` que `esp32/src/secrets.h` é ignorado e via `git ls-files` que só `esp32/src/secrets.example.h` é versionado; a URL de teste é `.invalid`, não um segredo. | `secrets.h` existe no disco local e pode estar em backup fora do Git; se vazar, rotacionar Wi-Fi/URL. |
| 12 | Sem mudança na amostragem; sem `delay()` no `loop()` | **Atendido e automatizado.** `run_tests.py` varre o código (ignorando comentários) e não encontra chamada `delay(`; o diff do 8C não toca `tarefaAmostragem`, `lerSensor`, histerese, debounce, patamares, pinos ou períodos; o envio não usa espera bloqueante. | `delayMicroseconds()` em `lerSensor` e `vTaskDelayUntil()` da tarefa de amostragem são pré-existentes e não são `delay()`; o `loop()` segue em busy-wait. |

### Resultado do ensaio de 20 min (preencher no hardware)

A parte de rede **não foi executada neste ambiente**: a cota do Webhook.site
continua esgotada e não há token do Wokwi CLI disponível. A tabela abaixo é o
local exato para colar os números do ensaio; a seca de host acima já valida a
política e a contabilidade.

| Métrica | Valor medido |
|---|---|
| Início/fim do offline | |
| `seq_max` e `gerados = seq_max + 1` | |
| `retidos` únicos no receptor | |
| Duplicatas por `(bootId, seq)` | |
| `pendentes` no fim da drenagem | |
| `descartados` (fila + persistência + compactação) | |
| `perdidosNaoConfirmados` (subconjunto) | |
| `dizimados` (política) | |
| `reancoragens` | |
| Bytes antes/depois da drenagem | |
| Tempo de drenagem (primeiro ao último 2xx) | |
| `p50` / `p95` de latência | |
| `desconhecidas` / `foraBoot` | |
| Conferência da equação | `gerados = retidos + pendentes + descartados + dizimados` |

### Riscos e limitações finais

- **Ensaio de 20 min não executado**: sem receptor 2xx, os critérios 5 e 10
  ficam com evidência de host, não de rede. O procedimento acima é o que falta.
- **`'u'`**: o `.invalid` falha no DNS; não garante exercitar timeout/TLS em
  todos os ambientes. Para timeout, use `'e'`; para TLS real, seria preciso um
  host com certificado válido e `setCACert()` no hardware.
- **Boot após reboot**: a decisão do INC 1 continua valendo; o critério 1
  literalmente ("primeira linha do arquivo") só é verdade em arquivo novo.
- **Contabilidade**: `dizimados` precisa entrar como terceiro termo; sem ele a
  igualdade do critério 5 não fecha. `perdidosNaoConfirmados` é subconjunto de
  `descartados`.
- **Duplicatas**: o conjunto de resolvidos é RAM; após reboot/falha, o cursor
  pode reenviar. Isso é aceitável (duplicata identificável), mas o p50/p95 pode
  ser puxado para cima.
- **Latência**: 1 s de resolução, anel de 128, inclui backlog e espera do lote;
  não é tempo de rede.
- **Arquivo**: linha inválida no meio ainda trava a drenagem; a recuperação é
  só de cauda. A compactação bloqueia o loop e pode reancorar (duplicatas).
- **Ramo LoRa**: o caminho `#else` não foi compilado/testado no Wokwi; o
  gateway real precisa reportar o ack para o firmware.
- **Sem commit nesta etapa**: o 8C já está commitado (`8dbaf76`); a suíte
  `test/host/` está no working tree e não foi commitada, mergeada ou enviada.

### Próximo

O 8C fecha o escopo de implementação e a suíte de host cobre a lógica sem
esperar 20 min. Falta apenas executar o protocolo de 20 min com um receptor 2xx
e colar os números na tabela acima; se você rodar, posso conferir a
contabilidade e fechar o relatório final.

Para não depender da cota do Webhook.site, use o receptor local em
`esp32/test/receiver/`. Ele responde 200, grava as requisições e calcula
únicos/duplicatas por `(bootId, seq)`; o README da pasta traz o passo a passo
com `cloudflared`, o checklist do ensaio e a tabela de onde tirar cada número.

> A suíte de testes está versionada em `esp32/test/host/`.

---

## Refatoração em módulos (pós-8C)

O `main_alt.cpp` chegou a ~1.950 linhas e foi dividido em módulos por
responsabilidade. **Não houve mudança de comportamento**: as funções foram
movidas com o corpo intacto, os nomes públicos foram preservados e os únicos
símbolos novos são três acessores de latência (`latenciaAmostras`,
`latenciaDesconhecidas`, `latenciaForaBoot`) usados pelo comando `'i'`.

`esp32/src/main_alt.cpp` agora contém apenas `setup()` e `loop()`, e todos os
módulos ficam em `esp32/src/modulos/`. O `secrets.example.h` (e o `secrets.h`
local, ignorado pelo git) continua em `esp32/src/`.

| Módulo | Responsabilidade |
|---|---|
| `config.h` | Constantes, pinos, limiares, timeouts, nomes dos campos JSON e include de `../secrets.h`/`../secrets.example.h`. |
| `estado.h/.cpp` | Tipos compartilhados (`Evento`, `ContadoresEnvio`, enums) e globais mutáveis (fila, Wi-Fi, cursor, relógio, boot). |
| `relogio.h/.cpp` | NTP sem bloqueio, fallback pelo header `Date`, timestamps e ISO-8601. |
| `linha.h/.cpp` | Montagem da linha NDJSON (`tipo`, `idSensor`, `seq`, `bootId` etc.). |
| `amostragem.h/.cpp` | Tarefa de amostragem dos T1592 e geração dos eventos da sonda HW-02. |
| `hw02_sonda.h/.cpp` | Driver da sonda de nível HW-02 (ADC, filtro, taxa e detecção de mudança). |
| `codec_quadro.h` | Codec binário do quadro no ar v1, compartilhado pelo firmware e pelo gateway. |
| `armazenamento.h/.cpp` | Gravação no LittleFS, compactação, recuperação de cauda e limpeza da fila. |
| `cursor.h/.cpp` | Cursor `(bootId, seq)` na NVS, leitura de linha e avanço do cursor. |
| `resolvidos.h/.cpp` | Conjunto em RAM das confirmações fora de ordem. |
| `latencia.h/.cpp` | Anel de latência criação→2xx, percentis e contadores auxiliares. |
| `rede.h/.cpp` | Máquina de estados do Wi-Fi e `postarLinha` via HTTPS. |
| `gateway.h/.cpp` | Parsing dos campos JSON, quadro do codec v1, JSON do gateway e autoteste `'t'`. |
| `drenagem.h/.cpp` | Seleção do lote (âncora, boot, eventos, backfill), envio, backoff e comandos `'o'`/`'u'`/`'e'`. |
| `diagnostico.h/.cpp` | Comando `'i'` e log do estado no boot. |

Dependências: `config.h` e `estado.h` são a base; `resolvidos` quebra o ciclo
entre `armazenamento` e `drenagem`; `latencia` expõe acessores em vez de
globais. A extração dos testes de host passou a ler todos os
`src/modulos/*.cpp` (exceto `hw02_sonda.cpp`, copiado inteiro em `sonda.inc`).

**Evidência de host:** `cd esp32 && python3 test/host/run_tests.py` →
`Resumo: 8/8 grupos OK`. A checagem estática de `delay(` agora varre todos os
`.cpp`. Não foi possível rodar um build real do firmware neste ambiente
(sem PlatformIO/Arduino core instalados). Como verificação extra, todos os
`.cpp` passaram por `g++ -std=gnu++11 -Wall -Wextra -fsyntax-only` contra
stubs de Arduino/LittleFS/WiFi/HTTPClient/Preferences, e a linkagem parcial
(`ld -r`) não acusou símbolos duplicados.

**Como estender:** novo comando serial → `main_alt.cpp` (dispatch) + módulo
correspondente em `modulos/`; novo campo persistido →
`modulos/estado.h`/`modulos/estado.cpp`; novo limiar ou nome de campo →
`modulos/config.h`.

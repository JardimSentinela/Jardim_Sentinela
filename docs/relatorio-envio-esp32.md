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

- `pendentes` é derivado do arquivo; sem cursor (INC 4) ele é igual a `linhas`.
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

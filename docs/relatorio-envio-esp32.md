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

---

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

Receptor verificado com o **payload exato** do firmware (POST via `curl`, não pelo
ESP32):

```
HTTP 200
metodo: POST | content-type: application/json
corpo: {"tipo":"boot",...,"horaSincronizada":false,"origem":"simulacao"}
```

> **Pendente:** o POST **a partir do ESP32** não pôde ser observado nesta máquina —
> o Wokwi CLI exige um token de CI (`WOKWI_CLI_TOKEN`) e o token local é de licença,
> não de CI. A simulação no navegador (Wokwi) faz esse teste.

### Como testar no Wokwi

1. Preencha `esp32/src/secrets.h` (SSID `Wokwi-GUEST`, senha vazia, URL do Webhook.site).
2. Build/upload e abra o monitor serial em 115200.
3. Observe `WIFI: conectando...` seguido de `WIFI: conectado em ... ms`.
4. A cada snapshot (5 s) deve aparecer `HTTP: codigo 200, ... bytes`.
5. Confira o corpo no Webhook.site.
6. Para testar o timeout: use uma rede inválida; o log deve mostrar
   `WIFI: timeout de 15 s` e o firmware continua gravando snapshots.


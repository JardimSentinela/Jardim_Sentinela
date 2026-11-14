# Validação — Wokwi e simulador no mesmo endpoint (SW-09)

> **Status:** validação real concluída em **2026-11-14**.
> **Branch:** `feat/API-simulador-intercalável`.
> **Card:** [SW-09] Wokwi e simulador intercambiáveis.
> **Contrato:** [`docs/contrato-telemetria.md`](contrato-telemetria.md) — este
> documento registra a **execução**, não substitui o contrato.
>
> Este arquivo existe para que a validação fique **no repositório**, e não
> apenas no computador de quem rodou. Ele descreve o que foi executado, as
> evidências observadas, como repetir e os empecilhos encontrados.

---

## 1. Objetivo e critério de aceite

O mesmo dado de telemetria deve chegar à API por duas fontes, sem tratamento
especial por origem:

```text
ESP32 no Wokwi  ─┐
                 ├─→  POST /api/leituras  ─→  API  ─→  H2
Simulador Java  ┘
```

Critério adotado:

1. As duas fontes enviam os cinco campos obrigatórios do contrato
   (`idJardim`, `idSensor`, `nivelAgua`, `bateria`, `timestamp`).
2. O mesmo `POST /api/leituras` responde `201` para as duas.
3. As leituras aparecem juntas na consulta do jardim.
4. A API não ramifica por `origem`, `tipo` ou metadados do Wokwi.

O contrato oficial está em [`docs/contrato-telemetria.md`](contrato-telemetria.md)
§1. A ponte `@JsonAlias` mantém os nomes antigos (`jardimId`, `sensorId`,
`nivelCm`) aceitos durante a migração, sem branch por origem no controller.

---

## 2. O que já é garantido por teste (reprodutível)

Estes testes rodam sem Wokwi, sem placa e sem rede externa:

| Arquivo | O que verifica |
|---|---|
| `api-jardim-sentinela/src/test/java/br/edu/cesar/jardimsentinela/TelemetriaIntercambiavelTests.java` | Alterna payload Wokwi e simulador no mesmo endpoint e exige 4 leituras no mesmo jardim, com timestamps preservados; confirma que `origem` não muda o processamento; que o payload antigo é aceito pela ponte; que `timestamp` é opcional; e que inválidos retornam `400`. |
| `api-jardim-sentinela/src/test/java/br/edu/cesar/jardimsentinela/gerador/GeradorDadosJardimTests.java` | O simulador emite `idJardim`/`idSensor`/`nivelAgua`/`bateria`/`timestamp` e não emite os nomes antigos. |
| `esp32/test/host/run_tests.py` | Lógica do firmware: codec, compactação, latência, drenagem de 20 min, URL, backoff, sonda e checagens estáticas. |
| `esp32/test/receiver/test_receber.py` | Autoteste do receptor local. |

Comandos e resultados desta rodada:

```text
$ cd api-jardim-sentinela && sh mvnw test
Tests run: 19, Failures: 0, Errors: 0, Skipped: 0
BUILD SUCCESS

$ cd esp32 && python3 test/host/run_tests.py
Resumo: 8/8 grupos OK

$ cd esp32/test/receiver && python3 test_receber.py
Ran 1 test in 0.547s
OK
```

> **Limite:** os testes provam a API e o contrato, não DNS/TLS do ESP32 nem o
> gateway do Wokwi. A execução real está na seção 3.

---

## 3. Execução real (2026-11-14)

### 3.1 Ambiente

- API local em `http://localhost:8080`, banco H2 em arquivo
  (`jdbc:h2:file:./data/jardimsentinela`).
- Firmware do `esp32/` compilado para `esp32dev` com `-DSIMULATION`, usando
  `URL_RECEPTOR = "http://host.wokwi.internal:8080/api/leituras"` em
  `esp32/src/secrets.h` (ignorado pelo git).
- Wokwi com **Private Gateway** habilitado; o ESP32 recebeu IP `10.13.37.2`
  (o gateway público usa `10.10.0.2`).
- Simulador `GeradorDadosJardim` apontando para a mesma URL local.

### 3.2 Build do firmware

```text
$ cd esp32 && pio run -t clean && pio run
RAM:   [==        ]  15.5% (used 50772 bytes from 327680 bytes)
Flash: [========  ]  77.1% (used 1010789 bytes from 1310720 bytes)
merge_firmware: ... firmware.merged.bin
========================= [SUCCESS] =========================
```

O `firmware.merged.bin` é o arquivo que o Wokwi carrega. **Depois de qualquer
`pio run`, reinicie a simulação** (Stop/Start, não apenas EN/RST); caso
contrário o Wokwi continua rodando o binário anterior.

### 3.3 Evidências observadas

Números no momento da validação (a base cresce a cada execução):

| Origem | Evidência |
|---|---|
| Wokwi (dispositivo real no simulador) | **13 registros novos** nesta rodada; **41 no total** com `sensorId = "jardim"` no H2, contando execuções anteriores |
| Simulador `GeradorDadosJardim` | **11 registros novos** em uma execução de 7 s (mais 1 registro do Wokwi chegou durante o mesmo intervalo) |
| Base total | 92 leituras ao final (eram 59 antes da rodada) |
| Duplicatas por `(jardim, sensor, timestampDispositivo)` | **0** |

Exemplos de registros reais do Wokwi:

```json
{"id":80,"jardimId":"jardim-centro-01","sensorId":"jardim",
 "nivelCm":0.0,"bateria":100,
 "timestampDispositivo":"2026-11-14T19:01:08Z",
 "recebidoEm":"2026-11-14T19:03:16.576205Z"}

{"id":92,"jardimId":"jardim-centro-01","sensorId":"jardim",
 "nivelCm":0.0,"bateria":100,
 "timestampDispositivo":"2026-11-14T19:07:08Z",
 "recebidoEm":"2026-11-14T19:07:14.684986Z"}
```

O `id 92` mostra o fluxo em tempo real: 6 s entre medição e recebimento, depois
de a fila do firmware drenar os registros acumulados.

Exemplo de registro do simulador:

```json
{"id":68,"jardimId":"J01","sensorId":"S01",
 "nivelCm":18.4,"bateria":100,
 "timestampDispositivo":"2026-11-14T19:02:25Z"}
```

O payload do firmware inclui metadados (`tipo`, `seq`, `bootId`,
`horaSincronizada`, `origem`); a API os recebe e ignora, como previsto no
contrato.

### 3.4 Matriz de casos

| Caso | Esperado | Observado |
|---|---|---|
| Wokwi real postando no `host.wokwi.internal` | registrar no H2 | ✅ registros persistidos |
| Payload completo do Wokwi | `201` | ✅ |
| Reenvio do mesmo evento (mesmo jardim/sensor/timestamp) | `200` com o mesmo `id`, sem duplicar | ✅ |
| Payload do simulador Java | `201` | ✅ |
| Payload novo sem `timestamp` | `201`, `timestampDispositivo: null` | ✅ |
| Payload legado (`jardimId`/`sensorId`/`nivelCm`) | `201` pela ponte | ✅ |
| Falta de `idJardim` | `400` | ✅ |
| `nivelAgua` negativo | `400` | ✅ |
| `bateria` fora de 0–100 | `400` | ✅ |
| `timestamp` malformado | `400` | ✅ |
| `GET /api/jardins/{id}/leituras` | lista do jardim | ✅ |
| `GET /api/leituras/{id}` inexistente | `404` | ✅ |
| CORS para origem externa | cabeçalho permitido | ✅ `Access-Control-Allow-Origin: *` |
| Reinício da API | dados preservados e envio retomado | ✅ 85 registros mantidos; base chegou a 92 sem duplicatas |

### 3.5 Evidência serial

Em uma execução anterior, o serial do Wokwi registrou:

```text
HTTP: codigo 201, 209 bytes
ENVIO: lote 1/1 2xx
```

Nesta rodada a confirmação veio pelos registros persistidos, porque a porta
RFC2217 não estava habilitada quando a simulação iniciou — o `esp32/wokwi.toml`
do pull não tinha `rfc2217ServerPort`. A linha foi reativada e o capturador
ficou aguardando; a captura exige reiniciar a simulação para abrir a porta.

---

## 4. Como repetir

1. Suba a API na máquina local:

   ```bash
   cd api-jardim-sentinela
   sh mvnw spring-boot:run
   curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/teste
   ```

2. Crie `esp32/src/secrets.h` (ignorado pelo git):

   ```cpp
   #define WIFI_SSID    "Wokwi-GUEST"
   #define WIFI_SENHA   ""
   #define URL_RECEPTOR "http://host.wokwi.internal:8080/api/leituras"
   ```

3. Compile e confirme que a URL entrou no binário:

   ```bash
   cd esp32
   pio run
   strings -a .pio/build/esp32dev/firmware.merged.bin | grep host.wokwi.internal
   ```

4. No Wokwi, habilite o **Private Gateway**. O IP do ESP32 deve ser
   `10.13.37.2`.

5. **Pare e inicie a simulação** e mantenha a aba visível. O Wokwi carrega o
   `firmware.merged.bin` no início; EN/RST não recarrega o binário.

6. Confira na API:

   ```bash
   curl -s http://localhost:8080/api/leituras | jq 'length'
   curl -s http://localhost:8080/api/leituras \
     | jq '[.[] | select(.sensorId == "jardim")] | length'
   ```

7. Rode o simulador contra o mesmo endpoint:

   ```bash
   cd api-jardim-sentinela
   java -cp target/classes \
     br.edu.cesar.jardimsentinela.gerador.GeradorDadosJardim \
     http://localhost:8080/api/leituras 10
   ```

### Captura serial opcional (RFC2217)

Com `rfc2217ServerPort = 4000` em `esp32/wokwi.toml` e a simulação reiniciada:

```python
import serial
porta = serial.serial_for_url("rfc2217://localhost:4000", 115200, timeout=1)
while True:
    dados = porta.read(4096)
    if dados:
        print(dados.decode("utf-8", "replace"), end="")
```

O Wokwi não cria `/dev/ttyUSB`; sem o RFC2217 o monitor serial do VS Code não
encontra porta. Não abra dois clientes RFC2217 ao mesmo tempo.

---

## 5. Empecilhos encontrados e soluções

### 5.1 Monitor serial não abria no VS Code

**Sintoma:** nenhuma porta serial disponível.

**Causa:** o Wokwi mantém o serial dentro da webview; não expõe
`/dev/ttyUSB`.

**Solução:** `rfc2217ServerPort = 4000` no `wokwi.toml` e um capturador
`pyserial` em `rfc2217://localhost:4000`. É uma conveniência local de
depuração.

### 5.2 A simulação pausava sozinha

**Sintoma:** o serial parava e nenhum POST era feito.

**Causa:** o Wokwi pausa a simulação quando a aba não está visível.

**Solução:** manter a aba do Wokwi visível, de preferência em um split com o
terminal. Não é falha do firmware nem da API.

### 5.3 HTTPS pelo túnel Cloudflare falhava no handshake TLS

**Sintoma:**

```text
[ssl_client.cpp:37] _handle_error(): [start_ssl_client():273]:
(-80) UNKNOWN ERROR CODE (0050)
[WiFiClientSecure.cpp:144] connect(): start_ssl_client: -80
HTTP: falha -1
ENVIO: rede/timeout
```

**Causa:** `-0x0050` é `MBEDTLS_ERR_NET_CONN_RESET`; a conexão era abortada
durante o handshake com o quick tunnel do Cloudflare. Com o timeout antigo, o
erro aparecia ~8 s depois, coincidindo com `HTTP_TIMEOUT_MS = 8000`.

**Tentativa:** subir o timeout para `30000` apenas em `SIMULATION`. O erro
imediato de 8 s deixou de ocorrer, mas o handshake continuou sem concluir e a
API não recebeu registros. **Não resolveu o TLS.**

**Solução:** abandonar o túnel nesta validação e usar o Private Gateway
(seção 5.4).

### 5.4 O caminho local exigia suporte a HTTP no firmware

**Sintoma:** o IP `10.13.37.2` indicava Private Gateway, mas o firmware só
criava `WiFiClientSecure`.

**Causa:** `postarLinha()` sempre usava TLS; uma URL
`http://host.wokwi.internal:8080` falharia no handshake.

**Solução:** escolher o transporte pela URL, sem código especial por origem:

```cpp
bool seguro = destino.startsWith("https://");
bool iniciou = seguro ? http.begin(clienteSeguro, destino)
                      : http.begin(clienteSimples, destino);
```

`https://` continua usando `WiFiClientSecure`; `http://` usa `WiFiClient`
puro. Payload, endpoint e resposta `201` são idênticos.

### 5.5 A URL do quick tunnel era efêmera

**Sintoma:** cada execução do `cloudflared` mudava a URL, exigindo editar
`secrets.h` e recompilar.

**Solução:** no Private Gateway, usar a URL fixa
`http://host.wokwi.internal:8080/api/leituras`. O túnel deixou de ser
necessário para esta validação.

### 5.6 `cloudflared` não estava no `PATH`

**Sintoma:** `cloudflared: comando não encontrado`.

**Solução:** binário em `~/.local/bin/cloudflared`, chamado pelo caminho
completo. O túnel público foi encerrado após a validação; expor a API local
sem autenticação não é seguro.

### 5.7 Ruídos benignos no boot

Estas mensagens **não** indicam falha:

```text
Corrupted dir pair at ...
nvs_open failed: NOT_FOUND
/littlefs/fila.ndjson does not exist, no permits for creation
```

O LittleFS é formatado no primeiro boot e a NVS ainda não tem cursor salvo. O
firmware segue, cria o arquivo e drena normalmente.

### 5.8 Fila acumulada após o reinício da API

Durante o teste de persistência a API ficou alguns segundos fora. O firmware
manteve os registros na fila e retomou os envios sozinho; a base passou de 85
para 92 sem perda nem duplicata. O atraso entre `timestampDispositivo` e
`recebidoEm` reflete a drenagem da fila, não um erro de contrato.

### 5.9 `nivelAgua` em 0,0 no Wokwi

Os snapshots do Wokwi chegaram com `nivelAgua: 0.0`. Isso não afeta a
intercambialidade nem a API, mas indica que a sonda no diagrama não estava
variando. É um ponto de hardware/simulação a conferir, não de telemetria.

---

## 6. Limitações e pendências

- **Idempotência depende de `timestamp`:** sem o campo não há como deduplicar;
  o simulador que não envia `timestamp` pode gerar duplicatas em retransmissão.
- **Metadados ignorados:** `tipo`, `seq`, `bootId`, `horaSincronizada` e
  `origem` chegam, mas não são armazenados nem usados para ramificar.
- **TLS desligado no Wokwi:** `setInsecure()` porque o gateway não entrega a
  cadeia de certificados. No hardware real, trocar por `setCACert()`.
- **Timeout maior só na simulação:** `HTTP_TIMEOUT_MS = 30000` em `SIMULATION`
  e `8000` fora dela.
- **Private Gateway é necessário** para `host.wokwi.internal`; sem ele, usar
  uma URL pública `https://`, com a ressalva do item 5.3.
- **`secrets.h` não é versionado:** cada máquina precisa criar o seu.
- **Formato dos ids ainda não é validado:** o padrão `J01`/`S01` está no
  dicionário de dados, mas a API só exige não vazio até o firmware migrar.
- **Evidência local, não versionada:** o H2 em `api-jardim-sentinela/data/` e
  os números deste documento são um retrato da execução; novas rodadas mudam a
  base.

---

## 7. Arquivos relacionados

| Arquivo | Papel |
|---|---|
| `api-jardim-sentinela/src/main/java/br/edu/cesar/jardimsentinela/dto/LeituraRequest.java` | contrato de entrada e ponte `@JsonAlias` |
| `api-jardim-sentinela/src/main/java/br/edu/cesar/jardimsentinela/controller/LeituraController.java` | `POST /api/leituras` e idempotência |
| `api-jardim-sentinela/src/main/java/br/edu/cesar/jardimsentinela/entity/Leitura.java` | `timestampDispositivo` e `recebidoEm` |
| `api-jardim-sentinela/src/main/java/br/edu/cesar/jardimsentinela/gerador/GeradorDadosJardim.java` | simulador com o mesmo payload |
| `api-jardim-sentinela/src/test/java/br/edu/cesar/jardimsentinela/TelemetriaIntercambiavelTests.java` | prova automatizada da intercalação |
| `esp32/src/modulos/rede.cpp` | transporte escolhido pela URL |
| `esp32/src/modulos/config.h` | timeout por ambiente |
| `esp32/wokwi.toml` | serial RFC2217 para depuração local |
| `docs/contrato-telemetria.md` | contrato oficial (não alterado por esta validação) |

---

## 8. Conclusão

O critério do SW-09 está atendido no comportamento: Wokwi e simulador usam o
mesmo endpoint, o mesmo payload obrigatório e a mesma resposta `201`, sem
ramificação por origem. A prova reprodutível está nos testes da seção 2; a
execução real está registrada na seção 3. As diferenças de transporte
(HTTP local no Wokwi, HTTPS no hardware) não alteram o dado trafegado.

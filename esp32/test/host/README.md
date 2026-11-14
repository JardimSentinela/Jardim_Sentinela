# Testes de host — sonda, envio e store-and-forward

Testes automatizados, sem placa, Wi-Fi ou receptor HTTPS. Eles compilam
**funções reais extraídas de `src/modulos/*.cpp`** e o
**`src/modulos/hw02_sonda.cpp` inteiro** contra stubs de Arduino, LittleFS e
`time()`, e rodam em segundos.

## Como rodar

```bash
cd esp32
python3 test/host/run_tests.py
python3 test/host/run_tests.py --verbose
```

Requisitos: `g++` com C++17 e Python 3.9+. O runner gera os trechos em
`test/host/.build/` (ignorado pelo Git), compila cada binário e executa.

## O que é testado

| Arquivo | Cobertura |
|---|---|
| `test_codec.cpp` | Round-trip do codec v1 para evento, snapshot e boot, gap de `seq`, timestamp absoluto, coordenadas e quadros inválidos. |
| `test_compactacao.cpp` | Compactação forçada e normal (64 KB), cursor, `perdidosNaoConfirmados`, `'x'` (linha truncada), `'f'`, arquivo com 2 linhas e limite de 3/4. |
| `test_latencia.cpp` | p50/p95 por posto mais próximo, anel de 128, filtros `horaSincronizada`/boot corrente, relógio inválido e timestamp futuro. |
| `test_drenagem_20min.cpp` | Seca de 20 min (241 registros) com a política real do 7B: âncora, boot, eventos, backfill dizimado, contabilidade e cursor. Também simula falha total no primeiro lote (500/timeout) e confirma que o mesmo conjunto é retentado sem perda; por fim, perde o cursor após drenar e mostra 25 duplicatas identificáveis por `(bootId, seq)`. |
| `test_url.cpp` | Comando `'u'`: alternância da URL inválida e reset do backoff. |
| `test_backoff.cpp` | Backoff 2/4/8/16/32/60 s, 429 30/60/120/240/300 s, Retry-After, 4xx (5 min), URL vazia, codec inválido e timeout. |
| `test_sonda.cpp` | Sonda HW-02: janela de 500 ms, média de 10 leituras, conversão ADC → cm, `pronta` separado de `mudou` (variação ≥ 1 cm), taxa assinada e regressão de 10 min parado + 2 cm. Também registra a limitação atual de saturação (calibração nominal). |
| `run_tests.py` (estático) | Nenhuma chamada `delay(` em `src/main_alt.cpp` nem em `src/modulos/*.cpp`; `secrets.h` ignorado e não versionado; `secrets.example.h` versionado. |

## Limitações

- É um teste **de lógica**, não de rede: não substitui o ensaio de 20 min no
  hardware nem prova DNS/TLS do ESP32. A seca de 20 min roda em milissegundos.
- As funções são extraídas por nome; se alguma for renomeada, atualize
  `GROUPS` em `run_tests.py`.
- `test_sonda.cpp` inclui o `hw02_sonda.cpp` real e usa o stub
  `stubs/Arduino.h`; ele acessa o estado `static` do módulo para resetar os
  cenários.
- Os stubs reproduzem apenas as APIs usadas pelo firmware (LittleFS, HTTP não
  entra nos testes de host). O build do firmware continua sendo
  `pio run`.

# Receptor local para o ensaio de 20 min

Ferramenta para executar o protocolo de 20 min offline do relatório
`docs/relatorio-envio-esp32.md` **sem depender da cota do Webhook.site**.

O firmware posta uma linha NDJSON por requisição. O `receber.py` responde
**200**, grava o corpo e calcula o que o ensaio precisa:

- requisições totais;
- chaves únicas `(bootId, seq)`;
- duplicatas;
- lacunas por boot (inclui snapshots dizimados, não é perda por si só);
- contagem por tipo, sensor, origem e `horaSincronizada`;
- faixa de nível por sensor.

## Arquivos

| Arquivo | Para que serve |
|---|---|
| `receber.py` | Servidor HTTP e analisador offline (só biblioteca padrão do Python). |
| `recebidas.ndjson` | Uma linha por requisição: metadados + corpo. Gerado em tempo de execução. |
| `recebidas.corpo.ndjson` | Só o corpo NDJSON, na ordem de chegada. É o arquivo para conferir com o firmware. |

Os dois `.ndjson` são ignorados pelo Git.

## Requisitos

- Python 3.9+ (testado com 3.11).
- `cloudflared` para expor o receptor ao Wokwi pela internet pública.
  - Download: <https://github.com/cloudflare/cloudflared/releases>
  - Não precisa de conta para o modo *Quick Tunnel*.
- O firmware usa `WiFiClientSecure` + `setInsecure()`, então **o receptor precisa
  ser alcançado por HTTPS**. O `cloudflared` termina o TLS e encaminha HTTP para
  o `receber.py` local.

> HTTP puro em `host.wokwi.internal` não é usado aqui porque o firmware sempre
> cria um `WiFiClientSecure`; o handshake TLS falharia contra um servidor HTTP.

### Teste rápido do receptor

Antes do ensaio, confirme que a ferramenta funciona:

```bash
python3 test_receber.py
```

Deve terminar com `OK`. Ele sobe o servidor em uma porta efêmera, posta boot +
snapshot + duplicata, confere o resumo e limpa tudo.

## Passo a passo

### 1. Suba o receptor

Em um terminal, na pasta `esp32/test/receiver/`:

```bash
python3 receber.py --port 8000 --token TROQUE-ESTE-TOKEN
```

Ele escuta em `127.0.0.1:8000` e grava os arquivos na pasta atual. Para usar
outra pasta:

```bash
python3 receber.py --port 8000 --token TROQUE-ESTE-TOKEN --pasta ./ensaio
```

### 2. Exponha com HTTPS

Em outro terminal:

```bash
cloudflared tunnel --url http://localhost:8000
```

O `cloudflared` imprime uma URL parecida com:

```text
https://algumas-palavras-aleatorias.trycloudflare.com
```

Essa URL muda a cada execução. Copie a sua.

Alternativas ao `cloudflared`, se ele não estiver disponível:

- `ngrok http 8000` (precisa de conta gratuita; a URL é `https://...ngrok-free.app`);
- `ssh -R 80:localhost:8000 nokey@localhost.run` (sem conta; a URL aparece no
  terminal).

O importante é que a URL pública seja **HTTPS**, porque o firmware usa
`WiFiClientSecure`.

### 3. Configure o firmware

Copie `esp32/src/secrets.example.h` para `esp32/src/secrets.h` (se ainda não
existir) e ajuste:

```cpp
#define WIFI_SSID    "Wokwi-GUEST"
#define WIFI_SENHA   ""
#define URL_RECEPTOR "https://SUA-URL.trycloudflare.com/api/leituras?token=TROQUE-ESTE-TOKEN"
```

O `secrets.h` é ignorado pelo Git; não faça commit dele.

Depois compile e grave normalmente (`pio run -t upload` ou o fluxo do Wokwi).

### 4. Confirme que o receptor está no ar

```bash
curl "https://SUA-URL.trycloudflare.com/stats?token=TROQUE-ESTE-TOKEN"
```

Deve responder um JSON com `"requisicoes": 0`.

### 5. Execute o protocolo de 20 min

No monitor serial do Wokwi:

1. **Limpe a fila:** pressione `'L'`.
2. **Reinicie o ESP32** (botão EN/RST ou pare/rode o Wokwi). Isso é necessário
   para nascer um boot novo com `seq 0` e todos os contadores zerados. Se você
   só pressionar `'L'`, a linha de boot é apagada e o `seq` continua de onde
   parou; a equação `gerados = seq_max + 1` fica deslocada.
3. Espere a linha de boot e o relógio (`HORA: fonte=ntp` ou `http`).
   Pressione `'i'` e confirme:
   - `urlTeste: normal`
   - `offline: nao`
   - `pendentes: 0`
   - `dizimados: 0`, `descartados: 0`, `reancoragens: 0`
4. Zere o receptor para a análise começar limpa:

   ```bash
   curl -X POST "https://SUA-URL.trycloudflare.com/reset?token=TROQUE-ESTE-TOKEN"
   ```

5. Anote `t0` e pressione `'o'` para ficar **offline**.
6. Aguarde **20 min**. No modo `SIMULATION`, o snapshot é de 5 s, então são
   ~240 snapshots + eventos. Não use `'x'`, `'f'` nem `'L'` durante a janela.
7. Ao final, pressione `'i'` e copie **todo** o bloco `--- DIAGNOSTICO ---`:
   `bytes`, `linhas`, `pendentes`, `descartados`, `perdidosNaoConfirmados`,
   `dizimados`, `reancoragens`, `latencia`, `bootId`.
8. Pressione `'o'` para voltar online. Cronometre do primeiro `ENVIO: lote` até
   `pendentes: 0` e nenhum lote novo.
9. Consulte o resumo do receptor:

   ```bash
   curl "https://SUA-URL.trycloudflare.com/stats?token=TROQUE-ESTE-TOKEN"
   ```

   ou analise o arquivo depois:

   ```bash
   python3 receber.py --analisar recebidas.ndjson
   ```

## O que preencher no relatório

Use a tabela "Resultado do ensaio de 20 min" de
`docs/relatorio-envio-esp32.md`.

| Métrica | Onde obter |
|---|---|
| Início/fim do offline | `t0` e `t1` anotados no monitor |
| `seq_max` e `gerados = seq_max + 1` | último `seq` no `/stats` (`boots.<bootId>.seq_max`) |
| `retidos` únicos | `/stats` → `unicas` |
| Duplicatas por `(bootId, seq)` | `/stats` → `duplicadas` e `duplicadas_por_chave` |
| `pendentes` no fim da drenagem | `'i'` |
| `descartados` | `'i'` |
| `perdidosNaoConfirmados` | `'i'` (subconjunto de `descartados`) |
| `dizimados` | `'i'` |
| `reancoragens` | `'i'` |
| Bytes antes/depois | `'i'` antes e depois |
| Tempo de drenagem | primeiro ao último `ENVIO: lote [ms]` |
| `p50` / `p95` | `'i'` → linha `latencia` |
| `desconhecidas` / `foraBoot` | `'i'` → linha `latencia` |
| Conferência | `gerados = retidos + pendentes + descartados + dizimados` |

### Como interpretar

- **`unicas`** é o número de registros diferentes que chegaram.
- **`duplicadas`** é quantas requisições repetiram uma chave já vista. Duplicata
  é aceita; perda não.
- **`lacunas_no_intervalo`** conta os `seq` que não chegaram entre o menor e o
  maior `seq` do boot. Isso inclui os snapshots **dizimados** pela política
  (1 a cada 10) e eventuais descartes. Não trate esse número como perda sem
  cruzar com `'i'`.
- **`invalidas`** deve ser 0. Se não for, alguma linha chegou truncada ou sem
  JSON válido.

## Resultado esperado no caminho normal

O teste de host usa um cenário fixo de **241 registros** (1 boot + 237 snapshots
+ 3 eventos) e projeta:

```text
gerados: 241
retidos: 25 (1 boot + 3 eventos + 21 snapshots mantidos)
dizimados: 216
duplicatas: 0
pendentes: 0
```

No device, o total real depende dos eventos e da sonda: com snapshot de 5 s são
~240 snapshots em 20 min, e cada evento `sensor-04` (variação ≥ 1 cm) entra
**sem ser dizimado**. Não compare o total com 241; confira a equação:

```text
gerados = retidos + pendentes + descartados + dizimados
```

## Problemas comuns

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| `HTTP: falha -1` no serial | URL/túnel fora do ar, TLS ou DNS | Confirme o `curl` na URL; reinicie o `cloudflared`; confira `URL_RECEPTOR` |
| `401` no `curl` | Token ausente/errado | Use `?token=...` igual ao `--token` |
| `404` no `curl` | Rota errada | Use `/stats`, `/export` ou `/reset` |
| `cloudflared` mostra URL nova | Quick Tunnel é efêmero | Atualize `secrets.h` e recompile |
| Receptor não recebe nada | Firmware ainda em `offline` ou `urlTeste: invalida` | Pressione `'o'`/`'u'` e confira `'i'` |
| `recebidas.ndjson` vazio | POST não chegou | Veja o log do `receber.py` e o `HTTP: codigo` no serial |

## Segurança

- O túnel é público. **Sempre use `--token`** e não divulgue a URL.
- O receptor não valida o conteúdo; ele só grava. A validação do contrato é da
  API.
- Ao terminar, pare o `cloudflared` (Ctrl+C) e o `receber.py`.
- `secrets.h`, `recebidas*.ndjson` e `ensaio/` não devem ser commitados.

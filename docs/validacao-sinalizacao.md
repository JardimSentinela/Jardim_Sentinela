# Validação — sinalização do totem no Wokwi (SW-13)

> **Status:** roteiro executado em **2026-11-19** com o Wokwi CLI 0.29.1.
> Resultados preenchidos abaixo. A validação de rede usou um receptor HTTP
> temporário (webhook.site); a API local via Private Gateway não foi testada
> pelo CLI. Os blocos 3 e 4 foram rodados em sessões separadas por causa do
> limite de tempo do CLI.
> **Branch:** `feat/SCRUM-26-estado-totem`. **Card:** [SW-13] Classificar estado físico do jardim.
> **Relatório:** [`docs/relatorio-tarefa-c.md`](relatorio-tarefa-c.md).
>
> Quem escreveu o roteiro não conseguia rodar o Wokwi nem compilar para o ESP32
> (não há PlatformIO no ambiente de desenvolvimento). A lógica foi testada no
> host; este roteiro cobre o que só a simulação mostra: pinos, LEDs e som.

---

## 1. Antes de começar

| # | Item | Resultado |
|---|---|---|
| 1.1 | `pio run` em `esp32/` compila sem erro | OK — SUCCESS; RAM 15,6% (51.276/327.680), Flash 77,9% (1.021.245/1.310.720) |
| 1.2 | `python3 test/host/run_tests.py` em `esp32/` mostra 14/14 grupos OK | OK — 14/14 grupos, 511 checagens, 0 falhas |
| 1.3 | Wokwi abre o `esp32/diagram.json` sem erro de pino (3 LEDs, 3 resistores, 1 buzzer) | OK — simulação subiu; `wokwi-cli lint` só informa o tipo não documentado `wokwi-esp32-devkit-v1` |
| 1.4 | **Conferir o rótulo `RX2`** do amarelo: o `diagram.json` liga o LED amarelo em `esp:RX2` (GPIO16 na placa `wokwi-esp32-devkit-v1`). Se o Wokwi não aceitar esse nome, ajustar o `diagram.json` e **manter** `PINO_LED_AMARELO = 16` em `totem.h` | OK — `expect-pin esp:RX2` acionou o LED amarelo; não foi preciso alterar o `diagram.json` |
| 1.5 | Receptor e URL como em [`docs/validacao-wokwi.md`](validacao-wokwi.md) §4 | **PARCIAL** — HTTP 200 via webhook.site; HTTPS falhou no Wokwi CI (`HTTP: falha -1`, ~30 s); a API local via Private Gateway não foi testada pelo CLI |

## 2. Como ler a simulação

- **Sonda HW-02 = `pot4` (GPIO35).** O nível é `posição × 100 cm`: 22 % do curso ≈ 22 cm.
  A sonda é a fonte do estado (mudança do §8 das decisões, a confirmar pelo grupo).
- **`pot1` a `pot3` (T1592)** só validam o protocolo: seus registros saem **sem** `estado`.
- **Tempo na simulação:** snapshot a cada **5 s**. A tendência usa **5 snapshots**
  (a janela cobre 20 s) e fica `DESATUALIZADO` quando o último snapshot passa de **15 s**.
  Depois de um boot, a tendência é `INSUFICIENTE` por cerca de **20 a 25 s**
  (nesse período o `TRANSBORDANDO` soa: regra provisória, §4).
- **Serial:** a cada mudança aparece uma linha `TOTEM: estado=… tendencia=… som=permitido|calado`.
  O comando `d` mostra a fila; `i` mostra o diagnóstico.
- **Padrão sonoro (provisório):** 3 pulsos de 150 ms a cada 2 s. **Piscar do ALAGADO:** 1 s (500 ms aceso, 500 ms apagado).

## 3. Roteiro

Mova só o `pot4`, salvo onde estiver escrito outro. "Passo de 1 cm a cada 5 s" mantém a
variação acima da tolerância de 1,5 cm dentro da janela.

| # | Ação | Esperado (LED / som / log) | Resultado | Obs. |
|---|---|---|---|---|
| 3.1 | Boot com `pot4` em ~10 % | Verde aceso. Log `estado=NORMAL`. Buzzer calado | OK | Boot em 0 % (e 19 % no passo 3.4) → `NORMAL`; 10 % fica na mesma faixa (0–21 cm). |
| 3.2 | Subir para 22 % | Amarelo aceso, verde apagado. `estado=ENCHENDO`. Calado | OK | 22 cm → `ENCHENDO`; amarelo aceso, verde apagado. |
| 3.3 | Descer para 21 % | **Continua amarelo** (histerese: só volta abaixo de 20 cm) | OK | 21 cm continua `ENCHENDO`/amarelo (histerese). |
| 3.4 | Descer para 19 % | Verde aceso. `estado=NORMAL` | OK | 19 cm → `NORMAL`; verde aceso. |
| 3.5 | Subir até 40 %, e daí +1 cm a cada 5 s até 46 % (≈ 30 s) | Vermelho **fixo** ao passar de 45 cm. Com `tendencia=SUBINDO`: buzzer soa (3 pulsos a cada 2 s). Log `som=permitido` | OK | 40→46 em passos de 1 cm/5 s; em 45/46 → `TRANSBORDANDO`, vermelho fixo, tendência `SUBINDO`, `som=permitido`. |
| 3.6 | Manter 46 % por 25 s | `tendencia=ESTAVEL`: buzzer **cala**, vermelho segue fixo. Log `som=calado` | OK | Após ~26 s em 46 cm → `ESTAVEL`, `som=calado`, vermelho fixo. |
| 3.7 | Descer até 43 % | Continua vermelho (histerese de 2 cm) | OK | 43 cm continua `TRANSBORDANDO`/vermelho (histerese). |
| 3.8 | Descer para 42 % | Amarelo aceso. `estado=ENCHENDO` | OK | 42 cm → `ENCHENDO`/amarelo; vermelho apagado. |
| 3.9 | Subir rápido para 60 % ou mais | **Vermelho piscando** (1 s). `estado=ALAGADO`. Buzzer soa **mesmo com tendência ESTAVEL ou DESCENDO** | OK | 65 cm → `ALAGADO`, `som=permitido`. Pisca de 1 s coberto por `test_sinalizacao`; não amostrado no pino. |
| 3.10 | Descer para 58 % | Continua ALAGADO (histerese). Em 57 %: `estado=TRANSBORDANDO`, vermelho fixo | OK | 58 cm continua `ALAGADO`; 57 cm → `TRANSBORDANDO`/vermelho fixo. |
| 3.11 | Mexer em `pot1`, `pot2` ou `pot3` | Aparecem eventos `sensor-01` a `sensor-03` na fila (`d`), **sem** `estado` nem `tendenciaDispositivo`. O totem **não** muda por causa deles | OK | `pot1` → evento `sensor-01` sem `estado`/`tendenciaDispositivo`/`versaoRegra`; totem permaneceu em `TRANSBORDANDO`. |
| 3.12 | Comando `o` (offline) e repetir 3.2 e 3.9 | LEDs e buzzer **continuam funcionando** (o totem não depende de rede) | OK | Comando `o` → modo offline; `ENCHENDO` e `ALAGADO` continuaram nos LEDs/som. |
| 3.13 | Comando `i` | O diagnóstico lista `rejeitadosVersao` (informativo: 0 no simulador) | OK (informativo) | `rejeitadosVersao: 0` no diagnóstico. **Ressalva:** hoje o contador só é incrementado no `#ifdef SIMULATION`, ao decodificar o quadro que o próprio firmware acabou de codificar; em operação normal ele nunca vê outra versão, então este passo sempre passa e **não valida a rejeição em campo**. O caminho real de recepção (rádio) não existe ainda: o contador está "preparado para o rádio". A rejeição é coberta pelos testes de unidade do codec/gateway. |
| 3.14 | Verificar o JSON recebido (receptor ou H2) de um snapshot e de um evento da sonda | Contém `idJardim: "J01"`, `estado`, `tendenciaDispositivo` e `versaoRegra: "v1-provisoria-2026-11"`. O `seq` não volta a zero | OK | Webhook recebeu snapshot e evento `sensor-04` com `J01` e os campos novos; `seq` 0..5 sem voltar. O nível decimal 42,5 chegava como 43,0 nesta execução (corrigido depois, ver §5). |
| 3.15 | Verificar o JSON de um evento de `sensor-01` e do boot | **Sem** `estado`, `tendenciaDispositivo` e `versaoRegra` | OK | Boot e evento `sensor-01` sem os campos novos. |
| 3.16 | **Receptor fora do ar** (URL do receptor inválida: comando `u`, ou receptor desligado) com `pot4` em ~65 % (ALAGADO) e depois em ~46 % com `SUBINDO` | Durante os POSTs que falham/expiram: vermelho **continua piscando** (ALAGADO) e o buzzer mantém o padrão de pulsos (**nunca contínuo**) | **PENDENTE** | Cenário pedido no review da SW-13. Não executado: exige Wokwi/hardware. |

## 4. Reinício em TRANSBORDANDO (regra provisória de 2026-11-19, a confirmar pelo grupo)

Depois de um boot a tendência é `INSUFICIENTE` até haver 5 snapshots (~4 min no hardware,
~25 s na simulação). Pela regra provisória, nesse período o `TRANSBORDANDO` **soa**, além de
soar com `SUBINDO`. Com `ESTAVEL`, `DESCENDO`, `INDETERMINADO` ou `DESATUALIZADO` ele cala.
O `ALAGADO` soa sempre. Ver `docs/relatorio-tarefa-c.md` §5.2.1.

| # | Ação | Esperado | Resultado |
|---|---|---|---|
| 4.1 | Deixar o `pot4` em ~46 % e reiniciar a simulação | Log `tendencia=INSUFICIENTE som=permitido`; vermelho fixo; **buzzer soa** | OK — equivalente: boot em 0 e `pot4` em 46 → `TRANSBORDANDO`/`INSUFICIENTE`, `som=permitido`; vermelho fixo. |
| 4.2 | Manter 46 % até a janela encher (~25 s) | `tendencia=ESTAVEL`, log `som=calado`; buzzer **cala**. Anotar quanto tempo durou o som | OK — após ~26 s → `ESTAVEL`, `som=calado`; vermelho fixo. O som durou ~26 s. |
| 4.3 | Reiniciar com o `pot4` em ~40 % (ENCHENDO) | Amarelo; buzzer **calado** mesmo com `INSUFICIENTE` | OK — equivalente: boot em 0 e `pot4` em 40 → `ENCHENDO`/`INSUFICIENTE`, `som=calado`; amarelo aceso. |
| 4.4 | Reiniciar com o `pot4` em ~65 % (ALAGADO) | Vermelho piscando; buzzer soa sempre | OK — equivalente: boot em 0 e `pot4` em 65 → `ALAGADO`/`INSUFICIENTE`, `som=permitido`; após 30 s, `ALAGADO`/`ESTAVEL`, `som=permitido`. |

## 5. Observações da execução

- Execução em **2026-11-19** com Wokwi CLI **0.29.1** e PlatformIO.
- Os passos 3 e 4 foram divididos em cenários separados (com `set-control` do `pot4`) por causa do limite de sessão do CLI; cada bloco terminou com `Scenario completed successfully`.
- LEDs estáveis conferidos com `expect-pin` (`D13`, `RX2`, `D14`). O pisca do ALAGADO e o padrão de 3 pulsos do buzzer foram validados por `test_sinalizacao.cpp`; no Wokwi o log confirma `estado=ALAGADO` e `som=permitido`.
- **Rede:** o teste usou `WIFI_SSID="Wokwi-GUEST"` e um receptor HTTP temporário (webhook.site), com `HTTP: codigo 200` e 7 POSTs recebidos. HTTPS para o webhook falhou no Wokwi CI (`HTTP: falha -1`); a API local (`host.wokwi.internal`) exige Private Gateway e não foi testada pelo CLI.
- **Achado (corrigido em 2026-11-20):** o nível decimal era arredondado no caminho do quadro/gateway. Com `pot4` em 42,5 cm, a linha local gravava `"nivelAgua":42.5`, mas o receptor recebia `"nivelAgua":43.0`. Causa: `uint8_t nivelCm` no codec e `(uint8_t)(nivel + 0.5)` no gateway. Correção: o quadro v2 carrega o nível em décimos de cm (`uint16_t`, +1 byte por registro). Regressão coberta em `test_linha.cpp` e `test_codec.cpp`. **Reexecutar o passo 3.14 no Wokwi** com `pot4` em 42,5 cm para confirmar `42.5` no receptor (a correção foi validada apenas nos testes de host).
- **Achado (corrigido em 2026-11-20):** `atualizarTotem()` rodava só no `loop()`, que bloqueia em `http.POST()` (até 30 s na simulação, 8 s no hardware). Com o buzzer dentro de um pulso, o som ficava contínuo durante o bloqueio, e o vermelho do ALAGADO parava de piscar. Correção: tarefa FreeRTOS própria (`iniciarTarefaTotem`, 20 ms, prioridade 2). Uma checagem estática em `run_tests.py` falha se o `loop()` voltar a chamar `atualizarTotem()`. **Não validado no Wokwi/hardware**: ver cenário 3.16.
- O `secrets.h` usado no teste era temporário e foi removido; o firmware foi reconstruído com `secrets.example.h`.

# Validação — sinalização do totem no Wokwi (SW-13)

> **Status:** roteiro pronto, **nenhum passo foi executado**. Todos os resultados
> estão `PENDENTE` e devem ser preenchidos pela Júlia.
> **Branch:** `feat/SCRUM-26-estado-totem`. **Card:** [SW-13] Classificar estado físico do jardim.
> **Relatório:** [`docs/relatorio-tarefa-c.md`](relatorio-tarefa-c.md).
>
> Quem escreveu o roteiro não consegue rodar o Wokwi nem compilar para o ESP32
> (não há PlatformIO no ambiente de desenvolvimento). A lógica foi testada no
> host; este roteiro cobre o que só a simulação mostra: pinos, LEDs e som.

---

## 1. Antes de começar

| # | Item | Resultado |
|---|---|---|
| 1.1 | `pio run` em `esp32/` compila sem erro (o firmware **nunca foi compilado** para o ESP32 nesta branch) | PENDENTE |
| 1.2 | `python3 test/host/run_tests.py` em `esp32/` mostra 14/14 grupos OK | PENDENTE |
| 1.3 | Wokwi abre o `esp32/diagram.json` sem erro de pino (3 LEDs, 3 resistores, 1 buzzer) | PENDENTE |
| 1.4 | **Conferir o rótulo `RX2`** do amarelo: o `diagram.json` liga o LED amarelo em `esp:RX2` (GPIO16 na placa `wokwi-esp32-devkit-v1`). Se o Wokwi não aceitar esse nome, ajustar o `diagram.json` e **manter** `PINO_LED_AMARELO = 16` em `totem.h` | PENDENTE |
| 1.5 | Receptor e URL como em [`docs/validacao-wokwi.md`](validacao-wokwi.md) §4 | PENDENTE |

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
| 3.1 | Boot com `pot4` em ~10 % | Verde aceso. Log `estado=NORMAL`. Buzzer calado | PENDENTE | |
| 3.2 | Subir para 22 % | Amarelo aceso, verde apagado. `estado=ENCHENDO`. Calado | PENDENTE | |
| 3.3 | Descer para 21 % | **Continua amarelo** (histerese: só volta abaixo de 20 cm) | PENDENTE | |
| 3.4 | Descer para 19 % | Verde aceso. `estado=NORMAL` | PENDENTE | |
| 3.5 | Subir até 40 %, e daí +1 cm a cada 5 s até 46 % (≈ 30 s) | Vermelho **fixo** ao passar de 45 cm. Com `tendencia=SUBINDO`: buzzer soa (3 pulsos a cada 2 s). Log `som=permitido` | PENDENTE | |
| 3.6 | Manter 46 % por 25 s | `tendencia=ESTAVEL`: buzzer **cala**, vermelho segue fixo. Log `som=calado` | PENDENTE | |
| 3.7 | Descer até 43 % | Continua vermelho (histerese de 2 cm) | PENDENTE | |
| 3.8 | Descer para 42 % | Amarelo aceso. `estado=ENCHENDO` | PENDENTE | |
| 3.9 | Subir rápido para 60 % ou mais | **Vermelho piscando** (1 s). `estado=ALAGADO`. Buzzer soa **mesmo com tendência ESTAVEL ou DESCENDO** | PENDENTE | |
| 3.10 | Descer para 58 % | Continua ALAGADO (histerese). Em 57 %: `estado=TRANSBORDANDO`, vermelho fixo | PENDENTE | |
| 3.11 | Mexer em `pot1`, `pot2` ou `pot3` | Aparecem eventos `sensor-01` a `sensor-03` na fila (`d`), **sem** `estado` nem `tendenciaDispositivo`. O totem **não** muda por causa deles | PENDENTE | |
| 3.12 | Comando `o` (offline) e repetir 3.2 e 3.9 | LEDs e buzzer **continuam funcionando** (o totem não depende de rede) | PENDENTE | |
| 3.13 | Comando `i` | `rejeitadosVersao: 0` | PENDENTE | |
| 3.14 | Verificar o JSON recebido (receptor ou H2) de um snapshot e de um evento da sonda | Contém `idJardim: "J01"`, `estado`, `tendenciaDispositivo` e `versaoRegra: "v1-provisoria-2026-11"`. O `seq` não volta a zero | PENDENTE | |
| 3.15 | Verificar o JSON de um evento de `sensor-01` e do boot | **Sem** `estado`, `tendenciaDispositivo` e `versaoRegra` | PENDENTE | |

## 4. Reinício em TRANSBORDANDO (regra provisória de 2026-11-19, a confirmar pelo grupo)

Depois de um boot a tendência é `INSUFICIENTE` até haver 5 snapshots (~4 min no hardware,
~25 s na simulação). Pela regra provisória, nesse período o `TRANSBORDANDO` **soa**, além de
soar com `SUBINDO`. Com `ESTAVEL`, `DESCENDO`, `INDETERMINADO` ou `DESATUALIZADO` ele cala.
O `ALAGADO` soa sempre. Ver `docs/relatorio-tarefa-c.md` §5.2.1.

| # | Ação | Esperado | Resultado |
|---|---|---|---|
| 4.1 | Deixar o `pot4` em ~46 % e reiniciar a simulação | Log `tendencia=INSUFICIENTE som=permitido`; vermelho fixo; **buzzer soa** | PENDENTE |
| 4.2 | Manter 46 % até a janela encher (~25 s) | `tendencia=ESTAVEL`, log `som=calado`; buzzer **cala**. Anotar quanto tempo durou o som | PENDENTE |
| 4.3 | Reiniciar com o `pot4` em ~40 % (ENCHENDO) | Amarelo; buzzer **calado** mesmo com `INSUFICIENTE` | PENDENTE |
| 4.4 | Reiniciar com o `pot4` em ~65 % (ALAGADO) | Vermelho piscando; buzzer soa sempre | PENDENTE |

## 5. Quando preencher

Para cada linha, trocar `PENDENTE` por `OK`, `FALHOU` ou `NAO SE APLICA` e anotar o motivo
na coluna **Obs.** Se algo divergir da regra, **não** mudar faixa, tolerância nem padrão
sonoro para o teste passar: registrar e levar ao grupo (todos são provisórios, ver
`docs/decisoes-tecnicas.md` §3, §3.1, §4.2 e §5.2).

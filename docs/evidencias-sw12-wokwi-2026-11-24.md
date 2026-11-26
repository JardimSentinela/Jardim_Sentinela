# Evidências SW-12 — validação no Wokwi (2026-11-24)

Executado por Rafa (RafaCarrilho) com apoio do Claude Desktop. Versionado neste PR (#32).
**Resultado final:** 3.14 = **PASSOU** · 3.16 = **VALIDADO COM RESSALVAS** (validação encerrada por decisão do responsável; cenários não serão repetidos só para melhorar a captura).
Legenda: [OBS] observado na tela por Claude · [LOG] verificado em log serial/API · [REL] relatado por Rafa · [COD] inferido pela leitura do código (não observado).

### Resumo por origem (3.16)
- Observado: vermelho piscando em ALAGADO; vermelho fixo em TRANSBORDANDO; HW-02 e snapshots seguindo no serial.
- Log: `som=permitido`/`som=calado` por estado e tendência; `HTTP: falha -1` e `lote 0/1 2xx (rede/timeout)` após o POST preso.
- Relatado por Rafa: buzzer tocou com ritmo (parecia acelerar); conexão aceita às 02:20:13 no listener (print).
- Inferido pelo código: o totem não depende do `loop()` (tarefa FreeRTOS `iniciarTarefaTotem`, 20 ms; checagem estática em `run_tests.py`); portanto o POST bloqueante não trava LED/buzzer.
- Não verificado: cadência 3 pulsos/2 s e "nunca contínuo" do buzzer; piscar dentro da janela exata dos 30 s; hardware real.

## Ambiente
- Clone isolado Jardim_Sentinela_sw12 (develop, HEAD 5a36b2d, remote JardimSentinela/Jardim_Sentinela)
- Firmware: PlatformIO 6.2.0, env esp32dev, -DSIMULATION, build SUCCESS (firmware.merged.bin)
- Simulador: extensão Wokwi VS Code 4.1.0, Community License, Private IoT Gateway embutido (host.wokwi.internal)
- API: Spring Boot 4.1.1, JDK 25.0.4, H2 em memória, porta 8080 (parada pelo usuário para o 3.16)
- secrets.h com valores de teste (git-ignored, não lido de volta). Nenhum arquivo de código/config alterado.

## 3.14 — Precisão decimal: PASSOU
- [LOG] serial `HW-02 ADC: 1741 | Nivel: 42.52 cm`; API (GET /api/leituras) com `"nivelCm":42.5` (ex.: id 52), sem arredondar para 43.

## 3.16 — Totem sem receptor
API fora do ar (receptor desligado). [LOG] POSTs falham: `ENVIO: lote 0/1 2xx (rede/timeout)`.

### Cenário 65% (ALAGADO)
- [LOG] `TOTEM: estado=ALAGADO tendencia=SUBINDO som=permitido` em 65,20 cm (ADC 2670); depois `tendencia=ESTAVEL som=permitido` em 65,10 cm.
- [OBS] LED vermelho piscando (alternância em capturas de 1 s). Loop e leitura HW-02 não travaram.

### Cenário ~46% (TRANSBORDANDO)
- [LOG] Subida 40,27 → 46,13 cm (ADC 1889), taxa 11,7–23,4 cm/min: `ENCHENDO/SUBINDO som=calado` abaixo de 45 cm;
  `TOTEM: estado=TRANSBORDANDO tendencia=SUBINDO som=permitido` ao passar de 45 cm; depois snapshots `tendenciaDispositivo=ESTAVEL`.
- [LOG] Em 54–57 cm, `TRANSBORDANDO` com `ESTAVEL`/`DESCENDO` registrou `som=calado`.
- [OBS] Vermelho aceso fixo em TRANSBORDANDO (sem piscar), comportamento diferente do ALAGADO.
- [REL] Rafa: "buzzer ativou" durante a subida (ao passar para `som=permitido`); disse que tem ritmo e que o ritmo parecia aumentar. Hipótese (não confirmada): a velocidade da simulação subiu (19%->61%) e o áudio acompanha. Cadência de 3 pulsos/2 s não verificada.
- Desvio do roteiro: a linha `TOTEM ... ESTAVEL som=calado` exatamente em 46 cm não foi localizada no serial (log rolável pequeno); o `som=calado` em ESTAVEL aparece em 54–57 cm.

### Timeout de 30 s (listener que aceita e não responde em 127.0.0.1:8080)
- Preparo: listener PowerShell temporário no lugar da API (sem alterar arquivo nem firmware); eventos gerados movendo o pot4 (54 -> 65 cm); comandos de serial `i`, `u`, `u` (reset do backoff) digitados por Rafa.
- [LOG] Diagnóstico (`i`): pendentes 193, tentativas 66, falhas 13, ultimoErro rede/timeout, offline nao, urlTeste normal.
- [REL] Rafa (print do listener): `02:20:13 conexao aceita, sem resposta`. Unica conexao registrada.
- [LOG] Serial depois da conexão: `ENVIO: lote 1 [...]: snapshot/290`, depois `HTTP: falha -1`, `ENVIO: rede/timeout; falha 1, proxima em 2 s`, `ENVIO: lote 0/1 2xx (rede/timeout)`. Ou seja, o POST ficou sem resposta até estourar o timeout (HTTP_TIMEOUT_MS=30000 no -DSIMULATION), sem 2xx.
- [OBS] Vermelho aceso/piscando em ALAGADO antes e depois; snapshots/HW-02 seguiram saindo no serial (seq 292-296, 04:53:56Z-04:54:16Z).
- NÃO CONFIRMADO: o piscar do vermelho DENTRO da janela exata do bloqueio. A velocidade da simulação variou (19% a 61%) e minhas capturas de 1 s ficaram aliasadas com o período do pisca; nao ha sequência de quadros que mostre alternância durante os 30 s. A garantia de que o totem nao depende do loop() vem do código (tarefa FreeRTOS propria, checagem estatica em run_tests.py), não de observação.

## Pendências reais
1. Timeout 30 s: POST preso e timeout comprovados; falta apenas evidência visual do vermelho piscando DENTRO da janela do bloqueio (repetir com velocidade da simulação fixa/lenta e gravação de tela, se necessário).
2. Buzzer: só relato auditivo + log `som=`; cadência (3 pulsos a cada 2 s) e "nunca contínuo" não verificados.
3. Correções de buzzer/totem validadas apenas em simulador (Wokwi); não há hardware real até agora.
4. Backup `sw12-arquivos-a-commitar.tar` não comparado com develop (Downloads não conectada).
5. Bugs F1/F2/F3 dependem de decisão do grupo.

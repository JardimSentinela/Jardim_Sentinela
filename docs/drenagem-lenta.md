# Drenagem lenta — regra provisória (SCRUM-30 / SW-15)

> **Status: critério experimental.** A regra é provisória e não foi validada com dados de campo.
> Limite e janela podem mudar sem aviso até a revisão da Sprint 3.
> **Versão da regra:** `v1-provisoria-drenagem-2026-11`

## 1. Objetivo

Sinalizar ao operador que o jardim está drenando devagar depois de uma chuva, o que pode indicar necessidade de manutenção (entupimento, dreno obstruído).

## 2. Regra v1

Entrada: leituras válidas do jardim (ver `CalculadoraTendencia.ehValida`), em ordem cronológica (`timestampDispositivo`, com `recebidoEm` como alternativa).

**Quais registros entram** (decisão do minha mesmo, Arthur para discutir com equipe em momemnto oportuno, 20/11):
- **Snapshots periódicos** (heartbeat) e **eventos** entram, tratados da mesma forma: são medições de nível.
- O **registro de boot** não entra: traz nível fictício (0 cm no boot) e a consulta da SW-14 o exclui (`tipo = 'boot'`).
- Eventos ocorrem em horários irregulares (disparados por mudança). A regra não depende de espaçamento regular, porque usa a primeira e a última leitura da janela e divide pelo tempo real entre elas.

1. A **janela** termina na leitura mais recente e tem **60 minutos**.
2. Dentro da janela, a **queda** é `nível(primeira) − nível(última)`.
3. A **taxa** é `queda ÷ minutos entre a primeira e a última leitura`, em cm/min.
4. Resultado:

| Situação | Condição |
|---|---|
| `DRENAGEM_LENTA` | queda > 0 e taxa **< 0,5 cm/min** |
| `DRENAGEM_NORMAL` | queda > 0 e taxa **≥ 0,5 cm/min** (o limite em si é normal) |
| `SEM_QUEDA` | queda ≤ 0 (nível igual ou subindo) |
| `INSUFICIENTE` | menos de 2 leituras válidas na janela, ou todas no mesmo instante |

## 3. Limitações conhecidas

- A regra só olha a diferença entre a primeira e a última leitura da janela. Oscilações no meio são ignoradas.
- Não verifica se a chuva realmente aconteceu. "Após a chuva" ainda não é detectado: a regra vale para qualquer queda lenta.
- Não trata leitura desatualizada. Um jardim sem envio há horas pode aparecer com a última queda antiga. A tendência DESATUALIZADO da SW-14 deve ser consultada junto.
- Depende da resolução do sensor. Quedas menores que 1 cm (resolução do formato binário v1) não são percebidas.

## 4. Pendências

- Decidir se a regra será exposta na API e em qual campo (hoje só existe como função pura, `DetectorDrenagemLenta`).
- Validar limite e janela com dados de campo.
- Definir o critério de "após a chuva" (por exemplo, nível acima de um patamar nas últimas horas).

## 5. Referências

- `api-jardim-sentinela/src/main/java/br/edu/cesar/jardimsentinela/drenagem/DetectorDrenagemLenta.java`
- `api-jardim-sentinela/src/test/java/br/edu/cesar/jardimsentinela/drenagem/DetectorDrenagemLentaTest.java`
- Issue #13 (taxa de subida e alerta, aberta): mesma discussão de janela, limite e histerese, para subida.

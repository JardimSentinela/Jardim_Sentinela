# Roteiro de teste integrado do MVP — [SCRUM-39][SW-19]

> **Versão 1 (26/11/2026), aprovada pela Júlia para entrar em `docs/`.** Ainda **não foi executada
> pelo time**: a seção 6 é só um ensaio preliminar feito numa cópia, com ressalvas.
> **Responsável (lead):** Júlia Tenório. **Participam:** Arthur, Rafael, Júlia, Gabriel, Larissa, Dandara, Adrielle.
> **Dependências:** SW-09, SW-16, SW-17, SW-18.

**Critério de aceite (Jira):** o cenário gera dados no Wokwi/simulador, a API processa,
o banco salva e o dashboard gerado/refinado pelo Design atualiza corretamente.

**Como ler os testes:** cada teste tem um comando, um resultado **objetivo** esperado e um
campo de evidência. "Parece ok" não vale: ou o valor bate, ou é FALHA. Os testes marcados
**(controle)** existem para provar que o teste *consegue* falhar (por exemplo, que o banco
estava vazio antes e que um payload inválido é de fato rejeitado).

Classificação usada nos textos: **[Certo]** executado e visto · **[Provável]** inferência forte ·
**[Dado a verificar]** não executado.

---

## 0. Antes de começar

| Item | Regra |
|---|---|
| Versão testada | `develop` **depois** do merge de #32 (SW-12) e #33 (SW-18). Registrar o SHA (`git rev-parse --short HEAD`). Se o teste for feito antes dos merges, registrar os SHAs dos dois PRs. |
| Banco | **Sempre descartável.** H2 em memória (blocos B e C) ou H2 em arquivo temporário (só o teste B5). Nunca o `./data/jardimsentinela` do projeto. |
| Portas livres | 8080 (API) e 5173 (dashboard). |
| Perfil da API | `simulacao` (a idade máxima da tendência passa de 180 s para **15 s**). |
| Navegador do dashboard | Abrir `http://localhost:5173`, **não** `127.0.0.1:5173` (o CORS só libera `localhost`). |
| Quem executa | Anotar nome, máquina e SO de cada execução. Resultado de Linux **não vale** para Windows e vice-versa. |

Subir a API:

```bash
cd api-jardim-sentinela
SPRING_DATASOURCE_URL='jdbc:h2:mem:sw19;DB_CLOSE_DELAY=-1' SPRING_H2_CONSOLE_ENABLED=false \
SPRING_PROFILES_ACTIVE=simulacao ./mvnw spring-boot:run
```

Subir o dashboard (outro terminal):

```bash
cd frontend && npm ci && VITE_API_BASE_URL=http://localhost:8080 npm run dev
```

> **Atenção ao `bootId`.** A API exige **8 caracteres hexadecimais** (`[0-9A-Fa-f]{8}`).
> Em todos os comandos abaixo o `--boot-id` é passado explicitamente. Ver **BUG-1** (§6).
> Usar `--boot-id=` com 8 hex **diferente a cada execução** (ex.: `0a0a0a01`, `0a0a0a02`).
> **A correção do BUG-1 (PR #34) já está na `develop` desde 26/11/2026 (`4d214c1`).** A partir dela, o
> runner gera um `bootId` válido sozinho e recusa um `--boot-id` fora do formato antes de enviar.
> **Quem estiver numa branch anterior a esse commit** (por exemplo, a do PR #33 sem atualizar com a
> `develop`) **precisa passar um `--boot-id` de 8 hex**: sem ele, ou com UUID, o runner recebe HTTP 400
> em tudo. Passar o `--boot-id` continua útil para reproduzir exatamente a mesma sequência (B4).

---

## Dependência do PR #33 (SW-18)

Em 26/11/2026 o PR #33 ainda **não** estava na `develop`. Sem ele, estes itens **não** fecham, e isso
**não é falha do código da `develop`**: o roteiro cobra comportamentos que o #33 traz.

| Item | O que o #33 traz |
|---|---|
| A3 | os 23 testes novos do frontend (27 → 50) |
| B9 | o parâmetro `incluirBoot` no histórico |
| C3 | o dashboard pedir o histórico com `incluirBoot=false` e limite 100 |
| C4 | o histórico sem o registro de boot |
| C5 | o aviso "Última atualização falhou" mantendo os últimos dados |
| C6 | o 404 de jardim sem leitura tratado como estado vazio, sem banner |

Resultado do ensaio de 26/11: **todos passaram com o #33 mesclado** (o B9 e o C4 com as ressalvas
descritas no relatório) e falharam sem ele (`docs/relatorio-ensaio-sw19-2026-11-26.md`). **Faça o SW-19 depois do merge do #33.**

---

## A. Portão automatizado (precisa passar antes de B e C)

| ID | Comando | Esperado (objetivo) | Evidência |
|---|---|---|---|
| A1 | `cd api-jardim-sentinela && ./mvnw -B -ntp test` | `Failures: 0, Errors: 0`; **≥ 200** testes (204 na `develop` em 26/11; 206 com o PR #33) | cole o resumo `Tests run:` |
| A2 | `cd frontend && npm ci && npm run lint` | exit 0 | exit code |
| A3 | `cd frontend && npm test` | exit 0; **≥ 50** testes em 7 arquivos **(só com o PR #33; sem ele são 27 testes em 6 arquivos)** | resumo do Vitest |
| A4 | `cd frontend && npm run build` | exit 0 (aviso de chunk > 500 kB é conhecido e não reprova) | exit code |
| A5 | `cd esp32 && python3 test/host/run_tests.py --verbose` | `Resumo: 14/14 grupos OK` | linha do resumo |
| A6 | `cd esp32 && python3 test/receiver/test_receber.py` | `OK` | saída |
| A7 | CI do PR/`develop` no GitHub | os 3 jobs verdes **no SHA testado** | link da execução |

Se qualquer A falhar, **para**: não vale rodar B e C sobre uma base quebrada.

---

## B. Simulador → API → banco

Rodar na ordem. Cada `ExecutaCenario` usa:
`java -cp api-jardim-sentinela/target/classes br.edu.cesar.jardimsentinela.gerador.ExecutaCenario ...`
(compilar antes: `./mvnw -B -q package -DskipTests`).

| ID | Passo | Esperado (objetivo) |
|---|---|---|
| **B1 (controle)** | `curl -s localhost:8080/api/jardins` com o banco recém-criado | `[]`. Prova que tudo que aparecer depois veio deste teste. |
| **B2 (controle)** | `curl -s -i -X POST localhost:8080/api/leituras -H 'Content-Type: application/json' -d '{"idJardim":"J01","idSensor":"S01","nivelAgua":-5,"bateria":100}'` e depois `GET /api/jardins` | `HTTP 400` com `PAYLOAD_INVALIDO` e `GET /api/jardins` **continua `[]`**. Prova que a API rejeita e não grava dado inválido. |
| **B3** | `--cenario=CHUVA_MODERADA --jardim=J01 --boot-id=0a0a0a01 --passos=20` | exatamente **21** linhas `HTTP 201` (1 boot + 20 snapshots) e **nenhuma** linha com outro código. |
| B3a | Em **até 15 s** após o B3: `curl -s localhost:8080/api/jardins/J01` | `nivelCm` **igual** ao último `nivelAgua` impresso pelo runner; `totalLeituras` = **21**. |
| B3b | `curl -s localhost:8080/api/jardins/J01/tendencia` (também em até 15 s) | `"tendencia":"SUBINDO"`. Se passar de 15 s vira `DESATUALIZADO`: **não é falha da API**, é a regra de idade do perfil `simulacao`; repetir o B3. |
| **B4 (idempotência)** | Repetir o B3 com **o mesmo** `--boot-id=0a0a0a01 --seed=1 --inicio=<o mesmo>` (usar o `inicio` impresso na 1ª execução) | respostas `HTTP 200` (não 201) e `totalLeituras` **continua 21**. |
| **B5 (persistência)** | Subir a API com `SPRING_DATASOURCE_URL='jdbc:h2:file:/tmp/sw19-teste;DB_CLOSE_ON_EXIT=FALSE'`, repetir o B3, **parar a API**, subir de novo com a mesma URL, `GET /api/jardins/J01` | `totalLeituras` = **21** e `nivelCm` iguais ao de antes de parar. Apagar `/tmp/sw19-teste*` depois. |
| **B6** | `--cenario=CHUVA_INTENSA --jardim=J02 --boot-id=0a0a0a02` | 31 × `HTTP 201`; `nivelCm` do J02 chega a **50.0** e fica. |
| **B7** | `--cenario=SATURACAO --jardim=J04 --boot-id=0a0a0a04` | 31 × `HTTP 201`; `nivelCm` = **50.0**. |
| **B8** | `--cenario=DRENAGEM_LENTA --jardim=J03 --boot-id=0a0a0a03` | **181** × `HTTP 201`; `GET /api/jardins/J03` com `totalLeituras` = **181**. |
| B8a | `./mvnw -B -ntp test -Dtest='DetectorDrenagemLentaTest,DrenagemServiceTests'` | `Failures: 0`. A regra **não tem endpoint** (decisão do Arthur, SW-15): a verificação é pelos testes, **não** pelo dashboard. |
| **B9 (boot fora do histórico)** | `curl -s 'localhost:8080/api/jardins/J01/leituras?limite=200'` e a mesma URL com `&incluirBoot=false` | **Requer o PR #33** (sem ele o parâmetro `incluirBoot` é ignorado e as duas listas são iguais: registrar como **N/E, depende do #33**, não como falha da API). Com o #33: a 1ª lista tem **tantos registros a mais quanto boots** enviados ao jardim (um por `--boot-id` diferente; só o B3 = **1**); na 2ª **nenhum** registro com `nivelCm == 0.0`. Faça o B9 **antes** de publicar outro `--boot-id` no J01. |
| **B10** | `curl -s -o /dev/null -w '%{http_code}' localhost:8080/api/jardins/J99` | `404`. |
| **B11** | `bash scripts/smoke-test-integracao.sh` (banco descartável!) | **69/69** e **2 achados informativos** (F1 e F2). Com `--estrito` o exit é 1 **hoje**, por causa de F1/F2: registrar como "achado em aberto", não como regressão. |

---

## C. Dashboard atualiza (o critério final)

O dashboard precisa mostrar o **mesmo** que a API devolve. Conferir com `curl` ao lado.

| ID | Passo | Esperado (objetivo) |
|---|---|---|
| **C1** | Abrir `http://localhost:5173` depois do bloco B | "Conectado à API Java"; J01–J04 com nível igual ao `nivelCm` da API (J01 50 cm, J02 50 cm, J03 36 cm, J04 50 cm no ensaio de 26/11). |
| **C2 (polling)** | Selecionar J01, publicar `--cenario=CHUVA_MODERADA --jardim=J01 --boot-id=0a0a0a11 --passos=10` **sem recarregar a página**; esperar ≤ 10 s. **Atenção ao horário:** os cenários sem `--inicio` publicam timestamps até ~1 min **à frente**, e uma rodada nova logo depois, no mesmo jardim, **fica escondida** (medido em 26/11: `nivelCm` 29,9 antes e depois). Publique os dados-base do bloco C com `--inicio=$(date -u -d '5 minutes ago' +%Y-%m-%dT%H:%M:%SZ)` ou espere ~1 min antes do C2 | o card de J01 muda para o novo `nivelCm` e "Última atualização" avança. Se não mudar: FALHA. |
| **C3** | Aba Network: a chamada do histórico | `GET /api/jardins/J01/leituras?limite=100&incluirBoot=false` com status **200**. **Requer o PR #33** (sem ele a chamada é `?limite=30`, sem `incluirBoot`). |
| **C4 (sem boot)** | **Use um jardim com poucas leituras** (menos que a janela de 20 do gráfico). Depois do C6, publique no J04: `--cenario=SATURACAO --jardim=J04 --boot-id=0a0a0a04 --passos=5 --inicio=<5 min atrás>` (1 boot + 5 snapshots), selecione J04 e clique em "Baixar Histórico" | **Requer o PR #33.** CSV com **5 linhas** e **nenhuma** com nível 0; **nenhuma** barra em 0 cm. Sem o #33 o CSV tem **6 linhas, uma com 0** (isso prova que o teste consegue falhar). Com um jardim de mais de 20 leituras o boot, que é o mais antigo, sai da janela e o teste passa sem provar nada. |
| **C5 (API fora)** | Parar a API, esperar 10 s, olhar a tela | os últimos dados **continuam** com o aviso "Última atualização falhou". Subir a API e clicar em "Tentar novamente": aviso some **sem recarregar a página**. **O aviso requer o PR #33**: sem ele os dados ficam, mas não há aviso. |
| **C6 (estado vazio)** | Com o banco novo (sem dados de J04), selecionar J04 | "Sem leituras recebidas" e **nenhum** banner vermelho. **Requer o PR #33** (sem ele aparece um banner de erro no detalhe). |
| C7 | Console do navegador durante C1–C6 | **zero** erros, **exceto** os `404` que o navegador registra para jardins sem leitura (o 404 é o contrato, ver C6). Qualquer outro erro reprova. |
| C8 | Alternar J01 → J02 → J01 três vezes rápido | o gráfico final é o do J01 (nunca mostra dado do J02). |

---

## D. Wokwi → API (recomendado; o critério diz "Wokwi/simulador")

O critério do Jira aceita **simulador**, então B e C já o cobrem. O Wokwi entra para fechar o
caminho do firmware. A evidência de 24/11 (PR #32, `docs/evidencias-sw12-wokwi-2026-11-24.md`)
vale como **histórica**, não como execução do SW-19. Se houver tempo, repetir:

| ID | Passo | Esperado |
|---|---|---|
| D1 | `pot4` em 42,5 cm, firmware com `URL_RECEPTOR` apontando para a API local | POST `201`; `GET /api/jardins/J01` com `nivelCm` **42.5** (e **não** 43.0), `seq` crescente. |
| D2 | API parada com `pot4` > 65 % | serial com `estado=ALAGADO`; vermelho piscando; sem som contínuo. |
| D3 | API de volta | retransmissão: `200` para o que já existia, **sem duplicar** (`totalLeituras` não cresce além do enviado). |

Ressalvas já conhecidas do 3.16: buzzer sem medição objetiva e só simulador (nunca hardware real).

---

## E. Divergências conhecidas — registrar, não reprovar

Decisões **ainda não tomadas** pelo grupo (Júlia, 26/11). Estes itens **não reprovam** o SW-19,
mas precisam estar escritos no relatório como "em aberto":

| ID | Situação |
|---|---|
| F1 | Reinício com horário não sincronizado pode mostrar leitura de boot antigo. |
| F2 | `boot` (0 cm) pode virar "leitura atual" no detalhe e na lista. |
| F3 | Limiares do firmware (22/45/60) × frontend (15/30/45). **Visível no ensaio:** J01 com 50 cm aparece "Alagado" no dashboard; o firmware classificaria 50 cm como TRANSBORDANDO (ALAGADO só a partir de 60 cm). O simulador nunca envia `estado`, então o dashboard usa o cálculo local. |
| Issue #29 | Se a API aceita só snapshots ou também eventos T1592. Afeta a regra de drenagem, que hoje só existe no backend. |

---

## F. Registro de evidências (uma linha por teste executado)

| ID | Data/hora | Quem | SHA | SO/máquina | Resultado (PASSOU / FALHOU / N/E) | Valor obtido (cole o dado) | Link/arquivo |
|---|---|---|---|---|---|---|---|
| A1 | | | | | | | |
| … | | | | | | | |

**Aceite do SW-19:** todos os A, B e C **PASSOU**; D executado **ou** justificado por escrito;
E registrado; qualquer FALHA vira bug com dono antes de o ticket ser movido para Concluído.

---

## 6. Ensaio preliminar (26/11/2026) — NÃO é a execução oficial

> **Versão posterior e mais completa:** `docs/relatorio-ensaio-sw19-2026-11-26.md` (ensaio na `develop`
> atual e na `develop` + #33). Esta seção é a **primeira versão**, feita antes dos merges de #32, #34,
> #35 e #37, e fica como histórico.

Eu (assistente) rodei B e C em **uma cópia** do repositório fora do projeto, com `develop`
(`5535953`) + PR #33 + PR #32 mesclados localmente (os dois mesclam sem conflito juntos).
Linux, H2 em memória, Chromium headless. **Não foi usado Wokwi nem hardware.**
**Isto não substitui a execução do time.** Serve para mostrar que os comandos acima funcionam e
para listar o que apareceu:

**[Certo] passou**
- A1–A6 na combinação #32+#33: API **200/200**, frontend **50/50**, lint e build OK, firmware **14/14**, receptor OK. (Na `develop` pura: API 198, frontend 27.)
- B1, B6–B10: com `bootId` de 8 hex, J01 61×201, J02 31×201, J03 181×201, J04 31×201; 313 leituras persistidas no total; histórico com 70 leituras (2 boots) e 68 com `incluirBoot=false`; J99 → 404.
- B3 equivalente (20 passos, J05): `SUBINDO`, `nivelCm` 29,9, `totalLeituras` 21.
- C1, C3, C7: dashboard carregou J01–J04 com os mesmos níveis da API, chamadas `200` com `incluirBoot=false`, **zero** erros de console ou rede.

**[Dado a verificar] não executado no ensaio:** B2, B4, B5, B11, C2, C4–C6, C8, todo o bloco D.

**Problemas que apareceram** (situação atualizada em 26/11/2026, depois do merge do #34):

| ID | Gravidade | O que aconteceu |
|---|---|---|
| **BUG-1** | **Alta** (bloqueava o SW-16). **Corrigido: PR #34 mergeado em 26/11/2026 (`4d214c1`)**, com o teste de regressão `ExecutaCenarioBootIdTests` | `ExecutaCenario` gerava por padrão `bootId = UUID` (36 caracteres). A API exige 8 hex (`LeituraRequest`: `[0-9A-Fa-f]{8}`) e respondia **400 em 100% dos envios**; nada é gravado. Reproduzido nos 4 cenários. O exemplo da própria `docs/cenarios-controlados.md` (`--boot-id=cenario-fixado-01`) também dá 400. Com `--boot-id=a1b2c3d4` dá 201. **Por que o CI não pegou [Certo]:** o único teste do gerador de cenários (`SequenciaDeterministicaTests`) usa `bootId = "boot-teste"` e só olha o JSON gerado; nenhum teste envia a sequência para o `POST /api/leituras`. |
| DOC-1 | Média (**correção no PR #37, aberto**) | `docs/cenarios-controlados.md` dizia que `CHUVA_MODERADA` dá `SUBINDO`. Com os 60 passos padrão o nível bate em 50 cm por volta do passo 40 e a janela final (5 leituras) fica plana: a tendência termina `ESTAVEL`. Com 20 passos dá `SUBINDO`. |
| QUIRK-1 | Baixa (**registrado como limitação no PR #37, aberto**) | `DRENAGEM_LENTA` publica 180 registros com timestamps ao longo de ~60 min **a partir de agora**, ou seja, no futuro. O dashboard mostrou "Última atualização 20:24" às 19:28. |
| QUIRK-2 | Baixa | No perfil `simulacao`, a tendência vira "Sem dados recentes" depois de 15 s sem leitura nova (esperado pela regra, mas confunde numa demonstração): publicar e olhar logo em seguida. |
| ATENÇÃO | — | `GET /api/jardins` (lista) **não traz** `tendencia`; só `GET /api/jardins/{id}` traz. Confirmar se o dashboard da rede deveria depender disso. |

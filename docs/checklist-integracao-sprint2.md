# Checklist de integração — Sprint 2 ([SCRUM-21] [SW-12])

> **Lead:** Rafael Carrilho. **Participam:** Arthur Vitorino, Julia Tenorio,
> Gabriel Rios, Larissa, Dandara, Adrielly.
> **Critério de aceite do card:** Wokwi e simulador alimentam a API; dados
> persistem; endpoints respondem; a interface gerada consegue consumir dados
> de teste.
> **Dependências:** SW-06, SW-07, SW-08, SW-09, SW-11, DS-02.

Este documento mapeia as 5 subtarefas sugeridas do card ao estado real do
código em `develop` (analisado via Jira + repositório GitHub, sem acesso ao
computador local de ninguém — qualquer item marcado **[Dado a verificar]**
precisa de confirmação rodando de verdade).

**Escopo:** SCRUM-38 (SW-06) fica de fora desta entrega de propósito —
é task do Arthur, que já sinalizou que pode estar pronta (possivelmente feita
por outra pessoa do time) e pediu pra ninguém mexer em `develop` antes dele
confirmar. O código já existe (ver item 5 abaixo), mas a confirmação e
qualquer ajuste ficam com ele.

## 0. Fluxo TDD aplicado nesta entrega

A partir desta rodada, os testes foram escritos **antes** da implementação
(ciclo vermelho → verde), pra quem for aplicar os commits poder reproduzir o
ciclo: rodar `./mvnw test` só com o teste (vê falhar), depois aplicar o código
(vê passar). Dois arquivos de teste novos:

- `LeituraIngestaoTests.java` — cobre SW-09 (`@JsonAlias`) e SW-10
  (idempotência + erro padronizado). Sem a implementação, a 2ª e a 3ª
  asserção de cada teste devem falhar (DTO antigo rejeita os nomes do
  firmware; sem `TratadorDeErros`, o corpo do erro não tem o formato
  esperado).
- `WebConfigCorsTests.java` (novo) — cobre SW-11 (CORS). Sem `WebConfig.java`,
  a API não manda nenhum cabeçalho `Access-Control-*`, então as duas
  asserções de `header().exists(...)` devem falhar.

**Atualização:** Já rodei. Os 12 testes automatizados (`LeituraIngestaoTests`,
`WebConfigCorsTests`) passam (BUILD SUCCESS). O smoke test ponta a ponta
também rodou contra a API real; 1 checagem falhou por bug no próprio script
(comparação truncada contra JSON com ordem de campo alfabética), corrigido
nesta mesma entrega — não era um problema na API.

## 1. Teste ponta a ponta parcial

**Script pronto:** `scripts/smoke-test-integracao.sh` (requer a API rodando
em `localhost:8080`). Cobre: API de pé, ingestão pelos dois vocabulários,
idempotência, consulta de estado atual, histórico, erro de validação, 404,
e cabeçalho CORS presente.

```bash
cd api-jardim-sentinela
./mvnw test                               # confirma os testes automatizados
./mvnw spring-boot:run &                  # sobe a API
../scripts/smoke-test-integracao.sh    # roda o smoke test
```

**Atualização:** Rodado contra a API real (H2 local), 9 etapas, todas
passando após a correção do script (ver seção 0).

## 2. Correções de contrato

Feito nesta entrega (ver `docs/contrato-telemetria.md`, seção "Resolução
parcial"): `LeituraRequest` aceita os dois vocabulários (`jardimId`/`nivelCm`
do simulador e `idJardim`/`nivelAgua`/`timestamp` do firmware atual) no
mesmo endpoint, via `@JsonAlias`. Isso fecha SW-09 sem esperar a equipe
decidir o nome oficial.

**Ainda em aberto (decisão da equipe, não é código):**
- Nome final do contrato (ver "O que isso NÃO resolve" no doc).
- `bootId`/`seq` do formato NDJSON mais rico só existem na branch
  `feat/SCRUM-34-HW-03-...`, que ainda não foi mergeada em `develop` — o
  card **HW-04** ([SCRUM-36], status "Validação" no Jira) é quem traz isso.
  Até lá, a idempotência usa `timestamp`.

## 3. Validar exemplos JSON

Exemplos reais (não inventados — vêm dos testes automatizados e do código
de `persistir()` no firmware), com os dois vocabulários, estão em
`docs/api-dashboard.md` e nos testes `LeituraIngestaoTests`. Resumo:

| Origem | Exemplo | Endpoint | Resultado |
|---|---|---|---|
| Simulador | `{"jardimId":"J01","sensorId":"S01","nivelCm":18.5,"bateria":97}` | `POST /api/leituras` | 201 |
| Firmware (formato de `main_alt.cpp`, ids no padrão novo) | `{"idJardim":"J01","idSensor":"S02","nivelAgua":10.0,"bateria":100,"timestamp":"2026-10-15T00:00:01Z"}` | `POST /api/leituras` | 201 |
| Qualquer um, campo faltando | `{"sensorId":"S01","nivelCm":1,"bateria":1}` | `POST /api/leituras` | 400, `PAYLOAD_INVALIDO` |

## 4. Testar consumo inicial pelo código gerado

**Bloqueado:** DS-02 ("Finalizar dashboard e gerar código-base") está "Em
andamento" com a Adrielly — ainda não existe código gerado pra testar
consumindo a API de verdade. O que dá pra adiantar sem o código gerado
existir:

- `docs/api-dashboard.md` tem os 3 formatos de resposta reais (estado atual,
  histórico, erro) prontos pra virar mock (MSW/json-server) — a Adrielly ou
  quem for integrar o front pode trabalhar contra o mock sem esperar o
  backend local subir.
- CORS já configurado (`WebConfig.java`) pra quando o código gerado rodar
  num host diferente da API.

Quando o código gerado existir, falta: apontar a `baseUrl` dele pra
`localhost:8080` (ou onde a API estiver rodando) e repetir as chamadas do
`api-dashboard.md` a partir do próprio código gerado, não só via `curl`.

## 5. Checklist de integração (resumo executivo)

| Item | Card Jira | Status real no código | Status no Jira | Observação |
|---|---|---|---|---|
| Contrato definido | SCRUM-19 | Proposta existe, documentada | Concluído | — |
| Validação de payload | SCRUM-22 | Implementada (`LeituraRequest`) | Concluído | — |
| Persistência | SCRUM-23 | Implementada (JPA/H2) | Concluído | — |
| Tratamento de erro padronizado | SCRUM-44 | **Só em `main`, faltava em `develop`** | A fazer | Trazido nesta entrega |
| Consultar estado atual | SCRUM-38 | **Já implementado e testado** | A fazer | Deixado fora desta entrega — é do Arthur, que já está de olho nisso |
| Consultar histórico | SCRUM-40 | Implementado e testado | Concluído | — |
| Simular múltiplos jardins | SCRUM-42 | **Já implementado** (J01–J04 no `GeradorDadosJardim`) | A fazer | Jira parece desatualizado — confirmar com Julia |
| Wokwi/simulador intercambiáveis | SCRUM-43 | Resolvido nesta entrega (ponte `@JsonAlias`) | A fazer | — |
| API pronta pro dashboard | SCRUM-20 | CORS + docs entregues nesta entrega | A fazer | Falta testar com código gerado de verdade (bloqueado por DS-02) |
| Dashboard gerado | SCRUM-24 | Em andamento (Adrielly) | Em andamento | Bloqueia o item 4 acima |
| Integração hardware (NTP/offline/backoff/LoRa) | SCRUM-34/36 | Implementado numa branch separada, não mergeada | Concluído/Validação | Não mexi nisso — fora do escopo "só software" de hoje |

**Para a reunião com a equipe, os pontos que precisam de decisão humana (não
são meus pra decidir):**
1. SCRUM-38 e SCRUM-42 parecem prontos no código mas abertos no Jira —
   alguém confirma e atualiza o board?
2. Nome final do campo de nível/id (ver contrato) — ou seguir com a ponte de
   aliases indefinidamente, que também é uma opção válida.
3. `main` tem um commit (PR#11, Arthur) que nunca chegou em `develop` — o
   fluxo do time é mergear em `develop` e só promover pra `main` "em última
   instância"; isso inverteu. Só percebi o `TratadorDeErros.java` porque
   fui atrás; pode haver mais coisa de main p/ conferir.

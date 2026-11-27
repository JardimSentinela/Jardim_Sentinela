# Ajustes da SW-18 — status da aplicação

> **Referência:** branch `feature/SW-18-detalhe-historico`.
> Revisão inicial: commit `81b0167` (“feat(SW-18): integrar detalhe e histórico
> dos jardins com a API”). Nota de revisão original: `cb3af9c`.
>
> Este documento registra o que foi corrigido depois da revisão do card SW-18.
> Os itens abaixo substituem a lista de pendências original.

## Resumo

| Item | Prioridade original | Status | Onde |
|---|---|---|---|
| Preservar os últimos dados válidos em falha transitória | P1 | Aplicado | `frontend/src/hooks/useJardimData.ts` |
| Tratar 404 como estado vazio | P1 | Aplicado | `useJardimData.ts`, `App.tsx`, `StatusCard.tsx` |
| Separar erro de detalhe e erro de histórico | P2 | Aplicado | `useJardimData.ts`, `App.tsx`, `StatusCard.tsx`, `HistoricoChart.tsx` |
| Corrigir a janela “Todas as leituras” / limite de 30 | P2 | Aplicado | `App.tsx`, `HistoricoChart.tsx` |
| Excluir registros `boot` do histórico do dashboard | P2 | Aplicado | API (`incluirBoot`), `api.ts`, testes |
| Completar os testes | P3 | Aplicado | `*.test.tsx`, `JardimControllerTests.java` |
| Atualizar a documentação | P3 | Aplicado | `INTEGRACAO-API.md`, `docs/api-dashboard.md`, este arquivo |
| Evitar corrida ao alternar J01 → J02 → J01 | P3 | Aplicado | `useJardimData.ts` |

## 1. Falha transitória não apaga mais os dados

### Problema

O hook limpava `jardim` e `historico` em qualquer rejeição. Uma falha de polling
(a cada 5 s) apagava os cards e o gráfico, apesar de o comentário do hook dizer
que ele preservava o último dado válido.

### O que foi feito

- `useJardimData` não chama mais `setJardim(null)` nem `setHistorico([])` nos
  caminhos de rejeição. O último dado válido permanece na tela.
- A limpeza continua apenas no `useEffect` de troca de jardim.
- O hook expõe `erroJardim` e `erroHistorico` para a interface avisar que a
  atualização falhou sem esconder o dado anterior.
- `StatusCard` mostra o aviso “Última atualização falhou; exibindo o dado
  anterior.” com ação de tentar novamente.
- Um contador de requisição (`requisicaoRef`) invalida respostas antigas. Assim,
  alternar J01 → J02 → J01 não deixa uma resposta atrasada de J01 sobrescrever a
  consulta mais recente.

### Validação

- Teste de sucesso → falha do histórico → mantém jardim e histórico.
- Teste de sucesso → falha total → mantém jardim e histórico.
- Teste de corrida J01 → J02 → J01 com respostas fora de ordem.

## 2. 404 é estado vazio, não erro

### Problema

O contrato de `docs/api-dashboard.md` define 404 como “jardim cadastrado, mas
ainda sem leitura”. O detalhe tratava esse caso como falha: banner vermelho,
“Jardim indisponível” e mensagem de API indisponível.

### O que foi feito

- O hook reconhece `ApiError` com status 404 e expõe `semLeituras`.
- `erroJardim` e `erroHistorico` ficam `null` no 404.
- `StatusCard` mostra **“Sem leituras recebidas”** e o texto “Este jardim ainda
  não enviou leituras para a API.”
- `DadosJardimCard` mantém “Aguardando a primeira leitura do sensor.”
- O gráfico mantém o estado vazio “Sem leituras para exibir”.
- A visão de rede já tratava 404 como “Sem leituras recebidas” e continua igual.

### Validação

- Teste do hook: 404 no detalhe e no histórico → `semLeituras`, sem erros.
- Teste da `App`: selecionar um jardim 404 não mostra “Jardim indisponível” nem
  banner vermelho.
- Teste de backend: jardim que só tem boot responde lista vazia quando
  `incluirBoot=false` (não 404).

## 3. Erros de detalhe e de histórico separados

### Problema

Um único `error` era usado no badge de conexão, no banner e no gráfico. Uma
falha no histórico podia esconder o status do jardim, e uma falha no detalhe
podia esconder o histórico já carregado. Em falha total, a tela repetia a mesma
informação em três lugares.

### O que foi feito

- O hook retorna `erroJardim` e `erroHistorico` separadamente.
- `StatusCard` recebe `erroJardim`; `HistoricoChart` recebe `erroHistorico`.
- O banner global de erro do jardim foi removido. Cada componente mostra o erro
  no seu contexto, com “Tentar novamente”.
- O badge de conexão acompanha o detalhe (`erroJardim`), não o histórico.
- Falha só no detalhe: o histórico já carregado continua visível.
- Falha só no histórico: o status e os dados do jardim continuam visíveis.

### Validação

- Teste do hook para falha só no detalhe e falha só no histórico.
- Teste da `App`: detalhe falha e histórico aparece; histórico falha e o status
  do jardim continua na tela.
- Teste da `App` em falha total confirma que o banner duplicado não existe mais.

## 4. Janela “Todas as leituras” e limite

### Problema

O dashboard carregava 30 leituras, mas o menu oferecia “Todas as leituras” e o
cabeçalho dizia “X leitura(s) recebida(s) da API”, dando a entender que o total
era aquele.

### O que foi feito

- O dashboard passou a carregar até **100** leituras (o padrão da API).
- A opção passou a se chamar **“Todas as leituras carregadas”**.
- O cabeçalho diferencia exibição de total carregado:
  - janela de 10/20: “X de Y leitura(s) carregada(s)”;
  - todas: “X leitura(s) recebida(s) da API”.
- O eixo Y acompanha leituras acima de 50 cm (`calcularDominioEixoY`), sem
  cortar barras.

### Validação

- Teste com 35 leituras: janela de 20 mostra “20 de 35”; “Todas” mostra 35.
- Teste do domínio do eixo Y: 50 → 50, 50,2 → 51, 63 → 63.
- Teste do CSV: leituras exportadas da mais antiga para a mais recente.

## 5. Boot fora do histórico do dashboard

### Problema

`findHistorico` incluía registros `tipo: "boot"`. O gerador grava `nivel = 0.0`
para o boot (`SequenciaDeterministica.java`), então o gráfico desenhava uma queda
falsa e o CSV exportava esse zero. O campo `tipo` é `@JsonIgnore`, então o
frontend não conseguia filtrar sozinho.

### O que foi feito

- `LeituraRepository` ganhou `findHistoricoSemBoot`, com a mesma ordenação
  cronológica e a mesma elegibilidade usada pela tendência.
- `JardimService.buscarHistorico` recebeu `incluirBoot`:
  - `true` (padrão) → mantém o contrato anterior e inclui boot;
  - `false` → usa `findHistoricoSemBoot`.
- `JardimController` aceita `incluirBoot` como query param opcional.
- O dashboard chama `fetchHistorico(id, limite, false)`.
- `docs/api-dashboard.md` documenta o parâmetro.

### Decisão sobre o estado atual

`buscarJardim` (detalhe) e `listarJardins` continuam usando a leitura mais
recente, inclusive um boot, para `nivelCm` e `ultimaLeituraEm`. O contrato atual
diz isso explicitamente. O estado e a tendência já ignoram boot. Se o produto
quiser que o nível atual também ignore boot, isso é uma decisão de contrato
separada, com impacto em `buscarJardim`, `findUltimaLeituraDeCadaJardim` e nos
testes de estado atual.

### Validação

- Backend: histórico padrão inclui boot; `incluirBoot=false` exclui.
- Backend: jardim que só tem boot responde lista vazia, não 404.
- Frontend: `fetchHistorico` monta a URL com `incluirBoot=false` quando pedido.

## 6. Testes

### Frontend

`npm test` — **50 testes** em 7 arquivos:

- `useJardimData`: sucesso, falha total, falha parcial, 404 (inclusive depois de
  um sucesso, limpando os dados), preservação após sucesso e corrida J01 → J02 →
  J01;
- `App`: J01–J04, troca para J02 com conteúdo do gráfico, 404 como estado vazio,
  falha só no detalhe e falha só no histórico;
- `HistoricoChart`: estado vazio, erro, janela, CSV cronológico, eixo Y acima de
  50 cm;
- `api`: URLs, `incluirBoot=false`, 404, timeout, rede e resposta inválida;
- tipos e visão de rede.

### Backend

`./mvnw test` — **200 testes**, incluindo os novos casos de boot no histórico.

## 7. Documentação

- `frontend/INTEGRACAO-API.md`: removida a informação desatualizada de que
  `develop` não teria `estado`/`tendencia`; contagem de testes atualizada para
  49; comportamento de falha parcial, 404, boot e janela documentado.
- `docs/api-dashboard.md`: parâmetro `incluirBoot` documentado.
- Este arquivo substitui a nota de revisão que ficava em
  `frontend/ajustes_SW18.md` e não usa mais caminhos absolutos.

## O que permanece como observação

1. O detalhe (`buscarJardim`) e a lista (`listarJardins`) ainda podem exibir o
   nível de um boot como estado atual. É o contrato documentado; mudar isso exige
   uma decisão de produto.
2. O histórico é buscado a cada 5 s junto com o detalhe. Com 100 leituras isso é
   aceitável, mas pode ser otimizado com um intervalo maior ou busca sob demanda
   ao selecionar “Todas as leituras carregadas”.
3. O build do frontend continua com o aviso de chunk acima de 500 kB
   (~628 kB). É anterior a estes ajustes e não bloqueia a entrega.

## Validação recomendada

### Automática

```bash
cd frontend
npm ci
npm run lint
npm test
npm run build

cd ../api-jardim-sentinela
./mvnw test
```

### Manual com a API real

Terminal 1:

```bash
cd api-jardim-sentinela
./mvnw spring-boot:run
```

Terminal 2:

```bash
cd frontend
npm run dev
```

Abrir **`http://localhost:5173`** (não `127.0.0.1:5173`): o CORS da API libera
apenas `http://localhost:5173` e `http://localhost:4173`.

### Checklist manual

- [ ] Selecionar J01–J04 e conferir nível, estado, tendência e gráfico.
- [ ] Esperar 5 s e confirmar o polling.
- [ ] Parar a API e confirmar que os últimos dados permanecem com o aviso de
      atualização falhou.
- [ ] Reiniciar a API e clicar em “Tentar novamente”; o aviso deve desaparecer.
- [ ] Selecionar J02 e confirmar que o gráfico mostra as leituras de J02, não as
      de J01.
- [ ] Selecionar um jardim sem leitura (404) e confirmar “Sem leituras
      recebidas”, sem banner vermelho.
- [ ] Selecionar “Todas as leituras carregadas” com mais de 30 leituras e
      conferir “X de Y” quando a janela limita.
- [ ] Baixar o CSV e conferir a ordem cronológica e a ausência de boot.
- [ ] Alternar J01 → J02 → J01 rapidamente e confirmar que não aparece dado
      antigo de J01.
- [ ] Alternar tema claro/escuro.

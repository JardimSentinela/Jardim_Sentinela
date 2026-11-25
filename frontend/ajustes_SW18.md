# Ajustes pendentes — SW-18: detalhe e histórico no dashboard

> **Referência:** branch `feature/SW-18-detalhe-historico`, commit `81b0167`
> (“feat(SW-18): integrar detalhe e histórico dos jardins com a API”).
> Este documento lista os ajustes recomendados após a revisão do card SW-18.
> As referências de linha correspondem a esse commit e podem mudar depois das correções.

## Contexto

O critério de aceite do SW-18 está atendido no caminho feliz: ao selecionar J01–J04,
o dashboard exibe nível, estado, tendência e histórico/gráfico consumidos da API.

A integração inicial — endpoint de histórico e mapeamento do gráfico — veio do
SW-17/DS-04. O commit do SW-18 acrescenta tratamento de erro parcial, ajuste de
eixo/rótulos e testes. Os itens abaixo tratam das lacunas encontradas nessa camada
de robustez e dos casos de borda previstos no contrato da API.

**Prioridades:**

- **P1** — recomendado corrigir antes de considerar a entrega “pronta”.
- **P2** — importante; pode entrar no mesmo ciclo ou virar débito técnico explícito.
- **P3** — melhoria de qualidade/documentação; não bloqueia o caminho feliz.

## Resumo

| Prioridade | Ajuste | Arquivo principal |
|---|---|---|
| P1 | Preservar os últimos dados válidos em falha transitória | `frontend/src/hooks/useJardimData.ts` |
| P1 | Tratar 404 como estado vazio, não como erro | `useJardimData.ts`, `App.tsx`, `StatusCard.tsx` |
| P2 | Separar erro de detalhe e erro de histórico | `useJardimData.ts`, `App.tsx` |
| P2 | Corrigir a janela “Todas as leituras” / limite de 30 | `App.tsx`, `HistoricoChart.tsx` |
| P2 | Confirmar o tratamento de registros `boot` no gráfico | API + `HistoricoChart.tsx` |
| P3 | Completar os testes de integração e de estados | `*.test.tsx` |
| P3 | Atualizar a documentação | `frontend/INTEGRACAO-API.md` |
| P3 | Evitar corrida ao alternar J01 → J02 → J01 | `useJardimData.ts` |

---

## 1. [P1] Preservar os últimos dados válidos em falha transitória

### Problema

Em `useJardimData.ts:83-93`, quando `fetchJardim` ou `fetchHistorico` falha, o
código limpa os dados:

```ts
setJardim(null)
setHistorico([])
```

Isso apaga os cards e o gráfico a cada falha de polling (5 s). O comentário do
próprio hook (`useJardimData.ts:31-33`) afirma que ele mantém os últimos dados
válidos, mas a implementação não faz isso. O `useJardinsRede.ts:86-95` já adota
o comportamento correto: preserva o dado anterior e marca o erro.

### Comportamento esperado

- Falha transitória da API **não** deve apagar o último estado válido.
- O componente deve mostrar o dado anterior com um aviso de que a última
  atualização falhou.
- A limpeza só deve ocorrer ao trocar de jardim, no `useEffect` de
  `useJardimData.ts:133-138`.

### Sugestão de implementação

```ts
if (resJardim.status === 'fulfilled') {
  setJardim(resJardim.value)
  setErroJardim(null)
} else {
  setErroJardim(paraErro(resJardim.reason))
  // NÃO chamar setJardim(null): preserva o último dado válido
}

if (resHistorico.status === 'fulfilled') {
  setHistorico(resHistorico.value)
  setErroHistorico(null)
} else {
  setErroHistorico(paraErro(resHistorico.reason))
  // NÃO chamar setHistorico([]): preserva o último gráfico válido
}
```

### Critérios de aceite

- [ ] Depois de um carregamento com sucesso, simular falha de rede no polling.
- [ ] Nível, estado, tendência e gráfico continuam visíveis com o último dado.
- [ ] A tela informa que a última atualização falhou.
- [ ] Quando a API volta, os dados são atualizados e o aviso desaparece.
- [ ] Ao trocar de jardim, os dados do jardim anterior são limpos normalmente.
- [ ] Teste automatizado cobre “sucesso → falha → mantém dados → sucesso”.

---

## 2. [P1] Tratar 404 como estado vazio, não como erro

### Problema

O contrato em `docs/api-dashboard.md:106-108` diz:

> Jardim sem nenhuma leitura ainda → **404**. O dashboard precisa tratar esse caso
> (jardim cadastrado mas ainda sem dado é diferente de erro).

Hoje:

- `useJardimData` propaga o 404 como `error`;
- `App.tsx:283-291` mostra um banner vermelho;
- `StatusCard.tsx:44-63` mostra “Jardim indisponível”.

O `RedeJardins.tsx:26-31` já trata 404 como “Sem leituras recebidas”, mas o
detalhe do jardim selecionado não.

### Comportamento esperado

- Jardim sem leitura deve ser um **estado vazio**, não uma falha.
- Nada de banner vermelho nem mensagem de “API indisponível” para 404.
- `StatusCard` deve mostrar “Sem leituras recebidas” (ou equivalente).
- `DadosJardimCard` já mostra “Aguardando a primeira leitura do sensor.”; manter.
- O gráfico deve mostrar o estado vazio já existente (“Sem leituras para exibir”).

### Sugestão de implementação

```ts
function ehNaoEncontrado(erro: unknown): boolean {
  return erro instanceof ApiError && erro.status === 404
}

if (resJardim.status === 'rejected' && ehNaoEncontrado(resJardim.reason)) {
  setJardim(null)
  setSemLeituras(true)
  setErroJardim(null)
}
```

Fazer o mesmo para `fetchHistorico`. Expor `semLeituras` no retorno do hook e
usar em `StatusCard`.

### Critérios de aceite

- [ ] Jardim sem leitura (404) não exibe banner vermelho.
- [ ] `StatusCard` mostra “Sem leituras recebidas”.
- [ ] O gráfico mostra “Sem leituras para exibir”.
- [ ] Jardim com leitura continua funcionando normalmente.
- [ ] Teste automatizado cobre 404 no detalhe e no histórico.

---

## 3. [P2] Separar erro de detalhe e erro de histórico

### Problema

`App.tsx:92-96` expõe um único `error`, que pode vir de `fetchJardim` ou de
`fetchHistorico`. Ele é repassado ao `HistoricoChart` (`App.tsx:326`) e usado no
badge de conexão (`App.tsx:131-132`).

Consequências:

- Se o **detalhe** falha e o histórico carrega, o gráfico diz “Última atualização
  falhou” e o badge diz “API indisponível”, mesmo com o histórico OK.
- Se o **histórico** falha e o detalhe carrega, o banner diz que o jardim não
  atualizou, mesmo com nível/estado atualizados.

### Comportamento esperado

- Cada chamada tem seu próprio erro.
- O gráfico recebe apenas o erro do histórico.
- Os cards de status/dados recebem apenas o erro do detalhe.
- O banner geral pode mostrar uma mensagem combinada, desde que não mascare qual
  parte falhou.
- O badge de conexão não deve dizer “API indisponível” quando só o histórico
  falhou.

### Sugestão de implementação

No hook:

```ts
return {
  // ...
  error: erroJardim ?? erroHistorico, // compatibilidade com o banner atual
  erroJardim,
  erroHistorico,
}
```

No `App`:

```tsx
<StatusCard ... erro={erroJardim} />
<HistoricoChart ... error={erroHistorico} />
```

### Critérios de aceite

- [ ] Falha só no detalhe: cards mostram indisponível; gráfico permanece normal.
- [ ] Falha só no histórico: cards permanecem normais; gráfico mostra o aviso.
- [ ] Badge de conexão reflete o estado do detalhe (ou uma mensagem parcial).
- [ ] Testes cobrem as duas falhas parciais separadamente.

---

## 4. [P2] Corrigir a janela “Todas as leituras” / limite de 30

### Problema

`App.tsx:95` usa `limiteHistorico: 30`. O `HistoricoChart` oferece a opção
“Todas as leituras” (`HistoricoChart.tsx:39-43`) e o cabeçalho diz
“X leitura(s) recebida(s) da API” (`HistoricoChart.tsx:153-160`).

Com mais de 30 leituras, “Todas” mostra no máximo 30, e o texto dá a entender
que todas as leituras recebidas estão ali.

### Opções de correção

1. Aumentar o limite para 100 (padrão da API) ou 1000 (máximo), avaliando o
   custo do polling.
2. Renomear a opção para “Todas as carregadas (até 30)” e exibir
   “30 de {jardim.totalLeituras}” no cabeçalho.
3. Buscar um histórico maior apenas quando o usuário selecionar “Todas”.

Também vale reduzir a frequência do histórico: hoje são 30 leituras buscadas a
cada 5 s, o que não é necessário para o gráfico.

### Critérios de aceite

- [ ] O rótulo “Todas as leituras” não promete mais do que a tela entrega.
- [ ] O cabeçalho diferencia “leituras em exibição” de “leituras recebidas”.
- [ ] Com mais de 30 leituras, o comportamento é claro para o usuário.
- [ ] Teste cobre a janela com mais de 30 leituras.

---

## 5. [P2] Confirmar o tratamento de registros `boot` no gráfico

### Problema

A query `findHistorico` (`LeituraRepository.java:17-25`) inclui registros
`tipo: boot`. O campo `tipo` está como `@JsonIgnore` em `Leitura.java:112`,
então o frontend não consegue distinguir um boot de uma leitura real.

A tendência já ignora boot; o gráfico não. Se o firmware envia boot com nível
fictício, o gráfico pode mostrar um valor falso.

### Decisão necessária (backend + frontend)

Escolher uma das opções:

1. Expor `tipo` no JSON do histórico e filtrar/marcar no frontend.
2. Adicionar um parâmetro `incluirBoot=false` ao endpoint de histórico.
3. Filtrar boot no endpoint usado pelo dashboard.

### Critérios de aceite

- [ ] A decisão está registrada no card ou em `docs/`.
- [ ] O gráfico não exibe nível fictício de boot como se fosse medição.
- [ ] Se boot continuar no histórico, ele é marcado visualmente de forma clara.

---

## 6. [P3] Completar os testes

### Lacunas atuais

- `App.test.tsx:167-194` verifica apenas que `fetchHistorico` foi chamado para
  J02; não verifica se o gráfico exibiu os dados de J02. Como o mock devolve o
  mesmo `HISTORICO` para qualquer id, um bug de mistura de jardins passaria.
- `useJardimData.test.tsx:118-133` cristaliza o comportamento de limpar o
  histórico quando ele falha. Se o item 1 for corrigido, esse teste precisa
  mudar.
- Faltam testes para 404 como estado vazio, falha transitória após sucesso,
  ordem antiga→recente no gráfico e eixo Y acima de 50 cm.

### Testes recomendados

- [ ] `useJardimData`: sucesso → falha → mantém jardim e histórico.
- [ ] `useJardimData`: 404 → `semLeituras`, sem `error`.
- [ ] `useJardimData`: falha só no detalhe e falha só no histórico.
- [ ] `App`: selecionar J02 e verificar o conteúdo do gráfico de J02, não só a
      chamada.
- [ ] `HistoricoChart`: ordem cronológica dos pontos.
- [ ] `HistoricoChart`: domínio do eixo Y com leitura acima de 50 cm.
- [ ] `HistoricoChart`: janela “Todas” com mais de 30 leituras.

---

## 7. [P3] Atualizar a documentação

- `frontend/INTEGRACAO-API.md:107` diz “27 testes”; o estado atual tem **37**.
- `frontend/INTEGRACAO-API.md:11-24` diz que `develop` não tem `estado`/`tendencia`;
  o `origin/develop` atual já tem o contrato.
- Se o comportamento de falha parcial for alterado, documentar em
  `INTEGRACAO-API.md` e/ou `VALIDACAO-QA.md`.

### Critérios de aceite

- [ ] Contagem de testes atualizada.
- [ ] Texto sobre `develop` reflete o estado atual.
- [ ] Comportamento de erro parcial documentado.

---

## 8. [P3] Evitar corrida ao alternar J01 → J02 → J01

### Problema

`consultaEmAndamentoRef` é uma string com o id do jardim. Se o usuário voltar
para J01 enquanto a primeira requisição de J01 ainda está em voo, pode haver
sobreposição de requisições e respostas fora de ordem.

### Sugestão

Usar um contador de requisição ou `AbortController` para cancelar a consulta
anterior. É uma melhoria de robustez; não bloqueia o caminho feliz.

---

## Validação recomendada

### Automática

```bash
cd "/home/rios/Área de trabalho/Jardim/Jardim_Sentinela/frontend"

# O node_modules pode estar sem as devDependencies de teste.
npm ci

npm run lint
npm test -- --reporter=verbose
npm run build
```

### Manual com API real

Terminal 1:

```bash
cd "/home/rios/Área de trabalho/Jardim/Jardim_Sentinela/api-jardim-sentinela"
./mvnw spring-boot:run
```

Terminal 2:

```bash
cd "/home/rios/Área de trabalho/Jardim/Jardim_Sentinela/frontend"
npm run dev
```

Abrir **`http://localhost:5173`** (não `127.0.0.1:5173`): o CORS da API libera
apenas `http://localhost:5173` e `http://localhost:4173`.

Checklist manual:

- [ ] Selecionar J01–J04 e conferir nível, estado, tendência e gráfico.
- [ ] Esperar 5 s e confirmar o polling.
- [ ] Parar a API e confirmar que os últimos dados permanecem com aviso.
- [ ] Reiniciar a API e clicar em “Tentar novamente”.
- [ ] Selecionar “Todas as leituras” com mais de 30 leituras.
- [ ] Alternar tema claro/escuro.
- [ ] Baixar o CSV e conferir os dados da janela selecionada.
- [ ] Selecionar jardim sem leitura e confirmar o estado vazio (404).

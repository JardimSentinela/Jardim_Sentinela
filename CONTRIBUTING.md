# Guia de Contribuição - Jardim Sentinela

Primeiramente, obrigado por dedicar seu tempo para contribuir com o projeto **Jardim Sentinela**! 

Este documento estabelece as diretrizes e padrões para versionamento, nomenclatura de *branches*, mensagens de *commit* e integração automatizada com o **Jira**. Seguir este padrão é essencial para manter a rastreabilidade do histórico de desenvolvimento e o fluxo de trabalho organizado.

---

## Sumário
1. [Visão Geral da Integração Jira + GitHub](#1-visão-geral-da-integração-jira--github)
2. [Padrão de Nomenclatura de Branches](#2-padrão-de-nomenclatura-de-branches)
3. [Padrão de Mensagens de Commit](#3-padrão-de-mensagens-de-commit)
4. [Jira Smart Commits (Ações Automáticas)](#4-jira-smart-commits-ações-automáticas)
5. [Padronização de Pull Requests (PRs)](#5-padronização-de-pull-requests-prs)
6. [Fluxo de Trabalho Passo a Passo](#6-fluxo-de-trabalho-passo-a-passo)

---

## 1. Visão Geral da Integração Jira + GitHub

Nosso repositório utiliza a integração oficial do Jira Cloud para GitHub. Isso significa que, ao mencionar a chave de uma tarefa do Jira (ex: `JS-123`) em suas branches, commits e Pull Requests:
- O Jira vinculará automaticamente os commits e PRs ao card correspondente.
- Você pode registrar tempo de trabalho (*worklog*), adicionar comentários e transicionar o status da tarefa diretamente a partir dos seus commits no Git.

> 💡 **Projeto Key no Jira:** Substitua `JS` pela chave oficial definida no seu projeto no Jira caso seja diferente (ex: `JS`, `SENT`, `JARDIM`).

---

## 2. Padrão de Nomenclatura de Branches

Toda nova funcionalidade, correção ou alteração deve ser desenvolvida em uma *branch* dedicada criada a partir da branch principal (`main` ou `develop`).

### Formato Recomendado:
```bash
<tipo>/<CHAVE-JIRA>-<breve-descricao-kebab-case>
```

### Tipos de Branch:
- `feat/`: Desenvolvimento de uma nova funcionalidade.
- `fix/`: Correção de bugs ou comportamentos inesperados em produção/desenvolvimento.
- `refactor/`: Alteração de código sem mudar funcionalidades existentes (melhoria de desempenho ou legibilidade).
- `docs/`: Alterações exclusivamente na documentação.
- `chore/`: Tarefas de manutenção, atualização de dependências, configurações do projeto.
- `test/`: Adição ou refatoração de testes automatizados.

### Exemplos Válidos:
- `feat/JS-45-integração-sensor-umidade`
- `fix/JS-102-correcao-leitura-temperatura`
- `docs/JS-12-atualizacao-readme-api`
- `chore/JS-89-configurar-pipeline-ci-cd`

---

## 3. Padrão de Mensagens de Commit

Adotamos a convenção do [Conventional Commits](https://www.conventionalcommits.org/) combinada com a **Chave da Issue do Jira** no início ou no corpo da mensagem.

### Formato Básico:
```bash
<tipo>(<escopo>): [<CHAVE-JIRA>] <descrição sucinta em português>
```
*(O escopo é opcional, mas recomendado para projetos maiores).*

### Exemplos:
- `feat(sensor): [JS-45] adiciona driver de leitura para o sensor DHT22`
- `fix(api): [JS-102] ajusta timeout ao consultar dados pluviométricos`
- `docs: [JS-12] atualiza instruções de setup no CONTRIBUTING.md`

---

## 4. Jira Smart Commits (Ações Automáticas)

Com os **Smart Commits**, você pode realizar ações diretas no Jira através da mensagem do seu commit no Git.

### Sintaxe Geral:
```bash
<CHAVE-JIRA> #<comando> <parâmetros-opcionais>
```

### Comandos Principais:

#### 1. Comentar no Card (`#comment`)
Adiciona um comentário diretamente no card do Jira.
```bash
JS-45 #comment Implementada a leitura analógica dos sensores de umidade do solo.
```

#### 2. Registrar Tempo de Trabalho (`#time`)
Registra o tempo gasto na tarefa no *worklog* do Jira (Formatos suportados: `1w`, `2d`, `4h`, `30m`).
```bash
JS-45 #time 2h 30m #comment Finalizado o mapeamento dos pinos do microcontrolador.
```

#### 3. Alterar Status do Card / Transição (`#<nome-da-transicao>`)
Altera o status da tarefa no Jira. O nome do comando deve corresponder ao nome ou ID exato da transição configurada no seu fluxo de trabalho no Jira (geralmente sem espaços ou em caixa baixa/kebab-case).

*(Exemplos comuns de status/transições: `#in-progress`, `#done`, `#close`, `#em-andamento`, `#concluido`).*

```bash
JS-45 #done #comment Testes do módulo de irrigação concluídos com sucesso em bancada.
```

#### Combinando Comandos em um Único Commit:
```bash
feat(irrigacao): [JS-45] adiciona acionamento automatico da bomba de agua

JS-45 #time 1h 45m #done #comment Validação concluída. Módulo pronto para deploy.
```

---

## 5. Padronização de Pull Requests (PRs)

Ao abrir um Pull Request no GitHub:

1. **Título do PR:** Deve conter a chave do Jira no início.
   - *Exemplo:* `[JS-45] Implementação do módulo de irrigação automatizada`
2. **Descrição do PR:** Deve descrever o que foi feito, testes realizados e referenciar o card do Jira.
   - Inclua o link direto para a issue no Jira para facilitar a revisão.

### Template de PR Recomendado:
```markdown
## Task Jira
 Link para o card: [JS-45](https://sua-organizacao.atlassian.net/browse/JS-45)

## O que foi feito?
- Adicionado driver para o sensor de umidade de solo.
- Criada lógica de acionamento do relé da bomba de água.

## Como testar?
1. Executar o script `python test_sensors.py`.
2. Verificar se as leituras estão sendo exibidas no terminal.
```

---

## 6. Fluxo de Trabalho Passo a Passo

1. **Atribua a task a você no Jira** e copie a chave do card (ex: `JS-50`).
2. **Crie a branch** com o padrão definido:
   ```bash
   git checkout -b feat/JS-50-modulo-alertas
   ```
3. **Faça os commits** utilizando a referência ao Jira e, se necessário, os Smart Commits:
   ```bash
   git commit -m "feat(alertas): [JS-50] adiciona envio de notificacao via Telegram"
   ```
4. **Envie sua branch para o repositório remoto:**
   ```bash
   git push origin feat/JS-50-modulo-alertas
   ```
5. **Abra um Pull Request** no GitHub apontando para a branch principal e aguarde a revisão da equipe.
6. **Ao aprovar e fazer o Merge**, verifique se o card no Jira atualizou o status automaticamente!

---

*Dúvidas sobre o fluxo de contribuição? Fale com os mantenedores do projeto Jardim Sentinela.* 🚀
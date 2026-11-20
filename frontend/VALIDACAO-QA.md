# Validação de QA — Etapa 4 (Ponta a Ponta)

**Responsável:** sentinela-qa
**Data:** 18/11/2026
**Escopo:** lint, build de produção, execução do dashboard, inspeção visual/funcional no portal Maestri (`Preview`, `http://127.0.0.1:5173`) e responsividade.

---

## 1. Qualidade de código

| Verificação | Comando | Resultado |
|---|---|---|
| Lint (ESLint) | `npm run lint` | ✅ Sem erros e sem warnings (exit 0) |
| Build de produção | `npm run build` (`tsc -b && vite build`) | ✅ Concluído em ~11 s |
| Type-check | embutido no build (`tsc -b`) | ✅ Sem erros de tipo |

**Artefatos gerados**

```
dist/index.html                   0,66 kB │ gzip:  0,41 kB
dist/assets/index-Cw1hEsT0.css   36,41 kB │ gzip:  7,40 kB
dist/assets/index-u4_ZSB9u.js   677,19 kB │ gzip: 195,95 kB
```

⚠️ Aviso não bloqueante: chunk JS de 677 kB (> 500 kB) — ver "Observações" abaixo.

## 2. Servidor e portal

- Servidor: `npx vite --port 5173 --host 127.0.0.1` → Vite 5.4.21 pronto em ~206 ms.
- `http://127.0.0.1:5173/` respondeu **HTTP 200** (confirmado por `curl` e pelo portal).
- Portal Maestri criado: `maestri portal create http://127.0.0.1:5173 "Preview"` → **Created portal "Preview"**.
- `maestri portal snapshot "Preview"` retornou a árvore de acessibilidade completa (tabs, grupo de tema, combobox, select, botões e gráficos), sem erros de console (`maestri portal logs` vazio).

## 3. Inspeção funcional

| Cenário | Procedimento | Resultado |
|---|---|---|
| Tema Claro/Escuro | Botões "Modo Claro" / "Modo Escuro" | ✅ Alternância imediata; cores, bordas e gráficos acompanham o tema |
| Persistência do tema | Recarregar a página em tema claro | ✅ Mantém `claro` (localStorage `jardim-sentinela-tema`) |
| Aba Painel Jardim Sentinela | Clique na aba | ✅ Cards de status, dados do jardim e histórico renderizados |
| Aba Painel COP | Clique em "Painel COP" | ✅ Estágio operacional, chuva 24 h, abrigos e gráfico de chuvas |
| Seleção de jardins | Select J01 → J02 → J03 → J04 (+ J032) | ✅ Dados e estado mudam por jardim |
| Busca rápida | Digitar `J032` + Enter | ✅ Filtra para 1 opção e seleciona J032 |
| Fechar sugestões | Tecla `Escape` | ✅ Listbox fecha |
| Atualizar dados | Botão "Atualizar" | ✅ Refetch sem erros |
| Período do histórico | Botão "1º Semestre" | ✅ Alterna sem erros |
| Exportar histórico | Botão "Baixar Histórico" | ✅ Aciona download sem erros de console |
| Responsividade | Viewport 390×844 | ✅ Sem overflow horizontal (`scrollWidth == clientWidth`), navegação e cards empilhados |

**Dados observados por jardim** (confirmam que a troca propaga estado + dados):

| Jardim | Nível | Estado | Bateria |
|---|---|---|---|
| J01 | 0,19 m / 18,5 cm (37%) | Atenção | 97% |
| J02 | 0,28 m / 27,8 cm (56%) | Atenção | 88% |
| J03 | 0,34 m / 34,2 cm (68%) | Transbordando | 76% |
| J04 | 0,09 m / 9,4 cm (19%) | Normal | 91% |
| J032 | 0,48 m / 47,5 cm (95%) | Alagado | 64% |

**Painel COP:** Estágio "Atenção", chuva 24 h 78,6 mm, máxima 112,4 mm, 2 abrigos ativos, gráfico de precipitação renderizado.

## 4. Evidências (screenshots)

- `Preview` claro, J01 — `/tmp/maestri-portal-66b73b5a-99e1-4689-a805-3d5a75e5b02f.png`
- `Preview` escuro, J01 — `/tmp/maestri-portal-a09780d6-ee1e-4601-84b2-51139b88c4ad.png`
- Painel COP claro — `/tmp/maestri-portal-0c3bb675-622e-4333-acd6-8f4ca6d3a2aa.png`
- Painel COP escuro — `/tmp/maestri-portal-e86ea570-1ab2-4095-871f-c300463b2c59.png`
- J02 — `/tmp/maestri-portal-dc77278e-4eb6-441f-8b59-0cba938d3af5.png`
- J03 — `/tmp/maestri-portal-1f6e1d45-7116-4232-bf82-c45d1df5befa.png`
- J04 — `/tmp/maestri-portal-7c87f3bf-cf01-4a06-8128-742ac2d3c380.png`
- Mobile 390×844 — `/tmp/maestri-portal-791ee8ca-b25f-4b3b-aef5-9d26a7754d8b.png`

## 5. Observações e recomendações

1. **Corrigido — texto fixo no StatusCard:** o selo e a linha de subtítulo exibiam sempre "Estado de alerta" / "Estado de alerta — …", inclusive quando o estado era **Normal** (J04). Ajustado durante esta etapa em `src/components/StatusCard.tsx` para derivar o rótulo de `nivel` ("Estado normal" / "Monitoramento regular"); revalidado ao vivo no portal com J04 e novo `lint`/`build` verdes.
2. **Informativo — bundle:** chunk principal de 677 kB por causa do `recharts`. Recomenda-se `manualChunks`/lazy-load dos gráficos para reduzir o TTI.
3. **Informativo — indicador de conexão:** como a API Java não está em execução neste ambiente, o badge mostra "Dados Mockados / Demonstração", comportamento esperado e implementado.
4. **Acessibilidade:** papéis `tab`/`listbox`/`option`/`progressbar` e rótulos ARIA presentes e coerentes; navegação por teclado na busca (Enter/Escape) funcionando.

## 6. Conclusão

✅ **Aprovado.** Lint limpo, build de produção bem-sucedido, dashboard estável nos temas claro/escuro, ambas as abas funcionais, seleção/busca dos jardins operando e layout responsivo sem regressões. O único ajuste encontrado (texto do selo de status) foi corrigido e revalidado nesta mesma etapa.

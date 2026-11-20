import { useEffect, useMemo, useRef, useState } from 'react'
import type { KeyboardEvent } from 'react'
import { ChevronDown, Search, X } from 'lucide-react'
import Navbar from './components/Navbar'
import type { AbaPainel } from './components/Navbar'
import StatusCard from './components/StatusCard'
import DadosJardimCard from './components/DadosJardimCard'
import HistoricoChart from './components/HistoricoChart'
import PainelCOP from './components/PainelCOP'
import RedeJardins from './components/RedeJardins'
import { useJardimData } from './hooks/useJardimData'
import {
  JARDINS_MONITORADOS,
  useJardinsRede,
  type JardimMonitoradoId,
} from './hooks/useJardinsRede'

const JARDINS_DISPONIVEIS = JARDINS_MONITORADOS
const CHAVE_TEMA = 'jardim-sentinela-tema'

type JardimId = JardimMonitoradoId

function lerTemaInicial(): boolean {
  if (typeof window === 'undefined') {
    return false
  }

  const salvo = window.localStorage.getItem(CHAVE_TEMA)
  if (salvo === 'escuro') {
    return true
  }
  if (salvo === 'claro') {
    return false
  }

  return window.matchMedia('(prefers-color-scheme: dark)').matches
}

/**
 * No modo escuro, clareia a cor do estado com o azul-claro oficial
 * (#4CC7FB) para o destaque do gráfico manter contraste sobre #353443.
 */
function corDestaqueDoGrafico(cor: string | undefined, escuro: boolean): string | undefined {
  if (!cor) {
    return undefined
  }
  return escuro ? `color-mix(in srgb, ${cor} 55%, #4cc7fb)` : cor
}

function StatusConexao({
  conectado,
  verificando,
}: {
  conectado: boolean
  verificando: boolean
}) {
  const rotulo = verificando
    ? 'Verificando conexão…'
    : conectado
      ? 'Conectado à API Java'
      : 'API indisponível'

  const cor = verificando
    ? 'text-[var(--cor-texto-suave)]'
    : conectado
      ? 'text-[#005b7f] dark:text-[#4cc7fb]'
      : 'text-[#9a3412] dark:text-[#fdba74]'

  return (
    <span
      className={`inline-flex items-center gap-2 text-xs font-medium ${cor}`}
      title={
        conectado
          ? 'Os dados exibidos vêm da API Java do Jardim Sentinela.'
          : 'A API não respondeu; os dados de jardins não puderam ser atualizados.'
      }
    >
      <span className="h-2 w-2 rounded-full bg-current" aria-hidden="true" />
      {rotulo}
    </span>
  )
}

function App() {
  const [temaEscuro, setTemaEscuro] = useState<boolean>(lerTemaInicial)
  const [abaAtiva, setAbaAtiva] = useState<AbaPainel>('jardim')
  const [jardimSelecionado, setJardimSelecionado] = useState<JardimId>('J01')
  const [busca, setBusca] = useState('')
  const [buscaAberta, setBuscaAberta] = useState(false)
  const areaBuscaRef = useRef<HTMLDivElement>(null)

  const { jardim, historico, alerta, cop, loading, error, refetch } =
    useJardimData(jardimSelecionado, {
      intervaloPollingMs: 5000,
      limiteHistorico: 30,
    })

  const {
    itens: itensRede,
    loading: loadingRede,
    error: errorRede,
    refetch: refetchRede,
  } = useJardinsRede(JARDINS_MONITORADOS, {
    intervaloPollingMs: 5000,
  })

  useEffect(() => {
    document.documentElement.classList.toggle('dark', temaEscuro)
    window.localStorage.setItem(CHAVE_TEMA, temaEscuro ? 'escuro' : 'claro')
  }, [temaEscuro])

  useEffect(() => {
    function aoClicarFora(evento: MouseEvent) {
      if (!areaBuscaRef.current?.contains(evento.target as Node)) {
        setBuscaAberta(false)
      }
    }

    document.addEventListener('mousedown', aoClicarFora)
    return () => document.removeEventListener('mousedown', aoClicarFora)
  }, [])

  const jardinsFiltrados = useMemo(() => {
    const termo = busca.trim().toLowerCase()
    if (!termo) {
      return [...JARDINS_DISPONIVEIS]
    }
    return JARDINS_DISPONIVEIS.filter((id) => id.toLowerCase().includes(termo))
  }, [busca])

  const verificandoConexao = !jardim && (loading || loadingRede)
  const conectado = !error && !errorRede && !verificandoConexao

  function selecionarJardim(id: JardimId) {
    setJardimSelecionado(id)
    setBusca('')
    setBuscaAberta(false)
  }

  function aoTeclarNaBusca(evento: KeyboardEvent<HTMLInputElement>) {
    if (evento.key === 'Escape') {
      setBuscaAberta(false)
      return
    }

    if (evento.key === 'Enter') {
      evento.preventDefault()
      const primeiro = jardinsFiltrados[0]
      if (primeiro) {
        selecionarJardim(primeiro)
      }
    }
  }

  function trocarAba(aba: AbaPainel) {
    setAbaAtiva(aba)
    setBuscaAberta(false)
  }

  return (
    <div className="min-h-screen bg-[var(--cor-fundo)]">
      <Navbar
        temaEscuro={temaEscuro}
        onDefinirTema={setTemaEscuro}
        abaAtiva={abaAtiva}
        onMudarAba={trocarAba}
      />

      <main className="mx-auto w-full max-w-[1440px] px-4 py-6 sm:px-6 lg:px-8 lg:py-8">
        {abaAtiva === 'jardim' ? (
          <>
            <div className="mb-4 flex flex-wrap items-center gap-4">
              <h1 className="text-2xl font-bold tracking-tight sm:text-3xl">
                Monitoramento Jardim Sentinela
              </h1>

              <label className="inline-flex items-center gap-2 rounded-full border border-[var(--cor-borda)] bg-[var(--cor-superficie)] py-2 pl-4 pr-3">
                <span className="text-sm text-[var(--cor-texto-suave)]">
                  Selecione o Jardim
                </span>
                <div className="relative">
                  <select
                    value={jardimSelecionado}
                    onChange={(evento) =>
                      setJardimSelecionado(evento.target.value as JardimId)
                    }
                    className="cursor-pointer appearance-none bg-transparent pr-5 text-sm font-semibold text-[var(--cor-texto-forte)] focus:outline-none"
                  >
                    {JARDINS_DISPONIVEIS.map((id) => (
                      <option key={id} value={id}>
                        {id}
                      </option>
                    ))}
                  </select>
                  <ChevronDown
                    className="pointer-events-none absolute right-0 top-1/2 h-4 w-4 -translate-y-1/2 text-[var(--cor-texto-suave)]"
                    aria-hidden="true"
                  />
                </div>
              </label>

              <div className="sm:ml-auto">
                <StatusConexao conectado={conectado} verificando={verificandoConexao} />
              </div>
            </div>

            <div className="relative mb-6 w-full max-w-md" ref={areaBuscaRef}>
              <Search
                className="pointer-events-none absolute left-3.5 top-1/2 h-4 w-4 -translate-y-1/2 text-[var(--cor-texto-suave)]"
                aria-hidden="true"
              />
              <input
                type="text"
                role="combobox"
                aria-label="Busca Rápida por Jardins"
                aria-expanded={buscaAberta}
                aria-controls="lista-sugestoes-jardins"
                aria-autocomplete="list"
                autoComplete="off"
                placeholder="Busca Rápida por Jardins"
                value={busca}
                onChange={(evento) => {
                  setBusca(evento.target.value)
                  setBuscaAberta(true)
                }}
                onFocus={() => setBuscaAberta(true)}
                onKeyDown={aoTeclarNaBusca}
                className="h-11 w-full rounded-full border border-[var(--cor-borda)] bg-[var(--cor-superficie)] pl-10 pr-10 text-sm text-[var(--cor-texto)] shadow-sm placeholder:text-[var(--cor-texto-suave)] focus:border-[var(--cor-foco)]"
              />
              {busca && (
                <button
                  type="button"
                  aria-label="Limpar busca"
                  onClick={() => {
                    setBusca('')
                    setBuscaAberta(false)
                  }}
                  className="absolute right-3 top-1/2 flex h-6 w-6 -translate-y-1/2 items-center justify-center rounded-full text-[var(--cor-texto-suave)] hover:bg-[var(--cor-superficie-suave)] hover:text-[var(--cor-texto)]"
                >
                  <X className="h-4 w-4" aria-hidden="true" />
                </button>
              )}

              {buscaAberta && (
                <ul
                  id="lista-sugestoes-jardins"
                  role="listbox"
                  aria-label="Jardins disponíveis"
                  className="absolute left-0 right-0 z-30 mt-2 overflow-hidden rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie)] p-1 shadow-lg"
                >
                  {jardinsFiltrados.length > 0 ? (
                    jardinsFiltrados.map((id) => {
                      const ativo = id === jardimSelecionado
                      return (
                        <li
                          key={id}
                          role="option"
                          aria-selected={ativo}
                          tabIndex={-1}
                          onMouseDown={(evento) => {
                            evento.preventDefault()
                            selecionarJardim(id)
                          }}
                          className={`cursor-pointer rounded-lg px-3 py-2 text-sm transition-colors ${
                            ativo
                              ? 'bg-[var(--cor-superficie-suave)] font-semibold text-[var(--cor-texto-forte)]'
                              : 'text-[var(--cor-texto)] hover:bg-[var(--cor-superficie-suave)]'
                          }`}
                        >
                          Jardim {id}
                        </li>
                      )
                    })
                  ) : (
                    <li className="px-3 py-2 text-sm text-[var(--cor-texto-suave)]">
                      Nenhum jardim encontrado.
                    </li>
                  )}
                </ul>
              )}
            </div>

            {error && (
              <p
                role="alert"
                className="mb-4 rounded-lg border border-[#f0b7a1] bg-[#fff4ef] px-3 py-2 text-sm text-[#9a3412] dark:border-[#7c2d12] dark:bg-[#3b1c12] dark:text-[#fdba74]"
              >
                Não foi possível atualizar o jardim {jardimSelecionado}:{' '}
                {error.message}
              </p>
            )}

            <RedeJardins
              itens={itensRede}
              loading={loadingRede}
              error={errorRede}
              jardimSelecionado={jardimSelecionado}
              onSelecionar={selecionarJardim}
              onTentarNovamente={() => {
                void refetchRede()
                void refetch()
              }}
              className="mb-6"
            />

            <div className="grid gap-6 lg:grid-cols-12">
              <div className="lg:col-span-5">
                <StatusCard
                  jardim={jardim}
                  alerta={alerta}
                  loading={loading && !jardim}
                  onTentarNovamente={() => void refetch()}
                />
              </div>
              <div className="lg:col-span-7">
                <DadosJardimCard jardim={jardim} loading={loading && !jardim} />
              </div>
            </div>

            <div className="mt-6">
              <HistoricoChart
                jardimId={jardimSelecionado}
                historico={historico}
                loading={loading && historico.length === 0}
                corDestaque={corDestaqueDoGrafico(alerta?.cor, temaEscuro)}
              />
            </div>
          </>
        ) : (
          <PainelCOP cop={cop} />
        )}
      </main>
    </div>
  )
}

export default App

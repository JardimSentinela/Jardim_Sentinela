import { useEffect, useMemo, useRef, useState } from 'react'
import type { KeyboardEvent } from 'react'
import {
  AlertTriangle,
  ChevronDown,
  Database,
  Flower2,
  RefreshCw,
  Search,
  Wifi,
  X,
} from 'lucide-react'
import Navbar from './components/Navbar'
import type { AbaPainel } from './components/Navbar'
import StatusCard from './components/StatusCard'
import DadosJardimCard from './components/DadosJardimCard'
import HistoricoChart from './components/HistoricoChart'
import PainelCOP from './components/PainelCOP'
import { useJardimData } from './hooks/useJardimData'
import { formatarDataHora } from './types/jardim'

const JARDINS_DISPONIVEIS = ['J01', 'J02', 'J03', 'J04', 'J032'] as const
const CHAVE_TEMA = 'jardim-sentinela-tema'

type JardimId = (typeof JARDINS_DISPONIVEIS)[number]

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

function BadgeConexao({
  conectado,
  verificando,
}: {
  conectado: boolean
  verificando: boolean
}) {
  const estilo = verificando
    ? 'border-amber-200 bg-amber-50 text-amber-700 dark:border-amber-900/60 dark:bg-amber-950/40 dark:text-amber-300'
    : conectado
      ? 'border-emerald-200 bg-emerald-50 text-emerald-700 dark:border-emerald-900/60 dark:bg-emerald-950/40 dark:text-emerald-300'
      : 'border-sky-200 bg-sky-50 text-sky-700 dark:border-sky-900/60 dark:bg-sky-950/40 dark:text-sky-300'

  const rotulo = verificando
    ? 'Verificando conexão…'
    : conectado
      ? 'Conectado à API Java'
      : 'Dados Mockados / Demonstração'

  const Icone = conectado && !verificando ? Wifi : Database

  return (
    <span
      className={`inline-flex items-center gap-2 rounded-full border px-3 py-1.5 text-[11px] font-semibold shadow-sm sm:text-xs ${estilo}`}
      title={
        conectado
          ? 'Os dados exibidos vêm da API Java do Jardim Sentinela.'
          : 'A API não respondeu; exibindo dados de demonstração.'
      }
    >
      <span className="relative flex h-2 w-2">
        <span
          className={`absolute inline-flex h-full w-full rounded-full opacity-60 ${
            verificando ? 'animate-ping bg-amber-400' : conectado ? 'bg-emerald-400' : 'bg-sky-400'
          }`}
        />
        <span
          className={`relative inline-flex h-2 w-2 rounded-full ${
            verificando ? 'bg-amber-500' : conectado ? 'bg-emerald-500' : 'bg-sky-500'
          }`}
        />
      </span>
      <Icone className="h-3.5 w-3.5" aria-hidden="true" />
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

  const { jardim, historico, alerta, cop, loading, error, isMock, refetch } =
    useJardimData(jardimSelecionado, {
      intervaloPollingMs: 5000,
      limiteHistorico: 30,
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

  const verificandoConexao = loading && !jardim
  const conectado = !isMock && !verificandoConexao && !error
  const { data: dataAtualizacao, hora: horaAtualizacao } = formatarDataHora(
    jardim?.ultimaLeituraEm ?? '',
  )

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
    <div className="relative min-h-screen">
      <div aria-hidden="true" className="pointer-events-none fixed inset-0 -z-10 overflow-hidden">
        <div className="absolute -left-32 top-20 h-80 w-80 rounded-full bg-jardin-200/30 blur-3xl dark:bg-jardin-800/15" />
        <div className="absolute -right-24 top-1/3 h-96 w-96 rounded-full bg-agua-200/30 blur-3xl dark:bg-agua-900/15" />
        <div className="absolute bottom-0 left-1/3 h-72 w-72 rounded-full bg-emerald-100/40 blur-3xl dark:bg-emerald-950/10" />
      </div>

      <Navbar
        temaEscuro={temaEscuro}
        onDefinirTema={setTemaEscuro}
        abaAtiva={abaAtiva}
        onMudarAba={trocarAba}
      />

      <main className="mx-auto w-full max-w-[1440px] px-4 pb-14 pt-6 sm:px-6 lg:px-8">
        <div key={abaAtiva} className="animar-surgir">
          {abaAtiva === 'jardim' ? (
            <>
              <div className="mb-5 flex flex-col gap-4 xl:flex-row xl:items-end xl:justify-between">
                <div>
                  <p className="kicker">Painel Jardim Sentinela</p>
                  <h2 className="mt-1 text-2xl sm:text-[30px]">Monitoramento em tempo real</h2>
                  <p className="mt-1 max-w-2xl text-sm text-[var(--cor-texto-suave)]">
                    Acompanhe o nível da água, a bateria do sensor e o histórico de variações
                    de cada jardim de chuva da rede.
                  </p>
                </div>
                <div className="flex flex-wrap items-center gap-2">
                  <BadgeConexao conectado={conectado} verificando={verificandoConexao} />
                </div>
              </div>

              {error && (
                <div className="mb-5 flex flex-wrap items-center gap-3 rounded-xl border border-red-200 bg-red-50 px-4 py-3 text-sm text-red-700 dark:border-red-900/60 dark:bg-red-950/40 dark:text-red-300">
                  <AlertTriangle className="h-4 w-4 shrink-0" aria-hidden="true" />
                  <span className="flex-1">
                    Não foi possível atualizar os dados: {error.message}
                  </span>
                  <button
                    type="button"
                    onClick={() => void refetch()}
                    className="rounded-lg border border-red-300 px-2.5 py-1 text-xs font-semibold transition-colors hover:bg-red-100 dark:border-red-800 dark:hover:bg-red-900/40"
                  >
                    Recarregar
                  </button>
                </div>
              )}

              {/* Seleção e busca de jardins */}
              <section className="card mb-6 p-4 sm:p-5" aria-label="Seleção de jardim">
                <div className="grid gap-3 md:grid-cols-[minmax(0,250px)_minmax(0,1fr)_auto] md:items-end">
                  <div>
                    <label htmlFor="seletor-jardim" className="etiqueta">
                      Selecione o Jardim
                    </label>
                    <div className="relative mt-1.5">
                      <Flower2
                        className="pointer-events-none absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-jardin-600 dark:text-jardin-300"
                        aria-hidden="true"
                      />
                      <select
                        id="seletor-jardim"
                        value={jardimSelecionado}
                        onChange={(evento) =>
                          selecionarJardim(evento.target.value as JardimId)
                        }
                        className="w-full cursor-pointer appearance-none rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie)] py-2.5 pl-10 pr-9 text-sm font-semibold text-[var(--cor-texto-forte)] shadow-sm transition-colors hover:border-[var(--cor-borda-forte)] focus:border-jardin-500"
                      >
                        {JARDINS_DISPONIVEIS.map((id) => (
                          <option key={id} value={id}>
                            Jardim {id}
                          </option>
                        ))}
                      </select>
                      <ChevronDown
                        className="pointer-events-none absolute right-3 top-1/2 h-4 w-4 -translate-y-1/2 text-[var(--cor-texto-suave)]"
                        aria-hidden="true"
                      />
                    </div>
                  </div>

                  <div ref={areaBuscaRef}>
                    <label htmlFor="busca-jardins" className="etiqueta">
                      Busca Rápida por Jardins
                    </label>
                    <div className="relative mt-1.5">
                      <Search
                        className="pointer-events-none absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-[var(--cor-texto-suave)]"
                        aria-hidden="true"
                      />
                      <input
                        id="busca-jardins"
                        type="text"
                        role="combobox"
                        aria-expanded={buscaAberta}
                        aria-controls="lista-sugestoes-jardins"
                        aria-autocomplete="list"
                        autoComplete="off"
                        placeholder="Ex.: J01, J032…"
                        value={busca}
                        onChange={(evento) => {
                          setBusca(evento.target.value)
                          setBuscaAberta(true)
                        }}
                        onFocus={() => setBuscaAberta(true)}
                        onKeyDown={aoTeclarNaBusca}
                        className="w-full rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie)] py-2.5 pl-10 pr-10 text-sm text-[var(--cor-texto)] shadow-sm transition-colors placeholder:text-[var(--cor-texto-suave)] hover:border-[var(--cor-borda-forte)] focus:border-jardin-500"
                      />
                      {busca && (
                        <button
                          type="button"
                          aria-label="Limpar busca"
                          onClick={() => {
                            setBusca('')
                            setBuscaAberta(false)
                          }}
                          className="absolute right-2.5 top-1/2 flex h-6 w-6 -translate-y-1/2 items-center justify-center rounded-md text-[var(--cor-texto-suave)] transition-colors hover:bg-[var(--cor-superficie-suave)] hover:text-[var(--cor-texto)]"
                        >
                          <X className="h-3.5 w-3.5" aria-hidden="true" />
                        </button>
                      )}

                      {buscaAberta && (
                        <ul
                          id="lista-sugestoes-jardins"
                          role="listbox"
                          aria-label="Sugestões de jardins"
                          className="sombra-flutuante absolute left-0 right-0 z-30 mt-2 max-h-64 overflow-y-auto rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie)] p-1"
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
                                  className={`flex cursor-pointer items-center justify-between gap-3 rounded-lg px-3 py-2 transition-colors ${
                                    ativo
                                      ? 'bg-jardin-50 dark:bg-jardin-950/40'
                                      : 'hover:bg-[var(--cor-superficie-suave)]'
                                  }`}
                                >
                                  <span className="flex items-center gap-2.5">
                                    <span
                                      className={`flex h-7 w-7 items-center justify-center rounded-lg ${
                                        ativo
                                          ? 'bg-jardin-100 text-jardin-700 dark:bg-jardin-900/60 dark:text-jardin-300'
                                          : 'bg-[var(--cor-superficie-suave)] text-[var(--cor-texto-suave)]'
                                      }`}
                                    >
                                      <Flower2 className="h-3.5 w-3.5" aria-hidden="true" />
                                    </span>
                                    <span>
                                      <span
                                        className={`block text-sm font-semibold ${
                                          ativo
                                            ? 'text-jardin-700 dark:text-jardin-300'
                                            : 'text-[var(--cor-texto)]'
                                        }`}
                                      >
                                        Jardim {id}
                                      </span>
                                      <span className="block text-[11px] text-[var(--cor-texto-suave)]">
                                        Jardim de chuva monitorado
                                      </span>
                                    </span>
                                  </span>
                                  {ativo && (
                                    <span className="rounded-full bg-jardin-100 px-2 py-0.5 text-[10px] font-bold uppercase tracking-wide text-jardin-700 dark:bg-jardin-900/60 dark:text-jardin-300">
                                      Atual
                                    </span>
                                  )}
                                </li>
                              )
                            })
                          ) : (
                            <li className="px-3 py-2.5 text-xs text-[var(--cor-texto-suave)]">
                              Nenhum jardim encontrado para “{busca}”.
                            </li>
                          )}
                        </ul>
                      )}
                    </div>
                  </div>

                  <div className="flex items-center gap-2 md:justify-end">
                    <button
                      type="button"
                      onClick={() => void refetch()}
                      disabled={loading}
                      className="botao-suave w-full md:w-auto"
                      aria-label="Atualizar dados do jardim"
                    >
                      <RefreshCw
                        className={`h-4 w-4 ${loading ? 'animate-spin' : ''}`}
                        aria-hidden="true"
                      />
                      {loading ? 'Atualizando…' : 'Atualizar'}
                    </button>
                  </div>
                </div>

                <div className="mt-3 flex flex-wrap items-center justify-between gap-2 border-t border-[var(--cor-borda)] pt-3 text-[11px] text-[var(--cor-texto-suave)]">
                  <span>
                    Jardim selecionado:{' '}
                    <strong className="font-semibold text-[var(--cor-texto)]">
                      {jardimSelecionado}
                    </strong>
                  </span>
                  <span>
                    {dataAtualizacao !== '--'
                      ? `Última leitura em ${dataAtualizacao} às ${horaAtualizacao}`
                      : 'Aguardando a primeira leitura'}
                  </span>
                </div>
              </section>

              <div className="grid gap-6 lg:grid-cols-12">
                <div className="lg:col-span-5">
                  <StatusCard
                    jardim={jardim}
                    alerta={alerta}
                    loading={verificandoConexao}
                    onTentarNovamente={() => void refetch()}
                  />
                </div>
                <div className="lg:col-span-7">
                  <DadosJardimCard jardim={jardim} loading={verificandoConexao} />
                </div>
              </div>

              <div className="mt-6">
                <HistoricoChart
                  historico={historico}
                  jardimId={jardimSelecionado}
                  loading={verificandoConexao}
                />
              </div>
            </>
          ) : (
            <>
              <div className="mb-5 flex flex-col gap-4 xl:flex-row xl:items-end xl:justify-between">
                <div>
                  <p className="kicker">Painel COP</p>
                  <h2 className="mt-1 text-2xl sm:text-[30px]">Centro de Operações</h2>
                  <p className="mt-1 max-w-2xl text-sm text-[var(--cor-texto-suave)]">
                    Estágio operacional, chuvas acumuladas e abrigos disponíveis para a rede
                    de jardins de chuva.
                  </p>
                </div>
                <div className="flex flex-wrap items-center gap-2">
                  <BadgeConexao conectado={conectado} verificando={verificandoConexao} />
                </div>
              </div>

              <PainelCOP cop={cop} />
            </>
          )}
        </div>

        <footer className="mt-12 flex flex-col gap-2 border-t border-[var(--cor-borda)] pt-6 text-[11px] text-[var(--cor-texto-suave)] sm:flex-row sm:items-center sm:justify-between">
          <p className="inline-flex items-center gap-2">
            <Flower2 className="h-4 w-4 text-jardin-600 dark:text-jardin-300" aria-hidden="true" />
            Jardim Sentinela · Rede de Jardins Monitorados
          </p>
          <p>Atualização automática a cada 5 segundos · Dados sujeitos a latência do sensor</p>
        </footer>
      </main>
    </div>
  )
}

export default App

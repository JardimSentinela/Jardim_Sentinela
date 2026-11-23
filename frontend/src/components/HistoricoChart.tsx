import { useEffect, useRef, useState } from 'react'
import {
  Bar,
  BarChart,
  CartesianGrid,
  Cell,
  LabelList,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'
import { ArrowUp, Check, ChevronDown, Download } from 'lucide-react'
import RotuloBarra from './RotuloBarra'

interface HistoricoChartProps {
  jardimId: string
  loading?: boolean
  className?: string
  /** Cor do estado atual, usada para destacar o mês corrente. */
  corDestaque?: string
}

interface PontoHistorico {
  mes: string
  nivelCm: number
}

const PERIODOS = [
  {
    id: 'semestre-1',
    rotulo: '1º Semestre',
    dados: [
      { mes: 'Janeiro', nivelCm: 25 },
      { mes: 'Fevereiro', nivelCm: 50 },
      { mes: 'Março', nivelCm: 80 },
      { mes: 'Abril', nivelCm: 100 },
      { mes: 'Maio', nivelCm: 70 },
      { mes: 'Junho', nivelCm: 90 },
      { mes: 'Julho', nivelCm: 60 },
    ],
  },
  {
    id: 'semestre-2',
    rotulo: '2º Semestre',
    dados: [
      { mes: 'Agosto', nivelCm: 45 },
      { mes: 'Setembro', nivelCm: 65 },
      { mes: 'Outubro', nivelCm: 85 },
      { mes: 'Novembro', nivelCm: 55 },
      { mes: 'Dezembro', nivelCm: 75 },
    ],
  },
  {
    id: 'ano',
    rotulo: 'Ano completo',
    dados: [
      { mes: 'Janeiro', nivelCm: 25 },
      { mes: 'Fevereiro', nivelCm: 50 },
      { mes: 'Março', nivelCm: 80 },
      { mes: 'Abril', nivelCm: 100 },
      { mes: 'Maio', nivelCm: 70 },
      { mes: 'Junho', nivelCm: 90 },
      { mes: 'Julho', nivelCm: 60 },
      { mes: 'Agosto', nivelCm: 45 },
      { mes: 'Setembro', nivelCm: 65 },
      { mes: 'Outubro', nivelCm: 85 },
      { mes: 'Novembro', nivelCm: 55 },
      { mes: 'Dezembro', nivelCm: 75 },
    ],
  },
] as const

type PeriodoId = (typeof PERIODOS)[number]['id']

function escaparCelulaCsv(valor: string | number): string {
  return `"${String(valor).replace(/"/g, '""')}"`
}

export default function HistoricoChart({
  jardimId,
  loading = false,
  className = '',
  corDestaque = '#005b7f',
}: HistoricoChartProps) {
  const [periodo, setPeriodo] = useState<PeriodoId>('semestre-1')
  const [menuAberto, setMenuAberto] = useState(false)
  const menuRef = useRef<HTMLDivElement>(null)

  const periodoAtual = PERIODOS.find((opcao) => opcao.id === periodo) ?? PERIODOS[0]
  const pontos: ReadonlyArray<PontoHistorico> = periodoAtual.dados
  const larguraMinima = Math.max(620, pontos.length * 80)

  useEffect(() => {
    function aoClicarFora(evento: MouseEvent) {
      if (!menuRef.current?.contains(evento.target as Node)) {
        setMenuAberto(false)
      }
    }

    document.addEventListener('mousedown', aoClicarFora)
    return () => document.removeEventListener('mousedown', aoClicarFora)
  }, [])

  function baixarHistorico() {
    const cabecalho = ['mes', 'nivel_cm']
    const linhas = pontos.map((ponto) =>
      [ponto.mes, ponto.nivelCm].map(escaparCelulaCsv).join(';'),
    )
    const csv = `\uFEFF${[cabecalho.map(escaparCelulaCsv).join(';'), ...linhas].join('\r\n')}`
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' })
    const url = URL.createObjectURL(blob)
    const link = document.createElement('a')
    link.href = url
    link.download = `historico-${jardimId}-${periodo}.csv`
    document.body.appendChild(link)
    link.click()
    document.body.removeChild(link)
    URL.revokeObjectURL(url)
  }

  if (loading) {
    return (
      <section className={`card p-4 sm:p-6 ${className}`}>
        <div className="flex items-center justify-between gap-4">
          <div className="h-6 w-80 max-w-full animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
          <div className="h-9 w-44 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
        </div>
        <div className="mt-6 h-[320px] animate-pulse rounded-xl bg-[var(--cor-superficie-forte)]/60" />
      </section>
    )
  }

  return (
    <section
      className={`card p-4 sm:p-6 ${className}`}
      aria-labelledby="titulo-historico"
    >
      <div className="flex flex-col gap-4 sm:flex-row sm:items-center sm:justify-between">
        <h2 id="titulo-historico" className="text-xl font-bold">
          Histórico de Variação do Nível da Água
        </h2>

        <div className="flex flex-wrap items-center gap-2">
          <div className="relative" ref={menuRef}>
            <button
              type="button"
              onClick={() => setMenuAberto((aberto) => !aberto)}
              aria-haspopup="listbox"
              aria-expanded={menuAberto}
              className="inline-flex items-center gap-2 rounded-full bg-[#071d41] px-4 py-2 text-sm font-semibold text-white transition-colors hover:bg-[#0d2a5c] dark:bg-white dark:text-[#071d41] dark:hover:bg-[#e6f7ff]"
            >
              {periodoAtual.rotulo}
              <ChevronDown
                className={`h-4 w-4 transition-transform ${menuAberto ? 'rotate-180' : ''}`}
                aria-hidden="true"
              />
            </button>

            {menuAberto && (
              <ul
                role="listbox"
                aria-label="Selecionar período"
                className="absolute right-0 z-30 mt-2 w-48 overflow-hidden rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie)] p-1 shadow-lg"
              >
                {PERIODOS.map((opcao) => {
                  const selecionado = opcao.id === periodo
                  return (
                    <li key={opcao.id}>
                      <button
                        type="button"
                        role="option"
                        aria-selected={selecionado}
                        onClick={() => {
                          setPeriodo(opcao.id)
                          setMenuAberto(false)
                        }}
                        className={`flex w-full items-center justify-between rounded-lg px-3 py-2 text-left text-sm transition-colors ${
                          selecionado
                            ? 'bg-[var(--cor-superficie-suave)] font-semibold text-[var(--cor-texto-forte)]'
                            : 'text-[var(--cor-texto)] hover:bg-[var(--cor-superficie-suave)]'
                        }`}
                      >
                        {opcao.rotulo}
                        {selecionado && (
                          <Check className="h-4 w-4" aria-hidden="true" />
                        )}
                      </button>
                    </li>
                  )
                })}
              </ul>
            )}
          </div>

          <button
            type="button"
            onClick={baixarHistorico}
            className="inline-flex items-center gap-2 rounded-full bg-[#ededed] px-4 py-2 text-sm font-semibold text-[#071d41] transition-colors hover:bg-[#e2e2e2] dark:bg-white/10 dark:text-white dark:hover:bg-white/15"
          >
            <Download className="h-4 w-4" aria-hidden="true" />
            Baixar Histórico
          </button>
        </div>
      </div>

      <div className="mt-6 overflow-x-auto pb-1">
        <div className="relative" style={{ minWidth: `${larguraMinima}px` }}>
          <ArrowUp
            className="pointer-events-none absolute left-[64px] top-3 h-3.5 w-3.5 -translate-x-1/2 text-[var(--cor-texto-suave)]"
            aria-hidden="true"
          />
          <div className="h-[320px]">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart
                data={[...pontos]}
                margin={{ top: 28, right: 8, left: 0, bottom: 0 }}
                barCategoryGap="28%"
              >
                <CartesianGrid
                  strokeDasharray="3 6"
                  vertical={false}
                  stroke="var(--cor-borda)"
                />
                <XAxis
                  dataKey="mes"
                  tickLine={false}
                  axisLine={{ stroke: 'var(--cor-borda)' }}
                  tick={{ fill: 'var(--cor-texto-suave)', fontSize: 11 }}
                  interval={0}
                />
                <YAxis
                  width={64}
                  domain={[0, 100]}
                  ticks={[0, 25, 50, 80, 100]}
                  tickLine={false}
                  axisLine={{ stroke: 'var(--cor-borda)' }}
                  tick={{ fill: 'var(--cor-texto-suave)', fontSize: 11 }}
                  tickFormatter={(valor) => `${valor} cm`}
                />
                <Tooltip
                  cursor={{ fill: 'rgba(76, 199, 251, 0.10)' }}
                  content={({ active, payload }) => {
                    if (!active || !payload?.length) {
                      return null
                    }
                    const ponto = payload[0]?.payload as PontoHistorico | undefined
                    if (!ponto) {
                      return null
                    }
                    return (
                      <div className="rounded-lg border border-[var(--cor-borda)] bg-[var(--cor-superficie)] px-3 py-2 shadow-lg">
                        <p className="text-xs font-medium text-[var(--cor-texto-suave)]">
                          {ponto.mes}
                        </p>
                        <p className="mt-0.5 text-sm font-semibold text-[var(--cor-texto-forte)]">
                          {ponto.nivelCm} cm
                        </p>
                      </div>
                    )
                  }}
                />
                <Bar
                  dataKey="nivelCm"
                  name="Nível da água"
                  radius={[4, 4, 0, 0]}
                  maxBarSize={56}
                  activeBar={{ fill: corDestaque }}
                  isAnimationActive={false}
                >
                  <LabelList
                    dataKey="nivelCm"
                    content={<RotuloBarra sufixo="cm" />}
                  />
                  {pontos.map((ponto, indice) => (
                    <Cell
                      key={ponto.mes}
                      fill={
                        indice === pontos.length - 1
                          ? corDestaque
                          : 'var(--cor-barra-neutra)'
                      }
                    />
                  ))}
                </Bar>
              </BarChart>
            </ResponsiveContainer>
          </div>
        </div>
      </div>
    </section>
  )
}

import { useEffect, useMemo, useRef, useState } from 'react'
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
import {
  CalendarRange,
  Check,
  ChevronDown,
  Download,
  LineChart,
} from 'lucide-react'
import { NIVEL_MAXIMO_JARDIM_CM } from '../types/jardim'
import type { Leitura } from '../types/jardim'

interface HistoricoChartProps {
  historico: Leitura[]
  jardimId: string
  loading?: boolean
  className?: string
}

const PERIODOS = [
  {
    id: 'semestre-1',
    rotulo: '1º Semestre',
    descricao: 'Primeiro ciclo monitorado',
  },
  {
    id: 'semestre-2',
    rotulo: '2º Semestre',
    descricao: 'Ciclo mais recente',
  },
  {
    id: 'ano',
    rotulo: 'Ano completo',
    descricao: 'Todo o histórico carregado',
  },
] as const

type PeriodoId = (typeof PERIODOS)[number]['id']

interface PontoHistorico {
  id: number
  jardimId: string
  sensorId: string
  nivelCm: number
  bateria: number
  recebidoEm: string
  hora: string
  data: string
  rotuloEixo: string
}

function formatarDecimal(valor: number, casas = 1): string {
  return valor.toLocaleString('pt-BR', {
    minimumFractionDigits: casas,
    maximumFractionDigits: casas,
  })
}

function formatarDataCurta(data: Date): string {
  return data.toLocaleDateString('pt-BR', {
    day: '2-digit',
    month: '2-digit',
  })
}

function formatarHora(data: Date): string {
  return data.toLocaleTimeString('pt-BR', {
    hour: '2-digit',
    minute: '2-digit',
  })
}

/**
 * Enquanto o histórico carregado cobre apenas as leituras recentes, os
 * semestres funcionam como recortes relativos da série: o 1º usa a metade
 * mais antiga e o 2º a metade mais recente. Assim o seletor continua
 * respondendo de imediato, mesmo com poucas leituras em memória.
 */
function recortarPorPeriodo(leituras: Leitura[], periodo: PeriodoId): Leitura[] {
  const cronologico = [...leituras].reverse()

  if (periodo === 'ano' || cronologico.length < 4) {
    return cronologico
  }

  const meio = Math.ceil(cronologico.length / 2)
  return periodo === 'semestre-1'
    ? cronologico.slice(0, meio)
    : cronologico.slice(Math.floor(cronologico.length / 2))
}

function escaparCelulaCsv(valor: string | number): string {
  return `"${String(valor).replace(/"/g, '""')}"`
}

export default function HistoricoChart({
  historico,
  jardimId,
  loading = false,
  className = '',
}: HistoricoChartProps) {
  const [periodo, setPeriodo] = useState<PeriodoId>('semestre-1')
  const [menuAberto, setMenuAberto] = useState(false)
  const [exportado, setExportado] = useState(false)
  const menuRef = useRef<HTMLDivElement>(null)

  const periodoAtual =
    PERIODOS.find((opcao) => opcao.id === periodo) ?? PERIODOS[0]

  const pontos = useMemo<PontoHistorico[]>(() => {
    return recortarPorPeriodo(historico, periodo).map((leitura) => {
      const dataLeitura = new Date(leitura.recebidoEm)
      const dataValida = !Number.isNaN(dataLeitura.getTime())
      const hora = dataValida ? formatarHora(dataLeitura) : '--:--'

      return {
        id: leitura.id,
        jardimId: leitura.jardimId,
        sensorId: leitura.sensorId,
        nivelCm: leitura.nivelCm,
        bateria: leitura.bateria,
        recebidoEm: leitura.recebidoEm,
        hora,
        data: dataValida ? formatarDataCurta(dataLeitura) : '--/--',
        rotuloEixo: hora,
      }
    })
  }, [historico, periodo])

  const maiorNivel = pontos.reduce(
    (maior, ponto) => Math.max(maior, ponto.nivelCm),
    0,
  )
  const limiteEixo = Math.max(
    NIVEL_MAXIMO_JARDIM_CM,
    Math.ceil(maiorNivel / 10) * 10,
  )

  useEffect(() => {
    function aoClicarFora(evento: MouseEvent) {
      if (!menuRef.current?.contains(evento.target as Node)) {
        setMenuAberto(false)
      }
    }

    document.addEventListener('mousedown', aoClicarFora)
    return () => document.removeEventListener('mousedown', aoClicarFora)
  }, [])

  useEffect(() => {
    if (!exportado) {
      return
    }
    const temporizador = globalThis.setTimeout(() => setExportado(false), 2400)
    return () => globalThis.clearTimeout(temporizador)
  }, [exportado])

  function baixarHistorico() {
    if (!pontos.length) {
      return
    }

    const cabecalho = [
      'id',
      'jardim_id',
      'sensor_id',
      'nivel_cm',
      'bateria',
      'recebido_em',
    ]

    const linhas = pontos.map((ponto) =>
      [
        ponto.id,
        ponto.jardimId,
        ponto.sensorId,
        formatarDecimal(ponto.nivelCm, 1),
        ponto.bateria,
        ponto.recebidoEm,
      ]
        .map(escaparCelulaCsv)
        .join(';'),
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
    setExportado(true)
  }

  const intervaloTexto =
    pontos.length > 0
      ? `${pontos[0].data} ${pontos[0].hora} — ${pontos[pontos.length - 1].data} ${
          pontos[pontos.length - 1].hora
        }`
      : 'Sem leituras no período'

  if (loading) {
    return (
      <section className={`card p-5 sm:p-6 ${className}`}>
        <div className="flex items-center justify-between gap-4">
          <div className="space-y-2">
            <div className="h-3 w-20 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
            <div className="h-6 w-80 max-w-full animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
          </div>
          <div className="h-9 w-36 animate-pulse rounded-xl bg-[var(--cor-superficie-forte)]" />
        </div>
        <div className="mt-6 h-[300px] animate-pulse rounded-xl bg-[var(--cor-superficie-forte)]/70" />
      </section>
    )
  }

  return (
    <section
      className={`card flex flex-col p-5 sm:p-6 ${className}`}
      aria-labelledby="titulo-historico"
    >
      <div className="flex flex-col gap-4 border-b border-[var(--cor-borda)] pb-5 lg:flex-row lg:items-start lg:justify-between">
        <div>
          <p className="kicker">Histórico</p>
          <h2 id="titulo-historico" className="mt-1.5 text-xl sm:text-[22px]">
            Histórico de Variação do Nível da Água
          </h2>
          <p className="mt-1 text-[13px] text-[var(--cor-texto-suave)]">
            Leituras do sensor em centímetros. Passe o mouse sobre as barras para ver os
            detalhes.
          </p>
        </div>

        <div className="flex flex-wrap items-center gap-2">
          <div className="relative" ref={menuRef}>
            <button
              type="button"
              onClick={() => setMenuAberto((aberto) => !aberto)}
              aria-haspopup="listbox"
              aria-expanded={menuAberto}
              className="botao-suave min-w-[150px] justify-between"
            >
              <span className="inline-flex items-center gap-2">
                <CalendarRange className="h-4 w-4 text-jardin-600 dark:text-jardin-300" aria-hidden="true" />
                {periodoAtual.rotulo}
              </span>
              <ChevronDown
                className={`h-4 w-4 text-[var(--cor-texto-suave)] transition-transform ${
                  menuAberto ? 'rotate-180' : ''
                }`}
                aria-hidden="true"
              />
            </button>

            {menuAberto && (
              <ul
                role="listbox"
                aria-label="Selecionar período"
                className="sombra-flutuante absolute right-0 z-30 mt-2 w-60 overflow-hidden rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie)] p-1"
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
                        className={`flex w-full items-start gap-2 rounded-lg px-3 py-2 text-left transition-colors ${
                          selecionado
                            ? 'bg-jardin-50 dark:bg-jardin-950/40'
                            : 'hover:bg-[var(--cor-superficie-suave)]'
                        }`}
                      >
                        <span className="mt-0.5 flex h-4 w-4 shrink-0 items-center justify-center">
                          {selecionado && (
                            <Check
                              className="h-4 w-4 text-jardin-600 dark:text-jardin-300"
                              aria-hidden="true"
                            />
                          )}
                        </span>
                        <span>
                          <span
                            className={`block text-sm font-medium ${
                              selecionado
                                ? 'text-jardin-700 dark:text-jardin-300'
                                : 'text-[var(--cor-texto)]'
                            }`}
                          >
                            {opcao.rotulo}
                          </span>
                          <span className="block text-[11px] text-[var(--cor-texto-suave)]">
                            {opcao.descricao}
                          </span>
                        </span>
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
            disabled={!pontos.length}
            className="inline-flex items-center justify-center gap-2 rounded-xl bg-jardin-600 px-3.5 py-2 text-sm font-semibold text-white shadow-sm shadow-jardin-900/20 transition-all hover:bg-jardin-700 disabled:cursor-not-allowed disabled:opacity-50 dark:bg-jardin-500 dark:text-jardin-950 dark:hover:bg-jardin-400"
          >
            {exportado ? (
              <Check className="h-4 w-4" aria-hidden="true" />
            ) : (
              <Download className="h-4 w-4" aria-hidden="true" />
            )}
            {exportado ? 'CSV baixado' : 'Baixar Histórico'}
          </button>
        </div>
      </div>

      {pontos.length === 0 ? (
        <div className="flex h-[300px] flex-col items-center justify-center gap-2 text-center">
          <span className="flex h-12 w-12 items-center justify-center rounded-2xl bg-[var(--cor-superficie-suave)] text-[var(--cor-texto-suave)]">
            <LineChart className="h-6 w-6" aria-hidden="true" />
          </span>
          <p className="text-sm font-medium">Nenhuma leitura no período selecionado</p>
          <p className="max-w-sm text-xs text-[var(--cor-texto-suave)]">
            Escolha outro período ou aguarde a próxima atualização automática do sensor.
          </p>
        </div>
      ) : (
        <div className="mt-5 h-[300px] w-full">
          <ResponsiveContainer width="100%" height="100%">
            <BarChart
              data={pontos}
              margin={{ top: 26, right: 8, left: -14, bottom: 0 }}
              barCategoryGap="26%"
            >
              <CartesianGrid
                strokeDasharray="3 6"
                vertical={false}
                stroke="var(--cor-borda)"
              />
              <XAxis
                dataKey="rotuloEixo"
                tickLine={false}
                axisLine={{ stroke: 'var(--cor-borda)' }}
                tick={{ fill: 'var(--cor-texto-suave)', fontSize: 11 }}
                minTickGap={22}
                interval="preserveStartEnd"
              />
              <YAxis
                tickLine={false}
                axisLine={false}
                width={46}
                domain={[0, limiteEixo]}
                ticks={Array.from(
                  { length: limiteEixo / 10 + 1 },
                  (_, indice) => indice * 10,
                )}
                tick={{ fill: 'var(--cor-texto-suave)', fontSize: 11 }}
                tickFormatter={(valor) => String(valor)}
                label={{
                  value: 'cm',
                  angle: -90,
                  position: 'insideLeft',
                  offset: 18,
                  fill: 'var(--cor-texto-suave)',
                  fontSize: 11,
                }}
              />
              <Tooltip
                cursor={{ fill: 'rgba(20, 184, 166, 0.08)' }}
                content={({ active, payload }) => {
                  if (!active || !payload?.length) {
                    return null
                  }

                  const ponto = payload[0]?.payload as PontoHistorico | undefined
                  if (!ponto) {
                    return null
                  }

                  return (
                    <div className="rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie)] px-3.5 py-2.5 shadow-xl">
                      <p className="text-[10px] font-semibold uppercase tracking-[0.12em] text-[var(--cor-texto-suave)]">
                        {ponto.data} · {ponto.hora}
                      </p>
                      <p className="mt-1 text-sm font-semibold text-[var(--cor-texto-forte)]">
                        {formatarDecimal(ponto.nivelCm, 1)} cm
                      </p>
                      <p className="mt-0.5 text-[11px] text-[var(--cor-texto-suave)]">
                        Bateria {formatarDecimal(ponto.bateria, 0)}%
                      </p>
                    </div>
                  )
                }}
              />
              <Bar
                dataKey="nivelCm"
                name="Nível da água"
                radius={[6, 6, 0, 0]}
                maxBarSize={38}
                activeBar={{ fill: '#0f766e' }}
                isAnimationActive={false}
              >
                <LabelList
                  dataKey="nivelCm"
                  position="top"
                  fill="var(--cor-texto-suave)"
                  fontSize={10}
                  formatter={(valor) => formatarDecimal(Number(valor), 1)}
                />
                {pontos.map((ponto, indice) => {
                  const ultimo = indice === pontos.length - 1
                  return (
                    <Cell
                      key={`${ponto.id}-${ponto.recebidoEm}`}
                      fill={ultimo ? '#0d9488' : '#5eead4'}
                    />
                  )
                })}
              </Bar>
            </BarChart>
          </ResponsiveContainer>
        </div>
      )}

      <div className="mt-4 flex flex-wrap items-center justify-between gap-2 border-t border-[var(--cor-borda)] pt-3.5 text-[11px] text-[var(--cor-texto-suave)]">
        <span className="inline-flex items-center gap-1.5">
          <span className="h-2 w-2 rounded-full bg-teal-500" aria-hidden="true" />
          Nível da água (cm)
          <span className="ml-2 h-2 w-2 rounded-full bg-teal-300" aria-hidden="true" />
          Barra mais recente em destaque
        </span>
        <span className="font-medium">
          {pontos.length} leitura{pontos.length === 1 ? '' : 's'} · {intervaloTexto}
        </span>
      </div>
    </section>
  )
}

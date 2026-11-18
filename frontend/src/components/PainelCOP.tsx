import { useMemo } from 'react'
import {
  Area,
  AreaChart,
  CartesianGrid,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'
import {
  Activity,
  AlertTriangle,
  CalendarDays,
  Clock3,
  CloudRain,
  CloudRainWind,
  House,
  ShieldCheck,
} from 'lucide-react'
import type { DadosCOP, NivelAlerta } from '../types/jardim'

interface PainelCOPProps {
  cop: DadosCOP
  className?: string
}

interface EstiloEstagio {
  texto: string
  fundo: string
  borda: string
  icone: string
  segmento: string
}

const ESTAGIOS: Record<NivelAlerta, EstiloEstagio> = {
  NORMAL: {
    texto: 'text-emerald-700 dark:text-emerald-300',
    fundo:
      'from-emerald-50 via-white to-white dark:from-emerald-950/40 dark:via-slate-900 dark:to-slate-900',
    borda: 'border-emerald-200/80 dark:border-emerald-900/60',
    icone:
      'bg-emerald-100 text-emerald-700 ring-emerald-200 dark:bg-emerald-950/60 dark:text-emerald-300 dark:ring-emerald-900',
    segmento: 'bg-emerald-500',
  },
  ATENCAO: {
    texto: 'text-amber-700 dark:text-amber-300',
    fundo:
      'from-amber-50 via-white to-white dark:from-amber-950/40 dark:via-slate-900 dark:to-slate-900',
    borda: 'border-amber-200/80 dark:border-amber-900/60',
    icone:
      'bg-amber-100 text-amber-700 ring-amber-200 dark:bg-amber-950/60 dark:text-amber-300 dark:ring-amber-900',
    segmento: 'bg-amber-500',
  },
  TRANSBORDANDO: {
    texto: 'text-cyan-700 dark:text-cyan-300',
    fundo:
      'from-cyan-50 via-white to-white dark:from-cyan-950/40 dark:via-slate-900 dark:to-slate-900',
    borda: 'border-cyan-200/80 dark:border-cyan-900/60',
    icone:
      'bg-cyan-100 text-cyan-700 ring-cyan-200 dark:bg-cyan-950/60 dark:text-cyan-300 dark:ring-cyan-900',
    segmento: 'bg-cyan-500',
  },
  ALAGADO: {
    texto: 'text-red-700 dark:text-red-300',
    fundo:
      'from-red-50 via-white to-white dark:from-red-950/40 dark:via-slate-900 dark:to-slate-900',
    borda: 'border-red-200/80 dark:border-red-900/60',
    icone:
      'bg-red-100 text-red-700 ring-red-200 dark:bg-red-950/60 dark:text-red-300 dark:ring-red-900',
    segmento: 'bg-red-500',
  },
}

const ORDEM_ESTAGIOS: Array<{ nivel: NivelAlerta; rotulo: string }> = [
  { nivel: 'NORMAL', rotulo: 'Normal' },
  { nivel: 'ATENCAO', rotulo: 'Atenção' },
  { nivel: 'TRANSBORDANDO', rotulo: 'Transbordando' },
  { nivel: 'ALAGADO', rotulo: 'Alagado' },
]

const HORARIOS_CHUVA = [
  '00h',
  '02h',
  '04h',
  '06h',
  '08h',
  '10h',
  '12h',
  '14h',
  '16h',
  '18h',
  '20h',
  '22h',
]

/** Série horária de demonstração que soma os 78,6 mm acumulados em 24h. */
const VALORES_CHUVA_MM = [
  1.2, 2.4, 4.8, 7.6, 11.2, 8.4, 6.2, 9.8, 12.4, 6.1, 5.4, 3.1,
]

function formatarDecimal(valor: number, casas = 1): string {
  return valor.toLocaleString('pt-BR', {
    minimumFractionDigits: casas,
    maximumFractionDigits: casas,
  })
}

function formatarDataHoraCurta(isoString: string): string {
  const data = new Date(isoString)
  if (Number.isNaN(data.getTime())) {
    return '--'
  }
  const dia = String(data.getDate()).padStart(2, '0')
  const mes = String(data.getMonth() + 1).padStart(2, '0')
  const hora = String(data.getHours()).padStart(2, '0')
  const minuto = String(data.getMinutes()).padStart(2, '0')
  return `${dia}/${mes} às ${hora}:${minuto}`
}

function MetricaRapida({
  icone: Icone,
  rotulo,
  valor,
  unidade,
  detalhe,
  corIcone,
  selo,
}: {
  icone: typeof CloudRain
  rotulo: string
  valor: string
  unidade: string
  detalhe: string
  corIcone: string
  selo: string
}) {
  return (
    <article className="card flex h-full flex-col p-4 sm:p-5">
      <div className="flex items-start justify-between gap-3">
        <span
          className={`flex h-10 w-10 items-center justify-center rounded-xl ring-1 ${corIcone}`}
        >
          <Icone className="h-5 w-5" aria-hidden="true" />
        </span>
        <span className="rounded-full border border-[var(--cor-borda)] bg-[var(--cor-superficie-suave)] px-2 py-0.5 text-[10px] font-semibold uppercase tracking-[0.1em] text-[var(--cor-texto-suave)]">
          {selo}
        </span>
      </div>
      <p className="mt-3 text-xs font-semibold uppercase tracking-[0.1em] text-[var(--cor-texto-suave)]">
        {rotulo}
      </p>
      <p className="mt-1 flex items-baseline gap-1.5">
        <span className="text-2xl font-semibold tabular-nums text-[var(--cor-texto-forte)] sm:text-[28px]">
          {valor}
        </span>
        <span className="text-xs font-medium text-[var(--cor-texto-suave)]">{unidade}</span>
      </p>
      <p className="mt-auto pt-3 text-[11px] text-[var(--cor-texto-suave)]">{detalhe}</p>
    </article>
  )
}

export default function PainelCOP({ cop, className = '' }: PainelCOPProps) {
  const estilo = ESTAGIOS[cop.estagio]

  const dadosChuva = useMemo(
    () =>
      HORARIOS_CHUVA.map((horario, indice) => ({
        horario,
        chuvaMm: VALORES_CHUVA_MM[indice],
      })),
    [],
  )

  const picoChuva = Math.max(...VALORES_CHUVA_MM)

  return (
    <div className={`space-y-6 ${className}`}>
      {/* Estágio operacional */}
      <section
        className={`relative overflow-hidden rounded-2xl border bg-gradient-to-br p-5 shadow-sm sm:p-6 ${estilo.borda} ${estilo.fundo}`}
        aria-labelledby="titulo-estagio-cop"
      >
        <div
          aria-hidden="true"
          className="pointer-events-none absolute -right-20 -top-24 h-60 w-60 rounded-full bg-white/40 blur-3xl dark:bg-white/5"
        />

        <div className="relative flex flex-col gap-6 lg:flex-row lg:items-center lg:justify-between">
          <div className="flex items-start gap-4">
            <span
              className={`flex h-12 w-12 shrink-0 items-center justify-center rounded-2xl ring-1 ${estilo.icone}`}
            >
              <ShieldCheck className="h-6 w-6" aria-hidden="true" />
            </span>
            <div>
              <p className="kicker">Painel COP</p>
              <h2 id="titulo-estagio-cop" className="mt-1.5 text-xl sm:text-2xl">
                Estágio Operacional Registrado
              </h2>
              <p className="mt-1.5 max-w-xl text-[13px] leading-relaxed text-[var(--cor-texto-suave)]">
                Situação consolidada do Centro de Operações para a rede de jardins de chuva.
                O acompanhamento é contínuo e considera chuva acumulada, picos e abrigos
                disponíveis.
              </p>
              <p className="mt-3 flex flex-wrap items-center gap-x-4 gap-y-1 text-[11px] text-[var(--cor-texto-suave)]">
                <span className="inline-flex items-center gap-1.5">
                  <CalendarDays className="h-3.5 w-3.5" aria-hidden="true" />
                  Última atualização em {formatarDataHoraCurta(cop.ultimaAtualizacao)}
                </span>
                <span className="inline-flex items-center gap-1.5">
                  <Activity className="h-3.5 w-3.5" aria-hidden="true" />
                  Monitoramento em tempo real
                </span>
              </p>
            </div>
          </div>

          <div className="flex shrink-0 items-center gap-4 rounded-2xl border border-[var(--cor-borda)] bg-[var(--cor-superficie)]/80 px-5 py-4 backdrop-blur-sm">
            <span className="flex h-10 w-10 items-center justify-center rounded-full bg-[var(--cor-superficie-suave)]">
              <AlertTriangle className={`h-5 w-5 ${estilo.texto}`} aria-hidden="true" />
            </span>
            <div>
              <p className="text-[10px] font-semibold uppercase tracking-[0.14em] text-[var(--cor-texto-suave)]">
                Estágio COP
              </p>
              <p className={`mt-0.5 flex items-center gap-2 text-xl font-bold ${estilo.texto}`}>
                <span className="relative flex h-2.5 w-2.5">
                  <span
                    className={`absolute inline-flex h-full w-full animate-ping rounded-full opacity-60 ${estilo.segmento}`}
                  />
                  <span
                    className={`relative inline-flex h-2.5 w-2.5 rounded-full ${estilo.segmento}`}
                  />
                </span>
                {cop.estagioTexto}
              </p>
              <p className="text-[10px] text-[var(--cor-texto-suave)]">Centro de Operações</p>
            </div>
          </div>
        </div>

        <div className="relative mt-6 grid grid-cols-4 gap-2 border-t border-[var(--cor-borda)]/70 pt-4">
          {ORDEM_ESTAGIOS.map((estagio) => {
            const ativo = estagio.nivel === cop.estagio
            return (
              <div key={estagio.nivel} className="space-y-1.5">
                <span
                  className={`block h-1.5 rounded-full ${
                    ativo ? estilo.segmento : 'bg-[var(--cor-borda)]'
                  }`}
                />
                <span
                  className={`block text-[10px] font-medium sm:text-[11px] ${
                    ativo
                      ? 'text-[var(--cor-texto-forte)]'
                      : 'text-[var(--cor-texto-suave)]'
                  }`}
                >
                  {estagio.rotulo}
                </span>
              </div>
            )
          })}
        </div>
      </section>

      {/* Métricas rápidas */}
      <div className="grid gap-4 sm:grid-cols-3">
        <MetricaRapida
          icone={CloudRain}
          rotulo="Chuva Últimas 24h"
          valor={formatarDecimal(cop.chuva24hMm, 1)}
          unidade="mm"
          detalhe="Volume acumulado na área monitorada."
          corIcone="bg-agua-50 text-agua-600 ring-agua-100 dark:bg-agua-950/40 dark:text-agua-300 dark:ring-agua-900/60"
          selo="24h"
        />
        <MetricaRapida
          icone={CloudRainWind}
          rotulo="Chuva Máx. Últimas 24h"
          valor={formatarDecimal(cop.chuvaMax24hMm, 1)}
          unidade="mm"
          detalhe="Maior registro entre os pontos da rede."
          corIcone="bg-cyan-50 text-cyan-600 ring-cyan-100 dark:bg-cyan-950/40 dark:text-cyan-300 dark:ring-cyan-900/60"
          selo="24h"
        />
        <MetricaRapida
          icone={House}
          rotulo="Abrigos"
          valor={String(cop.abrigosAtivos)}
          unidade="Ativos"
          detalhe="Estruturas de apoio prontas para acionamento."
          corIcone="bg-jardin-50 text-jardin-600 ring-jardin-100 dark:bg-jardin-950/40 dark:text-jardin-300 dark:ring-jardin-900/60"
          selo="COP"
        />
      </div>

      {/* Gráfico de chuvas */}
      <section className="card p-5 sm:p-6" aria-labelledby="titulo-chuvas">
        <div className="flex flex-col gap-4 border-b border-[var(--cor-borda)] pb-5 lg:flex-row lg:items-start lg:justify-between">
          <div>
            <p className="kicker">Precipitação</p>
            <h2 id="titulo-chuvas" className="mt-1.5 text-xl">
              Variação de Chuvas
            </h2>
            <p className="mt-1 text-[13px] text-[var(--cor-texto-suave)]">
              Volume de chuva em milímetros ao longo das últimas 24 horas.
            </p>
          </div>

          <div className="flex flex-wrap items-center gap-2">
            <span className="rounded-lg border border-[var(--cor-borda)] bg-[var(--cor-superficie-suave)] px-2.5 py-1.5 text-[11px] font-medium text-[var(--cor-texto-suave)]">
              Acumulado <strong className="text-[var(--cor-texto-forte)]">{formatarDecimal(cop.chuva24hMm, 1)} mm</strong>
            </span>
            <span className="rounded-lg border border-[var(--cor-borda)] bg-[var(--cor-superficie-suave)] px-2.5 py-1.5 text-[11px] font-medium text-[var(--cor-texto-suave)]">
              Pico <strong className="text-[var(--cor-texto-forte)]">{formatarDecimal(picoChuva, 1)} mm</strong>
            </span>
            <span className="rounded-lg border border-[var(--cor-borda)] bg-[var(--cor-superficie-suave)] px-2.5 py-1.5 text-[11px] font-medium text-[var(--cor-texto-suave)]">
              Máx. 24h{' '}
              <strong className="text-[var(--cor-texto-forte)]">
                {formatarDecimal(cop.chuvaMax24hMm, 1)} mm
              </strong>
            </span>
          </div>
        </div>

        <div className="mt-5 h-[300px] w-full">
          <ResponsiveContainer width="100%" height="100%">
            <AreaChart
              data={dadosChuva}
              margin={{ top: 16, right: 12, left: -16, bottom: 0 }}
            >
              <defs>
                <linearGradient id="gradiente-chuva" x1="0" y1="0" x2="0" y2="1">
                  <stop offset="0%" stopColor="#0ea5e9" stopOpacity={0.38} />
                  <stop offset="100%" stopColor="#0ea5e9" stopOpacity={0.02} />
                </linearGradient>
              </defs>
              <CartesianGrid
                strokeDasharray="3 6"
                vertical={false}
                stroke="var(--cor-borda)"
              />
              <XAxis
                dataKey="horario"
                tickLine={false}
                axisLine={{ stroke: 'var(--cor-borda)' }}
                tick={{ fill: 'var(--cor-texto-suave)', fontSize: 11 }}
                minTickGap={16}
              />
              <YAxis
                tickLine={false}
                axisLine={false}
                width={46}
                domain={[0, 16]}
                ticks={[0, 4, 8, 12, 16]}
                tick={{ fill: 'var(--cor-texto-suave)', fontSize: 11 }}
                tickFormatter={(valor) => String(valor)}
                label={{
                  value: 'mm',
                  angle: -90,
                  position: 'insideLeft',
                  offset: 18,
                  fill: 'var(--cor-texto-suave)',
                  fontSize: 11,
                }}
              />
              <Tooltip
                cursor={{ stroke: 'var(--cor-borda-forte)', strokeDasharray: '4 4' }}
                content={({ active, payload }) => {
                  if (!active || !payload?.length) {
                    return null
                  }
                  const ponto = payload[0]?.payload as
                    | { horario: string; chuvaMm: number }
                    | undefined
                  if (!ponto) {
                    return null
                  }
                  return (
                    <div className="rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie)] px-3.5 py-2.5 shadow-xl">
                      <p className="text-[10px] font-semibold uppercase tracking-[0.12em] text-[var(--cor-texto-suave)]">
                        Às {ponto.horario}
                      </p>
                      <p className="mt-1 text-sm font-semibold text-[var(--cor-texto-forte)]">
                        {formatarDecimal(ponto.chuvaMm, 1)} mm
                      </p>
                    </div>
                  )
                }}
              />
              <Area
                type="monotone"
                dataKey="chuvaMm"
                name="Chuva"
                stroke="#0ea5e9"
                strokeWidth={2.5}
                fill="url(#gradiente-chuva)"
                dot={{ r: 3, fill: '#0ea5e9', strokeWidth: 0 }}
                activeDot={{ r: 5, fill: '#0284c7', strokeWidth: 2, stroke: '#ffffff' }}
                isAnimationActive={false}
              />
            </AreaChart>
          </ResponsiveContainer>
        </div>

        <div className="mt-4 flex flex-wrap items-center justify-between gap-2 border-t border-[var(--cor-borda)] pt-3.5 text-[11px] text-[var(--cor-texto-suave)]">
          <span className="inline-flex items-center gap-1.5">
            <span className="h-2 w-2 rounded-full bg-sky-500" aria-hidden="true" />
            Precipitação horária (mm)
          </span>
          <span className="inline-flex items-center gap-1.5">
            <Clock3 className="h-3.5 w-3.5" aria-hidden="true" />
            Janela móvel de 24 horas
          </span>
        </div>
      </section>
    </div>
  )
}

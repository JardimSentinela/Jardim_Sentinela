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
import { ArrowUp, CloudRain, CloudRainWind, House } from 'lucide-react'
import RotuloBarra from './RotuloBarra'
import { CORES_ESTADO, ESTADOS_ALERTA } from '../types/jardim'
import type { DadosCOP } from '../types/jardim'

interface PainelCOPProps {
  cop: DadosCOP
  className?: string
}

const DADOS_CHUVA = [
  { mes: 'Janeiro', chuvaMm: 22.4 },
  { mes: 'Fevereiro', chuvaMm: 41.8 },
  { mes: 'Março', chuvaMm: 63.2 },
  { mes: 'Abril', chuvaMm: 112.4 },
  { mes: 'Maio', chuvaMm: 74.6 },
  { mes: 'Junho', chuvaMm: 95.2 },
  { mes: 'Julho', chuvaMm: 78.6 },
]

function formatarDecimal(valor: number): string {
  return valor.toLocaleString('pt-BR', {
    minimumFractionDigits: 1,
    maximumFractionDigits: 1,
  })
}

function MetricaRapida({
  icone: Icone,
  rotulo,
  valor,
  unidade,
}: {
  icone: typeof CloudRain
  rotulo: string
  valor: string
  unidade: string
}) {
  return (
    <article className="rounded-2xl border border-[#474656] bg-[#353443] p-4">
      <div className="flex items-center justify-between gap-4">
        <p className="text-sm font-medium text-[#e4b739]">{rotulo}</p>
        <Icone className="h-5 w-5 text-[#e4b739]" aria-hidden="true" />
      </div>
      <p className="mt-4 flex items-baseline gap-2 text-white">
        <span className="font-brand text-3xl font-bold tabular-nums">{valor}</span>
        <span className="text-sm font-medium text-[#e4b739]">{unidade}</span>
      </p>
    </article>
  )
}

export default function PainelCOP({ cop, className = '' }: PainelCOPProps) {
  const corEstagio = CORES_ESTADO[cop.estagio]

  return (
    <div className={`space-y-8 ${className}`}>
      <section
        className="rounded-2xl border p-4 sm:p-6"
        style={{
          backgroundColor: `${corEstagio}1f`,
          borderColor: `${corEstagio}66`,
        }}
      >
        <div className="flex flex-col gap-6 sm:flex-row sm:items-start sm:justify-between">
          <div>
            <h2 className="text-xl font-bold">Estágio Operacional Registrado</h2>
            <p className="mt-4 flex items-center gap-4 font-display text-4xl font-bold">
              <span
                className="h-3 w-3 shrink-0 rounded-full"
                style={{ backgroundColor: corEstagio }}
                aria-hidden="true"
              />
              {cop.estagioTexto}
            </p>
          </div>

          <div
            className="flex w-fit items-center gap-4 rounded-full bg-white px-4 py-3 shadow-sm"
            role="img"
            aria-label={`Estágio COP: ${cop.estagioTexto}`}
          >
            {ESTADOS_ALERTA.map((nivel) => (
              <span
                key={nivel}
                className="h-5 w-5 rounded-full"
                style={{
                  backgroundColor: nivel === cop.estagio ? corEstagio : '#d1d5db',
                }}
              />
            ))}
          </div>
        </div>
      </section>

      <div className="grid gap-4 sm:grid-cols-3">
        <MetricaRapida
          icone={CloudRain}
          rotulo="Chuva Últimas 24h"
          valor={formatarDecimal(cop.chuva24hMm)}
          unidade="mm"
        />
        <MetricaRapida
          icone={CloudRainWind}
          rotulo="Chuva Máx. Últimas 24h"
          valor={formatarDecimal(cop.chuvaMax24hMm)}
          unidade="mm"
        />
        <MetricaRapida
          icone={House}
          rotulo="Abrigos"
          valor={String(cop.abrigosAtivos)}
          unidade="Ativos"
        />
      </div>

      <section className="card p-4 sm:p-6" aria-labelledby="titulo-chuvas">
        <h2 id="titulo-chuvas" className="text-xl font-bold">
          Variação de Chuvas
        </h2>

        <div className="mt-6 overflow-x-auto pb-1">
          <div className="relative min-w-[620px]">
            <ArrowUp
              className="pointer-events-none absolute left-[64px] top-3 h-3.5 w-3.5 -translate-x-1/2 text-[var(--cor-texto-suave)]"
              aria-hidden="true"
            />
            <div className="h-[320px]">
              <ResponsiveContainer width="100%" height="100%">
                <BarChart
                  data={DADOS_CHUVA}
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
                    domain={[0, 125]}
                    ticks={[0, 25, 50, 75, 100, 125]}
                    tickLine={false}
                    axisLine={{ stroke: 'var(--cor-borda)' }}
                    tick={{ fill: 'var(--cor-texto-suave)', fontSize: 11 }}
                    tickFormatter={(valor) => `${valor} mm`}
                  />
                  <Tooltip
                    cursor={{ fill: 'rgba(76, 199, 251, 0.10)' }}
                    content={({ active, payload }) => {
                      if (!active || !payload?.length) {
                        return null
                      }
                      const ponto = payload[0]?.payload as
                        | { mes: string; chuvaMm: number }
                        | undefined
                      if (!ponto) {
                        return null
                      }
                      return (
                        <div className="rounded-lg border border-[var(--cor-borda)] bg-[var(--cor-superficie)] px-3 py-2 shadow-lg">
                          <p className="text-xs font-medium text-[var(--cor-texto-suave)]">
                            {ponto.mes}
                          </p>
                          <p className="mt-0.5 text-sm font-semibold text-[var(--cor-texto-forte)]">
                            {formatarDecimal(ponto.chuvaMm)} mm
                          </p>
                        </div>
                      )
                    }}
                  />
                  <Bar
                    dataKey="chuvaMm"
                    name="Chuva"
                    radius={[4, 4, 0, 0]}
                    maxBarSize={56}
                    activeBar={{ fill: 'var(--cor-barra-agua)' }}
                    isAnimationActive={false}
                  >
                    <LabelList
                      dataKey="chuvaMm"
                      content={<RotuloBarra sufixo="mm" casasDecimais={1} />}
                    />
                    {DADOS_CHUVA.map((ponto, indice) => (
                      <Cell
                        key={ponto.mes}
                        fill={
                          indice === DADOS_CHUVA.length - 1
                            ? 'var(--cor-barra-agua)'
                            : 'var(--cor-barra-agua-suave)'
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
    </div>
  )
}

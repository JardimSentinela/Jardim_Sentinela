import {
  Activity,
  Battery,
  BatteryCharging,
  BatteryWarning,
  CalendarDays,
  Clock3,
  Droplets,
  Gauge,
  Radio,
} from 'lucide-react'
import {
  NIVEL_MAXIMO_JARDIM_CM,
  calcularStatusAlerta,
  formatarDataHora,
} from '../types/jardim'
import type { JardimResponse, NivelAlerta } from '../types/jardim'

interface DadosJardimCardProps {
  jardim: JardimResponse | null
  loading?: boolean
  className?: string
}

const CORES_NIVEL: Record<NivelAlerta, { cor: string; rotulo: string }> = {
  NORMAL: { cor: '#16a34a', rotulo: 'Normal' },
  ATENCAO: { cor: '#d97706', rotulo: 'Atenção' },
  TRANSBORDANDO: { cor: '#0e7490', rotulo: 'Transbordando' },
  ALAGADO: { cor: '#dc2626', rotulo: 'Alagado' },
}

function formatarDecimal(valor: number, casas: number): string {
  return valor.toLocaleString('pt-BR', {
    minimumFractionDigits: casas,
    maximumFractionDigits: casas,
  })
}

/** Ex.: 60 cm -> "0,60 m / 60 cm"; 18,5 cm -> "0,19 m / 18,5 cm". */
function formatarNivelCm(nivelCm: number): string {
  const nivelSeguro = Number.isFinite(nivelCm) ? Math.max(0, nivelCm) : 0
  const metros = formatarDecimal(nivelSeguro / 100, 2)
  const centimetros = Number.isInteger(nivelSeguro)
    ? String(nivelSeguro)
    : formatarDecimal(nivelSeguro, 1)
  return `${metros} m / ${centimetros} cm`
}

function descreverBateria(percentual: number): { rotulo: string; cor: string } {
  if (percentual >= 60) {
    return { rotulo: 'Bateria saudável', cor: '#16a34a' }
  }
  if (percentual >= 30) {
    return { rotulo: 'Bateria moderada', cor: '#d97706' }
  }
  return { rotulo: 'Bateria baixa', cor: '#dc2626' }
}

function BarraProgresso({
  percentual,
  cor,
  rotulo,
}: {
  percentual: number
  cor: string
  rotulo: string
}) {
  const valor = Math.min(100, Math.max(0, percentual))

  return (
    <div
      className="relative h-2.5 w-full overflow-hidden rounded-full bg-[var(--cor-trilha)]"
      role="progressbar"
      aria-label={rotulo}
      aria-valuemin={0}
      aria-valuemax={100}
      aria-valuenow={Math.round(valor)}
    >
      <div
        className="relative h-full rounded-full transition-[width] duration-700 ease-out"
        style={{
          width: `${valor}%`,
          background: `linear-gradient(90deg, ${cor}b3 0%, ${cor} 100%)`,
        }}
      />
      <span
        aria-hidden="true"
        className="absolute inset-y-0 left-[30%] w-px bg-white/50 dark:bg-slate-950/40"
      />
      <span
        aria-hidden="true"
        className="absolute inset-y-0 left-[60%] w-px bg-white/50 dark:bg-slate-950/40"
      />
      <span
        aria-hidden="true"
        className="absolute inset-y-0 left-[90%] w-px bg-white/50 dark:bg-slate-950/40"
      />
    </div>
  )
}

function DadosJardimSkeleton() {
  return (
    <div className="card flex h-full flex-col gap-5 p-6">
      <div className="flex items-center justify-between gap-4">
        <div className="space-y-2">
          <div className="h-3 w-24 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
          <div className="h-7 w-32 animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
        </div>
        <div className="h-10 w-44 animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
      </div>
      <div className="grid flex-1 gap-4 sm:grid-cols-2">
        {[0, 1].map((indice) => (
          <div
            key={indice}
            className="rounded-xl border border-[var(--cor-borda)] p-4"
          >
            <div className="h-10 w-10 animate-pulse rounded-xl bg-[var(--cor-superficie-forte)]" />
            <div className="mt-4 h-4 w-28 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
            <div className="mt-3 h-8 w-40 animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
            <div className="mt-4 h-2.5 w-full animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
            <div className="mt-3 h-3 w-full animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
          </div>
        ))}
      </div>
    </div>
  )
}

export default function DadosJardimCard({
  jardim,
  loading = false,
  className = '',
}: DadosJardimCardProps) {
  if (loading) {
    return <DadosJardimSkeleton />
  }

  if (!jardim) {
    return (
      <section
        className={`card flex h-full min-h-[280px] flex-col items-center justify-center gap-2 p-6 text-center ${className}`}
      >
        <Gauge className="h-6 w-6 text-[var(--cor-texto-suave)]" aria-hidden="true" />
        <h2 className="text-lg">Dados do Jardim</h2>
        <p className="text-sm text-[var(--cor-texto-suave)]">
          Aguardando a primeira leitura do sensor.
        </p>
      </section>
    )
  }

  const nivelCm = Number.isFinite(jardim.nivelCm) ? Math.max(0, jardim.nivelCm) : 0
  const bateria = Math.min(100, Math.max(0, jardim.bateria))
  const status = calcularStatusAlerta(nivelCm)
  const corNivel = CORES_NIVEL[status.nivel]
  const percentualNivel = (nivelCm / NIVEL_MAXIMO_JARDIM_CM) * 100
  const bateriaInfo = descreverBateria(bateria)
  const { data, hora } = formatarDataHora(jardim.ultimaLeituraEm ?? '')
  const temData = data !== '--'

  const IconeBateria =
    bateria >= 60 ? BatteryCharging : bateria >= 30 ? Battery : BatteryWarning

  return (
    <section
      className={`card flex h-full flex-col p-5 sm:p-6 ${className}`}
      aria-labelledby="titulo-dados-jardim"
    >
      <header className="flex flex-wrap items-start justify-between gap-3 border-b border-[var(--cor-borda)] pb-4">
        <div>
          <p className="kicker">Dados do Jardim</p>
          <h2 id="titulo-dados-jardim" className="mt-1.5 text-xl">
            Jardim {jardim.jardimId}
          </h2>
        </div>
        <div className="rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie-suave)] px-3 py-2">
          <p className="text-[10px] font-semibold uppercase tracking-[0.12em] text-[var(--cor-texto-suave)]">
            {temData ? 'Atualizado em' : 'Aguardando leitura'}
          </p>
          {temData ? (
            <p className="mt-0.5 flex items-center gap-2 text-xs font-medium text-[var(--cor-texto)]">
              <span className="inline-flex items-center gap-1">
                <CalendarDays className="h-3.5 w-3.5" aria-hidden="true" />
                {data}
              </span>
              <span className="inline-flex items-center gap-1">
                <Clock3 className="h-3.5 w-3.5" aria-hidden="true" />
                {hora}
              </span>
            </p>
          ) : (
            <p className="mt-0.5 text-xs font-medium text-[var(--cor-texto)]">Sem leitura</p>
          )}
        </div>
      </header>

      <div className="mt-4 flex flex-1 flex-col gap-4">
        {/* Nível de água */}
        <article
          className="flex flex-1 flex-col justify-between rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie-suave)]/60 p-4 transition-colors sm:p-5"
          aria-label="Nível de água"
        >
          <div className="flex flex-wrap items-start justify-between gap-3">
            <div className="flex items-center gap-3">
              <span className="flex h-11 w-11 shrink-0 items-center justify-center rounded-xl bg-agua-50 text-agua-600 ring-1 ring-agua-100 dark:bg-agua-950/40 dark:text-agua-300 dark:ring-agua-900/60">
                <Droplets className="h-5 w-5" aria-hidden="true" />
              </span>
              <div>
                <p className="text-[11px] font-semibold uppercase tracking-[0.1em] text-[var(--cor-texto-suave)]">
                  Nível de Água
                </p>
                <p className="mt-0.5 text-2xl font-semibold leading-tight tabular-nums text-[var(--cor-texto-forte)]">
                  {formatarNivelCm(nivelCm)}
                </p>
                <p className="mt-0.5 text-xs text-[var(--cor-texto-suave)]">
                  {formatarDecimal(Math.min(100, Math.max(0, percentualNivel)), 0)}% da
                  capacidade do jardim
                </p>
              </div>
            </div>
            <span
              className="inline-flex items-center gap-1 rounded-full px-2 py-0.5 text-[10px] font-bold uppercase tracking-wide"
              style={{ color: corNivel.cor, backgroundColor: `${corNivel.cor}14` }}
            >
              <span
                className="h-1.5 w-1.5 rounded-full"
                style={{ backgroundColor: corNivel.cor }}
              />
              {corNivel.rotulo}
            </span>
          </div>

          <div className="mt-3">
            <BarraProgresso
              percentual={percentualNivel}
              cor={corNivel.cor}
              rotulo="Nível de água em relação ao limite"
            />
            <div className="mt-2 flex items-center justify-between text-[11px] text-[var(--cor-texto-suave)]">
              <span>0 cm</span>
              <span className="font-medium">
                {NIVEL_MAXIMO_JARDIM_CM} cm (limite)
              </span>
            </div>
          </div>
        </article>

        {/* Bateria */}
        <article
          className="flex flex-1 flex-col justify-between rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie-suave)]/60 p-4 transition-colors sm:p-5"
          aria-label="Bateria do sensor"
        >
          <div className="flex flex-wrap items-start justify-between gap-3">
            <div className="flex items-center gap-3">
              <span
                className="flex h-11 w-11 shrink-0 items-center justify-center rounded-xl ring-1"
                style={{
                  color: bateriaInfo.cor,
                  backgroundColor: `${bateriaInfo.cor}14`,
                  borderColor: `${bateriaInfo.cor}33`,
                }}
              >
                <IconeBateria className="h-5 w-5" aria-hidden="true" />
              </span>
              <div>
                <p className="text-[11px] font-semibold uppercase tracking-[0.1em] text-[var(--cor-texto-suave)]">
                  Bateria
                </p>
                <p className="mt-0.5 text-2xl font-semibold leading-tight tabular-nums text-[var(--cor-texto-forte)]">
                  {formatarDecimal(bateria, 0)}%
                </p>
                <p className="mt-0.5 text-xs text-[var(--cor-texto-suave)]">
                  {bateriaInfo.rotulo}
                </p>
              </div>
            </div>
            <span
              className="inline-flex items-center gap-1 rounded-full px-2 py-0.5 text-[10px] font-bold uppercase tracking-wide"
              style={{ color: bateriaInfo.cor, backgroundColor: `${bateriaInfo.cor}14` }}
            >
              <span
                className="h-1.5 w-1.5 rounded-full"
                style={{ backgroundColor: bateriaInfo.cor }}
              />
              {bateria >= 30 ? 'Operacional' : 'Crítica'}
            </span>
          </div>

          <div className="mt-3">
            <BarraProgresso
              percentual={bateria}
              cor={bateriaInfo.cor}
              rotulo="Carga da bateria"
            />
            <div className="mt-2 flex items-center justify-between text-[11px] text-[var(--cor-texto-suave)]">
              <span>0%</span>
              <span className="font-medium">100% (total)</span>
            </div>
          </div>
        </article>
      </div>

      <footer className="mt-4 flex flex-wrap items-center gap-x-4 gap-y-1.5 border-t border-[var(--cor-borda)] pt-3 text-[11px] text-[var(--cor-texto-suave)]">
        <span className="inline-flex items-center gap-1.5">
          <Radio className="h-3.5 w-3.5" aria-hidden="true" />
          Sensor {jardim.sensorId}
        </span>
        <span className="inline-flex items-center gap-1.5">
          <Activity className="h-3.5 w-3.5" aria-hidden="true" />
          {jardim.totalLeituras.toLocaleString('pt-BR')} leituras
        </span>
        <span className="ml-auto inline-flex items-center gap-1.5">
          <Clock3 className="h-3.5 w-3.5" aria-hidden="true" />
          Atualização automática
        </span>
      </footer>
    </section>
  )
}

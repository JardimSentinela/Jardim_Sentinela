import { Battery, CalendarDays, Clock3, Droplets } from 'lucide-react'
import { formatarDataHora, NIVEL_MAXIMO_JARDIM_CM } from '../types/jardim'
import type { JardimResponse } from '../types/jardim'

interface DadosJardimCardProps {
  jardim: JardimResponse | null
  loading?: boolean
  className?: string
}

/**
 * Escala do card: a mesma capacidade usada pela API e pelo simulador (50 cm).
 * Os limiares de alerta continuam vindo dos tipos compartilhados.
 */
const LIMITE_EXIBIDO_CM = NIVEL_MAXIMO_JARDIM_CM

function formatarDecimal(valor: number, casas: number): string {
  return valor.toLocaleString('pt-BR', {
    minimumFractionDigits: casas,
    maximumFractionDigits: casas,
  })
}

function formatarNivelCm(nivelCm: number): string {
  const nivel = Number.isFinite(nivelCm) ? Math.max(0, nivelCm) : 0
  const metros = formatarDecimal(nivel / 100, 2)
  const centimetros = Number.isInteger(nivel) ? String(nivel) : formatarDecimal(nivel, 1)
  return `${metros} m/${centimetros} cm`
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
      className="h-2 w-full overflow-hidden rounded-full bg-[var(--cor-trilha)]"
      role="progressbar"
      aria-label={rotulo}
      aria-valuemin={0}
      aria-valuemax={100}
      aria-valuenow={Math.round(valor)}
    >
      <div
        className="h-full rounded-full transition-[width] duration-500 ease-out"
        style={{ width: `${valor}%`, background: cor }}
      />
    </div>
  )
}

function DadosSkeleton() {
  return (
    <div className="card flex h-full flex-col gap-6 p-4 sm:p-6">
      <div className="h-6 w-40 animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
      <div className="h-5 w-3/4 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
      <div className="mt-4 space-y-8">
        {[0, 1].map((indice) => (
          <div key={indice} className="space-y-3">
            <div className="h-6 w-2/3 animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
            <div className="h-3 w-full animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
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
    return <DadosSkeleton />
  }

  if (!jardim) {
    return (
      <section
        className={`card flex h-full min-h-[280px] flex-col justify-center gap-2 p-4 sm:p-6 ${className}`}
      >
        <h2 className="text-xl font-bold">Dados do Jardim</h2>
        <p className="text-sm text-[var(--cor-texto-suave)]">
          Aguardando a primeira leitura do sensor.
        </p>
      </section>
    )
  }

  const nivelCm = Number.isFinite(jardim.nivelCm) ? Math.max(0, jardim.nivelCm) : 0
  const bateria = Math.min(100, Math.max(0, jardim.bateria))
  const percentualNivel = (nivelCm / LIMITE_EXIBIDO_CM) * 100
  const corBateria = bateria >= 30 ? '#30e4bc' : '#e4b739'
  const { data, hora } = formatarDataHora(jardim.ultimaLeituraEm ?? '')
  const temData = data !== '--'

  return (
    <section
      className={`card flex h-full flex-col p-4 sm:p-6 ${className}`}
      aria-labelledby="titulo-dados-jardim"
    >
      <h2 id="titulo-dados-jardim" className="text-xl font-bold">
        Dados do Jardim
      </h2>

      <header className="mt-4 flex flex-col gap-2 border-b border-[var(--cor-borda)] pb-4 sm:flex-row sm:items-center sm:justify-between sm:gap-6">
        <h3 className="text-lg font-semibold">Jardim {jardim.jardimId}</h3>
        <p className="flex flex-wrap items-center gap-x-3 gap-y-1 text-xs text-[var(--cor-texto-suave)] sm:justify-end sm:text-sm">
          <span>{temData ? 'Informação Atualizada em:' : 'Aguardando leitura'}</span>
          {temData && (
            <>
              <span className="inline-flex items-center gap-1.5">
                <CalendarDays className="h-4 w-4" aria-hidden="true" />
                Data {data}
              </span>
              <span className="inline-flex items-center gap-1.5">
                <Clock3 className="h-4 w-4" aria-hidden="true" />
                Hora {hora}
              </span>
            </>
          )}
        </p>
      </header>

      <div className="flex flex-1 flex-col justify-center gap-6 py-6">
        <div>
          <div className="flex flex-wrap items-center justify-between gap-2">
            <span className="inline-flex items-center gap-2 text-sm font-medium text-[var(--cor-texto)]">
              <Droplets className="h-5 w-5 text-[#005b7f]" aria-hidden="true" />
              Nível de Água
            </span>
            <span className="font-brand text-2xl font-bold tabular-nums text-[var(--cor-texto-forte)]">
              {formatarNivelCm(nivelCm)}
            </span>
          </div>
          <div className="mt-2">
            <BarraProgresso
              percentual={percentualNivel}
              cor="linear-gradient(90deg, #4cc7fb 0%, #005b7f 100%)"
              rotulo="Nível de água"
            />
            <div className="mt-2 flex items-center justify-between text-xs text-[var(--cor-texto-suave)]">
              <span>0 cm</span>
              <span className="font-medium text-[var(--cor-texto)]">
                {LIMITE_EXIBIDO_CM} cm (limite)
              </span>
            </div>
          </div>
        </div>

        <div>
          <div className="flex flex-wrap items-center justify-between gap-2">
            <span className="inline-flex items-center gap-2 text-sm font-medium text-[var(--cor-texto)]">
              <Battery className="h-5 w-5 text-[#071d41] dark:text-[#4cc7fb]" aria-hidden="true" />
              Bateria Sensor Resistivo T1592
            </span>
            <span className="font-brand text-2xl font-bold tabular-nums text-[var(--cor-texto-forte)]">
              {formatarDecimal(bateria, 0)}%
            </span>
          </div>
          <div className="mt-2">
            <BarraProgresso
              percentual={bateria}
              cor={corBateria}
              rotulo="Carga da bateria"
            />
            <div className="mt-2 flex items-center justify-between text-xs text-[var(--cor-texto-suave)]">
              <span>0%</span>
              <span className="font-medium text-[var(--cor-texto)]">100% (total)</span>
            </div>
          </div>
        </div>
      </div>
    </section>
  )
}

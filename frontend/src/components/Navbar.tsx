import { useEffect, useState } from 'react'
import {
  CalendarDays,
  Clock3,
  Flower2,
  LayoutDashboard,
  Moon,
  ShieldCheck,
  Sun,
} from 'lucide-react'

export type AbaPainel = 'jardim' | 'cop'

interface NavbarProps {
  temaEscuro: boolean
  /** Define explicitamente o tema (usado pelos dois botões do seletor). */
  onDefinirTema: (escuro: boolean) => void
  abaAtiva: AbaPainel
  onMudarAba: (aba: AbaPainel) => void
}

const ABAS: Array<{
  id: AbaPainel
  rotulo: string
  icone: typeof LayoutDashboard
}> = [
  { id: 'jardim', rotulo: 'Painel Jardim Sentinela', icone: LayoutDashboard },
  { id: 'cop', rotulo: 'Painel COP', icone: ShieldCheck },
]

function doisDigitos(valor: number): string {
  return String(valor).padStart(2, '0')
}

function formatarData(data: Date): string {
  return `${doisDigitos(data.getDate())}/${doisDigitos(data.getMonth() + 1)}/${String(
    data.getFullYear(),
  ).slice(-2)}`
}

function formatarHora(data: Date): string {
  return `${doisDigitos(data.getHours())}:${doisDigitos(data.getMinutes())}`
}

/** Relógio local, atualizado a cada segundo para virar o minuto sem atraso. */
function useAgora(): Date {
  const [agora, setAgora] = useState(() => new Date())

  useEffect(() => {
    const temporizador = globalThis.setInterval(() => setAgora(new Date()), 1000)
    return () => globalThis.clearInterval(temporizador)
  }, [])

  return agora
}

function IndicadorRelogio({
  icone: Icone,
  rotulo,
  valor,
}: {
  icone: typeof CalendarDays
  rotulo: string
  valor: string
}) {
  return (
    <div className="flex items-center gap-2 rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie-suave)] px-3 py-2">
      <Icone className="h-4 w-4 text-jardin-600 dark:text-jardin-300" aria-hidden="true" />
      <span className="text-[10px] font-semibold uppercase tracking-[0.12em] text-[var(--cor-texto-suave)]">
        {rotulo}
      </span>
      <span className="text-sm font-semibold tabular-nums text-[var(--cor-texto-forte)]">
        {valor}
      </span>
    </div>
  )
}

export default function Navbar({
  temaEscuro,
  onDefinirTema,
  abaAtiva,
  onMudarAba,
}: NavbarProps) {
  const agora = useAgora()

  return (
    <header className="sticky top-0 z-40 border-b border-[var(--cor-borda)] bg-white/85 backdrop-blur-xl dark:bg-slate-950/85">
      <div className="mx-auto w-full max-w-[1440px] px-4 sm:px-6 lg:px-8">
        <div className="flex min-h-[76px] flex-wrap items-center justify-between gap-4 py-3">
          {/* Identidade visual */}
          <div className="flex items-center gap-3.5">
            <span className="relative flex h-12 w-12 items-center justify-center rounded-2xl bg-gradient-to-br from-jardin-500 via-jardin-600 to-agua-600 text-white shadow-lg shadow-jardin-600/25 ring-1 ring-white/20">
              <Flower2 className="h-6 w-6" aria-hidden="true" />
              <span className="absolute -right-1 -top-1 flex h-3.5 w-3.5 items-center justify-center rounded-full bg-white dark:bg-slate-900">
                <span className="h-2 w-2 rounded-full bg-emerald-500" />
              </span>
            </span>
            <div>
              <h1 className="text-lg leading-tight sm:text-xl">Jardim Sentinela</h1>
              <p className="text-xs font-medium text-[var(--cor-texto-suave)] sm:text-[13px]">
                Rede de Jardins Monitorados
              </p>
            </div>
          </div>

          {/* Relógio + tema */}
          <div className="flex flex-wrap items-center gap-2.5">
            <div className="hidden items-center gap-2 md:flex">
              <IndicadorRelogio icone={CalendarDays} rotulo="Data" valor={formatarData(agora)} />
              <IndicadorRelogio icone={Clock3} rotulo="Hora" valor={formatarHora(agora)} />
            </div>

            <div
              role="group"
              aria-label="Selecionar tema"
              className="flex items-center rounded-xl border border-[var(--cor-borda)] bg-[var(--cor-superficie-suave)] p-1"
            >
              <button
                type="button"
                onClick={() => onDefinirTema(false)}
                aria-pressed={!temaEscuro}
                className={`inline-flex items-center gap-1.5 rounded-lg px-2.5 py-1.5 text-xs font-semibold transition-all ${
                  temaEscuro
                    ? 'text-[var(--cor-texto-suave)] hover:text-[var(--cor-texto)]'
                    : 'bg-[var(--cor-superficie)] text-jardin-700 shadow-sm ring-1 ring-black/5 dark:text-jardin-200 dark:ring-white/10'
                }`}
              >
                <Sun className="h-4 w-4" aria-hidden="true" />
                <span className="hidden sm:inline">Modo Claro</span>
              </button>
              <button
                type="button"
                onClick={() => onDefinirTema(true)}
                aria-pressed={temaEscuro}
                className={`inline-flex items-center gap-1.5 rounded-lg px-2.5 py-1.5 text-xs font-semibold transition-all ${
                  temaEscuro
                    ? 'bg-[var(--cor-superficie)] text-jardin-200 shadow-sm ring-1 ring-white/10'
                    : 'text-[var(--cor-texto-suave)] hover:text-[var(--cor-texto)]'
                }`}
              >
                <Moon className="h-4 w-4" aria-hidden="true" />
                <span className="hidden sm:inline">Modo Escuro</span>
              </button>
            </div>
          </div>
        </div>

        {/* Navegação principal */}
        <div className="flex flex-wrap items-center justify-between gap-3">
          <nav aria-label="Painéis" className="flex items-center gap-1 sm:gap-2">
            {ABAS.map((aba) => {
              const Icone = aba.icone
              const ativa = abaAtiva === aba.id
              return (
                <button
                  key={aba.id}
                  type="button"
                  role="tab"
                  aria-selected={ativa}
                  onClick={() => onMudarAba(aba.id)}
                  className={`group relative -mb-px flex items-center gap-2 border-b-2 px-3 pb-3 pt-2 text-sm font-semibold transition-colors sm:px-4 ${
                    ativa
                      ? 'border-jardin-500 text-jardin-700 dark:border-jardin-400 dark:text-jardin-300'
                      : 'border-transparent text-[var(--cor-texto-suave)] hover:border-[var(--cor-borda-forte)] hover:text-[var(--cor-texto)]'
                  }`}
                >
                  <Icone
                    className={`h-4 w-4 transition-transform group-hover:-translate-y-0.5 ${
                      ativa ? 'text-jardin-500 dark:text-jardin-400' : ''
                    }`}
                    aria-hidden="true"
                  />
                  {aba.rotulo}
                </button>
              )
            })}
          </nav>

          <div className="flex w-full items-center justify-end gap-3 pb-2 text-[11px] font-medium text-[var(--cor-texto-suave)] md:w-auto">
            <span className="hidden items-center gap-2 md:inline-flex">
              <span className="relative flex h-2 w-2">
                <span className="absolute inline-flex h-full w-full animate-ping rounded-full bg-emerald-400 opacity-60" />
                <span className="relative inline-flex h-2 w-2 rounded-full bg-emerald-500" />
              </span>
              Atualização automática a cada 5s
            </span>
            <span className="inline-flex items-center gap-1.5 md:hidden">
              <CalendarDays className="h-3.5 w-3.5" aria-hidden="true" />
              {formatarData(agora)}
            </span>
            <span className="inline-flex items-center gap-1.5 md:hidden">
              <Clock3 className="h-3.5 w-3.5" aria-hidden="true" />
              {formatarHora(agora)}
            </span>
          </div>
        </div>
      </div>
    </header>
  )
}

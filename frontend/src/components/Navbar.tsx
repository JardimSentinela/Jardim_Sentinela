import { useEffect, useState } from 'react'
import { CalendarDays, Clock3, Moon, Sun } from 'lucide-react'
import logo1 from '../../assets/logo1.svg'

export type AbaPainel = 'jardim' | 'cop'

interface NavbarProps {
  temaEscuro: boolean
  onDefinirTema: (escuro: boolean) => void
  abaAtiva: AbaPainel
  onMudarAba: (aba: AbaPainel) => void
}

const ABAS: Array<{ id: AbaPainel; rotulo: string }> = [
  { id: 'jardim', rotulo: 'Painel Jardim Sentinela' },
  { id: 'cop', rotulo: 'Painel COP' },
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

function useAgora(): Date {
  const [agora, setAgora] = useState(() => new Date())

  useEffect(() => {
    const temporizador = globalThis.setInterval(() => setAgora(new Date()), 1000)
    return () => globalThis.clearInterval(temporizador)
  }, [])

  return agora
}

export default function Navbar({
  temaEscuro,
  onDefinirTema,
  abaAtiva,
  onMudarAba,
}: NavbarProps) {
  const agora = useAgora()

  return (
    <header className="border-b border-[var(--cor-borda)] bg-[var(--cor-superficie)]">
      <div className="mx-auto w-full max-w-[1440px] px-4 sm:px-6 lg:px-8">
        <div className="flex flex-wrap items-center justify-between gap-x-6 gap-y-4 py-4 lg:grid lg:grid-cols-[1fr_auto_1fr]">
          <div className="flex items-center gap-4">
            <span className="flex h-10 w-10 items-center justify-center rounded-xl bg-[#071d41] text-white">
              <img src={logo1} alt="Logo Jardim Sentinela" className="h-5 w-5" />
            </span>
            <div>
              <div className="font-display text-lg font-bold leading-tight">
                Jardim Sentinela
              </div>
              <p className="text-xs text-[var(--cor-texto-suave)]">
                Rede de Jardins Monitorados
              </p>
            </div>
          </div>

          <div className="order-3 flex w-full items-center justify-center gap-2 sm:gap-4 lg:order-none lg:w-auto">
            <button
              type="button"
              onClick={() => onDefinirTema(false)}
              aria-pressed={!temaEscuro}
              className={`text-sm transition-colors ${
                temaEscuro
                  ? 'text-[var(--cor-texto-suave)] hover:text-[var(--cor-texto)]'
                  : 'font-semibold text-[var(--cor-texto-forte)]'
              }`}
            >
              Modo Claro
            </button>
            <Sun
              className={`h-4 w-4 ${
                temaEscuro ? 'text-[var(--cor-texto-suave)]' : 'text-[#e4b739]'
              }`}
              aria-hidden="true"
            />
            <button
              type="button"
              role="switch"
              aria-checked={temaEscuro}
              aria-label="Alternar entre modo claro e escuro"
              onClick={() => onDefinirTema(!temaEscuro)}
              className={`relative h-6 w-11 shrink-0 rounded-full transition-colors ${
                temaEscuro ? 'bg-[#005b7f]' : 'bg-[#d1d5db] dark:bg-[#5a5969]'
              }`}
            >
              <span
                className={`absolute left-0.5 top-0.5 h-5 w-5 rounded-full bg-white shadow-sm transition-transform ${
                  temaEscuro ? 'translate-x-5' : 'translate-x-0'
                }`}
              />
            </button>
            <Moon
              className={`h-4 w-4 ${
                temaEscuro
                  ? 'text-[#4cc7fb]'
                  : 'text-[var(--cor-texto-suave)]'
              }`}
              aria-hidden="true"
            />
            <button
              type="button"
              onClick={() => onDefinirTema(true)}
              aria-pressed={temaEscuro}
              className={`text-sm transition-colors ${
                temaEscuro
                  ? 'font-semibold text-[var(--cor-texto-forte)]'
                  : 'text-[var(--cor-texto-suave)] hover:text-[var(--cor-texto)]'
              }`}
            >
              Modo Escuro
            </button>
          </div>

          <div className="flex flex-col items-end gap-1 text-xs text-[var(--cor-texto)] sm:flex-row sm:items-center sm:gap-4 sm:text-sm lg:justify-self-end">
            <span className="inline-flex items-center gap-2">
              <CalendarDays
                className="h-4 w-4 text-[var(--cor-texto-suave)]"
                aria-hidden="true"
              />
              Data <strong className="font-semibold">{formatarData(agora)}</strong>
            </span>
            <span className="inline-flex items-center gap-2">
              <Clock3
                className="h-4 w-4 text-[var(--cor-texto-suave)]"
                aria-hidden="true"
              />
              Hora{' '}
              <strong className="font-brand font-semibold tabular-nums">
                {formatarHora(agora)}
              </strong>
            </span>
          </div>
        </div>

        <nav aria-label="Painéis" className="mt-1 flex gap-6">
          {ABAS.map((aba) => {
            const ativa = abaAtiva === aba.id
            return (
              <button
                key={aba.id}
                type="button"
                role="tab"
                aria-selected={ativa}
                onClick={() => onMudarAba(aba.id)}
                className={`relative -mb-px border-b-2 pb-3 pt-1 text-sm font-medium transition-colors ${
                  ativa
                    ? 'border-[#005b7f] text-[var(--cor-texto-forte)] dark:border-[#4cc7fb]'
                    : 'border-transparent text-[var(--cor-texto-suave)] hover:border-[var(--cor-borda-forte)] hover:text-[var(--cor-texto)]'
                }`}
              >
                {aba.rotulo}
              </button>
            )
          })}
        </nav>
      </div>
    </header>
  )
}

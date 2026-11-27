import { CalendarDays, Clock3 } from 'lucide-react'
import IndicadorTendencia from './IndicadorTendencia'
import { ESTADOS_ALERTA, formatarDataHora } from '../types/jardim'
import type { InformacoesAlerta, JardimResponse } from '../types/jardim'

interface StatusCardProps {
  jardim: JardimResponse | null
  alerta: InformacoesAlerta | null
  loading?: boolean
  className?: string
  /** Falha ao carregar o estado atual; com dados anteriores, vira aviso de defasagem. */
  erro?: Error | null
  /** 404 do detalhe e do histórico: jardim cadastrado, mas ainda sem leitura. */
  semLeituras?: boolean
  onTentarNovamente?: () => void
}

function StatusSkeleton() {
  return (
    <div className="card flex h-full min-h-[320px] flex-col gap-6 p-4 sm:p-6">
      <div className="flex items-center justify-between gap-4">
        <div className="h-7 w-28 animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
        <div className="h-4 w-52 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
      </div>
      <div className="flex gap-6">
        <div className="h-40 w-14 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
        <div className="flex-1 space-y-4 pt-2">
          <div className="h-9 w-3/4 animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
          <div className="h-5 w-1/2 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
          <div className="h-16 w-full animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
        </div>
      </div>
    </div>
  )
}

export default function StatusCard({
  jardim,
  alerta,
  loading = false,
  className = '',
  erro = null,
  semLeituras = false,
  onTentarNovamente,
}: StatusCardProps) {
  if (loading) {
    return <StatusSkeleton />
  }

  if (!jardim || !alerta) {
    const titulo = semLeituras ? 'Sem leituras recebidas' : 'Jardim indisponível'
    const descricao = semLeituras
      ? 'Este jardim ainda não enviou leituras para a API.'
      : (erro?.message ?? 'Não foi possível carregar o estado atual deste jardim.')

    return (
      <section
        aria-labelledby="titulo-status-jardim"
        className={`card flex h-full min-h-[280px] flex-col items-center justify-center gap-4 p-4 text-center sm:p-6 ${className}`}
      >
        <h2 id="titulo-status-jardim" className="text-xl font-bold">
          {titulo}
        </h2>
        <p className="max-w-xs text-sm text-[var(--cor-texto-suave)]">
          {descricao}
        </p>
        {onTentarNovamente && (
          <button
            type="button"
            onClick={onTentarNovamente}
            className="rounded-full border border-[var(--cor-borda)] px-4 py-2 text-sm font-medium hover:bg-[var(--cor-superficie-suave)]"
          >
            Tentar novamente
          </button>
        )}
      </section>
    )
  }

  const { data, hora } = formatarDataHora(jardim.ultimaLeituraEm ?? '')

  return (
    <section
      className={`flex h-full flex-col rounded-2xl p-4 sm:p-6 ${className}`}
      style={{ backgroundColor: alerta.cor, color: alerta.corTexto }}
      aria-labelledby="titulo-status-jardim"
    >
      <header className="flex flex-col gap-2 sm:flex-row sm:items-start sm:justify-between sm:gap-4">
        <h2
          id="titulo-status-jardim"
          className="shrink-0 whitespace-nowrap text-2xl font-bold leading-tight"
          style={{ color: alerta.corTexto }}
        >
          Jardim {jardim.jardimId}
        </h2>
        <p className="flex max-w-full flex-wrap items-center gap-x-4 gap-y-1 text-xs opacity-80 sm:max-w-[290px] sm:justify-end sm:text-right">
          <span>Informação atualizada em:</span>
          <span className="inline-flex items-center gap-2">
            <CalendarDays className="h-4 w-4" aria-hidden="true" />
            Data {data}
          </span>
          <span className="inline-flex items-center gap-2">
            <Clock3 className="h-4 w-4" aria-hidden="true" />
            Hora {hora}
          </span>
        </p>
      </header>

      <div className="mt-8 flex flex-1 gap-4 sm:gap-6">
        <div
          className="flex h-fit shrink-0 flex-col items-center gap-4 rounded-full bg-white px-3 py-4 shadow-sm"
          role="img"
          aria-label={`Estado do jardim: ${alerta.titulo}`}
        >
          {ESTADOS_ALERTA.map((nivel) => {
            const ativo = nivel === alerta.nivel
            return (
              <span
                key={nivel}
                className="h-5 w-5 rounded-full"
                style={{ backgroundColor: ativo ? alerta.cor : '#d1d5db' }}
              />
            )
          })}
        </div>

        <div className="min-w-0 flex-1 sm:pt-1">
          <h3
            className="text-3xl font-bold leading-tight sm:text-[34px]"
            style={{ color: alerta.corTexto }}
          >
            Estado Atual: {alerta.titulo}
          </h3>
          <p className="mt-2 text-base font-semibold opacity-90">
            Estado de alerta — {alerta.subtitulo}
          </p>
          <div className="mt-3 flex flex-wrap items-center gap-2 text-sm font-medium">
            <span className="opacity-80">Tendência:</span>
            <IndicadorTendencia tendencia={jardim.tendencia} />
          </div>
          <p className="mt-4 max-w-2xl text-sm leading-relaxed opacity-80">
            {alerta.descricao}
          </p>
        </div>
      </div>

      {erro && (
        <div className="mt-6 flex flex-wrap items-center gap-3 text-xs font-medium opacity-90">
          <span>Última atualização falhou; exibindo o dado anterior.</span>
          {onTentarNovamente && (
            <button
              type="button"
              onClick={onTentarNovamente}
              className="rounded-full border border-current px-3 py-1 font-semibold hover:bg-black/10"
            >
              Tentar novamente
            </button>
          )}
        </div>
      )}
    </section>
  )
}

import { AlertTriangle, RefreshCw } from 'lucide-react'
import IndicadorTendencia from './IndicadorTendencia'
import type { JardimRedeItem, JardimMonitoradoId } from '../hooks/useJardinsRede'
import { ApiError } from '../services/api'
import { alertaDoJardim, formatarDataHora } from '../types/jardim'
import type { JardimResponse } from '../types/jardim'

interface RedeJardinsProps {
  itens: JardimRedeItem[]
  loading?: boolean
  error?: Error | null
  jardimSelecionado: string
  onSelecionar: (id: JardimMonitoradoId) => void
  onTentarNovamente?: () => void
  className?: string
}

function formatarNivelCm(nivelCm: number): string {
  const nivel = Number.isFinite(nivelCm) ? Math.max(0, nivelCm) : 0
  return `${nivel.toLocaleString('pt-BR', {
    minimumFractionDigits: 0,
    maximumFractionDigits: 1,
  })} cm`
}

function descreverErroItem(erro: Error): string {
  if (erro instanceof ApiError && erro.status === 404) {
    return 'Sem leituras recebidas'
  }
  return 'Falha ao carregar'
}

function ConteudoJardim({ jardim }: { jardim: JardimResponse }) {
  const alerta = alertaDoJardim(jardim)
  const { data, hora } = formatarDataHora(jardim.ultimaLeituraEm ?? '')
  const temAtualizacao = data !== '--'

  return (
    <dl className="mt-3 grid grid-cols-2 gap-x-3 gap-y-2 text-sm">
      <div>
        <dt className="text-xs text-[var(--cor-texto-suave)]">Nível</dt>
        <dd className="font-brand text-xl font-bold tabular-nums text-[var(--cor-texto-forte)]">
          {formatarNivelCm(jardim.nivelCm)}
        </dd>
      </div>
      <div>
        <dt className="text-xs text-[var(--cor-texto-suave)]">Estado</dt>
        <dd className="mt-0.5 inline-flex items-center gap-1.5 font-semibold text-[var(--cor-texto-forte)]">
          <span
            className="h-2.5 w-2.5 rounded-full"
            style={{ backgroundColor: alerta.cor }}
            aria-hidden="true"
          />
          {alerta.titulo}
        </dd>
      </div>
      <div className="col-span-2">
        <dt className="text-xs text-[var(--cor-texto-suave)]">Tendência</dt>
        <dd className="mt-1">
          <IndicadorTendencia tendencia={jardim.tendencia} />
        </dd>
      </div>
      <div className="col-span-2">
        <dt className="text-xs text-[var(--cor-texto-suave)]">Última atualização</dt>
        <dd className="mt-0.5 text-[var(--cor-texto)]">
          {temAtualizacao ? `${data} · ${hora}` : '--'}
        </dd>
      </div>
    </dl>
  )
}

function CartaoVazio({ item }: { item: JardimRedeItem }) {
  return (
    <dl className="mt-3 grid grid-cols-2 gap-x-3 gap-y-2 text-sm">
      <div className="col-span-2 flex items-start gap-2 text-[var(--cor-texto-suave)]">
        <AlertTriangle className="mt-0.5 h-4 w-4 shrink-0" aria-hidden="true" />
        <span>{item.erro ? descreverErroItem(item.erro) : 'Aguardando dados da API'}</span>
      </div>
      <div>
        <dt className="text-xs text-[var(--cor-texto-suave)]">Nível</dt>
        <dd className="font-brand text-xl font-bold text-[var(--cor-texto-suave)]">--</dd>
      </div>
      <div>
        <dt className="text-xs text-[var(--cor-texto-suave)]">Estado</dt>
        <dd className="font-semibold text-[var(--cor-texto-suave)]">--</dd>
      </div>
      <div className="col-span-2">
        <dt className="text-xs text-[var(--cor-texto-suave)]">Tendência</dt>
        <dd className="mt-1">
          <IndicadorTendencia tendencia={null} />
        </dd>
      </div>
      <div className="col-span-2">
        <dt className="text-xs text-[var(--cor-texto-suave)]">Última atualização</dt>
        <dd className="text-[var(--cor-texto-suave)]">--</dd>
      </div>
    </dl>
  )
}

export default function RedeJardins({
  itens,
  loading = false,
  error = null,
  jardimSelecionado,
  onSelecionar,
  onTentarNovamente,
  className = '',
}: RedeJardinsProps) {
  const carregandoInicial = loading && itens.every((item) => !item.jardim)

  return (
    <section
      className={`card p-4 sm:p-6 ${className}`}
      aria-labelledby="titulo-rede-jardins"
    >
      <div className="flex flex-col gap-2 sm:flex-row sm:items-end sm:justify-between">
        <div>
          <h2 id="titulo-rede-jardins" className="text-xl font-bold">
            Rede de Jardins
          </h2>
          <p className="mt-1 text-sm text-[var(--cor-texto-suave)]">
            J01–J04 com nível, estado, tendência e última atualização da API.
          </p>
        </div>
        {error && onTentarNovamente && (
          <button
            type="button"
            onClick={onTentarNovamente}
            className="inline-flex items-center gap-2 self-start rounded-full border border-[var(--cor-borda)] px-3 py-1.5 text-sm font-medium hover:bg-[var(--cor-superficie-suave)] sm:self-auto"
          >
            <RefreshCw className="h-4 w-4" aria-hidden="true" />
            Tentar novamente
          </button>
        )}
      </div>

      {error && (
        <p
          role="alert"
          className="mt-4 rounded-lg border border-[#f0b7a1] bg-[#fff4ef] px-3 py-2 text-sm text-[#9a3412] dark:border-[#7c2d12] dark:bg-[#3b1c12] dark:text-[#fdba74]"
        >
          Não foi possível atualizar a rede: {error.message}
        </p>
      )}

      {carregandoInicial ? (
        <ul
          aria-label="Carregando jardins monitorados"
          className="mt-4 grid gap-3 sm:grid-cols-2 xl:grid-cols-4"
        >
          {itens.map((item) => (
            <li
              key={item.jardimId}
              className="rounded-xl border border-[var(--cor-borda)] p-4"
            >
              <div className="h-5 w-24 animate-pulse rounded bg-[var(--cor-superficie-forte)]" />
              <div className="mt-4 h-8 w-20 animate-pulse rounded bg-[var(--cor-superficie-forte)]" />
              <div className="mt-3 h-4 w-full animate-pulse rounded bg-[var(--cor-superficie-forte)]" />
              <div className="mt-2 h-4 w-2/3 animate-pulse rounded bg-[var(--cor-superficie-forte)]" />
            </li>
          ))}
        </ul>
      ) : (
        <ul
          aria-label="Jardins monitorados J01 a J04"
          className="mt-4 grid gap-3 sm:grid-cols-2 xl:grid-cols-4"
        >
          {itens.map((item) => {
            const selecionado = item.jardimId === jardimSelecionado
            return (
              <li key={item.jardimId}>
                <button
                  type="button"
                  aria-label={`Selecionar Jardim ${item.jardimId}`}
                  aria-pressed={selecionado}
                  onClick={() => onSelecionar(item.jardimId)}
                  className={`h-full w-full rounded-xl border p-4 text-left transition-colors hover:bg-[var(--cor-superficie-suave)] focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-[var(--cor-foco)] ${
                    selecionado
                      ? 'border-[#005b7f] ring-1 ring-[#005b7f] dark:border-[#4cc7fb] dark:ring-[#4cc7fb]'
                      : 'border-[var(--cor-borda)]'
                  }`}
                >
                  <div className="flex items-center justify-between gap-2">
                    <h3 className="text-base font-bold text-[var(--cor-texto-forte)]">
                      Jardim {item.jardimId}
                    </h3>
                    {selecionado && (
                      <span className="rounded-full bg-[#005b7f] px-2 py-0.5 text-[10px] font-bold uppercase tracking-wide text-white dark:bg-[#4cc7fb] dark:text-[#071d41]">
                        Em foco
                      </span>
                    )}
                  </div>

                  {item.jardim ? (
                    <ConteudoJardim jardim={item.jardim} />
                  ) : (
                    <CartaoVazio item={item} />
                  )}

                  {item.erro && item.jardim && (
                    <p className="mt-2 text-[11px] font-medium text-[#9a3412] dark:text-[#fdba74]">
                      Última consulta falhou; exibindo o dado anterior.
                    </p>
                  )}
                </button>
              </li>
            )
          })}
        </ul>
      )}
    </section>
  )
}

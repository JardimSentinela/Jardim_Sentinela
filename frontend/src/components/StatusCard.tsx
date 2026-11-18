import type { CSSProperties } from 'react'
import {
  AlertTriangle,
  CalendarDays,
  Clock3,
  Flower2,
  ShieldCheck,
  Waves,
} from 'lucide-react'
import { formatarDataHora } from '../types/jardim'
import type { InformacoesAlerta, JardimResponse, NivelAlerta } from '../types/jardim'

interface StatusCardProps {
  jardim: JardimResponse | null
  alerta: InformacoesAlerta | null
  loading?: boolean
  className?: string
  onTentarNovamente?: () => void
}

interface TemaAlerta {
  cor: string
  fundo: string
  fundoSuave: string
  fundoEscuro: string
  fundoSuaveEscuro: string
  borda: string
  bordaEscuro: string
}

const NIVEIS: Array<{ nivel: NivelAlerta; rotulo: string; cor: string }> = [
  { nivel: 'NORMAL', rotulo: 'Normal', cor: '#16a34a' },
  { nivel: 'ATENCAO', rotulo: 'Atenção', cor: '#d97706' },
  { nivel: 'TRANSBORDANDO', rotulo: 'Transbordando', cor: '#0e7490' },
  { nivel: 'ALAGADO', rotulo: 'Alagado', cor: '#dc2626' },
]

const TEMAS: Record<NivelAlerta, TemaAlerta> = {
  NORMAL: {
    cor: '#16a34a',
    fundo: 'rgba(22, 163, 74, 0.10)',
    fundoSuave: 'rgba(236, 253, 245, 0.75)',
    fundoEscuro: 'rgba(22, 163, 74, 0.18)',
    fundoSuaveEscuro: 'rgba(6, 78, 59, 0.16)',
    borda: 'rgba(22, 163, 74, 0.26)',
    bordaEscuro: 'rgba(74, 222, 128, 0.24)',
  },
  ATENCAO: {
    cor: '#d97706',
    fundo: 'rgba(217, 119, 6, 0.11)',
    fundoSuave: 'rgba(255, 251, 235, 0.78)',
    fundoEscuro: 'rgba(217, 119, 6, 0.18)',
    fundoSuaveEscuro: 'rgba(120, 53, 15, 0.16)',
    borda: 'rgba(217, 119, 6, 0.28)',
    bordaEscuro: 'rgba(251, 191, 36, 0.26)',
  },
  TRANSBORDANDO: {
    cor: '#0e7490',
    fundo: 'rgba(14, 116, 144, 0.10)',
    fundoSuave: 'rgba(236, 254, 255, 0.72)',
    fundoEscuro: 'rgba(14, 116, 144, 0.20)',
    fundoSuaveEscuro: 'rgba(21, 94, 117, 0.18)',
    borda: 'rgba(14, 116, 144, 0.28)',
    bordaEscuro: 'rgba(34, 211, 238, 0.26)',
  },
  ALAGADO: {
    cor: '#dc2626',
    fundo: 'rgba(220, 38, 38, 0.10)',
    fundoSuave: 'rgba(254, 242, 242, 0.80)',
    fundoEscuro: 'rgba(220, 38, 38, 0.18)',
    fundoSuaveEscuro: 'rgba(127, 29, 29, 0.18)',
    borda: 'rgba(220, 38, 38, 0.28)',
    bordaEscuro: 'rgba(248, 113, 113, 0.26)',
  },
}

const DETALHES: Record<NivelAlerta, string> = {
  NORMAL:
    'A última leitura confirma drenagem estável, sem acúmulo relevante de água no reservatório.',
  ATENCAO:
    'O volume segue dentro da faixa operacional, porém a tendência de subida pede acompanhamento nas próximas leituras.',
  TRANSBORDANDO:
    'A capacidade de retenção está próxima do limite e o extravasamento pode ocorrer se a chuva continuar.',
  ALAGADO:
    'A capacidade do jardim foi esgotada. A água pode transbordar para o entorno e atingir áreas de circulação.',
}

const RECOMENDACOES: Record<NivelAlerta, string[]> = {
  NORMAL: [
    'Mantenha a inspeção visual semanal das entradas e saídas de água.',
    'Remova folhas e sedimentos que possam obstruir a drenagem.',
    'Zele para que a bateria do sensor permaneça acima de 20%.',
  ],
  ATENCAO: [
    'Acompanhe as próximas leituras para confirmar a tendência de subida.',
    'Inspecione a saída de drenagem em busca de obstruções.',
    'Evite descartar resíduos na área do jardim nas próximas horas.',
  ],
  TRANSBORDANDO: [
    'Interrompa o trânsito de pessoas na área do jardim.',
    'Verifique imediatamente o extravasor e a rede de drenagem.',
    'Acione a equipe de manutenção e mantenha o monitoramento ativo.',
  ],
  ALAGADO: [
    'Isole o perímetro e sinalize o risco de alagamento.',
    'Acione a equipe de emergência e o plano de contingência.',
    'Registre o evento e verifique a integridade do sensor após a ocorrência.',
  ],
}

function criarEstiloAlerta({ nivel }: { nivel: NivelAlerta }) {
  const tema = TEMAS[nivel]
  const estilo = {
    '--alerta-cor': tema.cor,
    '--alerta-fundo': tema.fundo,
    '--alerta-fundo-suave': tema.fundoSuave,
    '--alerta-fundo-escuro': tema.fundoEscuro,
    '--alerta-fundo-suave-escuro': tema.fundoSuaveEscuro,
    '--alerta-borda': tema.borda,
    '--alerta-borda-escuro': tema.bordaEscuro,
  } as CSSProperties

  return estilo
}

function Semaforo({ nivelAtivo }: { nivelAtivo: NivelAlerta }) {
  return (
    <div className="flex shrink-0 flex-col">
      <span className="mb-3 text-[10px] font-semibold uppercase tracking-[0.18em] text-[var(--cor-texto-suave)]">
        Nível
      </span>
      <div className="relative grid grid-cols-2 gap-x-4 gap-y-3 sm:flex sm:items-center sm:gap-5 lg:flex-col lg:items-stretch lg:gap-4">
        <span
          aria-hidden="true"
          className="absolute bottom-2 left-[9px] top-2 hidden w-px bg-[var(--cor-borda)] lg:block"
        />
        {NIVEIS.map((item) => {
          const ativo = item.nivel === nivelAtivo
          return (
            <div
              key={item.nivel}
              className="relative z-10 flex items-center gap-2"
            >
              <span
                aria-hidden="true"
                className={`flex h-[18px] w-[18px] shrink-0 items-center justify-center rounded-full border-2 transition-all duration-300 ${
                  ativo ? 'scale-110' : 'opacity-60'
                }`}
                style={{
                  borderColor: ativo ? item.cor : `${item.cor}66`,
                  backgroundColor: ativo ? item.cor : `${item.cor}12`,
                  boxShadow: ativo ? `0 0 0 4px ${item.cor}22` : 'none',
                }}
              >
                {ativo && <span className="h-1.5 w-1.5 rounded-full bg-white/95" />}
              </span>
              <span
                className={`text-[11px] leading-none transition-colors ${
                  ativo ? 'font-semibold text-[var(--cor-texto-forte)]' : 'text-[var(--cor-texto-suave)]'
                }`}
              >
                {item.rotulo}
              </span>
            </div>
          )
        })}
      </div>
    </div>
  )
}

function StatusCardSkeleton() {
  return (
    <div className="card flex h-full min-h-[480px] flex-col gap-4 p-6">
      <div className="h-5 w-28 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
      <div className="h-8 w-44 animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
      <div className="h-4 w-56 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
      <div className="mt-4 flex gap-5">
        <div className="h-52 w-16 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
        <div className="flex-1 space-y-3">
          <div className="h-6 w-2/3 animate-pulse rounded-lg bg-[var(--cor-superficie-forte)]" />
          <div className="h-4 w-1/2 animate-pulse rounded-full bg-[var(--cor-superficie-forte)]" />
          <div className="h-20 w-full animate-pulse rounded-xl bg-[var(--cor-superficie-forte)]" />
          <div className="h-24 w-full animate-pulse rounded-xl bg-[var(--cor-superficie-forte)]" />
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
  onTentarNovamente,
}: StatusCardProps) {
  if (loading) {
    return <StatusCardSkeleton />
  }

  if (!jardim || !alerta) {
    return (
      <section className={`card flex h-full min-h-[320px] flex-col items-center justify-center gap-3 p-6 text-center ${className}`}>
        <span className="flex h-12 w-12 items-center justify-center rounded-2xl bg-[var(--cor-superficie-suave)] text-[var(--cor-texto-suave)]">
          <Waves className="h-6 w-6" aria-hidden="true" />
        </span>
        <h2 className="text-lg">Sem dados deste jardim</h2>
        <p className="max-w-xs text-sm text-[var(--cor-texto-suave)]">
          Não foi possível carregar o estado atual. Verifique a conexão com a API e tente
          novamente.
        </p>
        {onTentarNovamente && (
          <button type="button" onClick={onTentarNovamente} className="botao-suave mt-1">
            Tentar novamente
          </button>
        )}
      </section>
    )
  }

  const { data, hora } = formatarDataHora(jardim.ultimaLeituraEm ?? '')
  const nivel = alerta.nivel
  const recomendacoes = RECOMENDACOES[nivel]

  return (
    <section
      style={criarEstiloAlerta({ nivel })}
      className={`card-alerta relative flex h-full flex-col overflow-hidden rounded-2xl border p-5 shadow-sm sm:p-6 ${className}`}
      aria-labelledby="titulo-status-jardim"
    >
      <div
        aria-hidden="true"
        className="pointer-events-none absolute -right-16 -top-20 h-52 w-52 rounded-full blur-3xl"
        style={{ backgroundColor: 'var(--alerta-fundo)' }}
      />

      <header className="relative flex items-start justify-between gap-3">
        <div>
          <p className="kicker">Monitoramento</p>
          <h2 id="titulo-status-jardim" className="mt-1.5 text-2xl sm:text-[26px]">
            Jardim {jardim.jardimId}
          </h2>
        </div>
        <span
          className="flex h-11 w-11 items-center justify-center rounded-xl border"
          style={{
            borderColor: 'var(--alerta-borda)',
            backgroundColor: 'var(--alerta-fundo)',
            color: 'var(--alerta-cor)',
          }}
        >
          <Flower2 className="h-5 w-5" aria-hidden="true" />
        </span>
      </header>

      <p className="relative mt-2 flex flex-wrap items-center gap-x-3 gap-y-1 text-xs text-[var(--cor-texto-suave)]">
        Informações atualizadas em:
        <span className="inline-flex items-center gap-1 font-medium text-[var(--cor-texto)]">
          <CalendarDays className="h-3.5 w-3.5" aria-hidden="true" />
          Data {data}
        </span>
        <span className="inline-flex items-center gap-1 font-medium text-[var(--cor-texto)]">
          <Clock3 className="h-3.5 w-3.5" aria-hidden="true" />
          Hora {hora}
        </span>
      </p>

      <div className="relative mt-6 grid flex-1 gap-5 sm:grid-cols-[auto_minmax(0,1fr)] sm:gap-6">
        <Semaforo nivelAtivo={nivel} />

        <div className="flex min-w-0 flex-col">
          <span
            className="inline-flex w-fit items-center gap-1.5 rounded-full border px-2.5 py-1 text-[10px] font-bold uppercase tracking-[0.12em]"
            style={{
              borderColor: 'var(--alerta-borda)',
              backgroundColor: 'var(--alerta-fundo)',
              color: 'var(--alerta-cor)',
            }}
          >
            {nivel === 'NORMAL' ? (
              <ShieldCheck className="h-3.5 w-3.5" aria-hidden="true" />
            ) : (
              <AlertTriangle className="h-3.5 w-3.5" aria-hidden="true" />
            )}
            {nivel === 'NORMAL' ? 'Estado normal' : 'Estado de alerta'}
          </span>

          <h3 className="mt-3 text-xl leading-tight sm:text-[22px]">
            Estado Atual:{' '}
            <span style={{ color: 'var(--alerta-cor)' }}>{alerta.titulo}</span>
          </h3>
          <p className="mt-1 text-sm font-medium text-[var(--cor-texto)]">
            {nivel === 'NORMAL' ? 'Monitoramento regular' : 'Estado de alerta'} — {alerta.subtitulo}
          </p>

          <p className="mt-3 text-[13px] leading-relaxed text-[var(--cor-texto-suave)]">
            {alerta.descricao} {DETALHES[nivel]}
          </p>

          <div className="mt-4 rounded-xl border border-[var(--cor-borda)]/80 bg-[var(--cor-superficie)]/80 p-3.5 backdrop-blur-sm">
            <h4 className="flex items-center gap-1.5 text-[11px] font-bold uppercase tracking-[0.12em] text-[var(--cor-texto-forte)]">
              <ShieldCheck
                className="h-3.5 w-3.5"
                style={{ color: 'var(--alerta-cor)' }}
                aria-hidden="true"
              />
              Recomendações preventivas
            </h4>
            <ul className="mt-2.5 space-y-2 text-xs leading-snug text-[var(--cor-texto-suave)]">
              {recomendacoes.map((recomendacao) => (
                <li key={recomendacao} className="flex gap-2">
                  <span
                    aria-hidden="true"
                    className="mt-1.5 h-1.5 w-1.5 shrink-0 rounded-full"
                    style={{ backgroundColor: 'var(--alerta-cor)' }}
                  />
                  {recomendacao}
                </li>
              ))}
            </ul>
          </div>
        </div>
      </div>

      <footer className="relative mt-5 flex flex-wrap items-center justify-between gap-2 border-t border-[var(--cor-borda)]/70 pt-3 text-[11px] text-[var(--cor-texto-suave)]">
        <span className="inline-flex items-center gap-1.5">
          <Waves className="h-3.5 w-3.5" aria-hidden="true" />
          Sensor {jardim.sensorId}
        </span>
        <span>
          {jardim.totalLeituras.toLocaleString('pt-BR')} leitura
          {jardim.totalLeituras === 1 ? '' : 's'} recebida
          {jardim.totalLeituras === 1 ? '' : 's'}
        </span>
      </footer>
    </section>
  )
}

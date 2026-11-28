import { useCallback, useEffect, useRef, useState } from 'react'
import { fetchJardim } from '../services/api'
import type { JardimResponse } from '../types/jardim'

/** Jardins exigidos no critério de aceite (J01–J04). */
export const JARDINS_MONITORADOS = ['J01', 'J02', 'J03', 'J04'] as const
export type JardimMonitoradoId = (typeof JARDINS_MONITORADOS)[number]

export interface JardimRedeItem {
  jardimId: JardimMonitoradoId
  jardim: JardimResponse | null
  /** Falha específica deste jardim (por exemplo, 404 = ainda sem leituras). */
  erro: Error | null
}

export interface UseJardinsRedeOptions {
  /** Intervalo do polling automático em milissegundos. Padrão: 5000. */
  intervaloPollingMs?: number
  /** Permite pausar o polling sem desmontar o componente. Padrão: true. */
  ativo?: boolean
}

export interface UseJardinsRedeResultado {
  itens: JardimRedeItem[]
  loading: boolean
  /** Preenchido apenas quando nenhum jardim pôde ser consultado. */
  error: Error | null
  refetch: () => Promise<void>
}

function itensVazios(ids: readonly JardimMonitoradoId[]): JardimRedeItem[] {
  return ids.map((jardimId) => ({ jardimId, jardim: null, erro: null }))
}

/**
 * Consulta os jardins monitorados em paralelo (`GET /api/jardins/{id}`).
 *
 * Usamos o endpoint de detalhe (e não a lista) porque é ele que traz
 * `tendencia`; a lista `GET /api/jardins` não inclui esse campo. Uma falha em um
 * jardim não derruba os demais: cada item carrega seu próprio `erro`.
 */
export function useJardinsRede(
  ids: readonly JardimMonitoradoId[] = JARDINS_MONITORADOS,
  opcoes: UseJardinsRedeOptions = {},
): UseJardinsRedeResultado {
  const intervaloPollingMs = opcoes.intervaloPollingMs ?? 5000
  const ativo = opcoes.ativo ?? true
  const idsKey = ids.join(',')

  const [itens, setItens] = useState<JardimRedeItem[]>(() => itensVazios(ids))
  const [loading, setLoading] = useState<boolean>(ids.length > 0)
  const [error, setError] = useState<Error | null>(null)

  const consultaEmAndamentoRef = useRef(false)

  const carregar = useCallback(
    async (mostrarLoading: boolean) => {
      const lista = idsKey ? (idsKey.split(',') as JardimMonitoradoId[]) : []
      if (lista.length === 0) {
        setItens([])
        setError(null)
        setLoading(false)
        return
      }
      if (consultaEmAndamentoRef.current) {
        return
      }

      consultaEmAndamentoRef.current = true
      if (mostrarLoading) {
        setLoading(true)
      }

      try {
        const resultados = await Promise.allSettled(
          lista.map((jardimId) => fetchJardim(jardimId)),
        )

        setItens((anteriores) =>
          lista.map((jardimId, indice) => {
            const resultado = resultados[indice]
            if (resultado.status === 'fulfilled') {
              return { jardimId, jardim: resultado.value, erro: null }
            }

            const anterior = anteriores.find((item) => item.jardimId === jardimId)
            return {
              jardimId,
              // Mantém o último dado válido enquanto a API se recupera.
              jardim: anterior?.jardim ?? null,
              erro:
                resultado.reason instanceof Error
                  ? resultado.reason
                  : new Error('Falha ao consultar o jardim.'),
            }
          }),
        )

        const todosFalharam = resultados.every(
          (resultado) => resultado.status === 'rejected',
        )
        setError(
          todosFalharam && resultados[0]?.status === 'rejected'
            ? resultados[0].reason instanceof Error
              ? resultados[0].reason
              : new Error('Falha ao consultar os jardins.')
            : null,
        )
      } finally {
        consultaEmAndamentoRef.current = false
        setLoading(false)
      }
    },
    [idsKey],
  )

  useEffect(() => {
    if (!ativo) {
      setLoading(false)
      return
    }

    void carregar(true)

    const intervalo = globalThis.setInterval(
      () => {
        void carregar(false)
      },
      Math.max(1000, intervaloPollingMs),
    )

    return () => globalThis.clearInterval(intervalo)
  }, [ativo, carregar, intervaloPollingMs])

  const refetch = useCallback(async () => {
    await carregar(true)
  }, [carregar])

  return { itens, loading, error, refetch }
}

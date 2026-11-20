import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { fetchDadosCOP, fetchHistorico, fetchJardim } from '../services/api'
import { alertaDoJardim } from '../types/jardim'
import type {
  DadosCOP,
  InformacoesAlerta,
  JardimResponse,
  Leitura,
} from '../types/jardim'

export interface UseJardimDataOptions {
  /** Intervalo do polling automático em milissegundos. Padrão: 5000. */
  intervaloPollingMs?: number
  /** Quantidade máxima de leituras no histórico. Padrão: 30. */
  limiteHistorico?: number
  /** Permite pausar o polling sem desmontar o componente. Padrão: true. */
  ativo?: boolean
}

export interface UseJardimDataResultado {
  jardim: JardimResponse | null
  historico: Leitura[]
  alerta: InformacoesAlerta | null
  cop: DadosCOP
  loading: boolean
  error: Error | null
  refetch: () => Promise<void>
}

/**
 * Carrega estado atual + histórico de um jardim direto da API, com polling.
 * Em caso de falha, mantém os últimos dados válidos e expõe `error` para a
 * interface mostrar o aviso e o botão "Tentar novamente".
 */
export function useJardimData(
  jardimId: string,
  opcoes: UseJardimDataOptions = {},
): UseJardimDataResultado {
  const intervaloPollingMs = opcoes.intervaloPollingMs ?? 5000
  const limiteHistorico = opcoes.limiteHistorico ?? 30
  const ativo = opcoes.ativo ?? true

  const [jardim, setJardim] = useState<JardimResponse | null>(null)
  const [historico, setHistorico] = useState<Leitura[]>([])
  const [loading, setLoading] = useState<boolean>(Boolean(jardimId))
  const [error, setError] = useState<Error | null>(null)

  const jardimIdAtualRef = useRef(jardimId)
  jardimIdAtualRef.current = jardimId

  // Evita polling sobreposto para o mesmo jardim; trocar de jardim continua livre.
  const consultaEmAndamentoRef = useRef<string | null>(null)

  const carregar = useCallback(
    async (mostrarLoading: boolean) => {
      if (!jardimId) {
        setJardim(null)
        setHistorico([])
        setError(null)
        setLoading(false)
        return
      }

      if (consultaEmAndamentoRef.current === jardimId) {
        return
      }

      consultaEmAndamentoRef.current = jardimId
      if (mostrarLoading) {
        setLoading(true)
      }

      try {
        const [jardimAtualizado, historicoAtualizado] = await Promise.all([
          fetchJardim(jardimId),
          fetchHistorico(jardimId, limiteHistorico),
        ])

        if (jardimIdAtualRef.current !== jardimId) {
          return
        }

        setJardim(jardimAtualizado)
        setHistorico(historicoAtualizado)
        setError(null)
      } catch (erro) {
        if (jardimIdAtualRef.current !== jardimId) {
          return
        }

        setError(
          erro instanceof Error
            ? erro
            : new Error('Falha ao carregar os dados do jardim.'),
        )
      } finally {
        if (consultaEmAndamentoRef.current === jardimId) {
          consultaEmAndamentoRef.current = null
        }
        if (jardimIdAtualRef.current === jardimId) {
          setLoading(false)
        }
      }
    },
    [jardimId, limiteHistorico],
  )

  // Ao trocar de jardim, limpa os dados antigos antes da próxima carga.
  useEffect(() => {
    setJardim(null)
    setHistorico([])
    setError(null)
    setLoading(Boolean(jardimId))
  }, [jardimId])

  useEffect(() => {
    if (!ativo || !jardimId) {
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
  }, [ativo, carregar, jardimId, intervaloPollingMs])

  const refetch = useCallback(async () => {
    await carregar(true)
  }, [carregar])

  const alerta = useMemo<InformacoesAlerta | null>(
    () => (jardim ? alertaDoJardim(jardim) : null),
    [jardim],
  )

  const cop = useMemo<DadosCOP>(() => fetchDadosCOP(), [])

  return {
    jardim,
    historico,
    alerta,
    cop,
    loading,
    error,
    refetch,
  }
}

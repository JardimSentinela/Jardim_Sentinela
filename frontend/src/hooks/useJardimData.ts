import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import {
  ApiError,
  fetchDadosCOP,
  fetchHistorico,
  fetchJardim,
} from '../services/api'
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
  /** Quantidade máxima de leituras no histórico. Padrão: 100 (o mesmo da API). */
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
  /** Falha ao carregar o estado atual. `null` em 404 (jardim ainda sem leituras). */
  erroJardim: Error | null
  /** Falha ao carregar o histórico. `null` em 404 (jardim ainda sem leituras). */
  erroHistorico: Error | null
  /**
   * A API respondeu 404 para o detalhe e para o histórico: o jardim existe no
   * cadastro, mas ainda não recebeu nenhuma leitura. Isso é estado vazio, não erro.
   */
  semLeituras: boolean
  refetch: () => Promise<void>
}

function ehJardimNaoEncontrado(erro: unknown): boolean {
  return erro instanceof ApiError && erro.status === 404
}

function comoErro(erro: unknown, mensagemPadrao: string): Error {
  return erro instanceof Error ? erro : new Error(mensagemPadrao)
}

/**
 * Carrega estado atual + histórico de um jardim direto da API, com polling.
 *
 * Uma falha transitória preserva os últimos dados válidos: o erro fica exposto
 * em `erroJardim`/`erroHistorico` e a interface avisa que a atualização falhou,
 * sem apagar cards e gráfico. Os dados só são limpos ao trocar de jardim ou
 * quando a API responde 404 (estado vazio). Jardim sem leitura (404) é
 * `semLeituras`, não erro.
 */
export function useJardimData(
  jardimId: string,
  opcoes: UseJardimDataOptions = {},
): UseJardimDataResultado {
  const intervaloPollingMs = opcoes.intervaloPollingMs ?? 5000
  const limiteHistorico = opcoes.limiteHistorico ?? 100
  const ativo = opcoes.ativo ?? true

  const [jardim, setJardim] = useState<JardimResponse | null>(null)
  const [historico, setHistorico] = useState<Leitura[]>([])
  const [loading, setLoading] = useState<boolean>(Boolean(jardimId))
  const [erroJardim, setErroJardim] = useState<Error | null>(null)
  const [erroHistorico, setErroHistorico] = useState<Error | null>(null)
  const [semLeituras, setSemLeituras] = useState(false)

  const jardimIdAtualRef = useRef(jardimId)
  jardimIdAtualRef.current = jardimId

  // Evita polling sobreposto para o mesmo jardim; trocar de jardim continua livre.
  const consultaEmAndamentoRef = useRef<string | null>(null)
  // Invalida respostas de requisições antigas (ex.: J01 → J02 → J01).
  const requisicaoRef = useRef(0)

  const carregar = useCallback(
    async (mostrarLoading: boolean) => {
      if (!jardimId) {
        setJardim(null)
        setHistorico([])
        setErroJardim(null)
        setErroHistorico(null)
        setSemLeituras(false)
        setLoading(false)
        return
      }

      if (consultaEmAndamentoRef.current === jardimId) {
        return
      }

      const requisicao = ++requisicaoRef.current
      consultaEmAndamentoRef.current = jardimId
      if (mostrarLoading) {
        setLoading(true)
      }

      try {
        const [resJardim, resHistorico] = await Promise.allSettled([
          fetchJardim(jardimId),
          fetchHistorico(jardimId, limiteHistorico, false),
        ])

        if (
          requisicao !== requisicaoRef.current ||
          jardimIdAtualRef.current !== jardimId
        ) {
          return
        }

        const detalhe404 =
          resJardim.status === 'rejected' &&
          ehJardimNaoEncontrado(resJardim.reason)
        const historico404 =
          resHistorico.status === 'rejected' &&
          ehJardimNaoEncontrado(resHistorico.reason)

        if (resJardim.status === 'fulfilled') {
          setJardim(resJardim.value)
          setErroJardim(null)
        } else if (detalhe404) {
          // 404 é estado vazio, não falha transitória: não há dado atual.
          setJardim(null)
          setErroJardim(null)
        } else {
          // Mantém o último estado válido enquanto a API se recupera.
          setErroJardim(
            comoErro(
              resJardim.reason,
              'Falha ao carregar os dados do jardim.',
            ),
          )
        }

        if (resHistorico.status === 'fulfilled') {
          setHistorico(resHistorico.value)
          setErroHistorico(null)
        } else if (historico404) {
          setHistorico([])
          setErroHistorico(null)
        } else {
          // Mantém o último histórico válido enquanto a API se recupera.
          setErroHistorico(
            comoErro(
              resHistorico.reason,
              'Falha ao carregar o histórico do jardim.',
            ),
          )
        }

        setSemLeituras(detalhe404 && historico404)
      } catch (erro) {
        if (
          requisicao !== requisicaoRef.current ||
          jardimIdAtualRef.current !== jardimId
        ) {
          return
        }

        setErroJardim(
          comoErro(erro, 'Falha ao carregar os dados do jardim.'),
        )
      } finally {
        if (requisicao === requisicaoRef.current) {
          consultaEmAndamentoRef.current = null
          if (jardimIdAtualRef.current === jardimId) {
            setLoading(false)
          }
        }
      }
    },
    [jardimId, limiteHistorico],
  )

  // Ao trocar de jardim, invalida requisições antigas e limpa os dados.
  useEffect(() => {
    requisicaoRef.current += 1
    consultaEmAndamentoRef.current = null
    setJardim(null)
    setHistorico([])
    setErroJardim(null)
    setErroHistorico(null)
    setSemLeituras(false)
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
    erroJardim,
    erroHistorico,
    semLeituras,
    refetch,
  }
}

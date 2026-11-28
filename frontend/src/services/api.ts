import type { JardimResponse, JardimResumo, Leitura } from '../types/jardim'
import { dadosCOPMock } from './mockData'

/**
 * Camada de acesso à API Java do Jardim Sentinela.
 *
 * Os dados de jardins e de histórico vêm exclusivamente da API: se ela estiver
 * fora do ar, a chamada rejeita com `ApiError` e a interface mostra o erro
 * (com opção de tentar novamente). Não há mais fallback silencioso para mock —
 * o único mock restante é o painel COP, que ainda não tem endpoint.
 */

export const API_BASE_URL = (
  import.meta.env.VITE_API_BASE_URL || 'http://localhost:8080'
).replace(/\/+$/, '')

const TIMEOUT_REQUISICAO_MS = 5000

/** Códigos estáveis para a interface distinguir timeout, rede e erros da API. */
export type CodigoErroApi =
  | 'TIMEOUT'
  | 'REDE'
  | 'RESPOSTA_INVALIDA'
  | 'JARDIM_NAO_ENCONTRADO'
  | (string & {})

/** Erro normalizado da API, com status HTTP e código devolvido pelo backend. */
export class ApiError extends Error {
  readonly status: number
  readonly codigo?: CodigoErroApi

  constructor(mensagem: string, status = 0, codigo?: CodigoErroApi) {
    super(mensagem)
    this.name = 'ApiError'
    this.status = status
    this.codigo = codigo
  }
}

interface CorpoErroApi {
  erro?: string
  mensagem?: string
}

async function requisitar<T>(caminho: string): Promise<T> {
  const controlador = new AbortController()
  const temporizador = globalThis.setTimeout(
    () => controlador.abort(),
    TIMEOUT_REQUISICAO_MS,
  )

  try {
    const resposta = await fetch(`${API_BASE_URL}${caminho}`, {
      method: 'GET',
      headers: { Accept: 'application/json' },
      signal: controlador.signal,
    })

    if (!resposta.ok) {
      let corpo: CorpoErroApi = {}
      try {
        corpo = (await resposta.json()) as CorpoErroApi
      } catch {
        // Corpo vazio ou não-JSON: mantém apenas o status HTTP.
      }
      const mensagem =
        corpo.mensagem ?? corpo.erro ?? `A API respondeu com HTTP ${resposta.status}.`
      throw new ApiError(mensagem, resposta.status, corpo.erro)
    }

    try {
      return (await resposta.json()) as T
    } catch {
      throw new ApiError(
        'A resposta da API não é um JSON válido.',
        resposta.status,
        'RESPOSTA_INVALIDA',
      )
    }
  } catch (erro) {
    if (erro instanceof ApiError) {
      throw erro
    }

    const nomeDoErro =
      typeof erro === 'object' && erro !== null && 'name' in erro
        ? String((erro as { name?: unknown }).name)
        : ''

    if (nomeDoErro === 'AbortError' || controlador.signal.aborted) {
      throw new ApiError(
        `A API não respondeu em ${TIMEOUT_REQUISICAO_MS / 1000} segundos.`,
        0,
        'TIMEOUT',
      )
    }

    throw new ApiError(
      erro instanceof Error ? erro.message : 'Falha de rede ao consultar a API.',
      0,
      'REDE',
    )
  } finally {
    globalThis.clearTimeout(temporizador)
  }
}

function exigirLista<T>(valor: unknown, descricao: string): T[] {
  if (!Array.isArray(valor)) {
    throw new ApiError(
      `Resposta inesperada para ${descricao}: esperada uma lista.`,
      0,
      'RESPOSTA_INVALIDA',
    )
  }
  return valor as T[]
}

/** Lista os jardins que já enviaram leitura (`GET /api/jardins`). */
export async function fetchJardins(): Promise<JardimResumo[]> {
  const dados = await requisitar<unknown>('/api/jardins')
  return exigirLista<JardimResumo>(dados, 'a lista de jardins')
}

/**
 * Busca o estado atual de um jardim (`GET /api/jardins/{id}`).
 * Inclui `estado` e `tendencia` quando o backend os expõe.
 */
export async function fetchJardim(id: string): Promise<JardimResponse> {
  return requisitar<JardimResponse>(`/api/jardins/${encodeURIComponent(id)}`)
}

/**
 * Busca o histórico de um jardim (`GET /api/jardins/{id}/leituras?limite=N`).
 *
 * `incluirBoot=false` exclui o registro de inicialização (`tipo: "boot"`), que
 * traz nível fictício e não é medição. O dashboard usa `false`; o padrão `true`
 * preserva o contrato anterior para outros consumidores.
 */
export async function fetchHistorico(
  id: string,
  limite = 100,
  incluirBoot = true,
): Promise<Leitura[]> {
  const limiteNormalizado = Math.min(1000, Math.max(1, Math.trunc(limite) || 1))
  const parametroBoot = incluirBoot ? '' : '&incluirBoot=false'
  const dados = await requisitar<unknown>(
    `/api/jardins/${encodeURIComponent(id)}/leituras?limite=${limiteNormalizado}${parametroBoot}`,
  )
  return exigirLista<Leitura>(dados, `o histórico do jardim ${id}`)
}

/** Busca todas as leituras recebidas (`GET /api/leituras`). */
export async function fetchTodasLeituras(): Promise<Leitura[]> {
  const dados = await requisitar<unknown>('/api/leituras')
  return exigirLista<Leitura>(dados, 'a lista de leituras')
}

/**
 * Dados do painel COP: ainda não existe endpoint na API do Jardim Sentinela,
 * então este painel continua usando `mockData.ts`.
 */
export function fetchDadosCOP() {
  return dadosCOPMock
}

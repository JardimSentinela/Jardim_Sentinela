import type { JardimResponse, Leitura } from '../types/jardim'
import {
  buscarHistoricoMock,
  buscarJardimMock,
  dadosCOPMock,
  todasLeiturasMock,
} from './mockData'

/**
 * Camada de acesso à API Java do Jardim Sentinela.
 *
 * Toda chamada tem fallback gracioso para `mockData.ts`: se a API estiver fora
 * do ar, demorar demais ou bloquear a requisição por CORS, o dashboard continua
 * funcionando e o console avisa que os dados exibidos são de demonstração.
 */

export const API_BASE_URL = (
  import.meta.env.VITE_API_BASE_URL || 'http://localhost:8080'
).replace(/\/+$/, '')

const TIMEOUT_REQUISICAO_MS = 5000

let loteUsouDadosMock = false

/**
 * Marca o início de um conjunto de consultas (por exemplo, o `Promise.all` de
 * um hook) para que o consumidor saiba se alguma delas precisou do mock.
 */
export function iniciarLoteDeConsultas(): void {
  loteUsouDadosMock = false
}

/** Indica se alguma consulta do último lote caiu no fallback mockado. */
export function ultimoLoteUsouMock(): boolean {
  return loteUsouDadosMock
}

/** Atalho para quando uma consulta isolada é feita fora de um lote. */
export function apiEstaUsandoMock(): boolean {
  return loteUsouDadosMock
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
      let detalhe = `HTTP ${resposta.status}`
      try {
        const corpo = (await resposta.json()) as { mensagem?: string; erro?: string }
        const mensagem = corpo.mensagem ?? corpo.erro
        if (mensagem) {
          detalhe += ` - ${mensagem}`
        }
      } catch {
        // Corpo vazio ou não-JSON: mantém apenas o status HTTP.
      }
      throw new Error(detalhe)
    }

    return (await resposta.json()) as T
  } finally {
    globalThis.clearTimeout(temporizador)
  }
}

function usarFallback<T>(descricao: string, erro: unknown, dados: T): T {
  loteUsouDadosMock = true
  const motivo = erro instanceof Error ? erro.message : String(erro)
  console.warn(
    `[Jardim Sentinela] ${descricao} indisponível (${motivo}). ` +
      'Exibindo dados mockados para manter o dashboard funcional.',
  )
  return dados
}

/** Busca o estado atual de um jardim (`GET /api/jardins/{id}`). */
export async function fetchJardim(id: string): Promise<JardimResponse> {
  const descricao = `Estado do jardim ${id}`
  try {
    const jardim = await requisitar<JardimResponse>(
      `/api/jardins/${encodeURIComponent(id)}`,
    )
    return jardim
  } catch (erro) {
    const mock = buscarJardimMock(id)
    if (!mock) {
      throw new Error(`Jardim "${id}" não encontrado na API nem nos dados mockados.`)
    }
    return usarFallback(descricao, erro, mock)
  }
}

/** Busca o histórico de um jardim (`GET /api/jardins/{id}/leituras?limite=N`). */
export async function fetchHistorico(
  id: string,
  limite = 100,
): Promise<Leitura[]> {
  const limiteNormalizado = Math.min(1000, Math.max(1, Math.trunc(limite) || 1))
  const descricao = `Histórico do jardim ${id}`

  try {
    const leituras = await requisitar<Leitura[]>(
      `/api/jardins/${encodeURIComponent(id)}/leituras?limite=${limiteNormalizado}`,
    )
    if (!Array.isArray(leituras)) {
      throw new Error('Resposta da API não é uma lista de leituras.')
    }
    return leituras
  } catch (erro) {
    return usarFallback(
      descricao,
      erro,
      buscarHistoricoMock(id, limiteNormalizado),
    )
  }
}

/** Busca todas as leituras recebidas (`GET /api/leituras`). */
export async function fetchTodasLeituras(): Promise<Leitura[]> {
  try {
    const leituras = await requisitar<Leitura[]>('/api/leituras')
    if (!Array.isArray(leituras)) {
      throw new Error('Resposta da API não é uma lista de leituras.')
    }
    return leituras
  } catch (erro) {
    return usarFallback('Lista completa de leituras', erro, todasLeiturasMock)
  }
}

/** Dados do painel COP (ainda sem endpoint na API; sempre vem do mock). */
export function fetchDadosCOP() {
  loteUsouDadosMock = true
  return dadosCOPMock
}

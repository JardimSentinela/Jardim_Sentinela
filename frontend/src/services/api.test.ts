import { afterEach, describe, expect, it, vi } from 'vitest'
import {
  API_BASE_URL,
  ApiError,
  fetchHistorico,
  fetchJardim,
  fetchJardins,
  fetchTodasLeituras,
} from './api'
import type { JardimResponse, JardimResumo, Leitura } from '../types/jardim'

function resposta(corpo: unknown, status = 200): Response {
  return {
    ok: status >= 200 && status < 300,
    status,
    json: async () => corpo,
  } as unknown as Response
}

const jardim: JardimResponse = {
  jardimId: 'J01',
  sensorId: 'S01',
  nivelCm: 18.5,
  bateria: 97,
  ultimaLeituraEm: '2026-11-19T10:00:00Z',
  totalLeituras: 12,
  estado: 'NORMAL',
  tendencia: 'SUBINDO',
}

const resumo: JardimResumo = {
  jardimId: 'J01',
  nivelCm: 18.5,
  estado: 'NORMAL',
  ultimaLeituraEm: '2026-11-19T10:00:00Z',
  totalLeituras: 12,
}

const leitura: Leitura = {
  id: 12,
  jardimId: 'J01',
  sensorId: 'S01',
  nivelCm: 18.5,
  bateria: 97,
  recebidoEm: '2026-11-19T10:00:01Z',
  timestampDispositivo: '2026-11-19T10:00:00Z',
}

afterEach(() => {
  vi.useRealTimers()
  vi.unstubAllGlobals()
})

describe('API do Jardim Sentinela', () => {
  it('busca o estado atual no endpoint de detalhe e preserva estado/tendência', async () => {
    const fetchMock = vi.fn().mockResolvedValue(resposta(jardim))
    vi.stubGlobal('fetch', fetchMock)

    const resultado = await fetchJardim('J01')

    expect(resultado).toEqual(jardim)
    expect(fetchMock).toHaveBeenCalledWith(
      `${API_BASE_URL}/api/jardins/J01`,
      expect.objectContaining({
        method: 'GET',
        headers: { Accept: 'application/json' },
      }),
    )
  })

  it('busca a lista de jardins em GET /api/jardins', async () => {
    const fetchMock = vi.fn().mockResolvedValue(resposta([resumo]))
    vi.stubGlobal('fetch', fetchMock)

    await expect(fetchJardins()).resolves.toEqual([resumo])
    expect(fetchMock).toHaveBeenCalledWith(
      `${API_BASE_URL}/api/jardins`,
      expect.anything(),
    )
  })

  it('busca o histórico com limite normalizado', async () => {
    const fetchMock = vi.fn().mockResolvedValue(resposta([leitura]))
    vi.stubGlobal('fetch', fetchMock)

    await expect(fetchHistorico('J01', 5000)).resolves.toEqual([leitura])
    expect(fetchMock).toHaveBeenCalledWith(
      `${API_BASE_URL}/api/jardins/J01/leituras?limite=1000`,
      expect.anything(),
    )
  })

  it('busca todas as leituras em GET /api/leituras', async () => {
    const fetchMock = vi.fn().mockResolvedValue(resposta([leitura]))
    vi.stubGlobal('fetch', fetchMock)

    await expect(fetchTodasLeituras()).resolves.toEqual([leitura])
  })

  it('transforma 404 da API em ApiError com status e código do backend', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn().mockResolvedValue(
        resposta(
          {
            erro: 'JARDIM_NAO_ENCONTRADO',
            mensagem: 'Jardim J99 não encontrado.',
          },
          404,
        ),
      ),
    )

    const erro = await fetchJardim('J99').catch((e: unknown) => e)

    expect(erro).toBeInstanceOf(ApiError)
    expect(erro).toMatchObject({
      name: 'ApiError',
      status: 404,
      codigo: 'JARDIM_NAO_ENCONTRADO',
      message: 'Jardim J99 não encontrado.',
    })
  })

  it('não cai para dados mockados quando a API está indisponível', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn().mockRejectedValue(new TypeError('fetch failed')),
    )

    const erro = await fetchJardim('J01').catch((e: unknown) => e)

    expect(erro).toBeInstanceOf(ApiError)
    expect(erro).toMatchObject({ status: 0, codigo: 'REDE' })
  })

  it('rejeita resposta que não é lista em vez de exibir dado inválido', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue(resposta({})))

    const erro = await fetchJardins().catch((e: unknown) => e)

    expect(erro).toBeInstanceOf(ApiError)
    expect(erro).toMatchObject({ codigo: 'RESPOSTA_INVALIDA' })
  })

  it('interrompe a requisição e devolve TIMEOUT após 5 segundos', async () => {
    vi.useFakeTimers()
    vi.stubGlobal(
      'fetch',
      vi.fn(
        (_url: string, init: RequestInit) =>
          new Promise((_resolve, reject) => {
            init.signal?.addEventListener('abort', () => {
              reject(new DOMException('Aborted', 'AbortError'))
            })
          }),
      ),
    )

    const promessa = fetchJardim('J01').catch((e: unknown) => e)
    await vi.advanceTimersByTimeAsync(5000)
    const erro = await promessa

    expect(erro).toBeInstanceOf(ApiError)
    expect(erro).toMatchObject({ status: 0, codigo: 'TIMEOUT' })
  })
})

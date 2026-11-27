import { act, renderHook, waitFor } from '@testing-library/react'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { useJardimData } from './useJardimData'
import { ApiError, fetchHistorico, fetchJardim } from '../services/api'
import type { JardimResponse, Leitura } from '../types/jardim'

vi.mock('../services/api', async (importOriginal) => {
  const original = await importOriginal<typeof import('../services/api')>()
  return {
    ...original,
    fetchJardim: vi.fn(),
    fetchHistorico: vi.fn(),
  }
})

const fetchJardimMock = vi.mocked(fetchJardim)
const fetchHistoricoMock = vi.mocked(fetchHistorico)

const jardim: JardimResponse = {
  jardimId: 'J01',
  sensorId: 'S01',
  nivelCm: 40,
  bateria: 97,
  ultimaLeituraEm: '2026-11-19T10:00:00Z',
  totalLeituras: 12,
  estado: 'NORMAL',
  tendencia: 'SUBINDO',
}

const leitura: Leitura = {
  id: 12,
  jardimId: 'J01',
  sensorId: 'S01',
  nivelCm: 40,
  bateria: 97,
  recebidoEm: '2026-11-19T10:00:01Z',
  timestampDispositivo: '2026-11-19T10:00:00Z',
  estado: 'NORMAL',
}

interface Deferred<T> {
  promise: Promise<T>
  resolve: (valor: T) => void
}

function criarDeferred<T>(): Deferred<T> {
  let resolve!: (valor: T) => void
  const promise = new Promise<T>((resolver) => {
    resolve = resolver
  })
  return { promise, resolve }
}

beforeEach(() => {
  fetchJardimMock.mockReset()
  fetchHistoricoMock.mockReset()
})

describe('useJardimData', () => {
  it('expõe os dados reais e usa o estado da API (NORMAL → Seco)', async () => {
    fetchJardimMock.mockResolvedValue(jardim)
    fetchHistoricoMock.mockResolvedValue([leitura])

    const { result } = renderHook(() =>
      useJardimData('J01', { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))

    expect(result.current.erroJardim).toBeNull()
    expect(result.current.erroHistorico).toBeNull()
    expect(result.current.semLeituras).toBe(false)
    expect(result.current.jardim).toEqual(jardim)
    expect(result.current.historico).toEqual([leitura])
    expect(result.current.alerta?.nivel).toBe('SECO')
    expect(fetchJardimMock).toHaveBeenCalledWith('J01')
    expect(fetchHistoricoMock).toHaveBeenCalledWith('J01', 100, false)
  })

  it('expõe erro quando a API está indisponível, sem inventar dados', async () => {
    fetchJardimMock.mockRejectedValue(new ApiError('API fora do ar', 0, 'REDE'))
    fetchHistoricoMock.mockRejectedValue(new ApiError('API fora do ar', 0, 'REDE'))

    const { result } = renderHook(() =>
      useJardimData('J01', { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))

    expect(result.current.jardim).toBeNull()
    expect(result.current.historico).toEqual([])
    expect(result.current.alerta).toBeNull()
    expect(result.current.erroJardim).toBeInstanceOf(ApiError)
    expect(result.current.erroHistorico).toBeInstanceOf(ApiError)
    expect(result.current.semLeituras).toBe(false)
  })

  it('trata 404 do detalhe e do histórico como jardim sem leituras', async () => {
    const erro404 = new ApiError(
      'Jardim J01 não encontrado.',
      404,
      'JARDIM_NAO_ENCONTRADO',
    )
    fetchJardimMock.mockRejectedValue(erro404)
    fetchHistoricoMock.mockRejectedValue(erro404)

    const { result } = renderHook(() =>
      useJardimData('J01', { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))

    expect(result.current.semLeituras).toBe(true)
    expect(result.current.erroJardim).toBeNull()
    expect(result.current.erroHistorico).toBeNull()
    expect(result.current.jardim).toBeNull()
    expect(result.current.historico).toEqual([])
  })

  it('limpa os dados quando a API passa a responder 404', async () => {
    fetchJardimMock.mockResolvedValueOnce(jardim)
    fetchHistoricoMock.mockResolvedValueOnce([leitura])

    const { result } = renderHook(() =>
      useJardimData('J01', { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))
    expect(result.current.jardim).toEqual(jardim)

    const erro404 = new ApiError(
      'Jardim J01 não encontrado.',
      404,
      'JARDIM_NAO_ENCONTRADO',
    )
    fetchJardimMock.mockRejectedValueOnce(erro404)
    fetchHistoricoMock.mockRejectedValueOnce(erro404)

    await act(async () => {
      await result.current.refetch()
    })

    expect(result.current.semLeituras).toBe(true)
    expect(result.current.jardim).toBeNull()
    expect(result.current.historico).toEqual([])
    expect(result.current.erroJardim).toBeNull()
    expect(result.current.erroHistorico).toBeNull()
  })

  it('limpa dados e recarrega ao trocar o jardim selecionado', async () => {
    fetchJardimMock.mockResolvedValue(jardim)
    fetchHistoricoMock.mockResolvedValue([leitura])

    let idAtual = 'J01'
    const { result, rerender } = renderHook(() =>
      useJardimData(idAtual, { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))
    expect(result.current.jardim?.jardimId).toBe('J01')

    const jardimJ02: JardimResponse = {
      ...jardim,
      jardimId: 'J02',
      nivelCm: 28,
      estado: 'ENCHENDO',
    }
    const leituraJ02: Leitura = {
      ...leitura,
      id: 99,
      jardimId: 'J02',
      nivelCm: 28,
    }

    fetchJardimMock.mockResolvedValue(jardimJ02)
    fetchHistoricoMock.mockResolvedValue([leituraJ02])

    idAtual = 'J02'
    rerender()

    await waitFor(() => expect(result.current.loading).toBe(false))
    expect(result.current.jardim?.jardimId).toBe('J02')
    expect(result.current.historico).toEqual([leituraJ02])
    expect(result.current.alerta?.nivel).toBe('ENCHENDO')
  })

  it('mantém o jardim quando apenas o histórico falha após um sucesso', async () => {
    fetchJardimMock.mockResolvedValue(jardim)
    fetchHistoricoMock.mockResolvedValueOnce([leitura])

    const { result } = renderHook(() =>
      useJardimData('J01', { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))
    expect(result.current.historico).toEqual([leitura])

    fetchHistoricoMock.mockRejectedValueOnce(
      new ApiError('Histórico indisponível', 500, 'ERRO_HISTORICO'),
    )

    await act(async () => {
      await result.current.refetch()
    })

    expect(result.current.jardim).toEqual(jardim)
    expect(result.current.historico).toEqual([leitura])
    expect(result.current.erroJardim).toBeNull()
    expect(result.current.erroHistorico).toBeInstanceOf(ApiError)
  })

  it('preserva jardim e histórico quando a falha total acontece após um sucesso', async () => {
    fetchJardimMock.mockResolvedValueOnce(jardim)
    fetchHistoricoMock.mockResolvedValueOnce([leitura])

    const { result } = renderHook(() =>
      useJardimData('J01', { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))

    fetchJardimMock.mockRejectedValueOnce(
      new ApiError('API fora do ar', 0, 'REDE'),
    )
    fetchHistoricoMock.mockRejectedValueOnce(
      new ApiError('API fora do ar', 0, 'REDE'),
    )

    await act(async () => {
      await result.current.refetch()
    })

    expect(result.current.jardim).toEqual(jardim)
    expect(result.current.historico).toEqual([leitura])
    expect(result.current.erroJardim).toBeInstanceOf(ApiError)
    expect(result.current.erroHistorico).toBeInstanceOf(ApiError)
  })

  it('separa a falha do detalhe da falha do histórico', async () => {
    fetchJardimMock.mockRejectedValue(
      new ApiError('Detalhe indisponível', 500, 'ERRO_DETALHE'),
    )
    fetchHistoricoMock.mockResolvedValue([leitura])

    const { result } = renderHook(() =>
      useJardimData('J01', { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))

    expect(result.current.jardim).toBeNull()
    expect(result.current.historico).toEqual([leitura])
    expect(result.current.erroJardim).toBeInstanceOf(ApiError)
    expect(result.current.erroHistorico).toBeNull()
  })

  it('separa a falha do histórico da falha do detalhe', async () => {
    fetchJardimMock.mockResolvedValue(jardim)
    fetchHistoricoMock.mockRejectedValue(
      new ApiError('Histórico indisponível', 500, 'ERRO_HISTORICO'),
    )

    const { result } = renderHook(() =>
      useJardimData('J01', { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))

    expect(result.current.jardim).toEqual(jardim)
    expect(result.current.historico).toEqual([])
    expect(result.current.erroJardim).toBeNull()
    expect(result.current.erroHistorico).toBeInstanceOf(ApiError)
  })

  it('ignora a resposta antiga de J01 ao voltar para o jardim (J01 → J02 → J01)', async () => {
    const detalheJ01Antigo = criarDeferred<JardimResponse>()
    const historicoJ01Antigo = criarDeferred<Leitura[]>()
    const detalheJ01Novo = criarDeferred<JardimResponse>()
    const historicoJ01Novo = criarDeferred<Leitura[]>()
    let chamadasJ01 = 0

    const jardimJ02: JardimResponse = {
      ...jardim,
      jardimId: 'J02',
      nivelCm: 28,
      estado: 'ENCHENDO',
    }
    const leituraJ02: Leitura = {
      ...leitura,
      id: 99,
      jardimId: 'J02',
      nivelCm: 28,
    }

    fetchJardimMock.mockImplementation((id) => {
      if (id === 'J02') {
        return Promise.resolve(jardimJ02)
      }
      chamadasJ01 += 1
      return chamadasJ01 === 1
        ? detalheJ01Antigo.promise
        : detalheJ01Novo.promise
    })
    fetchHistoricoMock.mockImplementation((id) => {
      if (id === 'J02') {
        return Promise.resolve([leituraJ02])
      }
      return chamadasJ01 === 1
        ? historicoJ01Antigo.promise
        : historicoJ01Novo.promise
    })

    let idAtual = 'J01'
    const { result, rerender } = renderHook(() =>
      useJardimData(idAtual, { intervaloPollingMs: 60_000 }),
    )

    idAtual = 'J02'
    rerender()
    idAtual = 'J01'
    rerender()

    await act(async () => {
      detalheJ01Novo.resolve({ ...jardim, nivelCm: 11 })
      historicoJ01Novo.resolve([{ ...leitura, nivelCm: 11 }])
    })

    await waitFor(() => expect(result.current.jardim?.nivelCm).toBe(11))

    await act(async () => {
      detalheJ01Antigo.resolve({ ...jardim, nivelCm: 99 })
      historicoJ01Antigo.resolve([{ ...leitura, nivelCm: 99 }])
      await Promise.resolve()
    })

    expect(result.current.jardim?.nivelCm).toBe(11)
    expect(result.current.historico[0]?.nivelCm).toBe(11)
  })
})

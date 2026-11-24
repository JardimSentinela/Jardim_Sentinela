import { renderHook, waitFor } from '@testing-library/react'
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

    expect(result.current.error).toBeNull()
    expect(result.current.jardim).toEqual(jardim)
    expect(result.current.historico).toEqual([leitura])
    expect(result.current.alerta?.nivel).toBe('SECO')
    expect(fetchJardimMock).toHaveBeenCalledWith('J01')
    expect(fetchHistoricoMock).toHaveBeenCalledWith('J01', 30)
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
    expect(result.current.error).toBeInstanceOf(ApiError)
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

  it('mantém os dados do jardim quando apenas o histórico falha na consulta', async () => {
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
    expect(result.current.error).toBeInstanceOf(ApiError)
  })
})

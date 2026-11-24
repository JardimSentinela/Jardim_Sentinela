import { renderHook, waitFor } from '@testing-library/react'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import {
  JARDINS_MONITORADOS,
  useJardinsRede,
} from './useJardinsRede'
import { fetchJardim } from '../services/api'
import { ApiError } from '../services/api'
import type { JardimResponse } from '../types/jardim'

vi.mock('../services/api', async (importOriginal) => {
  const original = await importOriginal<typeof import('../services/api')>()
  return {
    ...original,
    fetchJardim: vi.fn(),
  }
})

const fetchJardimMock = vi.mocked(fetchJardim)

const JARDINS: Record<string, JardimResponse> = {
  J01: {
    jardimId: 'J01',
    sensorId: 'S01',
    nivelCm: 18.5,
    bateria: 97,
    ultimaLeituraEm: '2026-11-19T10:00:00Z',
    totalLeituras: 10,
    estado: 'NORMAL',
    tendencia: 'SUBINDO',
  },
  J02: {
    jardimId: 'J02',
    sensorId: 'S02',
    nivelCm: 27.8,
    bateria: 88,
    ultimaLeituraEm: '2026-11-19T10:00:05Z',
    totalLeituras: 9,
    estado: 'ENCHENDO',
    tendencia: 'ESTAVEL',
  },
  J03: {
    jardimId: 'J03',
    sensorId: 'S03',
    nivelCm: 34.2,
    bateria: 76,
    ultimaLeituraEm: '2026-11-19T10:00:10Z',
    totalLeituras: 8,
    estado: 'TRANSBORDANDO',
    tendencia: 'DESCENDO',
  },
  J04: {
    jardimId: 'J04',
    sensorId: 'S04',
    nivelCm: 9.4,
    bateria: 91,
    ultimaLeituraEm: '2026-11-19T10:00:15Z',
    totalLeituras: 7,
    estado: null,
    tendencia: 'INSUFICIENTE',
  },
}

beforeEach(() => {
  fetchJardimMock.mockReset()
})

describe('useJardinsRede', () => {
  it('carrega J01–J04 em paralelo com nível, estado, tendência e última atualização', async () => {
    fetchJardimMock.mockImplementation(async (id) => JARDINS[id])

    const { result } = renderHook(() =>
      useJardinsRede(JARDINS_MONITORADOS, { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))

    expect(result.current.error).toBeNull()
    expect(result.current.itens.map((item) => item.jardimId)).toEqual([
      'J01',
      'J02',
      'J03',
      'J04',
    ])
    expect(result.current.itens.map((item) => item.jardim?.tendencia)).toEqual([
      'SUBINDO',
      'ESTAVEL',
      'DESCENDO',
      'INSUFICIENTE',
    ])
    expect(result.current.itens[0]?.jardim).toMatchObject({
      nivelCm: 18.5,
      estado: 'NORMAL',
      ultimaLeituraEm: '2026-11-19T10:00:00Z',
    })
  })

  it('isola a falha de um jardim sem derrubar os demais', async () => {
    fetchJardimMock.mockImplementation(async (id) => {
      if (id === 'J02') {
        throw new ApiError('Jardim sem leituras.', 404, 'JARDIM_NAO_ENCONTRADO')
      }
      return JARDINS[id]
    })

    const { result } = renderHook(() =>
      useJardinsRede(JARDINS_MONITORADOS, { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))

    expect(result.current.error).toBeNull()
    const j02 = result.current.itens.find((item) => item.jardimId === 'J02')
    expect(j02?.jardim).toBeNull()
    expect(j02?.erro).toBeInstanceOf(ApiError)
    expect(result.current.itens.filter((item) => item.jardim)).toHaveLength(3)
  })

  it('expõe erro global quando nenhum jardim pôde ser consultado', async () => {
    fetchJardimMock.mockRejectedValue(new ApiError('API fora do ar', 0, 'REDE'))

    const { result } = renderHook(() =>
      useJardinsRede(JARDINS_MONITORADOS, { intervaloPollingMs: 60_000 }),
    )

    await waitFor(() => expect(result.current.loading).toBe(false))

    expect(result.current.error).toBeInstanceOf(ApiError)
    expect(result.current.itens.every((item) => item.jardim === null)).toBe(true)
  })
})

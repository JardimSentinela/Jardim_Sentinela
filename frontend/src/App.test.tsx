import { render, screen, within, waitFor } from '@testing-library/react'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import App from './App'
import { fetchHistorico, fetchJardim } from './services/api'
import type { JardimResponse, Leitura } from './types/jardim'

vi.mock('./services/api', async (importOriginal) => {
  const original = await importOriginal<typeof import('./services/api')>()
  return {
    ...original,
    fetchJardim: vi.fn(),
    fetchHistorico: vi.fn(),
  }
})

const fetchJardimMock = vi.mocked(fetchJardim)
const fetchHistoricoMock = vi.mocked(fetchHistorico)

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
    ultimaLeituraEm: '2026-11-19T10:05:00Z',
    totalLeituras: 9,
    estado: 'ENCHENDO',
    tendencia: 'ESTAVEL',
  },
  J03: {
    jardimId: 'J03',
    sensorId: 'S03',
    nivelCm: 34.2,
    bateria: 76,
    ultimaLeituraEm: '2026-11-19T10:10:00Z',
    totalLeituras: 8,
    estado: 'TRANSBORDANDO',
    tendencia: 'DESCENDO',
  },
  J04: {
    jardimId: 'J04',
    sensorId: 'S04',
    nivelCm: 9.4,
    bateria: 91,
    ultimaLeituraEm: '2026-11-19T10:15:00Z',
    totalLeituras: 7,
    estado: null,
    tendencia: 'INSUFICIENTE',
  },
}

const HISTORICO: Leitura[] = [
  {
    id: 2,
    jardimId: 'J01',
    sensorId: 'S01',
    nivelCm: 18.5,
    bateria: 97,
    recebidoEm: '2026-11-19T10:00:01Z',
    timestampDispositivo: '2026-11-19T10:00:00Z',
    estado: 'NORMAL',
  },
  {
    id: 1,
    jardimId: 'J01',
    sensorId: 'S01',
    nivelCm: 17,
    bateria: 97,
    recebidoEm: '2026-11-19T09:55:01Z',
    timestampDispositivo: '2026-11-19T09:55:00Z',
    estado: 'NORMAL',
  },
]

beforeEach(() => {
  window.localStorage.clear()
  fetchJardimMock.mockReset()
  fetchHistoricoMock.mockReset()
  fetchJardimMock.mockImplementation(async (id) => JARDINS[id])
  fetchHistoricoMock.mockResolvedValue(HISTORICO)
})

describe('App — integração com a API', () => {
  it('exibe J01–J04 com nível, estado, tendência e última atualização', async () => {
    render(<App />)

    const rede = await screen.findByRole('region', { name: 'Rede de Jardins' })
    await waitFor(() =>
      expect(
        within(rede).getAllByRole('button', { name: /Selecionar Jardim/ }),
      ).toHaveLength(4),
    )

    const esperados = [
      { id: 'J01', estado: 'Seco', tendencia: 'Subindo', nivel: '18,5 cm' },
      { id: 'J02', estado: 'Enchendo', tendencia: 'Estável', nivel: '27,8 cm' },
      {
        id: 'J03',
        estado: 'Transbordando',
        tendencia: 'Descendo',
        nivel: '34,2 cm',
      },
      { id: 'J04', estado: 'Seco', tendencia: 'Poucos dados', nivel: '9,4 cm' },
    ]

    for (const esperado of esperados) {
      const card = within(rede).getByRole('button', {
        name: `Selecionar Jardim ${esperado.id}`,
      })
      expect(within(card).getByText(`Jardim ${esperado.id}`)).toBeInTheDocument()
      expect(within(card).getByText(esperado.nivel)).toBeInTheDocument()
      expect(within(card).getByText(esperado.estado)).toBeInTheDocument()
      expect(within(card).getByText(esperado.tendencia)).toBeInTheDocument()
      expect(
        within(card).getByText('Última atualização'),
      ).toBeInTheDocument()
    }

    await waitFor(() => expect(fetchJardimMock).toHaveBeenCalledWith('J04'))
    expect(fetchJardimMock).toHaveBeenCalledWith('J01')
    expect(fetchJardimMock).toHaveBeenCalledWith('J02')
    expect(fetchJardimMock).toHaveBeenCalledWith('J03')
  })

  it('mostra o estado e a tendência do jardim selecionado no card de status', async () => {
    render(<App />)

    const status = await screen.findByRole('region', { name: 'Jardim J01' })

    expect(within(status).getByText('Estado Atual: Seco')).toBeInTheDocument()
    expect(within(status).getByText('Tendência:')).toBeInTheDocument()
    expect(within(status).getByText('Subindo')).toBeInTheDocument()
    expect(
      within(status).getByText('Informação atualizada em:'),
    ).toBeInTheDocument()
  })

  it('mostra erro acionável quando a API está fora do ar', async () => {
    fetchJardimMock.mockRejectedValue(new Error('API fora do ar'))
    fetchHistoricoMock.mockRejectedValue(new Error('API fora do ar'))

    render(<App />)

    expect(await screen.findByText(/API indisponível/)).toBeInTheDocument()
    const alertas = await screen.findAllByRole('alert')
    expect(
      alertas.some((alerta) =>
        alerta.textContent?.includes('Não foi possível atualizar a rede'),
      ),
    ).toBe(true)
    expect(
      screen.getAllByRole('button', { name: /Tentar novamente/i }).length,
    ).toBeGreaterThan(0)
  })
})

import { render, screen, within, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import App from './App'
import { ApiError, fetchHistorico, fetchJardim } from './services/api'
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

const HISTORICO_J02: Leitura[] = [
  {
    id: 22,
    jardimId: 'J02',
    sensorId: 'S02',
    nivelCm: 27.8,
    bateria: 88,
    recebidoEm: '2026-11-19T10:05:01Z',
    timestampDispositivo: '2026-11-19T10:05:00Z',
    estado: 'ENCHENDO',
  },
  {
    id: 21,
    jardimId: 'J02',
    sensorId: 'S02',
    nivelCm: 26.0,
    bateria: 89,
    recebidoEm: '2026-11-19T10:00:01Z',
    timestampDispositivo: '2026-11-19T10:00:00Z',
    estado: 'ENCHENDO',
  },
  {
    id: 20,
    jardimId: 'J02',
    sensorId: 'S02',
    nivelCm: 24.0,
    bateria: 90,
    recebidoEm: '2026-11-19T09:55:01Z',
    timestampDispositivo: '2026-11-19T09:55:00Z',
    estado: 'ENCHENDO',
  },
]

const HISTORICO_POR_JARDIM: Record<string, Leitura[]> = {
  J01: HISTORICO,
  J02: HISTORICO_J02,
  J03: [],
  J04: [],
}

beforeEach(() => {
  window.localStorage.clear()
  fetchJardimMock.mockReset()
  fetchHistoricoMock.mockReset()
  fetchJardimMock.mockImplementation(async (id) => JARDINS[id])
  fetchHistoricoMock.mockImplementation(
    async (id) => HISTORICO_POR_JARDIM[id] ?? [],
  )
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
    expect(
      screen.queryByText(/Não foi possível atualizar o jardim/),
    ).not.toBeInTheDocument()
  })

  it('atualiza status, dados e histórico ao selecionar outro jardim na rede', async () => {
    const user = userEvent.setup()

    render(<App />)

    // Aguarda carregar J01 inicialmente
    const statusJ01 = await screen.findByRole('region', { name: 'Jardim J01' })
    expect(within(statusJ01).getByText('Estado Atual: Seco')).toBeInTheDocument()

    // Clica no card do Jardim J02 na Rede de Jardins
    const rede = screen.getByRole('region', { name: 'Rede de Jardins' })
    const botaoJ02 = within(rede).getByRole('button', {
      name: 'Selecionar Jardim J02',
    })
    await user.click(botaoJ02)

    // O status do detalhe deve atualizar para J02 com os dados reais
    const statusJ02 = await screen.findByRole('region', { name: 'Jardim J02' })
    expect(within(statusJ02).getByText('Estado Atual: Enchendo')).toBeInTheDocument()
    expect(within(statusJ02).getByText('Estável')).toBeInTheDocument()

    // O card de dados do jardim deve exibir os dados de J02
    expect(within(statusJ02).getByRole('heading', { name: 'Jardim J02' })).toBeInTheDocument()
    expect(screen.getByText('0,28 m/27,8 cm')).toBeInTheDocument()

    // O gráfico deve exibir o histórico de J02, não o de J01.
    const grafico = screen.getByRole('region', {
      name: 'Histórico de Variação do Nível da Água',
    })
    expect(
      within(grafico).getByText(
        '3 leitura(s) recebida(s) da API, da mais antiga para a mais recente.',
      ),
    ).toBeInTheDocument()
    expect(fetchHistoricoMock).toHaveBeenCalledWith('J02', 100, false)
  })

  it('trata histórico vazio para jardim selecionado sem quebrar a interface', async () => {
    fetchHistoricoMock.mockImplementation(
      async (id) => HISTORICO_POR_JARDIM[id] ?? [],
    )

    const user = userEvent.setup()

    render(<App />)

    // Seleciona J03 que possui histórico vazio
    const select = screen.getByRole('combobox', { name: /Selecione o Jardim/i })
    await user.selectOptions(select, 'J03')

    const statusJ03 = await screen.findByRole('region', { name: 'Jardim J03' })
    expect(within(statusJ03).getByText('Estado Atual: Transbordando')).toBeInTheDocument()

    expect(await screen.findByText('Sem leituras para exibir')).toBeInTheDocument()
    expect(
      screen.getByText(
        'O histórico aparece aqui assim que a API registrar leituras deste jardim.',
      ),
    ).toBeInTheDocument()
  })

  it('trata 404 do jardim selecionado como estado vazio, não como erro', async () => {
    const erro404 = new ApiError(
      'Jardim J03 não encontrado.',
      404,
      'JARDIM_NAO_ENCONTRADO',
    )
    fetchJardimMock.mockImplementation(async (id) => {
      if (id === 'J03') {
        throw erro404
      }
      return JARDINS[id]
    })
    fetchHistoricoMock.mockImplementation(async (id) => {
      if (id === 'J03') {
        throw erro404
      }
      return HISTORICO_POR_JARDIM[id] ?? []
    })

    const user = userEvent.setup()

    render(<App />)

    const select = screen.getByRole('combobox', { name: /Selecione o Jardim/i })
    await user.selectOptions(select, 'J03')

    const statusJ03 = await screen.findByRole('region', {
      name: 'Sem leituras recebidas',
    })
    expect(
      within(statusJ03).getByText(
        'Este jardim ainda não enviou leituras para a API.',
      ),
    ).toBeInTheDocument()
    expect(screen.queryByText('Jardim indisponível')).not.toBeInTheDocument()
    expect(
      screen.queryByText(/Não foi possível atualizar o jardim/),
    ).not.toBeInTheDocument()
    expect(await screen.findByText('Sem leituras para exibir')).toBeInTheDocument()
  })

  it('mantém o histórico visível quando o detalhe do jardim falha', async () => {
    fetchJardimMock.mockImplementation(async (id) => {
      if (id === 'J01') {
        throw new ApiError('Detalhe indisponível', 500, 'ERRO_DETALHE')
      }
      return JARDINS[id]
    })

    render(<App />)

    expect(
      await screen.findByRole('region', { name: 'Jardim indisponível' }),
    ).toBeInTheDocument()
    expect(
      await screen.findByText(
        '2 leitura(s) recebida(s) da API, da mais antiga para a mais recente.',
      ),
    ).toBeInTheDocument()
    expect(
      screen.queryByText('Não foi possível carregar o histórico'),
    ).not.toBeInTheDocument()
  })

  it('mostra o erro do histórico sem esconder o estado do jardim', async () => {
    fetchHistoricoMock.mockImplementation(async (id) => {
      if (id === 'J01') {
        throw new ApiError('Histórico indisponível', 500, 'ERRO_HISTORICO')
      }
      return HISTORICO_POR_JARDIM[id] ?? []
    })

    render(<App />)

    const statusJ01 = await screen.findByRole('region', { name: 'Jardim J01' })
    expect(within(statusJ01).getByText('Estado Atual: Seco')).toBeInTheDocument()
    expect(
      await screen.findByText('Não foi possível carregar o histórico'),
    ).toBeInTheDocument()
    expect(screen.queryByText('Jardim indisponível')).not.toBeInTheDocument()
  })
})

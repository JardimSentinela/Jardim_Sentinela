import { render, screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { afterEach, describe, expect, it, vi } from 'vitest'
import HistoricoChart from './HistoricoChart'
import { calcularDominioEixoY } from './historicoChartUtils'
import type { Leitura } from '../types/jardim'

const LEITURAS_EXEMPLO: Leitura[] = [
  {
    id: 3,
    jardimId: 'J01',
    sensorId: 'S01',
    nivelCm: 25.5,
    bateria: 95,
    recebidoEm: '2026-11-19T10:10:00Z',
    timestampDispositivo: '2026-11-19T10:10:00Z',
    estado: 'ENCHENDO',
  },
  {
    id: 2,
    jardimId: 'J01',
    sensorId: 'S01',
    nivelCm: 20.0,
    bateria: 96,
    recebidoEm: '2026-11-19T10:05:00Z',
    timestampDispositivo: '2026-11-19T10:05:00Z',
    estado: 'ENCHENDO',
  },
  {
    id: 1,
    jardimId: 'J01',
    sensorId: 'S01',
    nivelCm: 15.0,
    bateria: 97,
    recebidoEm: '2026-11-19T10:00:00Z',
    timestampDispositivo: '2026-11-19T10:00:00Z',
    estado: 'NORMAL',
  },
]

function gerarLeituras(quantidade: number): Leitura[] {
  const base = Date.parse('2026-11-19T10:00:00Z')
  return Array.from({ length: quantidade }, (_, indice) => {
    const id = quantidade - indice
    const timestamp = new Date(base + id * 60_000).toISOString()
    return {
      id,
      jardimId: 'J01',
      sensorId: 'S01',
      nivelCm: id,
      bateria: 90,
      recebidoEm: timestamp,
      timestampDispositivo: timestamp,
    }
  })
}

function lerBlob(blob: Blob): Promise<string> {
  return new Promise((resolver, rejeitar) => {
    const leitor = new FileReader()
    leitor.onload = () => resolver(String(leitor.result))
    leitor.onerror = () => rejeitar(leitor.error)
    leitor.readAsText(blob)
  })
}

afterEach(() => {
  vi.restoreAllMocks()
})

describe('HistoricoChart', () => {
  it('renderiza o estado vazio quando o histórico não possui leituras', () => {
    render(<HistoricoChart jardimId="J01" historico={[]} />)

    expect(
      screen.getByRole('heading', {
        name: 'Histórico de Variação do Nível da Água',
      }),
    ).toBeInTheDocument()
    expect(screen.getByText('Sem leituras para exibir')).toBeInTheDocument()
    expect(
      screen.getByText(
        'O histórico aparece aqui assim que a API registrar leituras deste jardim.',
      ),
    ).toBeInTheDocument()
    expect(
      screen.getByRole('button', { name: /Baixar Histórico/i }),
    ).toBeDisabled()
  })

  it('renderiza skeleton quando loading é verdadeiro', () => {
    const { container } = render(
      <HistoricoChart jardimId="J01" historico={[]} loading={true} />,
    )

    expect(
      container.querySelectorAll('.animate-pulse').length,
    ).toBeGreaterThan(0)
    expect(
      screen.queryByRole('heading', {
        name: 'Histórico de Variação do Nível da Água',
      }),
    ).not.toBeInTheDocument()
  })

  it('exibe estado de erro acionável com botão de tentar novamente', async () => {
    const onTentarNovamente = vi.fn()
    const user = userEvent.setup()

    render(
      <HistoricoChart
        jardimId="J01"
        historico={[]}
        error={new Error('Falha de conexão com a API')}
        onTentarNovamente={onTentarNovamente}
      />,
    )

    expect(
      screen.getByText('Não foi possível carregar o histórico'),
    ).toBeInTheDocument()
    expect(
      screen.getByText('Falha de conexão com a API'),
    ).toBeInTheDocument()

    const botaoTentar = screen.getByRole('button', { name: /Tentar novamente/i })
    expect(botaoTentar).toBeInTheDocument()

    await user.click(botaoTentar)
    expect(onTentarNovamente).toHaveBeenCalledTimes(1)
  })

  it('apresenta leituras recebidas da API e habilita download do histórico', () => {
    render(<HistoricoChart jardimId="J01" historico={LEITURAS_EXEMPLO} />)

    expect(
      screen.getByText(
        '3 leitura(s) recebida(s) da API, da mais antiga para a mais recente.',
      ),
    ).toBeInTheDocument()

    const botaoDownload = screen.getByRole('button', {
      name: /Baixar Histórico/i,
    })
    expect(botaoDownload).not.toBeDisabled()
  })

  it('permite alternar a janela de visualização das leituras', async () => {
    const user = userEvent.setup()

    render(<HistoricoChart jardimId="J01" historico={LEITURAS_EXEMPLO} />)

    const botaoMenu = screen.getByRole('button', {
      name: /Últimas 20 leituras/i,
    })
    expect(botaoMenu).toBeInTheDocument()

    await user.click(botaoMenu)

    const listbox = screen.getByRole('listbox', {
      name: 'Selecionar janela de leituras',
    })
    expect(listbox).toBeInTheDocument()

    const opcaoTodas = within(listbox).getByRole('option', {
      name: /Todas as leituras/i,
    })
    await user.click(opcaoTodas)

    expect(
      screen.getByRole('button', { name: /Todas as leituras/i }),
    ).toBeInTheDocument()
  })

  it('aciona download com formato CSV ao clicar em Baixar Histórico', async () => {
    const user = userEvent.setup()
    const clickSpy = vi.fn()
    const originalCreateElement = document.createElement.bind(document)

    vi.spyOn(document, 'createElement').mockImplementation((tagName) => {
      const el = originalCreateElement(tagName)
      if (tagName === 'a') {
        el.click = clickSpy
      }
      return el
    })

    const createObjectUrlSpy = vi
      .spyOn(window.URL, 'createObjectURL')
      .mockReturnValue('blob:http://localhost/teste-historico')
    const revokeObjectUrlSpy = vi
      .spyOn(window.URL, 'revokeObjectURL')
      .mockImplementation(() => {})

    render(<HistoricoChart jardimId="J01" historico={LEITURAS_EXEMPLO} />)

    const botaoDownload = screen.getByRole('button', {
      name: /Baixar Histórico/i,
    })
    await user.click(botaoDownload)

    expect(createObjectUrlSpy).toHaveBeenCalled()
    expect(clickSpy).toHaveBeenCalled()
    expect(revokeObjectUrlSpy).toHaveBeenCalledWith(
      'blob:http://localhost/teste-historico',
    )
  })

  it('informa quantas leituras estão em exibição quando a janela limita o total', async () => {
    const user = userEvent.setup()

    render(<HistoricoChart jardimId="J01" historico={gerarLeituras(35)} />)

    expect(
      screen.getByText(
        '20 de 35 leitura(s) carregada(s), da mais antiga para a mais recente.',
      ),
    ).toBeInTheDocument()

    await user.click(
      screen.getByRole('button', { name: /Últimas 20 leituras/i }),
    )
    await user.click(
      screen.getByRole('option', { name: /Todas as leituras carregadas/i }),
    )

    expect(
      screen.getByText(
        '35 leitura(s) recebida(s) da API, da mais antiga para a mais recente.',
      ),
    ).toBeInTheDocument()
  })

  it('amplia o domínio do eixo Y para leituras acima da capacidade', () => {
    expect(calcularDominioEixoY(0)).toBe(50)
    expect(calcularDominioEixoY(50)).toBe(50)
    expect(calcularDominioEixoY(50.2)).toBe(51)
    expect(calcularDominioEixoY(63)).toBe(63)
    expect(calcularDominioEixoY(Number.NaN)).toBe(50)
  })

  it('exporta as leituras em ordem cronológica', async () => {
    const user = userEvent.setup()
    const blobsCapturados: Blob[] = []
    const originalCreateElement = document.createElement.bind(document)

    vi.spyOn(document, 'createElement').mockImplementation((tagName) => {
      const el = originalCreateElement(tagName)
      if (tagName === 'a') {
        el.click = vi.fn()
      }
      return el
    })
    vi.spyOn(window.URL, 'createObjectURL').mockImplementation((blob) => {
      blobsCapturados.push(blob as Blob)
      return 'blob:http://localhost/teste-historico'
    })
    vi.spyOn(window.URL, 'revokeObjectURL').mockImplementation(() => {})

    render(<HistoricoChart jardimId="J01" historico={LEITURAS_EXEMPLO} />)
    await user.click(screen.getByRole('button', { name: /Baixar Histórico/i }))

    const blob = blobsCapturados[0]
    expect(blob).toBeDefined()
    if (!blob) {
      throw new Error('O download não gerou um Blob.')
    }

    const texto = await lerBlob(blob)
    const linhas = texto.split('\r\n')
    const niveis = linhas.slice(1).map((linha) => linha.split(';')[1])

    expect(linhas[0]).toContain('data_hora')
    expect(niveis).toEqual(['"15"', '"20"', '"25.5"'])
  })
})

import { render, screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { describe, expect, it, vi } from 'vitest'
import RedeJardins from './RedeJardins'
import { ApiError } from '../services/api'
import { formatarDataHora } from '../types/jardim'
import type { JardimRedeItem } from '../hooks/useJardinsRede'

const ITENS: JardimRedeItem[] = [
  {
    jardimId: 'J01',
    erro: null,
    jardim: {
      jardimId: 'J01',
      sensorId: 'S01',
      nivelCm: 18.5,
      bateria: 97,
      ultimaLeituraEm: '2026-11-19T10:00:00Z',
      totalLeituras: 10,
      estado: 'NORMAL',
      tendencia: 'SUBINDO',
    },
  },
  {
    jardimId: 'J02',
    erro: null,
    jardim: {
      jardimId: 'J02',
      sensorId: 'S02',
      nivelCm: 27.8,
      bateria: 88,
      ultimaLeituraEm: '2026-11-19T10:05:00Z',
      totalLeituras: 9,
      estado: 'ENCHENDO',
      tendencia: 'ESTAVEL',
    },
  },
  {
    jardimId: 'J03',
    erro: null,
    jardim: {
      jardimId: 'J03',
      sensorId: 'S03',
      nivelCm: 34.2,
      bateria: 76,
      ultimaLeituraEm: '2026-11-19T10:10:00Z',
      totalLeituras: 8,
      estado: 'TRANSBORDANDO',
      tendencia: 'DESCENDO',
    },
  },
  {
    jardimId: 'J04',
    erro: null,
    jardim: {
      jardimId: 'J04',
      sensorId: 'S04',
      nivelCm: 9.4,
      bateria: 91,
      ultimaLeituraEm: '2026-11-19T10:15:00Z',
      totalLeituras: 7,
      estado: null,
      tendencia: 'INSUFICIENTE',
    },
  },
]

const EXPECTED = [
  { id: 'J01', nivel: '18,5 cm', estado: 'Seco', tendencia: 'Subindo' },
  { id: 'J02', nivel: '27,8 cm', estado: 'Enchendo', tendencia: 'Estável' },
  {
    id: 'J03',
    nivel: '34,2 cm',
    estado: 'Transbordando',
    tendencia: 'Descendo',
  },
  { id: 'J04', nivel: '9,4 cm', estado: 'Seco', tendencia: 'Poucos dados' },
] as const

describe('RedeJardins', () => {
  it('mostra J01–J04 com nível, estado, tendência e última atualização', () => {
    render(
      <RedeJardins
        itens={ITENS}
        jardimSelecionado="J01"
        onSelecionar={() => {}}
      />,
    )

    const lista = screen.getByRole('list', {
      name: 'Jardins monitorados J01 a J04',
    })
    expect(within(lista).getAllByRole('button')).toHaveLength(4)

    for (const esperado of EXPECTED) {
      const card = screen.getByRole('button', {
        name: `Selecionar Jardim ${esperado.id}`,
      })
      const item = ITENS.find((i) => i.jardimId === esperado.id)
      const { data, hora } = formatarDataHora(
        item?.jardim?.ultimaLeituraEm ?? '',
      )

      expect(within(card).getByText(`Jardim ${esperado.id}`)).toBeInTheDocument()
      expect(within(card).getByText(esperado.nivel)).toBeInTheDocument()
      expect(within(card).getByText(esperado.estado)).toBeInTheDocument()
      expect(within(card).getByText(esperado.tendencia)).toBeInTheDocument()
      expect(
        within(card).getByText(`Última atualização`),
      ).toBeInTheDocument()
      expect(
        within(card).getByText(`${data} · ${hora}`),
      ).toBeInTheDocument()
    }
  })

  it('seleciona o jardim clicado', async () => {
    const usuario = userEvent.setup()
    const onSelecionar = vi.fn()

    render(
      <RedeJardins
        itens={ITENS}
        jardimSelecionado="J01"
        onSelecionar={onSelecionar}
      />,
    )

    await usuario.click(
      screen.getByRole('button', { name: 'Selecionar Jardim J02' }),
    )

    expect(onSelecionar).toHaveBeenCalledWith('J02')
  })

  it('trata jardim sem leituras e tendência desconhecida sem quebrar', () => {
    render(
      <RedeJardins
        itens={[
          {
            jardimId: 'J03',
            jardim: null,
            erro: new ApiError(
              'Jardim não encontrado',
              404,
              'JARDIM_NAO_ENCONTRADO',
            ),
          },
        ]}
        jardimSelecionado="J03"
        onSelecionar={() => {}}
      />,
    )

    const card = screen.getByRole('button', {
      name: 'Selecionar Jardim J03',
    })
    expect(within(card).getByText('Sem leituras recebidas')).toBeInTheDocument()
    expect(within(card).getByText('Indisponível')).toBeInTheDocument()
    expect(within(card).getAllByText('--').length).toBeGreaterThan(0)
  })
})

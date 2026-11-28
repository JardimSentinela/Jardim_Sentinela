import { describe, expect, it } from 'vitest'
import {
  alertaDoJardim,
  informacoesTendencia,
  mapearEstadoApi,
  normalizarTendencia,
} from './jardim'

describe('mapeamento do estado da API', () => {
  it('converte NORMAL (dispositivo) em SECO (dashboard)', () => {
    expect(mapearEstadoApi('NORMAL')).toBe('SECO')
  })

  it('aceita os demais estados sem diferenciar maiúsculas', () => {
    expect(mapearEstadoApi('enchendo')).toBe('ENCHENDO')
    expect(mapearEstadoApi('TRANSBORDANDO')).toBe('TRANSBORDANDO')
    expect(mapearEstadoApi('ALAGADO')).toBe('ALAGADO')
  })

  it('devolve null para estado ausente ou desconhecido', () => {
    expect(mapearEstadoApi(null)).toBeNull()
    expect(mapearEstadoApi(undefined)).toBeNull()
    expect(mapearEstadoApi('NOVO_ESTADO')).toBeNull()
  })
})

describe('alertaDoJardim', () => {
  it('prioriza o estado enviado pela API, mesmo que divirja da faixa pelo nível', () => {
    const alerta = alertaDoJardim({ nivelCm: 40, estado: 'NORMAL' })
    expect(alerta.nivel).toBe('SECO')
    expect(alerta.titulo).toBe('Seco')
  })

  it('deriva a faixa pelo nível quando o registro não tem classificação (limiares 22/45/60)', () => {
    expect(alertaDoJardim({ nivelCm: 21.9, estado: null }).nivel).toBe('SECO')
    expect(alertaDoJardim({ nivelCm: 22, estado: null }).nivel).toBe('ENCHENDO')
    expect(alertaDoJardim({ nivelCm: 45, estado: null }).nivel).toBe('TRANSBORDANDO')
    expect(alertaDoJardim({ nivelCm: 50, estado: null }).nivel).toBe('TRANSBORDANDO')
    expect(alertaDoJardim({ nivelCm: 60, estado: null }).nivel).toBe('ALAGADO')
  })
})

describe('tendência da API', () => {
  it('normaliza os seis estados oficiais', () => {
    expect(normalizarTendencia('subindo')).toBe('SUBINDO')
    expect(normalizarTendencia('ESTAVEL')).toBe('ESTAVEL')
    expect(normalizarTendencia('DESCENDO')).toBe('DESCENDO')
    expect(normalizarTendencia('INSUFICIENTE')).toBe('INSUFICIENTE')
    expect(normalizarTendencia('INDETERMINADO')).toBe('INDETERMINADO')
    expect(normalizarTendencia('DESATUALIZADO')).toBe('DESATUALIZADO')
  })

  it('trata valores novos/desconhecidos como indisponíveis, sem quebrar a tela', () => {
    expect(normalizarTendencia('NOVO_VALOR')).toBeNull()
    expect(informacoesTendencia('NOVO_VALOR').titulo).toBe('Indisponível')
    expect(informacoesTendencia(null).disponivel).toBe(false)
  })

  it('não descreve ESTAVEL como nível parado', () => {
    expect(informacoesTendencia('ESTAVEL').descricao).toContain(
      'não significa que o nível ficou parado',
    )
  })
})

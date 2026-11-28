import { NIVEL_MAXIMO_JARDIM_CM } from '../types/jardim'

/**
 * Domínio do eixo Y do histórico: nunca menor que a capacidade do jardim
 * (50 cm), mas acompanha leituras acima dela para não cortar barras.
 */
export function calcularDominioEixoY(nivelMaximoCm: number): number {
  const maximo = Number.isFinite(nivelMaximoCm) ? Math.max(0, nivelMaximoCm) : 0
  return Math.max(NIVEL_MAXIMO_JARDIM_CM, Math.ceil(maximo))
}

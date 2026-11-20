import { Clock3, HelpCircle, Minus, TrendingDown, TrendingUp } from 'lucide-react'
import { informacoesTendencia, normalizarTendencia } from '../types/jardim'

interface IndicadorTendenciaProps {
  tendencia?: string | null
  className?: string
}

/**
 * Pílula de tendência. Valores ausentes ou desconhecidos viram "Indisponível"
 * — a interface nunca calcula tendência localmente para substituir a API.
 */
export default function IndicadorTendencia({
  tendencia,
  className = '',
}: IndicadorTendenciaProps) {
  const info = informacoesTendencia(tendencia)
  const normalizada = normalizarTendencia(tendencia)

  const Icone =
    normalizada === 'SUBINDO'
      ? TrendingUp
      : normalizada === 'DESCENDO'
        ? TrendingDown
        : normalizada === 'ESTAVEL'
          ? Minus
          : normalizada === 'DESATUALIZADO'
            ? Clock3
            : HelpCircle

  return (
    <span
      className={`inline-flex items-center gap-1.5 rounded-full px-2.5 py-1 text-xs font-semibold ${className}`}
      style={{ backgroundColor: info.cor, color: info.corTexto }}
      title={info.descricao}
    >
      <Icone className="h-3.5 w-3.5" aria-hidden="true" />
      {info.titulo}
    </span>
  )
}

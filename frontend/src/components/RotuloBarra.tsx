interface RotuloBarraProps {
  x?: number | string
  y?: number | string
  width?: number | string
  value?: number | string
  sufixo: string
  casasDecimais?: number
}

/**
 * Rótulo de valor das barras desenhado como um único texto SVG.
 * Evita a quebra automática de linha que o LabelList padrão aplica
 * quando a barra é estreita (ex.: "100 cm" virando duas linhas).
 */
export default function RotuloBarra({
  x = 0,
  y = 0,
  width = 0,
  value = '',
  sufixo,
  casasDecimais = 0,
}: RotuloBarraProps) {
  const numero = Number(value)
  const texto = Number.isFinite(numero)
    ? numero.toLocaleString('pt-BR', {
        minimumFractionDigits: casasDecimais,
        maximumFractionDigits: casasDecimais,
      })
    : String(value)

  const centroX = Number(x) + Number(width) / 2
  const baseY = Number(y)

  return (
    <text
      x={centroX}
      y={baseY - 6}
      textAnchor="middle"
      fill="var(--cor-texto-suave)"
      fontSize={11}
    >
      {`${texto} ${sufixo}`}
    </text>
  )
}

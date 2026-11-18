/**
 * Tipos compartilhados do dashboard do Jardim Sentinela.
 *
 * Os contratos de `JardimResponse` e `Leitura` seguem `docs/api-dashboard.md` e
 * `docs/dicionario-de-dados.md` (chaves em camelCase, nível em centímetros,
 * bateria de 0 a 100 e datas em ISO-8601 UTC).
 */

export interface JardimResponse {
  jardimId: string
  sensorId: string
  nivelCm: number
  bateria: number
  ultimaLeituraEm: string | null
  totalLeituras: number
}

export interface Leitura {
  id: number
  jardimId: string
  sensorId: string
  nivelCm: number
  bateria: number
  recebidoEm: string
  timestampDispositivo: string | null
}

export type NivelAlerta = 'NORMAL' | 'ATENCAO' | 'TRANSBORDANDO' | 'ALAGADO'

export interface InformacoesAlerta {
  titulo: string
  subtitulo: string
  descricao: string
  cor: string
  nivel: NivelAlerta
}

export interface DadosCOP {
  estagio: NivelAlerta
  estagioTexto: string
  chuva24hMm: number
  chuvaMax24hMm: number
  abrigosAtivos: number
  ultimaAtualizacao: string
}

/** Capacidade considerada para o jardim de chuva (mesma faixa do simulador da API). */
export const NIVEL_MAXIMO_JARDIM_CM = 50

/**
 * Faixas de alerta do protótipo, proporcionais à capacidade de 50 cm:
 * - NORMAL: menos de 15 cm (até 30% da capacidade);
 * - ATENCAO: de 15 cm a menos de 30 cm (30% a 60%);
 * - TRANSBORDANDO: de 30 cm a menos de 45 cm (60% a 90%);
 * - ALAGADO: 45 cm ou mais (a partir de 90% da capacidade).
 */
export const LIMITES_ALERTA_CM = {
  atencao: 15,
  transbordando: 30,
  alagado: 45,
} as const

const INFORMACOES_POR_NIVEL: Record<NivelAlerta, Omit<InformacoesAlerta, 'nivel'>> = {
  NORMAL: {
    titulo: 'Nível normal',
    subtitulo: 'Jardim em equilíbrio',
    descricao:
      'O nível da água está baixo e o jardim segue drenando normalmente. Nenhuma ação necessária.',
    cor: '#16a34a',
  },
  ATENCAO: {
    titulo: 'Atenção',
    subtitulo: 'Nível acima do habitual',
    descricao:
      'A água está acumulando acima do comum. Acompanhe as próximas leituras e verifique a drenagem.',
    cor: '#d97706',
  },
  TRANSBORDANDO: {
    titulo: 'Transbordando',
    subtitulo: 'Próximo da capacidade máxima',
    descricao:
      'O jardim está perto do limite e pode extravasar. Evite circular pela área e acompanhe o nível.',
    cor: '#ea580c',
  },
  ALAGADO: {
    titulo: 'Alagado',
    subtitulo: 'Capacidade esgotada',
    descricao:
      'O jardim atingiu a capacidade máxima. Não transite pelo local e acione a equipe responsável.',
    cor: '#dc2626',
  },
}

/**
 * Classifica o nível de água (em cm) em um dos quatro estados de alerta.
 * Valores negativos ou não finitos são tratados como nível zero (normal).
 */
export function calcularStatusAlerta(nivelCm: number): InformacoesAlerta {
  const nivel = Number.isFinite(nivelCm) ? Math.max(0, nivelCm) : 0

  let chave: NivelAlerta = 'NORMAL'
  if (nivel >= LIMITES_ALERTA_CM.alagado) {
    chave = 'ALAGADO'
  } else if (nivel >= LIMITES_ALERTA_CM.transbordando) {
    chave = 'TRANSBORDANDO'
  } else if (nivel >= LIMITES_ALERTA_CM.atencao) {
    chave = 'ATENCAO'
  }

  return { ...INFORMACOES_POR_NIVEL[chave], nivel: chave }
}

const formatadorData = new Intl.DateTimeFormat('pt-BR', {
  day: '2-digit',
  month: '2-digit',
  year: 'numeric',
})

const formatadorHora = new Intl.DateTimeFormat('pt-BR', {
  hour: '2-digit',
  minute: '2-digit',
})

/**
 * Converte uma data ISO-8601 para os formatos exibidos no dashboard.
 * Datas ausentes ou inválidas retornam `--` em ambos os campos.
 */
export function formatarDataHora(isoString: string): { data: string; hora: string } {
  if (!isoString) {
    return { data: '--', hora: '--' }
  }

  const data = new Date(isoString)
  if (Number.isNaN(data.getTime())) {
    return { data: '--', hora: '--' }
  }

  return {
    data: formatadorData.format(data),
    hora: formatadorHora.format(data),
  }
}

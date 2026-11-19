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

/**
 * Estados oficiais do Guia de Estilo (Manual da Marca / node 122-2).
 * Substituem os antigos NORMAL/ATENCAO.
 */
export type NivelAlerta = 'SECO' | 'ENCHENDO' | 'TRANSBORDANDO' | 'ALAGADO'

/** Ordem em que os quatro estados aparecem na pílula de indicadores. */
export const ESTADOS_ALERTA: readonly NivelAlerta[] = [
  'SECO',
  'ENCHENDO',
  'TRANSBORDANDO',
  'ALAGADO',
]

/** Cores digitais oficiais de cada estado de alerta. */
export const CORES_ESTADO: Record<NivelAlerta, string> = {
  SECO: '#e4b739',
  ENCHENDO: '#30e4bc',
  TRANSBORDANDO: '#005b7f',
  ALAGADO: '#141781',
}

export interface InformacoesAlerta {
  titulo: string
  subtitulo: string
  descricao: string
  cor: string
  /** Cor de texto com contraste adequado sobre `cor` (âmbar e turquesa pedem texto escuro). */
  corTexto: string
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
 * - SECO: menos de 15 cm (até 30% da capacidade);
 * - ENCHENDO: de 15 cm a menos de 30 cm (30% a 60%);
 * - TRANSBORDANDO: de 30 cm a menos de 45 cm (60% a 90%);
 * - ALAGADO: 45 cm ou mais (a partir de 90% da capacidade).
 */
export const LIMITES_ALERTA_CM = {
  enchendo: 15,
  transbordando: 30,
  alagado: 45,
} as const

const INFORMACOES_POR_NIVEL: Record<NivelAlerta, Omit<InformacoesAlerta, 'nivel'>> = {
  SECO: {
    titulo: 'Seco',
    subtitulo: 'Nível baixo',
    descricao:
      'Sensor do totem físico do Jardim Sentinela identifica nível de água baixo, abaixo do habitual. O jardim segue drenando normalmente e não requer ação imediata.',
    cor: CORES_ESTADO.SECO,
    corTexto: '#071d41',
  },
  ENCHENDO: {
    titulo: 'Enchendo',
    subtitulo: 'Nível em elevação',
    descricao:
      'Sensor do totem físico do Jardim Sentinela identifica nível de água em elevação. A drenagem segue ativa, mas o volume pede acompanhamento nas próximas leituras.',
    cor: CORES_ESTADO.ENCHENDO,
    corTexto: '#071d41',
  },
  TRANSBORDANDO: {
    titulo: 'Transbordando',
    subtitulo: 'Transbordamento',
    descricao:
      'Sensor do totem físico do Jardim Sentinela identifica nível de água acima da capacidade de contenção. A água pode extravasar para a área de circulação; acompanhe o nível e acione a equipe de manutenção.',
    cor: CORES_ESTADO.TRANSBORDANDO,
    corTexto: '#ffffff',
  },
  ALAGADO: {
    titulo: 'Alagado',
    subtitulo: 'Alagamento',
    descricao:
      'Sensor do totem físico do Jardim Sentinela identifica nível de água na capacidade máxima de contenção. A água pode transbordar para o entorno; isole a área e acione a equipe responsável.',
    cor: CORES_ESTADO.ALAGADO,
    corTexto: '#ffffff',
  },
}

/**
 * Classifica o nível de água (em cm) em um dos quatro estados de alerta.
 * Valores negativos ou não finitos são tratados como nível zero (seco).
 */
export function calcularStatusAlerta(nivelCm: number): InformacoesAlerta {
  const nivel = Number.isFinite(nivelCm) ? Math.max(0, nivelCm) : 0

  let chave: NivelAlerta = 'SECO'
  if (nivel >= LIMITES_ALERTA_CM.alagado) {
    chave = 'ALAGADO'
  } else if (nivel >= LIMITES_ALERTA_CM.transbordando) {
    chave = 'TRANSBORDANDO'
  } else if (nivel >= LIMITES_ALERTA_CM.enchendo) {
    chave = 'ENCHENDO'
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

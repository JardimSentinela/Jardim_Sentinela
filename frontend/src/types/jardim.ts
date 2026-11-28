/**
 * Tipos compartilhados do dashboard do Jardim Sentinela.
 *
 * Os contratos de `JardimResponse` e `Leitura` seguem `docs/api-dashboard.md` e
 * `docs/dicionario-de-dados.md` (chaves em camelCase, nível em centímetros,
 * bateria de 0 a 100 e datas em ISO-8601 UTC).
 */

/** Item de `GET /api/jardins` (resumo da leitura mais recente de cada jardim). */
export interface JardimResumo {
  jardimId: string
  nivelCm: number
  /** Estado enviado pelo dispositivo (`NORMAL`, `ENCHENDO`, `TRANSBORDANDO`, `ALAGADO`) ou `null`. */
  estado: string | null
  ultimaLeituraEm: string | null
  totalLeituras: number
}

export interface JardimResponse {
  jardimId: string
  sensorId: string
  nivelCm: number
  bateria: number
  ultimaLeituraEm: string | null
  totalLeituras: number
  /**
   * Estado do registro mais recente, como o dispositivo enviou. A API não
   * recalcula; `null`/ausente em registros antigos ou quando o backend ainda
   * não expõe o campo.
   */
  estado?: string | null
  /**
   * Tendência calculada pela API (`SUBINDO`, `ESTAVEL`, `DESCENDO`,
   * `INSUFICIENTE`, `INDETERMINADO`, `DESATUALIZADO`). `null`/ausente quando a
   * API não expõe o campo — nesse caso a interface mostra "Indisponível".
   */
  tendencia?: string | null
}

export interface Leitura {
  id: number
  jardimId: string
  sensorId: string
  nivelCm: number
  bateria: number
  recebidoEm: string
  timestampDispositivo: string | null
  estado?: string | null
  tendenciaDispositivo?: string | null
  versaoRegra?: string | null
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
 * Faixas de alerta alinhadas ao firmware (`esp32/src/modulos/estado_fisico.h`):
 * - SECO (`NORMAL`): menos de 22 cm;
 * - ENCHENDO: de 22 cm a menos de 45 cm;
 * - TRANSBORDANDO: de 45 cm a menos de 60 cm;
 * - ALAGADO: 60 cm ou mais.
 */
export const LIMITES_ALERTA_CM = {
  enchendo: 22,
  transbordando: 45,
  alagado: 60,
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

/**
 * Estados enviados pelo dispositivo (`GET /api/jardins` e `GET /api/jardins/{id}`).
 * O dispositivo usa `NORMAL` para o que o dashboard chama de `SECO`.
 */
const NIVEL_POR_ESTADO_API: Record<string, NivelAlerta> = {
  NORMAL: 'SECO',
  SECO: 'SECO',
  ENCHENDO: 'ENCHENDO',
  TRANSBORDANDO: 'TRANSBORDANDO',
  ALAGADO: 'ALAGADO',
}

/** Converte o `estado` da API em um dos quatro estados do dashboard, com tolerância a valores desconhecidos. */
export function mapearEstadoApi(estado: string | null | undefined): NivelAlerta | null {
  if (!estado) {
    return null
  }
  return NIVEL_POR_ESTADO_API[estado.trim().toUpperCase()] ?? null
}

/**
 * Monta o estado exibido no dashboard priorizando o `estado` vindo da API.
 * Só deriva a faixa pelo nível quando o backend não envia classificação
 * (registros antigos), mantendo a tela funcional sem inventar o campo.
 */
export function alertaDoJardim(
  jardim: Pick<JardimResponse, 'nivelCm' | 'estado'>,
): InformacoesAlerta {
  const nivel =
    mapearEstadoApi(jardim.estado) ?? calcularStatusAlerta(jardim.nivelCm).nivel
  return { ...INFORMACOES_POR_NIVEL[nivel], nivel }
}

/** Tendências calculadas pela API (SW-14). */
export type TendenciaApi =
  | 'SUBINDO'
  | 'ESTAVEL'
  | 'DESCENDO'
  | 'INSUFICIENTE'
  | 'INDETERMINADO'
  | 'DESATUALIZADO'

export const TENDENCIAS_API: readonly TendenciaApi[] = [
  'SUBINDO',
  'ESTAVEL',
  'DESCENDO',
  'INSUFICIENTE',
  'INDETERMINADO',
  'DESATUALIZADO',
]

export interface InformacoesTendencia {
  titulo: string
  descricao: string
  cor: string
  corTexto: string
  /** `false` quando a API não informou uma tendência reconhecida. */
  disponivel: boolean
}

const TENDENCIA_INDISPONIVEL: InformacoesTendencia = {
  titulo: 'Indisponível',
  descricao: 'A API não informou a tendência para este jardim.',
  cor: '#6b7280',
  corTexto: '#ffffff',
  disponivel: false,
}

export const INFORMACOES_TENDENCIA: Record<TendenciaApi, InformacoesTendencia> = {
  SUBINDO: {
    titulo: 'Subindo',
    descricao: 'O nível subiu além da tolerância nas leituras mais recentes.',
    cor: '#005b7f',
    corTexto: '#ffffff',
    disponivel: true,
  },
  ESTAVEL: {
    titulo: 'Estável',
    descricao:
      'A variação entre a primeira e a última leitura ficou dentro da tolerância; não significa que o nível ficou parado.',
    cor: '#e4b739',
    corTexto: '#071d41',
    disponivel: true,
  },
  DESCENDO: {
    titulo: 'Descendo',
    descricao: 'O nível desceu além da tolerância nas leituras mais recentes.',
    cor: '#30e4bc',
    corTexto: '#071d41',
    disponivel: true,
  },
  INSUFICIENTE: {
    titulo: 'Poucos dados',
    descricao: 'Ainda não há leituras suficientes para calcular a tendência.',
    cor: '#6b7280',
    corTexto: '#ffffff',
    disponivel: true,
  },
  INDETERMINADO: {
    titulo: 'Indeterminado',
    descricao: 'Não foi possível determinar a direção da variação com as leituras disponíveis.',
    cor: '#6b7280',
    corTexto: '#ffffff',
    disponivel: true,
  },
  DESATUALIZADO: {
    titulo: 'Sem dados recentes',
    descricao: 'A última leitura recebida está mais antiga que o limite configurado na API.',
    cor: '#9a3412',
    corTexto: '#ffffff',
    disponivel: true,
  },
}

/** Normaliza valores da API; valores desconhecidos viram `null` (tratados como indisponíveis). */
export function normalizarTendencia(
  valor: string | null | undefined,
): TendenciaApi | null {
  if (!valor) {
    return null
  }
  const normalizado = valor.trim().toUpperCase()
  return (TENDENCIAS_API as readonly string[]).includes(normalizado)
    ? (normalizado as TendenciaApi)
    : null
}

/** Metadados de exibição da tendência, com fallback seguro para valores novos/desconhecidos. */
export function informacoesTendencia(
  valor: string | null | undefined,
): InformacoesTendencia {
  const tendencia = normalizarTendencia(valor)
  return tendencia ? INFORMACOES_TENDENCIA[tendencia] : TENDENCIA_INDISPONIVEL
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

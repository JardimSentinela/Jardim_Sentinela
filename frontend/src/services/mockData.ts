import type { DadosCOP, JardimResponse, Leitura } from '../types/jardim'

/**
 * Dados de demonstração do dashboard.
 *
 * Usados quando a API Java não está acessível e também como base para a
 * interface evoluir sem depender do backend. Os horários são relativos ao
 * momento em que o módulo é carregado, para que a tela pareça "ao vivo".
 */

const INTERVALO_LEITURA_MS = 2 * 60 * 1000
const QUANTIDADE_LEITURAS = 30
const AGORA_MS = Date.now()

interface ConfiguracaoJardimMock {
  jardimId: string
  sensorId: string
  nivelAtualCm: number
  bateria: number
  totalLeituras: number
  amplitudeCm: number
  semente: number
  idBase: number
}

const CONFIGURACOES_JARDINS: ConfiguracaoJardimMock[] = [
  {
    jardimId: 'J01',
    sensorId: 'S01',
    nivelAtualCm: 18.5,
    bateria: 97,
    totalLeituras: 1284,
    amplitudeCm: 0.7,
    semente: 1,
    idBase: 1284,
  },
  {
    jardimId: 'J02',
    sensorId: 'S02',
    nivelAtualCm: 27.8,
    bateria: 88,
    totalLeituras: 986,
    amplitudeCm: 1.2,
    semente: 42,
    idBase: 986,
  },
  {
    jardimId: 'J03',
    sensorId: 'S03',
    nivelAtualCm: 34.2,
    bateria: 76,
    totalLeituras: 742,
    amplitudeCm: 1.5,
    semente: 7,
    idBase: 742,
  },
  {
    jardimId: 'J04',
    sensorId: 'S04',
    nivelAtualCm: 9.4,
    bateria: 91,
    totalLeituras: 1530,
    amplitudeCm: 0.8,
    semente: 99,
    idBase: 1530,
  },
  {
    jardimId: 'J032',
    sensorId: 'S32',
    nivelAtualCm: 47.5,
    bateria: 64,
    totalLeituras: 318,
    amplitudeCm: 2.5,
    semente: 23,
    idBase: 318,
  },
]

/** Gerador pseudoaleatório determinístico (LCG), para o mock não "tremer" a cada render. */
function criarGeradorAleatorio(semente: number): () => number {
  let estado = semente >>> 0
  return () => {
    estado = (estado * 1664525 + 1013904223) >>> 0
    return estado / 0x100000000
  }
}

function arredondarNivel(valor: number): number {
  return Math.round(Math.min(50, Math.max(0, valor)) * 10) / 10
}

function criarHistorico(configuracao: ConfiguracaoJardimMock): Leitura[] {
  const aleatorio = criarGeradorAleatorio(configuracao.semente)

  return Array.from({ length: QUANTIDADE_LEITURAS }, (_, indice) => {
    const variacao = Math.sin(indice / 2.7 + configuracao.semente) * configuracao.amplitudeCm
    const ruido = indice === 0 ? 0 : (aleatorio() - 0.5) * 0.5
    const nivelCm =
      indice === 0
        ? configuracao.nivelAtualCm
        : arredondarNivel(configuracao.nivelAtualCm + variacao + ruido)

    const instante = AGORA_MS - indice * INTERVALO_LEITURA_MS
    const recebidoEm = new Date(instante).toISOString()
    // Assim como no simulador, algumas leituras chegam sem timestamp do dispositivo.
    const timestampDispositivo =
      indice % 7 === 0 ? null : new Date(instante - 1000).toISOString()

    return {
      id: configuracao.idBase - indice,
      jardimId: configuracao.jardimId,
      sensorId: configuracao.sensorId,
      nivelCm,
      bateria: Math.min(100, Math.round(configuracao.bateria + indice * 0.12)),
      recebidoEm,
      timestampDispositivo,
    }
  })
}

/** Histórico mockado por jardim, do mais recente para o mais antigo. */
export const historicoMock: Record<string, Leitura[]> = Object.fromEntries(
  CONFIGURACOES_JARDINS.map((configuracao) => [
    configuracao.jardimId,
    criarHistorico(configuracao),
  ]),
)

/** Estado atual mockado por jardim, sempre coerente com a primeira leitura do histórico. */
export const jardinsMock: Record<string, JardimResponse> = Object.fromEntries(
  CONFIGURACOES_JARDINS.map((configuracao) => {
    const leituraMaisRecente = historicoMock[configuracao.jardimId][0]
    const jardim: JardimResponse = {
      jardimId: configuracao.jardimId,
      sensorId: configuracao.sensorId,
      nivelCm: leituraMaisRecente.nivelCm,
      bateria: leituraMaisRecente.bateria,
      ultimaLeituraEm: leituraMaisRecente.timestampDispositivo ?? leituraMaisRecente.recebidoEm,
      totalLeituras: configuracao.totalLeituras,
    }
    return [configuracao.jardimId, jardim]
  }),
)

/** Ids disponíveis no mock, na ordem em que aparecem no painel. */
export const idsJardinsMock = CONFIGURACOES_JARDINS.map((configuracao) => configuracao.jardimId)

/** Todas as leituras mockadas, da mais recente para a mais antiga. */
export const todasLeiturasMock: Leitura[] = Object.values(historicoMock)
  .flat()
  .sort((a, b) => new Date(b.recebidoEm).getTime() - new Date(a.recebidoEm).getTime())

/** Painel COP mockado: estágio Enchendo, com chuva acumulada e abrigos ativos. */
export const dadosCOPMock: DadosCOP = {
  estagio: 'ENCHENDO',
  estagioTexto: 'Enchendo',
  chuva24hMm: 78.6,
  chuvaMax24hMm: 112.4,
  abrigosAtivos: 2,
  ultimaAtualizacao: new Date(AGORA_MS - 3 * 60 * 1000).toISOString(),
}

export function buscarJardimMock(jardimId: string): JardimResponse | undefined {
  return jardinsMock[jardimId]
}

export function buscarHistoricoMock(jardimId: string, limite = 100): Leitura[] {
  return (historicoMock[jardimId] ?? []).slice(0, limite)
}

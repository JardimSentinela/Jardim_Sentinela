import type { DadosCOP } from '../types/jardim'

/**
 * Único mock restante do dashboard: o painel COP ainda não tem endpoint na API
 * do Jardim Sentinela. Jardins e histórico vêm exclusivamente da API.
 */
const AGORA_MS = Date.now()

/** Painel COP mockado: estágio Enchendo, com chuva acumulada e abrigos ativos. */
export const dadosCOPMock: DadosCOP = {
  estagio: 'ENCHENDO',
  estagioTexto: 'Enchendo',
  chuva24hMm: 78.6,
  chuvaMax24hMm: 112.4,
  abrigosAtivos: 2,
  ultimaAtualizacao: new Date(AGORA_MS - 3 * 60 * 1000).toISOString(),
}

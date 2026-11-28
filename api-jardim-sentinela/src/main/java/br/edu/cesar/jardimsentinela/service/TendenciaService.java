package br.edu.cesar.jardimsentinela.service;

import java.time.Clock;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

import org.springframework.data.domain.PageRequest;
import org.springframework.stereotype.Service;

import br.edu.cesar.jardimsentinela.config.TendenciaProperties;
import br.edu.cesar.jardimsentinela.dto.TendenciaResponse;
import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;
import br.edu.cesar.jardimsentinela.tendencia.CalculadoraTendencia;
import br.edu.cesar.jardimsentinela.tendencia.LeituraNivel;
import br.edu.cesar.jardimsentinela.tendencia.ResultadoTendencia;

/**
 * [SCRUM-28][SW-14] Coordena a tendência de nível de um jardim: consulta o histórico
 * (consulta própria da SW-14, ordenada pela chave cronológica), converte as leituras sem
 * alterar valores, busca em páginas o suficiente para uma janela correta, aplica a
 * tolerância e a idade máxima configuradas, usa o {@link Clock} injetado e chama a
 * {@link CalculadoraTendencia}. A decisão do cálculo é toda da calculadora.
 */
@Service
public class TendenciaService {

    private final LeituraRepository leituraRepository;
    private final TendenciaProperties propriedades;
    private final Clock relogio;

    public TendenciaService(LeituraRepository leituraRepository, TendenciaProperties propriedades,
            Clock relogio) {
        this.leituraRepository = leituraRepository;
        this.propriedades = propriedades;
        this.relogio = relogio;
    }

    /**
     * Vazio só se o jardim nunca enviou NENHUMA leitura (vira 404 no controller). Jardim
     * com leituras, mesmo que todas inválidas, sempre tem resposta: INSUFICIENTE,
     * DESATUALIZADO, INDETERMINADO ou uma tendência.
     */
    public Optional<TendenciaResponse> calcularTendencia(String jardimId) {
        if (!leituraRepository.existsByJardimId(jardimId)) {
            return Optional.empty();
        }
        ResultadoTendencia resultado = CalculadoraTendencia.calcular(
                lerHistoricoNecessario(jardimId),
                propriedades.toleranciaCm(),
                propriedades.idadeMaxima(),
                relogio.instant());
        return Optional.of(TendenciaResponse.de(jardimId, resultado, propriedades.toleranciaCm(),
                propriedades.idadeMaxima().toSeconds()));
    }

    /**
     * Lê o histórico, da leitura mais recente para a mais antiga, em páginas, até que a
     * janela esteja correta. A calculadora não sabe se faltou buscar mais, então este
     * critério de parada é daqui, e só aceita duas razões:
     *
     * 1. O histórico acabou (uma página veio menor que o tamanho pedido). Nunca se conclui
     *    "histórico insuficiente" antes disso: leituras inválidas, mesmo as mais recentes,
     *    não contam para a janela, e a busca segue até achar N válidas ou esgotar tudo.
     * 2. Já há N leituras válidas E o grupo do corte está completo: apareceu uma válida
     *    ESTRITAMENTE mais antiga que a N-ésima. Como a ordem é decrescente pela chave,
     *    isso prova que não há mais leituras com a chave da N-ésima. Assim, um empate que
     *    atravessa o corte (ou várias páginas) traz TODOS os seus membros, sem margem
     *    arbitrária de registros.
     *
     * Devolve todas as leituras examinadas, válidas e inválidas: a calculadora descarta e
     * conta as inválidas. O resultado não depende do tamanho da página.
     */
    private List<LeituraNivel> lerHistoricoNecessario(String jardimId) {
        int tamanhoPagina = propriedades.tamanhoPagina();
        List<LeituraNivel> examinadas = new ArrayList<>();
        int validas = 0;
        Instant chaveDoCorte = null;

        for (int pagina = 0;; pagina++) {
            List<Leitura> lidas = leituraRepository.findHistoricoPorChaveCronologica(
                    jardimId, PageRequest.of(pagina, tamanhoPagina));
            for (Leitura leitura : lidas) {
                LeituraNivel nivel = converter(leitura);
                examinadas.add(nivel);
                if (!CalculadoraTendencia.ehValida(nivel)) {
                    continue;
                }
                validas++;
                Instant chave = CalculadoraTendencia.chaveCronologica(nivel);
                if (validas == CalculadoraTendencia.TAMANHO_JANELA) {
                    chaveDoCorte = chave;
                } else if (validas > CalculadoraTendencia.TAMANHO_JANELA
                        && chave.isBefore(chaveDoCorte)) {
                    return examinadas;       // grupo do corte completo
                }
            }
            if (lidas.size() < tamanhoPagina) {
                return examinadas;           // histórico esgotado
            }
        }
    }

    /** Cópia fiel dos valores: nada é corrigido, arredondado ou substituído aqui (D10). */
    private static LeituraNivel converter(Leitura leitura) {
        return new LeituraNivel(
                leitura.getNivelCm(),
                leitura.getTimestampDispositivo(),
                leitura.getRecebidoEm(),
                leitura.getSensorId(),
                leitura.getHoraSincronizada());
    }
}

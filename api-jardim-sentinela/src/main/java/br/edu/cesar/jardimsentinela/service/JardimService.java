package br.edu.cesar.jardimsentinela.service;

import java.time.Clock;
import java.util.List;
import java.util.Optional;

import org.springframework.data.domain.PageRequest;
import org.springframework.stereotype.Service;

import br.edu.cesar.jardimsentinela.config.TendenciaProperties;
import br.edu.cesar.jardimsentinela.dto.JardimResponse;
import br.edu.cesar.jardimsentinela.dto.JardimResumoResponse;
import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

@Service
public class JardimService {

    public static final int LIMITE_PADRAO = 100;
    public static final int LIMITE_MAXIMO = 1000;

    private final LeituraRepository leituraRepository;
    private final TendenciaService tendenciaService;
    private final TendenciaProperties propriedades;
    private final Clock relogio;

    public JardimService(LeituraRepository leituraRepository, TendenciaService tendenciaService,
            TendenciaProperties propriedades, Clock relogio) {
        this.leituraRepository = leituraRepository;
        this.tendenciaService = tendenciaService;
        this.propriedades = propriedades;
        this.relogio = relogio;
    }

    /**
     * Estado atual do jardim = sua leitura mais recente. A tendência vem do TendenciaService
     * (a mesma regra de GET /api/jardins/{id}/tendencia). O estado é o do último registro
     * elegível que o informou (como o dispositivo enviou, sem recálculo): se o registro mais
     * recente for um evento de T1592 (sem estado) ou um boot, não se perde a classificação válida.
     * Esse estado de reserva só vale dentro da idade máxima da tendência (mesma configuração);
     * fora dela vem {@code null}, nunca um estado antigo apresentado como atual. Se o próprio
     * registro mais recente traz estado, ele vale como sempre valeu (a defasagem aparece em
     * ultimaLeituraEm e em tendencia = DESATUALIZADO).
     * Vazio se o jardim nunca enviou dados.
     */
    public Optional<JardimResponse> buscarJardim(String jardimId) {
        return leituraRepository.findHistorico(jardimId, PageRequest.of(0, 1)).stream().findFirst()
                .map(ultima -> JardimResponse.de(ultima, leituraRepository.countByJardimId(jardimId),
                        estadoValido(ultima, jardimId),
                        tendenciaService.calcularTendencia(jardimId)
                                .map(t -> t.tendencia().name()).orElse(null)));
    }

    private String estadoValido(Leitura ultima, String jardimId) {
        if (ultima.getEstado() != null && !Leitura.TIPO_BOOT.equals(ultima.getTipo())) {
            return ultima.getEstado();
        }
        return leituraRepository.findComEstadoElegivel(jardimId,
                relogio.instant().minus(propriedades.idadeMaxima()), PageRequest.of(0, 1))
                .stream().findFirst().map(Leitura::getEstado).orElse(null);
    }

    /** Resumo de cada jardim que já enviou dados, ordenado por jardimId. Jardim sem leitura não aparece. */
    public List<JardimResumoResponse> listarJardins() {
        return leituraRepository.findJardimIdsComLeitura().stream()
                .flatMap(id -> leituraRepository.findHistorico(id, PageRequest.of(0, 1)).stream()
                        .map(ultima -> JardimResumoResponse.de(ultima, leituraRepository.countByJardimId(id))))
                .toList();
    }

    /** Histórico do mais recente para o mais antigo. Vazio se o jardim nunca enviou dados. */
    public Optional<List<Leitura>> buscarHistorico(String jardimId, int limite) {
        if (!leituraRepository.existsByJardimId(jardimId)) {
            return Optional.empty();
        }
        int limiteEfetivo = Math.min(Math.max(limite, 1), LIMITE_MAXIMO);
        return Optional.of(leituraRepository.findHistorico(
                jardimId, PageRequest.of(0, limiteEfetivo)));
    }
}

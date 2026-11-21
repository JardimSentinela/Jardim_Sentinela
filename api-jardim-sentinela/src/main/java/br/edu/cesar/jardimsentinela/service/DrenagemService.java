package br.edu.cesar.jardimsentinela.service;

import java.time.Duration;
import java.util.List;

import org.springframework.data.domain.PageRequest;
import org.springframework.stereotype.Service;

import br.edu.cesar.jardimsentinela.drenagem.DetectorDrenagemLenta;
import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;
import br.edu.cesar.jardimsentinela.tendencia.LeituraNivel;

/**
 * [SCRUM-30][SW-15] Liga a regra de drenagem lenta ao histórico. Usa a consulta elegível da
 * SW-14 (findHistoricoPorChaveCronologica), que exclui o registro de boot.
 */
@Service
public class DrenagemService {

    static final Duration JANELA = Duration.ofMinutes(60);
    static final double LIMITE_CM_POR_MIN = 0.5;
    private static final int TAMANHO_PAGINA = 200;

    private final LeituraRepository leituraRepository;

    public DrenagemService(LeituraRepository leituraRepository) {
        this.leituraRepository = leituraRepository;
    }

    public DetectorDrenagemLenta.Resultado avaliar(String jardimId) {
        List<Leitura> historico = leituraRepository.findHistoricoPorChaveCronologica(
                jardimId, PageRequest.of(0, TAMANHO_PAGINA));
        List<LeituraNivel> leituras = historico.stream()
                .map(l -> new LeituraNivel(l.getNivelCm(), l.getTimestampDispositivo(),
                        l.getRecebidoEm(), l.getSensorId()))
                .toList();
        return DetectorDrenagemLenta.avaliar(leituras, JANELA, LIMITE_CM_POR_MIN);
    }
}

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
 *
 * <p><b>Escolha provisória:</b> a consulta devolve snapshots <b>e</b> eventos T1592 como
 * medições. A issue #29 (item "Divergência entre Eventos T1592 e Snapshots periódicos")
 * ainda discute se a API deve aceitar apenas snapshots; se a equipe decidir filtrar,
 * o ajuste acontece aqui (query ou stream) e não na regra pura. Enquanto não há decisão,
 * os testes cobrem o comportamento atual — não o tratamos como contrato fechado.
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
        // Snapshots + eventos entram por decisão provisória (issue #29). Se a equipe
        // optar por só snapshots, filtrar aqui por tipo antes de montar LeituraNivel.
        List<Leitura> historico = leituraRepository.findHistoricoPorChaveCronologica(
                jardimId, PageRequest.of(0, TAMANHO_PAGINA));
        List<LeituraNivel> leituras = historico.stream()
                .map(l -> new LeituraNivel(l.getNivelCm(), l.getTimestampDispositivo(),
                        l.getRecebidoEm(), l.getSensorId()))
                .toList();
        return DetectorDrenagemLenta.avaliar(leituras, JANELA, LIMITE_CM_POR_MIN);
    }
}

package br.edu.cesar.jardimsentinela.service;

import java.util.List;
import java.util.Optional;

import org.springframework.data.domain.PageRequest;
import org.springframework.stereotype.Service;

import br.edu.cesar.jardimsentinela.dto.JardimResponse;
import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

@Service
public class JardimService {

    public static final int LIMITE_PADRAO = 100;
    public static final int LIMITE_MAXIMO = 1000;

    private final LeituraRepository leituraRepository;

    public JardimService(LeituraRepository leituraRepository) {
        this.leituraRepository = leituraRepository;
    }

    /** Estado atual do jardim = sua leitura mais recente. Vazio se o jardim nunca enviou dados. */
    public Optional<JardimResponse> buscarJardim(String jardimId) {
        return leituraRepository.findFirstByJardimIdOrderByRecebidoEmDescIdDesc(jardimId)
                .map(ultima -> JardimResponse.de(ultima, leituraRepository.countByJardimId(jardimId)));
    }

    /** Histórico do mais recente para o mais antigo. Vazio se o jardim nunca enviou dados. */
    public Optional<List<Leitura>> buscarHistorico(String jardimId, int limite) {
        if (!leituraRepository.existsByJardimId(jardimId)) {
            return Optional.empty();
        }
        int limiteEfetivo = Math.min(Math.max(limite, 1), LIMITE_MAXIMO);
        return Optional.of(leituraRepository.findByJardimIdOrderByRecebidoEmDescIdDesc(
                jardimId, PageRequest.of(0, limiteEfetivo)));
    }
}

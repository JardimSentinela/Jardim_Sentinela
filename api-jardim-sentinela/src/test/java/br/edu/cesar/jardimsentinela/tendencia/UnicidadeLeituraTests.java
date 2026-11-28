package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.dao.DataIntegrityViolationException;

import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

/**
 * [SCRUM-28][SW-14] A identidade da leitura (jardimId + bootId + seq) é garantida pelo banco,
 * não só pela checagem do controller: duas requisições simultâneas idênticas não duplicam.
 * Registros sem bootId/seq (antigos, simulador) não são afetados (nulos não colidem).
 */
@SpringBootTest
class UnicidadeLeituraTests {

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    private Leitura leitura(String jardim, String bootId, Long seq) {
        return new Leitura(jardim, "jardim", 10.0, 100, T0, T0, null, null, null, "snapshot", bootId, seq);
    }

    @Test
    void mesmoJardimBootIdESeqNaoPodemSerGravadosDuasVezes() {
        repository.saveAndFlush(leitura("J01", "AAAA0001", 1L));
        assertThatThrownBy(() -> repository.saveAndFlush(leitura("J01", "AAAA0001", 1L)))
                .isInstanceOf(DataIntegrityViolationException.class);
    }

    @Test
    void bootIdOuSeqDiferentesOuJardimDiferenteNaoColidem() {
        repository.saveAndFlush(leitura("J01", "AAAA0001", 1L));
        repository.saveAndFlush(leitura("J01", "AAAA0001", 2L));
        repository.saveAndFlush(leitura("J01", "BBBB0002", 1L));
        repository.saveAndFlush(leitura("J02", "AAAA0001", 1L));
        assertThat(repository.count()).isEqualTo(4);
    }

    @Test
    void registrosSemBootIdESeqNaoColidemEntreSi() {
        repository.saveAndFlush(leitura("J01", null, null));
        repository.saveAndFlush(leitura("J01", null, null));
        assertThat(repository.count()).isEqualTo(2);
    }
}

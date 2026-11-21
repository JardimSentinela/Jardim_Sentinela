package br.edu.cesar.jardimsentinela.drenagem;

import static org.junit.jupiter.api.Assertions.assertEquals;

import java.time.Instant;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;

import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;
import br.edu.cesar.jardimsentinela.service.DrenagemService;

/**
 * [SCRUM-30][SW-15] O serviço de drenagem lê o histórico elegível (sem o registro de boot).
 * Boot traz nível fictício (0 cm) e não é medição: se entrasse, criaria uma queda falsa.
 */
@SpringBootTest
class DrenagemServiceTests {

    private static final Instant T0 = Instant.parse("2026-11-20T12:00:00Z");

    @Autowired
    DrenagemService service;

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    @Test
    void registroDeBootNaoEntraNaRegra() {
        repository.save(new Leitura("J01", "sensor-01", 0.0, 90, T0.minusSeconds(300),
                T0.minusSeconds(300), null, null, null, "boot", "BOOT0001", 0L));
        repository.save(new Leitura("J01", "S01", 20.0, 90, T0, T0, null, null, null,
                null, null, null));
        repository.save(new Leitura("J01", "S01", 19.0, 90, T0.plusSeconds(3000),
                T0.plusSeconds(3000), null, null, null, null, null, null));

        DetectorDrenagemLenta.Resultado r = service.avaliar("J01");

        assertEquals(DetectorDrenagemLenta.Situacao.DRENAGEM_LENTA, r.situacao());
        assertEquals(2, r.leiturasUsadas());
        assertEquals(1.0 / 50.0, r.taxaCmMin(), 1e-9);
    }

    @Test
    void eventoEMedicaoComoSnapshot() {
        repository.save(new Leitura("J01", "S01", 20.0, 90, T0, T0, null, null, null,
                "snapshot", "BOOT0001", 1L));
        repository.save(new Leitura("J01", "S02", 19.5, 90, T0.plusSeconds(600), T0.plusSeconds(600),
                null, null, null, "evento", "BOOT0001", 2L));
        repository.save(new Leitura("J01", "S01", 19.0, 90, T0.plusSeconds(3000),
                T0.plusSeconds(3000), null, null, null, "snapshot", "BOOT0001", 3L));

        DetectorDrenagemLenta.Resultado r = service.avaliar("J01");

        assertEquals(DetectorDrenagemLenta.Situacao.DRENAGEM_LENTA, r.situacao());
        assertEquals(3, r.leiturasUsadas());
        assertEquals(1.0 / 50.0, r.taxaCmMin(), 1e-9);
    }
}

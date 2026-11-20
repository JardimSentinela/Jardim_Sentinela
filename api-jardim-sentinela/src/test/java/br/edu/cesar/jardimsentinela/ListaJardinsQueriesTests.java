package br.edu.cesar.jardimsentinela;

import static org.assertj.core.api.Assertions.assertThat;

import java.time.Instant;

import org.hibernate.SessionFactory;
import org.hibernate.stat.Statistics;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;

import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;
import br.edu.cesar.jardimsentinela.service.JardimService;
import jakarta.persistence.EntityManagerFactory;

/**
 * [SCRUM-20][SW-11] Regressão do N+1 apontado na revisão: listarJardins() fazia 1 + 2N
 * queries. O número de queries não pode crescer com a quantidade de jardins.
 */
@SpringBootTest(properties = "spring.jpa.properties.hibernate.generate_statistics=true")
class ListaJardinsQueriesTests {

    private static final Instant BASE = Instant.parse("2026-11-12T12:00:00Z");

    @Autowired
    JardimService service;

    @Autowired
    LeituraRepository repository;

    @Autowired
    EntityManagerFactory emf;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    private long queriesParaListar(int jardins) {
        repository.deleteAll();
        for (int j = 1; j <= jardins; j++) {
            for (int k = 0; k < 3; k++) {
                repository.save(new Leitura(String.format("J%02d", j), "S" + j, 10.0 + k, 90,
                        BASE.plusSeconds(k), BASE.plusSeconds(k), "NORMAL", null, null));
            }
        }
        Statistics stats = emf.unwrap(SessionFactory.class).getStatistics();
        stats.clear();
        assertThat(service.listarJardins()).hasSize(jardins);
        return stats.getPrepareStatementCount();
    }

    @Test
    void quantidadeDeQueriesNaoCrescePorJardim() {
        long com2 = queriesParaListar(2);
        long com10 = queriesParaListar(10);
        assertThat(com10).isEqualTo(com2);
    }

    @Test
    void resumoContinuaCorretoComVariosJardins() {
        queriesParaListar(4);
        var lista = service.listarJardins();
        assertThat(lista).extracting("jardimId").containsExactly("J01", "J02", "J03", "J04");
        assertThat(lista).allSatisfy(r -> {
            assertThat(r.totalLeituras()).isEqualTo(3);
            assertThat(r.nivelCm()).isEqualTo(12.0);
        });
    }
}

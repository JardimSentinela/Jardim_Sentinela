package br.edu.cesar.jardimsentinela.tendencia;

import static br.edu.cesar.jardimsentinela.tendencia.LeiturasDeTeste.T0;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.time.Instant;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.data.domain.PageRequest;

import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;

/**
 * [SCRUM-28][SW-14] Etapa 6, Bloco A: a consulta nova do repositório. Ela entrega as
 * leituras de um jardim da mais recente para a mais antiga pela chave cronológica
 * (timestampDispositivo, ou recebidoEm quando ele falta), em páginas determinísticas.
 *
 * O desempate por id dentro da mesma chave serve SÓ para a paginação não perder nem
 * repetir linhas entre páginas; não é ordem temporal (a calculadora nem recebe o id).
 * Quem decide quando parar de buscar é o TendenciaService.
 */
@SpringBootTest
class LeituraRepositoryTendenciaTests {

    @Autowired
    LeituraRepository repository;

    @BeforeEach
    void limpar() {
        repository.deleteAll();
    }

    private Leitura salvar(String jardimId, double nivel, Instant timestampDispositivo,
            Instant recebidoEm) {
        return repository.save(new Leitura(jardimId, "S01", nivel, 100, recebidoEm,
                timestampDispositivo));
    }

    private List<Leitura> pagina(String jardimId, int numero, int tamanho) {
        return repository.findHistoricoPorChaveCronologica(jardimId, PageRequest.of(numero, tamanho));
    }

    private static List<Double> niveis(List<Leitura> leituras) {
        return leituras.stream().map(Leitura::getNivelCm).toList();
    }

    @Test
    void ordenaPelaChaveCronologicaENaoPeloRecebidoEm() {
        // X: medida em T0+10, recebida cedo. Y: sem timestamp (a chave é o recebidoEm, T0+5).
        // Z: medida em T0+2 e recebida por último (retransmissão).
        salvar("J01", 1.0, T0.plusSeconds(10), T0.plusSeconds(1));
        salvar("J01", 2.0, null, T0.plusSeconds(5));
        salvar("J01", 3.0, T0.plusSeconds(2), T0.plusSeconds(9));

        // Por recebidoEm (ordem da SW-07) seria 3, 2, 1.
        assertEquals(List.of(1.0, 2.0, 3.0), niveis(pagina("J01", 0, 10)));
    }

    @Test
    void devolveSoAsLeiturasDoJardimPedido() {
        salvar("J01", 1.0, T0, T0.plusSeconds(1));
        salvar("J02", 99.0, T0.plusSeconds(1), T0.plusSeconds(2));

        assertEquals(List.of(1.0), niveis(pagina("J01", 0, 10)));
    }

    @Test
    void jardimSemLeiturasDevolveListaVazia() {
        assertTrue(pagina("J99", 0, 10).isEmpty());
    }

    @Test
    void paginacaoComTodasEmpatadasNaoPerdeNemRepeteLeituras() {
        // Sete leituras na mesma chave e páginas de 3: sem um desempate determinístico, o
        // banco poderia repetir ou omitir linhas entre as páginas.
        Set<Long> gravadas = new HashSet<>();
        for (int i = 0; i < 7; i++) {
            gravadas.add(salvar("J01", i, T0, T0.plusSeconds(i)).getId());
        }

        List<Leitura> todas = new ArrayList<>();
        for (int numero = 0; numero < 3; numero++) {
            todas.addAll(pagina("J01", numero, 3));
        }

        assertEquals(7, todas.size());
        Set<Long> lidas = new HashSet<>();
        todas.forEach(l -> lidas.add(l.getId()));
        assertEquals(gravadas, lidas, "cada leitura aparece exatamente uma vez");
    }

    @Test
    void grupoEmpatadoQueAtravessaOLimiteDaPaginaContinuaNaPaginaSeguinte() {
        // Chaves: T0+3 (1 leitura), T0+2 (3 empatadas), T0+1 (1 leitura). Com páginas de 2,
        // o grupo T0+2 começa na página 0 e termina na 1.
        salvar("J01", 30.0, T0.plusSeconds(3), T0.plusSeconds(10));
        salvar("J01", 20.0, T0.plusSeconds(2), T0.plusSeconds(11));
        salvar("J01", 21.0, T0.plusSeconds(2), T0.plusSeconds(12));
        salvar("J01", 22.0, T0.plusSeconds(2), T0.plusSeconds(13));
        salvar("J01", 10.0, T0.plusSeconds(1), T0.plusSeconds(14));

        List<Leitura> pagina0 = pagina("J01", 0, 2);
        List<Leitura> pagina1 = pagina("J01", 1, 2);
        List<Leitura> pagina2 = pagina("J01", 2, 2);

        assertEquals(2, pagina0.size());
        assertEquals(2, pagina1.size());
        assertEquals(1, pagina2.size());
        // Na página 0: a mais nova e UMA do grupo empatado; na 1: as outras DUAS do grupo.
        assertEquals(30.0, pagina0.get(0).getNivelCm());
        Set<Double> grupo = new HashSet<>();
        grupo.add(pagina0.get(1).getNivelCm());
        grupo.addAll(niveis(pagina1));
        assertEquals(Set.of(20.0, 21.0, 22.0), grupo);
        assertEquals(10.0, pagina2.get(0).getNivelCm());
    }
}

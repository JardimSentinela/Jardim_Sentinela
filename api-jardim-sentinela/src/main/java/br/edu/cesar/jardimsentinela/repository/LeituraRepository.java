package br.edu.cesar.jardimsentinela.repository;

import java.time.Instant;
import java.util.List;
import java.util.Optional;

import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import br.edu.cesar.jardimsentinela.entity.Leitura;

public interface LeituraRepository extends JpaRepository<Leitura, Long> {

    Optional<Leitura> findFirstByJardimIdOrderByRecebidoEmDescIdDesc(String jardimId);

    List<Leitura> findByJardimIdOrderByRecebidoEmDescIdDesc(String jardimId, Pageable pageable);

    boolean existsByJardimId(String jardimId);

    long countByJardimId(String jardimId);

    /**
     * [SCRUM-44][SW-10] Base da idempotência básica: mesmo jardim, mesmo sensor e mesmo
     * timestamp de dispositivo = o mesmo evento retransmitido. Só se aplica quando a
     * origem manda timestamp (hoje, o firmware manda; o simulador não manda e cai fora
     * desta checagem, sem precisar de código especial por origem no controller).
     */
    Optional<Leitura> findByJardimIdAndSensorIdAndTimestampDispositivo(
            String jardimId, String sensorId, Instant timestampDispositivo);

    /**
     * [SCRUM-28][SW-14] Chave cronológica em JPQL (D7, contrato de telemetria 3.4):
     * timestampDispositivo e, quando ele falta, recebidoEm. É a mesma regra de
     * CalculadoraTendencia.chaveCronologica (em Java); quem mudar uma muda a outra.
     */
    String CHAVE_CRONOLOGICA = "coalesce(l.timestampDispositivo, l.recebidoEm)";

    /**
     * [SCRUM-28][SW-14] Leituras do jardim da mais recente para a mais antiga pela chave
     * cronológica, em páginas. Consulta própria da SW-14; a SW-06 e a SW-07 continuam
     * ordenando por recebidoEm (divergência temporária documentada em
     * docs/tendencia-nivel.md, seção 12).
     *
     * O desempate por id existe só para a paginação ser determinística: sem uma ordem
     * total, o banco poderia repetir ou omitir linhas empatadas entre páginas. Não é
     * ordem temporal: a calculadora não recebe o id e trata os empates como conjunto de
     * possibilidades (A3).
     */
    @Query("select l from Leitura l where l.jardimId = :jardimId"
            + " order by " + CHAVE_CRONOLOGICA + " desc, l.id desc")
    List<Leitura> findHistoricoPorChaveCronologica(@Param("jardimId") String jardimId,
            Pageable pageable);
}

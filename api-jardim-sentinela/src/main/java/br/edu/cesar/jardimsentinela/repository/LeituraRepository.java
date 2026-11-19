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

    /**
     * [SCRUM-20][SW-11] Ordenação opção B (docs/decisoes-tecnicas.md §6.2): chave cronológica
     * COALESCE(timestampDispositivo, recebidoEm), mais recente primeiro, desempate por id.
     */
    @Query("""
            select l from Leitura l
            where l.jardimId = :jardimId
            order by coalesce(l.timestampDispositivo, l.recebidoEm) desc, l.id desc
            """)
    List<Leitura> findHistorico(@Param("jardimId") String jardimId, Pageable pageable);

    /** [SCRUM-20][SW-11] Jardins que já enviaram ao menos uma leitura, em ordem de id. */
    @Query("select distinct l.jardimId from Leitura l order by l.jardimId")
    List<String> findJardimIdsComLeitura();

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
}

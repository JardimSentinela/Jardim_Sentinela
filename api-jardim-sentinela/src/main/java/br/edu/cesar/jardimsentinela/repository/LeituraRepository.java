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

    /**
     * [SCRUM-20][SW-11] Leitura mais recente de cada jardim numa única query (sem N+1), pela mesma
     * chave de {@link #findHistorico}: COALESCE(timestampDispositivo, recebidoEm), desempate por id.
     * Ordenada por jardimId. Jardim sem leitura não aparece.
     */
    @Query("""
            select l from Leitura l
            where not exists (
                select 1 from Leitura o
                where o.jardimId = l.jardimId
                  and (coalesce(o.timestampDispositivo, o.recebidoEm) > coalesce(l.timestampDispositivo, l.recebidoEm)
                       or (coalesce(o.timestampDispositivo, o.recebidoEm) = coalesce(l.timestampDispositivo, l.recebidoEm)
                           and o.id > l.id)))
            order by l.jardimId
            """)
    List<Leitura> findUltimaLeituraDeCadaJardim();

    /** [SCRUM-20][SW-11] Total de leituras por jardim (linhas {jardimId, total}) numa única query. */
    @Query("select l.jardimId, count(l) from Leitura l group by l.jardimId")
    List<Object[]> contarLeiturasPorJardim();

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

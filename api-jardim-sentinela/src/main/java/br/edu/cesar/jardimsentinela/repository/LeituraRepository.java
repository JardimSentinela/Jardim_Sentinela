package br.edu.cesar.jardimsentinela.repository;

import java.time.Instant;
import java.util.List;
import java.util.Optional;

import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;

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
}

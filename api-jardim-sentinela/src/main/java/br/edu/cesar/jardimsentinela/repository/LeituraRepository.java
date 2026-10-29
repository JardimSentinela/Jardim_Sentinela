package br.edu.cesar.jardimsentinela.repository;

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
}

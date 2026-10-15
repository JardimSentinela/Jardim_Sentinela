package br.edu.cesar.jardimsentinela.repository;

import org.springframework.data.jpa.repository.JpaRepository;

import br.edu.cesar.jardimsentinela.entity.Leitura;

public interface LeituraRepository extends JpaRepository<Leitura, Long> {
}
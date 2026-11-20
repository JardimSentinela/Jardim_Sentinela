package br.edu.cesar.jardimsentinela.controller;

import java.net.URI;
import java.time.Instant;
import java.util.List;
import java.util.Optional;

import org.springframework.dao.DataIntegrityViolationException;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

import br.edu.cesar.jardimsentinela.dto.LeituraRequest;
import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.repository.LeituraRepository;
import jakarta.validation.Valid;

@RestController
public class LeituraController {

    private final LeituraRepository leituraRepository;

    public LeituraController(LeituraRepository leituraRepository) {
        this.leituraRepository = leituraRepository;
    }

    @GetMapping("/teste")
    public String teste() {
        return "API do Jardim Sentinela funcionando";
    }

    @PostMapping("/api/leituras")
    public ResponseEntity<Leitura> receberLeitura(@Valid @RequestBody LeituraRequest leitura) {
        // [SCRUM-44][SW-10] Idempotência básica: só dá pra checar quando a origem manda
        // timestamp (hoje, o firmware manda; o simulador não manda e cai direto no save,
        // sem branch por origem). Mesmo jardim+sensor+timestamp = o mesmo evento já recebido.
        // [SCRUM-28][SW-14] Quando a origem manda bootId+seq (firmware), a leitura é identificada
        // por eles: o timestamp pode repetir entre dois boots (relógio sem sincronizar) e não pode
        // descartar uma leitura nova. Sem bootId/seq (simulador, registros antigos) vale a chave
        // anterior, sem mudança de comportamento.
        String bootId = leitura.bootId() != null ? leitura.bootId().toUpperCase() : null;
        Optional<Leitura> existente = Optional.empty();
        if (bootId != null && leitura.seq() != null) {
            existente = leituraRepository.findByJardimIdAndBootIdAndSeq(
                    leitura.jardimId(), bootId, leitura.seq());
        } else if (leitura.timestamp() != null) {
            existente = leituraRepository.findByJardimIdAndSensorIdAndTimestampDispositivo(
                    leitura.jardimId(), leitura.sensorId(), leitura.timestamp());
        }
        if (existente.isPresent()) {
            return ResponseEntity.ok(existente.get());
        }

        Leitura salva;
        try {
            salva = leituraRepository.saveAndFlush(new Leitura(
                leitura.jardimId(),
                leitura.sensorId(),
                leitura.nivelCm(),
                leitura.bateria(),
                Instant.now(),
                leitura.timestamp(),
                leitura.estado(),
                leitura.tendenciaDispositivo(),
                leitura.versaoRegra(),
                leitura.tipo(),
                bootId,
                leitura.seq()));
        } catch (DataIntegrityViolationException corrida) {
            // Mesma leitura (jardimId + bootId + seq) gravada por outra requisição no meio tempo:
            // é uma retransmissão, devolve a existente em vez de erro ou duplicata.
            if (bootId != null && leitura.seq() != null) {
                Optional<Leitura> jaGravada = leituraRepository.findByJardimIdAndBootIdAndSeq(
                        leitura.jardimId(), bootId, leitura.seq());
                if (jaGravada.isPresent()) {
                    return ResponseEntity.ok(jaGravada.get());
                }
            }
            throw corrida;
        }
        return ResponseEntity.created(URI.create("/api/leituras/" + salva.getId())).body(salva);
    }

    @GetMapping("/api/leituras")
    public List<Leitura> listarLeituras() {
        return leituraRepository.findAll();
    }

    @GetMapping("/api/leituras/{id}")
    public ResponseEntity<Leitura> buscarLeitura(@PathVariable Long id) {
        return leituraRepository.findById(id)
                .map(ResponseEntity::ok)
                .orElseGet(() -> ResponseEntity.notFound().build());
    }
}

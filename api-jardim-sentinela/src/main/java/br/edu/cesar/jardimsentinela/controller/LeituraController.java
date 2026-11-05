package br.edu.cesar.jardimsentinela.controller;

import java.net.URI;
import java.time.Instant;
import java.util.List;
import java.util.Optional;

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
        if (leitura.timestamp() != null) {
            Optional<Leitura> existente = leituraRepository.findByJardimIdAndSensorIdAndTimestampDispositivo(
                    leitura.jardimId(), leitura.sensorId(), leitura.timestamp());
            if (existente.isPresent()) {
                return ResponseEntity.ok(existente.get());
            }
        }

        Leitura salva = leituraRepository.save(new Leitura(
                leitura.jardimId(),
                leitura.sensorId(),
                leitura.nivelCm(),
                leitura.bateria(),
                Instant.now(),
                leitura.timestamp()));
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

package br.edu.cesar.jardimsentinela.controller;

import java.util.List;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import br.edu.cesar.jardimsentinela.dto.JardimResponse;
import br.edu.cesar.jardimsentinela.entity.Leitura;
import br.edu.cesar.jardimsentinela.service.JardimService;

@RestController
public class JardimController {

    private final JardimService jardimService;

    public JardimController(JardimService jardimService) {
        this.jardimService = jardimService;
    }

    @GetMapping("/api/jardins/{id}")
    public ResponseEntity<JardimResponse> buscarJardim(@PathVariable String id) {
        return ResponseEntity.of(jardimService.buscarJardim(id));
    }

    @GetMapping("/api/jardins/{id}/leituras")
    public ResponseEntity<List<Leitura>> buscarHistorico(
            @PathVariable String id,
            @RequestParam(defaultValue = "" + JardimService.LIMITE_PADRAO) int limite) {
        return ResponseEntity.of(jardimService.buscarHistorico(id, limite));
    }
}

package br.edu.cesar.jardimsentinela.controller;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RestController;

import br.edu.cesar.jardimsentinela.dto.TendenciaResponse;
import br.edu.cesar.jardimsentinela.exception.JardimNaoEncontradoException;
import br.edu.cesar.jardimsentinela.service.TendenciaService;

/**
 * [SCRUM-28][SW-14] Endpoint novo e aditivo: não altera nem se mistura com
 * {@code GET /api/jardins/{id}} (SW-06) nem com {@code GET /api/jardins/{id}/leituras}
 * (SW-07). A tolerância vem da configuração do backend, nunca da requisição.
 *
 * HTTP 200 para qualquer um dos seis estados; 404 (corpo padrão JARDIM_NAO_ENCONTRADO,
 * o mesmo da SW-06 e da SW-07) só quando o jardim nunca enviou nenhuma leitura.
 */
@RestController
public class TendenciaController {

    private final TendenciaService tendenciaService;

    public TendenciaController(TendenciaService tendenciaService) {
        this.tendenciaService = tendenciaService;
    }

    @GetMapping("/api/jardins/{id}/tendencia")
    public ResponseEntity<TendenciaResponse> buscarTendencia(@PathVariable String id) {
        return ResponseEntity.ok(tendenciaService.calcularTendencia(id)
                .orElseThrow(() -> new JardimNaoEncontradoException(id)));
    }
}

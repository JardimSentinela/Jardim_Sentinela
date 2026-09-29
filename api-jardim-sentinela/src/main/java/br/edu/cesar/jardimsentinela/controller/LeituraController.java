package br.edu.cesar.jardimsentinela.controller;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

import br.edu.cesar.jardimsentinela.dto.LeituraRequest;

@RestController
public class LeituraController {

    @GetMapping("/teste")
    public String teste() {
        return "API do Jardim Sentinela funcionando";
    }

    @PostMapping("/api/leituras")
    public ResponseEntity<Void> receberLeitura(@RequestBody LeituraRequest leitura) {
        System.out.println("Leitura recebida: " + leitura);
        return ResponseEntity.accepted().build();
    }
}
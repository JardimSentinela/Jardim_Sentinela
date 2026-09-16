package br.edu.cesar.jardimsentinela.controller;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController

public class LeituraController {
	@GetMapping("/teste")
	public String teste() {
		return "API do Jardim Sentinela funcionando";
	}

}

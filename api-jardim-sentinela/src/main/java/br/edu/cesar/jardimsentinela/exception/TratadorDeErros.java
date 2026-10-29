package br.edu.cesar.jardimsentinela.exception;

import java.time.Instant;
import java.util.Comparator;
import java.util.List;

import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.http.converter.HttpMessageNotReadableException;
import org.springframework.validation.FieldError;
import org.springframework.web.bind.MethodArgumentNotValidException;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;


//Padroniza as respostas de erro da API.
//Forrmato definido em docs/contrato-telemetria.md (SCRUM-22).
 
@RestControllerAdvice
public class TratadorDeErros {

    public record CampoInvalido(String campo, String mensagem) {
    }

    public record ErroResposta(
            int status,
            String erro,
            String mensagem,
            List<CampoInvalido> campos,
            Instant momento) {
    }

    @ExceptionHandler(MethodArgumentNotValidException.class)
    public ResponseEntity<ErroResposta> payloadInvalido(MethodArgumentNotValidException ex) {
        List<CampoInvalido> campos = ex.getBindingResult().getFieldErrors().stream()
                .sorted(Comparator.comparing(FieldError::getField))
                .map(erro -> new CampoInvalido(erro.getField(), erro.getDefaultMessage()))
                .toList();
        return ResponseEntity.badRequest().body(new ErroResposta(
                HttpStatus.BAD_REQUEST.value(),
                "PAYLOAD_INVALIDO",
                "Um ou mais campos não passaram na validação.",
                campos,
                Instant.now()));
    }

    @ExceptionHandler(HttpMessageNotReadableException.class)
    public ResponseEntity<ErroResposta> corpoIlegivel(HttpMessageNotReadableException ex) {
        return ResponseEntity.badRequest().body(new ErroResposta(
                HttpStatus.BAD_REQUEST.value(),
                "CORPO_ILEGIVEL",
                "O corpo da requisição não é um JSON válido ou tem campo com tipo incorreto.",
                List.of(),
                Instant.now()));
    }
}

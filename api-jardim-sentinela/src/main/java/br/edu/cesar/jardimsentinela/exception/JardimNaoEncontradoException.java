package br.edu.cesar.jardimsentinela.exception;

public class JardimNaoEncontradoException extends RuntimeException {

    public JardimNaoEncontradoException(String jardimId) {
        super("Não existe leitura registrada para o jardim '" + jardimId + "'.");
    }
}

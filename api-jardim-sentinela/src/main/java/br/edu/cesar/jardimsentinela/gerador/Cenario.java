package br.edu.cesar.jardimsentinela.gerador;

/**
 * Cenários controlados do [SCRUM-32][SW-16]. Cada cenário descreve apenas os
 * parâmetros da sequência determinística; a geração em si vive em
 * {@link SequenciaDeterministica}.
 *
 * Os limites seguem o gerador existente ({@code GeradorDadosJardim}): nível
 * entre 0 e 50 cm, uma casa decimal e resolução de 1 s no timestamp.
 */
public enum Cenario {

    /** Subida constante, usada para exercitar a tendência de alta. */
    CHUVA_MODERADA(10.0, 1.0, 0.2, 3, 1, 60, 0),

    /** Subida rápida, para levar o jardim à saturação em poucas leituras. */
    CHUVA_INTENSA(5.0, 2.5, 0.3, 3, 2, 30, 0),

    /** Sobe até o teto do gerador e permanece nele. */
    SATURACAO(46.0, 1.0, 0.0, 3, 3, 30, 0),

    /** Queda de 0,1 cm a cada duas leituras: ~0,15 cm/min com intervalo de 20 s. */
    DRENAGEM_LENTA(45.0, 0.0, 0.0, 20, 4, 180, 2);

    private final double nivelInicialCm;
    private final double variacaoCmPorLeitura;
    private final double ruidoCm;
    private final long intervaloSegundos;
    private final long sementePadrao;
    private final int passosPadrao;
    private final int leiturasPorDecrescimo;

    Cenario(double nivelInicialCm, double variacaoCmPorLeitura, double ruidoCm,
            long intervaloSegundos, long sementePadrao, int passosPadrao,
            int leiturasPorDecrescimo) {
        this.nivelInicialCm = nivelInicialCm;
        this.variacaoCmPorLeitura = variacaoCmPorLeitura;
        this.ruidoCm = ruidoCm;
        this.intervaloSegundos = intervaloSegundos;
        this.sementePadrao = sementePadrao;
        this.passosPadrao = passosPadrao;
        this.leiturasPorDecrescimo = leiturasPorDecrescimo;
    }

    double nivelInicialCm() {
        return nivelInicialCm;
    }

    double variacaoCmPorLeitura() {
        return variacaoCmPorLeitura;
    }

    double ruidoCm() {
        return ruidoCm;
    }

    public long intervaloSegundos() {
        return intervaloSegundos;
    }

    public long sementePadrao() {
        return sementePadrao;
    }

    public int passosPadrao() {
        return passosPadrao;
    }

    int leiturasPorDecrescimo() {
        return leiturasPorDecrescimo;
    }
}

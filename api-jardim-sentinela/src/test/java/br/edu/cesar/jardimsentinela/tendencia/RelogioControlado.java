package br.edu.cesar.jardimsentinela.tendencia;

import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.time.ZoneId;
import java.time.ZoneOffset;

/**
 * [SCRUM-28][SW-14] Relógio de teste: o instante só muda quando o teste manda. Permite
 * verificar a idade máxima dos dados de forma determinística (D12).
 */
final class RelogioControlado extends Clock {

    private volatile Instant agora;

    RelogioControlado(Instant inicial) {
        this.agora = inicial;
    }

    void definir(Instant novo) {
        this.agora = novo;
    }

    void avancar(Duration duracao) {
        this.agora = agora.plus(duracao);
    }

    @Override
    public ZoneId getZone() {
        return ZoneOffset.UTC;
    }

    @Override
    public Clock withZone(ZoneId zone) {
        return this;
    }

    @Override
    public Instant instant() {
        return agora;
    }
}

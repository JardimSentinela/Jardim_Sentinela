package br.edu.cesar.jardimsentinela.entity;

import java.time.Instant;

import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;

@Entity
public class Leitura {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    private String jardimId;
    private String sensorId;
    private Double nivelCm;
    private Integer bateria;

    /** Instante em que a API recebeu a requisição. Sempre preenchido pelo servidor. */
    private Instant recebidoEm;

    /**
     * [SCRUM-44][SW-10] Instante do evento no dispositivo, quando informado
     * (ver LeituraRequest). Pode ser nulo — é a chave de idempotência básica
     * junto com jardimId+sensorId.
     */
    private Instant timestampDispositivo;

    protected Leitura() {
        // exigido pelo JPA
    }

    public Leitura(String jardimId, String sensorId, Double nivelCm, Integer bateria, Instant recebidoEm) {
        this(jardimId, sensorId, nivelCm, bateria, recebidoEm, null);
    }

    public Leitura(String jardimId, String sensorId, Double nivelCm, Integer bateria, Instant recebidoEm,
                    Instant timestampDispositivo) {
        this.jardimId = jardimId;
        this.sensorId = sensorId;
        this.nivelCm = nivelCm;
        this.bateria = bateria;
        this.recebidoEm = recebidoEm;
        this.timestampDispositivo = timestampDispositivo;
    }

    public Long getId() { return id; }
    public String getJardimId() { return jardimId; }
    public String getSensorId() { return sensorId; }
    public Double getNivelCm() { return nivelCm; }
    public Integer getBateria() { return bateria; }
    public Instant getRecebidoEm() { return recebidoEm; }
    public Instant getTimestampDispositivo() { return timestampDispositivo; }
}

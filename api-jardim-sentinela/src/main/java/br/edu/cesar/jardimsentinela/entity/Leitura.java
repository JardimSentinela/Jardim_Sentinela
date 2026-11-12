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

    /** Instante informado pelo dispositivo no campo `timestamp`. */
    private Instant medidoEm;

    /** Instante em que a API recebeu a leitura. */
    private Instant recebidoEm;

    protected Leitura() {
        // exigido pelo JPA
    }

    public Leitura(String jardimId, String sensorId, Double nivelCm, Integer bateria,
                   Instant medidoEm, Instant recebidoEm) {
        this.jardimId = jardimId;
        this.sensorId = sensorId;
        this.nivelCm = nivelCm;
        this.bateria = bateria;
        this.medidoEm = medidoEm;
        this.recebidoEm = recebidoEm;
    }

    public Long getId() { return id; }
    public String getJardimId() { return jardimId; }
    public String getSensorId() { return sensorId; }
    public Double getNivelCm() { return nivelCm; }
    public Integer getBateria() { return bateria; }
    public Instant getMedidoEm() { return medidoEm; }
    public Instant getRecebidoEm() { return recebidoEm; }
}

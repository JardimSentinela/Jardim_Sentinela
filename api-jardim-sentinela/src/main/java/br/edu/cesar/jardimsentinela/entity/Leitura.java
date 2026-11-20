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

    /** [SCRUM-20][SW-11] Estado físico enviado pelo dispositivo. Nulo em registros antigos (sem classificação). */
    private String estado;

    /** [SCRUM-20][SW-11] Tendência enviada pelo dispositivo, guardada para auditoria. Não é recalculada. */
    private String tendenciaDispositivo;

    /** [SCRUM-20][SW-11] Versão da regra que gerou estado/tendência. */
    private String versaoRegra;

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

    public Leitura(String jardimId, String sensorId, Double nivelCm, Integer bateria, Instant recebidoEm,
                    Instant timestampDispositivo, String estado, String tendenciaDispositivo, String versaoRegra) {
        this(jardimId, sensorId, nivelCm, bateria, recebidoEm, timestampDispositivo);
        this.estado = estado;
        this.tendenciaDispositivo = tendenciaDispositivo;
        this.versaoRegra = versaoRegra;
    }

    public Long getId() { return id; }
    public String getJardimId() { return jardimId; }
    public String getSensorId() { return sensorId; }
    public Double getNivelCm() { return nivelCm; }
    public Integer getBateria() { return bateria; }
    public Instant getRecebidoEm() { return recebidoEm; }
    public Instant getTimestampDispositivo() { return timestampDispositivo; }
    public String getEstado() { return estado; }
    public String getTendenciaDispositivo() { return tendenciaDispositivo; }
    public String getVersaoRegra() { return versaoRegra; }
}

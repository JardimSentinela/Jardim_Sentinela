package br.edu.cesar.jardimsentinela.entity;

import java.time.Instant;

import com.fasterxml.jackson.annotation.JsonIgnore;

import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.Table;
import jakarta.persistence.UniqueConstraint;

/**
 * [SCRUM-28][SW-14] A identidade de uma leitura do firmware é (jardimId, bootId, seq): o banco
 * impede duplicata mesmo com requisições simultâneas. Nulos (registros antigos, simulador)
 * não colidem entre si.
 */
@Entity
@Table(uniqueConstraints = @UniqueConstraint(columnNames = {"jardimId", "bootId", "seq"}))
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

    /** Tipo de registro do firmware: "evento", "snapshot" ou "boot". Nulo em registros antigos e no simulador. */
    private String tipo;

    /** Identificador do boot do firmware (8 hex, em maiúsculas). Nulo quando a origem não o envia. */
    private String bootId;

    /** Contador do registro dentro do boot. Com bootId, identifica a leitura. Nulo quando não enviado. */
    private Long seq;

    /** [F1] Indica se o relógio do dispositivo estava sincronizado. False = usa recebidoEm na ordenação. */
    private Boolean horaSincronizada;

    /** Valor de {@link #tipo} do registro de inicialização, que não é medição de nível. */
    public static final String TIPO_BOOT = "boot";

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

    public Leitura(String jardimId, String sensorId, Double nivelCm, Integer bateria, Instant recebidoEm,
                    Instant timestampDispositivo, String estado, String tendenciaDispositivo, String versaoRegra,
                    String tipo, String bootId, Long seq) {
        this(jardimId, sensorId, nivelCm, bateria, recebidoEm, timestampDispositivo, estado,
                tendenciaDispositivo, versaoRegra);
        this.tipo = tipo;
        this.bootId = bootId;
        this.seq = seq;
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

    // tipo, bootId, seq e horaSincronizada são de uso interno: ficam fora das respostas JSON
    // para o contrato de saída da API não mudar.
    @JsonIgnore public String getTipo() { return tipo; }
    @JsonIgnore public String getBootId() { return bootId; }
    @JsonIgnore public Long getSeq() { return seq; }
    @JsonIgnore public Boolean getHoraSincronizada() { return horaSincronizada; }
    public void setHoraSincronizada(Boolean horaSincronizada) { this.horaSincronizada = horaSincronizada; }
}

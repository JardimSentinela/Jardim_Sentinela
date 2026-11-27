package br.edu.cesar.jardimsentinela.repository;

import java.time.Instant;
import java.util.List;
import java.util.Optional;

import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import br.edu.cesar.jardimsentinela.entity.Leitura;

public interface LeituraRepository extends JpaRepository<Leitura, Long> {

    /**
     * [SCRUM-20][SW-11][F1] Ordenação opção B (docs/decisoes-tecnicas.md §6.2): quando
     * horaSincronizada = false, usa recebidoEm; caso contrário, COALESCE(timestampDispositivo,
     * recebidoEm), mais recente primeiro, desempate por id.
     */
    @Query("select l from Leitura l where l.jardimId = :jardimId order by " + CHAVE_CRONOLOGICA + " desc, l.id desc")
    List<Leitura> findHistorico(@Param("jardimId") String jardimId, Pageable pageable);

    /**
     * [SCRUM-20][SW-11][F1][F2] Leitura mais recente elegível (exclui boot) de cada jardim numa
     * única query (sem N+1), pela mesma chave de {@link #findHistorico}, desempate por id.
     * Ordenada por jardimId.
     */
    @Query("""
            select l from Leitura l
            where (l.tipo is null or l.tipo <> 'boot')
              and not exists (
                select 1 from Leitura o
                where o.jardimId = l.jardimId
                  and (o.tipo is null or o.tipo <> 'boot')
                  and ((case when o.horaSincronizada = false then o.recebidoEm else coalesce(o.timestampDispositivo, o.recebidoEm) end)
                       > (case when l.horaSincronizada = false then l.recebidoEm else coalesce(l.timestampDispositivo, l.recebidoEm) end)
                       or ((case when o.horaSincronizada = false then o.recebidoEm else coalesce(o.timestampDispositivo, o.recebidoEm) end)
                           = (case when l.horaSincronizada = false then l.recebidoEm else coalesce(l.timestampDispositivo, l.recebidoEm) end)
                           and o.id > l.id)))
            order by l.jardimId
            """)
    List<Leitura> findUltimaLeituraDeCadaJardim();

    /**
     * [SCRUM-28][SW-14][F1] Último estado válido de cada jardim (mesma regra de
     * {@link #findComEstadoElegivel}: elegível, com estado, recebido desde {@code recebidoDesde}),
     * numa única query para a lista não voltar ao N+1.
     */
    @Query("""
            select l from Leitura l
            where l.estado is not null and (l.tipo is null or l.tipo <> 'boot')
              and l.recebidoEm >= :recebidoDesde
              and not exists (
                select 1 from Leitura o
                where o.jardimId = l.jardimId and o.estado is not null
                  and (o.tipo is null or o.tipo <> 'boot') and o.recebidoEm >= :recebidoDesde
                  and ((case when o.horaSincronizada = false then o.recebidoEm else coalesce(o.timestampDispositivo, o.recebidoEm) end)
                       > (case when l.horaSincronizada = false then l.recebidoEm else coalesce(l.timestampDispositivo, l.recebidoEm) end)
                       or ((case when o.horaSincronizada = false then o.recebidoEm else coalesce(o.timestampDispositivo, o.recebidoEm) end)
                           = (case when l.horaSincronizada = false then l.recebidoEm else coalesce(l.timestampDispositivo, l.recebidoEm) end)
                           and o.id > l.id)))
            """)
    List<Leitura> findUltimoEstadoValidoDeCadaJardim(@Param("recebidoDesde") Instant recebidoDesde);

    /** [SCRUM-20][SW-11] Total de leituras por jardim (linhas {jardimId, total}) numa única query. */
    @Query("select l.jardimId, count(l) from Leitura l group by l.jardimId")
    List<Object[]> contarLeiturasPorJardim();

    boolean existsByJardimId(String jardimId);

    long countByJardimId(String jardimId);

    /**
     * [SCRUM-44][SW-10] Base da idempotência básica: mesmo jardim, mesmo sensor e mesmo
     * timestamp de dispositivo = o mesmo evento retransmitido. Só se aplica quando a
     * origem manda timestamp (hoje, o firmware manda; o simulador não manda e cai fora
     * desta checagem, sem precisar de código especial por origem no controller).
     */
    Optional<Leitura> findByJardimIdAndSensorIdAndTimestampDispositivo(
            String jardimId, String sensorId, Instant timestampDispositivo);

    /**
     * [SCRUM-28][SW-14] Chave cronológica em JPQL (D7, contrato de telemetria 3.4):
     * timestampDispositivo e, quando ele falta, recebidoEm. É a mesma regra de
     * CalculadoraTendencia.chaveCronologica (em Java); quem mudar uma muda a outra.
     */
    String CHAVE_CRONOLOGICA = "(case when l.horaSincronizada = false then l.recebidoEm else coalesce(l.timestampDispositivo, l.recebidoEm) end)";

    /**
     * [SCRUM-28][SW-14] Elegibilidade para tendência e estado: o registro de inicialização
     * (tipo "boot") traz nível fictício e não é medição. Registros sem tipo (antigos,
     * simulador) continuam elegíveis.
     */
    String ELEGIVEL = "(l.tipo is null or l.tipo <> 'boot')";

    /**
     * [SCRUM-28][SW-14] Leituras do jardim da mais recente para a mais antiga pela chave
     * cronológica, em páginas. Consulta própria da SW-14: difere de {@link #findHistorico}
     * só pela elegibilidade (exclui o registro de inicialização). A ordenação é a mesma
     * (a SW-11 alinhou o histórico à chave cronológica; ver docs/tendencia-nivel.md, seção 12).
     *
     * O desempate por id existe só para a paginação ser determinística: sem uma ordem
     * total, o banco poderia repetir ou omitir linhas empatadas entre páginas. Não é
     * ordem temporal: a calculadora não recebe o id e trata os empates como conjunto de
     * possibilidades (A3).
     */
    @Query("select l from Leitura l where l.jardimId = :jardimId and " + ELEGIVEL
            + " order by " + CHAVE_CRONOLOGICA + " desc, l.id desc")
    List<Leitura> findHistoricoPorChaveCronologica(@Param("jardimId") String jardimId,
            Pageable pageable);

    /**
     * [SCRUM-28][SW-14] Leituras elegíveis com estado informado, da mais recente para a mais
     * antiga. A primeira é o "último estado válido" do jardim (o registro mais recente pode
     * ser um evento de T1592, sem estado). O boot não conta: seu estado vem de um nível fictício.
     * Só vale se recebido a partir de {@code recebidoDesde} (agora - idade máxima): um estado
     * antigo não pode aparecer como atual quando a sonda para de informar.
     */
    @Query("select l from Leitura l where l.jardimId = :jardimId and l.estado is not null and " + ELEGIVEL
            + " and l.recebidoEm >= :recebidoDesde"
            + " order by " + CHAVE_CRONOLOGICA + " desc, l.id desc")
    List<Leitura> findComEstadoElegivel(@Param("jardimId") String jardimId,
            @Param("recebidoDesde") Instant recebidoDesde, Pageable pageable);

    /**
     * [SCRUM-28][SW-14] Identificação da leitura pelo firmware: bootId + seq (único por registro
     * dentro de um boot; o bootId muda a cada reinício). Base da idempotência quando a origem
     * envia os dois, no lugar do timestamp, que pode repetir entre reinícios.
     */
    Optional<Leitura> findByJardimIdAndBootIdAndSeq(String jardimId, String bootId, Long seq);
}

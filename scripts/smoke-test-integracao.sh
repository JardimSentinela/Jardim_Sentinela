#!/usr/bin/env bash
# [SCRUM-21][SW-12] Smoke test de integração contra uma API Spring Boot em execução.
#
# Uso:
#   scripts/smoke-test-integracao.sh [URL_BASE] [--estrito]
#   scripts/smoke-test-integracao.sh --autoteste-url     (só testa a validação de URL; sem rede)
#
#   URL_BASE         padrão http://localhost:8080 (formato: http(s)://host[:porta], ver abaixo)
#   --estrito        trata os ACHADOS (problemas conhecidos, ver abaixo) como falha
#   --autoteste-url  verifica, sem nenhuma requisição, que URLs ambíguas são recusadas
#
# Variáveis de ambiente:
#   SMOKE_PERMITIR_REMOTO=1  autoriza um host que não seja localhost/127.0.0.1/[::1]
#   SMOKE_TIMEOUT=N          tempo máximo, em segundos, de cada requisição (padrão 15)
#
# ┌──────────────────────────────────────────────────────────────────────────┐
# │ ATENÇÃO: ESTE SCRIPT GRAVA DADOS DE TESTE NA API (11 jardins "SW12-…" e  │
# │ cerca de 40 leituras por execução). USE SEMPRE UM BANCO DESCARTÁVEL.     │
# │ O padrão do projeto é um H2 em ARQUIVO (./data/jardimsentinela): se você │
# │ subir a API sem a configuração abaixo, os dados ficam no seu banco.      │
# └──────────────────────────────────────────────────────────────────────────┘
#
# Suba a API ANTES, com banco em memória:
#
#   cd api-jardim-sentinela
#   SPRING_DATASOURCE_URL='jdbc:h2:mem:smoke;DB_CLOSE_DELAY=-1' \
#   SPRING_H2_CONSOLE_ENABLED=false SPRING_PROFILES_ACTIVE=simulacao \
#   ./mvnw spring-boot:run
#
# Validação da URL (antes de QUALQUER requisição):
#   - só http:// ou https://, host e porta opcional (1 a 65535) e, no máximo, uma "/" final;
#   - recusa credenciais embutidas (@), caminho, query, fragmento, espaços, barra
#     invertida, porta vazia ou fora da faixa, host vazio ou malformado;
#   - só aceita localhost, 127.0.0.1 e [::1]; outro host exige SMOKE_PERMITIR_REMOTO=1,
#     e mesmo assim a URL precisa ser sintaticamente válida e não ambígua.
#
# Requisitos: bash, curl e um leitor de JSON que FUNCIONE (jq, python3 ou python;
# o script testa o interpretador antes de usar). Para subir a API: JDK compatível com
# api-jardim-sentinela/pom.xml (o CI usa 17). No Windows, use o Git Bash.
#
# Premissas (conferidas contra docs/contrato-telemetria.md e
# docs/dicionario-de-dados.md):
#   - "Leitura mais recente" = maior COALESCE(timestampDispositivo, recebidoEm)
#     (contrato §3.4). Por isso os blocos com timestamp usam horários EXPLÍCITOS
#     e ordenados, que não dependem do relógio nem da velocidade da execução.
#     (A versão anterior usava um timestamp fixo de 04/11 e esperava que ele
#     vencesse uma leitura SEM timestamp, recebida depois: expectativa errada.)
#   - O bloco R (sem timestamp) depende da hora de recepção; ele confere antes
#     que as duas recepções tenham horários distintos e, se não tiverem, fica
#     "inconclusivo" em vez de falhar.
#   - A API hoje NÃO valida o formato do id (contrato §1.6). Os ids deste script
#     são únicos por execução (SW12-...). Se a validação ^J\d{2}$ for ligada,
#     troque o prefixo por ids válidos.
#   - A comparação de JSON é semântica (não depende da ordem dos campos nem de
#     20 x 20.0).
#
# Dois tipos de resultado:
#   [OK]/[FALHA]  asserções do contrato escrito. Falha => exit 1.
#   [ACHADO]      comportamento que contraria o esperado mas depende de decisão
#                 de domínio ainda aberta (ver docs/relatorio-integracao-sw12.md):
#                   F1 reinício com hora estimada, F2 boot como leitura atual.
#                 Não falha, a menos que use --estrito.
#
# Códigos de saída: 0 ok | 1 falha de asserção (ou achado com --estrito) | 2 uso/ambiente.
#
# Não substitui os testes automatizados (./mvnw test, npm test, run_tests.py)
# nem a validação do Wokwi: é o "cheiro de fumaça" contra uma instância real.

set -uo pipefail

# ---------- validação de URL (pura: não faz requisição) ----------
# validar_url <url>  -> 0 se sintaticamente válida; define BASE, HOST, URL_LOCAL; senão URL_ERRO
validar_url() {
  local LC_ALL=C
  local url="$1"
  local re='^(https?)://(\[[0-9A-Fa-f:]+\]|[A-Za-z0-9][A-Za-z0-9.-]*)(:([0-9]{1,5}))?/?$'
  URL_ERRO=""; URL_LOCAL=0; HOST=""
  if ! [[ "$url" =~ $re ]]; then
    URL_ERRO="use http(s)://host[:porta], sem credenciais, caminho, query, fragmento ou espaços"
    return 1
  fi
  local host porta
  # shellcheck disable=SC2018,SC2019  # o host já é só ASCII (regex acima) e LC_ALL=C vale aqui
  host="$(printf '%s' "${BASH_REMATCH[2]}" | tr 'A-Z' 'a-z')"
  porta="${BASH_REMATCH[4]}"
  case "$host" in
    \[*\]) case "$host" in *:*) ;; *) URL_ERRO="IPv6 malformado"; return 1 ;; esac ;;
    *..*|*.|*-) URL_ERRO="host malformado"; return 1 ;;
  esac
  if [ -n "$porta" ]; then
    local p=$((10#$porta))
    if [ "$p" -lt 1 ] || [ "$p" -gt 65535 ]; then URL_ERRO="porta fora de 1-65535"; return 1; fi
  fi
  case "$host" in localhost|127.0.0.1|'[::1]') URL_LOCAL=1 ;; esac
  HOST="$host"; BASE="${url%/}"
  return 0
}
# url_permitida -> 0 se o host é local ou se o remoto foi autorizado explicitamente
url_permitida() { [ "$URL_LOCAL" = 1 ] || [ "${SMOKE_PERMITIR_REMOTO:-0}" = 1 ]; }

# autoteste_url: matriz de URLs; nenhuma requisição é feita (curl nem é chamado)
autoteste_url() {
  local total=0 falhas=0 c esperado url res perm0 perm1 salvo="${SMOKE_PERMITIR_REMOTO-}"
  local casos=(
    'INVALIDA|http://localhost:80@exemplo.invalid'
    'INVALIDA|http://localhost@exemplo.invalid'
    'INVALIDA|http://usuario:senha@localhost:8080'
    'INVALIDA|http://localhost:8080@127.0.0.1'
    'INVALIDA|http://127.0.0.1:8080@exemplo.invalid:9'
    'INVALIDA|http://localhost#@exemplo.invalid'
    'INVALIDA|http://localhost?x=@exemplo.invalid'
    'INVALIDA|http://localhost:8080/../etc'
    'INVALIDA|http://localhost:8080/api'
    'INVALIDA|http://localhost\@exemplo.invalid'
    'INVALIDA|http://localhost :8080'
    'INVALIDA|http://localhost%00@exemplo.invalid'
    'INVALIDA|http://localhost:'
    'INVALIDA|http://localhost:0'
    'INVALIDA|http://localhost:65536'
    'INVALIDA|http://localhost:99999999999'
    'INVALIDA|http://'
    'INVALIDA|http://:8080'
    'INVALIDA|http://.localhost'
    'INVALIDA|http://localhost.'
    'INVALIDA|http://local..host'
    'INVALIDA|http://[::1'
    'INVALIDA|http://[abc]:8080'
    'INVALIDA|ftp://localhost'
    'INVALIDA|file:///etc/passwd'
    'INVALIDA|//localhost:8080'
    'INVALIDA|localhost:8080'
    'INVALIDA|HTTP://localhost:8080'
    'INVALIDA|'
    $'INVALIDA|http://localhost\n@exemplo.invalid'
    $'INVALIDA|http://localhost\t:8080'
    'REMOTA|http://exemplo.invalid:8080'
    'REMOTA|https://exemplo.invalid'
    'REMOTA|http://localhost.exemplo.invalid:8080'
    'REMOTA|http://127.0.0.1.exemplo.invalid'
    'REMOTA|http://127.1:8080'
    'REMOTA|http://0x7f.1'
    'REMOTA|http://10.0.0.5:8080'
    'LOCAL|http://localhost:8080'
    'LOCAL|http://localhost'
    'LOCAL|http://localhost:8080/'
    'LOCAL|http://LOCALHOST:8080'
    'LOCAL|https://localhost:8443'
    'LOCAL|http://127.0.0.1:8080'
    'LOCAL|http://[::1]:8080'
  )
  echo "== autoteste de URL (sem rede) =="
  for c in "${casos[@]}"; do
    esperado="${c%%|*}"; url="${c#*|}"; total=$((total+1))
    if ! validar_url "$url"; then res=INVALIDA; elif [ "$URL_LOCAL" = 1 ]; then res=LOCAL; else res=REMOTA; fi
    # política: inválida nunca passa; remota só com SMOKE_PERMITIR_REMOTO=1; local sempre
    SMOKE_PERMITIR_REMOTO=0; perm0=nao; if [ "$res" != INVALIDA ] && url_permitida; then perm0=sim; fi
    SMOKE_PERMITIR_REMOTO=1; perm1=nao; if [ "$res" != INVALIDA ] && url_permitida; then perm1=sim; fi
    local pe0=nao pe1=nao
    case "$esperado" in LOCAL) pe0=sim; pe1=sim ;; REMOTA) pe1=sim ;; esac
    if [ "$res" = "$esperado" ] && [ "$perm0" = "$pe0" ] && [ "$perm1" = "$pe1" ]; then
      printf '[OK]    %-8s sem-permissão=%-3s com-permissão=%-3s %q\n' "$esperado" "$perm0" "$perm1" "$url"
    else
      falhas=$((falhas+1))
      printf '[FALHA] esperado %s (sem=%s com=%s), veio %s (sem=%s com=%s): %q\n' "$esperado" "$pe0" "$pe1" "$res" "$perm0" "$perm1" "$url"
    fi
  done
  if [ -n "$salvo" ]; then SMOKE_PERMITIR_REMOTO="$salvo"; else unset SMOKE_PERMITIR_REMOTO; fi
  echo "=============================="
  echo "$((total-falhas))/$total URLs classificadas corretamente"
  [ "$falhas" -eq 0 ]
}

# ---------- argumentos ----------
BASE_ARG=""; ESTRITO=0; AUTOTESTE=0
for arg in "$@"; do
  case "$arg" in
    --estrito) ESTRITO=1 ;;
    --autoteste-url) AUTOTESTE=1 ;;
    -*) echo "Argumento desconhecido: $arg" >&2; exit 2 ;;
    *) if [ -n "$BASE_ARG" ]; then echo "ERRO: mais de uma URL informada." >&2; exit 2; fi; BASE_ARG="$arg" ;;
  esac
done
if [ "$AUTOTESTE" = 1 ]; then autoteste_url; exit $?; fi
[ -n "$BASE_ARG" ] || BASE_ARG="http://localhost:8080"

if ! validar_url "$BASE_ARG"; then
  echo "ERRO: URL inválida ($URL_ERRO): $(printf '%q' "$BASE_ARG")" >&2
  exit 2
fi
if ! url_permitida; then
  echo "ERRO: $HOST não é local. Este script GRAVA dados de teste. Use SMOKE_PERMITIR_REMOTO=1 se for intencional." >&2
  exit 2
fi

# ---------- pré-requisitos ----------
command -v curl >/dev/null 2>&1 || { echo "ERRO: curl não encontrado." >&2; exit 2; }
TIMEOUT="${SMOKE_TIMEOUT:-15}"
case "$TIMEOUT" in ''|*[!0-9]*|0) echo "ERRO: SMOKE_TIMEOUT deve ser um inteiro positivo (segundos)." >&2; exit 2 ;; esac
CURL=(curl -s -g --connect-timeout 5 --max-time "$TIMEOUT")

# ---------- leitura semântica de JSON (jq ou python) ----------
PYNORM='
def norm(v):
    if v is None: return "null"
    if isinstance(v, bool): return "true" if v else "false"
    if isinstance(v, (int, float)): return "%g" % v
    return str(v)
'
# jv <json-objeto> <campo>            -> valor escalar ("null" se nulo/ausente; 20.0 vira 20)
jv() {
  if [ "$JSON" = jq ]; then
    printf '%s' "$1" | jq -r --arg k "$2" '.[$k] | if .==null then "null" elif type=="number" then (.*1|tostring) else tostring end' 2>/dev/null || echo "ERRO_JSON"
  else
    printf '%s' "$1" | "$JSON" -c "import sys,json
$PYNORM
print(norm(json.load(sys.stdin).get(sys.argv[1])))" "$2" 2>/dev/null || echo "ERRO_JSON"
  fi
}
# jl <json-array> <campo>             -> valores do campo, separados por espaço
jl() {
  if [ "$JSON" = jq ]; then
    printf '%s' "$1" | jq -r --arg k "$2" '[.[] | .[$k] | if .==null then "null" elif type=="number" then (.*1|tostring) else tostring end] | join(" ")' 2>/dev/null || echo "ERRO_JSON"
  else
    printf '%s' "$1" | "$JSON" -c "import sys,json
$PYNORM
print(' '.join(norm(x.get(sys.argv[1])) for x in json.load(sys.stdin)))" "$2" 2>/dev/null || echo "ERRO_JSON"
  fi
}
# jf <json-array> <chave> <valor> <campo> -> campo do 1º elemento com chave==valor ("AUSENTE" se não há)
jf() {
  if [ "$JSON" = jq ]; then
    printf '%s' "$1" | jq -r --arg a "$2" --arg b "$3" --arg k "$4" '[.[] | select(.[$a]==$b)] | .[0] | if .==null then "AUSENTE" else (.[$k] | if .==null then "null" elif type=="number" then (.*1|tostring) else tostring end) end' 2>/dev/null || echo "ERRO_JSON"
  else
    printf '%s' "$1" | "$JSON" -c "import sys,json
$PYNORM
m=[x for x in json.load(sys.stdin) if x.get(sys.argv[1])==sys.argv[2]]
print(norm(m[0].get(sys.argv[3])) if m else 'AUSENTE')" "$2" "$3" "$4" 2>/dev/null || echo "ERRO_JSON"
  fi
}
# jn <json-array> -> tamanho
jn() {
  if [ "$JSON" = jq ]; then printf '%s' "$1" | jq -r 'length' 2>/dev/null || echo "ERRO_JSON"
  else printf '%s' "$1" | "$JSON" -c "import sys,json; print(len(json.load(sys.stdin)))" 2>/dev/null || echo "ERRO_JSON"; fi
}
# Só usa um interpretador que de fato responda (no Windows, "python3" pode ser um atalho vazio).
json_funciona() { [ "$(jv '{"a":20.0,"b":null}' a)" = "20" ] && [ "$(jv '{"a":20.0,"b":null}' b)" = "null" ]; }
JSON=""
for cand in jq python3 python; do
  command -v "$cand" >/dev/null 2>&1 || continue
  JSON="$cand"
  if json_funciona; then break; fi
  JSON=""
done
if [ -z "$JSON" ]; then
  echo "ERRO: nenhum leitor de JSON funcional encontrado (tentados: jq, python3, python). Instale o jq ou o Python 3." >&2
  exit 2
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
RUN="$(date +%s)$$"
TOTAL=0; FALHAS=0; ACHADOS=0; INCONCLUSIVOS=0

ok()           { TOTAL=$((TOTAL+1)); printf '[OK]       %s\n' "$1"; }
falha()        { TOTAL=$((TOTAL+1)); FALHAS=$((FALHAS+1)); printf '[FALHA]    %s\n' "$1"; }
inconclusivo() { INCONCLUSIVOS=$((INCONCLUSIVOS+1)); printf '[INCONCL.] %s\n' "$1"; }
achado()       { ACHADOS=$((ACHADOS+1)); printf '[ACHADO]   %s\n' "$1"; }
eq() { if [ "$2" = "$3" ]; then ok "$1 (=$3)"; else falha "$1 (esperado '$2', veio '$3')"; fi; }

# ---------- HTTP (timeout em toda requisição; falha de conexão = 000, uma vez só) ----------
st_ok() { local s="$1"; printf '%s' "${s:-000}"; }
post() { local s; s="$("${CURL[@]}" -o "$TMP/body" -w '%{http_code}' -X POST "$BASE/api/leituras" -H 'Content-Type: application/json' --data "$1" 2>/dev/null)" || true; st_ok "$s"; }
get()  { "${CURL[@]}" "$BASE$1" 2>/dev/null || true; }
code() { local s; s="$("${CURL[@]}" -o "$TMP/body" -w '%{http_code}' "$BASE$1" 2>/dev/null)" || true; st_ok "$s"; }
# leit <jardim> <sensor> <nivel> <timestamp|-> [campos extras já em JSON, começando por vírgula]
leit() {
  local ts=""; [ "$4" != "-" ] && ts=",\"timestamp\":\"$4\""
  printf '{"idJardim":"%s","idSensor":"%s","nivelAgua":%s,"bateria":90%s%s}' "$1" "$2" "$3" "$ts" "${5:-}"
}
extra() { printf ',"tipo":"%s","bootId":"%s","seq":%s' "$1" "$2" "$3"; }

echo "################################################################"
echo "# ATENÇÃO: este script GRAVA dados de teste na API alvo."
echo "# Use SEMPRE um banco descartável (H2 em memória; ver o topo do script)."
echo "# Alvo: $BASE"
echo "################################################################"
if [ "$URL_LOCAL" != 1 ]; then echo "AVISO: alvo REMOTO autorizado por SMOKE_PERMITIR_REMOTO=1."; fi
echo "== SW-12 smoke | run: $RUN | JSON via: $JSON | timeout: ${TIMEOUT}s | $( [ $ESTRITO = 1 ] && echo estrito || echo 'achados informativos') =="

# ---------- Bloco 0: API de pé ----------
st="$(code /api/jardins)"
if [ "$st" != 200 ]; then echo "ERRO: GET /api/jardins devolveu $st. A API está no ar em $BASE?" >&2; exit 2; fi
ok "API respondendo (GET /api/jardins = 200)"

# ---------- Bloco T: COM timestamp (ordem cronológica do dispositivo) ----------
echo "-- T: leituras com timestamp"
JT="SW12-T-$RUN"
T1="2026-11-20T10:00:00Z"; T2="2026-11-20T10:01:00Z"; T3="2026-11-20T10:00:30Z"   # T3 chega por último, mas é do meio
eq "T: 1ª leitura (T1, 18.5, vocabulário do simulador) -> 201" 201 "$(post "{\"jardimId\":\"$JT\",\"sensorId\":\"S01\",\"nivelCm\":18.5,\"bateria\":97,\"timestamp\":\"$T1\"}")"
eq "T: 2ª leitura (T2, 20.0, vocabulário do firmware) -> 201" 201 "$(post "$(leit "$JT" S01 20.0 "$T2")")"
eq "T: reenvio idêntico da 2ª -> 200" 200 "$(post "$(leit "$JT" S01 20.0 "$T2")")"
eq "T: leitura atrasada (T3, 19.25) recebida por último -> 201" 201 "$(post "$(leit "$JT" S01 19.25 "$T3")")"
DET="$(get "/api/jardins/$JT")"
eq "T: atual = leitura de T2 (a mais recente por timestamp, não a última recebida)" 20 "$(jv "$DET" nivelCm)"
eq "T: totalLeituras = 3 (o reenvio não duplicou)" 3 "$(jv "$DET" totalLeituras)"
HIST="$(get "/api/jardins/$JT/leituras")"
eq "T: histórico mais recente primeiro (nível)" "20 19.25 18.5" "$(jl "$HIST" nivelCm)"
eq "T: histórico mais recente primeiro (timestampDispositivo)" "$T2 $T3 $T1" "$(jl "$HIST" timestampDispositivo)"
eq "T: ?limite=2 devolve 2 leituras" 2 "$(jn "$(get "/api/jardins/$JT/leituras?limite=2")")"

# ---------- Bloco D: decimais em cm ----------
echo "-- D: precisão decimal"
JD="SW12-D-$RUN"
post "$(leit "$JD" S01 42.5 2026-11-20T11:00:00Z)" >/dev/null
eq "D: 42.5 cm preservado" 42.5 "$(jv "$(get "/api/jardins/$JD")" nivelCm)"
post "$(leit "$JD" S01 7.25 2026-11-20T11:01:00Z)" >/dev/null
eq "D: 7.25 cm preservado" 7.25 "$(jv "$(get "/api/jardins/$JD")" nivelCm)"

# ---------- Bloco R: SEM timestamp (recebidoEm como alternativa) ----------
echo "-- R: leituras sem timestamp (ordem de recepção)"
JR="SW12-R-$RUN"
eq "R: 1ª sem timestamp -> 201" 201 "$(post "{\"jardimId\":\"$JR\",\"sensorId\":\"S01\",\"nivelCm\":10.0,\"bateria\":90}")"
R1="$(jv "$(cat "$TMP/body")" recebidoEm)"
sleep 2
eq "R: 2ª sem timestamp -> 201" 201 "$(post "{\"jardimId\":\"$JR\",\"sensorId\":\"S01\",\"nivelCm\":11.0,\"bateria\":90}")"
R2="$(jv "$(cat "$TMP/body")" recebidoEm)"
eq "R: reenvio idêntico sem timestamp grava de novo (não há como detectar duplicata) -> 201" 201 "$(post "{\"jardimId\":\"$JR\",\"sensorId\":\"S01\",\"nivelCm\":11.0,\"bateria\":90}")"
if [[ "$R2" > "$R1" ]]; then
  HR="$(get "/api/jardins/$JR/leituras")"
  eq "R: timestampDispositivo nulo em todas" "null null null" "$(jl "$HR" timestampDispositivo)"
  eq "R: histórico por recebidoEm, mais recente primeiro" "11 11 10" "$(jl "$HR" nivelCm)"
  eq "R: atual = última recebida" 11 "$(jv "$(get "/api/jardins/$JR")" nivelCm)"
else
  inconclusivo "R: recebidoEm não ficou distinto ($R1 / $R2); ordenação por recepção não verificada"
fi

# ---------- Bloco I: idempotência por bootId+seq ----------
echo "-- I: idempotência por bootId+seq"
JI="SW12-I-$RUN"
eq "I: bootId AAAAAAAA seq 1 -> 201" 201 "$(post "$(leit "$JI" jardim 5.0 2026-11-20T12:00:00Z "$(extra snapshot AAAAAAAA 1)")")"
eq "I: mesmo bootId+seq com timestamp DIFERENTE -> 200 (identidade é bootId+seq)" 200 "$(post "$(leit "$JI" jardim 5.0 2026-11-20T12:05:00Z "$(extra snapshot AAAAAAAA 1)")")"
eq "I: mesmo seq em OUTRO boot (reinício) -> 201" 201 "$(post "$(leit "$JI" jardim 6.0 2026-11-20T12:10:00Z "$(extra snapshot BBBBBBBB 1)")")"
eq "I: totalLeituras = 2" 2 "$(jv "$(get "/api/jardins/$JI")" totalLeituras)"
eq "I: seq de 32 bits (4294967295) aceito -> 201" 201 "$(post "$(leit "$JI" jardim 6.5 2026-11-20T12:11:00Z "$(extra snapshot BBBBBBBB 4294967295)")")"

# ---------- Bloco F: estado de reserva e boot ----------
echo "-- F: estado físico (dispositivo) e leitura sem estado"
JF="SW12-F-$RUN"
post "$(leit "$JF" S01 5.0 2026-11-20T13:00:00Z)" >/dev/null
eq "F: jardim sem estado informado -> estado null" "null" "$(jv "$(get "/api/jardins/$JF")" estado)"
post "$(leit "$JF" jardim 30.0 2026-11-20T13:01:00Z "$(extra snapshot CCCCCCCC 1),\"estado\":\"ENCHENDO\",\"tendenciaDispositivo\":\"SUBINDO\",\"versaoRegra\":\"smoke\"")" >/dev/null
eq "F: estado do dispositivo guardado como veio" ENCHENDO "$(jv "$(get "/api/jardins/$JF")" estado)"
post "$(leit "$JF" sensor-01 31.0 2026-11-20T13:01:10Z "$(extra evento CCCCCCCC 2)")" >/dev/null
DF="$(get "/api/jardins/$JF")"
eq "F: leitura mais nova SEM estado: nível é o dela" 31 "$(jv "$DF" nivelCm)"
eq "F: leitura mais nova SEM estado: estado de reserva (dentro da idade máxima)" ENCHENDO "$(jv "$DF" estado)"
post "$(leit "$JF" jardim 0.0 2026-11-20T13:05:00Z "$(extra boot DDDDDDDD 0),\"estado\":\"NORMAL\"")" >/dev/null
eq "F: boot NÃO altera o estado (continua ENCHENDO)" ENCHENDO "$(jv "$(get "/api/jardins/$JF")" estado)"
post "$(leit "$JF" jardim 50.0 2026-11-20T13:06:00Z "$(extra snapshot DDDDDDDD 1),\"estado\":\"TRANSBORDANDO\"")" >/dev/null
eq "F: nova leitura com estado -> TRANSBORDANDO" TRANSBORDANDO "$(jv "$(get "/api/jardins/$JF")" estado)"
HF="$(get "/api/jardins/$JF/leituras")"
eq "F: histórico guarda tendenciaDispositivo da leitura enviada" SUBINDO "$(jf "$HF" versaoRegra smoke tendenciaDispositivo)"
eq "F: histórico guarda o estado da leitura enviada" ENCHENDO "$(jf "$HF" versaoRegra smoke estado)"

# ---------- Bloco N: tendência (leituras determinísticas) ----------
echo "-- N: tendência do nível"
ts() { printf '2026-11-20T14:%02d:00Z' "$1"; }
JN1="SW12-N1-$RUN"; JN2="SW12-N2-$RUN"; JN3="SW12-N3-$RUN"; JN4="SW12-N4-$RUN"
i=0; for v in 10 13 16 19 22; do post "$(leit "$JN1" jardim $v "$(ts $i)" "$(extra snapshot EEEE0001 $i)")" >/dev/null; i=$((i+1)); done
i=0; for v in 22 19 16 13 10; do post "$(leit "$JN2" jardim $v "$(ts $i)" "$(extra snapshot EEEE0002 $i)")" >/dev/null; i=$((i+1)); done
i=0; for v in 20 20.5 20 19.8 20.2; do post "$(leit "$JN3" jardim $v "$(ts $i)" "$(extra snapshot EEEE0003 $i)")" >/dev/null; i=$((i+1)); done
post "$(leit "$JN4" jardim 10 "$(ts 0)" "$(extra snapshot EEEE0004 0)")" >/dev/null
post "$(leit "$JN4" jardim 12 "$(ts 1)" "$(extra snapshot EEEE0004 1)")" >/dev/null
eq "N: +12 cm em 5 leituras -> SUBINDO" SUBINDO "$(jv "$(get "/api/jardins/$JN1/tendencia")" tendencia)"
eq "N: -12 cm em 5 leituras -> DESCENDO" DESCENDO "$(jv "$(get "/api/jardins/$JN2/tendencia")" tendencia)"
eq "N: variação de 0,2 cm (< tolerância) -> ESTAVEL" ESTAVEL "$(jv "$(get "/api/jardins/$JN3/tendencia")" tendencia)"
eq "N: 2 leituras -> INSUFICIENTE" INSUFICIENTE "$(jv "$(get "/api/jardins/$JN4/tendencia")" tendencia)"
eq "N: GET /api/jardins/{id} traz a mesma tendência do endpoint de tendência" SUBINDO "$(jv "$(get "/api/jardins/$JN1")" tendencia)"

# ---------- Bloco L: lista de jardins ----------
echo "-- L: GET /api/jardins"
LISTA="$(get /api/jardins)"
IDS="$(jl "$LISTA" jardimId)"
# shellcheck disable=SC2086  # separação por palavras intencional: ids não têm espaços
SORTED="$(printf '%s\n' $IDS | LC_ALL=C sort | tr '\n' ' ' | sed 's/ $//')"
eq "L: ordenada por jardimId" "$SORTED" "$IDS"
for j in "$JT" "$JD" "$JF" "$JN1"; do
  d="$(get "/api/jardins/$j")"
  eq "L: $j - nível da lista = nível do detalhe" "$(jv "$d" nivelCm)" "$(jf "$LISTA" jardimId "$j" nivelCm)"
  eq "L: $j - estado da lista = estado do detalhe" "$(jv "$d" estado)" "$(jf "$LISTA" jardimId "$j" estado)"
  eq "L: $j - total da lista = total do detalhe" "$(jv "$d" totalLeituras)" "$(jf "$LISTA" jardimId "$j" totalLeituras)"
done

# ---------- Bloco E: erros ----------
# Os payloads inválidos usam um id isolado (JE), nunca J01–J04: se uma validação futura
# passar a aceitá-los, nada é gravado em jardins reais.
echo "-- E: erros padronizados"
JE="SW12-E-$RUN"
eq "E: sem jardimId -> 400" 400 "$(post '{"sensorId":"S01","nivelCm":1,"bateria":1}')"
eq "E: ... com erro PAYLOAD_INVALIDO" PAYLOAD_INVALIDO "$(jv "$(cat "$TMP/body")" erro)"
eq "E: nível negativo -> 400" 400 "$(post "{\"jardimId\":\"$JE\",\"sensorId\":\"S01\",\"nivelCm\":-1,\"bateria\":50}")"
eq "E: bateria 101 -> 400" 400 "$(post "{\"jardimId\":\"$JE\",\"sensorId\":\"S01\",\"nivelCm\":1,\"bateria\":101}")"
eq "E: estado inválido -> 400" 400 "$(post "{\"jardimId\":\"$JE\",\"sensorId\":\"S01\",\"nivelCm\":1,\"bateria\":50,\"estado\":\"INUNDADO\"}")"
eq "E: tendenciaDispositivo inválida -> 400" 400 "$(post "{\"jardimId\":\"$JE\",\"sensorId\":\"S01\",\"nivelCm\":1,\"bateria\":50,\"tendenciaDispositivo\":\"X\"}")"
eq "E: tipo inválido -> 400" 400 "$(post "{\"jardimId\":\"$JE\",\"sensorId\":\"S01\",\"nivelCm\":1,\"bateria\":50,\"tipo\":\"x\"}")"
eq "E: bootId não-hexadecimal -> 400" 400 "$(post "{\"jardimId\":\"$JE\",\"sensorId\":\"S01\",\"nivelCm\":1,\"bateria\":50,\"bootId\":\"ZZZZZZZZ\"}")"
eq "E: seq negativo -> 400" 400 "$(post "{\"jardimId\":\"$JE\",\"sensorId\":\"S01\",\"nivelCm\":1,\"bateria\":50,\"seq\":-1}")"
eq "E: JSON ilegível -> 400" 400 "$(post '{nao e json')"
eq "E: ... com erro CORPO_ILEGIVEL" CORPO_ILEGIVEL "$(jv "$(cat "$TMP/body")" erro)"
eq "E: jardim inexistente -> 404" 404 "$(code "/api/jardins/NAO-EXISTE-$RUN")"
eq "E: ... com erro JARDIM_NAO_ENCONTRADO" JARDIM_NAO_ENCONTRADO "$(jv "$(cat "$TMP/body")" erro)"
eq "E: histórico de jardim inexistente -> 404" 404 "$(code "/api/jardins/NAO-EXISTE-$RUN/leituras")"
eq "E: tendência de jardim inexistente -> 404" 404 "$(code "/api/jardins/NAO-EXISTE-$RUN/tendencia")"

# ---------- Bloco C: CORS ----------
echo "-- C: CORS"
acao() { "${CURL[@]}" -D - -o /dev/null "$@" 2>/dev/null | tr -d '\r' | awk 'tolower($1)=="access-control-allow-origin:"{print $2}'; }
eq "C: origem do dashboard (5173) liberada" "http://localhost:5173" "$(acao "$BASE/api/jardins" -H 'Origin: http://localhost:5173')"
eq "C: origem do preview (4173) liberada" "http://localhost:4173" "$(acao "$BASE/api/jardins" -H 'Origin: http://localhost:4173')"
eq "C: origem desconhecida NÃO recebe Allow-Origin" "" "$(acao "$BASE/api/jardins" -H 'Origin: http://evil.example')"
pf() { local s; s="$("${CURL[@]}" -o /dev/null -w '%{http_code}' -X OPTIONS "$BASE/api/jardins" -H 'Origin: http://localhost:5173' -H 'Access-Control-Request-Method: GET' -H "Access-Control-Request-Headers: $1" 2>/dev/null)" || true; st_ok "$s"; }
eq "C: preflight com Content-Type -> 200" 200 "$(pf Content-Type)"
eq "C: preflight com header não permitido -> 403" 403 "$(pf X-Evil)"

# ---------- Bloco S: ACHADOS (decisões de domínio em aberto) ----------
echo "-- S: achados conhecidos (informativos; --estrito falha)"
# F2: o registro de boot (nível fictício 0) pode virar a "leitura atual".
JB="SW12-B-$RUN"
post "$(leit "$JB" jardim 25.0 2026-11-20T15:00:00Z "$(extra snapshot FFFF0001 1)")" >/dev/null
post "$(leit "$JB" jardim 0.0 2026-11-20T15:05:00Z "$(extra boot FFFF0002 0)")" >/dev/null
NB="$(jv "$(get "/api/jardins/$JB")" nivelCm)"
if [ "$NB" = 25 ]; then ok "F2: boot (nível fictício) não aparece como nível atual"
else achado "F2: após registro boot, nivelCm atual = $NB (esperado 25, o último nível real). O dicionário inclui o boot em 'última leitura'; decisão de domínio."; fi
# F1: reinício com hora estimada (horaSincronizada=false) recomeça em 2026-10-15.
JS="SW12-S-$RUN"
post "$(leit "$JS" jardim 10.0 2026-10-15T00:10:00Z "$(extra snapshot AAAA0001 1),\"horaSincronizada\":false")" >/dev/null
post "$(leit "$JS" jardim 12.0 2026-10-15T00:11:00Z "$(extra snapshot AAAA0001 2),\"horaSincronizada\":false")" >/dev/null
post "$(leit "$JS" jardim 30.0 2026-10-15T00:00:05Z "$(extra snapshot BBBB0002 1),\"horaSincronizada\":false")" >/dev/null
NS="$(jv "$(get "/api/jardins/$JS")" nivelCm)"
if [ "$NS" = 30 ]; then ok "F1: leitura do boot novo (hora estimada menor) aparece como atual"
else achado "F1: após reinício com hora estimada, nivelCm atual = $NS (a leitura mais nova de verdade é 30). A API ordena pelo relógio do dispositivo; contrato §3.4 deixa horaSincronizada=false em aberto."; fi

# ---------- resumo ----------
echo "=============================="
echo "$((TOTAL-FALHAS))/$TOTAL asserções OK | $FALHAS falha(s) | $ACHADOS achado(s) | $INCONCLUSIVOS inconclusivo(s)"
echo "Dados gravados em $BASE: 11 jardins SW12-*-$RUN. Descarte o banco (reinicie a API em memória) depois do teste."
if [ "$FALHAS" -gt 0 ]; then exit 1; fi
if [ "$ESTRITO" = 1 ] && [ "$ACHADOS" -gt 0 ]; then echo "--estrito: achados contam como falha."; exit 1; fi
exit 0

#!/usr/bin/env bash
# [SCRUM-21][SW-12] Teste ponta a ponta parcial para validar integração.
#
# Sobe nada sozinho: suba a API antes (`./mvnw spring-boot:run` dentro de
# api-jardim-sentinela/), com a porta padrão 8080. Depois rode este script.
#
# O que ele cobre:
#   1. /teste responde (API de pé)
#   2. POST no vocabulário do simulador (jardimId/nivelCm) -> 201
#   3. POST no vocabulário do firmware (idJardim/nivelAgua/timestamp) -> 201
#   4. Reenvio do mesmo payload do firmware -> 200 (idempotência, não duplica)
#   5. GET /api/jardins/{id} reflete a última leitura e o total correto
#   6. GET /api/jardins/{id}/leituras traz o histórico, mais recente primeiro
#   7. Payload inválido -> 400 com corpo PAYLOAD_INVALIDO
#   8. Jardim inexistente -> 404
#   9. CORS: cabeçalho Access-Control-Allow-Origin presente (WebConfig ativo)
#
# Não substitui os testes automatizados (JardimControllerTests,
# LeituraIngestaoTests) — é o "cheiro de fumaça" rápido contra uma instância
# de verdade, do jeito que o card pede.

set -euo pipefail

BASE="${1:-http://localhost:8080}"
JARDIM="SMOKE-$$"  # id único por execução, pra não colidir com dado antigo
FALHAS=0

ok()   { printf '[OK]   %s\n' "$1"; }
falha(){ printf '[FALHA] %s\n' "$1"; FALHAS=$((FALHAS+1)); }

checar_status() {
  local descricao="$1" esperado="$2" obtido="$3"
  if [ "$obtido" = "$esperado" ]; then
    ok "$descricao (HTTP $obtido)"
  else
    falha "$descricao (esperado HTTP $esperado, veio $obtido)"
  fi
}

echo "== Alvo: $BASE  |  jardim de teste: $JARDIM =="

# 1. API de pé
status=$(curl -s -o /dev/null -w '%{http_code}' "$BASE/teste")
checar_status "API respondendo em /teste" 200 "$status"

# 2. Vocabulário do simulador
status=$(curl -s -o /tmp/smoke_sim.json -w '%{http_code}' -X POST "$BASE/api/leituras" \
  -H 'Content-Type: application/json' \
  -d "{\"jardimId\":\"$JARDIM\",\"sensorId\":\"S01\",\"nivelCm\":18.5,\"bateria\":97}")
checar_status "POST vocabulário simulador" 201 "$status"

# 3. Vocabulário do firmware, no MESMO endpoint
TS="2026-11-04T14:00:00Z"
status=$(curl -s -o /tmp/smoke_fw.json -w '%{http_code}' -X POST "$BASE/api/leituras" \
  -H 'Content-Type: application/json' \
  -d "{\"idJardim\":\"$JARDIM\",\"idSensor\":\"S01\",\"nivelAgua\":20.0,\"bateria\":95,\"timestamp\":\"$TS\"}")
checar_status "POST vocabulário firmware (mesmo endpoint)" 201 "$status"

# 4. Reenvio idêntico -> idempotente, não duplica
status=$(curl -s -o /dev/null -w '%{http_code}' -X POST "$BASE/api/leituras" \
  -H 'Content-Type: application/json' \
  -d "{\"idJardim\":\"$JARDIM\",\"idSensor\":\"S01\",\"nivelAgua\":20.0,\"bateria\":95,\"timestamp\":\"$TS\"}")
checar_status "Reenvio idêntico é idempotente" 200 "$status"

# 5. Estado atual reflete a última leitura (nivelAgua=20.0 venceu, é a mais recente) e total=2
resposta=$(curl -s "$BASE/api/jardins/$JARDIM")
echo "   GET /api/jardins/$JARDIM -> $resposta"
echo "$resposta" | grep -q '"nivelCm":20.0' && ok "Estado atual reflete a última leitura" \
  || falha "Estado atual não reflete a última leitura (esperado nivelCm=20.0)"
echo "$resposta" | grep -q '"totalLeituras":2' && ok "Total de leituras correto (2, sem duplicar o reenvio)" \
  || falha "Total de leituras incorreto (esperado 2)"

# 6. Histórico mais recente primeiro
resposta=$(curl -s "$BASE/api/jardins/$JARDIM/leituras")
echo "   GET /api/jardins/$JARDIM/leituras -> $resposta"
primeiro_objeto=$(echo "$resposta" | grep -o '{[^}]*}' | head -1)
echo "$primeiro_objeto" | grep -q '"nivelCm":20.0' && ok "Histórico ordenado (mais recente primeiro)" \
  || falha "Histórico não está ordenado como esperado (primeiro objeto: $primeiro_objeto)"

# 7. Payload inválido
status=$(curl -s -o /tmp/smoke_invalido.json -w '%{http_code}' -X POST "$BASE/api/leituras" \
  -H 'Content-Type: application/json' -d '{"sensorId":"S01","nivelCm":1,"bateria":1}')
checar_status "Payload sem jardimId -> erro controlado" 400 "$status"
grep -q PAYLOAD_INVALIDO /tmp/smoke_invalido.json && ok "Corpo do erro tem PAYLOAD_INVALIDO" \
  || falha "Corpo do erro não tem o formato esperado (TratadorDeErros não está em develop?)"

# 8. Jardim inexistente
status=$(curl -s -o /dev/null -w '%{http_code}' "$BASE/api/jardins/NAO-EXISTE-$$")
checar_status "Jardim inexistente -> 404" 404 "$status"

# 9. CORS: cabeçalho presente numa resposta com Origin de outra URL
cabecalho_cors=$(curl -s -D - -o /dev/null "$BASE/api/leituras" -H 'Origin: http://localhost:5173' \
  | grep -i '^Access-Control-Allow-Origin:' || true)
if [ -n "$cabecalho_cors" ]; then
  ok "CORS: cabeçalho Access-Control-Allow-Origin presente ($cabecalho_cors)"
else
  falha "CORS: cabeçalho Access-Control-Allow-Origin ausente (WebConfig não está ativo?)"
fi

echo "=============================="
if [ "$FALHAS" -eq 0 ]; then
  echo "Tudo passou. :)"
  exit 0
else
  echo "$FALHAS checagem(ns) falhando."
  exit 1
fi

#!/usr/bin/env python3
"""Receptor HTTP 2xx local para o ensaio de 20 min do ESP32.

O firmware posta uma linha NDJSON por requisicao. Este servidor:

- responde 200 para qualquer POST (caminho configuravel na URL);
- grava cada requisicao em `recebidas.ndjson` (metadados + corpo);
- grava o corpo cru em `recebidas.corpo.ndjson` (uma linha por requisicao);
- calcula unicos, duplicatas por (bootId, seq) e lacunas por boot;
- expoe /stats, /export e /reset.

Sem dependencias externas. Uso:

    python3 receber.py --port 8000 --token MEU_TOKEN
    python3 receber.py --analisar recebidas.ndjson

Para o ESP32 no Wokwi, exponha a porta com um tunel HTTPS (cloudflared) e
configure `URL_RECEPTOR` em `esp32/src/secrets.h`. Veja o README.md desta pasta.
"""

from __future__ import annotations

import argparse
import json
import sys
import threading
from collections import Counter, defaultdict
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ARQUIVO = "recebidas.ndjson"
ARQUIVO_CORPO = "recebidas.corpo.ndjson"


# ===== ANALISE ===============================================================


def calcular_resumo(registros: list[dict]) -> dict:
    """Resume as requisicoes recebidas.

    `lacunas_no_intervalo` NAO significa perda: o firmware dizima snapshots
    (1 a cada 10) e pode descartar por fila cheia/compactacao. A conferencia
    final usa os contadores do `'i'` no monitor serial.
    """
    total = len(registros)
    chaves: Counter[tuple[str, int]] = Counter()
    tipos: Counter[str] = Counter()
    sensores: Counter[str] = Counter()
    origens: Counter[str] = Counter()
    hora: Counter[str] = Counter()
    niveis: dict[str, list[float]] = defaultdict(list)
    por_boot: dict[str, dict] = {}
    invalidas = 0
    bytes_total = 0
    recebidos: list[str] = []

    for registro in registros:
        bytes_total += int(registro.get("bytes") or 0)
        quando = registro.get("recebidoEm")
        if quando:
            recebidos.append(str(quando))

        corpo = registro.get("corpo")
        if not isinstance(corpo, dict):
            invalidas += 1
            continue

        boot = str(corpo.get("bootId", ""))
        seq = corpo.get("seq")
        if boot and isinstance(seq, int) and not isinstance(seq, bool):
            chaves[(boot, seq)] += 1
            info = por_boot.setdefault(
                boot, {"min": seq, "max": seq, "seqs": set()}
            )
            info["min"] = min(info["min"], seq)
            info["max"] = max(info["max"], seq)
            info["seqs"].add(seq)

        tipos[str(corpo.get("tipo", "?"))] += 1
        sensores[str(corpo.get("idSensor", "?"))] += 1
        origens[str(corpo.get("origem", "?"))] += 1
        if "horaSincronizada" in corpo:
            hora["true" if corpo["horaSincronizada"] else "false"] += 1

        nivel = corpo.get("nivelAgua")
        if isinstance(nivel, (int, float)) and not isinstance(nivel, bool):
            niveis[str(corpo.get("idSensor", "?"))].append(float(nivel))

    duplicadas = {f"{boot}/{seq}": n for (boot, seq), n in chaves.items() if n > 1}
    boots: dict[str, dict] = {}
    for boot, info in por_boot.items():
        faltando = sorted(set(range(info["min"], info["max"] + 1)) - info["seqs"])
        boots[boot] = {
            "seq_min": info["min"],
            "seq_max": info["max"],
            "seqs_recebidos": len(info["seqs"]),
            "lacunas_no_intervalo": len(faltando),
            "primeiras_lacunas": faltando[:20],
        }

    resumo_niveis = {
        sensor: {
            "min": min(valores),
            "max": max(valores),
            "amostras": len(valores),
        }
        for sensor, valores in niveis.items()
    }

    return {
        "requisicoes": total,
        "com_chave": sum(chaves.values()),
        "unicas": len(chaves),
        "duplicadas": sum(n - 1 for n in chaves.values()),
        "duplicadas_por_chave": duplicadas,
        "invalidas": invalidas,
        "bytes": bytes_total,
        "primeira_recebida": min(recebidos) if recebidos else None,
        "ultima_recebida": max(recebidos) if recebidos else None,
        "por_tipo": dict(tipos),
        "por_sensor": dict(sensores),
        "por_origem": dict(origens),
        "hora_sincronizada": dict(hora),
        "niveis": resumo_niveis,
        "boots": boots,
    }


def carregar_arquivo(caminho: Path) -> list[dict]:
    """Le um arquivo do receptor ou um NDJSON cru de corpos.

    Aceita tanto `recebidas.ndjson` (objetos com a chave `corpo`) quanto
    `recebidas.corpo.ndjson`/exportacoes de outros receptores (um JSON por
    linha, sem metadados).
    """
    registros: list[dict] = []
    with caminho.open("r", encoding="utf-8") as arquivo:
        for linha in arquivo:
            linha = linha.strip()
            if not linha:
                continue
            try:
                objeto = json.loads(linha)
            except json.JSONDecodeError:
                continue
            if isinstance(objeto, dict) and "corpo" in objeto:
                registros.append(objeto)
            else:
                registros.append({"corpo": objeto, "bytes": len(linha)})
    return registros


def imprimir_resumo(resumo: dict) -> None:
    print(json.dumps(resumo, ensure_ascii=False, indent=2))


# ===== SERVIDOR ==============================================================


class Estado:
    def __init__(self, pasta: Path) -> None:
        self.pasta = pasta
        self.arquivo = pasta / ARQUIVO
        self.arquivo_corpo = pasta / ARQUIVO_CORPO
        self.trava = threading.Lock()
        self.registros: list[dict] = []
        self._carregar()

    def _carregar(self) -> None:
        if not self.arquivo.exists():
            return
        try:
            self.registros = carregar_arquivo(self.arquivo)
        except OSError as erro:
            print(f"AVISO: nao consegui ler {self.arquivo}: {erro}", file=sys.stderr)

    def registrar(self, registro: dict) -> int:
        with self.trava:
            self.registros.append(registro)
            self.pasta.mkdir(parents=True, exist_ok=True)
            with self.arquivo.open("a", encoding="utf-8") as arquivo:
                arquivo.write(json.dumps(registro, ensure_ascii=False) + "\n")

            corpo = registro.get("corpo")
            with self.arquivo_corpo.open("a", encoding="utf-8") as arquivo:
                if isinstance(corpo, (dict, list)):
                    arquivo.write(json.dumps(corpo, ensure_ascii=False) + "\n")
                else:
                    arquivo.write(str(corpo) + "\n")
            return len(self.registros)

    def resumo(self) -> dict:
        with self.trava:
            return calcular_resumo(list(self.registros))

    def limpar(self) -> None:
        with self.trava:
            self.registros.clear()
            self.pasta.mkdir(parents=True, exist_ok=True)
            self.arquivo.write_text("", encoding="utf-8")
            self.arquivo_corpo.write_text("", encoding="utf-8")


class Servidor(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, endereco, handler, estado: Estado, token: str, quieto: bool):
        super().__init__(endereco, handler)
        self.estado = estado
        self.token = token
        self.quieto = quieto


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    # ----- utilidades -----

    def _autorizado(self) -> bool:
        token = self.server.token
        if not token:
            return True
        consulta = parse_qs(urlparse(self.path).query)
        if consulta.get("token", [""])[0] == token:
            return True
        return self.headers.get("X-Token") == token

    def _responder(
        self,
        codigo: int,
        corpo: bytes,
        tipo: str = "application/json; charset=utf-8",
        extra: dict[str, str] | None = None,
    ) -> None:
        self.send_response(codigo)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        for chave, valor in (extra or {}).items():
            self.send_header(chave, valor)
        self.end_headers()
        if corpo:
            self.wfile.write(corpo)

    def _json(self, codigo: int, objeto: dict) -> None:
        corpo = json.dumps(objeto, ensure_ascii=False, indent=2).encode("utf-8")
        self._responder(codigo, corpo)

    def log_message(self, formato: str, *args) -> None:
        if not self.server.quieto:
            print(f"[{self.log_date_time_string()}] {self.address_string()} {formato % args}")

    # ----- rotas -----

    def do_POST(self) -> None:
        caminho = urlparse(self.path).path
        if caminho.rstrip("/") == "/reset":
            if not self._autorizado():
                self._json(401, {"erro": "token invalido"})
                return
            self.server.estado.limpar()
            self._json(200, {"ok": True, "mensagem": "arquivos limpos"})
            return

        if not self._autorizado():
            self._json(401, {"erro": "token invalido"})
            return

        try:
            tamanho = int(self.headers.get("Content-Length", "0") or 0)
        except ValueError:
            tamanho = 0
        corpo = self.rfile.read(tamanho) if tamanho > 0 else b""
        texto = corpo.decode("utf-8", errors="replace")

        try:
            objeto = json.loads(texto)
        except json.JSONDecodeError:
            objeto = texto

        registro = {
            "recebidoEm": datetime.now(timezone.utc)
            .isoformat(timespec="milliseconds")
            .replace("+00:00", "Z"),
            "ip": self.client_address[0],
            "caminho": self.path,
            "bytes": len(corpo),
            "corpo": objeto,
        }
        total = self.server.estado.registrar(registro)
        self._json(200, {"ok": True, "total": total})

    def do_GET(self) -> None:
        caminho = urlparse(self.path).path.rstrip("/") or "/"

        if caminho == "/":
            self._responder(
                200,
                b"receptor ativo\nuse /stats e /export (com ?token= se configurado)\n",
                "text/plain; charset=utf-8",
            )
            return

        if not self._autorizado():
            self._json(401, {"erro": "token invalido"})
            return

        if caminho == "/stats":
            self._json(200, self.server.estado.resumo())
            return

        if caminho == "/export":
            arquivo = self.server.estado.arquivo_corpo
            if not arquivo.exists():
                self._json(404, {"erro": "nenhuma requisicao recebida"})
                return
            conteudo = arquivo.read_bytes()
            self._responder(
                200,
                conteudo,
                "application/x-ndjson; charset=utf-8",
                {"Content-Disposition": f'attachment; filename="{ARQUIVO_CORPO}"'},
            )
            return

        self._json(404, {"erro": "rota desconhecida", "caminho": caminho})


# ===== CLI ===================================================================


def servidor(args: argparse.Namespace) -> None:
    pasta = Path(args.pasta).resolve()
    estado = Estado(pasta)
    httpd = Servidor((args.host, args.port), Handler, estado, args.token, args.quieto)

    print(f"Receptor ouvindo em http://{args.host}:{args.port}")
    print(f"Arquivos em {pasta}")
    if args.token:
        print("Token exigido: use ?token=... ou o header X-Token")
    else:
        print("AVISO: sem token; qualquer um que alcance a porta pode postar")
    print("Pare com Ctrl+C.\n")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nEncerrando...")
    finally:
        httpd.server_close()
        imprimir_resumo(estado.resumo())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1", help="endereco de escuta (padrao: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8000, help="porta (padrao: 8000)")
    parser.add_argument("--token", default="", help="exige ?token=... ou X-Token")
    parser.add_argument("--pasta", default=".", help="pasta dos arquivos (padrao: .)")
    parser.add_argument("--quieto", action="store_true", help="nao loga cada requisicao")
    parser.add_argument("--analisar", metavar="ARQUIVO", help="apenas resume um arquivo e sai")
    args = parser.parse_args()

    if args.analisar:
        caminho = Path(args.analisar)
        if not caminho.exists():
            print(f"ERRO: {caminho} nao existe", file=sys.stderr)
            return 2
        imprimir_resumo(calcular_resumo(carregar_arquivo(caminho)))
        return 0

    servidor(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

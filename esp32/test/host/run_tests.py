#!/usr/bin/env python3
"""Testes automatizados de host do firmware do ESP32.

Os testes compilam funcoes reais extraidas de src/main_alt.cpp e o
src/hw02_sonda.cpp inteiro contra stubs de Arduino/LittleFS. Nao precisam de
placa, Wi-Fi ou receptor HTTPS.

Uso:
    python3 test/host/run_tests.py
    python3 test/host/run_tests.py --verbose
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ESP32 = HERE.parents[1]
SRC = ESP32 / "src" / "main_alt.cpp"
SONDA_SRC = ESP32 / "src" / "hw02_sonda.cpp"
BUILD = HERE / ".build"

# Funcoes extraidas do firmware, por grupo de teste. O extrator usa o corpo
# real; os stubs ficam em cada arquivo de teste.
GROUPS = {
    "latencia": [
        "zerarLatencia",
        "registrarLatencia",
        "registrarLatenciaDoRegistro",
        "percentilLatencia",
    ],
    "compactacao": [
        "lerLinha",
        "extrairChave",
        "indiceResolvido",
        "contarLinhasNoPrefixo",
        "offsetDoCursor",
        "contarPerdidosNoPrefixo",
        "inicioDaLinhaSeguinte",
        "reescreverTrecho",
        "fimDaUltimaLinhaCompleta",
        "recuperarLinhaParcial",
        "compactarFila",
        "forcarCompactacao",
        "injetarLinhaTruncada",
    ],
    "drenagem": [
        "lerLinha",
        "extrairChave",
        "indiceResolvido",
        "removerResolvido",
        "marcarResolvido",
        "pularAteCursor",
        "abrirAposCursor",
        "tipoDaLinha",
        "avancarCursorResolvido",
        "adicionarCandidato",
        "buscarAncora",
        "coletarBoot",
        "buscarEventos",
        "buscarBackfill",
        "selecionarLote",
    ],
    "url": ["alternarUrlInvalida"],
    "backoff": ["backoffMs", "backoff429Ms", "agendarNovaTentativa"],
}

TESTS = [
    # nome, arquivo, grupos de funcoes geradas
    ("codec", "test_codec.cpp", []),
    ("compactacao", "test_compactacao.cpp", ["compactacao"]),
    ("latencia", "test_latencia.cpp", ["latencia"]),
    ("drenagem_20min", "test_drenagem_20min.cpp", ["drenagem"]),
    ("url", "test_url.cpp", ["url"]),
    ("backoff", "test_backoff.cpp", ["backoff"]),
    ("sonda", "test_sonda.cpp", []),
]

START_RE = re.compile(
    r"^(?:static\s+)?(?:bool|void|int|uint32_t|size_t|float|File|uint8_t|"
    r"const\s+char\s*\*)\s+"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*\(",
    re.MULTILINE,
)


def extract_function(text: str, name: str) -> str:
    """Extrai uma funcao pelo nome, equilibrando chaves e ignorando strings."""
    match = None
    for m in START_RE.finditer(text):
        if m.group(1) != name:
            continue
        # Ignora prototipos (declaracao termina em ';' antes de qualquer '{').
        brace = text.find("{", m.end())
        semi = text.find(";", m.end())
        if semi != -1 and (brace == -1 or semi < brace):
            continue
        match = m
        break
    if match is None:
        raise RuntimeError(f"funcao nao encontrada (com corpo): {name}")

    start = match.start()
    brace = text.find("{", match.end())
    if brace < 0:
        raise RuntimeError(f"corpo nao encontrado: {name}")

    i = brace
    depth = 0
    string: str | None = None
    escape = False
    line_comment = False
    block_comment = False
    while i < len(text):
        c = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if line_comment:
            if c == "\n":
                line_comment = False
        elif block_comment:
            if c == "*" and nxt == "/":
                block_comment = False
                i += 1
        elif string is not None:
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == string:
                string = None
        else:
            if c == "/" and nxt == "/":
                line_comment = True
                i += 1
            elif c == "/" and nxt == "*":
                block_comment = True
                i += 1
            elif c in ('"', "'"):
                string = c
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    return text[start : i + 1]
        i += 1
    raise RuntimeError(f"chaves nao fecharam: {name}")


def gerar_headers(source: str) -> None:
    BUILD.mkdir(parents=True, exist_ok=True)
    for grupo, nomes in GROUPS.items():
        trechos = []
        for nome in nomes:
            trechos.append(extract_function(source, nome))
        destino = BUILD / f"{grupo}.inc"
        destino.write_text(
            f"// Gerado por run_tests.py a partir de src/main_alt.cpp\n\n"
            + "\n\n".join(trechos)
            + "\n",
            encoding="utf-8",
        )
        print(f"  extraido: {grupo}.inc ({len(trechos)} funcoes)")


def gerar_sonda_inc() -> None:
    """Copia o hw02_sonda.cpp inteiro: ele tem constantes e estado de modulo."""
    destino = BUILD / "sonda.inc"
    destino.write_text(
        "// Gerado por run_tests.py a partir de src/hw02_sonda.cpp\n\n"
        + SONDA_SRC.read_text(encoding="utf-8")
        + "\n",
        encoding="utf-8",
    )
    print("  extraido: sonda.inc (arquivo inteiro)")


def compilar(nome: str, arquivo: str) -> Path:
    exe = BUILD / nome
    cmd = [
        "g++",
        "-std=c++17",
        "-O2",
        "-Wall",
        "-Wextra",
        "-I",
        str(ESP32 / "src"),
        "-I",
        str(BUILD),
        "-I",
        str(HERE / "stubs"),
        str(HERE / arquivo),
        "-o",
        str(exe),
    ]
    proc = subprocess.run(cmd, text=True, capture_output=True)
    if proc.returncode != 0:
        print(proc.stdout)
        print(proc.stderr)
        raise RuntimeError(f"falha ao compilar {arquivo}")
    return exe


def rodar(exe: Path, verbose: bool) -> None:
    proc = subprocess.run([str(exe)], text=True, capture_output=True)
    if proc.returncode != 0:
        print(proc.stdout)
        print(proc.stderr)
        raise RuntimeError(f"teste falhou: {exe.name}")
    if verbose:
        print(proc.stdout.rstrip())
    else:
        linhas = [l for l in proc.stdout.splitlines() if l.strip()]
        print(linhas[-1] if linhas else "OK")


def check_estatico() -> bool:
    """Checagens baratas de contrato que nao dependem de execucao."""
    ok = True

    # Criterio 12: nenhuma chamada delay( no codigo (comentarios nao contam).
    achados = []
    for arquivo in (SRC, SONDA_SRC):
        for numero, linha in enumerate(
            arquivo.read_text(encoding="utf-8").splitlines(), 1
        ):
            codigo = linha.split("//", 1)[0]
            if re.search(r"\bdelay\s*\(", codigo):
                achados.append(f"{arquivo.name} linha {numero}: {linha.strip()}")
    if achados:
        print("FALHOU sem delay(: " + "; ".join(achados))
        ok = False
    else:
        print("OK   sem chamada delay( no firmware")

    # Criterio 11: secrets.h ignorado e nao versionado.
    repo = subprocess.run(
        ["git", "-C", str(ESP32), "rev-parse", "--show-toplevel"],
        text=True,
        capture_output=True,
    )
    if repo.returncode != 0:
        print("SKIP git nao disponivel para checar secrets.h")
        return ok

    raiz = Path(repo.stdout.strip())
    ignorado = subprocess.run(
        ["git", "-C", str(raiz), "check-ignore", "-q", "esp32/src/secrets.h"]
    )
    versionado = subprocess.run(
        ["git", "-C", str(raiz), "ls-files", "--", "esp32/src/secrets.h"],
        text=True,
        capture_output=True,
    )
    exemplo = subprocess.run(
        ["git", "-C", str(raiz), "ls-files", "--", "esp32/src/secrets.example.h"],
        text=True,
        capture_output=True,
    )
    if ignorado.returncode == 0:
        print("OK   secrets.h esta no .gitignore")
    else:
        print("FALHOU secrets.h nao esta no .gitignore")
        ok = False
    if versionado.stdout.strip():
        print("FALHOU secrets.h esta versionado")
        ok = False
    else:
        print("OK   secrets.h nao esta versionado")
    if exemplo.stdout.strip():
        print("OK   secrets.example.h esta versionado")
    else:
        print("FALHOU secrets.example.h nao esta versionado")
        ok = False
    return ok


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    if not SRC.exists() or not SONDA_SRC.exists():
        print(f"ERRO: arquivo de firmware nao existe ({SRC} / {SONDA_SRC})")
        return 2
    if shutil.which("g++") is None:
        print("ERRO: g++ nao encontrado")
        return 2

    print("== extraindo funcoes reais ==")
    try:
        gerar_headers(SRC.read_text(encoding="utf-8"))
        gerar_sonda_inc()
    except RuntimeError as erro:
        print(f"ERRO: {erro}")
        return 2

    print("== compilando e executando ==")
    falhas = 0
    for nome, arquivo, _grupos in TESTS:
        try:
            exe = compilar(nome, arquivo)
            print(f"[{nome}] ", end="")
            rodar(exe, args.verbose)
        except RuntimeError as erro:
            print(f"\nERRO: {erro}")
            falhas += 1

    print("== checagens estaticas ==")
    if not check_estatico():
        falhas += 1

    total = len(TESTS) + 1
    print(f"\nResumo: {total - falhas}/{total} grupos OK")
    return 0 if falhas == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

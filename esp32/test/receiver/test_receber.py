#!/usr/bin/env python3
"""Autoteste do receptor local (sem rede externa).

Sobe o servidor em uma porta efemera, posta alguns registros e confere o
resumo. Uso:

    python3 test_receber.py
"""

from __future__ import annotations

import json
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

sys.path.insert(0, str(Path(__file__).resolve().parent))

import receber  # noqa: E402


class ReceptorTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.estado = receber.Estado(Path(self.tmp.name))
        self.httpd = receber.Servidor(
            ("127.0.0.1", 0), receber.Handler, self.estado, "tok", True
        )
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.thread.start()
        self.base = f"http://127.0.0.1:{self.httpd.server_address[1]}"

    def tearDown(self) -> None:
        self.httpd.shutdown()
        self.httpd.server_close()
        self.thread.join(timeout=2)
        self.tmp.cleanup()

    def postar(self, corpo: dict, token: str = "tok") -> dict:
        dados = json.dumps(corpo).encode("utf-8")
        url = f"{self.base}/api/leituras?token={token}" if token else f"{self.base}/api/leituras"
        pedido = Request(
            url,
            data=dados,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urlopen(pedido, timeout=2) as resposta:
            return json.loads(resposta.read())

    def stats(self) -> dict:
        with urlopen(f"{self.base}/stats?token=tok", timeout=2) as resposta:
            return json.loads(resposta.read())

    def test_fluxo_completo(self) -> None:
        boot = {
            "tipo": "boot",
            "idJardim": "jardim-centro-01",
            "idSensor": "jardim",
            "nivelAgua": 0.0,
            "bateria": 100,
            "timestamp": "2026-10-31T12:00:00Z",
            "seq": 0,
            "bootId": "AABBCCDD",
            "horaSincronizada": True,
            "origem": "simulacao",
        }
        snapshot = dict(boot, tipo="snapshot", seq=10, nivelAgua=42.5)

        self.assertEqual(self.postar(boot)["total"], 1)
        self.assertEqual(self.postar(snapshot)["total"], 2)
        self.assertEqual(self.postar(snapshot)["total"], 3)

        resumo = self.stats()
        self.assertEqual(resumo["requisicoes"], 3)
        self.assertEqual(resumo["unicas"], 2)
        self.assertEqual(resumo["duplicadas"], 1)
        self.assertEqual(resumo["invalidas"], 0)
        self.assertEqual(resumo["por_tipo"], {"boot": 1, "snapshot": 2})
        self.assertEqual(resumo["boots"]["AABBCCDD"]["seq_max"], 10)

        with urlopen(f"{self.base}/export?token=tok", timeout=2) as resposta:
            linhas = [l for l in resposta.read().decode().splitlines() if l]
        self.assertEqual(len(linhas), 3)

        with self.assertRaises(HTTPError) as erro:
            self.postar(boot, token="")
        self.assertEqual(erro.exception.code, 401)

        pedido = Request(f"{self.base}/reset?token=tok", method="POST")
        with urlopen(pedido, timeout=2) as resposta:
            self.assertEqual(resposta.status, 200)
        self.assertEqual(self.stats()["requisicoes"], 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)

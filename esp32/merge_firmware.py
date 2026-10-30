Import("env")
import os
import subprocess

# Imagem unica para gravar em 0x0: bootloader + tabela de particoes + app.
IMAGENS = (
    ("0x1000", "bootloader.bin"),
    ("0x8000", "partitions.bin"),
    ("0x10000", None),  # None = imagem da aplicacao ($BUILD_DIR/${PROGNAME}.bin)
)


def _primeiro_arquivo(base, nomes):
    for nome in nomes:
        caminho = os.path.join(base, nome)
        if os.path.isfile(caminho):
            return caminho
    return None


def _esptool(env):
    """Localiza o esptool e devolve (prefixo_do_comando, usa_sintaxe_nova).

    PlatformIO 8.x (platform-espressif32 8.x) instala o binario autonomo em
    tool-esptool: "esptool" no Linux/macOS e "esptool.exe" no Windows.
    Plataformas mais antigas usam tool-esptoolpy com o script esptool.py.
    """
    plataforma = env.PioPlatform()

    # PlatformIO novo: binario autonomo (esptool 5.x, comandos com hifen).
    try:
        base = plataforma.get_package_dir("tool-esptool")
    except Exception:
        base = None
    if base and os.path.isdir(base):
        executavel = _primeiro_arquivo(base, ("esptool", "esptool.exe"))
        if executavel:
            return [executavel], True

    # PlatformIO antigo: script Python (esptool 4.x, comandos com underline).
    try:
        base = plataforma.get_package_dir("tool-esptoolpy")
    except Exception:
        base = None
    if base and os.path.isdir(base):
        script = _primeiro_arquivo(base, ("esptool.py",))
        if script:
            return [env.subst("$PYTHONEXE"), script], False

    return None, None


def merge_firmware(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    merged = os.path.join(build_dir, "firmware.merged.bin")

    prefixo, sintaxe_nova = _esptool(env)
    if not prefixo:
        raise RuntimeError(
            "merge_firmware: esptool nao encontrado. "
            "Rode 'pio pkg install' para instalar a plataforma espressif32."
        )

    # esptool 5.x usa merge-bin/--flash-size; esptool 4.x usa merge_bin/--flash_size.
    comando = prefixo + [
        "--chip", "esp32",
        "merge-bin" if sintaxe_nova else "merge_bin",
        "-o", merged,
        "--flash-size" if sintaxe_nova else "--flash_size", "4MB",
    ]
    for endereco, nome in IMAGENS:
        if nome is None:
            nome = env.subst("$BUILD_DIR/${PROGNAME}.bin")
        else:
            nome = os.path.join(build_dir, nome)
        comando += [endereco, nome]

    print("merge_firmware: %s" % " ".join(comando))
    resultado = subprocess.run(comando)
    if resultado.returncode != 0:
        raise RuntimeError(
            "merge_firmware: esptool terminou com codigo %d" % resultado.returncode
        )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)

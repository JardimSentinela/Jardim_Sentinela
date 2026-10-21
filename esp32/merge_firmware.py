
Import("env")
import os

def merge_firmware(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    merged = os.path.join(build_dir, "firmware.merged.bin")
    esptool = os.path.join(
        env.PioPlatform().get_package_dir("tool-esptoolpy"),
        "esptool.py"
    )

    images = [
        "0x1000", os.path.join(build_dir, "bootloader.bin"),
        "0x8000", os.path.join(build_dir, "partitions.bin"),
        "0x10000", os.path.join(build_dir, "firmware.bin")
    ]

    command = [
        env.subst("$PYTHONEXE"), esptool,
        "--chip", "esp32", "merge_bin",
        "-o", merged,
        "--flash_size", "4MB"
    ] + images

    env.Execute(" ".join('"' + item + '"' for item in command))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)

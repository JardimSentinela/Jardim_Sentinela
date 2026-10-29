Import("env")
import os

def merge_firmware(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    merged = os.path.join(build_dir, "firmware.merged.bin")

    esptool = os.path.join(
        env.PioPlatform().get_package_dir("tool-esptool"),
        "esptool.exe"
    )

    images = [
        "0x1000", os.path.join(build_dir, "bootloader.bin"),
        "0x8000", os.path.join(build_dir, "partitions.bin"),
        "0x10000", os.path.join(build_dir, "firmware.bin")
    ]

    command = [
        '"' + esptool + '"',
        "--chip", "esp32", "merge-bin",
        "-o", '"' + merged + '"',
        "--flash-size", "4MB"
    ] + [
        '"' + item + '"' if os.path.isabs(item) else item
        for item in images
    ]

    env.Execute(" ".join(command))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)
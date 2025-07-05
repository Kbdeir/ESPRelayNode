Import("env")
import os
import serial.tools.list_ports

DUMP_SIZE = "0x400000"
OUTPUT_FILE = "esp32_flash_dump.bin"

def get_esp32_com_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if ("USB" in p.description or "UART" in p.description or "CP210" in p.description or "CH340" in p.description) and ("ESP" in p.description or "Silicon Labs" in p.manufacturer or "wch.cn" in p.manufacturer or "FTDI" in p.manufacturer or "CP210" in p.device):
            return p.device
    if ports:
        return ports[0].device  # fallback to first available port
    else:
        raise RuntimeError("No COM port found. Connect your ESP32.")

def dump_flash(source, target, env):
    com_port = get_esp32_com_port()
    print(f"==> Detected COM port: {com_port}")
    cmd = f'esptool.py --port {com_port} read_flash 0x00000 {DUMP_SIZE} {OUTPUT_FILE}'
    print(f"==> Executing: {cmd}")
    os.system(cmd)
    print(f"==> Flash dumped to {OUTPUT_FILE}")

env.AddCustomTarget(
    name="dump_flash",
    dependencies=None,
    actions=[dump_flash],
    title="Dump Flash",
    description="Automatically dump ESP32 flash to esp32_flash_dump.bin"
)

Import("env")
import os
import serial.tools.list_ports

DUMP_FILE = "esp32_flash_dump.bin"
FLASH_SIZE = "0x400000"
FLASH_OFFSET = "0x00000"

def get_esp32_com_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if ("USB" in p.description or "UART" in p.description or "CP210" in p.description or "CH340" in p.description) and ("ESP" in p.description or "Silicon Labs" in p.manufacturer or "wch.cn" in p.manufacturer or "FTDI" in p.manufacturer or "CP210" in p.device):
            return p.device
    if ports:
        return ports[0].device  # fallback to first available port
    else:
        raise RuntimeError("No COM port found. Connect your ESP32.")

def restore_flash(source, target, env):
    if not os.path.exists(DUMP_FILE):
        raise RuntimeError(f"{DUMP_FILE} not found. Run dump_flash first.")
    com_port = get_esp32_com_port()
    print(f"==> Detected COM port: {com_port}")
    print("==> Restoring flash, this will overwrite the device contents.")
    cmd = f'esptool.py --port {com_port} write_flash {FLASH_OFFSET} {DUMP_FILE}'
    print(f"==> Executing: {cmd}")
    os.system(cmd)
    print("==> Flash restore complete.")

env.AddCustomTarget(
    name="restore_flash",
    dependencies=None,
    actions=[restore_flash],
    title="Restore Flash",
    description="Flash esp32_flash_dump.bin back to ESP32"
)

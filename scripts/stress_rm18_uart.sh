#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_PATH="${CONFIG_PATH:-/etc/sunray-core/config.json}"

python3 - "$ROOT_DIR" "$CONFIG_PATH" <<'PY'
import json
import os
import select
import sys
import termios
import time

root_dir = sys.argv[1]
config_path = sys.argv[2]

driver_port = "/dev/ttyS0"
driver_baud = 19200
duration_s = 5.0

try:
    with open(config_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    driver_port = data.get("driver_port", driver_port)
    driver_baud = int(data.get("driver_baud", driver_baud))
except Exception:
    pass

BAUD_MAP = {
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
}

def crc_for(body: str) -> int:
    return sum(body.encode("ascii")) & 0xFF

def frame_for(body: str) -> bytes:
    return f"{body},0x{crc_for(body):02X}\r\n".encode("ascii")

def decode_ascii(blob: bytes) -> str:
    return blob.decode("ascii", errors="replace").replace("\r", "\\r").replace("\n", "\\n")

print("== RM18 UART Stress Check ==")
print(f"Repo:    {root_dir}")
print(f"Config:  {config_path}")
print(f"UART:    {driver_port}")
print(f"Baud:    {driver_baud}")
print(f"Window:  {duration_s:.1f}s")

if not os.path.exists(driver_port):
    print(f"\nERROR: UART device not found: {driver_port}")
    sys.exit(4)

fd = os.open(driver_port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
try:
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CLOCAL | termios.CREAD
    attrs[3] = 0
    attrs[4] = BAUD_MAP.get(driver_baud, termios.B19200)
    attrs[5] = BAUD_MAP.get(driver_baud, termios.B19200)
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcflush(fd, termios.TCIOFLUSH)
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)

    start = time.monotonic()
    next_m = start
    next_s = start + 0.5
    tx_m = 0
    tx_s = 0
    rx_blob = bytearray()

    while time.monotonic() - start < duration_s:
        now = time.monotonic()
        if now >= next_m:
            os.write(fd, frame_for("AT+M,0,0,0"))
            tx_m += 1
            next_m += 0.02
        if now >= next_s:
            os.write(fd, frame_for("AT+S"))
            tx_s += 1
            next_s += 0.5

        r, _, _ = select.select([fd], [], [], 0.01)
        if r:
            chunk = os.read(fd, 512)
            if chunk:
                rx_blob.extend(chunk)

    time.sleep(0.2)
    while True:
        r, _, _ = select.select([fd], [], [], 0.05)
        if not r:
            break
        chunk = os.read(fd, 512)
        if not chunk:
            break
        rx_blob.extend(chunk)

    ascii_blob = decode_ascii(bytes(rx_blob))
    rx_m = ascii_blob.count("M,")
    rx_s = ascii_blob.count("S,")
    rx_v = ascii_blob.count("V,")

    print("\nSummary")
    print(f"  TX M:   {tx_m}")
    print(f"  TX S:   {tx_s}")
    print(f"  RX M:   {rx_m}")
    print(f"  RX S:   {rx_s}")
    print(f"  RX V:   {rx_v}")
    print(f"  bytes:  {len(rx_blob)}")
    print(f"  ascii:  {ascii_blob[:1200] if ascii_blob else '<no response>'}")

    if rx_m > 0 and rx_s > 0:
        print("\nResult: RM18 responds even under sunray-core-like polling load.")
        sys.exit(0)
    if len(rx_blob) > 0:
        print("\nResult: RM18 responds under load, but frame extraction needs care.")
        sys.exit(2)
    print("\nResult: RM18 does not answer under sustained polling load.")
    sys.exit(3)
finally:
    os.close(fd)
PY

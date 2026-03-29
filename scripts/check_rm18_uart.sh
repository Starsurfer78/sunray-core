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

def read_response(fd: int, timeout_s: float = 1.0) -> bytes:
    deadline = time.monotonic() + timeout_s
    chunks = []
    while time.monotonic() < deadline:
        remaining = max(0.0, deadline - time.monotonic())
        r, _, _ = select.select([fd], [], [], remaining)
        if not r:
            break
        chunk = os.read(fd, 512)
        if not chunk:
            break
        chunks.append(chunk)
        # RM18 often answers as one compact frame without CR/LF at the end.
        if b",0x" in b"".join(chunks):
            time.sleep(0.05)
            while True:
                r2, _, _ = select.select([fd], [], [], 0.05)
                if not r2:
                    break
                extra = os.read(fd, 512)
                if not extra:
                    break
                chunks.append(extra)
            break
    return b"".join(chunks)

def decode_ascii(blob: bytes) -> str:
    return blob.decode("ascii", errors="replace").replace("\r", "\\r").replace("\n", "\\n")

def print_result(name: str, body: str, blob: bytes) -> None:
    print(f"\n[{name}] request")
    print(f"  body:   {body}")
    print(f"  frame:  {frame_for(body).decode('ascii').rstrip()}")
    if blob:
        print(f"  bytes:  {blob.hex(' ')}")
        print(f"  ascii:  {decode_ascii(blob)}")
    else:
        print("  bytes:  <no response>")

def assess(results):
    ok_v = bool(results.get("AT+V"))
    ok_s = bool(results.get("AT+S"))
    ok_m = bool(results.get("AT+M"))

    print("\nSummary")
    print(f"  config: {config_path}")
    print(f"  uart:   {driver_port} @ {driver_baud}")
    print(f"  V:      {'PASS' if ok_v else 'FAIL'}")
    print(f"  S:      {'PASS' if ok_s else 'FAIL'}")
    print(f"  M:      {'PASS' if ok_m else 'FAIL'}")

    if ok_v and ok_s and ok_m:
        print("\nResult: RM18 UART protocol responds on all core commands.")
        return 0
    if ok_v and (not ok_s or not ok_m):
        print("\nResult: Basic UART works, but some operational commands do not respond.")
        return 2
    print("\nResult: No reliable RM18 communication on the configured UART path.")
    return 3

print("== RM18 UART Check ==")
print(f"Repo:    {root_dir}")
print(f"Config:  {config_path}")
print(f"UART:    {driver_port}")
print(f"Baud:    {driver_baud}")

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

    requests = [
        ("AT+V", "AT+V"),
        ("AT+S", "AT+S"),
        ("AT+M", "AT+M,0,0,0"),
    ]
    results = {}
    for name, body in requests:
        os.write(fd, frame_for(body))
        time.sleep(0.1)
        blob = read_response(fd, timeout_s=1.2)
        results[name] = blob
        print_result(name, body, blob)

    sys.exit(assess(results))
finally:
    os.close(fd)
PY

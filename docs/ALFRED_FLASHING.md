# STM32F103 Flashing über Raspberry Pi

**Kritisch für Phase 2:** Ohne diese Fähigkeit kann die Firmware nicht auf die Hardware übertragen werden.

---

## Hardware-Verbindung (SWD — Serial Wire Debug)

Alfred-Stecker (STM32F103) ↔ Raspberry Pi GPIO

| Alfred-Pin | Signal | Pi GPIO | Pi-Pinnummer |
|------------|--------|---------|--------------|
| P18 | SWDIO | GPIO 24 | Pin 18 |
| P22 | SWCLK | GPIO 25 | Pin 22 |
| P16 | SRST (Reset) | GPIO 23 | Pin 16 |
| GND | GND | GND | Pin 6/9/14/20/25/30 |

**Verkabelung:**
```
         Raspberry Pi             Alfred (STM32F103)
         Pin 18 (SWDIO) ────────→ P18 (SWDIO)
         Pin 22 (SWCLK) ────────→ P22 (SWCLK)
         Pin 16 (SRST)  ────────→ P16 (Reset)
         Pin 6 (GND)    ────────→ GND
```

> ⚠️ **Wichtig:** Zuerst **GND verbinden**, dann die Signale. Im laufenden Betrieb nicht verbinden/trennen (ESD-Risiko).

---

## Vorbereitung auf dem Pi

### 1. OpenOCD installieren
```bash
sudo apt-get update
sudo apt-get install openocd
```

### 2. Arduino-CLI installieren (für STM32-Kompilierung)
```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
sudo mv ~/bin/arduino-cli /usr/local/bin/
```

### 3. STM32-Board-Support hinzufügen
```bash
arduino-cli config add board_manager.additional_urls https://github.com/stm32duino/BoardManagerFiles/raw/main/STM32/package_stm_index.json
arduino-cli core install STMicroelectronics:stm32
```

---

## Flashing-Prozess

### Option A: Komplettes Build + Flash (recommended)

```bash
cd e:/TRAE/sunray-core
mkdir -p build_gcc && cd build_gcc
cmake .. -DCMAKE_BUILD_TYPE=Release
make sunray-core

# Binärdatei von C++ → STM32 firmware location kopieren
# (zukünftig: Cmake-Target für flash.sh)

# SSH zum Pi
ssh pi@raspberrypi.local
cd ~/sunray-core

# Flash-Script ausführen (angepasste Version von Sunray/alfred/flash.sh)
sudo bash scripts/flash_alfred.sh build-flash
```

### Option B: Nur Probe (Verbindung testen)
```bash
sudo bash scripts/flash_alfred.sh probe
# Output: "Probe: OK" wenn SWD-Verbindung aktiv
```

### Option C: Nur Flash (Binary existiert bereits)
```bash
sudo bash scripts/flash_alfred.sh flash
```

---

## Flash-Script Vorlage

Basierend auf `e:/TRAE/Sunray/alfred/flash.sh`, angepasst für sunray-core:

**`scripts/flash_alfred.sh`** (zu erstellen)

```bash
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# OpenOCD & SWD-Konfiguration
OPENOCD_BIN="${OPENOCD_BIN:-openocd}"
OPENOCD_CFG="$REPO_ROOT/docs/swd-pi.ocd"

# Binary-Pfad (aus CMake build)
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build_gcc}"
BIN_PATH="${BIN_PATH:-$BUILD_DIR/sunray-core.bin}"

probe_target() {
  echo "SWD-Probe: STM32 Verbindung testen..."
  $OPENOCD_BIN -f "$OPENOCD_CFG" \
    -c "init; targets; reset halt; exit"
  echo "SWD-Probe: OK ✓"
}

flash_bin() {
  if [ ! -f "$BIN_PATH" ]; then
    echo "ERROR: Binary nicht gefunden: $BIN_PATH"
    exit 1
  fi
  echo "Flashing: $BIN_PATH → STM32F103 @ 0x08000000"
  $OPENOCD_BIN -f "$OPENOCD_CFG" \
    -c "init; targets; reset halt; program $BIN_PATH 0x08000000 verify reset exit"
  echo "Flashing: OK ✓"
}

main() {
  local cmd="${1:-flash}"
  case "$cmd" in
    probe) probe_target ;;
    flash)
      [ "$(id -u)" = 0 ] || { echo "Muss mit sudo ausgeführt werden"; exit 1; }
      probe_target
      flash_bin
      ;;
    *)
      echo "Usage: sudo bash $0 [probe|flash]"
      exit 1
      ;;
  esac
}

main "$@"
```

**`docs/swd-pi.ocd`** (Copy aus Sunray/alfred/config_files/openocd/swd-pi.ocd)

---

## Troubleshooting

| Problem | Lösung |
|---------|--------|
| `openocd: command not found` | `sudo apt-get install openocd` |
| `Probe: failed` | GPIO-Pins prüfen, Verkabelung überprüfen, Pi reboot |
| `Permission denied /dev/mem` | `sudo` verwenden oder `udev`-Regel setzen |
| `stm32f1x.cfg not found` | OpenOCD-Skripte in `/usr/share/openocd/scripts` prüfen |
| Binary zu groß | STM32F103 hat 256 kB Flash; Check `map` nach Build |

### udev-Regel für GPIO-Zugriff ohne sudo (optional)
```bash
# /etc/udev/rules.d/99-openocd.rules
SUBSYSTEM=="gpio", MODE="0666"
SUBSYSTEM=="spidev", MODE="0666"
```

---

## Integration in CMakeLists.txt (Phase 2 Todo)

```cmake
add_custom_target(flash
  COMMAND sudo bash ${CMAKE_SOURCE_DIR}/scripts/flash_alfred.sh flash
  DEPENDS sunray-core
  COMMENT "Flashing STM32F103 via SWD"
)

add_custom_target(flash_probe
  COMMAND sudo bash ${CMAKE_SOURCE_DIR}/scripts/flash_alfred.sh probe
  COMMENT "Testing SWD connection"
)
```

Danach: `make flash` direkt im Build-Dir aufrufen.

---

## Referenzen

- OpenOCD SWD Docs: http://openocd.org/doc-release/html/Debug-Adapter-Configuration.html
- STM32F103 Reference: https://www.st.com/resource/en/datasheet/stm32f103c8.pdf
- Original Sunray Implementation: `e:/TRAE/Sunray/alfred/flash.sh`

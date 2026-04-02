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

`sunray-core` nutzt auf Alfred (Raspberry Pi 4) den nativen OpenOCD-Adapter
`bcm2835gpio`. Das ältere `sysfsgpio`-Backend wird bewusst nicht mehr verwendet,
weil es auf aktuellen Pi-OS-Ständen bereits vor dem eigentlichen SWD-Probe mit
GPIO-Exportfehlern scheitern kann.

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

> ⚠️ **Wichtig:** Das Flash-Script kompiliert die **Alfred-STM32-Firmware** (`rm18.ino`) —
> **nicht** das Pi-Binary `sunray-core`. Beide sind getrennte Binaries für getrennte CPUs.

### Option A: Komplett-Workflow (Build + Flash, empfohlen)

```bash
# SSH zum Pi
ssh pi@raspberrypi.local
cd ~/sunray-core

# Alfred-Sketch bereitstellen (einmalig, falls noch nicht vorhanden)
mkdir -p ~/sunray_install/alfred/firmware
cp /pfad/zu/alfred/firmware/rm18.ino ~/sunray_install/alfred/firmware/rm18.ino

# FQBN für STM32F103 setzen (einmalig in ~/.bashrc eintragen)
export FQBN="STMicroelectronics:stm32:GenF1:pnum=GENERIC_F103VE"

# Kompilieren + Flashen
sudo bash scripts/flash_alfred.sh build-flash
```

### Option B: Nur Probe (SWD-Verbindung testen)
```bash
sudo bash scripts/flash_alfred.sh probe
# Output: "✓ SWD connection OK" wenn Verkabelung stimmt
```

### Bewiesener Stand auf Alfred

- `FACT`: Der SWD-Probe ist auf Alfred jetzt praktisch bestätigt.
- `FACT`: OpenOCD erreicht den STM32 ueber Pi-GPIO-SWD und liefert:
  - `Info : SWD DPIDR 0x1ba01477`
  - `Info : [stm32f1x.cpu] Cortex-M3 r1p1 processor detected`
  - `Info : [stm32f1x.cpu] Examination succeed`
  - `halted due to debug-request`
  - `✓ SWD connection OK`
- `FACT`: Damit sind `openocd`, die Pi-4-OpenOCD-Konfiguration (`bcm2835gpio`), die SWD-Verkabelung und der grundsaetzliche Flash-Zugriff auf Alfreds STM32 belegt.
- `UNKNOWN`: Ein kompletter Flash aus der WebUI ist damit noch nicht bewiesen; bislang ist nur der Probe-Pfad hart bestaetigt.

### Option C: Nur Flash (Binary bereits vorhanden)
```bash
sudo bash scripts/flash_alfred.sh flash
```

> Empfohlener naechster Schritt fuer WebUI-OTA:
> zuerst Upload einer bereits am PC erzeugten `rm18.ino.bin`, danach manuelles Flashen
> auf Alfred. Das reduziert den Einfuehrungsumfang deutlich gegenueber einem sofortigen
> kompletten On-Pi-Build per `arduino-cli`.

### Option D: Nur Kompilieren (kein Flash)
```bash
export FQBN="STMicroelectronics:stm32:GenF1:pnum=GENERIC_F103VE"
bash scripts/flash_alfred.sh build
# → ~/sunray_install/firmware/rm18.ino.bin
```

---

## Script- und Konfigurations-Dateien

Beide Dateien sind bereits im Repository vorhanden:

| Datei | Inhalt |
|-------|--------|
| `scripts/flash_alfred.sh` | Vollständiges Build+Flash-Script (basierend auf `e:/TRAE/Sunray/alfred/flash.sh`) |
| `docs/swd-pi.ocd` | OpenOCD SWD-Konfiguration für Raspberry Pi GPIO |

Das Script kompiliert `rm18.ino` (Alfred-STM32-Firmware) via `arduino-cli` und flasht das
resultierende `rm18.ino.bin` via SWD. Wichtige Umgebungsvariablen:

```bash
FQBN="STMicroelectronics:stm32:GenF1:pnum=GENERIC_F103VE"  # Pflicht für build
ALFRED_SKETCH=/path/to/rm18.ino   # default: ~/sunray_install/alfred/firmware/rm18.ino
BIN_PATH=/path/to/rm18.ino.bin    # default: ~/sunray_install/firmware/rm18.ino.bin
```

---

## Troubleshooting

| Problem | Lösung |
|---------|--------|
| `openocd: command not found` | `sudo apt-get install openocd` |
| `Couldn't export gpio 25` / `sysfsgpio: Invalid argument` | veraltete `sysfsgpio`-Konfiguration; `docs/swd-pi.ocd` auf `bcm2835gpio`-Variante aktualisieren und erneut probieren |
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

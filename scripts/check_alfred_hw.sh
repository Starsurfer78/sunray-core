#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_PATH="${CONFIG_PATH:-/etc/sunray/config.json}"
DEFAULT_DRIVER_PORT="/dev/ttyS0"
DEFAULT_I2C_BUS="/dev/i2c-1"
DRIVER_PORT="${DRIVER_PORT:-$DEFAULT_DRIVER_PORT}"
I2C_BUS="${I2C_BUS:-$DEFAULT_I2C_BUS}"

PASS_COUNT=0
WARN_COUNT=0
FAIL_COUNT=0

PASS_LINES=()
WARN_LINES=()
FAIL_LINES=()

trim() {
  local s="$1"
  s="${s#"${s%%[![:space:]]*}"}"
  s="${s%"${s##*[![:space:]]}"}"
  printf '%s' "$s"
}

note_pass() {
  PASS_LINES+=("$1")
  PASS_COUNT=$((PASS_COUNT + 1))
}

note_warn() {
  WARN_LINES+=("$1")
  WARN_COUNT=$((WARN_COUNT + 1))
}

note_fail() {
  FAIL_LINES+=("$1")
  FAIL_COUNT=$((FAIL_COUNT + 1))
}

have_cmd() {
  command -v "$1" >/dev/null 2>&1
}

read_config_string() {
  local key="$1"
  if [[ ! -f "$CONFIG_PATH" ]]; then
    return 1
  fi
  if ! have_cmd python3; then
    return 1
  fi
  python3 - "$CONFIG_PATH" "$key" <<'PY'
import json
import sys

path, key = sys.argv[1], sys.argv[2]
try:
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    value = data.get(key, "")
    if isinstance(value, str):
        print(value)
except Exception:
    pass
PY
}

print_section() {
  printf '\n== %s ==\n' "$1"
}

print_kv() {
  printf '%-24s %s\n' "$1" "$2"
}

check_device_node() {
  local path="$1"
  local label="$2"

  if [[ -e "$path" ]]; then
    note_pass "$label vorhanden: $path"
  else
    note_fail "$label fehlt: $path"
    return
  fi

  if [[ -r "$path" && -w "$path" ]]; then
    note_pass "$label lesbar/schreibbar"
  else
    note_warn "$label vorhanden, aber nicht voll lesbar/schreibbar fuer den aktuellen Benutzer"
  fi
}

check_groups() {
  local groups_str
  groups_str="$(id -nG 2>/dev/null || true)"
  print_kv "Gruppen" "$groups_str"

  if grep -qw "dialout" <<<"$groups_str"; then
    note_pass "Benutzer ist in dialout"
  else
    note_warn "Benutzer ist nicht in dialout"
  fi

  if grep -Eqw "i2c|gpio" <<<"$groups_str"; then
    note_pass "Benutzer ist in i2c/gpio"
  else
    note_warn "Benutzer ist weder in i2c noch gpio"
  fi
}

check_i2c_scan() {
  local bus_num="${I2C_BUS##*/dev/i2c-}"
  local scan

  if ! have_cmd i2cdetect; then
    note_warn "i2cdetect nicht installiert; I2C-Bestandsaufnahme uebersprungen"
    return
  fi

  if [[ ! "$bus_num" =~ ^[0-9]+$ ]]; then
    note_warn "I2C-Busname nicht numerisch interpretierbar: $I2C_BUS"
    return
  fi

  if ! scan="$(i2cdetect -y "$bus_num" 2>&1)"; then
    note_warn "i2cdetect auf $I2C_BUS fehlgeschlagen: $(trim "$scan")"
    return
  fi

  print_section "I2C Scan ($I2C_BUS)"
  printf '%s\n' "$scan"

  local map=(
    "20:PCA9555 EX2 (Buzzer)"
    "21:PCA9555 EX1 (IMU/Fan)"
    "22:PCA9555 EX3 (LEDs)"
    "50:EEPROM"
    "68:ADC oder IMU"
    "70:TCA9548A Mux"
  )
  local entry addr label
  for entry in "${map[@]}"; do
    addr="${entry%%:*}"
    label="${entry#*:}"
    if grep -Eq "(^|[[:space:]])${addr}($|[[:space:]])" <<<"$scan"; then
      note_pass "I2C-Adresse 0x${addr} sichtbar: ${label}"
    else
      note_warn "I2C-Adresse 0x${addr} nicht sichtbar: ${label}"
    fi
  done
}

check_repo_expectations() {
  if [[ -f "$ROOT_DIR/hal/SerialRobotDriver/SerialRobotDriver.cpp" ]]; then
    note_pass "SerialRobotDriver im Repo vorhanden"
  else
    note_fail "SerialRobotDriver im Repo nicht gefunden"
  fi

  if [[ -f "$ROOT_DIR/docs/ALFRED_HARDWARE_ACCEPTANCE.md" ]]; then
    note_pass "Hardware-Abnahmedoku vorhanden"
  else
    note_warn "Hardware-Abnahmedoku fehlt"
  fi
}

main() {
  print_section "Alfred Hardware Check"
  print_kv "Repo" "$ROOT_DIR"
  print_kv "Config" "$CONFIG_PATH"

  if [[ -f "$CONFIG_PATH" ]]; then
    local cfg_driver_port cfg_i2c_bus
    cfg_driver_port="$(trim "$(read_config_string driver_port || true)")"
    cfg_i2c_bus="$(trim "$(read_config_string i2c_bus || true)")"
    if [[ -n "$cfg_driver_port" ]]; then
      DRIVER_PORT="$cfg_driver_port"
    fi
    if [[ -n "$cfg_i2c_bus" ]]; then
      I2C_BUS="$cfg_i2c_bus"
    fi
    note_pass "Konfigurationsdatei gefunden"
  else
    note_warn "Konfigurationsdatei nicht gefunden; verwende Default-Pfade"
  fi

  print_kv "UART" "$DRIVER_PORT"
  print_kv "I2C Bus" "$I2C_BUS"
  print_kv "Benutzer" "$(id -un)"

  check_repo_expectations

  print_section "Rechte und Device-Nodes"
  check_groups
  check_device_node "$DRIVER_PORT" "UART"
  check_device_node "$I2C_BUS" "I2C-Bus"

  print_section "Runtime-Hinweise"
  if have_cmd systemctl; then
    if systemctl is-active sunray >/dev/null 2>&1; then
      note_pass "systemd-Dienst sunray ist aktiv"
    else
      note_warn "systemd-Dienst sunray ist nicht aktiv"
    fi
  else
    note_warn "systemctl nicht verfuegbar"
  fi

  check_i2c_scan

  print_section "Zusammenfassung"
  printf 'PASS: %d\n' "$PASS_COUNT"
  printf 'WARN: %d\n' "$WARN_COUNT"
  printf 'FAIL: %d\n' "$FAIL_COUNT"

  if ((${#PASS_LINES[@]} > 0)); then
    printf '\nPASS:\n'
    for line in "${PASS_LINES[@]}"; do
      printf '  - %s\n' "$line"
    done
  fi

  if ((${#WARN_LINES[@]} > 0)); then
    printf '\nWARN:\n'
    for line in "${WARN_LINES[@]}"; do
      printf '  - %s\n' "$line"
    done
  fi

  if ((${#FAIL_LINES[@]} > 0)); then
    printf '\nFAIL:\n'
    for line in "${FAIL_LINES[@]}"; do
      printf '  - %s\n' "$line"
    done
  fi

  printf '\nNaechste Schritte:\n'
  printf '  1. sunray-core auf dem Pi starten und Button/LED/Buzzer/Fan/IMU manuell pruefen.\n'
  printf '  2. Ergebnisse in docs/ALFRED_HARDWARE_ACCEPTANCE.md uebernehmen.\n'
  printf '  3. Bei fehlenden 0x70/0x50/0x68-Adressen Board-Revision und Verdrahtung pruefen.\n'

  if ((FAIL_COUNT > 0)); then
    exit 2
  fi
  exit 0
}

main "$@"

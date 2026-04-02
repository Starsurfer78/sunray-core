#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_LINUX_DIR="${BUILD_LINUX_DIR:-$ROOT_DIR/build_linux}"
BUILD_PI_DIR="${BUILD_PI_DIR:-$ROOT_DIR/build_pi}"
CONFIG_PATH="${CONFIG_PATH:-/etc/sunray-core/config.json}"
MAP_PATH="${MAP_PATH:-/etc/sunray-core/map.json}"
SERVICE_PATH="${SERVICE_PATH:-/etc/systemd/system/sunray-core.service}"
WEBUI_DIST_DIR="${WEBUI_DIST_DIR:-$ROOT_DIR/webui-svelte/dist}"

PASS_COUNT=0
WARN_COUNT=0
FAIL_COUNT=0

PASS_LINES=()
WARN_LINES=()
FAIL_LINES=()

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

print_section() {
  printf '\n== %s ==\n' "$1"
}

print_kv() {
  printf '%-24s %s\n' "$1" "$2"
}

check_binary() {
  local path="$1"
  local label="$2"
  if [[ -x "$path" ]]; then
    note_pass "$label vorhanden und ausfuehrbar: $path"
  elif [[ -e "$path" ]]; then
    note_warn "$label vorhanden, aber nicht ausfuehrbar: $path"
  else
    note_warn "$label fehlt: $path"
  fi
}

check_file() {
  local path="$1"
  local label="$2"
  if [[ -f "$path" ]]; then
    note_pass "$label vorhanden: $path"
  else
    note_warn "$label fehlt: $path"
  fi
}

check_dir() {
  local path="$1"
  local label="$2"
  if [[ -d "$path" ]]; then
    note_pass "$label vorhanden: $path"
  else
    note_warn "$label fehlt: $path"
  fi
}

check_service_file() {
  if [[ ! -f "$SERVICE_PATH" ]]; then
    note_warn "sunray-core systemd Unit fehlt: $SERVICE_PATH"
    return
  fi

  note_pass "sunray-core systemd Unit vorhanden: $SERVICE_PATH"

  if grep -q "ExecStart=.*sunray-core" "$SERVICE_PATH"; then
    note_pass "sunray-core Unit verweist auf sunray-core Binary"
  else
    note_fail "sunray-core Unit enthaelt keinen belegten ExecStart auf sunray-core"
  fi

  if grep -q "^Restart=on-failure" "$SERVICE_PATH"; then
    note_pass "sunray-core Unit nutzt Restart=on-failure"
  else
    note_warn "sunray-core Unit nutzt keinen belegten Restart=on-failure Pfad"
  fi
}

check_systemd_state() {
  if ! have_cmd systemctl; then
    note_warn "systemctl nicht verfuegbar; Service-Status kann nicht geprueft werden"
    return
  fi

  if systemctl cat sunray-core >/dev/null 2>&1; then
    note_pass "systemd kennt sunray-core"
    if systemctl is-enabled sunray-core >/dev/null 2>&1; then
      note_pass "sunray-core ist aktiviert"
    else
      note_warn "sunray-core ist nicht aktiviert"
    fi

    if systemctl is-active sunray-core >/dev/null 2>&1; then
      note_pass "sunray-core ist aktiv"
    else
      note_warn "sunray-core ist nicht aktiv"
    fi
  else
    note_warn "systemd kennt keine sunray-core Unit"
  fi

  if systemctl cat sunray >/dev/null 2>&1; then
    note_pass "Rollback-Anker sunray ist als systemd Unit sichtbar"
    if systemctl is-enabled sunray >/dev/null 2>&1; then
      note_pass "Rollback-Anker sunray ist aktiviert"
    else
      note_warn "Rollback-Anker sunray ist vorhanden, aber nicht aktiviert"
    fi
  else
    note_warn "Rollback-Anker sunray ist nicht als systemd Unit sichtbar"
  fi
}

print_list() {
  local title="$1"
  shift
  local items=("$@")
  if ((${#items[@]} == 0)); then
    return
  fi
  printf '\n%s:\n' "$title"
  local item
  for item in "${items[@]}"; do
    printf '  - %s\n' "$item"
  done
}

main() {
  print_section "sunray-core Deploy State Check"
  print_kv "Repo" "$ROOT_DIR"
  print_kv "build_linux" "$BUILD_LINUX_DIR"
  print_kv "build_pi" "$BUILD_PI_DIR"
  print_kv "Config" "$CONFIG_PATH"
  print_kv "Map" "$MAP_PATH"
  print_kv "Service" "$SERVICE_PATH"

  print_section "Build Artefacts"
  check_binary "$BUILD_LINUX_DIR/sunray-core" "build_linux Binary"
  check_binary "$BUILD_PI_DIR/sunray-core" "build_pi Binary"
  check_dir "$WEBUI_DIST_DIR" "WebUI dist"

  print_section "Runtime Files"
  check_file "$CONFIG_PATH" "Runtime config"
  check_file "$MAP_PATH" "Runtime map"

  print_section "Service State"
  check_service_file
  check_systemd_state

  print_section "Rollback Reminder"
  printf 'Manual rollback path from repo docs:\n'
  printf '  1. sudo systemctl stop sunray-core\n'
  printf '  2. sudo systemctl start sunray\n'
  printf '  3. journalctl -u sunray -n 100 --no-pager\n'
  printf '  4. journalctl -u sunray-core -n 100 --no-pager\n'

  print_section "Summary"
  printf 'PASS: %d\n' "$PASS_COUNT"
  printf 'WARN: %d\n' "$WARN_COUNT"
  printf 'FAIL: %d\n' "$FAIL_COUNT"

  print_list "PASS" "${PASS_LINES[@]}"
  print_list "WARN" "${WARN_LINES[@]}"
  print_list "FAIL" "${FAIL_LINES[@]}"

  if ((FAIL_COUNT > 0)); then
    exit 2
  fi
}

main "$@"

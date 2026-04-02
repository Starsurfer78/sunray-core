#!/usr/bin/env bash
# Flash STM32F103 on Alfred via Raspberry Pi SWD interface.
# Compiles rm18.ino (Alfred firmware) with arduino-cli, then flashes via OpenOCD.
#
# Usage:
#   sudo bash scripts/flash_alfred.sh probe        # Test SWD connection only
#   sudo bash scripts/flash_alfred.sh build        # Compile Alfred firmware only
#   sudo bash scripts/flash_alfred.sh flash        # Flash pre-built binary
#   sudo bash scripts/flash_alfred.sh build-flash  # Compile + Flash (default)
#
# Required env for build:
#   FQBN              Board FQBN, e.g. STMicroelectronics:stm32:GenF1:pnum=GENERIC_F103VETX
#   ALFRED_SKETCH     Path to rm18.ino (default: ~/sunray_install/alfred/firmware/rm18.ino)

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# OpenOCD configuration
OPENOCD_BIN="${OPENOCD_BIN:-openocd}"
OPENOCD_CFG="${OPENOCD_CFG:-$REPO_ROOT/docs/swd-pi.ocd}"

# Arduino CLI
ARDUINO_CLI_BIN="${ARDUINO_CLI_BIN:-arduino-cli}"
FQBN="${FQBN:-}"

# Alfred firmware paths
SUDO_USER_NAME="${SUDO_USER:-${USER:-pi}}"
SUNRAY_INSTALL_DIR="${SUNRAY_INSTALL_DIR:-/home/${SUDO_USER_NAME}/sunray_install}"
ALFRED_SKETCH="${ALFRED_SKETCH:-$SUNRAY_INSTALL_DIR/alfred/firmware/rm18.ino}"
BUILD_DIR="${BUILD_DIR:-$SUNRAY_INSTALL_DIR/build_rm18}"
BIN_PATH="${BIN_PATH:-$SUNRAY_INSTALL_DIR/firmware/rm18.ino.bin}"

# ── Helpers ───────────────────────────────────────────────────────────────────

ensure_root() {
  if [ "${EUID:-0}" -ne 0 ]; then
    echo "ERROR: Must run as root (sudo)"
    exit 1
  fi
}

openocd_cmd() {
  local cmd=("$OPENOCD_BIN")
  for dir in /usr/local/share/openocd/scripts /usr/share/openocd/scripts; do
    [ -d "$dir" ] && cmd+=("-s" "$dir")
  done
  cmd+=("$@")
  "${cmd[@]}"
}

# ── Commands ──────────────────────────────────────────────────────────────────

probe_target() {
  echo "=== SWD Probe ==="
  echo "Testing SWD connection to STM32F103..."
  if openocd_cmd -f "$OPENOCD_CFG" -c "init; targets; reset halt; reset run; shutdown"; then
    echo "✓ SWD connection OK"
  else
    echo "✗ SWD connection FAILED"
    echo "  Check wiring: docs/ALFRED_FLASHING.md"
    return 1
  fi
}

compile_rm18() {
  if ! command -v "$ARDUINO_CLI_BIN" >/dev/null 2>&1; then
    echo "ERROR: arduino-cli not found. Install with:"
    echo "  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    exit 1
  fi
  if [ -z "$FQBN" ]; then
    echo "ERROR: FQBN not set. Example:"
    echo "  export FQBN='STMicroelectronics:stm32:GenF1:pnum=GENERIC_F103VETX'"
    exit 1
  fi
  if [ ! -f "$ALFRED_SKETCH" ]; then
    echo "ERROR: Alfred sketch not found: $ALFRED_SKETCH"
    echo "  Set ALFRED_SKETCH=<path to rm18.ino>"
    exit 1
  fi

  local run_as=()
  if [ "${EUID:-0}" -eq 0 ] && [ -n "${SUDO_USER:-}" ] && [ "${SUDO_USER:-}" != "root" ]; then
    run_as=(sudo -u "$SUDO_USER_NAME" -H)
  fi

  local tmp_root=""
  cleanup_tmp_root() {
    local tmp="${tmp_root:-}"
    if [ -n "$tmp" ] && [ -d "$tmp" ]; then
      rm -rf "$tmp"
    fi
  }
  trap cleanup_tmp_root RETURN

  local firmware_dir
  firmware_dir="$(dirname "$BIN_PATH")"
  "${run_as[@]}" mkdir -p "$firmware_dir" "$BUILD_DIR"

  # arduino-cli requires the sketch in a folder matching the .ino name
  tmp_root="$("${run_as[@]}" mktemp -d)"
  "${run_as[@]}" mkdir -p "$tmp_root/rm18"
  "${run_as[@]}" cp "$ALFRED_SKETCH" "$tmp_root/rm18/rm18.ino"

  echo "=== Compiling Alfred firmware ==="
  echo "Sketch: $ALFRED_SKETCH"
  echo "FQBN:   $FQBN"
  "${run_as[@]}" "$ARDUINO_CLI_BIN" compile \
    --fqbn "$FQBN" \
    --output-dir "$BUILD_DIR" \
    "$tmp_root/rm18"

  local produced_bin
  produced_bin="$("${run_as[@]}" find "$BUILD_DIR" -maxdepth 1 -type f -name 'rm18.ino.bin' | head -n 1)"
  if [ -z "$produced_bin" ]; then
    produced_bin="$("${run_as[@]}" find "$BUILD_DIR" -maxdepth 1 -type f -name '*.bin' | sort | head -n 1)"
  fi
  if [ -z "$produced_bin" ]; then
    echo "ERROR: No .bin produced in $BUILD_DIR"
    exit 1
  fi

  "${run_as[@]}" cp -f "$produced_bin" "$BIN_PATH"
  echo "✓ Compiled → $BIN_PATH"
}

flash_bin() {
  if [ ! -f "$BIN_PATH" ]; then
    echo "ERROR: Binary not found: $BIN_PATH"
    echo "  Run: sudo bash scripts/flash_alfred.sh build-flash"
    exit 1
  fi

  echo "=== Flashing Alfred firmware ==="
  echo "Binary: $BIN_PATH"
  echo "Target: STM32F103 @ 0x08000000"

  local program_cmd
  program_cmd="init; targets; reset halt; program {$BIN_PATH} 0x08000000 verify reset exit"
  if openocd_cmd -f "$OPENOCD_CFG" \
    -c "$program_cmd"; then
    echo "✓ Flash successful"
  else
    echo "✗ Flash failed"
    return 1
  fi
}

usage() {
  echo "Usage: sudo bash scripts/flash_alfred.sh [probe|build|flash|build-flash]"
  echo ""
  echo "Commands:"
  echo "  probe        Test SWD connection to STM32"
  echo "  build        Compile rm18.ino → rm18.ino.bin (no sudo needed)"
  echo "  flash        Flash pre-built binary (requires sudo)"
  echo "  build-flash  Compile + Flash (default, requires sudo)"
  echo ""
  echo "Required env for build:"
  echo "  FQBN             Board FQBN, e.g. STMicroelectronics:stm32:GenF1:pnum=GENERIC_F103VETX"
  echo ""
  echo "Optional env:"
  echo "  ALFRED_SKETCH    Path to rm18.ino    (default: ~/sunray_install/alfred/firmware/rm18.ino)"
  echo "  BIN_PATH         Output binary path  (default: ~/sunray_install/firmware/rm18.ino.bin)"
  echo "  BUILD_DIR        arduino-cli build   (default: ~/sunray_install/build_rm18)"
  echo "  OPENOCD_BIN      openocd executable  (default: openocd)"
  echo "  OPENOCD_CFG      OpenOCD config      (default: docs/swd-pi.ocd)"
}

# ── Main ──────────────────────────────────────────────────────────────────────

main() {
  local cmd="${1:-build-flash}"
  case "$cmd" in
    probe)
      ensure_root
      probe_target
      ;;
    build)
      compile_rm18
      ;;
    flash)
      ensure_root
      probe_target || exit 1
      flash_bin
      ;;
    build-flash)
      compile_rm18
      ensure_root
      probe_target || exit 1
      flash_bin
      ;;
    -h|--help|help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown command: $cmd"
      usage
      exit 1
      ;;
  esac
}

main "$@"

#!/usr/bin/env bash
# Flash STM32F103 on Alfred via Raspberry Pi SWD interface
# Based on: e:/TRAE/Sunray/alfred/flash.sh
#
# Usage:
#   sudo bash scripts/flash_alfred.sh probe  # Test SWD connection
#   sudo bash scripts/flash_alfred.sh flash  # Flash binary to STM32

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# OpenOCD configuration
OPENOCD_BIN="${OPENOCD_BIN:-openocd}"
OPENOCD_CFG="$REPO_ROOT/docs/swd-pi.ocd"

# Build output
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build_gcc}"
BIN_PATH="${BIN_PATH:-$BUILD_DIR/sunray-core.bin}"

# === Helper Functions ===

ensure_root() {
  if [ "${EUID:-0}" -ne 0 ]; then
    echo "ERROR: Must run as root (sudo)"
    exit 1
  fi
}

openocd_cmd() {
  local cmd=("$OPENOCD_BIN")
  # Add OpenOCD script search paths
  for dir in /usr/local/share/openocd/scripts /usr/share/openocd/scripts; do
    if [ -d "$dir" ]; then
      cmd+=("-s" "$dir")
    fi
  done
  cmd+=("$@")
  "${cmd[@]}"
}

probe_target() {
  echo "=== SWD Probe ==="
  echo "Testing SWD connection to STM32F103..."
  if openocd_cmd -f "$OPENOCD_CFG" \
    -c "init; targets; reset halt; exit"; then
    echo "✓ SWD connection OK"
    return 0
  else
    echo "✗ SWD connection FAILED"
    echo "  - Check GPIO pin wiring (docs/ALFRED_FLASHING.md)"
    echo "  - Check OpenOCD installation"
    return 1
  fi
}

flash_bin() {
  if [ ! -f "$BIN_PATH" ]; then
    echo "ERROR: Binary not found: $BIN_PATH"
    echo "  Expected: $BUILD_DIR/sunray-core.bin"
    echo "  Run: cd $BUILD_DIR && cmake .. && make"
    exit 1
  fi

  echo "=== Flashing ==="
  echo "Binary: $BIN_PATH"
  echo "Target: STM32F103 @ 0x08000000"

  if openocd_cmd -f "$OPENOCD_CFG" \
    -c "init; targets; reset halt; program $BIN_PATH 0x08000000 verify reset exit"; then
    echo "✓ Flash successful"
    return 0
  else
    echo "✗ Flash failed"
    return 1
  fi
}

usage() {
  echo "Usage:"
  echo "  sudo bash scripts/flash_alfred.sh probe     # Test SWD connection"
  echo "  sudo bash scripts/flash_alfred.sh flash     # Flash binary to STM32"
  echo ""
  echo "Environment variables:"
  echo "  OPENOCD_BIN     OpenOCD executable (default: openocd)"
  echo "  OPENOCD_CFG     Config file (default: docs/swd-pi.ocd)"
  echo "  BUILD_DIR       CMake build directory (default: build_gcc)"
  echo "  BIN_PATH        Binary path (default: build_gcc/sunray-core.bin)"
  echo ""
  echo "Example:"
  echo "  cd build_gcc && cmake .. && make"
  echo "  cd .. && sudo bash scripts/flash_alfred.sh flash"
}

# === Main ===

main() {
  local cmd="${1:-flash}"

  case "$cmd" in
    probe)
      ensure_root
      probe_target
      ;;
    flash)
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

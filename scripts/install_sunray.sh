#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="build_linux"
INSTALL_DEPS="yes"
START_AFTER_INSTALL="yes"
AUTOSTART_MODE="ask"
SIM_MODE="no"
CONFIG_PATH="/etc/sunray-core/config.json"
MAP_PATH="/etc/sunray-core/map.json"
SERVICE_NAME="sunray-core"
WEBUI_DIR="webui-svelte"

if [[ "${EUID}" -eq 0 && -n "${SUDO_USER:-}" ]]; then
  BUILD_USER="${SUDO_USER}"
else
  BUILD_USER="$(id -un)"
fi
BUILD_GROUP="$(id -gn "${BUILD_USER}")"

log() {
  printf '[sunray-install] %s\n' "$*"
}

fail() {
  printf '[sunray-install] ERROR: %s\n' "$*" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Usage: scripts/install_sunray.sh [options]

Options:
  --sim                  Build and start in simulation mode
  --config PATH          Config path for hardware mode (default: /etc/sunray-core/config.json)
  --build-dir DIR        Build directory (default: build_linux)
  --skip-deps            Skip apt dependency installation
  --no-start             Build/install only, do not start afterward
  --autostart yes|no     Enable or disable systemd autostart without prompting
  -h, --help             Show this help

Examples:
  ./scripts/install_sunray.sh
  ./scripts/install_sunray.sh --sim
  ./scripts/install_sunray.sh --autostart yes
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sim)
      SIM_MODE="yes"
      shift
      ;;
    --config)
      [[ $# -ge 2 ]] || fail "--config requires a path"
      CONFIG_PATH="$2"
      shift 2
      ;;
    --build-dir)
      [[ $# -ge 2 ]] || fail "--build-dir requires a directory"
      BUILD_DIR="$2"
      shift 2
      ;;
    --skip-deps)
      INSTALL_DEPS="no"
      shift
      ;;
    --no-start)
      START_AFTER_INSTALL="no"
      shift
      ;;
    --autostart)
      [[ $# -ge 2 ]] || fail "--autostart requires yes or no"
      AUTOSTART_MODE="$2"
      [[ "${AUTOSTART_MODE}" == "yes" || "${AUTOSTART_MODE}" == "no" || "${AUTOSTART_MODE}" == "ask" ]] \
        || fail "--autostart must be yes, no, or ask"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "Unknown option: $1"
      ;;
  esac
done

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || fail "Missing required command: $1"
}

group_exists() {
  getent group "$1" >/dev/null 2>&1
}

run_as_build_user() {
  if [[ "$(id -un)" == "${BUILD_USER}" ]]; then
    "$@"
  else
    sudo -u "${BUILD_USER}" "$@"
  fi
}

run_with_root() {
  if [[ "${EUID}" -eq 0 ]]; then
    "$@"
  else
    sudo "$@"
  fi
}

detect_pkg_manager() {
  if command -v apt-get >/dev/null 2>&1; then
    echo "apt"
    return
  fi
  fail "Only apt-based systems are supported by this installer right now"
}

ensure_dependencies() {
  local pkg_manager
  pkg_manager="$(detect_pkg_manager)"
  [[ "${pkg_manager}" == "apt" ]] || fail "Unsupported package manager: ${pkg_manager}"

  log "Installing build/runtime dependencies via apt"
  run_with_root apt-get update
  run_with_root apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    git \
    curl \
    ca-certificates \
    libmosquitto-dev \
    sqlite3 \
    libsqlite3-dev

  if command -v node >/dev/null 2>&1 && command -v npm >/dev/null 2>&1; then
    log "Node.js and npm already available — skipping apt install for JS toolchain"
    return
  fi

  if ! command -v node >/dev/null 2>&1; then
    log "Installing Node.js via apt"
    run_with_root apt-get install -y --no-install-recommends nodejs
  fi

  if ! command -v npm >/dev/null 2>&1; then
    log "npm not found after Node.js install — trying apt package"
    if ! run_with_root apt-get install -y --no-install-recommends npm; then
      fail "npm could not be installed automatically. Please repair the Node.js/npm setup manually or rerun with --skip-deps once node and npm work."
    fi
  fi
}

check_node_version() {
  need_cmd node
  local node_major raw
  raw="$(node -p "process.versions.node.split('.')[0]")"
  node_major="${raw}"
  if (( node_major < 20 )); then
    fail "Node.js >= 20 is required for the WebUI build. Installed major version: ${node_major}"
  fi
}

ensure_runtime_files() {
  if [[ "${SIM_MODE}" == "yes" ]]; then
    return
  fi

  log "Ensuring runtime config exists at ${CONFIG_PATH}"
  run_with_root mkdir -p "$(dirname "${CONFIG_PATH}")"
  if [[ ! -f "${CONFIG_PATH}" ]]; then
    run_with_root cp "${ROOT_DIR}/config.example.json" "${CONFIG_PATH}"
    log "Created ${CONFIG_PATH} from config.example.json"
  fi

  run_with_root mkdir -p "$(dirname "${MAP_PATH}")"
  if [[ ! -f "${MAP_PATH}" ]]; then
    run_with_root tee "${MAP_PATH}" >/dev/null <<'EOF'
{"perimeter":[],"mow":[],"dock":[],"exclusions":[]}
EOF
    log "Created empty map file at ${MAP_PATH}"
  fi
}

build_native() {
  log "Configuring native build in ${BUILD_DIR}"
  run_as_build_user cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/${BUILD_DIR}"

  log "Building native target"
  run_as_build_user cmake --build "${ROOT_DIR}/${BUILD_DIR}" -j"$(getconf _NPROCESSORS_ONLN)"
}

build_webui() {
  need_cmd npm
  check_node_version

  log "Installing WebUI dependencies"
  (
    cd "${ROOT_DIR}/${WEBUI_DIR}"
    run_as_build_user npm install
  )

  log "Building WebUI"
  (
    cd "${ROOT_DIR}/${WEBUI_DIR}"
    run_as_build_user npm run build
  )
}

prompt_yes_no() {
  local prompt="$1"
  local default_answer="$2"
  local reply

  if [[ ! -t 0 ]]; then
    if [[ "${default_answer}" == "yes" ]]; then
      return 0
    fi
    return 1
  fi

  if [[ "${default_answer}" == "yes" ]]; then
    read -r -p "${prompt} [Y/n] " reply
  else
    read -r -p "${prompt} [y/N] " reply
  fi

  reply="${reply,,}"
  if [[ -z "${reply}" ]]; then
    [[ "${default_answer}" == "yes" ]]
    return
  fi

  [[ "${reply}" == "y" || "${reply}" == "yes" ]]
}

ensure_service_permissions() {
  if [[ "${SIM_MODE}" == "yes" ]]; then
    return
  fi

  local config_dir map_dir history_dirs
  config_dir="$(dirname "${CONFIG_PATH}")"
  map_dir="$(dirname "${MAP_PATH}")"
  history_dirs=("/var/lib/sunray-core" "/var/lib/sunray")

  run_with_root chown -R "${BUILD_USER}:${BUILD_GROUP}" "${config_dir}"
  if [[ "${map_dir}" != "${config_dir}" ]]; then
    run_with_root chown -R "${BUILD_USER}:${BUILD_GROUP}" "${map_dir}"
  fi

  for history_dir in "${history_dirs[@]}"; do
    run_with_root mkdir -p "${history_dir}"
    run_with_root chown -R "${BUILD_USER}:${BUILD_GROUP}" "${history_dir}"
  done
}

systemd_quote() {
  local s="$1"
  s="${s//\\/\\\\}"
  s="${s//\"/\\\"}"
  printf '"%s"' "${s}"
}

write_systemd_service() {
  local exec_path service_path exec_line supplementary_groups=()
  exec_path="${ROOT_DIR}/${BUILD_DIR}/sunray-core"
  service_path="/etc/systemd/system/${SERVICE_NAME}.service"

  if [[ "${SIM_MODE}" == "yes" ]]; then
    exec_line="$(systemd_quote "${exec_path}") --sim $(systemd_quote "${ROOT_DIR}/config.example.json")"
  else
    exec_line="$(systemd_quote "${exec_path}") $(systemd_quote "${CONFIG_PATH}")"
  fi

  if group_exists "dialout"; then
    supplementary_groups+=("dialout")
  fi
  if group_exists "i2c"; then
    supplementary_groups+=("i2c")
  fi

  run_with_root tee "${service_path}" >/dev/null <<EOF
[Unit]
Description=Sunray Core
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=${ROOT_DIR}
User=${BUILD_USER}
Group=${BUILD_GROUP}
ExecStart=${exec_line}
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

  if (( ${#supplementary_groups[@]} > 0 )); then
    run_with_root sed -i "/^Group=/a SupplementaryGroups=${supplementary_groups[*]}" "${service_path}"
  fi

  run_with_root systemctl daemon-reload
}

handle_autostart() {
  local enable_service="no"

  case "${AUTOSTART_MODE}" in
    yes) enable_service="yes" ;;
    no) enable_service="no" ;;
    ask)
      if prompt_yes_no "Soll Sunray beim Systemstart automatisch per systemd starten?" "no"; then
        enable_service="yes"
      fi
      ;;
  esac

  if [[ "${enable_service}" == "yes" ]]; then
    log "Installing systemd service"
    write_systemd_service
    run_with_root systemctl enable "${SERVICE_NAME}.service"
    if [[ "${START_AFTER_INSTALL}" == "yes" ]]; then
      log "Starting systemd service"
      run_with_root systemctl restart "${SERVICE_NAME}.service"
      log "Sunray is running as systemd service: ${SERVICE_NAME}.service"
    fi
    return 0
  fi

  return 1
}

setup_ota() {
  local sudoers_file="/etc/sudoers.d/sunray-ota"
  local ota_script="${ROOT_DIR}/scripts/ota_update.sh"
  local flash_script="${ROOT_DIR}/scripts/flash_alfred.sh"
  local restart_cmd="/bin/systemctl restart ${SERVICE_NAME}.service"
  local stm_probe_cmd="/bin/bash ${flash_script} probe"

  # Make ota_update.sh executable
  chmod +x "${ota_script}" 2>/dev/null || true
  chmod +x "${flash_script}" 2>/dev/null || true

  # Grant passwordless sudo only for the specific restart and STM probe commands
  log "Configuring OTA sudoers rule for user '${BUILD_USER}'"
  run_with_root bash -c "printf '%s\n%s\n' \
    '${BUILD_USER} ALL=(ALL) NOPASSWD: ${restart_cmd}' \
    '${BUILD_USER} ALL=(ALL) NOPASSWD: ${stm_probe_cmd}' > ${sudoers_file}"
  run_with_root chmod 440 "${sudoers_file}"

  # Write initial version file
  "${ota_script}" --write-version || true

  log "OTA updates enabled — use the WebUI Settings panel to update"
}

start_foreground() {
  local exec_path
  exec_path="${ROOT_DIR}/${BUILD_DIR}/sunray-core"
  [[ -x "${exec_path}" ]] || fail "Binary not found: ${exec_path}"

  log "Starting Sunray in foreground"
  if [[ "${SIM_MODE}" == "yes" ]]; then
    exec "${exec_path}" --sim "${ROOT_DIR}/config.example.json"
  else
    exec "${exec_path}" "${CONFIG_PATH}"
  fi
}

main() {
  if [[ "${INSTALL_DEPS}" == "yes" ]]; then
    ensure_dependencies
  fi

  need_cmd cmake
  need_cmd git

  ensure_runtime_files
  ensure_service_permissions
  build_native
  build_webui

  setup_ota

  if [[ "${START_AFTER_INSTALL}" == "no" ]]; then
    log "Install/build complete. Start skipped by request."
    exit 0
  fi

  if handle_autostart; then
    exit 0
  fi

  start_foreground
}

main "$@"

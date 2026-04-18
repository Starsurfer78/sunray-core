#!/usr/bin/env bash
# ota_update.sh — OTA update for sunray-core.
#
# Usage:
#   ota_update.sh --check-only     Print update status and exit (no changes)
#   ota_update.sh --write-version  Write current git state to version file and exit
#   ota_update.sh                  Check, build, install, restart
#
# Output on stdout (machine-readable, one line):
#   already_up_to_date
#   update_available:<short-hash>
#   update_complete
#   error:<message>
# Ensure we are running under bash (in case executed via 'sh script.sh')
if [ -z "${BASH_VERSION:-}" ]; then
    exec bash "$0" "$@"
fi

set -euo pipefail


REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVICE_NAME="sunray-core"
VERSION_FILE="/etc/sunray-core/version.json"
BACKUP_BIN="/etc/sunray-core/sunray-core.bak"
BUILD_DIR="${REPO_DIR}/build_linux"
LOG_FILE="/var/lib/sunray-core/ota.log"

log() {
    echo "[ota] $*" >&2
}

fail() {
    echo "error:$*"
    exit 1
}

write_version() {
    local hash branch ts
    hash=$(git -C "${REPO_DIR}" rev-parse HEAD 2>/dev/null || echo "unknown")
    branch=$(git -C "${REPO_DIR}" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
    ts=$(date -Iseconds)
    mkdir -p "$(dirname "${VERSION_FILE}")"
    printf '{"hash":"%s","short":"%s","branch":"%s","updatedAt":"%s"}\n' \
        "${hash}" "${hash:0:8}" "${branch}" "${ts}" > "${VERSION_FILE}"
    log "Version written: ${hash:0:8} (${branch})"
}

mkdir -p "$(dirname "${LOG_FILE}")" 2>/dev/null || true
touch "${LOG_FILE}" 2>/dev/null || true

if [[ "${1:-}" == "--write-version" ]]; then
    write_version
    exit 0
fi

# Fetch remote refs (no checkout)
log "Fetching remote refs..."
git -C "${REPO_DIR}" fetch origin --quiet 2>/dev/null \
    || fail "git fetch failed — check network or remote URL"

CURRENT_HASH=$(git -C "${REPO_DIR}" rev-parse HEAD)
CURRENT_BRANCH=$(git -C "${REPO_DIR}" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "")
UPSTREAM_REF=$(git -C "${REPO_DIR}" rev-parse --abbrev-ref --symbolic-full-name '@{u}' 2>/dev/null || echo "")
if [[ -z "${UPSTREAM_REF}" && -n "${CURRENT_BRANCH}" && "${CURRENT_BRANCH}" != "HEAD" ]]; then
    if git -C "${REPO_DIR}" show-ref --verify --quiet "refs/remotes/origin/${CURRENT_BRANCH}"; then
        UPSTREAM_REF="origin/${CURRENT_BRANCH}"
    fi
fi
REMOTE_HASH=$(git -C "${REPO_DIR}" rev-parse "${UPSTREAM_REF:-origin/HEAD}" 2>/dev/null \
    || git -C "${REPO_DIR}" rev-parse origin/main 2>/dev/null \
    || git -C "${REPO_DIR}" rev-parse origin/master 2>/dev/null \
    || echo "")

if [[ -z "${REMOTE_HASH}" ]]; then
    fail "could not determine remote HEAD"
fi

if [[ "${CURRENT_HASH}" == "${REMOTE_HASH}" ]]; then
    echo "already_up_to_date"
    exit 0
fi

echo "update_available:${REMOTE_HASH:0:8}"

if [[ "${1:-}" == "--check-only" ]]; then
    exit 0
fi

# ── Full update ────────────────────────────────────────────────────────────────
log "Starting update ${CURRENT_HASH:0:8} → ${REMOTE_HASH:0:8}"

# Backup current binary for rollback
if [[ -f "${BUILD_DIR}/sunray-core" ]]; then
    cp "${BUILD_DIR}/sunray-core" "${BACKUP_BIN}" 2>/dev/null || true
    log "Backup saved to ${BACKUP_BIN}"
fi

# Pull
git -C "${REPO_DIR}" pull --ff-only origin 2>&1 | tee -a "${LOG_FILE}" \
    || fail "git pull failed"

# Build (no deps, no start — the service is still running during build)
"${REPO_DIR}/scripts/install_sunray.sh" \
    --no-start \
    --skip-deps \
    --autostart no \
    2>&1 | tee -a "${LOG_FILE}" \
    || {
        # Rollback
        log "Build failed — rolling back binary"
        if [[ -f "${BACKUP_BIN}" ]]; then
            cp "${BACKUP_BIN}" "${BUILD_DIR}/sunray-core"
            log "Rollback complete"
        fi
        fail "build failed"
    }

# Write version before restart so it is available immediately after reconnect
write_version

# Restart service (requires passwordless sudo for this command, set up by install_sunray.sh)
sudo systemctl restart "${SERVICE_NAME}.service" 2>&1 | tee -a "${LOG_FILE}" \
    || fail "systemctl restart failed"

log "Update complete: ${REMOTE_HASH:0:8}"
echo "update_complete"

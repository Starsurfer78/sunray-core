#!/usr/bin/env bash
# Ensure we are running under bash (in case executed via 'sh script.sh')
if [ -z "${BASH_VERSION:-}" ]; then
    exec bash "$0" "$@"
fi

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
UPLOAD_BIN="${UPLOAD_BIN:-/var/lib/sunray-core/stm-upload/rm18-upload.bin}"

export BIN_PATH="${UPLOAD_BIN}"

exec /bin/bash "${SCRIPT_DIR}/flash_alfred.sh" flash

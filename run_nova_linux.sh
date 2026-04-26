#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export WINEPREFIX="${NOVA_WINEPREFIX:-$HOME/.nova-wine}"
mkdir -p "$WINEPREFIX"
exec wine "$ROOT_DIR/Nova.exe"

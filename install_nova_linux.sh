#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WINEPREFIX="${NOVA_WINEPREFIX:-$HOME/.nova-wine}"
BUILD_EXE="$ROOT_DIR/Nova.exe"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "This installer is for Linux hosts."
  exit 1
fi

if command -v apt-get >/dev/null 2>&1; then
  if command -v dpkg >/dev/null 2>&1 && ! dpkg --print-foreign-architectures | grep -qx i386; then
    sudo dpkg --add-architecture i386
  fi
  sudo apt-get update
  sudo apt-get install -y --no-install-recommends \
    mingw-w64 wine wine64 wine32:i386 xvfb imagemagick curl ca-certificates
else
  echo "Install dependencies manually: mingw-w64, wine, wine64, wine32, xvfb, imagemagick, curl."
fi

mkdir -p "$ROOT_DIR/engine" "$ROOT_DIR/models" "$WINEPREFIX"

x86_64-w64-mingw32-g++ -O2 -std=c++17 -DUNICODE -D_UNICODE -DNOVA_DISABLE_SAPI \
  -static -static-libgcc -static-libstdc++ "$ROOT_DIR/nova.cpp" -o "$BUILD_EXE" -mwindows \
  -lwininet -luser32 -lgdi32 -lkernel32 -lole32 -lcomctl32 -lgdiplus -lcomdlg32 \
  -lwinmm -lshell32 -lshlwapi -ladvapi32

cat > "$ROOT_DIR/nova_config.ini" <<'CONFIG'
provider=1
host=127.0.0.1
port=11434
api_key=
model=llama3:latest
endpoint_path=/v1/chat/completions
use_ssl=0
temperature=0.7
max_tokens=1024
context_size=4096
gpu_layers=0
auto_start_engine=0
model_path=models\llama3.gguf
engine_port=11434
CONFIG

cat > "$ROOT_DIR/run_nova_linux.sh" <<'RUNNER'
#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export WINEPREFIX="${NOVA_WINEPREFIX:-$HOME/.nova-wine}"
mkdir -p "$WINEPREFIX"
exec wine "$ROOT_DIR/Nova.exe"
RUNNER
chmod +x "$ROOT_DIR/run_nova_linux.sh"

echo "Nova Linux/Wine setup complete."
echo "Run: ./run_nova_linux.sh"

#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export USB_MANAGER_DEPS_PREFIX="${USB_MANAGER_DEPS_PREFIX:-$ROOT/.deps/prefix}"
export PATH="$USB_MANAGER_DEPS_PREFIX/usr/bin:$PATH"
export PKG_CONFIG_PATH="$USB_MANAGER_DEPS_PREFIX/usr/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="$USB_MANAGER_DEPS_PREFIX/usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"

cmake -S "$ROOT/usb-manager-c" -B "$ROOT/build-c"
cmake --build "$ROOT/build-c" -j"$(nproc)"
ctest --test-dir "$ROOT/build-c" --output-on-failure
echo "C binaries in $ROOT/build-c/"

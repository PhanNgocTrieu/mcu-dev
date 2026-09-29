#!/usr/bin/env bash
# Host-side build helper (works without sudo by using .deps/prefix).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export USB_MANAGER_DEPS_PREFIX="${USB_MANAGER_DEPS_PREFIX:-$ROOT/.deps/prefix}"
export PATH="$USB_MANAGER_DEPS_PREFIX/usr/bin:$PATH"
export PKG_CONFIG_PATH="$USB_MANAGER_DEPS_PREFIX/usr/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"

cmake -S "$ROOT/usb-manager" -B "$ROOT/build"
cmake --build "$ROOT/build" -j"$(nproc)"
ctest --test-dir "$ROOT/build" --output-on-failure
echo "Binaries in $ROOT/build/"

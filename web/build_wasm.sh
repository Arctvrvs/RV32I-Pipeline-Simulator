#!/usr/bin/env bash
# Build the RV32 WASM module and serve it locally.
# Requires Emscripten (emsdk) to be activated in your shell.
set -e

REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"

# Configure + build
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "Built web/sim.js"

# Serve (ES modules require HTTP, not file://)
echo "Serving web/ at http://localhost:8000  (Ctrl+C to stop)"
cd web
python3 -m http.server 8000

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

cmake -S . -B build
cmake --build build -j4
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$ROOT_DIR/dist/install"
cpack --config build/CPackConfig.cmake

echo "release artifacts:"
find "$ROOT_DIR" -maxdepth 1 -name 'mtop-*.tar.gz' -print
find "$ROOT_DIR/dist" -maxdepth 3 -print 2>/dev/null || true

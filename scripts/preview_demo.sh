#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [[ ! -x build/mtop ]]; then
  cmake -S . -B build
  cmake --build build -j4
fi

exec ./build/mtop --demo

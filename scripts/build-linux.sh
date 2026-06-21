#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
CMAKE_BIN="${CMAKE_BIN:-cmake}"

CMAKE_ARGS=(
    -S "$ROOT_DIR"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
)

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH")
fi

"$CMAKE_BIN" "${CMAKE_ARGS[@]}"
"$CMAKE_BIN" --build "$BUILD_DIR" --config "$BUILD_TYPE"


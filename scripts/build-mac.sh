#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

CMAKE_BIN="${CMAKE_BIN:-cmake}"
if ! command -v "$CMAKE_BIN" >/dev/null 2>&1; then
    QT_CMAKE="/Users/fuk/Qt/Tools/CMake/CMake.app/Contents/bin/cmake"
    if [[ -x "$QT_CMAKE" ]]; then
        CMAKE_BIN="$QT_CMAKE"
    else
        echo "cmake was not found. Set CMAKE_BIN or install CMake." >&2
        exit 1
    fi
fi

CMAKE_ARGS=(
    -S "$ROOT_DIR"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
)

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH")
elif [[ -d "/Users/fuk/Qt/6.11.1/macos" ]]; then
    CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="/Users/fuk/Qt/6.11.1/macos")
fi

"$CMAKE_BIN" "${CMAKE_ARGS[@]}"
"$CMAKE_BIN" --build "$BUILD_DIR" --config "$BUILD_TYPE"

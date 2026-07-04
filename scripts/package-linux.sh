#!/usr/bin/env bash
set -euo pipefail

# Build a Linux install package (.deb) into dist/.
# Usage: scripts/package-linux.sh
# Environment overrides: BUILD_DIR, BUILD_TYPE, DIST_DIR, CMAKE_BIN, CPACK_BIN, CMAKE_PREFIX_PATH

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
DIST_DIR="${DIST_DIR:-"$ROOT_DIR/dist"}"
CMAKE_BIN="${CMAKE_BIN:-cmake}"
CPACK_BIN="${CPACK_BIN:-cpack}"

if ! command -v dpkg-shlibdeps >/dev/null 2>&1; then
    echo "error: dpkg-shlibdeps not found. Install it with: sudo apt install dpkg-dev" >&2
    exit 1
fi

CMAKE_ARGS=(
    -S "$ROOT_DIR"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
)

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH")
fi

"$CMAKE_BIN" "${CMAKE_ARGS[@]}"
"$CMAKE_BIN" --build "$BUILD_DIR" --config "$BUILD_TYPE" --target mycel

mkdir -p "$DIST_DIR"
(cd "$BUILD_DIR" && "$CPACK_BIN" -G DEB -B "$DIST_DIR")

# CPack leaves its staging directory next to the package; keep dist/ clean.
rm -rf "$DIST_DIR/_CPack_Packages"

echo
echo "Package(s) created in $DIST_DIR:"
ls -1 "$DIST_DIR"/*.deb

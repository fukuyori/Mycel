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

APP_VERSION="$(sed -nE 's/^[[:space:]]*project\([[:space:]]*Mycel[[:space:]]+VERSION[[:space:]]+([0-9]+(\.[0-9]+){1,3}).*/\1/p' "$ROOT_DIR/CMakeLists.txt" | head -n 1)"
if [[ -z "$APP_VERSION" ]]; then
    echo "error: could not read the Mycel version from CMakeLists.txt" >&2
    exit 1
fi

PACKAGE_ARCH="$(uname -m | tr '[:upper:]' '[:lower:]')"
case "$PACKAGE_ARCH" in
    x86_64|amd64) PACKAGE_ARCH="x64" ;;
    aarch64|arm64) PACKAGE_ARCH="arm64" ;;
esac
PACKAGE_PATH="$DIST_DIR/Mycel-$APP_VERSION-linux-$PACKAGE_ARCH.deb"

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
if [[ -f "$PACKAGE_PATH" ]]; then
    echo "Created package: $PACKAGE_PATH"
else
    echo "Package(s) created in $DIST_DIR:"
    find "$DIST_DIR" -maxdepth 1 -type f -name "Mycel-$APP_VERSION-linux-*.deb" | sort
fi

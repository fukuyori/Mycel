#!/usr/bin/env bash
set -euo pipefail

# Build a macOS disk image (.dmg) into dist/.
# Usage: scripts/package-mac.sh
# Environment overrides: BUILD_DIR, BUILD_TYPE, DIST_DIR, CMAKE_BIN,
# CMAKE_PREFIX_PATH, MACDEPLOYQT_BIN

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
DIST_DIR="${DIST_DIR:-"$ROOT_DIR/dist"}"

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

APP_BUNDLE="$BUILD_DIR/Mycel.app"
PACKAGE_PATH="$DIST_DIR/Mycel-$APP_VERSION-macos-$PACKAGE_ARCH.dmg"

"$ROOT_DIR/scripts/build-mac.sh"

MACDEPLOYQT_BIN="${MACDEPLOYQT_BIN:-}"
if [[ -z "$MACDEPLOYQT_BIN" && -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    MACDEPLOYQT_BIN="$CMAKE_PREFIX_PATH/bin/macdeployqt"
fi
if [[ -z "$MACDEPLOYQT_BIN" && -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    QT6_DIR="$(sed -n 's/^Qt6_DIR:PATH=//p' "$BUILD_DIR/CMakeCache.txt" | head -n 1)"
    if [[ -n "$QT6_DIR" ]]; then
        MACDEPLOYQT_BIN="$(cd "$QT6_DIR/../../.." && pwd)/bin/macdeployqt"
    fi
fi
if [[ -z "$MACDEPLOYQT_BIN" ]] && command -v macdeployqt >/dev/null 2>&1; then
    MACDEPLOYQT_BIN="$(command -v macdeployqt)"
fi
if [[ -z "$MACDEPLOYQT_BIN" || ! -x "$MACDEPLOYQT_BIN" ]]; then
    echo "error: macdeployqt was not found. Set MACDEPLOYQT_BIN or CMAKE_PREFIX_PATH." >&2
    exit 1
fi

mkdir -p "$DIST_DIR"
rm -f "$BUILD_DIR/Mycel.dmg"
"$MACDEPLOYQT_BIN" "$APP_BUNDLE" -dmg
mv "$BUILD_DIR/Mycel.dmg" "$PACKAGE_PATH"

echo
echo "Created package: $PACKAGE_PATH"

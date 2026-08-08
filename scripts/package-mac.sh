#!/usr/bin/env bash
set -euo pipefail

# Build a macOS installer package (.pkg) into dist/.
# Usage: scripts/package-mac.sh
# Environment overrides: BUILD_DIR, BUILD_TYPE, DIST_DIR, CMAKE_BIN,
# CMAKE_PREFIX_PATH, MACDEPLOYQT_BIN, PKGBUILD_BIN, CODESIGN_BIN,
# APPLICATION_IDENTITY, INSTALLER_IDENTITY, NOTARYTOOL_PROFILE, XCRUN_BIN

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
DIST_DIR="${DIST_DIR:-"$ROOT_DIR/dist"}"
APPLICATION_IDENTITY="${APPLICATION_IDENTITY:-Developer ID Application: Noriaki Fukuyori (Q6GG27UYG5)}"
INSTALLER_IDENTITY="${INSTALLER_IDENTITY:-Developer ID Installer: Noriaki Fukuyori (Q6GG27UYG5)}"
NOTARYTOOL_PROFILE="${NOTARYTOOL_PROFILE:-notarytool}"
XCRUN_BIN="${XCRUN_BIN:-xcrun}"

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
PACKAGE_PATH="$DIST_DIR/Mycel-$APP_VERSION-macos-$PACKAGE_ARCH.pkg"

# macdeployqt modifies the application bundle in place. Reusing that deployed bundle causes
# stale plugins and qt.conf to be processed again, so always start from CMake's clean output.
if [[ -e "$APP_BUNDLE" ]]; then
    if [[ "$APP_BUNDLE" != */Mycel.app || "$APP_BUNDLE" == "/Mycel.app" ]]; then
        echo "error: refusing to remove unexpected app bundle path: $APP_BUNDLE" >&2
        exit 1
    fi
    rm -rf "$APP_BUNDLE"
fi

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

PKGBUILD_BIN="${PKGBUILD_BIN:-pkgbuild}"
if ! command -v "$PKGBUILD_BIN" >/dev/null 2>&1; then
    echo "error: pkgbuild was not found. Set PKGBUILD_BIN." >&2
    exit 1
fi
CODESIGN_BIN="${CODESIGN_BIN:-codesign}"
if ! command -v "$CODESIGN_BIN" >/dev/null 2>&1; then
    echo "error: codesign was not found. Set CODESIGN_BIN." >&2
    exit 1
fi
if ! command -v "$XCRUN_BIN" >/dev/null 2>&1; then
    echo "error: xcrun was not found. Set XCRUN_BIN." >&2
    exit 1
fi

mkdir -p "$DIST_DIR"

DEPLOY_LOG="$(mktemp "${TMPDIR:-/tmp}/mycel-macdeployqt.XXXXXX")"
FILTERED_DEPLOY_LOG="$(mktemp "${TMPDIR:-/tmp}/mycel-macdeployqt-filtered.XXXXXX")"
cleanup_deploy_logs() {
    rm -f "$DEPLOY_LOG" "$FILTERED_DEPLOY_LOG"
}
trap cleanup_deploy_logs EXIT

if ! "$MACDEPLOYQT_BIN" "$APP_BUNDLE" \
    -sign-for-notarization="$APPLICATION_IDENTITY" >"$DEPLOY_LOG" 2>&1; then
    cat "$DEPLOY_LOG" >&2
    exit 1
fi

# Qt Positioning ships an optional serial-GPS plugin even when Qt SerialPort is not installed.
# Mycel does not use serial positioning, so omit that plugin and its known deployment diagnostic.
NMEA_PLUGIN="$APP_BUNDLE/Contents/PlugIns/position/libqtposition_nmea.dylib"
if grep -Fq 'QtSerialPort.framework' "$DEPLOY_LOG"; then
    if [[ ! -f "$NMEA_PLUGIN" ]]; then
        cat "$DEPLOY_LOG" >&2
        echo "error: QtSerialPort is missing but the optional NMEA plugin was not found" >&2
        exit 1
    fi
    rm -f "$NMEA_PLUGIN"
    sed -e '\|ERROR: Cannot resolve rpath "@rpath/QtSerialPort.framework/Versions/A/QtSerialPort"|d' \
        -e '\|ERROR:  using QList(|d' "$DEPLOY_LOG" >"$FILTERED_DEPLOY_LOG"
else
    cp "$DEPLOY_LOG" "$FILTERED_DEPLOY_LOG"
fi

if grep -Eq '^(ERROR|WARNING):' "$FILTERED_DEPLOY_LOG"; then
    cat "$FILTERED_DEPLOY_LOG" >&2
    exit 1
fi
cat "$FILTERED_DEPLOY_LOG"

# Removing a deployed plugin invalidates the outer bundle seal. Re-sign only that outer seal;
# macdeployqt has already signed the nested code with the hardened runtime and timestamp.
"$CODESIGN_BIN" --force --sign "$APPLICATION_IDENTITY" \
    --options runtime --timestamp "$APP_BUNDLE"
"$CODESIGN_BIN" --verify --deep --strict "$APP_BUNDLE"

PKGBUILD_ARGS=(
    --component "$APP_BUNDLE"
    --install-location /Applications
    --identifier org.spumoni.mycel
    --version "$APP_VERSION"
    --sign "$INSTALLER_IDENTITY"
)
PKGBUILD_ARGS+=("$PACKAGE_PATH")
"$PKGBUILD_BIN" "${PKGBUILD_ARGS[@]}"

echo
echo "Submitting package for notarization..."
if ! "$XCRUN_BIN" notarytool submit "$PACKAGE_PATH" \
    --keychain-profile "$NOTARYTOOL_PROFILE" --wait; then
    echo >&2
    echo "error: notarization failed or keychain profile '$NOTARYTOOL_PROFILE' was not found." >&2
    echo "Create it once with:" >&2
    echo "  xcrun notarytool store-credentials '$NOTARYTOOL_PROFILE' --apple-id <apple-id> --team-id Q6GG27UYG5 --password <app-specific-password>" >&2
    exit 1
fi

"$XCRUN_BIN" stapler staple "$PACKAGE_PATH"
"$XCRUN_BIN" stapler validate "$PACKAGE_PATH"

echo
echo "Created signed and notarized package: $PACKAGE_PATH"

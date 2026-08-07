#!/bin/sh
# Anabasis system-wide installer (needs root — run with sudo).
# Installs the VST3 to /usr/lib/vst3 and the Standalone to /usr/local/bin,
# the standard system-wide locations every DAW scans by default.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
VST3_SRC="$HERE/Anabasis.vst3"
APP_SRC="$HERE/Anabasis"
VST3_DIR="/usr/lib/vst3"
BIN_DIR="/usr/local/bin"

[ -d "$VST3_SRC" ] || { echo "error: Anabasis.vst3 not found next to install.sh" >&2; exit 1; }
[ -f "$APP_SRC" ]  || { echo "error: Anabasis (Standalone) not found next to install.sh" >&2; exit 1; }
[ "$(id -u)" -eq 0 ] || { echo "error: a system-wide install needs root — run:  sudo ./install.sh" >&2; exit 1; }

mkdir -p "$VST3_DIR" "$BIN_DIR"
rm -rf "$VST3_DIR/Anabasis.vst3"
cp -R "$VST3_SRC" "$VST3_DIR/"
cp "$APP_SRC" "$BIN_DIR/Anabasis"
chmod 755 "$BIN_DIR/Anabasis" "$VST3_DIR/Anabasis.vst3/Contents/x86_64-linux/Anabasis.so" 2>/dev/null || true

echo "Installed (system-wide, all users):"
echo "  VST3       -> $VST3_DIR/Anabasis.vst3"
echo "  Standalone -> $BIN_DIR/Anabasis"
echo "Rescan plug-ins in your DAW to pick it up. Uninstall later with:  sudo ./uninstall.sh"

#!/usr/bin/env bash
# ============================================================================
#  Anabasis -- Linux build dependency setup
#
#  Installs everything needed to build the VST3 headlessly on a fresh Ubuntu
#  machine (no GUI / no IDE). Safe to re-run.
#
#  Network domains this script needs (allow-list these in a restricted sandbox):
#    - Ubuntu apt mirrors (archive.ubuntu.com / ports.ubuntu.com / your mirror)
#  The build itself additionally needs:
#    - github.com           (JUCE source, pinned commit, via CMake FetchContent)
#    - github.com           (pluginval release download, optional)
#
#  libegl-dev: JUCE 9 creates Linux OpenGL contexts via EGL instead of GLX
#  (juce_opengl linuxPackages "egl gl"), so EGL headers are a build dependency
#  even if Anabasis never attaches a GL context on Linux.
#
#  xvfb, curl and unzip are for pluginval (editor tests need a display; the
#  release download needs the other two), not for the build itself.
# ============================================================================
set -euo pipefail

SUDO=""
if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi

$SUDO apt-get update -y

# curl + unzip are for scripts/run-pluginval.sh, which downloads and extracts the
# pluginval release. They are NOT implied by libcurl4-openssl-dev (that is the
# development headers, not the CLI). GitHub-hosted runners preinstall both, which
# is exactly why their absence would only ever bite on a fresh machine or a
# minimal container -- i.e. the case this script exists to cover.
# `env` carries the assignment whether $SUDO is "sudo" or empty (as root).
# A bare `$SUDO DEBIAN_FRONTEND=... apt-get` breaks in the root case: the
# assignment is not in assignment position at parse time (the first word is
# $SUDO), so when $SUDO expands to nothing it becomes the COMMAND NAME.
# CI always runs the sudo path, which is why this never failed there.
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential cmake git ninja-build pkg-config \
    curl unzip \
    libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev \
    libfreetype6-dev libfontconfig1-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev \
    libglu1-mesa-dev mesa-common-dev libegl-dev \
    libwebkit2gtk-4.1-dev libgtk-3-dev \
    xvfb

echo
echo "Anabasis: Linux build dependencies installed."
echo "Note: if 'libwebkit2gtk-4.1-dev' is unavailable on your release, try 'libwebkit2gtk-4.0-dev'."

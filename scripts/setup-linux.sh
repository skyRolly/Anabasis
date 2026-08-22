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
#  lld is LLVM's linker, and it is a REQUIREMENT for the Clang builds rather
#  than a preference: GNU ld scans a static archive once, while Clang's LTO
#  codegen runs after that scan and then needs members the scan passed over, so
#  linking the plugin can fail with hundreds of undefined references to symbols
#  that are demonstrably inside libAnabasis_SharedCode.a. CMakeLists.txt probes
#  for it and falls back with a warning; installing it here is what makes the
#  probe succeed. GCC builds ignore it.
#  xvfb, curl and unzip are for pluginval (editor tests need a display; the
#  release download needs the other two), not for the build itself.
# ============================================================================
#  TWO PROFILES, because one caller is not a fresh Ubuntu machine. The GCC
#  compatibility job (`linux-lto-tests`, ADR-0033) runs inside the official `gcc`
#  image -- Debian, with its own pinned compiler already installed -- and builds
#  only the two headless test targets. `headless` installs what COMPILING AND
#  LINKING the JUCE targets needs and nothing else; `full` (the default, and what
#  a developer or any packaging job wants) adds the host toolchain, the pluginval
#  fetch/display pair and lld. The lists stay HERE rather than in the workflow so
#  there is still ONE place that knows what a build needs; the profile only
#  decides how much of it.
#
#  Both profiles name `python3` EXPLICITLY, and that line is load-bearing rather
#  than tidy. Every checker in `scripts/` is Python, and GitHub's Ubuntu images
#  preinstall an interpreter, so nothing ever had to ask -- but a container
#  carries one only if something else happened to pull it in. A gate that cannot
#  run because its interpreter is absent is the failure this prevents.
#  `ca-certificates` is here for the same class of reason: FetchContent clones
#  JUCE over HTTPS, and a minimal container may ship no trust store.
# ============================================================================
set -euo pipefail

PROFILE="${1:-full}"
case "$PROFILE" in
    full|headless) ;;
    *)
        echo "setup-linux: usage: $0 [full|headless]  (got '${PROFILE}')" >&2
        exit 2
        ;;
esac

SUDO=""
if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi

# WHICH DISTRIBUTION IS RESOLVING THESE NAMES. This script now runs on two --
# Ubuntu on the runners and Debian inside the `gcc:<major>` container -- and
# package names are not a constant across them (see `libfreetype-dev` below).
# `ARCHITECTURE_REVIEW_GATE.md` rule 2 asks for detection and record where a
# version cannot be pinned; this is the record, and it is what a future rename
# will be diagnosed from.
if [ -r /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    echo "setup-linux: ${PRETTY_NAME:-${NAME:-unknown}} (profile: ${PROFILE})"
fi

$SUDO apt-get update -y

# What compiling and linking the JUCE targets needs, on any profile. The GUI
# development headers are NOT optional for a headless build: the test targets
# link juce_gui_basics, juce_gui_extra and juce_opengl (CMakeLists.txt), so their
# headers are required to compile even though nothing here opens a window.
#
# `libfreetype-dev`, NOT `libfreetype6-dev`, and the difference is a lane-killer
# rather than a style choice. Debian **trixie** -- the base of the `gcc:16`
# container (`FROM buildpack-deps:trixie`) -- ships no `libfreetype6-dev` at all,
# neither as a real package nor as a virtual one, so `apt-get install` would fail
# and take the whole GCC lane with it at dependency install. It worked on Ubuntu
# only because noble's `libfreetype-dev` carries `Provides: libfreetype6-dev` --
# a compatibility name one distribution has already dropped. `libfreetype-dev` is
# a REAL package on both (noble 2.13.2, trixie 2.13.3), so it resolves natively
# either way. Verified against both archives, 2026-08-22.
#
# `libxi-dev` is named here for the same class of reason, and it is the second
# lane-killer this list has had to learn. JUCE 9.0.1 defaults `JUCE_USE_XINPUT`
# to 1 (juce_gui_basics.h), so `juce_gui_basics.h:393` includes
# <X11/extensions/XInput2.h> UNCONDITIONALLY in practice -- and that header
# belongs to `libxi-dev`, which nothing here used to ask for. It arrived anyway
# on `full`, as a transitive dependency of `libgtk-3-dev` (`Depends: libxi-dev`),
# which is why a developer machine and the Ubuntu runners never noticed. The
# `headless` profile drops the gtk/webkit pair on purpose -- nothing this project
# compiles needs webkit -- and silently dropped the X-input HEADERS with it, so
# the GCC container lane died at `fatal error: X11/extensions/XInput2.h: No such
# file or directory` in three JUCE translation units. Depending on a GUI toolkit
# we do not compile to supply a header we DO compile is exactly the accident an
# explicit list exists to prevent, so it is explicit on BOTH profiles.
# `libxi-dev` is a real package on noble and on trixie alike; verified against
# both archives, 2026-08-22.
CORE_PACKAGES="
    cmake git ninja-build pkg-config ca-certificates python3
    libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev
    libfreetype-dev libfontconfig1-dev
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev
    libxi-dev libxinerama-dev libxrandr-dev libxrender-dev
    libglu1-mesa-dev mesa-common-dev libegl-dev
"

# Everything `headless` deliberately leaves out.
#   build-essential -- the HOST toolchain. A container that ships its own pinned
#                      compiler must not have a distribution one installed over
#                      the top of it; that would silently un-pin the lane.
#   curl, unzip     -- run-pluginval.sh fetches and extracts a release. They are
#                      NOT implied by libcurl4-openssl-dev, which is the
#                      development headers rather than the CLI.
#   xvfb            -- pluginval needs a display; nothing else here does.
#   lld             -- the Clang LTO link (see above). GCC never reaches it.
#   webkit/gtk      -- the WebBrowserComponent's Linux backend, and NOTHING here
#                      compiles it: every Anabasis target sets
#                      `JUCE_WEB_BROWSER=0` (CMakeLists.txt), the webkit include
#                      in `juce_gui_extra.cpp` is gated on that macro, and JUCE
#                      9.0.1 declares no `linuxPackages` for `juce_gui_extra` at
#                      all (only alsa, freetype2+fontconfig and egl+gl, in three
#                      other modules). They are kept on `full` because that
#                      profile serves a developer's machine, where flipping the
#                      macro should not mean a second dependency hunt -- and they
#                      are OUT of `headless` because they are simultaneously the
#                      heaviest entries in the list and the most volatile names
#                      in it: `libwebkit2gtk-4.0-dev` is already gone from
#                      trixie, and the successor there is `libwebkitgtk-6.0-dev`.
FULL_EXTRA_PACKAGES="
    build-essential
    curl unzip
    xvfb
    lld
    libwebkit2gtk-4.1-dev libgtk-3-dev
"

PACKAGES="$CORE_PACKAGES"
if [ "$PROFILE" = "full" ]; then
    PACKAGES="$PACKAGES $FULL_EXTRA_PACKAGES"
fi

# `env` carries the assignment whether $SUDO is "sudo" or empty (as root).
# A bare `$SUDO DEBIAN_FRONTEND=... apt-get` breaks in the root case: the
# assignment is not in assignment position at parse time (the first word is
# $SUDO), so when $SUDO expands to nothing it becomes the COMMAND NAME and the
# script dies with "command not found". CI always took the sudo path, which is
# why this never failed there -- it fails in a root container, i.e. exactly the
# minimal environment the `headless` profile exists for.
# Unquoted ON PURPOSE: $PACKAGES is a whitespace-separated LIST and word
# splitting is how it becomes several arguments. The values are literals from
# this file, never caller input.
# shellcheck disable=SC2086
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y $PACKAGES

echo
echo "Anabasis: Linux build dependencies installed (profile: ${PROFILE})."
if [ "$PROFILE" = "full" ]; then
    # Named rather than silently dropped: the webkit binding is the one package
    # here whose NAME moves between releases, and it is `full`-only precisely
    # because nothing this project compiles needs it. On a distribution where
    # `libwebkit2gtk-4.1-dev` is absent the successor is `libwebkitgtk-6.0-dev`
    # (Debian trixie and later); `libwebkit2gtk-4.0-dev` is the OLDER name and is
    # already gone from trixie, so it is not a fallback there.
    echo "Note: 'libwebkit2gtk-4.1-dev' is a \`full\`-profile extra for JUCE_WEB_BROWSER=1 builds;"
    echo "      where it is absent the successor is 'libwebkitgtk-6.0-dev'. Nothing this project"
    echo "      compiles needs it -- every target sets JUCE_WEB_BROWSER=0."
fi

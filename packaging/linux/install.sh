#!/bin/sh
# Anabasis Linux installer. Two modes, chosen interactively:
#
#   1) Current user (default, no root)  VST3 -> ~/.vst3, Standalone -> ~/.local/bin
#   2) System-wide  (needs root)        VST3 -> /usr/lib/vst3, Standalone -> /usr/local/bin
#
# ~/.vst3 is the standard per-user VST3 folder and is scanned by most DAWs, so the
# recommended install needs no root at all. Running this script as root
# (sudo ./install.sh) installs system-wide without asking.
#
# An existing installation is replaced safely rather than overwritten: the previous
# version is kept until the new one is fully in place, so an interrupted install
# still leaves a working Anabasis behind and the next run recovers on its own. The
# VST3 plug-in and the Standalone application are separate files and are replaced
# independently of each other.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
VST3_SRC="$HERE/Anabasis.vst3"
APP_SRC="$HERE/Anabasis"

SYS_VST3_DIR="/usr/lib/vst3"
SYS_BIN_DIR="/usr/local/bin"

# Only a system-wide install sets this; a per-user install never uses sudo.
SUDO=''

[ -d "$VST3_SRC" ] || { echo "error: Anabasis.vst3 not found next to install.sh" >&2; exit 1; }
[ -f "$APP_SRC" ]  || { echo "error: Anabasis (Standalone) not found next to install.sh" >&2; exit 1; }

# ---------------------------------------------------------------- mode choice
mode=user

if [ "$(id -u)" -eq 0 ]; then
    mode=system
    echo "Running as root - installing system-wide (for all users)."
elif [ -t 0 ]; then
    cat <<'EOF'
Anabasis Linux Installer

Choose installation location:

1) Install for current user (recommended)
   ~/.vst3

2) Install system-wide
   /usr/lib/vst3

EOF
    printf 'Select [1/2]: '
    choice=''
    read -r choice || choice=''      # EOF on stdin answers with the default
    case "$choice" in
        ''|1) mode=user   ;;
        2)    mode=system ;;
        *)    echo "Unrecognised answer '$choice' - using the default (current user)."
              mode=user ;;
    esac
else
    echo "Not running on a terminal - installing for the current user (the default)."
fi

# ------------------------------------------------------------ install helpers
# Both modes install the same way; only the elevation and the summary differ.

# Chooses a temporary working directory for the new version, kept out of the
# folder your DAW scans whenever that is possible.
choose_stage_dir() {            # $1 = plug-in directory; prints the stage directory
    _out="${1%/*}/.anabasis-install-stage"
    _in="$1/.anabasis-install-stage"
    _probe="$1/.anabasis-probe"
    $SUDO rm -rf "$_probe" 2>/dev/null || true
    # NOTE for anyone editing this function: the `rm -rf "$_out"` on the failure
    # path below is the highest-consequence line in the script, and it is safe
    # only because of what is above it. `$1` is always one of the two plug-in
    # directories set in the mode branches, never user input, so `${1%/*}` cannot
    # expand to something unexpected; the two early returns mean a parked
    # `Anabasis.vst3.prev` is never the thing being deleted; and the name is
    # installer-owned. What it DOES remove is any other leftover an aborted run
    # put there — intended, since the fallback is about to stage somewhere else.
    # An earlier install may have left the previous version parked here; keep
    # using whichever directory holds it so it stays recoverable.
    if [ -d "$_out/Anabasis.vst3.prev" ]; then printf '%s\n' "$_out"; return 0; fi
    if [ -d "$_in/Anabasis.vst3.prev" ];  then printf '%s\n' "$_in";  return 0; fi
    if $SUDO mkdir -p "$_out" 2>/dev/null \
       && $SUDO touch "$_out/.probe" 2>/dev/null \
       && $SUDO ln "$_out/.probe" "$_probe" 2>/dev/null
    then
        $SUDO rm -f "$_out/.probe" "$_probe" 2>/dev/null || true
        printf '%s\n' "$_out"
    else
        $SUDO rm -rf "$_out" 2>/dev/null || true
        printf '%s\n' "$_in"
    fi
}

# Puts back the previous version if an install was interrupted, then clears the
# temporary files. Runs once before installing and again if the script is stopped.
reconcile() {
    if [ ! -e "$VST3_DEST" ] && [ -d "$PREV_VST3" ]; then
        $SUDO mv "$PREV_VST3" "$VST3_DEST" 2>/dev/null || true
    fi
    $SUDO rm -rf "$STAGE_VST3" "$STAGE_APP" 2>/dev/null || true
    # The kept copy is only discarded once the plug-in is back in place.
    if [ -e "$VST3_DEST" ]; then
        $SUDO rm -rf "$PREV_VST3" 2>/dev/null || true
    fi
    # …but the empty stage DIRECTORY goes unconditionally, and that is a
    # different question from the one above. It used to sit inside the guard, so
    # a FIRST-TIME install interrupted during the copy — no previous version, so
    # `$VST3_DEST` does not exist — removed the staged files and left
    # `.anabasis-install-stage` behind in the plug-in folder. Empty and reused by
    # the next run, so nothing broke; it simply was not the zero residue this
    # transaction claims.
    #
    # Unguarded is SAFE here, and it is worth saying why rather than trusting it:
    #   * `rmdir` removes only an EMPTY directory. It cannot take the parked
    #     `Anabasis.vst3.prev` with it — while that copy exists the call fails and
    #     `|| true` swallows it, which is exactly the protection the guard was
    #     providing.
    #   * `$STAGE_DIR` is never the plug-in directory itself. Both branches of
    #     `choose_stage_dir` return a path ENDING in `.anabasis-install-stage` —
    #     the fallback is `$1/.anabasis-install-stage`, a child of the plug-in
    #     folder, not the folder. So this can never remove `~/.vst3` or
    #     `/usr/lib/vst3`, even when both are empty.
    $SUDO rmdir "$STAGE_DIR" 2>/dev/null || true
}

arm_traps() {
    trap 'reconcile' EXIT
    trap 'reconcile; exit 130' INT
    trap 'reconcile; exit 143' TERM
    trap 'reconcile; exit 129' HUP
}

# ------------------------------------------------------- 1) current-user mode
if [ "$mode" = user ]; then
    [ -n "${HOME:-}" ] || {
        echo "error: HOME is not set, so there is no per-user location to install into." >&2
        echo "       Run 'sudo ./install.sh' for a system-wide install instead." >&2
        exit 1
    }
    VST3_DIR="$HOME/.vst3"
    BIN_DIR="$HOME/.local/bin"
    VST3_DEST="$VST3_DIR/Anabasis.vst3"

    mkdir -p "$VST3_DIR" "$BIN_DIR"
    STAGE_DIR="$(choose_stage_dir "$VST3_DIR")"
    STAGE_VST3="$STAGE_DIR/Anabasis.vst3"
    PREV_VST3="$STAGE_DIR/Anabasis.vst3.prev"
    STAGE_APP="$BIN_DIR/.Anabasis.new"

    reconcile
    arm_traps
    mkdir -p "$STAGE_DIR"

    cp -R "$VST3_SRC" "$STAGE_VST3"
    cp "$APP_SRC" "$STAGE_APP"
    chmod 755 "$STAGE_APP" "$STAGE_VST3/Contents/x86_64-linux/Anabasis.so" 2>/dev/null || true

    [ ! -e "$VST3_DEST" ] || mv "$VST3_DEST" "$PREV_VST3"
    mv "$STAGE_VST3" "$VST3_DEST"
    mv "$STAGE_APP" "$BIN_DIR/Anabasis"
    rm -rf "$PREV_VST3"
    rmdir "$STAGE_DIR" 2>/dev/null || true
    trap - EXIT INT TERM HUP

    echo "Installed (current user only - no root needed):"
    echo "  VST3       -> $VST3_DEST"
    echo "  Standalone -> $BIN_DIR/Anabasis"
    case ":${PATH:-}:" in
        *":$BIN_DIR:"*) ;;
        *) echo "Note: $BIN_DIR is not on your PATH - start the Standalone with the full path above." ;;
    esac
    if [ -e "$SYS_VST3_DIR/Anabasis.vst3" ] || [ -e "$SYS_BIN_DIR/Anabasis" ]; then
        echo "Note: an older system-wide install is still present, so Anabasis may appear"
        echo "      twice in your DAW and the older copy may load instead:"
        if [ -e "$SYS_VST3_DIR/Anabasis.vst3" ]; then echo "        $SYS_VST3_DIR/Anabasis.vst3"; fi
        if [ -e "$SYS_BIN_DIR/Anabasis" ];      then echo "        $SYS_BIN_DIR/Anabasis"; fi
        echo "      Remove it with:  sudo ./uninstall.sh"
    fi
    echo "Rescan plug-ins in your DAW to pick it up. Uninstall later with:  ./uninstall.sh"
    exit 0
fi

# --------------------------------------------------------- 2) system-wide mode
if [ "$(id -u)" -ne 0 ]; then
    command -v sudo >/dev/null 2>&1 || {
        echo "error: a system-wide install needs root, but 'sudo' is not available." >&2
        echo "       Re-run and choose 1) install for current user (~/.vst3, no root needed)," >&2
        echo "       or run this script as root:  su -c './install.sh'" >&2
        exit 1
    }
    SUDO='sudo'
    echo "System-wide installation requires administrator privileges."
    echo "You may be prompted for your password."
fi

# Only the individual install steps are elevated, never the whole script.
priv() {
    $SUDO "$@" || {
        echo "System installation failed because permission was denied." >&2
        echo "Try using a user installation or ensure sudo access is available." >&2
        exit 1
    }
}

VST3_DIR="$SYS_VST3_DIR"
BIN_DIR="$SYS_BIN_DIR"
VST3_DEST="$VST3_DIR/Anabasis.vst3"

priv mkdir -p "$VST3_DIR" "$BIN_DIR"
STAGE_DIR="$(choose_stage_dir "$VST3_DIR")"
STAGE_VST3="$STAGE_DIR/Anabasis.vst3"
PREV_VST3="$STAGE_DIR/Anabasis.vst3.prev"
STAGE_APP="$BIN_DIR/.Anabasis.new"

reconcile
arm_traps
priv mkdir -p "$STAGE_DIR"

priv cp -R "$VST3_SRC" "$STAGE_VST3"
priv cp "$APP_SRC" "$STAGE_APP"
$SUDO chmod 755 "$STAGE_APP" "$STAGE_VST3/Contents/x86_64-linux/Anabasis.so" 2>/dev/null || true

[ ! -e "$VST3_DEST" ] || priv mv "$VST3_DEST" "$PREV_VST3"
priv mv "$STAGE_VST3" "$VST3_DEST"
priv mv "$STAGE_APP" "$BIN_DIR/Anabasis"
$SUDO rm -rf "$PREV_VST3" 2>/dev/null || true
$SUDO rmdir "$STAGE_DIR" 2>/dev/null || true
trap - EXIT INT TERM HUP

echo "Installed (system-wide, all users):"
echo "  VST3       -> $VST3_DEST"
echo "  Standalone -> $BIN_DIR/Anabasis"
# The mirror of the per-user branch's warning, and the more important direction
# of the two: a per-user copy is scanned as well as the system one, so leaving
# one behind means Anabasis appears twice and the per-user (older) copy is the
# one many DAWs load.
#
# Finding it needs the INVOKING user's home, not `$HOME` — under `sudo` that is
# root's. There are TWO ways to reach a system-wide install and they answer that
# question differently, which is what the branch below is for:
#
#   * `sudo ./install.sh` — the whole script runs as root, `$HOME` is root's, and
#     `$SUDO_USER` names the real account. Their home comes from the passwd
#     database rather than from `~$SUDO_USER`, which POSIX sh does not expand.
#   * `./install.sh` answered with 2 — the script runs as the USER and elevates
#     only individual operations (`priv`), so `$HOME` is already the right home
#     and `$SUDO_USER` is NEVER SET. Naming it here was an unguarded expansion
#     under `set -u`: the install completed, the traps were already cleared, and
#     then the courtesy note aborted the script with "SUDO_USER: parameter not
#     set" and a non-zero exit — reporting failure for a run that had succeeded.
#     `user_as` carries the answer instead, empty on this path because the
#     account is the one already reading the message.
user_home=''
user_as=''
if [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != root ]; then
    if command -v getent >/dev/null 2>&1; then
        user_home=$(getent passwd "$SUDO_USER" 2>/dev/null | cut -d: -f6)
    fi
    [ -n "$user_home" ] || user_home="/home/$SUDO_USER"
    user_as=" as $SUDO_USER"
elif [ "$(id -u)" -ne 0 ]; then
    user_home="${HOME:-}"          # elevated per-operation; $HOME is still ours
fi

# The subshell is the structural half of that fix, and it is worth more than the
# variable was. Everything above this line has already happened: the payload is
# installed and `trap -` has released the rollback. Nothing after that point is
# allowed to decide the exit status, and a `set -u` abort is NOT containable by
# `|| true` on a plain command or a function — the shell exits regardless. It is
# containable inside a subshell, which is what this is. An advisory that cannot
# run correctly should print nothing and cost nothing, not fail the install.
(
    if [ -n "$user_home" ]; then
        _uv="$user_home/.vst3/Anabasis.vst3"
        _ua="$user_home/.local/bin/Anabasis"
        if [ -e "$_uv" ] || [ -e "$_ua" ]; then
            echo "Note: a per-user install is also present, so Anabasis may appear twice in"
            echo "      your DAW and the per-user copy may load instead of this one:"
            if [ -e "$_uv" ]; then echo "        $_uv"; fi
            if [ -e "$_ua" ]; then echo "        $_ua"; fi
            echo "      Remove it${user_as} with:  ./uninstall.sh"
        fi
    fi
) || true

echo "Rescan plug-ins in your DAW to pick it up. Uninstall later with:  sudo ./uninstall.sh"

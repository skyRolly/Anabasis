#!/bin/sh
# Anabasis Linux uninstaller. Two modes, matching the installer:
#
#   1) Current user (default, no root)  ~/.vst3/Anabasis.vst3, ~/.local/bin/Anabasis
#   2) System-wide  (needs root)        /usr/lib/vst3/Anabasis.vst3, /usr/local/bin/Anabasis
#
# Running this script as root (sudo ./uninstall.sh) removes the system-wide
# installation without asking. Your presets and settings are always kept.
set -eu

SYS_VST3_DIR="/usr/lib/vst3"
SYS_BIN_DIR="/usr/local/bin"
SYS_VST3="$SYS_VST3_DIR/Anabasis.vst3"
SYS_APP="$SYS_BIN_DIR/Anabasis"

# Only a system-wide uninstall sets this; a per-user uninstall never uses sudo.
SUDO=''

# ---------------------------------------------------------------- mode choice
mode=user

if [ "$(id -u)" -eq 0 ]; then
    mode=system
    echo "Running as root - removing the system-wide installation."
elif [ -t 0 ]; then
    cat <<'EOF'
Anabasis Linux Uninstaller

Choose which installation to remove:

1) Current user installation (recommended)
   ~/.vst3

2) System-wide installation
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
    echo "Not running on a terminal - removing the current user's installation (the default)."
fi

removed=0

# Clears temporary files an interrupted install may have left behind. Only the
# exact names the installer creates are removed; nothing else is touched.
# THIS LIST MUST MATCH `choose_stage_dir` IN `install.sh`, candidate for
# candidate. It briefly did not: a `/var/tmp` staging candidate was added there
# and not here, so an interrupted install's staged bundle survived a deliberate
# uninstall — against what the CHANGELOG promises. That candidate is gone (it
# carried a local root exploit; the reasoning is at its removal site), which
# makes the two lists identical again. Add a candidate to one and it belongs in
# the other in the same change set.
remove_install_scratch() {          # $1 = plug-in directory, $2 = bin directory
    for _scratch in "${1%/*}/.anabasis-install-stage" \
                    "$1/.anabasis-install-stage" \
                    "$1/.anabasis-probe" \
                    "$2/.Anabasis.new"
    do
        if [ -e "$_scratch" ]; then
            # Never fatal. Under `set -e` an `rm && echo` list is one command whose
            # status is tested, so a scratch file this user cannot remove (owned by
            # a root-mode install, an immutable parent) would abort the script —
            # after the plug-in itself had already been removed, and with nothing
            # printed to say why. Leftover scratch is cosmetic; a half-finished
            # uninstall is not.
            if $SUDO rm -rf "$_scratch" 2>/dev/null; then
                echo "removed leftover installer file $_scratch"
                # …and it COUNTS as having removed something. Without this the
                # run printed "removed leftover installer file …" and "nothing to
                # remove" in the same output — two lines contradicting each
                # other, the second being the one a user reads to decide whether
                # the uninstall did anything.
                removed=1
            else
                echo "note: could not remove $_scratch (left in place)" >&2
            fi
        fi
    done
}

# ------------------------------------------------------- 1) current-user mode
if [ "$mode" = user ]; then
    [ -n "${HOME:-}" ] || {
        echo "error: HOME is not set, so there is no per-user installation to remove." >&2
        echo "       Run 'sudo ./uninstall.sh' to remove a system-wide install instead." >&2
        exit 1
    }
    VST3="$HOME/.vst3/Anabasis.vst3"
    APP="$HOME/.local/bin/Anabasis"

    if [ -d "$VST3" ]; then rm -rf "$VST3"; echo "removed $VST3"; removed=1; fi
    if [ -f "$APP" ];  then rm -f  "$APP";  echo "removed $APP";  removed=1; fi
    remove_install_scratch "$HOME/.vst3" "$HOME/.local/bin"

    [ "$removed" -eq 1 ] || echo "nothing to remove for this user (a system-wide install is removed with:  sudo ./uninstall.sh)"
    echo "Per-user presets/settings (if any) are kept; remove them manually if desired."
    exit 0
fi

# --------------------------------------------------------- 2) system-wide mode
if [ "$(id -u)" -ne 0 ]; then
    command -v sudo >/dev/null 2>&1 || {
        echo "error: removing a system-wide install needs root, but 'sudo' is not available." >&2
        echo "       Run this script as root instead:  su -c './uninstall.sh'" >&2
        exit 1
    }
    SUDO='sudo'
    echo "Removing the system-wide installation requires administrator privileges."
    echo "You may be prompted for your password."
fi

# Only the individual removals are elevated, never the whole script.
priv() {
    $SUDO "$@" || {
        echo "System uninstall failed because permission was denied." >&2
        echo "Ensure sudo access is available, or run this script as root." >&2
        exit 1
    }
}

if [ -d "$SYS_VST3" ]; then priv rm -rf "$SYS_VST3"; echo "removed $SYS_VST3"; removed=1; fi
if [ -f "$SYS_APP" ];  then priv rm -f  "$SYS_APP";  echo "removed $SYS_APP";  removed=1; fi
remove_install_scratch "$SYS_VST3_DIR" "$SYS_BIN_DIR"

[ "$removed" -eq 1 ] || echo "nothing to remove system-wide (a per-user install is removed with:  ./uninstall.sh)"
echo "Per-user presets/settings (if any) are kept; remove them manually if desired."

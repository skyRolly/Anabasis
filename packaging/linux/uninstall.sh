#!/bin/sh
# Anabasis Linux uninstaller. Two modes, matching the installer:
#
#   1) Current user (default, no root)  ~/.vst3/Anabasis.vst3, ~/.local/bin/Anabasis
#   2) System-wide  (needs root)        /usr/lib/vst3/Anabasis.vst3, /usr/local/bin/Anabasis
#
# Running this script as root (sudo ./uninstall.sh) removes the system-wide
# installation without asking. Your presets and settings are always kept, and so
# is a plug-in copy parked by an interrupted install unless --discard-parked
# says otherwise.
set -eu

# `--user` / `--system` MIRROR `install.sh`, and the reason is the same one: the
# prompt below is gated on `[ -t 0 ]`, so a piped or CI invocation silently took
# the per-user default with no way to ask for anything else. Adding them to the
# installer alone would have left the asymmetry in the worse place — a script
# that can INSTALL system-wide non-interactively but can only REMOVE per-user.
# `sudo ./uninstall.sh` reaches the system-wide path, but that is the
# whole-script-as-root route the installer's own comments argue against as a
# different transaction.
#
# `--discard-parked` is the opt-in for the ONE thing this script will not throw
# away on its own: a `Anabasis.vst3.prev` parked in the installer's scratch
# directory. See `remove_install_scratch`.
discard_parked=0
requested=''
for arg in "$@"; do
    case "$arg" in
        --user|--system)
            # Same refusal as the installer: a contradiction has less intent to
            # infer than a typo, and the two differ in destination AND privilege.
            _want=${arg#--}
            if [ -n "$requested" ] && [ "$requested" != "$_want" ]; then
                echo "error: --user and --system name different installations; pass one." >&2
                exit 1
            fi
            requested=$_want ;;
        --discard-parked) discard_parked=1 ;;
        -h|--help)
            echo "usage: ./uninstall.sh [--user|--system] [--discard-parked]"
            echo "  --user            remove the current user's install (~/.vst3) - the default"
            echo "  --system          remove the system-wide install (/usr/lib/vst3) - needs root"
            echo "  --discard-parked  also delete a plug-in copy parked by an"
            echo "                    interrupted install (default: keep it)"
            echo "Repeating an option is accepted; --user and --system together are not."
            echo "With no mode option, an interactive run asks; a non-interactive one"
            echo "removes the current user's install."
            exit 0 ;;
        *)
            echo "error: unrecognised option '$arg'" >&2
            echo "usage: ./uninstall.sh [--user|--system] [--discard-parked]" >&2
            exit 1 ;;
    esac
done

SYS_VST3_DIR="/usr/lib/vst3"
SYS_BIN_DIR="/usr/local/bin"
SYS_VST3="$SYS_VST3_DIR/Anabasis.vst3"
SYS_APP="$SYS_BIN_DIR/Anabasis"

# Only a system-wide uninstall sets this; a per-user uninstall never uses sudo.
SUDO=''

# ---------------------------------------------------------------- mode choice
mode=user

if [ "$(id -u)" -eq 0 ]; then
    # `--user` as root is refused for the installer's reason: which home `$HOME`
    # names under sudo depends on the sudoers configuration, so the target would
    # not be predictable — and here the unpredictable operation is a DELETION.
    if [ "$requested" = user ]; then
        echo "error: --user cannot be combined with running as root: which home directory" >&2
        echo "       \$HOME names under sudo depends on the sudoers configuration, so the" >&2
        echo "       files removed would not be predictable." >&2
        echo "       Re-run without sudo to remove a per-user install." >&2
        exit 1
    fi
    mode=system
    echo "Running as root - removing the system-wide installation."
elif [ -n "$requested" ]; then
    mode="$requested"
    if [ "$mode" = system ]; then
        echo "Removing the system-wide installation (--system)."
    else
        echo "Removing the current user's installation (--user)."
    fi
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
    echo "Pass --system to remove the system-wide installation instead."
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
        # `-L` FIRST, for the reason `stage_dir_is_adoptable` was fixed for: `-e`
        # follows the link, so a DANGLING symlink wearing a scratch name reads as
        # "absent" and is never cleaned. The installer never creates such a link,
        # so this is residue rather than a defect — but the two lists were using
        # different tests for the same question, and that is how one of them ends
        # up wrong later.
        if [ -L "$_scratch" ] || [ -e "$_scratch" ]; then
            # Never fatal. Under `set -e` an `rm && echo` list is one command whose
            # status is tested, so a scratch file this user cannot remove (owned by
            # a root-mode install, an immutable parent) would abort the script —
            # after the plug-in itself had already been removed, and with nothing
            # printed to say why. Leftover scratch is cosmetic; a half-finished
            # uninstall is not.
            # KEEP WHAT ONLY THE INSTALLER CAN RESTORE. An install interrupted in
            # the two-rename window parks the working plug-in here as
            # `Anabasis.vst3.prev`; `install.sh` puts it back, and no other path
            # does. An uninstall should uninstall — but "uninstall" cannot
            # sensibly mean "destroy the copy that is standing in for the
            # installation you are removing", so this one case is refused and
            # named instead of swept up with the scratch.
            if $SUDO test -d "$_scratch/Anabasis.vst3.prev" && [ "$discard_parked" -eq 0 ]; then
                # REFUSED, NOT ANNOUNCED. Until 2026-08-14 this printed a note and
                # deleted the copy anyway — the script establishing that it knew
                # the copy was valuable, and then destroying it, in the same
                # breath. The user this reaches is the one whose plug-in has just
                # vanished mid-install and who reaches for the uninstaller to
                # clean up: exactly the person for whom `Anabasis.vst3.prev` is
                # the ONLY working copy, and for whom the note arrives after the
                # fact. `install.sh` restores it; nothing else can.
                #
                # STDOUT, not stderr, and the stream choice is the point: a user
                # redirecting stderr, or reading a log that splits the streams,
                # must still see why this directory was left behind.
                echo "note: $_scratch holds a saved copy of your previous Anabasis,"
                echo "      parked there by an interrupted install. It is the only copy of that"
                echo "      version, so it is being KEPT rather than removed."
                echo "      Run './install.sh' to put it back, or re-run this script with"
                echo "      --discard-parked to delete it along with the rest of the scratch."
                # Deliberately does NOT set `removed`: nothing was removed here,
                # and claiming otherwise would make the summary line disagree with
                # what is still on disk.
                continue
            fi
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

#!/bin/sh
# Anabasis Linux installer. Two modes — `--user` / `--system`, or chosen
# interactively when neither is given and stdin is a terminal:
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
# EXPLICIT SELECTION EXISTS BECAUSE THE PROMPT CANNOT ALWAYS BE ASKED. The gate
# on it is `[ -t 0 ]`, so any non-interactive invocation — a provisioning script,
# a CI step, `sh install.sh < /dev/null` — silently took the per-user default,
# and the ONLY non-interactive route to a system-wide install was to run the
# whole script as root. That is not the same install: `sudo ./install.sh` makes
# the entire transaction root's, where answering `2` at the prompt elevates one
# operation at a time and is the path the staging guards are written for.
requested=''
for arg in "$@"; do
    case "$arg" in
        --user)   requested=user   ;;
        --system) requested=system ;;
        -h|--help)
            echo "usage: ./install.sh [--user|--system]"
            echo "  --user    install for the current user (~/.vst3) - the default"
            echo "  --system  install for all users (/usr/lib/vst3) - needs root"
            echo "With neither option, an interactive run asks; a non-interactive one"
            echo "installs for the current user."
            exit 0 ;;
        *)
            echo "error: unrecognised option '$arg'" >&2
            echo "usage: ./install.sh [--user|--system]" >&2
            exit 1 ;;
    esac
done

mode=user

if [ "$(id -u)" -eq 0 ]; then
    # `--user` AS ROOT IS REFUSED RATHER THAN GUESSED. The per-user branch writes
    # to `$HOME`, and under `sudo` whose home that is depends on the sudoers
    # configuration (`always_set_home`, `env_keep`) — so the same command would
    # install to `/root/.vst3` on one machine and the caller's folder, root-owned,
    # on another. Neither is what the flag was asking for.
    if [ "$requested" = user ]; then
        echo "error: --user cannot be combined with running as root: which home directory" >&2
        echo "       \$HOME names under sudo depends on the sudoers configuration, so the" >&2
        echo "       install location would not be predictable." >&2
        echo "       Re-run without sudo for a per-user install." >&2
        exit 1
    fi
    mode=system
    echo "Running as root - installing system-wide (for all users)."
elif [ -n "$requested" ]; then
    mode="$requested"
    if [ "$mode" = system ]; then
        echo "Installing system-wide (--system)."
    else
        echo "Installing for the current user (--user)."
    fi
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
    echo "Pass --system to install for all users instead."
fi

# ------------------------------------------------------------ install helpers
# Both modes install the same way; only the elevation and the summary differ.

# Chooses a temporary working directory for the new version, kept out of the
# folder your DAW scans whenever that is possible.
#
# TWO CANDIDATES, and the absence of a third is the important part.
#
#   1. `<parent of the plug-in dir>/.anabasis-install-stage` — for a system
#      install that is `/usr/lib`, for a per-user one it is `$HOME`. Same
#      filesystem as the destination by construction, and in both cases a
#      directory only the installing identity can create.
#   2. `<the plug-in dir>/.anabasis-install-stage` — always same-filesystem, at
#      the cost of sitting inside the folder the DAW scans.
#
# `/var/tmp` WAS TRIED HERE AND IS DELIBERATELY GONE. It was added to keep a
# system install from leaving a dot-directory in `/usr/lib`, and it opened a
# local root exploit: `/var/tmp` is world-writable (mode 1777), the staging name
# is a literal, so any unprivileged user can create
# `/var/tmp/.anabasis-install-stage` first and own it. Two ways that pays off,
# and the first needs no race at all — the recovery scan below adopts whichever
# candidate holds an `Anabasis.vst3.prev`, and `reconcile` then does
# `mv "$PREV_VST3" "$VST3_DEST"` AS ROOT, so a planted `.prev` is installed
# system-wide. The second is a straightforward TOCTOU: root copies the payload
# into an attacker-owned directory and renames it out later, and the sticky bit
# on `/var/tmp` protects only `/var/tmp` itself, never the attacker's own
# subdirectory. Tidiness in `/usr/lib` is not worth either. If this is
# revisited, the answer is `mktemp -d` under a root-owned parent, never a
# fixed name in a shared writable tree.
#
# THE PROBE IS NOT TIDINESS, it is the transaction. The final step is `mv`, and a
# `mv` is atomic only WITHIN a filesystem; across one it degrades to copy-then-
# delete, which is precisely the "partially written plug-in" window this whole
# script exists to close. So a candidate is accepted only if a hard link can be
# made from it INTO the destination directory — the cheapest true test of "same
# filesystem" there is.

# An EXISTING staging directory is adopted only if this run could have created
# it: owned by the identity whose writes land in the destination, not a symlink,
# and writable by nobody else. Fails CLOSED — anything it cannot determine is
# refused, including `stat` being unavailable.
stage_dir_is_adoptable() {      # $1 = candidate, $2 = uid that must own it
    # THE SYMLINK TEST COMES FIRST, and the order is the difference between the
    # guard doing its job and something else covering for it. `-e` follows the
    # link, so a DANGLING symlink named `.anabasis-install-stage` reads as
    # "absent" and this returned 0 — the explicit symlink rejection below never
    # ran. The install still failed closed, but by accident downstream:
    # `mkdir -p` on a dangling symlink returns EEXIST and `make_stage_dir`
    # returns 1, which is indistinguishable from a full disk and says nothing
    # about a symlink. `-L` does not follow, so testing it first rejects both
    # kinds of symlink for the stated reason.
    [ -L "$1" ] && return 1     # a symlink could aim the whole transaction elsewhere
    [ -e "$1" ] || return 0     # absent — we are about to create it ourselves
    [ -d "$1" ] || return 1     # a file wearing the directory's name
    _st=$(LC_ALL=C stat -c '%u %a' "$1" 2>/dev/null) || return 1
    [ "${_st%% *}" = "$2" ] || return 1
    # BOTH PATTERNS ARE ANCHORED TO THE LAST TWO DIGITS, which is what makes them
    # correct for a mode string of either length. `%a` prints three digits
    # normally but FOUR when any special bit is set — `2700` for set-gid — so a
    # test written against a fixed offset would read the wrong digits. Anchored
    # at the end, `2770` is still rejected as group-writable and `2700` is still
    # accepted; only the special-bit digit itself goes unexamined.
    #
    # That omission is deliberate, and the reason is the same self-refusal trap
    # the `chmod` in `make_stage_dir` exists for: set-gid is INHERITED by `mkdir`
    # from the parent directory and survives `chmod 700`, verified — under a
    # set-gid parent, `make_stage_dir`'s own directory lands at `2700`. A guard
    # that rejected four-digit modes would refuse the directory it had just
    # created. Nor is there anything to reject on the merits: on a 0700
    # directory, set-gid grants no access the owner bits do not already deny,
    # sticky only narrows who may unlink, and set-uid has no meaning for a
    # directory on Linux. The permission bits are the whole question, and those
    # are the two digits actually tested.
    case "${_st##* }" in
        *[2367][0-7]) return 1 ;;   # group-writable
        *[2367])      return 1 ;;   # other-writable
    esac
    return 0
}

# Creates a staging directory with a mode `stage_dir_is_adoptable` will accept.
# The explicit `chmod` is the whole point: `mkdir` inherits the umask, so on a
# user-private-group system (`umask 002`) the directory lands at 775 —
# group-writable — and the NEXT run refuses the very directory THIS run created,
# then falls through to whatever comes after. A test its own creator fails is not
# a test, it is a trap; this makes the creator's output satisfy it.
make_stage_dir() {              # $1 = directory
    $SUDO mkdir -p "$1" 2>/dev/null || return 1
    $SUDO chmod 700 "$1" 2>/dev/null || return 1
    return 0
}

# The same-filesystem test, factored out because BOTH the recovery scan and the
# creation loop need it. The final step of the install is `mv`, and `mv` is
# atomic only WITHIN a filesystem; across one it degrades to copy-then-delete,
# which is the partially-written-bundle window this whole script exists to close.
# A hard link from the candidate INTO the destination directory is the cheapest
# true test of "same filesystem" there is.
stage_dir_is_same_filesystem() {    # $1 = candidate, $2 = probe path in the destination
    $SUDO rm -f "$1/.probe" "$2" 2>/dev/null || true
    if $SUDO touch "$1/.probe" 2>/dev/null && $SUDO ln "$1/.probe" "$2" 2>/dev/null; then
        $SUDO rm -f "$1/.probe" "$2" 2>/dev/null || true
        return 0
    fi
    $SUDO rm -f "$1/.probe" "$2" 2>/dev/null || true
    return 1
}

# Prints the staging directory and returns 0, or returns NON-ZERO having printed
# nothing. The caller must check, and both callers do.
#
# IT USED TO END IN AN UNGATED `printf`, which is worth spelling out because the
# comment above it claimed the opposite. When both candidates were refused it
# returned the second one anyway — so a directory just judged untrustworthy was
# used as the staging area, `reconcile` then `mv`-ed whatever `Anabasis.vst3.prev`
# it contained into place AS ROOT, and root copied the payload into a directory
# the script had already decided it did not own. That is INC-006's adoption path
# reached through the fallback instead of through `/var/tmp`, in a function
# documenting itself as failing closed. It now fails closed for real: no
# trustworthy candidate means no install.
choose_stage_dir() {            # $1 = plug-in directory; prints the stage directory
    _probe="$1/.anabasis-probe"
    $SUDO rm -rf "$_probe" 2>/dev/null || true
    # Who must own a staging directory for it to be ours: root when the payload
    # is written with elevation (either `sudo ./install.sh` or `priv`), the
    # invoking user otherwise.
    if [ "$mode" = system ]; then _owner=0; else _owner=$(id -u); fi

    # Adding a candidate here means adding it to THREE other places or something
    # silently stops matching: the creation loop below, `stage_dir_advice` (the
    # message both callers print when this returns non-zero), and
    # `remove_install_scratch` in `uninstall.sh` — the last of which, if missed,
    # lets an interrupted install survive an uninstall.
    #
    # RECOVERY FIRST: an earlier install may have parked the previous version in
    # either candidate, and whichever holds it must be the one used, or
    # `reconcile` cannot put it back. Gated on the SAME two tests as creation —
    # trust, then same-filesystem. The probe used to be skipped here on the
    # reasoning that a `.prev` can only have been parked by a run that already
    # passed it; true today, and an induction that a third candidate or an
    # externally-created `.prev` would break silently. Running it is cheaper than
    # relying on that.
    #
    # The `.prev` test comes FIRST because it is the selective one, and because
    # a candidate that holds a parked copy AND fails a trust test is worth SAYING
    # SO ABOUT rather than skipping in silence. That combination needs the layout
    # or the directory's ownership to have changed between the interrupted run
    # and this one — `$HOME` and `$HOME/.vst3` becoming separate filesystems, say
    # — so it is narrow, but it is the ONE path on which the interrupted-install
    # guarantee in `INSTALL.txt` does not hold: the parked copy is neither
    # restored nor removed by this run, and only `uninstall.sh` clears it. Silent
    # would make that indistinguishable from a clean run.
    for _c in "${1%/*}/.anabasis-install-stage" "$1/.anabasis-install-stage"; do
        # Elevated for the same reason as `reconcile`'s test: on the system-wide
        # path this candidate is root-owned at 0700 and the script is running as
        # the invoking user, so a plain `[ -d ]` reads FALSE on a parked bundle
        # that exists. Unelevated, this loop never adopts the candidate holding
        # the previous version AND never prints the warning below — the recovery
        # and its diagnostic went blind together.
        $SUDO test -d "$_c/Anabasis.vst3.prev" || continue
        if stage_dir_is_adoptable "$_c" "$_owner" \
           && stage_dir_is_same_filesystem "$_c" "$_probe"; then
            printf '%s\n' "$_c"
            return 0
        fi
        echo "warning: a previous version of the plug-in is parked in" >&2
        echo "         $_c/Anabasis.vst3.prev," >&2
        echo "         but that directory is no longer usable for staging, so this run cannot" >&2
        echo "         put it back. It is left untouched. To recover it, make that directory" >&2
        echo "         usable again (it must be a real directory you own, not writable by" >&2
        echo "         others) and re-run this installer. './uninstall.sh' DELETES it rather" >&2
        echo "         than restoring it, so run it second, not first." >&2
    done

    for _c in "${1%/*}/.anabasis-install-stage" "$1/.anabasis-install-stage"; do
        stage_dir_is_adoptable "$_c" "$_owner" || continue
        make_stage_dir "$_c" || continue
        if stage_dir_is_same_filesystem "$_c" "$_probe"; then
            printf '%s\n' "$_c"
            return 0
        fi
        # `rmdir`, never `rm -rf`. Two things this can name, and the second is
        # the one worth being precise about: a directory `make_stage_dir` just
        # created, OR one that already existed and passed
        # `stage_dir_is_adoptable` — in which case this run also `chmod 700`'d it
        # on the way in, and now removes it if it is empty. So the reject path
        # can take away a directory this run did not create. `rmdir` declines a
        # non-empty one, so nothing with contents in it is ever lost, which is
        # the INC-006 lesson and the reason this is not `rm -rf`; what is left is
        # an empty directory of the right name, owned by the right identity,
        # which is indistinguishable from one we made.
        $SUDO rmdir "$_c" 2>/dev/null || true
    done
    return 1
}

# The message BOTH callers print when `choose_stage_dir` returns non-zero. It
# lives here so the advice cannot name a different set of directories from the
# one the loops above actually try — the per-user branch named only the first
# candidate for exactly as long as this text was written out twice, which made
# the advice unfollowable whenever the SECOND candidate was the blocker.
stage_dir_advice() {            # $1 = plug-in directory
    echo "error: no usable staging directory beside $1." >&2
    echo "       One is present but is a symlink, owned by another account, or writable" >&2
    echo "       by others — this installer will not stage a privileged copy there." >&2
    echo "       Inspect and remove whichever of these exists, then re-run:" >&2
    echo "         ${1%/*}/.anabasis-install-stage" >&2
    echo "         $1/.anabasis-install-stage" >&2
}

# Puts back the previous version if an install was interrupted, then clears the
# temporary files. Runs once before installing and again if the script is stopped.
reconcile() {
    # `$SUDO test -d`, NOT `[ -d ]`, and this is the difference between the
    # transaction working and losing the user's plug-in.
    #
    # THE STAGING DIRECTORY IS ROOT-OWNED AT MODE 0700 on the system-wide path,
    # because `make_stage_dir` creates it that way on purpose (INC-006's lesson).
    # The WRITES into it are all elevated. The TESTS were not — and there are two
    # ways to reach a system install: `sudo ./install.sh`, where the whole script
    # is root and every test succeeds, and the mode prompt answered `2`, where the
    # script runs as the INVOKING USER and elevates one operation at a time. On
    # that second path — the documented normal one — `[ -d "$PREV_VST3" ]` stats a
    # path inside a directory the tester cannot open, gets EACCES, and reads
    # FALSE even though the parked bundle is right there.
    #
    # What that cost, reproduced end to end before this fix: SIGINT inside the
    # two-rename window, the trap fires, this test declines to restore, the line
    # below removes the staged replacement, and `/usr/lib/vst3/Anabasis.vst3` is
    # GONE while the only good copy sits unreachable in
    # `/usr/lib/.anabasis-install-stage/`. The CHANGELOG says only a signal no
    # handler can catch can leave the plug-in absent in that window; a catchable
    # Ctrl-C did too. Silent, because the directory itself still stats fine (only
    # `/usr/lib` needs to be readable for that), so `stage_dir_is_adoptable`
    # passed and nothing anywhere reported a problem.
    #
    # Every test on a path INSIDE `$STAGE_DIR` is elevated for this reason. `$SUDO`
    # is empty on the per-user path, where it degrades to plain `test` and the
    # directory is the user's own.
    if [ ! -e "$VST3_DEST" ] && $SUDO test -d "$PREV_VST3"; then
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
    STAGE_DIR="$(choose_stage_dir "$VST3_DIR")" || {
        stage_dir_advice "$VST3_DIR"
        exit 1
    }
    STAGE_VST3="$STAGE_DIR/Anabasis.vst3"
    PREV_VST3="$STAGE_DIR/Anabasis.vst3.prev"
    STAGE_APP="$BIN_DIR/.Anabasis.new"

    # ORDER IS LOAD-BEARING, and neither line says so on its own. `reconcile`
    # ends in an UNCONDITIONAL `rmdir "$STAGE_DIR"`, so this pre-install call
    # DELETES the directory `choose_stage_dir` just created and validated — and
    # `make_stage_dir` below is what puts it back. Moving `make_stage_dir` above
    # `reconcile`, or dropping it on the reasonable-looking assumption that
    # `choose_stage_dir` already created the directory, leaves `cp -R` writing
    # into a path that does not exist.
    reconcile
    arm_traps
    make_stage_dir "$STAGE_DIR" || {
        echo "error: could not create the staging directory $STAGE_DIR." >&2
        exit 1
    }

    cp -R "$VST3_SRC" "$STAGE_VST3"
    cp "$APP_SRC" "$STAGE_APP"
    chmod 755 "$STAGE_APP" "$STAGE_VST3/Contents/x86_64-linux/Anabasis.so" 2>/dev/null || true

# PARK ONLY INTO AN EMPTY SLOT. `reconcile`'s restore is `|| true`, so a failed
# `mv "$PREV_VST3" "$VST3_DEST"` leaves the parked bundle where it is — and then
# this line would move the destination INTO the surviving `.prev` DIRECTORY
# rather than aside, because `mv a b` with `b` an existing directory means "put
# `a` inside `b`". The next `rm -rf "$PREV_VST3"` clears the lot, so nothing is
# left half-installed; what breaks is the MEANING of `.prev`, which `reconcile`
# reads as "the previous version, restorable". Refusing to park onto a non-empty
# slot keeps that name true, and an install that cannot park is an install that
# should stop rather than proceed on a false assumption.
#
# HOW NARROW THIS IS, since a reader who traces the control flow will conclude it
# is dead code and be almost right: `reconcile` ran two lines above, and its last
# act when `$VST3_DEST` exists is `rm -rf "$PREV_VST3"`. So on every path where
# the outer `if` is entered, the slot has just been cleared. The one way through
# is that `rm -rf` FAILING — it is `2>/dev/null || true`, so an immutable
# attribute, a mount point or a read-only remount leaves `.prev` standing and
# says nothing. That is the case this refusal is for: not a race, but a silent
# failure two lines up. Keep it.
    if [ -e "$VST3_DEST" ]; then
        [ ! -e "$PREV_VST3" ] || {
            echo "error: $PREV_VST3 already holds a parked copy, so this run cannot set the" >&2
            echo "       previous version aside. Re-run './install.sh' to let it reconcile," >&2
            echo "       or remove that directory once you are sure of its contents." >&2
            exit 1
        }
        mv "$VST3_DEST" "$PREV_VST3"
    fi
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
STAGE_DIR="$(choose_stage_dir "$VST3_DIR")" || {
    stage_dir_advice "$VST3_DIR"
    exit 1
}
STAGE_VST3="$STAGE_DIR/Anabasis.vst3"
PREV_VST3="$STAGE_DIR/Anabasis.vst3.prev"
STAGE_APP="$BIN_DIR/.Anabasis.new"

# Same load-bearing order as the per-user branch above: `reconcile` ends in an
# unconditional `rmdir "$STAGE_DIR"`, so it removes the directory
# `choose_stage_dir` created and `make_stage_dir` below recreates it. Do not
# reorder these two, and do not drop the `make_stage_dir` call.
reconcile
arm_traps
make_stage_dir "$STAGE_DIR" || { echo "System installation failed: cannot create $STAGE_DIR" >&2; exit 1; }

priv cp -R "$VST3_SRC" "$STAGE_VST3"
priv cp "$APP_SRC" "$STAGE_APP"
$SUDO chmod 755 "$STAGE_APP" "$STAGE_VST3/Contents/x86_64-linux/Anabasis.so" 2>/dev/null || true

# Same rule as the per-user branch: never park onto an occupied slot (see there,
# including why this is not the dead code it reads as).
if [ -e "$VST3_DEST" ]; then
    $SUDO test ! -e "$PREV_VST3" || {
        echo "error: $PREV_VST3 already holds a parked copy, so this run cannot set the" >&2
        echo "       previous version aside. Re-run the installer to let it reconcile," >&2
        echo "       or remove that directory once you are sure of its contents." >&2
        exit 1
    }
    priv mv "$VST3_DEST" "$PREV_VST3"
fi
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
# The subshell is the structural half of that fix, and it is worth more than the
# variable was. Everything above this line has already happened: the payload is
# installed and `trap -` has released the rollback. Nothing after that point is
# allowed to decide the exit status, and a `set -u` abort is NOT containable by
# `|| true` on a plain command or a function — the shell exits regardless. It is
# containable inside a subshell, which is what this is. An advisory that cannot
# run correctly should print nothing and cost nothing, not fail the install.
#
# EVERYTHING THE ADVISORY NEEDS IS COMPUTED INSIDE IT, and that placement is the
# point rather than a tidiness. The `user_home` lookup used to sit outside, past
# the point of no return, and was safe only by accident: `getent … | cut -d: -f6`
# takes the PIPELINE's status from `cut`, which exits 0 even when `getent` fails
# or matches nothing. Remove `cut` from the image and that assignment returns 127,
# `set -e` fires, and a completed install exits non-zero — the exact failure this
# subshell was introduced to prevent, reintroduced two lines above it. Inside, the
# invariant is structural: nothing here can decide the exit status, whatever any
# of these tools does.
(
    user_home=''
    user_as=''
    if [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != root ]; then
        if command -v getent >/dev/null 2>&1; then
            user_home=$(getent passwd "$SUDO_USER" 2>/dev/null | cut -d: -f6)
        fi
        [ -n "$user_home" ] || user_home="/home/$SUDO_USER"
        user_as=" as $SUDO_USER"
    elif [ "$(id -u)" -ne 0 ]; then
        user_home="${HOME:-}"      # elevated per-operation; $HOME is still ours
    fi

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

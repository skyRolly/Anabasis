#!/usr/bin/env bash
# ============================================================================
#  Anabasis -- pinned Clang toolchain from the official LLVM apt repository
#
#  Usage: scripts/setup-llvm-apt.sh <clang-major>
#
#  Installs exactly ONE Clang major -- compiler, lld and the sanitizer runtime
#  -- from apt.llvm.org, LLVM's own Debian/Ubuntu distribution channel. Safe to
#  re-run.
#
#  WHY THIS EXISTS AT ALL. Ubuntu's own archives stop at clang-20 for noble
#  (24.04), which is what `ubuntu-latest` resolves to. The upstream STABLE
#  release is 23.x, and apt.llvm.org is where upstream ships it for noble.
#  Staying on the stock archive would mean holding the warning gate and the
#  sanitizer host THREE majors behind upstream because of a packaging boundary,
#  which is a distribution fact about Ubuntu rather than a fact about this
#  project.
#
#  `apt.llvm.org/llvm.sh` IS NOT THE VERSION ORACLE, and this header used to cite
#  it as one -- "the same source llvm.sh uses, whose own CURRENT_LLVM_STABLE
#  reads 22". That variable lags the release train: on 2026-08-30, with 23.1.0
#  released and `llvm-toolchain-noble-23` published, it still read 22. The
#  release record is `releases.llvm.org` and `llvm.org`; llvm.sh is one consumer
#  of the same archive, and citing a consumer's default as evidence of what
#  upstream has released is how a pin gets talked out of a move it should make.
#
#  WHY NOT `llvm.sh`. That script decides the version itself from its own
#  notion of stable, installs a broad toolchain, and can add more than one
#  suite. `ANABASIS_CLANG_VERSION` in `.github/workflows/build.yml` is this
#  repository's single authority for the major, so the suite is added by hand,
#  the major comes from the caller, and exactly three packages are installed.
#  The mechanism follows the version; it never chooses it.
#
#  FAIL-CLOSED, unlike the ccache install beside it. ccache is an optimization
#  the jobs fall back from with a `::warning::`; the pinned Clang IS the job.
#  `linux` BUILDS THE SHIPPED ARTIFACT with it (ADR-0032) and runs the
#  FIRST-PARTY WARNING GATE -- which fails on ANY first-party warning, so the
#  compiler is what defines the bar it is measured against -- and links the
#  plugin under `-flto` with the matching `lld-<major>`; `sanitizers` links
#  `libclang-rt-<major>`; `realtime` needs the RTSan runtime from the same
#  package, which Ubuntu's own archives do not ship at all for this major. A
#  partial install must stop the job, never let it quietly proceed with the
#  image's default `clang`. So: `set -e`, every step checked, and
#  `clang-<major> --version` asserted at the end. If
#  apt.llvm.org is unreachable the job fails saying so, which is the honest
#  outcome -- falling back to a different compiler would change the warning
#  surface and read as a project problem instead.
#
#  Transient network failure is absorbed rather than ignored: the key fetch and
#  the index update both retry. A repeatable failure still fails.
#
#  Network domains this script needs: apt.llvm.org (suite + signing key), the
#  Ubuntu apt mirrors, and github.com -- the last one for the RELEASE-TAG
#  assertion at the foot of this script, which is a `git ls-remote` and clones
#  nothing.
#
#  WHY A RELEASE-TAG ASSERTION EXISTS (0.2.7, ADR-0037). Every check here used to
#  be MAJOR-only, and a major-only check cannot see the difference between a
#  released compiler and a release-branch build that is not released yet.
#  Measured, not hypothesised: on 2026-08-30 the noble suite for major 23 carried
#  `1:23.1.0~++20260818083557+55feb0a3b6b7-…`, whose upstream commit sits AFTER
#  the `llvmorg-23.1.0-rc3` tag and BEFORE the release commit, still carrying
#  `LLVM_VERSION_SUFFIX -rc3`. Debian's packaging drops that suffix, so
#  `clang-23 --version`, `__clang_version__` and the dpkg version ALL read a
#  clean `23.1.0`. Nothing local distinguishes it. The one thing that does is the
#  upstream COMMIT the package names in its own version string, compared against
#  the commit the release tag points at -- which is what this asserts.
#
#  LOCALLY: if the machine already carries an apt.llvm.org source added by
#  `llvm.sh` or by hand, apt refuses the pair with "Conflicting values set for
#  option Signed-By" and names both keyring paths. That message is better than
#  anything this script could add, so it is left to apt -- delete the other
#  `/etc/apt/sources.list.d/*llvm*` file and re-run. CI starts from a clean
#  image, where the case cannot arise.
# ============================================================================
set -euo pipefail

MAJOR="${1:-}"
case "$MAJOR" in
    ''|*[!0-9]*)
        echo "setup-llvm-apt: usage: $0 <clang-major>  (got '${MAJOR}')" >&2
        exit 2
        ;;
esac

SUDO=""
if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi

# The suite name is per-distribution, so it is READ from the machine rather than
# assumed: apt.llvm.org publishes `llvm-toolchain-<codename>-<major>`, and a
# hard-coded `noble` would silently point at the wrong suite the day
# `ubuntu-latest` moves. An unknown codename fails here instead of installing
# something unintended.
CODENAME="$(. /etc/os-release && echo "${VERSION_CODENAME:-}")"
if [ -z "$CODENAME" ]; then
    echo "setup-llvm-apt: cannot read VERSION_CODENAME from /etc/os-release" >&2
    exit 1
fi

KEYRING="/etc/apt/keyrings/llvm-apt.gpg"
SOURCE="/etc/apt/sources.list.d/llvm-toolchain-${MAJOR}.list"
SUITE="llvm-toolchain-${CODENAME}-${MAJOR}"

echo "setup-llvm-apt: installing clang-${MAJOR} from apt.llvm.org (${SUITE})"

# The key is FETCHED rather than vendored. Vendoring it would remove one network
# dependency and add a worse one: a committed key that upstream later rotates
# fails the build with a signature error and needs a repository commit to fix,
# on a schedule nobody here controls. It arrives over HTTPS from the same host
# that serves the packages, and `signed-by=` scopes it to this one suite so it
# can never validate anything else on the machine.
$SUDO install -d -m 0755 /etc/apt/keyrings
curl -fsSL --retry 3 --retry-delay 2 https://apt.llvm.org/llvm-snapshot.gpg.key \
    | $SUDO gpg --dearmor --yes -o "$KEYRING"
if [ ! -s "$KEYRING" ]; then
    echo "setup-llvm-apt: the LLVM signing key did not arrive at ${KEYRING}" >&2
    exit 1
fi
$SUDO chmod 0644 "$KEYRING"

# The key is PINNED BY IDENTITY, which is what makes fetching it defensible.
# HTTPS proves the bytes came from apt.llvm.org; this proves the keyring holds
# the key this repository decided to trust AND NOTHING ELSE. If upstream ever
# rotates it, the job fails here with a specific message and a human decides --
# which is the outcome you want from a third-party package source, rather than
# silently trusting whatever key the host served this morning.
#
# WHY THE ASSERTION IS ON PRIMARY KEYS AND NOT ON A SUBSTRING. `signed-by=`
# trusts EVERY key in the keyring for that suite, so "the expected fingerprint
# appears somewhere in this file" is not the guarantee above: a served blob
# carrying the genuine key CONCATENATED with another one satisfies it, and both
# get trusted. The list of primary fingerprints must therefore equal the pinned
# one exactly -- one line, that value. A second primary key makes it two lines
# and fails.
#
# It counts `pub:` records rather than `fpr:` records because the genuine key
# has TWO fingerprints: the primary and one subkey. An "exactly one fingerprint"
# test would reject the real key, so this walks each `pub:` and takes the `fpr:`
# that follows it. Subkeys are deliberately not enumerated -- a subkey is bound
# to its primary by a signature only the primary's holder can make, so it adds
# no identity beyond the primary this pins.
LLVM_KEY_FPR="6084F3CF814B57C1CF12EFD515CF4D18AF4F7421"
# `|| true` so the ASSERTION decides, not `set -e`: on an unreadable or corrupt
# keyring `gpg` exits non-zero, and under `pipefail` that would abort the script
# at this assignment with no message at all. Failing is right; failing silently
# is not, so the empty result falls through to the diagnostic below.
LLVM_PRIMARY_FPRS="$(gpg --show-keys --with-colons "$KEYRING" 2>/dev/null \
    | awk -F: '/^pub:/ { primary = 1; next } /^fpr:/ { if (primary) { print $10; primary = 0 } }' \
    || true)"
if [ "$LLVM_PRIMARY_FPRS" != "$LLVM_KEY_FPR" ]; then
    echo "setup-llvm-apt: ${KEYRING} does not hold exactly the expected LLVM signing key" >&2
    echo "setup-llvm-apt: expected exactly one primary key, ${LLVM_KEY_FPR}" >&2
    echo "setup-llvm-apt:            (Sylvestre Ledru - Debian LLVM packages)" >&2
    echo "setup-llvm-apt: got primary key(s):" >&2
    printf '%s\n' "${LLVM_PRIMARY_FPRS:-(none)}" | sed 's/^/setup-llvm-apt:   /' >&2
    exit 1
fi

echo "deb [signed-by=${KEYRING}] https://apt.llvm.org/${CODENAME}/ ${SUITE} main" \
    | $SUDO tee "$SOURCE" > /dev/null

# Scoped to the new source: a full `apt-get update` here would also re-fetch
# every Ubuntu index, so an unrelated mirror hiccup would present as an LLVM
# failure. Retries absorb a transient 5xx from apt.llvm.org itself.
$SUDO apt-get update -y -o Acquire::Retries=3 \
    -o Dir::Etc::sourcelist="$SOURCE" -o Dir::Etc::sourceparts="-" \
    -o APT::Get::List-Cleanup="0"

# All three, in one transaction, for every Clang job. Only the sanitizer jobs
# link a sanitizer runtime, but installing the same set from one code path is
# worth more than the few seconds it saves elsewhere: two divergent package
# lists is how one job ends up with a toolchain the other does not have.
# `lld-${MAJOR}` is not optional for the release build -- `CMakeLists.txt` selects
# lld for Clang-on-Linux as a CORRECTNESS requirement (it carries the full
# reasoning at the `-fuse-ld=lld` block), and the Clang build links the plugin
# under `-flto`, which needs lld to resolve archive members the single GNU ld
# scan passed over. `libclang-rt-${MAJOR}-dev` carries both the ASan/UBSan
# runtimes the `sanitizers` job links and `libclang_rt.rtsan`, which the
# `realtime` job cannot run without.
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y \
    "clang-${MAJOR}" "lld-${MAJOR}" "libclang-rt-${MAJOR}-dev"

# The assertion, not a courtesy print: if the suite resolved to something else,
# or the install half-succeeded, the job stops here rather than at a confusing
# baseline mismatch two steps later.
"clang-${MAJOR}" --version
"clang-${MAJOR}" --version | grep -qE "clang version ${MAJOR}\." || {
    echo "setup-llvm-apt: clang-${MAJOR} reports a version that is not ${MAJOR}.x" >&2
    exit 1
}
command -v "ld.lld-${MAJOR}" >/dev/null 2>&1 || [ -x "/usr/lib/llvm-${MAJOR}/bin/ld.lld" ] || {
    echo "setup-llvm-apt: lld-${MAJOR} is missing; the LTO plugin link needs it" >&2
    exit 1
}

# THE RELEASE-TAG ASSERTION. See the header for the measurement that motivated it.
#
# The package version carries the upstream commit it was built from:
#
#   1:22.1.8~++20260714014902+ca7933e47d3a-1~exp1~20260714135019.80
#     ^ upstream version              ^ upstream commit
#
# and `llvmorg-<version>` peels to the commit the release was cut at. A RELEASED
# build matches; a branch build taken before (or after) the release commit does
# not, whatever its version string says.
#
# `git ls-remote` is the whole network cost -- no clone, no history, one ref.
PKG_VER="$(dpkg-query -W -f='${Version}' "clang-${MAJOR}")"
UPSTREAM_VER="${PKG_VER#*:}"          # drop the epoch
UPSTREAM_VER="${UPSTREAM_VER%%~*}"    # 22.1.8
BUILT_SHA="${PKG_VER##*+}"            # ca7933e47d3a-1~exp1~...
BUILT_SHA="${BUILT_SHA%%-*}"          # ca7933e47d3a

if [ -z "$UPSTREAM_VER" ] || [ -z "$BUILT_SHA" ]; then
    echo "setup-llvm-apt: cannot read an upstream version and commit out of '${PKG_VER}'" >&2
    exit 1
fi

# Retried like the other network steps; a repeatable failure still fails, because
# "could not check" and "checked and it is a release" must not look the same.
TAG_REFS=""
for _attempt in 1 2 3; do
    TAG_REFS="$(git ls-remote --tags https://github.com/llvm/llvm-project \
        "llvmorg-${UPSTREAM_VER}" "llvmorg-${UPSTREAM_VER}^{}" 2>/dev/null || true)"
    [ -n "$TAG_REFS" ] && break
    sleep 2
done
if [ -z "$TAG_REFS" ]; then
    echo "setup-llvm-apt: could not reach github.com to resolve llvmorg-${UPSTREAM_VER}" >&2
    echo "setup-llvm-apt: refusing to assume the installed clang-${MAJOR} is a released build" >&2
    exit 1
fi

# Prefer the PEELED ref: an annotated tag's own object id is not the commit id,
# and every LLVM release tag is annotated. The unpeeled line is the fallback for
# a lightweight tag, where the two are the same thing.
TAG_SHA="$(printf '%s\n' "$TAG_REFS" | awk '/\^\{\}$/ { print $1; found = 1 } END { if (!found) exit 1 }' \
    || printf '%s\n' "$TAG_REFS" | awk 'NR==1 { print $1 }')"

case "$TAG_SHA" in
    "${BUILT_SHA}"*) ;;
    *)
        echo "setup-llvm-apt: clang-${MAJOR} is NOT a build of the ${UPSTREAM_VER} release" >&2
        echo "setup-llvm-apt:   package built from : ${BUILT_SHA}" >&2
        echo "setup-llvm-apt:   llvmorg-${UPSTREAM_VER} is at: ${TAG_SHA}" >&2
        echo "setup-llvm-apt: apt.llvm.org is serving a release-BRANCH build for this major." >&2
        echo "setup-llvm-apt: the reported version drops any -rcN suffix, so nothing local says so." >&2
        echo "setup-llvm-apt: wait for the suite to rebuild at the tag, or pin a major that is released." >&2
        exit 1
        ;;
esac

echo "setup-llvm-apt: clang-${MAJOR} is the ${UPSTREAM_VER} release (built from ${BUILT_SHA})"

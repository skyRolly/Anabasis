#!/bin/bash
# Builds the Anabasis macOS installer package (.pkg) from the CI-staged
# customer payload. The installer set is a port from the sibling product,
# deferred past v0.1.0 by OQ-007 — which is why this file lands at 0.1.1 and
# not at P6. Unsigned: Developer ID signing + notarization are NOT part of this
# port. (The sibling's header names its own tracking tickets for the installer
# and for the later signing/notarization work; Anabasis has no equivalent
# ticket IDs, so they are dropped rather than invented.)
#
#   build-pkg.sh <staged-dir> <version> <output.pkg>
#
# <staged-dir> must contain Anabasis.vst3 / Anabasis.component / Anabasis.app
# exactly as validated by the build.yml package step (stripped, universal,
# ad-hoc signed). pkgbuild archives the payload as-is — permissions and the
# signed bundle layout are preserved. Files installed by Installer.app carry
# no quarantine attribute, so a pkg install needs no xattr step afterwards
# (unlike the zip, whose extracted bundles inherit quarantine).
# The distribution offers COMPONENT SELECTION (VST3 / AU / Standalone app,
# all pre-selected — the default is a full install; Installer.app's
# "Customize" button exposes the checkboxes).
#
# Every component is built NON-RELOCATABLE and NON-VERSION-CHECKED, so each
# install copies the payload to its declared destination unconditionally —
# see build_component() below (INC-005).
#
# NOTICE and THIRD_PARTY_LICENSES.md are not in <staged-dir> and not in this
# package: since ADR-0021 they ship as version-named RELEASE-PAGE assets, which
# is where RELEASE_POLICY.md's amended "accompany the distribution" requirement
# is satisfied for every carrier — this .pkg, the Inno installer and the three
# zips alike. Only the three bundles become component roots below, so no fourth
# component is needed to carry them.
set -euo pipefail

DIST=${1:?usage: build-pkg.sh <staged-dir> <version> <output.pkg>}
VERSION=${2:?usage: build-pkg.sh <staged-dir> <version> <output.pkg>}
OUT=${3:?usage: build-pkg.sh <staged-dir> <version> <output.pkg>}

for b in Anabasis.vst3 Anabasis.component Anabasis.app; do
  [ -d "$DIST/$b" ] || { echo "error: $DIST/$b missing (staged payload incomplete)" >&2; exit 1; }
done

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# Sets a component-plist key, adding it if pkgbuild's analysis did not emit it.
#   plist_put <plist> <array-index> <key> <type> <value>
plist_put () {
  /usr/libexec/PlistBuddy -c "Set :$2:$3 $5" "$1" 2>/dev/null \
    || /usr/libexec/PlistBuddy -c "Add :$2:$3 $4 $5" "$1"
}

# Builds one component package, with RELOCATION and VERSION CHECKING OFF.
#
# Why: pkgbuild's default component plist marks every bundle it finds as
# relocatable. At install time the Installer then looks the bundle identifier
# up in the system's receipt/Spotlight database and, if a copy is found
# ANYWHERE, writes the payload over THAT copy instead of the declared
# --install-location. Move /Applications/Anabasis.app to the Desktop (or drag
# it to the Trash, which is still on disk and still indexed) and re-run the
# installer: it reports success, updates the copy it found, and /Applications
# stays empty. Version checking fails the same way from the other end — a
# bundle already at or above the package version is skipped rather than
# overwritten. Both are switched off here, so a component always writes its
# payload to its declared destination, from the payload alone, with no
# reference to previous installation state (INC-005).
#
# BundleOverwriteAction=upgrade (pkgbuild's default, pinned explicitly) makes
# that write a replacement rather than a merge, so no file from an older
# install survives inside the new bundle.
build_component () {
  local id=$1 bundle=$2 dest=$3 out=$4
  local root="$WORK/root-$id" plist="$WORK/component-$id.plist" scripts="$WORK/scripts-$id"

  mkdir -p "$root" "$scripts"
  cp -R "$DIST/$bundle" "$root/"

  # Patch what pkgbuild itself analysed (rather than hand-writing the plist), so
  # RootRelativeBundlePath always matches by construction. The loop walks the
  # top-level array entries; a bundle nested inside another is reported under the
  # parent's ChildBundles rather than as its own entry, and is not patched here —
  # nor does it need to be, since only top-level bundles appear in PackageInfo's
  # membership lists, and the assertions below would fail the build if one ever did.
  pkgbuild --analyze --root "$root" "$plist"
  local i=0
  while /usr/libexec/PlistBuddy -c "Print :$i:RootRelativeBundlePath" "$plist" >/dev/null 2>&1; do
    plist_put "$plist" "$i" BundleIsRelocatable    bool   false
    plist_put "$plist" "$i" BundleIsVersionChecked bool   false
    plist_put "$plist" "$i" BundleOverwriteAction  string upgrade
    i=$((i + 1))
  done
  [ "$i" -gt 0 ] || { echo "error: pkgbuild --analyze found no bundle in $bundle" >&2; exit 1; }

  # Fail-closed backstop: if the payload is not at the destination when the
  # component finishes, the install reports FAILURE instead of success. It runs
  # only for components the user actually selected.
  cat > "$scripts/postinstall" <<POST
#!/bin/sh
# Installed-state check for $bundle (\$3 = destination volume).
set -eu
DEST="\${3:-/}"
DEST="\${DEST%/}$dest/$bundle"
[ -e "\$DEST" ] || { echo "Anabasis: $bundle is missing from \$DEST after install" >&2; exit 1; }
exit 0
POST
  chmod +x "$scripts/postinstall"

  pkgbuild --root "$root" --identifier "com.rollytech.anabasis.$id" \
           --version "$VERSION" --install-location "$dest" \
           --component-plist "$plist" --scripts "$scripts" "$out"

  # Registered only on success, so the probe below can never claim to have
  # covered a component whose package did not build.
  COMPONENT_IDS="${COMPONENT_IDS:-} $id"
}

# One component package per install destination. `COMPONENT_IDS` accumulates as
# they are built rather than being written out a second time, because the
# liveness probe below iterates it and must cover exactly what was built — a
# hand-maintained copy that fell behind would silently shrink the proof while
# the assertion loop kept its reach.
COMPONENT_IDS=''
build_component vst3 Anabasis.vst3      "/Library/Audio/Plug-Ins/VST3"       "$WORK/AnabasisVST3.pkg"
build_component au   Anabasis.component "/Library/Audio/Plug-Ins/Components" "$WORK/AnabasisAU.pkg"
build_component app  Anabasis.app       "/Applications"                      "$WORK/AnabasisApp.pkg"

# Distribution definition, written explicitly (a synthesized one hard-wires
# customize="never", which hides the component checkboxes). customize="allow"
# keeps the default flow a FULL install (all three choices start selected)
# while the Installer's "Customize" button lets the user deselect components.
# enable_localSystem pins the system-wide destinations (/Library, /Applications).
cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Anabasis ${VERSION}</title>
    <options customize="allow" require-scripts="false"/>
    <domains enable_localSystem="true"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="app"/>
    </choices-outline>
    <choice id="vst3" title="VST3 Plug-in" start_selected="true"
            description="Installs Anabasis.vst3 into /Library/Audio/Plug-Ins/VST3 (REAPER, Ableton Live, Cubase, Bitwig, ...).">
        <pkg-ref id="com.rollytech.anabasis.vst3"/>
    </choice>
    <choice id="au" title="AU Plug-in" start_selected="true"
            description="Installs Anabasis.component into /Library/Audio/Plug-Ins/Components (Logic Pro, GarageBand, ...).">
        <pkg-ref id="com.rollytech.anabasis.au"/>
    </choice>
    <choice id="app" title="Standalone Application" start_selected="true"
            description="Installs Anabasis.app into /Applications.">
        <pkg-ref id="com.rollytech.anabasis.app"/>
    </choice>
    <pkg-ref id="com.rollytech.anabasis.vst3" version="${VERSION}">AnabasisVST3.pkg</pkg-ref>
    <pkg-ref id="com.rollytech.anabasis.au" version="${VERSION}">AnabasisAU.pkg</pkg-ref>
    <pkg-ref id="com.rollytech.anabasis.app" version="${VERSION}">AnabasisApp.pkg</pkg-ref>
</installer-gui-script>
EOF
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$OUT"

# Self-check: the package must expand, contain all three components, and keep
# component selection enabled (customize="allow" with every choice pre-selected
# — i.e. the default remains a full install). `installer -pkginfo` is a query
# flag (no root needed) — installing is what needs sudo, not this. (The sibling
# cites its own CI run as the evidence for that; Anabasis has no such run yet,
# so the citation is dropped rather than transplanted.)
installer -pkginfo -pkg "$OUT"
pkgutil --expand "$OUT" "$WORK/expanded"
for id in vst3 au app; do
  grep -Rq "com.rollytech.anabasis.$id" "$WORK/expanded" \
    || { echo "error: component com.rollytech.anabasis.$id missing from $OUT" >&2; exit 1; }
done
grep -q 'customize="allow"' "$WORK/expanded/Distribution" \
  || { echo "error: $OUT lost customize=\"allow\" (component selection disabled)" >&2; exit 1; }
[ "$(grep -c 'start_selected="true"' "$WORK/expanded/Distribution")" -eq 3 ] \
  || { echo "error: $OUT does not pre-select all three components (default must be a full install)" >&2; exit 1; }

# Install-state independence (INC-005): no component may be relocatable or
# version-checked, each must keep BundleOverwriteAction=upgrade, and each must
# carry its postinstall check. In PackageInfo each of those states is a
# membership list — `<relocate>` for BundleIsRelocatable, `<bundle-version>` for
# BundleIsVersionChecked, `<upgrade-bundle>` for the overwrite action — emitted
# self-closing when empty, so it is the `<element><bundle` pair that means a
# bundle is listed.
#
# Those assertions match on element names, and a name that pkgbuild never writes
# is an assertion that always passes — precisely the silent-success shape this
# block exists to prevent. Proving the name is producible is still not enough:
# it does not show the list TRACKS the component-plist key it stands for. So the
# mapping is established by a controlled A/B — a component's payload is packaged
# twice, differing only in the keys build_component() sets. Each list must appear
# with pkgbuild's defaults and disappear when its key is switched off. If that
# stops holding, the build stops here rather than shipping assertions that
# cannot fire.
#
# The A/B runs ONCE PER COMPONENT, and that is the point rather than thoroughness
# for its own sake. `pkgbuild --analyze` decides its defaults per bundle, and a
# `.vst3` or a `.component` is not a `.app`. Proving the mapping on the app alone
# while the assertion loop below applies it to all three would leave the two
# plug-in components asserted by patterns never shown to be producible for THEM:
# if either were analysed differently — not marked relocatable by default, say —
# its `<relocate>` assertion would pass by finding nothing. That is the same
# silent success as INC-005 itself, one level up.
#
# THE COST, COUNTED PROPERLY. An earlier version of this comment said "three
# extra `pkgbuild` runs", which undercounts by half: the probe runs TWO arms per
# component — `defaults` and `patched` — over three components, so it is SIX extra
# `pkgbuild` invocations and six extra `pkgutil --expand` passes over the full
# universal payload, on top of the three real component builds. Nine `pkgbuild`
# runs where a naive build does three, paid on every packaging run rather than
# only when a plist changes: the packaging step's runtime roughly triples. That is
# the price of the assertions below being falsifiable rather than decorative, and
# it is worth knowing before someone times the job and calls it a regression.
#
# The probe fixes `--install-location` for every component instead of using the
# real destination: the membership lists come from the component plist and from
# what `--analyze` made of the ROOT, and `pkgbuild` never shows the install
# location to either. Varying it would suggest it mattered; varying only the
# payload is what makes this a controlled comparison.
probe_info() {                  # $1 = component id, $2 = tag; rest -> pkgbuild
  _id=$1; _tag=$2; shift 2
  pkgbuild --root "$WORK/root-$_id" --identifier com.rollytech.anabasis.probe \
           --version "$VERSION" --install-location "/Applications" \
           "$@" "$WORK/probe-$_id-$_tag.pkg" >/dev/null
  pkgutil --expand "$WORK/probe-$_id-$_tag.pkg" "$WORK/probe-$_id-$_tag-x"
  tr -d ' \n\t' < "$WORK/probe-$_id-$_tag-x/PackageInfo"
}
probe_fail() {                  # $1 = component id, $2 = tag, $3 = message
  # READ THIS BEFORE BLAMING build_component(). A failure here does NOT mean the
  # package is wrong — it means the probe could no longer establish that the
  # assertions below can fire. The likeliest cause by far is TOOLCHAIN DRIFT: a
  # macOS or Xcode update changing what `pkgbuild --analyze` emits by default, or
  # renaming a PackageInfo element. That fails the BUILD rather than shipping a
  # package whose guarantees are unverified, which is the intended direction, but
  # it means this job is deliberately sensitive to the platform tools.
  #
  # If it fails for ONE component only, that is the more interesting outcome and
  # the reason the probe is per-component: the platform treats that bundle type
  # differently, and the assertion loop's claim about it was never true.
  #
  # If that happens: compare the PackageInfo dumped below against the element
  # names the loop matches, update both together, and record the change in
  # POSTMORTEMS.md INC-005 — the incident this whole block exists for.
  echo "error: [$1] $3" >&2
  echo "---- PackageInfo of probe '$1/$2' ----" >&2
  cat "$WORK/probe-$1-$2-x/PackageInfo" >&2
  exit 1
}

# WHAT THE FIRST macOS RUN OF THIS PROBE ACTUALLY FOUND, because it changed the
# design and the finding is more useful than the assertion was.
#
# `pkgbuild --analyze` does NOT mark a `.vst3` or a `.component` relocatable. Its
# synthesized plist emits `<relocate/>` EMPTY for them, and `relocatable="false"`
# on the `pkg-info` element itself; only the `.app` is marked relocatable, which
# is consistent with relocation being a Launch-Services notion about
# applications. So INC-005's relocation half was only ever a hazard for the
# Standalone — and, more to the point here, the `<relocate><bundle` assertion is
# UNFALSIFIABLE for the two plug-in components: it passes by finding nothing.
#
# That is the exact silent-success shape this block exists to prevent, so a
# missing list is neither ignored nor treated as failure. It is classified:
#
#   * PRESENT with defaults  → the list is LIVE for this bundle type. It must
#     disappear when the key is switched off, which proves the assertion below
#     tracks the key it is named for.
#   * ABSENT with defaults   → NOT APPLICABLE to this bundle type. The platform
#     already gives the safe answer; the assertion below still runs and is still
#     correct, but it is evidence about pkgbuild rather than about us, and it is
#     logged as such so nobody later reads it as proof that our plist patching
#     works for that component.
#
# A component where NOTHING is live would prove nothing at all, and that does
# fail: it would mean the whole A/B told us nothing about that payload.
PROBED=0
for pid in $COMPONENT_IDS; do
  PROBE_ON=$(probe_info "$pid" defaults)
  PROBE_OFF=$(probe_info "$pid" patched --component-plist "$WORK/component-$pid.plist")

  live=0
  for pattern in '<relocate><bundle' '<bundle-version><bundle'; do
    case "$PROBE_ON" in
      *"$pattern"*)
        # Live: it must track the key.
        case "$PROBE_OFF" in
          *"$pattern"*) probe_fail "$pid" patched "'$pattern' survives with its component-plist key switched off — that list does not track the key it is asserted for" ;;
          *) live=$((live + 1)) ;;
        esac
        ;;
      *)
        echo "note: [$pid] pkgbuild's defaults emit no '$pattern' for this bundle type;" \
             "the assertion keyed on it is a platform default, not proof of our plist patching" >&2
        # Still has to be absent once patched — a weaker claim, but a real one.
        case "$PROBE_OFF" in
          *"$pattern"*) probe_fail "$pid" patched "'$pattern' appeared only AFTER patching — our component plist is turning ON what it exists to turn off" ;;
        esac
        ;;
    esac
  done

  # `<upgrade-bundle>` gets the SAME per-component classification as the two
  # above, and the reason is the finding that produced that classification in the
  # first place: `pkgbuild --analyze` does not treat every bundle type alike, and
  # this block once assumed it did for `<relocate>`.
  #
  # The two arms are not the same kind of claim, so they are not asserted the
  # same way:
  #
  #   * OFF / patched is OUR component plist. `BundleOverwriteAction=upgrade` is
  #     written there explicitly, so its absence is a real regression in the thing
  #     this script exists to guarantee — HARD FAIL, on every component.
  #   * ON / defaults is PKGBUILD'S default for this bundle type. Demanding it
  #     asserts something about Apple's tool, not about us. If a future macOS
  #     stops defaulting it for a `.vst3` or a `.component` — exactly what turned
  #     out to be true of `<relocate>` for those two types — the build would fail
  #     outright on a toolchain change rather than on a defect here. Logged, not
  #     failed.
  case "$PROBE_OFF" in
    *'<upgrade-bundle><bundle'*) ;;
    *) probe_fail "$pid" patched \
           "no '<upgrade-bundle><bundle' — BundleOverwriteAction=upgrade is not reaching PackageInfo from OUR component plist" ;;
  esac
  case "$PROBE_ON" in
    *'<upgrade-bundle><bundle'*) ;;
    *) echo "note: [$pid] pkgbuild's defaults emit no '<upgrade-bundle><bundle' for this bundle" \
            "type; our component plist supplies it, which the patched arm above proves" >&2 ;;
  esac

  [ "$live" -gt 0 ] \
    || probe_fail "$pid" defaults "no membership list is falsifiable for this bundle type — the A/B establishes nothing about this component"
  PROBED=$((PROBED + 1))
done

# Count first — a loop over an empty `find` would pass every assertion below
# without executing one of them. The probe count is checked against the SAME
# number for the same reason: the assertions may not be applied to more
# components than the A/B established them for.
INFOS=$(find "$WORK/expanded" -name PackageInfo | wc -l | tr -d ' ')
[ "$INFOS" -eq 3 ] \
  || { echo "error: expected 3 component PackageInfo files in $OUT, found $INFOS" >&2; exit 1; }
[ "$PROBED" -eq "$INFOS" ] \
  || { echo "error: the plist->PackageInfo mapping was proved for $PROBED component(s) but $INFOS are asserted below" >&2; exit 1; }
while IFS= read -r info; do
  flat=$(tr -d ' \n\t' < "$info")
  case "$flat" in
    *'<relocate><bundle'*)
      echo "error: $info marks a bundle relocatable — a re-install could write over a moved copy" >&2; exit 1 ;;
  esac
  case "$flat" in
    *'<bundle-version><bundle'*)
      echo "error: $info marks a bundle version-checked — a re-install could skip the destination" >&2; exit 1 ;;
  esac
  case "$flat" in
    *'<upgrade-bundle><bundle'*) ;;
    *) echo "error: $info does not carry BundleOverwriteAction=upgrade — a re-install could merge into the old bundle" >&2; exit 1 ;;
  esac
  case "$flat" in
    *'postinstall'*) ;;
    *) echo "error: $info carries no postinstall installed-state check" >&2; exit 1 ;;
  esac
done < <(find "$WORK/expanded" -name PackageInfo)

echo "built $OUT"

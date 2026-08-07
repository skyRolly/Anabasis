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

# One component package per install destination.
mkdir -p "$WORK/vst3" "$WORK/au" "$WORK/app"
cp -R "$DIST/Anabasis.vst3"      "$WORK/vst3/"
cp -R "$DIST/Anabasis.component" "$WORK/au/"
cp -R "$DIST/Anabasis.app"       "$WORK/app/"

pkgbuild --root "$WORK/vst3" --identifier com.rollytech.anabasis.vst3 \
         --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/VST3" \
         "$WORK/AnabasisVST3.pkg"
pkgbuild --root "$WORK/au"   --identifier com.rollytech.anabasis.au \
         --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/Components" \
         "$WORK/AnabasisAU.pkg"
pkgbuild --root "$WORK/app"  --identifier com.rollytech.anabasis.app \
         --version "$VERSION" --install-location "/Applications" \
         "$WORK/AnabasisApp.pkg"

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
    <choice id="vst3" title="VST3 plug-in" start_selected="true"
            description="Installs Anabasis.vst3 into /Library/Audio/Plug-Ins/VST3 (REAPER, Ableton Live, Cubase, Bitwig, ...).">
        <pkg-ref id="com.rollytech.anabasis.vst3"/>
    </choice>
    <choice id="au" title="AU plug-in" start_selected="true"
            description="Installs Anabasis.component into /Library/Audio/Plug-Ins/Components (Logic Pro, GarageBand, ...).">
        <pkg-ref id="com.rollytech.anabasis.au"/>
    </choice>
    <choice id="app" title="Standalone application" start_selected="true"
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
echo "built $OUT"

# Installing Anabasis

This guide is for **users and testers** — how to install a build you were given.
Maintainers building the project: [`docs/procedures/BUILD.md`](../procedures/BUILD.md).

Anabasis v0.1.x is distributed as **plain ZIP archives**, one per platform, produced and
validated by the project's CI — there are no installers yet (they arrive with the first
commercial release, together with code-signing and notarization). Extracting a ZIP shows
the payload directly — there is no wrapper folder inside.

| Platform | Archive | Contains |
|---|---|---|
| Linux | `Anabasis-Linux.zip` | `Anabasis.vst3` + `Anabasis` (Standalone) |
| Windows | `Anabasis-Windows.zip` | `Anabasis.vst3` + `Anabasis.exe` (Standalone) |
| macOS | `Anabasis-macOS.zip` | `Anabasis.vst3` + `Anabasis.component` (AU) + `Anabasis.app` — universal (Apple Silicon + Intel) |

The plug-in is 64-bit only. Third-party attribution is **not inside the archives**: `NOTICE`
and `THIRD_PARTY_LICENSES.md` are published on the release page beside the downloads,
version-named — `Anabasis-<version>-NOTICE.txt` and
`Anabasis-<version>-THIRD_PARTY_LICENSES.md`. Take them from there if you need them, and pass
them on with the build if you forward it. (The release page is the single carrier for every
download route — the two installers never carried them either.)

> **Two things to expect from a pre-release build:**
>
> 1. **Security warnings.** The binaries are not code-signed (Windows) or notarized
>    (macOS) yet. Your OS will warn once; the workarounds below are the normal ones for
>    unsigned software.
> 2. **Executable permissions (Linux and macOS).** The CI packaging pipeline does not
>    preserve Unix file modes inside the zip, so after extracting you may need to restore
>    the executable bit — the exact `chmod` commands are in each platform's section. If a
>    freshly copied plug-in refuses to load or the Standalone won't start, this is the
>    first thing to check.

---

## Linux

Copy `Anabasis.vst3` (the whole folder) into `/usr/lib/vst3/` and the `Anabasis`
standalone executable into `/usr/local/bin/` (both need root), then restore the
executable bits:

```sh
sudo mkdir -p /usr/lib/vst3
sudo cp -R Anabasis.vst3 /usr/lib/vst3/
sudo cp Anabasis /usr/local/bin/
sudo chmod +x /usr/local/bin/Anabasis
sudo chmod +x /usr/lib/vst3/Anabasis.vst3/Contents/x86_64-linux/Anabasis.so
```

Rescan plug-ins in your DAW (REAPER: *Options → Preferences → Plug-ins → VST →
Re-scan*; Bitwig: *Settings → Locations*; Ardour: *Preferences → Plugins*).

**Troubleshooting**

- **"Permission denied" or the DAW can't load the plug-in** — the `chmod` steps above.
- **DAW doesn't find it** — check `/usr/lib/vst3` is in the DAW's VST3 search path (it is
  by default in REAPER/Bitwig/Ardour), then rescan.
- **Standalone needs audio** — a working ALSA/JACK/PipeWire setup; pick the device in the
  app's audio settings.

---

## Windows

Extract the zip, then (administrator approval needed for both):

1. Copy the **whole `Anabasis.vst3` folder** into `C:\Program Files\Common Files\VST3\`.
2. Create `C:\Program Files\Anabasis\` and copy `Anabasis.exe` (Standalone) into it.

Rescan plug-ins in your DAW (REAPER: *Preferences → Plug-ins → VST → Re-scan*;
Ableton: *Options → Preferences → Plug-Ins*; FL Studio: *Plugin Manager*;
Cubase: *Studio → VST Plug-in Manager*).

**Troubleshooting**

- **Plug-in doesn't appear** — make sure you copied the entire `Anabasis.vst3` *folder*,
  not a file from inside it, then rescan.
- **SmartScreen blocks the Standalone** — *More info → Run anyway* (expected until the
  binaries are code-signed).
- **32-bit host** — Anabasis is 64-bit only and won't show up in 32-bit DAWs.

---

## macOS

Copy what you need, restore the executable bits, then remove macOS's quarantine flag —
the bundles are ad-hoc-signed but not notarized, so without the `xattr` step the DAW will
refuse to load them:

```sh
sudo mkdir -p /Library/Audio/Plug-Ins/VST3 /Library/Audio/Plug-Ins/Components
sudo cp -R "Anabasis.vst3"      /Library/Audio/Plug-Ins/VST3/
sudo cp -R "Anabasis.component" /Library/Audio/Plug-Ins/Components/
sudo cp -R "Anabasis.app"       /Applications/
sudo chmod +x /Library/Audio/Plug-Ins/VST3/Anabasis.vst3/Contents/MacOS/Anabasis
sudo chmod +x /Library/Audio/Plug-Ins/Components/Anabasis.component/Contents/MacOS/Anabasis
sudo chmod +x /Applications/Anabasis.app/Contents/MacOS/Anabasis
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/Anabasis.vst3
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/Anabasis.component
sudo xattr -dr com.apple.quarantine /Applications/Anabasis.app
```

Rescan in your DAW. Logic Pro / GarageBand use the AU and validate it automatically on
launch — you can check from Terminal with `auval -v aufx Anbs RTec` ("PASS" means Logic
will see it).

**Troubleshooting**

- **"Cannot be opened because it is from an unidentified developer"** — right-click →
  Open (once), or *System Settings → Privacy & Security → Open Anyway*.
- **Plug-in doesn't load after copying** — you skipped the `xattr` quarantine step or the
  `chmod` step above; run them and rescan.
- **Logic/GarageBand don't see it** — they only use the AU (`.component`); check
  `auval -v aufx Anbs RTec`.

**Uninstall:** delete the three installed items (Finder asks for your password for the
two `/Library/…` ones).

---

## Verifying a download

Checksums and a release manifest are part of the tag-triggered release pipeline, which is
planned but not in place for v0.1.x — a build you receive today comes straight from CI. The
plug-in's exact version and build number are on its About screen (click the **ANABASIS**
title), so you can always confirm *what* you are running.

---

## Next steps

Installed? The user manual's **[Quick start](USER_MANUAL.md#2-quick-start)** covers
rescanning, the first launch and your first loudness push. If something isn't working, the
manual's **[FAQ & troubleshooting](USER_MANUAL.md#9-faq--troubleshooting)** is the fastest
route; after that, [`KNOWN_ISSUES.md`](../KNOWN_ISSUES.md) lists every confirmed limitation
with its status.

---

*Anabasis is © 2026 RollyTech. All rights reserved.*

# Installing Anabasis

This guide is for **users and testers** — how to install a build you were given.
Maintainers building the project: [`docs/procedures/BUILD.md`](../procedures/BUILD.md).

Anabasis v0.1.x is distributed as **plain ZIP archives**, one per platform, produced and
validated by the project's CI. Extracting a ZIP shows the payload directly — there is no
wrapper folder inside. A Windows installer and a macOS `.pkg` are built too, as **separate
downloads** rather than as archive contents; neither the archives nor the installers are
code-signed or notarized yet.

Each archive carries an `INSTALL.txt` with the same steps this guide gives, in English and
Chinese. Linux additionally carries the two scripts its `INSTALL.txt` describes:

| Platform | Archive | Contains |
|---|---|---|
| Linux | `Anabasis-Linux.zip` | `Anabasis.vst3` + `Anabasis` (Standalone) + `install.sh` + `uninstall.sh` + `INSTALL.txt` |
| Windows | `Anabasis-Windows.zip` | `Anabasis.vst3` + `Anabasis.exe` (Standalone) + `INSTALL.txt` |
| macOS | `Anabasis-macOS.zip` | `Anabasis.vst3` + `Anabasis.component` (AU) + `Anabasis.app` — universal (Apple Silicon + Intel) — + `INSTALL.txt` |

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

**Use the installer** — it is in the zip beside the plug-in:

```sh
./install.sh
```

It asks where to install:

1. **Current user** (the default, no root) — `~/.vst3/Anabasis.vst3` and
   `~/.local/bin/Anabasis`. `~/.vst3` is the standard per-user VST3 folder and REAPER,
   Bitwig and Ardour all scan it by default.
2. **System-wide** (for every user on the machine) — `/usr/lib/vst3/Anabasis.vst3` and
   `/usr/local/bin/Anabasis`. Only the steps that write to those locations ask for your
   password; the script does not run as root as a whole. `sudo ./install.sh` skips the
   question and installs system-wide directly.

`./install.sh --user` and `./install.sh --system` answer the question up front, which is
what a provisioning script or a CI step needs: without a flag, the question is only asked
when the terminal is there to answer it, and anything else takes the per-user default.
`--system` is not the same as `sudo ./install.sh` — it keeps the elevation per-operation,
so only the writes to `/usr/lib/vst3` and `/usr/local/bin` run as root. Two combinations
are refused rather than guessed at: `--user --system` together, since the two differ in
destination *and* in privilege and there is no sensible way to honour both; and `--user`
under `sudo`, since which home directory `$HOME` names there depends on the machine's
sudoers configuration. Repeating the same option is not a conflict and is accepted.

Replacing an existing install is safe: the previous version is kept until the new one is
in place, and an interrupted run is tidied up by the next one. The VST3 and the
Standalone are replaced one after the other rather than together, so an interruption
between the two can leave the new plug-in beside the old Standalone — both work; run the
installer again to finish the pair.

Uninstall with `./uninstall.sh` (or `sudo ./uninstall.sh` for a system-wide install). It
asks the same question, takes the same `--user` / `--system` flags for a run with no
terminal, and keeps your presets and settings. It also **keeps** a plug-in copy that an
interrupted install parked in its scratch directory — that copy is the only one of that
version, and only `./install.sh` can put it back, so the uninstaller names it and leaves
it rather than sweeping it up. Pass `--discard-parked` if you want it gone too.

Rescan plug-ins in your DAW (REAPER: *Options → Preferences → Plug-ins → VST →
Re-scan*; Bitwig: *Settings → Locations*; Ardour: *Preferences → Plugins*).

**Installing by hand**, if you would rather not run the script — per-user:

```sh
mkdir -p ~/.vst3 ~/.local/bin
cp -R Anabasis.vst3 ~/.vst3/
cp Anabasis ~/.local/bin/
chmod +x ~/.local/bin/Anabasis
chmod +x ~/.vst3/Anabasis.vst3/Contents/x86_64-linux/Anabasis.so
```

…or system-wide, with `sudo` and the `/usr/lib/vst3` + `/usr/local/bin` paths in place of
the two above.

**Troubleshooting**

- **`./install.sh` says "Permission denied"** — the executable bit was lost in transit
  (per-push CI artifact downloads do not preserve file modes). Run it through the shell
  instead: `sh ./install.sh`, or `sudo sh ./install.sh` for a system-wide install.
- **"Permission denied" or the DAW can't load the plug-in** — the `chmod` steps above.
- **Anabasis appears twice in your DAW** — you have both a per-user and a system-wide
  install. The installer warns when it finds one alongside the other. Remove whichever you
  do not want with `./uninstall.sh` or `sudo ./uninstall.sh`.
- **DAW doesn't find it** — check that the directory you installed into (`~/.vst3` or
  `/usr/lib/vst3`) is in the DAW's VST3 search path — both are by default in
  REAPER/Bitwig/Ardour — then rescan. Make sure the whole `Anabasis.vst3` folder was
  copied, not a file from inside it.
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

`SHA256SUMS.txt` and a build manifest are published beside the downloads. The plug-in's
exact version and build number are also on its About screen (click the **ANABASIS** title),
so you can always confirm *what* you are running.

---

## Next steps

Installed? The user manual's **[Quick start](USER_MANUAL.md#2-quick-start)** covers
rescanning, the first launch and your first loudness push. If something isn't working, the
manual's **[FAQ & troubleshooting](USER_MANUAL.md#9-faq--troubleshooting)** is the fastest
route; after that, [`KNOWN_ISSUES.md`](../KNOWN_ISSUES.md) lists every confirmed limitation
with its status.

---

*Anabasis is © 2026 RollyTech. All rights reserved.*

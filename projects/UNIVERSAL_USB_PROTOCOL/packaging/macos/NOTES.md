# macOS packaging notes

## What this installer does

`build-pkg.sh` produces `dist/Omniplex-<version>-macOS.pkg`: a standard macOS
installer package that puts the `omniplex` CLI at `/usr/local/bin/omniplex`. Running
the resulting `.pkg` again after a rebuild removes the previous install
first (via `scripts/preinstall`), so there's never a stale binary left
over between test builds.

## What "no driver install" currently means, honestly

- **MIDI already works with zero driver/extension install.** `omniplex run
  --midi` registers CoreMIDI virtual ports ("Omniplex In"/"Omniplex Out") using the
  OS's built-in virtual-port API. Any app (Logic, Ableton, etc.) sees them
  immediately, system-wide, with nothing to install and nothing to
  approve beyond the app itself running.
- **Audio and video do NOT yet show up as selectable system devices in
  other apps.** `--audio` currently opens your Mac's actual physical
  microphone/speakers directly via `cpal`/CoreAudio -- it does not create
  a new virtual audio device that a DAW could select as an input, and
  there is no virtual camera yet either. Building those is real driver
  work:
  - **Virtual audio device**: a CoreAudio Audio Server Plugin (HAL
    plugin), installed into `/Library/Audio/Plug-Ins/HAL/` with
    `sudo killall coreaudiod` to reload, or the modern DriverKit-based
    Audio Driver Extension path (requires a macOS **System Extension**
    user-approval flow).
  - **Virtual camera**: a CoreMediaIO DAL plugin (legacy) or a Camera
    Extension via DriverKit (modern, App Store/System-Extension flow).
  - Both DriverKit paths require an **Apple Developer Program
    membership** ($99/yr) for the entitlements and notarization Apple
    requires to load a system extension at all -- this is not something
    ad-hoc signing can substitute for.
  - This is scoped as Phase 2 work in the design spec (§4 layer 5); it
    has not been started.

## Code signing: what ad-hoc signing gets you, and what it doesn't

The build script ad-hoc signs the binary (`codesign --sign -`) with a
fixed identifier (`com.omniplex.cli`). This is free and needs no Apple account,
and it:
- satisfies `codesign --verify` (the binary has *a* signature, not none),
- gives the binary a stable identifier across rebuilds, which helps (but
  does not guarantee) macOS's TCC privacy-permission system recognize it
  as "the same app" across reinstalls so the Microphone-access grant
  persists instead of re-prompting every rebuild.

It does **not**:
- remove the Gatekeeper "unidentified developer" warning on first launch
  (only a real Apple Developer ID certificate + notarization does that),
- allow loading a System Extension/DriverKit driver at all -- unsigned or
  ad-hoc-signed system extensions are rejected by macOS outright.

**If you outgrow this**: enroll in the Apple Developer Program, get a
"Developer ID Application" + "Developer ID Installer" certificate, sign
both the binary and the `.pkg` with those, and notarize via
`xcrun notarytool submit`. That removes the Gatekeeper warning entirely
and is a hard prerequisite for shipping the real virtual audio/camera
drivers described above.

## Re-running during active development

Every time you change the code, re-run `./packaging/macos/build-pkg.sh`
and re-run the resulting `.pkg` on the target Mac -- `preinstall` deletes
the old binary first, so there's no "did the update actually take"
ambiguity.

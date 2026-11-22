# PMOR STUDIO Max devices: what needs a human in Live (no PMOR needed for 1-7)

Installed by `max/build_externals.sh` + `max/install_devices.sh`: externals in `~/Documents/Max 9/Library`, devices in the User Library (Instruments/Max Instrument/PMOR for the core, MIDI Effects/Max MIDI Effect/PMOR for the lanes), js helpers next to them and in Max 9/Library.

1. Restart Live (it must rescan Max 9/Library). Browser > User Library > Instruments > Max Instrument > PMOR: drag `PMOR core` onto a MIDI track. Expect: device loads, status reads "disconnected", no Max console errors (open the Max window from the device's wrench icon). Any "js: can't find pmor_*.js" = the helper files are not on the search path.
2. Add `PMOR lane drums` / `synth` / `sampler` / `control` (MIDI Effects > Max MIDI Effect > PMOR) to other MIDI tracks. Record or draw a clip of notes: drums C1.., synth any, sampler C1... Expect: the core track sounds (the lanes forward over `pmor_midi`) with PMOR's default patch. Check the synth pitch follows the clip.
3. Click `editor` on the core: the browser window opens; dials in a lane editor move audio params (drums pad VOL, synth cutoff).
4. Save the Live set, close, reopen: the devices come back, nothing is connected, no port is opened (check with `lsof /dev/cu.usbmodem*`, expect no PMOR file handle while `connect` is off).
5. Open the core's `connect` toggle ONLY with PMOR attached (see max/README.md, "PMOR on the Mac").
6. Known gaps: the sample cache goes to `<Live set folder>/PMOR` only if the device file sits inside a Live Project folder (User Library devices fall back to `~/Music/PMOR_Cache`); dial/umenu value conversion is checked in headless Max (tests/max_dials) but not by dragging in Live; the .amxd wrapper has the right container (compared byte-for-byte in layout with CHORUS.amxd) but Live has never loaded these.
7. If Live refuses a device: open `max/devices/PMOR core.maxpat` in Max directly (File > Open) and Save As .amxd from there; report the Max console error.

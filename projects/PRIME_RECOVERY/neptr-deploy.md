# NEPTR deploy on gorpi7v5 (file-level only; nothing started)

## Built
- csound-nam.so via ~/NEPTR_phase4/csound/nam/build-on-pi.sh (NeuralAmpModelerCore pinned e49c93e, cloned into csound/nam/libs, git-ignored).
  Installed: /usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/csound-nam.so (get_active_pair present).
  Eigen submodule clone failed twice on a GitLab load error; manual retry of `git submodule update --init --depth 1 Dependencies/eigen` worked.
- `csound --syntax-check-only` of neptrPhase4.csd with wrapper macros (STATE_DIR PRESET_DIR NAM_DIR IR_DIR DRUMLOOP_DIR NEPTR_NCHNLS=8 NEPTR_NCHNLS_I=8): 0 errors.

## Layout
- Launcher reads ~/demiurge/live.conf (/boot/firmware/demiurge.conf has no launch= override). NEPTR's own ~/NEPTR_phase4/live.conf is stale (Scarlett, bypass=on, old keys); not used. Backup: ~/demiurge/live.conf.bak-pre-neptr-*.
- live.conf edited in place: bpm 124, quantum 64 (= ksmps 64), sync_layer = off, audio_interface hw:CARD=sndrpihifiberry,DEV=0, ui/ui_cmd/ui_also, chain = ~/NEPTR_phase4/csound/neptrPhase4.csd.
- Data dirs demiurge-run-csound expects, symlinked: ~/demiurge/{ir,samples,presets} and ~/demiurge/profiles/nam -> ~/NEPTR_phase4/{csound/ir,samples,presets,csound/nam/profiles/live}.
- ~/.config/neptr/mode = 8ch (card enforces symmetric 8ch capture+playback). No ~/.config/neptr/routing yet (mixer off; main stays on outs 1-2).
- /usr/local/bin/demiurge-ui-launch.sh: taken from DEMIURGE_OS.git branch teensy-ring-pedal (config/pi5-performance/), absent from the public repo.
- /etc/systemd/system/neptr-ui.service (+ .d/10-reclaim-retry.conf from same branch), neptr-bridge.service: templated to gorpi7v5. daemon-reload done.
- ~/kivy-venv created (--system-site-packages) with python-osc mido python-rtmidi pyserial.

## State
- enabled: neptr-bridge.service. disabled: demiurge.service, neptr-ui.service. Nothing started.

## Start later (Emory's go, at the bench)
    sudo systemctl enable --now demiurge.service    # engine; neptr-ui is PartOf it
    sudo systemctl enable --now neptr-ui.service    # UI (needs python3-pygame from apt)
    sudo systemctl start neptr-bridge.service       # Teensy serial -> MIDI

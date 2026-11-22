# Install & run (on the Pi)

## 1. Sync the games folder to the Pi

From the Mac repo (same pattern as the rest of Demiurge):

```bash
rsync -avh demiurge/games/ gorpi7@demiurge.local:~/demiurge/games/
```

## 2. Run the installer

```bash
ssh gorpi7@demiurge.local
cd ~/demiurge/games
bash bin/games-install.sh
```

It installs RetroArch + `python3-evdev/mido/rtmidi`, loads `uinput`, adds you to
`input`+`dialout`, and writes the NOPASSWD sudoers for freeze/thaw.
**Log out/in (or reboot) once** so the group changes take effect.

## 3. Cores

The installer pulls what apt has. For the rest, in RetroArch:
**Main Menu → Online Updater → Core Downloader** and grab:

- **MAME** (or **MAME 2003-Plus** / **FBNeo** — lighter, great for these titles)
  → Galaga, Ms. Pac-Man, Dig Dug, Spy Hunter, Donkey Kong, Centipede, Frogger,
    Mario Bros.
- **Nestopia** or **FCEUmm** → NES (Tetris, Super Mario Bros.)
- **Flappy Bird** libretro (homebrew clone) → if available in your build.

See `GAMES.md` for the per-game core/ROM mapping.

## 4. ROMs

Drop your ROM files into `~/demiurge/games/roms/` (see `roms/README.md`). You
supply your own ROMs.

## 5. Audio path

Make sure `~/demiurge/live.conf` has:

```
sync_layer = on
```

so RetroArch's PipeWire output rides the sync layer to the Scarlett while the
synth is frozen. (In `sync_layer = off` bypass mode game audio won't get the
device — see ARCHITECTURE.md caveat.)

## 6. Run it

```bash
pkill -f serial_bridge.py          # the supervisor replaces the old bridge
python3 ~/demiurge/games/arcade_supervisor.py
```

Hold the **arcade button** and **click+hold the joystick** for ~1 second → the
synth freezes and RetroArch comes up. Same gesture again → back to the synth.

## 7. Run on boot (later — once it's proven)

Create a user service (not yet auto-installed — keeping changes in this folder):

```ini
# ~/.config/systemd/user/demiurge-arcade.service
[Unit]
Description=Demiurge Arcade supervisor (serial owner + console toggle)
After=demiurge.service

[Service]
ExecStart=/usr/bin/python3 %h/demiurge/games/arcade_supervisor.py
Restart=on-failure

[Install]
WantedBy=default.target
```

```bash
systemctl --user daemon-reload
systemctl --user enable --now demiurge-arcade
```

> When this graduates: disable/replace the `serial_bridge.py` service so the two
> don't fight over `/dev/ttyACM0`, and fold `games.conf` keys + the installer
> steps into the main setup-script and live.conf.

## Troubleshooting

- **Console never opens** → check the supervisor log for `entering CONSOLE`. If
  the combo never fires, confirm `b1`/`jb` appear in frames: `cat /dev/ttyACM0`.
- **No game audio** → `live.conf` must be `sync_layer = on`; confirm RetroArch
  audio driver is `pipewire` and PipeWire is running (`systemctl --user status pipewire`).
- **Synth doesn't freeze** → run `bin/demiurge-freeze` by hand; if it asks for a
  password the sudoers line didn't apply (`sudo visudo -cf /etc/sudoers.d/demiurge-arcade`).
- **Gamepad not detected** → `evtest` should list "Demiurge Arcade P1"; if not,
  `/dev/uinput` perms / `input` group / reboot.
- **UI crash-loops with `pygame.error: kmsdrm not available`** → a previous
  RetroArch didn't release DRM cleanly (usually only after a hard `kill -9`, not a
  normal combo-exit which uses SIGTERM). The screen/CRTC is stuck. Fix: make sure
  no `retroarch` is running (`pkill -9 retroarch`) then `sudo systemctl restart
  neptr-ui`; if it still won't grab KMSDRM, reboot. Normal combo-exits don't hit
  this because the supervisor SIGTERMs RetroArch and waits before force-killing.

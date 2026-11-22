#!/bin/bash
# launch_test.sh <rom.zip>  -- headless RetroArch launch for cheat verification
ROM="$1"
sudo systemctl stop neptr-ui demiurge-arcade
sleep 1
pkill -TERM -x retroarch 2>/dev/null; sleep 1
CFG=/home/gorpi7/demiurge/games/retroarch/retroarch.cfg
CORE=/home/gorpi7/.config/retroarch/cores/fbneo_libretro.so
unset DISPLAY WAYLAND_DISPLAY PIPEWIRE_NODE
export XDG_RUNTIME_DIR=/run/user/$(id -u)
nohup retroarch --config "$CFG" --appendconfig /tmp/netcmd.cfg -L "$CORE" "/home/gorpi7/demiurge/games/roms/$ROM" >/tmp/ra_$ROM.log 2>&1 &
echo "launched pid $!"

#!/bin/bash
# hello-autoload-script — tiny smoke test for the live.conf `ui_also =` path.
#
# Wire it in by adding this line under `ui_also =` in ~/demiurge/live.conf:
#   /home/gorpi7/NEPTR_phase4/scripts/hello-autoload-script.sh
#
# Then every time the UI autoloads — at boot, on `sudo systemctl restart neptr-ui`,
# or on `sudo systemctl restart demiurge` — this prints to the UI's journal:
#   journalctl -u neptr-ui -n 20 | grep "hello autoload script"
#
# It just proves an aux ui_also command actually launches alongside the UI.
echo "hello autoload script"

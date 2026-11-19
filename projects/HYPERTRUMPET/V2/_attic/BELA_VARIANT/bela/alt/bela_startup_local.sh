#!/bin/bash
# ALTERNATIVE, NOT INSTALLED BY install.sh. Do not use together with
# hypertrumpet-supervisor.service (two mechanisms for one job).
#
# Bela's stock /opt/Bela/bela_startup.sh execs /opt/Bela/local/bela_startup.sh
# when it exists (documented override hook: its first lines). If you prefer
# Bela's own service (`make ... startup` / the IDE "run on boot" toggle) to
# own boot, install this to /opt/Bela/local/bela_startup.sh, `chmod +x`, DISABLE
# hypertrumpet-supervisor.service, and enable bela_startup.service. hyperGuitar
# ran this way until the supervisor unit replaced it (the unit is simpler to
# reason about and can `Conflicts=` the stock runner).
while true; do [ "$(cut -d. -f1 /proc/uptime)" -gt 15 ] && break; sleep 1; done
exec /root/hyperTrumpet-supervisor.sh

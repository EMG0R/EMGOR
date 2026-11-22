#!/bin/bash
#
# DEMIURGE — Network guard
# Install to: /usr/local/bin/demiurge-netguard.sh
#
# Runs on a timer (see demiurge-netguard.timer) and is also fired instantly on
# link changes by the NetworkManager dispatcher hook (90-demiurge-failover).
# Its job is to keep exactly one thing true: the box answers to
# `demiurge.local`, over the best link available, with no human in the loop.
#
# ── The four failure modes this exists to kill ───────────────────────────────
#
#   1. SSID WANDERING. With no profile for the home network present (or a
#      lower autoconnect-priority than a neighbour's saved net), NM happily
#      associates with a DIFFERENT SSID on a DIFFERENT subnet. The Pi is then
#      "up" but on another LAN entirely — mDNS dead, IP dead, indistinguishable
#      from a crash. This actually happened: the home profile was deleted and
#      the box silently moved to a neighbour's AP on 192.168.1.0/24.
#
#   2. AUTOCONNECT GIVE-UP. NM's default autoconnect-retries is finite (4).
#      After a burst of association failures it stops trying until something
#      external pokes it, so a 30-second AP reboot becomes an indefinite
#      outage. (Fixed globally in 10-demiurge-network.conf; the guard is the
#      backstop that pokes it.)
#
#   3. SILENT HALF-LINK. Associated, DHCP lease held, but the gateway has
#      stopped answering — roamed to a dead BSSID, stale lease. NM still says
#      "connected", so nothing ever retries. The guard therefore tests
#      REACHABILITY (can we actually ping the gateway) rather than trusting
#      NM's state.
#
#   4. DUAL-HOMED AMBIGUITY — the subtle one. Wired and wireless are on the
#      SAME subnet here, so with both up the Pi holds two addresses and avahi
#      advertises BOTH. Route metrics only decide what the Pi *sends* on; they
#      have no say in which address a Mac resolving `demiurge.local` picks —
#      and it was picking the wifi one. Result: every tool talked to the box
#      over the flaky link while the perfectly good cable sat idle. That is
#      not "ethernet primary with wifi fallback", that is two interfaces
#      fighting.
#
# ── The invariant ───────────────────────────────────────────────────────────
# ONE LINK AT A TIME. When ethernet is up AND its gateway actually answers,
# wifi is parked (disconnected). The moment ethernet stops working, wifi is
# brought up. One interface up means one address, means `demiurge.local` is
# unambiguous and re-announces itself on every switchover.
#
# The cost is that wifi is not kept warm, so failover is not instantaneous.
# That is bought back by the dispatcher hook, which fires this script the
# instant NM sees eth0 go down — typically a couple of seconds, rather than
# waiting for the next timer tick. Set ETH_EXCLUSIVE=0 in the config to keep
# both links up instead (and accept the ambiguity).
#
# ── Fail-open ────────────────────────────────────────────────────────────────
# Always exits 0. A watchdog that shows up in `systemctl --failed` because the
# network is down is noise at exactly the moment you need signal. Everything
# it does is logged instead; `journalctl -u demiurge-netguard` is the one
# place to look.
#
# Config: /etc/demiurge/network.conf (see demiurge-network.conf.default)
#
set -u

CONF="${CONF:-/etc/demiurge/network.conf}"

# Defaults — overridden by $CONF if present.
HOME_SSID=""              # pinned home network; empty = no SSID enforcement
ENFORCE_HOME_SSID=1       # steer wifi back to HOME_SSID when it wanders
ETH_EXCLUSIVE=1           # park wifi while ethernet is healthy
ETH_IF="eth0"
WIFI_IF="wlan0"
PING_TIMEOUT=2            # seconds to wait for a gateway reply
BOUNCE_AFTER=2            # consecutive bad wifi checks before bouncing wlan0
RADIO_CYCLE_AFTER=6       # consecutive bad wifi checks before an rfkill cycle

# shellcheck source=/dev/null
[ -r "$CONF" ] && . "$CONF"

STATE_DIR="/run/demiurge"
STATE="$STATE_DIR/netguard.fails"
PAUSE="$STATE_DIR/netguard.pause"
mkdir -p "$STATE_DIR" 2>/dev/null || true

log() { printf '[%s] %s\n' "$1" "$2"; }

# Manual override: `sudo touch /run/demiurge/netguard.pause` while you sit on
# some other network on purpose (field test, phone hotspot). Cleared by reboot.
if [ -e "$PAUSE" ]; then
    log INFO "paused (${PAUSE} exists) — no action"
    exit 0
fi

fails=0
[ -r "$STATE" ] && fails=$(cat "$STATE" 2>/dev/null || echo 0)
case "$fails" in ''|*[!0-9]*) fails=0 ;; esac

# ── Observe ─────────────────────────────────────────────────────────────────
dev_state() {
    nmcli -t -f DEVICE,STATE dev 2>/dev/null \
        | awk -F: -v d="$1" '$1==d{print $2; exit}'
}
dev_conn() {
    nmcli -t -f DEVICE,CONNECTION dev 2>/dev/null \
        | awk -F: -v d="$1" '$1==d{sub(/^[^:]*:/,""); print; exit}'
}
# Gateway of the default route pinned to one interface (there may be two).
gw_of() {
    ip route show default dev "$1" 2>/dev/null \
        | awk '$1=="default"{print $3; exit}'
}
# Reachability is tested ON the interface in question, so a wired reply can
# never be mistaken for proof that wifi works (both share a subnet here).
gw_alive() {
    [ -n "$2" ] || return 1
    ping -c 1 -W "$PING_TIMEOUT" -n -I "$1" "$2" >/dev/null 2>&1
}

eth_state=$(dev_state "$ETH_IF")
wifi_state=$(dev_state "$WIFI_IF")
wifi_ssid=$(dev_conn "$WIFI_IF")

eth_gw=$(gw_of "$ETH_IF")
eth_ok=0
if [ "$eth_state" = "connected" ] && gw_alive "$ETH_IF" "$eth_gw"; then
    eth_ok=1
fi

action=""

# ── Ethernet healthy: park wifi, done ───────────────────────────────────────
if [ "$eth_ok" = "1" ]; then
    if [ "$fails" -gt 0 ]; then
        log INFO "ethernet gateway $eth_gw reachable again after $fails bad check(s)"
    fi
    fails=0

    # Only ever reached when ethernet has been POSITIVELY proven working —
    # NM says connected AND its gateway answered a ping pinned to eth0. It is
    # never parked on an assumption, and because the probe is pinned to the
    # interface a wifi reply can't be mistaken for a wired one (both links sit
    # on the same subnet here, so an unpinned ping would prove nothing).
    if [ "$ETH_EXCLUSIVE" = "1" ] && [ "$wifi_state" = "connected" ]; then
        log INFO "ethernet is healthy — parking $WIFI_IF so demiurge.local resolves to the wired address only"
        nmcli dev disconnect "$WIFI_IF" >/dev/null 2>&1 \
            && action=" action=park-wifi" || action=" action=park-wifi-failed"
        wifi_state=$(dev_state "$WIFI_IF")
        wifi_ssid=""
    fi

    echo "$fails" > "$STATE" 2>/dev/null || true
    log INFO "primary=$ETH_IF eth=$eth_state wifi=$wifi_state gw=$eth_gw reachable=1 fails=0$action"
    exit 0
fi

# ── Ethernet not usable: wifi is the link ───────────────────────────────────
if [ "$eth_state" = "connected" ]; then
    log WARN "$ETH_IF is up but its gateway (${eth_gw:-none}) is not answering — falling back to wifi"
else
    log INFO "$ETH_IF is ${eth_state:-absent} — wifi is the primary link"
fi

# ANTI-STRANDING, and the reason this box can never talk itself into having no
# link at all. Parking wifi uses `nmcli dev disconnect`, which ALSO clears the
# device's autoconnect flag, so NM will not bring wifi back on its own. Re-arm
# it here before doing anything else: if the `con up` below fails, or if this
# script dies halfway, NM's own autoconnect still recovers the box — and with
# connection.autoconnect-retries=0 it retries forever instead of giving up
# after four tries. (The flag is runtime-only, so a reboot re-arms it too;
# worst case is a power cycle, never a keyboard and monitor.)
nmcli dev set "$WIFI_IF" autoconnect yes >/dev/null 2>&1

# Bring wifi up if it is parked or idle. autoconnect-retries=0 means NM will
# keep trying forever on its own; this just starts it now instead of waiting.
#
# Deliberately "disconnected" only, not "!= connected && != unavailable" — the
# broader check also matched "connecting", which is the normal state for the
# few seconds after a user (or the web UI) manually joins a network. Under the
# old check, a timer tick landing mid-join saw "not connected yet" and called
# `nmcli dev connect`, which reactivates whatever NM currently considers best
# — stealing the interface out from under an in-progress manual join. This
# happened for real: joining a non-home network kept silently reverting to
# the home network within a few seconds, and it was this guard doing it.
if [ "$wifi_state" = "disconnected" ]; then
    if [ -n "$HOME_SSID" ] && nmcli -t -f NAME con show 2>/dev/null | grep -Fxq "$HOME_SSID"; then
        log INFO "activating '$HOME_SSID' on $WIFI_IF"
        nmcli con up id "$HOME_SSID" ifname "$WIFI_IF" >/dev/null 2>&1
    else
        # `nmcli dev connect` picks NM's own idea of "best known network" and
        # activates it — and it does this WHETHER OR NOT that connection has
        # autoconnect=no, because that flag only suppresses NM's own passive
        # background logic, not an explicit connect request. With no
        # HOME_SSID pinned, this was silently overriding every profile's
        # autoconnect=no every ~20s, repeatedly stealing the link back to
        # whatever NM preferred (observed for real: kicked a manual join to
        # a different network back to a previous one, over and over, on this
        # guard's own timer cadence). No pinned SSID now means "leave wifi
        # alone" — the box staying disconnected until a human (or a UI)
        # explicitly picks something is the correct behavior, not a fault
        # for this guard to correct.
        log INFO "$WIFI_IF is disconnected and no SSID is pinned — leaving it, not picking one"
    fi
    action=" action=wifi-up"
    wifi_state=$(dev_state "$WIFI_IF")
    wifi_ssid=$(dev_conn "$WIFI_IF")
fi

# SSID enforcement — only steers when the home net is genuinely in range, so a
# Pi that is legitimately out of range keeps whatever fallback it found.
if [ -n "$HOME_SSID" ] && [ "$ENFORCE_HOME_SSID" = "1" ] \
   && [ "$wifi_state" = "connected" ] && [ "$wifi_ssid" != "$HOME_SSID" ]; then
    if nmcli -t -f NAME con show 2>/dev/null | grep -Fxq "$HOME_SSID" \
       && nmcli -t -f SSID dev wifi list --rescan no 2>/dev/null | grep -Fxq "$HOME_SSID"; then
        log WARN "wifi is on '$wifi_ssid' but '$HOME_SSID' is in range — steering back"
        nmcli con up id "$HOME_SSID" ifname "$WIFI_IF" >/dev/null 2>&1 \
            && action="$action steered=ok" || action="$action steered=failed"
        wifi_ssid=$(dev_conn "$WIFI_IF")
    fi
fi

# ── Wifi reachability + escalation ladder ───────────────────────────────────
wifi_gw=$(gw_of "$WIFI_IF")
if gw_alive "$WIFI_IF" "$wifi_gw"; then
    if [ "$fails" -gt 0 ]; then
        log INFO "wifi gateway $wifi_gw reachable again after $fails bad check(s)"
    fi
    fails=0
else
    fails=$((fails + 1))
    if [ "$fails" -ge "$RADIO_CYCLE_AFTER" ]; then
        log CRITICAL "no wifi link after $fails checks — cycling the radio"
        nmcli radio wifi off >/dev/null 2>&1
        sleep 2
        nmcli radio wifi on >/dev/null 2>&1
        action="$action action=radio-cycle"
        fails=0
    elif [ "$fails" -ge "$BOUNCE_AFTER" ]; then
        log WARN "no wifi gateway reply (gw=${wifi_gw:-none}, fails=$fails) — bouncing $WIFI_IF"
        nmcli dev disconnect "$WIFI_IF" >/dev/null 2>&1
        nmcli dev connect "$WIFI_IF" >/dev/null 2>&1
        action="$action action=bounce-wifi"
    else
        log WARN "no wifi gateway reply (gw=${wifi_gw:-none}, fails=$fails) — waiting"
    fi
fi

echo "$fails" > "$STATE" 2>/dev/null || true

log INFO "primary=$WIFI_IF eth=$eth_state wifi=$wifi_state ssid=${wifi_ssid:-none} gw=${wifi_gw:-none} fails=$fails$action"

exit 0

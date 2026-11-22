#!/bin/bash
#
# pi-agent.sh — run a coding agent ON THE PI from the Mac, and move its work back.
#
# WHY: the point of Phase 11 is that the machine with the problem does the work.
# An agent on the Mac cannot read `aplay -l`, cannot watch `journalctl` while a
# fault happens, cannot run a hardware probe while an artifact is audible. This
# is the Mac-side door to the agent that can. It is a thin wrapper over
# `ssh <rig> demiurge-agent ...` whose only job is to know the rig's address
# (pi-rig.conf) so nobody types it again, and to make the git handoff one
# command instead of the bundle juggling that was happening by hand.
#
# Usage:
#   setup-script/pi-agent.sh --list                 which backends are installed
#   setup-script/pi-agent.sh "why is csound crash-looping?"
#   setup-script/pi-agent.sh --gemini "..."         pick a backend
#   setup-script/pi-agent.sh --bg "long job..."     detached in tmux on the Pi
#   setup-script/pi-agent.sh --attach               interactive: watch a --bg run
#   setup-script/pi-agent.sh --auth                 the one-time interactive logins
#   setup-script/pi-agent.sh --pull                 fetch the Pi's commits to the Mac
#   setup-script/pi-agent.sh --push                 send the Mac's commits to the Pi
#   PI_USER=x PI_HOST=y setup-script/pi-agent.sh …  one-off override
#
# Flags not listed here are passed through to demiurge-agent untouched.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

[[ -f "$ROOT/pi-rig.conf" ]] && source "$ROOT/pi-rig.conf"
PI_USER="${PI_USER:-${PI_USER_DEFAULT:-gorpi7v2}}"
PI_HOST="${PI_HOST:-${PI_HOST_DEFAULT:-demiurge.local}}"
TARGET="$PI_USER@$PI_HOST"
PI_REPO="${DEMIURGE_PI_REPO:-_______DEMIURGE}"

# BatchMode for everything except --attach/--auth, which are deliberately
# interactive (a tmux UI, and a browser login flow).
SSH_BATCH=(ssh -o BatchMode=yes -o ConnectTimeout=8 "$TARGET")
SSH_TTY=(ssh -t "$TARGET")

case "${1:-}" in
--auth)
    # Each backend's login is a browser/code flow that CANNOT be automated and
    # deliberately is not: no credentials belong in this repo. This just puts
    # them in front of you in order, on the Pi, with a tty.
    cat <<EOF
Run these ON THE PI, once each. Opening a shell there now.

  claude          then /login, follow the code flow
  gemini          first run prompts for auth
  codex           first run prompts for auth
  gh auth login   choose HTTPS; enables push from the Pi

EOF
    exec "${SSH_TTY[@]}"
    ;;
--pull)
    # The Pi repo has no network remote, and the Mac can reach the Pi but not
    # the reverse, so the MAC always drives. Pulling is a plain fetch over SSH.
    cd "$ROOT" || exit 1
    git remote get-url pi >/dev/null 2>&1 || {
        git remote add pi "$TARGET:$PI_REPO" && echo "added remote 'pi' -> $TARGET:$PI_REPO"
    }
    git fetch pi && echo && git --no-pager log --oneline --graph HEAD..pi/main | head -20
    echo
    echo "merge with:  git merge pi/main"
    exit $?
    ;;
--push)
    # Pushing into the Pi's CHECKED-OUT branch only works because Phase 11 sets
    # receive.denyCurrentBranch=updateInstead there, which also updates its
    # working tree — so an agent on the Pi sees the new files immediately.
    cd "$ROOT" || exit 1
    git remote get-url pi >/dev/null 2>&1 || git remote add pi "$TARGET:$PI_REPO"
    git push pi HEAD:main
    exit $?
    ;;
--attach)
    exec "${SSH_TTY[@]}" demiurge-agent --attach
    ;;
esac

if [ $# -eq 0 ]; then
    sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi

# Quote each argument so a prompt with spaces, quotes or newlines survives the
# trip through the remote shell intact.
printf -v _q '%q ' "$@" 2>/dev/null || _q="$*"
exec "${SSH_BATCH[@]}" "demiurge-agent $_q"

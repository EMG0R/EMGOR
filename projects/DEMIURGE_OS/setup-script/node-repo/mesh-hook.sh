#!/usr/bin/env bash
# mesh-hook.sh — Agent A's SSH-mesh / git-remote plug-in point.
#
# init-node-repo.sh calls this (if executable) AFTER the node repo is created:
#     mesh-hook.sh <node_repo_dir> <device_unique>
#   $1 = node repo dir (e.g. .../demiurge/local)
#   $2 = device unique name (e.g. twink, neuralgrid-a)
#
# CONTRACT:
#   - This is where remotes + known-hosts wiring go. Agent B (this file) creates
#     NONE of that — no remotes, no keys. That is A's lane.
#   - It is a NO-OP until EMGOR's repo topology is decided (GitHub-origin vs
#     gorcave-origin + GitHub-mirror). Until then it must point nowhere.
#   - Keys/credentials are LOCAL/personal and NEVER committed to a node repo.
#
# A: replace the body below with the mesh wiring once EMGOR picks topology, e.g.
#     git -C "$1" remote add origin <ssh-url-per-verdict>
#     git -C "$1" remote add <peer> <peer-ssh-url>   # full mesh, both directions
set -euo pipefail
DIR="${1:?node repo dir required}"
DEVICE="${2:?device unique required}"

echo "[mesh-hook] stub — no remotes wired for $DEVICE at $DIR"
echo "[mesh-hook] waiting on EMGOR topology verdict; Agent A fills this in."
exit 0

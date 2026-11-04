#!/usr/bin/env bash
# tools/test-node.sh - run the pure-module node test suite (no browser, no network).
#   usage: tools/test-node.sh [extra node --test args, e.g. --test-name-pattern=quests]
# Runs every tests/node/*.test.mjs from the repo root. Tests marked { todo: '<file:line> why' } document known bugs in js/ and do not fail the run.
set -euo pipefail
cd "$(dirname "$0")/.."
exec node --test "$@" tests/node/*.test.mjs

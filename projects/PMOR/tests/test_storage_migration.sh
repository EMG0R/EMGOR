#!/usr/bin/env bash
# test_storage_migration.sh TEST_BIN [CARD_FILES_DIR]
# Copies a card's PMOR3 folder (default: the real-card backup) to a scratch dir, migrates the COPY, and proves every
# original file is byte-identical (sha256) and unchanged in mtime afterwards. Never touches the source.
set -uo pipefail
bin="$1"; src="${2:-$HOME/pmor-backups/sd-20261107-0152/files}"
[ -d "$src" ] || { echo "SKIP: no card copy at $src (set CARD_FILES_DIR)"; exit 0; }
w="$(mktemp -d)"; trap 'rm -rf "$w"' EXIT
mkdir -p "$w/PMOR3" "$w/pristine/PMOR3"; cp -a "$src/." "$w/PMOR3/"; cp -a "$src/." "$w/pristine/PMOR3/"
( cd "$w/PMOR3" && find . -maxdepth 1 -type f -printf '%f %T@ %s\n' | sort > "$w/before.meta" && sha256sum $(ls -p | grep -v /) | sort -k2 > "$w/before.sha" )
"$bin" "$w" "$w/pristine" || { echo "FAIL: test binary"; exit 1; }
( cd "$w/PMOR3" && sha256sum $(cut -d' ' -f1 "$w/before.meta") | sort -k2 > "$w/after.sha" && find . -maxdepth 1 -type f -printf '%f %T@ %s\n' | sort > "$w/after.meta" )
if ! cmp -s "$w/before.sha" "$w/after.sha"; then echo "FAIL: legacy file content changed"; diff "$w/before.sha" "$w/after.sha" | head; exit 1; fi
# every original name still present with identical mtime/size
missing=$(comm -23 <(cut -d' ' -f1,2,3 "$w/before.meta") <(cut -d' ' -f1,2,3 "$w/after.meta") | wc -l)
[ "$missing" = 0 ] || { echo "FAIL: $missing legacy files missing/modified (name/mtime/size)"; exit 1; }
n=$(wc -l < "$w/before.sha"); new=$(find "$w/PMOR3" -mindepth 2 -type f | wc -l); sz=$(du -sk "$w/PMOR3/SAMPLES" | cut -f1)
echo "legacy files: $n, sha256 + mtime + size identical after migration; new-tree files: $new (SAMPLES ${sz} KB)"
echo "test_storage_migration: PASS"

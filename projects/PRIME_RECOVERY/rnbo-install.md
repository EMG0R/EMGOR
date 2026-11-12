# RNBO install (gorpi7v5) - 2026-10-22

## Findings before running
- docs/rnbo.md and setup-script/deploy-rnbo-wave1.sh DO NOT EXIST in ~/demiurge (branch gorpi7v5-install). wave1 step skipped.
- Phase 10 of setup-demiurge.sh does NOT use the apt-prebuilt runner (rnbo-install-runner.sh); it builds v1.4.5-9 from source (compile enabled) -> /opt/rnbo/bin/rnbooscquery, and rnbo-runner.service points there. Following Phase 10 path instead of rnbo-install-runner.sh.
- Python rule: rnbo-build-deps.sh pip --user installs conan; moved to venv ~/venvs/rnbo-conan, symlink ~/.local/bin/conan.

## Done
1. sudo bash setup-script/rnbo-add-repo.sh : OK, candidate rnbooscquery 1.4.5-9 (repo + key present).
2. rnbo-build-deps.sh : apt deps OK, conan 1.66.0 (venv), profile gcc14/libstdc++11/c++20.
3. Build started (RNBO_BUILD_JOBS=2, nice, log ~/PRIME_RECOVERY/logs/rnbo-build.log).
4. Pi hard-reset (power loss) mid-build while fetching boost/1.80.0 (signature mismatch on partial download). Restarting build; conan cache partial entries cleaned.
5. Rebuilt after reset (boost conan entries wiped); build OK: rnbooscquery 1.4.5 (tag v1.4.5-9, compile enabled, ruby 3.3.8 present).
6. Installed Phase-10 style: /opt/rnbo/{bin/rnbooscquery,bin/rnbo-compile-so,share/rnbo/so,src/rnbo}; ~/.config/rnbo/demiurge.json (paths rewritten to $HOME; so_build_exe -> /opt/rnbo/bin/rnbo-compile-so, rnbo_cpp_dir -> /opt/rnbo/src/rnbo, because Phase 10 never installs them where the stock json points); /usr/local/bin/demiurge-rnbo-gate.sh; /etc/systemd/system/rnbo-runner.service (rendered); autoreload script+user unit files placed.
7. State: rnbo-runner DISABLED + inactive (live.conf rnbo = off, gate fail-closed). autoreload user unit NOT enabled. No audio touched. Repo ~/demiurge unchanged.
## Issues
- systemd warns: rnbo-runner.service:24 Invalid environment assignment node.dont-reconnect=true (PIPEWIRE_PROPS needs quoting in repo unit; not changed).
- docs/rnbo.md, deploy-rnbo-wave1.sh missing from repo.
- demiurge.service was already inactive when checked (not caused by this task).
## To run
Emory permission, then: set rnbo = on (or *.rnbo in chain) in live.conf; sudo systemctl enable --now rnbo-runner; runner is a pipewire-jack client (card owned by NEPTR engine). Optionally systemctl --user enable --now demiurge-rnbo-autoreload.

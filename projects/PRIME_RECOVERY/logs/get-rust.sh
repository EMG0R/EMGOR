#!/bin/bash
# Resumable rustup-init fetch for a lossy link, then local install.
set -u
D=$HOME/PRIME_RECOVERY/dl; mkdir -p $D; F=$D/rustup-init
U=https://static.rust-lang.org/rustup/dist/aarch64-unknown-linux-gnu/rustup-init
for i in $(seq 1 40); do
  curl --http1.1 -fL -C - --retry 10 --retry-all-errors --retry-delay 3 -o $F $U && break
  echo "fetch attempt $i failed"; sleep 3
done
curl --http1.1 -fsSL --retry 10 --retry-all-errors -o $F.sha256 $U.sha256
(cd $D && echo "$(cut -d' ' -f1 rustup-init.sha256)  rustup-init" | sha256sum -c -) || exit 1
chmod +x $F
for i in $(seq 1 20); do
  RUSTUP_USE_CURL=1 $F -y --default-toolchain stable --profile minimal && break
  echo "rustup install attempt $i failed"; sleep 5
done
$HOME/.cargo/bin/cargo --version && echo RUST_DONE

"""Integration test for the ChucK native reference renderer (Task 2.1).

Launches ``chuck --silent sept_render_core.ck sept_render_osc.ck
sept_manifest.ck`` as a subprocess and drives it exactly like a real
controller would: runs the conformance checker against it, then blasts a
burst of hits and asserts the process is still alive. Skipped entirely if
``chuck`` is not on PATH (CI / non-dev machines).
"""
from __future__ import annotations

import pathlib
import random
import shutil
import subprocess
import time

import pytest

from septagraph.codec import encode_hit
from septagraph.conformance import check_renderer
from septagraph.net import RENDER_PORT, Sender
from septagraph.types import ZERO, Hit, vec
from septagraph.axes import K

CHUCK_DIR = pathlib.Path(__file__).parents[2] / "chuck"
CHUCK_FILES = ["sept_render_core.ck", "sept_render_osc.ck", "sept_manifest.ck"]

pytestmark = pytest.mark.skipif(
    shutil.which("chuck") is None, reason="chuck not on PATH"
)


@pytest.fixture
def chuck_renderer():
    for name in CHUCK_FILES:
        assert (CHUCK_DIR / name).exists(), f"missing {name}"

    proc = subprocess.Popen(
        ["chuck", "--loop", "--silent", *CHUCK_FILES],
        cwd=CHUCK_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    # give the VM time to compile, spork all shreds, and announce.
    time.sleep(1.5)
    assert proc.poll() is None, (
        "chuck exited before test start; stdout:\n"
        + (proc.stdout.read().decode("utf-8", "replace") if proc.stdout else "")
    )
    try:
        yield proc
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=3.0)


def test_conformance_checker_passes(chuck_renderer):
    failed = check_renderer("127.0.0.1", timeout_s=3.0)
    assert failed == []


def test_survives_hit_blast(chuck_renderer):
    sender = Sender([("127.0.0.1", RENDER_PORT)])
    rng = random.Random(1234)
    try:
        for i in range(20):
            mu = vec(*[rng.random() for _ in range(K)])
            sigma = vec(*[rng.random() * 0.1 for _ in range(K)])
            hit = Hit(f"blast:{i % 16}", 0, True, 0.7, mu, sigma)
            sender.send(encode_hit(hit))
            time.sleep(0.01)  # ~100 Hz
    finally:
        sender.close()

    time.sleep(0.2)
    assert chuck_renderer.poll() is None, "renderer process died during hit blast"

    # release everything cleanly and confirm it's still alive and answering
    sender2 = Sender([("127.0.0.1", RENDER_PORT)])
    try:
        for i in range(16):
            sender2.send(
                encode_hit(Hit(f"blast:{i}", 0, False, 0.0, vec(*[0.5] * K), ZERO))
            )
    finally:
        sender2.close()

    time.sleep(0.2)
    assert chuck_renderer.poll() is None
    failed = check_renderer("127.0.0.1", timeout_s=3.0)
    assert failed == []

"""Tests for septagraph.tools.latency_probe (plan Task 3.4)."""
from __future__ import annotations

import csv

from septagraph.tools import latency_probe as lp


def test_controller_stage_p95_under_2ms():
    samples = lp.measure_controller(n=200)
    assert len(samples) == 200
    assert all(s >= 0.0 for s in samples)
    summary = lp.summarize(samples)
    assert summary["n"] == 200
    assert summary["p95_ms"] < 2.0


def test_wire_stage_p95_under_2ms():
    samples = lp.measure_wire(n=200)
    assert len(samples) == 200
    assert all(s >= 0.0 for s in samples)
    summary = lp.summarize(samples)
    assert summary["n"] == 200
    assert summary["p95_ms"] < 2.0


def test_summarize_percentiles_are_monotonic():
    summary = lp.summarize([1.0, 2.0, 3.0, 4.0, 5.0])
    assert summary["p50_ms"] <= summary["p95_ms"] <= summary["max_ms"]
    assert summary["max_ms"] == 5.0
    assert summary["n"] == 5


def test_csv_well_formed(tmp_path):
    out = tmp_path / "latency.csv"
    rows = [
        ("controller", 0.1, 0.2, 0.3, 200, "testhost"),
        ("wire", 0.2, 0.4, 0.6, 200, "testhost"),
        ("renderer", 1.0, 2.0, 3.0, 200, "testhost"),
    ]
    lp.write_csv(out, rows)

    with out.open(newline="") as f:
        reader = csv.DictReader(f)
        assert reader.fieldnames == ["stage", "p50_ms", "p95_ms", "max_ms", "n", "host"]
        data = list(reader)

    assert len(data) == 3
    assert [row["stage"] for row in data] == ["controller", "wire", "renderer"]
    assert float(data[0]["p50_ms"]) == 0.1
    assert int(data[1]["n"]) == 200
    assert data[2]["host"] == "testhost"


def test_renderer_stage_skips_gracefully_without_chuck(monkeypatch):
    monkeypatch.setattr(lp.shutil, "which", lambda name: None)
    assert lp.chuck_available() is False
    assert lp.measure_renderer(n=5) is None


def test_run_skips_renderer_row_when_no_chuck(monkeypatch):
    monkeypatch.setattr(lp.shutil, "which", lambda name: None)
    rows = lp.run(n=10, include_chuck=True)
    stages = [row[0] for row in rows]
    assert stages == ["controller", "wire"]

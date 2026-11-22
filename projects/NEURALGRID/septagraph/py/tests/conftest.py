import pathlib

import pytest

GOLDEN_DIR = pathlib.Path(__file__).parents[2] / "golden"


def pytest_addoption(parser):
    parser.addoption(
        "--write-golden",
        action="store_true",
        default=False,
        help="(Re)generate septagraph/golden/*.osc from tests/golden_fixtures.py. "
             "Golden files are a wire-format contract: run this only for a deliberate "
             "format change, and commit the result.",
    )


def pytest_configure(config):
    if not config.getoption("--write-golden"):
        return
    from tests.golden_fixtures import GOLDEN_FIXTURES
    GOLDEN_DIR.mkdir(parents=True, exist_ok=True)
    for name, make in GOLDEN_FIXTURES.items():
        path = GOLDEN_DIR / f"{name}.osc"
        path.write_bytes(make())
        print(f"[write-golden] wrote {path} ({path.stat().st_size} bytes)")

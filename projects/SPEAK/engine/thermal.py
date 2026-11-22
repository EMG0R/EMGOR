from __future__ import annotations
import platform
import time
from pathlib import Path

TEMP_PATH = Path("/sys/class/thermal/thermal_zone0/temp")

_IS_PI = platform.machine() in ("aarch64", "armv7l")

_PAUSE_ABOVE = 85.0
_RESUME_BELOW = 75.0
_WARN_ABOVE = 80.0


def read_cpu_temp() -> float | None:
    if not _IS_PI:
        return None
    if not TEMP_PATH.exists():
        return None
    try:
        return int(TEMP_PATH.read_text().strip()) / 1000.0
    except (ValueError, OSError):
        return None


def check_and_throttle(llm_instance) -> bool:
    temp = read_cpu_temp()
    if temp is None:
        return False
    if temp > _PAUSE_ABOVE:
        llm_instance.n_batch = 64
        while True:
            time.sleep(10)
            current = read_cpu_temp()
            if current is None or current < _RESUME_BELOW:
                break
        llm_instance.n_batch = 512
        return True
    if temp > _WARN_ABOVE:
        llm_instance.n_batch = 64
        return True
    llm_instance.n_batch = 512
    return False

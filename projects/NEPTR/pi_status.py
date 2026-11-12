"""System-metrics helpers for the companion dashboard.

The pure parsers (parse_throttled / summarize_throttled) are unit-tested.
gather() pulls a live snapshot via vcgencmd + sysfs and degrades gracefully
when those are unavailable (e.g. running on a dev laptop), so the dashboard
can render "n/a" instead of crashing.
"""

import shutil
import subprocess

# vcgencmd get_throttled bit positions.
_BITS = {
    "under_voltage_now": 0,
    "freq_capped_now": 1,
    "throttled_now": 2,
    "soft_temp_now": 3,
    "under_voltage_occurred": 16,
    "freq_capped_occurred": 17,
    "throttled_occurred": 18,
    "soft_temp_occurred": 19,
}


def parse_throttled(value):
    """Decode a vcgencmd get_throttled value (int or '0x...' string) into a
    flag dict. 'ok' is True only when nothing is or has been wrong."""
    if isinstance(value, str):
        value = int(value.strip(), 16)
    flags = {name: bool(value & (1 << bit)) for name, bit in _BITS.items()}
    flags["ok"] = (value == 0)
    return flags


def summarize_throttled(value):
    """One short human label. Active conditions first, then past ('was ...')."""
    f = parse_throttled(value)
    if f["ok"]:
        return "ok"
    now = []
    if f["under_voltage_now"]:
        now.append("under-volt")
    if f["freq_capped_now"]:
        now.append("freq-cap")
    if f["throttled_now"]:
        now.append("throttled")
    if f["soft_temp_now"]:
        now.append("temp-lim")
    past = []
    if f["under_voltage_occurred"]:
        past.append("under-volt")
    if f["freq_capped_occurred"]:
        past.append("freq-cap")
    if f["throttled_occurred"]:
        past.append("throttled")
    if f["soft_temp_occurred"]:
        past.append("temp-lim")
    parts = list(now)
    if past:
        parts.append("was " + "/".join(past))
    return " ".join(parts) if parts else "ok"


def _run(cmd):
    """Run a command, return stripped stdout or None on any failure."""
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=2)
        if out.returncode == 0:
            return out.stdout.strip()
    except (OSError, subprocess.SubprocessError):
        pass
    return None


def _read(path):
    try:
        with open(path) as fh:
            return fh.read().strip()
    except OSError:
        return None


def cpu_temp_c():
    raw = _read("/sys/class/thermal/thermal_zone0/temp")
    if raw and raw.lstrip("-").isdigit():
        return round(int(raw) / 1000.0, 1)
    return None


def cpu_freq_mhz():
    raw = _read("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq")
    if raw and raw.isdigit():
        return int(raw) // 1000
    return None


def cpu_max_freq_mhz():
    raw = _read("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq")
    if raw and raw.isdigit():
        return int(raw) // 1000
    return None


def label_for_maxfreq(mhz):
    """Power-mode label from max CPU MHz, matching the demiurge CLI plabel():
    <=1600 LOW, <=2400 MED, else HIGH. None/0 -> unknown (None)."""
    if not mhz:
        return None
    if mhz <= 1600:
        return "LOW"
    if mhz <= 2400:
        return "MED"
    return "HIGH"


def conf_power_label(path=None):
    """Active level from live.conf `power =` (what demiurge-power applies).
    Frequency alone cannot tell the levels apart: low is 2000 flat and
    medium/high are both 2400 without an overclock. None if unset/unreadable."""
    import os
    path = path or os.environ.get("DEMIURGE_LIVE_CONF") or os.path.expanduser("~/demiurge/live.conf")
    try:
        with open(path) as fh:
            for line in fh:
                s = line.split("#", 1)[0].strip()
                if s.startswith("power") and "=" in s and s.split("=", 1)[0].strip() == "power":
                    v = s.split("=", 1)[1].strip().lower()
                    return {"low": "LOW", "med": "MED", "medium": "MED", "high": "HIGH"}.get(v)
    except OSError:
        pass
    return None


def power_label():
    return conf_power_label() or label_for_maxfreq(cpu_max_freq_mhz())


def load_avg():
    raw = _read("/proc/loadavg")
    if raw:
        try:
            return float(raw.split()[0])
        except (ValueError, IndexError):
            return None
    return None


def throttled_raw():
    """Return the vcgencmd throttled integer, or None if vcgencmd is absent."""
    if not shutil.which("vcgencmd"):
        return None
    out = _run(["vcgencmd", "get_throttled"])
    if out and "=" in out:
        try:
            return int(out.split("=", 1)[1].strip(), 16)
        except ValueError:
            return None
    return None


def gather():
    """Live snapshot for the dashboard. Missing values come back as None so
    the renderer can show 'n/a' without special-casing the platform."""
    thr = throttled_raw()
    maxf = cpu_max_freq_mhz()
    return {
        "temp_c": cpu_temp_c(),
        "freq_mhz": cpu_freq_mhz(),
        "max_mhz": maxf,
        "power_label": conf_power_label() or label_for_maxfreq(maxf),
        "load": load_avg(),
        "throttled": thr,
        "throttled_label": summarize_throttled(thr) if thr is not None else None,
    }

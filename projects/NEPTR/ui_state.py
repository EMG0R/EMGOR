"""Persisted UI position for the NEPTR Phase 4 UI ("remember where I was").

ONE rule decides what belongs in here:

    the UI persists only what the UI owns — WHERE THE USER WAS.
    Everything the ENGINE owns (effect on/off, parameter values, preset bank,
    looper transport, NAM/IR selections) is NEVER written here; it arrives on
    the OSC snapshot the engine rebroadcasts on the ch16 CC102 sync request
    (DEMIURGE_OS demiurge/docs/engine-ui.md). Persisting engine state would be
    a second source of truth for the same facts — exactly the parallel-system
    trap this project bans.

Format is the demiurge house format: `key = value` lines, `#` comments, in
`~/demiurge/ui-state.conf` (override with NEPTR_UI_STATE). Any read problem —
missing file, truncated write, garbage, wrong types — yields DEFAULTS; the UI
never fails to start because of this file. Writes are atomic (tmp + rename in
the same directory) and debounced, so a power cut can only ever cost the last
few seconds of cursor position, never a half-written file.
"""

import os
import tempfile

# Persisted keys and their defaults. Anything else found in the file is
# ignored (forward/backward compatible), and anything missing falls back here.
DEFAULTS = {
    "view": "menu0",          # current_view: 'menuN' or an effect code
    "menu": 0,                # last_menu_index (the menu an effect view sits in)
    "companion": 0,           # 1 = companion menu was open
    # The tab is persisted by NAME, never by index. Tab order is a layout
    # decision that changes (WAVE left the menu, SYSTEM and NETWORK arrived);
    # an index silently means a DIFFERENT tab after any such change, which
    # would land the user somewhere they never chose. A name either resolves
    # or it doesn't, and an unknown one falls back to the default tab.
    "companion_screen": "DASHBOARD",
    "companion_sel": 0,       # cursor row on that screen
    "scroll_patches": -1,     # list scroll top row, -1 = "follow the cursor"
    "scroll_audio": -1,
}

_INT_KEYS = ("menu", "companion", "companion_sel",
             "scroll_patches", "scroll_audio")

SAVE_DEBOUNCE = 2.0           # s of quiet before a changed position is written


def state_path():
    """Where the state file lives. Env override first (tests, dev machines),
    then the deployed demiurge config directory."""
    env = os.environ.get("NEPTR_UI_STATE")
    if env:
        return env
    return os.path.expanduser("~/demiurge/ui-state.conf")


def load(path=None):
    """Parsed state, always a complete dict — DEFAULTS for anything the file
    does not supply, and DEFAULTS entirely if the file is missing or corrupt.
    Never raises."""
    out = dict(DEFAULTS)
    try:
        with open(path or state_path()) as fh:
            text = fh.read()
    except (OSError, UnicodeDecodeError):
        return out
    try:
        for line in text.splitlines():
            line = line.split("#", 1)[0].strip()
            if "=" not in line:
                continue
            key, val = line.split("=", 1)
            key, val = key.strip(), val.strip()
            if key not in DEFAULTS:
                continue
            if key in _INT_KEYS:
                try:
                    out[key] = int(val)
                except ValueError:
                    pass                    # keep the default, drop the junk
            elif val:
                out[key] = val
    except Exception:                       # truly malformed input
        return dict(DEFAULTS)
    return out


def save(state, path=None):
    """Atomically write `state`. Returns True on success; a failure (read-only
    filesystem, missing parent that cannot be made) is swallowed — losing the
    cursor position must never take the UI down."""
    path = path or state_path()
    body = ["# NEPTR UI position — written by the UI, safe to delete.",
            "# UI-owned view state ONLY; engine state comes over OSC.", ""]
    for key in DEFAULTS:
        body.append(f"{key} = {state.get(key, DEFAULTS[key])}")
    text = "\n".join(body) + "\n"
    try:
        directory = os.path.dirname(os.path.abspath(path))
        os.makedirs(directory, exist_ok=True)
        fd, tmp = tempfile.mkstemp(dir=directory, prefix=".ui-state.")
        try:
            with os.fdopen(fd, "w") as fh:
                fh.write(text)
                fh.flush()
                os.fsync(fh.fileno())
            os.replace(tmp, path)
        except Exception:
            try:
                os.unlink(tmp)
            except OSError:
                pass
            raise
    except Exception:
        return False
    return True


class Store:
    """Debounced, change-detecting writer.

    The UI calls tick(now, snapshot) on every loop pass; the snapshot is a
    plain dict built from live UI attributes. Nothing is written while the
    position keeps changing, nothing is written at all when it has not
    changed, and flush() forces the final write on shutdown. That keeps the
    SD card out of the hot path (one small rename every few seconds at worst).
    """

    def __init__(self, path=None, debounce=SAVE_DEBOUNCE, saver=save):
        self.path = path or state_path()
        self.debounce = debounce
        self._saver = saver
        self._written = None       # last snapshot actually on disk
        self._pending = None       # changed snapshot waiting out the debounce
        self._due = None
        self.writes = 0

    def tick(self, now, snapshot):
        """Note the current position; write it once it has been stable for
        `debounce` seconds. Returns True if a write happened."""
        if snapshot != self._written and snapshot != self._pending:
            self._pending = dict(snapshot)
            self._due = now + self.debounce
            return False
        if self._pending is not None and self._due is not None and now >= self._due:
            return self._write(self._pending)
        return False

    def flush(self, snapshot=None):
        """Write immediately (shutdown path). Returns True if a write happened."""
        snap = snapshot if snapshot is not None else self._pending
        if snap is None or snap == self._written:
            return False
        return self._write(snap)

    def _write(self, snapshot):
        ok = self._saver(snapshot, self.path)
        self._pending = None
        self._due = None
        if ok:
            self._written = dict(snapshot)
            self.writes += 1
        return ok

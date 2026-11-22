"""The three neuralGrid Pi app modes: DIRECT LAB, SEPTAGRAPH, MIDI.

Each mode implements the same small interface (``Mode``) so app.py can
dispatch key/analog/lidar events to whichever mode is active and ask for
an LED frame, without knowing mode internals. Modes own their own
``Sequencer`` instance where relevant (Direct Lab and MIDI share the
sequencer *engine*, not an instance -- each mode drives its own).
"""
from __future__ import annotations

import json
import logging
import time
from pathlib import Path
from typing import Callable, Optional

from septagraph.net import Sender
from septagraph.types import HALF, vec

from .fsr import FsrChannel, FsrConfig, N_FSR
from .sequencer import Sequencer
from ..axes import K

log = logging.getLogger("septagraph.pi.modes")

STATE_DIR = Path.home() / ".neuralgrid"
DIRECT_LAYOUT_PATH = STATE_DIR / "direct_layout.json"

CHANNEL_NAMES = ("N", "S", "E", "W")
SOFTPOT_TOUCH_THRESHOLD = 120
SOFTPOT_MID = 2048.0

DIRECT_ROWS = 4      # play rows (0-3) in SPLIT layout; FULL layout plays all 8
GRID_ROWS = 8         # rows_config always describes all 8 rows
GRID_COLS = 8
PAGE_SIZE = 8         # legacy: one full-width row = 8 points per page
LAYOUT_VERSION = 2    # direct_layout.json schema (v2 = per-row segments)

# Direct Lab layout modes (task: MODULETTES preset). "full"/"split" are the
# original two layouts (see DirectLabMode docstring); "modulettes" is a third
# preset -- every one of the 64 pads plays one modulettes solenoid, pad
# (x,y) -> note 1-64 column-major from bottom-right (modulettes_note_for_pad), no paging.
LAYOUT_MODES = ("full", "split", "modulettes")


def layout_mode_from_state(saved: Optional[dict]) -> str:
    """Migrate a persisted direct-mode state dict to a ``LAYOUT_MODES``
    value: a new-style ``"layout_mode"`` string wins if valid, else the
    legacy boolean ``"split"`` (``True`` -> "split", falsy/missing ->
    "full") is migrated. Never returns "modulettes" from a legacy dict --
    that layout didn't exist before this field, so an old save can only
    resolve to "full" or "split"."""
    saved = saved or {}
    mode = saved.get("layout_mode")
    if mode in LAYOUT_MODES:
        return mode
    return "split" if saved.get("split") else "full"


def _softpot_delta(raw: int) -> Optional[float]:
    """Simple bipolar delta in [-1,1]; None when the strip reads untouched."""
    if raw < SOFTPOT_TOUCH_THRESHOLD:
        return None
    return max(-1.0, min(1.0, (raw - SOFTPOT_MID) / SOFTPOT_MID))


class Mode:
    """Base class: no-op handlers so a mode only needs to override what it uses."""

    name = "base"

    def on_key(self, x: int, y: int, edge: int) -> None:
        pass

    def on_analog(self, raw4) -> None:
        pass

    def on_lidar(self, mm4) -> None:
        pass

    def led_frame(self):
        """Return a flat list of 64 (r,g,b) tuples (leds.blank() shape)."""
        from . import leds
        return leds.blank()

    def panic(self) -> None:
        pass

    def state(self) -> dict:
        return {}


# ---------------------------------------------------------------------------
# Mode 1 -- DIRECT MACHINE LAB
# ---------------------------------------------------------------------------


def load_lab_map(path) -> list[dict]:
    data = json.loads(Path(path).read_text())
    return list(data.get("points", []))


def _lab_osc_fallback(point: dict, velocity: float) -> bytes:
    """Minimal private OSC encoder matching the arg-token grammar in the
    task brief, used only until septagraph.renderers.lab.lab_osc exists.

    Token grammar: "i:name=CONST" / "f:name=CONST" / "s:name=CONST" are
    constants; "i:vel"/"f:vel" scale velocity 0-1 into [vel_min,vel_max]
    then linearly into vel_range (int for "i", float 0-1 for "f");
    "i:dur=MS" is just another integer constant.
    """
    from pythonosc.osc_message_builder import OscMessageBuilder

    vel_min = point.get("vel_min", 0.0)
    vel_max = point.get("vel_max", 1.0)
    scaled01 = vel_min + (vel_max - vel_min) * max(0.0, min(1.0, velocity))
    lo, hi = point.get("vel_range", [1, 127])

    builder = OscMessageBuilder(address=point["osc"]["address"])
    for tok in point["osc"]["args"]:
        kind, rest = tok.split(":", 1)
        if "=" in rest:
            _name, const = rest.split("=", 1)
            if kind == "i":
                builder.add_arg(int(const))
            elif kind == "f":
                builder.add_arg(float(const))
            else:
                builder.add_arg(str(const))
        elif rest == "vel":
            if kind == "i":
                builder.add_arg(int(round(lo + (hi - lo) * scaled01)))
            else:
                builder.add_arg(float(scaled01))
        else:
            log.warning("unknown OSC arg token %r on point %r; skipped", tok, point.get("instrument"))
    return builder.build().dgram


def _load_lab_renderer(map_path, sender, ctl_targets):
    """Best-effort: use the real LabRenderer (owned by another agent) if it
    is importable; otherwise return None and the caller uses the private
    fallback so this module never blocks on that agent's landing."""
    try:
        from septagraph.renderers.lab import LabRenderer  # type: ignore
        return LabRenderer(map_path, sender, ctl_targets)
    except Exception as exc:  # ImportError, or any construction error
        log.info("septagraph.renderers.lab not usable yet (%s); using fallback OSC path", exc)
        return None


def _note_of(point: dict) -> int:
    """Best-effort ascending sort key: the constant on an "i:note=NN" arg
    token if present, else the point_id (stable, still deterministic)."""
    for tok in point.get("osc", {}).get("args", []):
        if tok.startswith("i:note=") or tok.startswith("f:note="):
            try:
                return int(float(tok.split("=", 1)[1]))
            except ValueError:
                pass
    return point["point_id"]


def instruments_by_enabled_count(points: list[dict]) -> list[str]:
    """Enabled-point-count per instrument, most points first (stable
    tie-break: first-seen-in-map order). Task 1's default row assignment:
    "4 different instruments with the most enabled points"."""
    counts: dict[str, int] = {}
    order: list[str] = []
    for p in points:
        if not p.get("enabled", True):
            continue
        instr = p["instrument"]
        if instr not in counts:
            counts[instr] = 0
            order.append(instr)
        counts[instr] += 1
    return sorted(order, key=lambda i: -counts[i])



def modulettes_note_for_pad(x: int, y: int) -> int:
    """MODULETTES numbering (user, 2026-09-10): notes 1-64, column-major
    starting at the player's BOTTOM-RIGHT and running UP each column, then
    one column to the LEFT: (7,7)=1 ... (7,0)=8, (6,7)=9 ... (0,0)=64.
    x = column 0..7 left->right, y = row 0..7 top->bottom (logical grid)."""
    return 1 + (GRID_COLS - 1 - x) * GRID_ROWS + (GRID_ROWS - 1 - y)

def sorted_instrument_points(points: list[dict], instrument: str) -> list[dict]:
    """All enabled points of one instrument, MIDI note ascending (task 1:
    "Marimbot rows page through its 32 notes in 4 pages, ordered by MIDI
    note ascending")."""
    pts = [p for p in points if p.get("enabled", True) and p["instrument"] == instrument]
    # optional per-point "sort_key" wins (e.g. breakBot: kick, snare, ride,
    # crash first so the 4-wide row shows a kit, not note order)
    return sorted(pts, key=lambda p: (p.get("sort_key", 1_000_000), _note_of(p)))


def _build_note_ranks(points: list[dict], instruments: list[str]) -> dict[int, tuple[int, int]]:
    """point_id -> (note_index, n_notes) within its instrument's own
    enabled-and-note-ascending list; feeds leds.note_color's per-note
    lightness/hue shading (colour spec: low->high note index within an
    instrument, not across the whole grid)."""
    ranks: dict[int, tuple[int, int]] = {}
    for instr in instruments:
        pts = sorted_instrument_points(points, instr)
        n = len(pts)
        for i, p in enumerate(pts):
            ranks[p["point_id"]] = (i, n)
    return ranks


def direct_led_frame(
    layout: dict[str, int],
    points_by_id: dict[int, dict],
    note_ranks: dict[int, tuple[int, int]],
    play_rows: int,
    press_times: Optional[dict] = None,
    now: Optional[float] = None,
    held: Optional[dict] = None,
) -> list:
    """Build the raw (pre-crossfade) Direct Lab LED frame: per-instrument
    hue (``leds.INSTRUMENT_COLORS``), per-note lightness/hue shade
    (``leds.note_color``), gamma-corrected and brightness-capped for the
    physical NeoTrellis. Disabled/empty pads stay OFF. A pad struck within
    ``leds.FLASH_S`` seconds flashes ``leds.FLASH_WHITE`` (pre-gamma, so the
    flash itself also gets gamma-corrected like everything else)."""
    from . import leds

    now = now if now is not None else time.monotonic()
    press_times = press_times or {}
    held = held or {}
    frame = leds.blank()
    for y in range(play_rows):
        for x in range(GRID_COLS):
            pid = layout.get(f"{x},{y}")
            point = points_by_id.get(pid) if pid is not None else None
            if point is None or not point.get("enabled", True):
                continue
            note_index, n_notes = note_ranks.get(pid, (0, 1))
            color = leds.note_color(point["instrument"], note_index, n_notes)
            t0 = press_times.get((x, y))
            # White while the pad is HELD (t0 = press time, cleared on
            # release) and for at least FLASH_S after a quick tap, so a
            # tap still reads as a click.
            if held.get((x, y)) or (t0 is not None and (now - t0) < leds.FLASH_S):
                color = leds.FLASH_WHITE
            frame[leds.idx(x, y)] = color
    frame = leds.apply_gamma_frame(frame)
    frame = leds.cap_brightness(frame)
    return frame


# -- row segments (layout v2) -----------------------------------------------
#
# rows_config[row] = {"segments": [{"instrument": str|None, "page": int,
# "width": int}, ...]} for every row 0-7; a row's widths always sum to 8.
# The legacy shape (one instrument per row) is exactly one width-8 segment.


def _segment(instrument: Optional[str], page: int = 0, width: int = GRID_COLS) -> dict:
    return {"instrument": instrument, "page": int(page), "width": int(width)}


def normalize_segments(segments, known_instruments) -> list[dict]:
    """Validate + copy a segment list. Raises ValueError (never clamps) so
    app.py can hand the message back to the UI: widths must be ints >= 1
    summing to exactly 8, instrument None or a known enabled instrument."""
    if not isinstance(segments, (list, tuple)) or not segments:
        raise ValueError("segments must be a non-empty list")
    out: list[dict] = []
    total = 0
    for seg in segments:
        if not isinstance(seg, dict):
            raise ValueError("each segment must be an object")
        width = seg.get("width")
        if isinstance(width, bool) or not isinstance(width, int) or width < 1:
            raise ValueError(f"segment width must be an int >= 1 (got {width!r})")
        instr = seg.get("instrument")
        if instr is not None and instr not in known_instruments:
            raise ValueError(f"unknown instrument {instr!r}")
        page = seg.get("page", 0)
        try:
            page = max(0, int(page))
        except (TypeError, ValueError):
            raise ValueError(f"segment page must be an int (got {page!r})")
        out.append(_segment(instr, page, width))
        total += width
    if total != GRID_COLS:
        raise ValueError(f"segment widths must sum to {GRID_COLS} (got {total})")
    return out


# The Tuesday FULL-layout plan: (instrument, page, width) per segment, per
# row. Piano/keyboard reading convention -- bottom row = lowest pitch,
# top-right = highest -- so marimbot's 4 pages run row 3 (page 0) up to
# row 0 (page 3), lydia row 5 (page 0) then row 4 (page 1). Tammy's 25
# notes page 8-at-a-time on row 6 (4 pages); row 7 is split 4|4 between
# breakBot and ganaPati, each paged independently 4 pads at a time.
DEFAULT_FULL_PLAN: dict[int, list[tuple[str, int, int]]] = {
    0: [("marimbot", 3, 8)],
    1: [("marimbot", 2, 8)],
    2: [("marimbot", 1, 8)],
    3: [("marimbot", 0, 8)],
    4: [("lydia", 1, 8)],
    5: [("lydia", 0, 8)],
    6: [("tammy", 0, 8)],
    7: [("breakBot", 0, 3), ("ganaPati", 0, 5)],  # kit: kick/snare/cymbal | 8,3,1,13,11
}


def default_full_rows(points: list[dict]) -> dict[int, dict]:
    """Rows 0-7 -> {"segments": [...]} for the FULL layout, from the
    enabled points of ``points``. Instruments missing from the map (e.g. a
    small test fixture) fall back to ``instruments_by_enabled_count``
    order, cycling through whatever instruments *are* available so each
    row still gets a distinct instrument where possible; with zero enabled
    instruments every segment gets instrument None. Widths always sum to 8."""
    available = instruments_by_enabled_count(points)
    rows: dict[int, dict] = {}
    fallback_i = 0
    for row in range(GRID_ROWS):
        segs = []
        for instr, page, width in DEFAULT_FULL_PLAN[row]:
            if instr not in available:
                if available:
                    instr = available[fallback_i % len(available)]
                    fallback_i += 1
                    page = 0
                else:
                    instr = None
                    page = 0
            segs.append(_segment(instr, page, width))
        rows[row] = {"segments": segs}
    return rows


def _rows_have_segments(rows) -> bool:
    return isinstance(rows, dict) and bool(rows) and all(
        isinstance(v, dict) and "segments" in v for v in rows.values()
    )


def migrate_layout(saved: Optional[dict], points: list[dict]) -> dict:
    """Bring a direct_layout.json dict to the v2 (segments) shape.

    * None / empty, or v1 rows (``{"instrument","page"}`` per row) -> rows
      are rebuilt from ``default_full_rows`` (a warning is logged); the old
      file's "overrides" and "fsr_assign" are preserved.
    * v2 rows load as-is; any row whose segments fail validation (or is
      missing) falls back to the default for that row only.
    Always returns ``{"layout_version": 2, "rows": {int: ...}, "overrides":
    {...}, "fsr_assign": {...}}``."""
    saved = saved or {}
    defaults = default_full_rows(points)
    known = set(instruments_by_enabled_count(points))
    rows_in = saved.get("rows")
    rows_out: dict[int, dict] = {}
    if not _rows_have_segments(rows_in):
        if rows_in:
            log.warning("direct_layout.json is v1 (row=instrument); rebuilding rows as the default FULL layout")
        rows_out = defaults
    else:
        for row in range(GRID_ROWS):
            v = rows_in.get(str(row), rows_in.get(row))
            try:
                if v is None:
                    raise ValueError("missing row")
                rows_out[row] = {"segments": normalize_segments(v["segments"], known)}
            except (ValueError, KeyError, TypeError) as exc:
                log.warning("direct_layout.json row %d invalid (%s); using default", row, exc)
                rows_out[row] = defaults[row]
    return {
        "layout_version": LAYOUT_VERSION,
        "rows": rows_out,
        "overrides": dict(saved.get("overrides", {}) or {}),
        "fsr_assign": dict(saved.get("fsr_assign", {}) or {}),
    }


class DirectLabMode(Mode):
    """Direct play across the grid. Every row 0-7 is described by
    ``rows_config[row] = {"segments": [{"instrument","page","width"}, ...]}``
    (widths sum to 8); each segment shows one ``width``-wide page of its
    instrument's enabled points, MIDI-note ascending, and pages
    independently. Persisted in ``direct_layout.json`` (v2) alongside any
    manual per-pad overrides (which always win) and FSR assignments.

    Three layouts, selected by ``layout_mode`` (``LAYOUT_MODES``):
      * FULL       (the default) -- all 64 pads play the rows_config catalog.
      * SPLIT      -- rows 0-3 play, rows 4-7 are the on-grid 4-track x
        8-step sequencer.
      * MODULETTES -- all 64 pads play modulettes: pad (x,y) is note
        y*8+x (0-63), one solenoid per pad, no row/segment paging. Like
        FULL, the sequencer keeps running UI-only. Switching to
        MODULETTES never touches ``rows_config`` (the FULL layout), so
        switching back to FULL restores exactly what was there before.
    ``split`` is kept as a derived bool property (``True`` iff
    ``layout_mode == "split"``) for backward compat with existing callers.
    The sequencer is 4x8 in every layout and keeps running in FULL/MODULETTES
    (it is just UI-driven there; grid presses on rows 4-7 strike pads
    instead of toggling a step -- only SPLIT repurposes rows 4-7).

    FSR0-3 (task 4): four assignable velocity-sensitive strike pads --
    default one per row's instrument (same points as the sequencer's
    default track assignment) -- giving the robots real dynamics the
    Trellis itself can't (no velocity on a key press).
    """

    name = "direct_lab"

    def __init__(
        self,
        map_path,
        lab_target: Optional[tuple[str, int]] = None,
        dry_run: bool = False,
        layout_path: Path = DIRECT_LAYOUT_PATH,
        lab_renderer=None,
    ):
        self.map_path = map_path
        self.points = load_lab_map(map_path)
        self.points_by_id = {p["point_id"]: p for p in self.points}
        self.enabled_points = [p for p in self.points if p.get("enabled", True)]
        self.dry_run = dry_run
        self.layout_path = layout_path

        self.sender = Sender(targets=[lab_target] if lab_target else [])
        # A pre-built shared LabRenderer (e.g. app.py's one in-process
        # instance also used by SeptagraphMode) wins over building our own,
        # so Direct and Septagraph modes strike through the same rate-limit
        # state and the same UDP target.
        self._lab_renderer = lab_renderer or _load_lab_renderer(map_path, self.sender, ctl_targets=[])

        self._instrument_order = instruments_by_enabled_count(self.enabled_points)
        self._note_ranks = _build_note_ranks(self.enabled_points, self._instrument_order)
        # note -> point for the modulettes preset (MODULETTES layout mode);
        # built once here so switching into that mode is just a layout
        # rebuild, no per-switch scan of the catalog.
        self._modulettes_by_note: dict[int, dict] = {
            _note_of(p): p for p in sorted_instrument_points(self.enabled_points, "modulettes")
        }
        self._press_times: dict[tuple[int, int], float] = {}
        self._held: dict[tuple[int, int], bool] = {}  # pads currently down -> white
        self.rows_config: dict[int, dict] = {}
        self.pad_overrides: dict[str, int] = {}
        saved = self._load_layout()
        saved = self._load_rows_and_overrides(saved)
        self.layout_mode = "full"  # LAYOUT_MODES; FULL is the default
        self.layout: dict[str, int] = {}
        self._rebuild_layout()

        self.last_strike_time: dict[int, float] = {}
        self.skip_count = 0
        self.last_sent_text = ""

        self.vel_base = 0.7
        self.vel_offset = 0.0  # live-offset by the E softpot
        self.tempo_base = 120

        self.sequencer = Sequencer(n_tracks=4, n_steps=8, tempo_bpm=self.tempo_base)
        self.sequencer.on_step = self._on_seq_step
        self.track_assignment: dict[int, int] = {}
        self._default_track_assignment()

        # -- FSR strike pads (task 4) ---------------------------------
        self.fsr_config = FsrConfig()
        self._fsr_channels = [FsrChannel() for _ in range(N_FSR)]
        self.fsr_assign: dict[int, Optional[int]] = dict(saved.get("fsr_assign", {})) if saved else {}
        self.fsr_assign = {int(k): v for k, v in self.fsr_assign.items()}
        self._default_fsr_assignment()
        self.fsr_pressure = [0.0] * N_FSR

    # -- layout persistence ------------------------------------------------

    def _load_layout(self) -> Optional[dict]:
        try:
            return json.loads(self.layout_path.read_text())
        except Exception:
            return None

    def _load_rows_and_overrides(self, saved: Optional[dict]) -> dict:
        """Migrate ``saved`` (any version, or None) to v2 and install its
        rows/overrides. Returns the migrated dict (fsr_assign is read from
        it by __init__)."""
        migrated = migrate_layout(saved, self.enabled_points)
        self.rows_config = {int(k): v for k, v in migrated["rows"].items()}
        self.pad_overrides = dict(migrated["overrides"])
        return migrated

    def save_layout(self) -> None:
        self.layout_path.parent.mkdir(parents=True, exist_ok=True)
        data = {
            "layout_version": LAYOUT_VERSION,
            "rows": {str(k): v for k, v in self.rows_config.items()},
            "overrides": self.pad_overrides,
            "fsr_assign": self.fsr_assign,
        }
        self.layout_path.write_text(json.dumps(data))

    # -- row segments + per-segment catalog paging ---------------------------

    def instrument_list(self) -> list[str]:
        return list(self._instrument_order)

    def _segments(self, row: int) -> list[dict]:
        cfg = self.rows_config.get(row)
        if cfg is None:
            cfg = self.rows_config[row] = {"segments": [_segment(None)]}
        return cfg["segments"]

    def segment_points(self, row: int, seg_idx: int) -> list[dict]:
        segs = self._segments(row)
        if not 0 <= seg_idx < len(segs):
            return []
        instr = segs[seg_idx].get("instrument")
        if not instr:
            return []
        return sorted_instrument_points(self.enabled_points, instr)

    def segment_n_pages(self, row: int, seg_idx: int) -> int:
        segs = self._segments(row)
        if not 0 <= seg_idx < len(segs):
            return 1
        width = max(1, int(segs[seg_idx].get("width", GRID_COLS)))
        n = len(self.segment_points(row, seg_idx))
        return max(1, (n + width - 1) // width)

    def set_segment_page(self, row: int, seg_idx: int, page: int) -> None:
        segs = self._segments(row)
        if not 0 <= seg_idx < len(segs):
            return
        segs[seg_idx]["page"] = max(0, min(int(page), self.segment_n_pages(row, seg_idx) - 1))
        self._rebuild_layout()
        self.save_layout()

    def segment_page(self, row: int, seg_idx: int, delta: int) -> None:
        segs = self._segments(row)
        if not 0 <= seg_idx < len(segs):
            return
        self.set_segment_page(row, seg_idx, segs[seg_idx].get("page", 0) + delta)

    def set_row_segments(self, row: int, segments) -> None:
        """Replace a row's segments. Raises ValueError (row untouched) when
        widths don't sum to 8 / an instrument is unknown -- app.py returns
        that message to the UI instead of silently clamping."""
        if not 0 <= int(row) < GRID_ROWS:
            raise ValueError(f"row must be 0..{GRID_ROWS - 1}")
        self.rows_config[int(row)] = {"segments": normalize_segments(segments, self._instrument_order)}
        self._rebuild_layout()
        self.save_layout()

    # legacy single-instrument-per-row accessors (act on segment 0)

    def row_points(self, row: int) -> list[dict]:
        return self.segment_points(row, 0)

    def n_pages(self, row: int) -> int:
        return self.segment_n_pages(row, 0)

    def set_row_instrument(self, row: int, instrument: str) -> None:
        if instrument not in self._instrument_order:
            return
        self.rows_config[row] = {"segments": [_segment(instrument, 0, GRID_COLS)]}
        self._rebuild_layout()
        self.save_layout()

    def set_row_page(self, row: int, page: int) -> None:
        self.set_segment_page(row, 0, page)

    def page(self, row: int, delta: int) -> None:
        self.segment_page(row, 0, delta)

    def _modulettes_layout(self) -> dict[str, int]:
        """MODULETTES layout: pad (x,y) -> the modulettes point whose note
        is y*8+x, for every one of the 64 pads. Manual pad overrides still
        win, same as FULL/SPLIT."""
        layout: dict[str, int] = {}
        for y in range(GRID_ROWS):
            for x in range(GRID_COLS):
                note = modulettes_note_for_pad(x, y)
                point = self._modulettes_by_note.get(note)
                if point is not None:
                    layout[f"{x},{y}"] = point["point_id"]
        for key, pid in self.pad_overrides.items():
            layout[key] = pid
        return layout

    def _rebuild_layout(self) -> None:
        if self.layout_mode == "modulettes":
            self.layout = self._modulettes_layout()
            return
        layout: dict[str, int] = {}
        for row in range(GRID_ROWS):
            x0 = 0
            for i, seg in enumerate(self._segments(row)):
                width = max(1, int(seg.get("width", GRID_COLS)))
                page = max(0, min(int(seg.get("page", 0)), self.segment_n_pages(row, i) - 1))
                seg["page"] = page
                pts = self.segment_points(row, i)
                start = page * width
                for dx, p in enumerate(pts[start:start + width]):
                    if x0 + dx < GRID_COLS:
                        layout[f"{x0 + dx},{row}"] = p["point_id"]
                x0 += width
        for key, pid in self.pad_overrides.items():
            layout[key] = pid
        self.layout = layout

    def set_pad(self, x: int, y: int, point_id: Optional[int]) -> None:
        key = f"{x},{y}"
        if point_id is None:
            self.pad_overrides.pop(key, None)
        else:
            self.pad_overrides[key] = point_id
        self._rebuild_layout()
        self.save_layout()

    # -- track assignment --------------------------------------------------

    def _default_track_assignment(self) -> None:
        seen_instruments: set = set()
        track = 0
        for p in self.enabled_points:
            if p["instrument"] in seen_instruments:
                continue
            seen_instruments.add(p["instrument"])
            self.track_assignment[track] = p["point_id"]
            track += 1
            if track >= self.sequencer.n_tracks:
                break

    def assign_track(self, track: int, point_id: int) -> None:
        self.track_assignment[track] = point_id

    # -- FSR pad assignment (task 4) ---------------------------------------

    def _default_fsr_assignment(self) -> None:
        """One per row's instrument by default -- same points the sequencer's
        default track assignment picked, so the demo's four rows and four
        FSRs agree out of the box."""
        for i in range(N_FSR):
            if i in self.fsr_assign:
                continue
            self.fsr_assign[i] = self.track_assignment.get(i)

    def assign_fsr(self, index: int, point_id: Optional[int]) -> None:
        self.fsr_assign[index] = point_id
        self.save_layout()

    # -- toggling layout mode (FULL / SPLIT / MODULETTES) --------------------

    def set_layout_mode(self, mode: str) -> None:
        """FULL: all 64 pads play rows_config. SPLIT: rows 0-3 play, rows
        4-7 = on-grid sequencer. MODULETTES: all 64 pads play modulettes
        (pad (x,y) = note y*8+x), rows_config untouched so FULL is restored
        exactly on switching back. The 4x8 sequencer is never resized and
        keeps running (UI-driven only) outside of SPLIT."""
        if mode not in LAYOUT_MODES:
            raise ValueError(f"layout_mode must be one of {LAYOUT_MODES} (got {mode!r})")
        self.layout_mode = mode
        self._rebuild_layout()

    def set_split(self, split: bool) -> None:
        """Back-compat shim: True -> SPLIT, False -> FULL. Never selects
        MODULETTES (only set_layout_mode can)."""
        self.set_layout_mode("split" if split else "full")

    @property
    def split(self) -> bool:
        """Derived, back-compat: True iff layout_mode == "split"."""
        return self.layout_mode == "split"

    def is_play_row(self, y: int) -> bool:
        return (self.layout_mode != "split") or y < DIRECT_ROWS

    # -- striking a point ----------------------------------------------------

    def direct_strike(self, point_id: int, velocity: Optional[float] = None) -> bool:
        point = self.points_by_id.get(point_id)
        if point is None:
            return False

        if self._lab_renderer is not None:
            vel = velocity if velocity is not None else max(0.0, min(1.0, self.vel_base + self.vel_offset))
            ok = self._lab_renderer.direct_strike(point_id, vel)
            if ok:
                self.last_sent_text = f"{point['osc']['address']} (via LabRenderer) vel={vel:.2f}"
            else:
                self.skip_count += 1
            return ok

        now = time.monotonic()
        max_rate_hz = point.get("max_rate_hz", 1000.0) or 1000.0
        min_interval = 1.0 / max_rate_hz
        last = self.last_strike_time.get(point_id)
        if last is not None and (now - last) < min_interval:
            self.skip_count += 1
            return False
        self.last_strike_time[point_id] = now

        vel = velocity if velocity is not None else max(0.0, min(1.0, self.vel_base + self.vel_offset))
        packet = _lab_osc_fallback(point, vel)
        self.last_sent_text = f"{point['osc']['address']} vel={vel:.2f} -> {point.get('target', '?')}"
        if not self.dry_run:
            self.sender.send(packet)
        return True

    # -- sequencer -------------------------------------------------------

    def _on_seq_step(self, track: int, step: int, vel: float) -> None:
        point_id = self.track_assignment.get(track)
        if point_id is None:
            return
        self.direct_strike(point_id, velocity=vel)

    # -- Mode interface ----------------------------------------------------

    def on_key(self, x: int, y: int, edge: int) -> None:
        if not edge:
            # release: pad stops being white (a quick tap still shows
            # FLASH_S of white via _press_times)
            self._held.pop((x, y), None)
            return
        if self.is_play_row(y):
            point_id = self.layout.get(f"{x},{y}")
            if point_id is not None:
                self._press_times[(x, y)] = time.monotonic()
                self._held[(x, y)] = True
                self.direct_strike(point_id)
        else:
            self.sequencer.toggle_step(y - DIRECT_ROWS, x)

    def on_analog(self, raw) -> None:
        for name, raw_val in zip(CHANNEL_NAMES, raw[:4]):
            delta = _softpot_delta(raw_val)
            if name == "E":
                self.vel_offset = 0.0 if delta is None else delta * 0.3
            elif name == "N":
                self.sequencer.tempo_bpm = self.tempo_base if delta is None else self.tempo_base + delta * 40

        # FSRs NEVER strike robots (user rule, locked 2026-09-09): mounted in
        # the housing they flex with every pad press and were firing ganaPati
        # on pad release. Pressure is tracked for the meters / future
        # expression use only.
        for i, raw_val in enumerate(raw[4:4 + N_FSR]):
            _event, pressure = self._fsr_channels[i].update(raw_val, self.fsr_config)
            self.fsr_pressure[i] = pressure

    def led_frame(self):
        from . import leds
        play_rows = DIRECT_ROWS if self.layout_mode == "split" else GRID_ROWS
        # play pads: colour = the pad's instrument, shaded by note index
        # within that instrument (colour spec); empty pads stay OFF. In
        # MODULETTES every pad's "instrument" is modulettes, so this is the
        # same per-note hue shading the FULL layout already uses.
        frame = direct_led_frame(
            self.layout, self.points_by_id, self._note_ranks, play_rows, self._press_times,
            held=self._held,
        )
        if self.layout_mode == "split":
            for track in range(4):
                for step in range(8):
                    level = self.sequencer.pattern[track][step]
                    y = track + DIRECT_ROWS
                    if level:
                        frame[leds.idx(step, y)] = leds.dim(leds.WARM_WHITE, 0.4 + 0.2 * level)
                    else:
                        frame[leds.idx(step, y)] = leds.dim(leds.WARM_WHITE, 0.08)
            ph = self.sequencer.playhead
            if self.sequencer.playing:
                frame[leds.idx(ph, DIRECT_ROWS)] = leds.AMBER
        return frame

    def panic(self) -> None:
        self.sequencer.stop_transport()

    def _rows_state(self) -> dict:
        out = {}
        for row in sorted(self.rows_config):
            segs = []
            for i, seg in enumerate(self._segments(row)):
                segs.append({
                    "instrument": seg.get("instrument"),
                    "page": seg.get("page", 0),
                    "width": seg.get("width", GRID_COLS),
                    "n_pages": self.segment_n_pages(row, i),
                })
            first = segs[0] if segs else {"instrument": None, "page": 0, "n_pages": 1}
            out[row] = {
                "segments": segs,
                # legacy (segment 0) fields kept for older UI/tests
                "instrument": first["instrument"],
                "page": first["page"],
                "n_pages": first["n_pages"],
            }
        return out

    def state(self) -> dict:
        from . import leds
        return {
            "layout_mode": self.layout_mode,
            "split": self.split,  # derived, back-compat with older UI/tests
            "layout": self.layout,
            "rows": self._rows_state(),
            "instrument_colors": {
                name: leds.to_hex(leds.instrument_color(name)) for name in self.instrument_list()
            },
            "instruments": self.instrument_list(),
            "track_assignment": self.track_assignment,
            "fsr_assign": self.fsr_assign,
            "fsr_pressure": self.fsr_pressure,
            "fsr_config": self.fsr_config.to_json(),
            "sequencer": self.sequencer.to_json(),
            "skip_count": self.skip_count,
            "last_sent": self.last_sent_text,
            "vel_base": self.vel_base,
            "points": [
                {"point_id": p["point_id"], "instrument": p["instrument"],
                 "element": p.get("element", ""), "enabled": p.get("enabled", True)}
                for p in self.points
            ],
        }


# ---------------------------------------------------------------------------
# Mode 2 -- SEPTAGRAPH
# ---------------------------------------------------------------------------


class SeptagraphMode(Mode):
    """Thin wrapper around controllers.grid_ctl.GridController. Real window
    projection / manifest snapping / catalog-vs-continuum LEDs live there
    (plan Task 3.3); this class adapts it to the app's Mode interface and
    holds the UI-facing preset/window-slider state.
    """

    name = "septagraph"

    PRESETS = ("LAB_DIRECT", "CONTINUUM", "HYBRID", "MIRROR")

    # CONTINUUM/HYBRID share this window (task 2): center HALF, extent
    # covering freq/loud/harm/scale generously, everything else (rate, az,
    # spread, dist) pinned at the window center (extent 0).
    CONTINUUM_EXTENT = vec(0.45, 0.3, 0.3, 0.0, 0.45, 0.0, 0.0, 0.0, 0.0)

    def __init__(self, controller, manifest_store, lab_renderer=None,
                 lab_local_targets=None, external_targets=None):
        self.controller = controller
        self.manifest_store = manifest_store
        self.lab_renderer = lab_renderer  # in-process LabRenderer listening on 7700, if --lab given
        # target lists the four presets pick between (task 2): LAB_DIRECT ->
        # local LabRenderer only; CONTINUUM -> external --renderers only;
        # HYBRID -> both.
        self._lab_local_targets = list(lab_local_targets or [])
        self._external_targets = list(external_targets or [])
        self.preset = None
        self._last_led_frame = None
        controller.leds = self._capture_leds
        self.set_preset("LAB_DIRECT")

    def _capture_leds(self, frame_bytes: bytes) -> None:
        self._last_led_frame = [
            tuple(frame_bytes[i * 3: i * 3 + 3]) for i in range(64)
        ]

    def _catalog_window(self):
        """LAB_DIRECT (task 2): window centered on the catalog's centroid,
        extent covering its bounding box (with a small floor so a
        single-point/degenerate axis doesn't collapse to zero extent)."""
        points = self.manifest_store.catalog()
        if not points:
            return HALF, vec(*([0.05] * K))
        mins = [min(p.mu[i] for p in points) for i in range(K)]
        maxs = [max(p.mu[i] for p in points) for i in range(K)]
        center = vec(*[(mn + mx) / 2.0 for mn, mx in zip(mins, maxs)])
        extent = vec(*[max((mx - mn) / 2.0, 0.01) for mn, mx in zip(mins, maxs)])
        return center, extent

    def set_preset(self, name: str) -> bool:
        if name == "MIRROR":
            return False  # greyed: model not built
        if name not in self.PRESETS:
            return False

        w = self.controller.window
        w.assign = {"grid": (0, 4)}  # artistic default: x->freq, y->scale, all presets

        if name == "LAB_DIRECT":
            w.center, w.extent = self._catalog_window()
            self.controller.snap_radius = 1.0  # everything snaps
            self.controller.set_targets(self._lab_local_targets)
        elif name == "CONTINUUM":
            w.center, w.extent = HALF, self.CONTINUUM_EXTENT
            self.controller.snap_radius = 0.0  # snap off
            self.controller.set_targets(self._external_targets)
        elif name == "HYBRID":
            w.center, w.extent = HALF, self.CONTINUUM_EXTENT
            self.controller.snap_radius = 0.06
            self.controller.set_targets(self._lab_local_targets + self._external_targets)

        self.preset = name
        self.controller.rebuild()  # rebuilds cell table + LED frame; app.py's
        # led loop crossfades old->new over 150ms (leds.CROSSFADE_S) the
        # next time it notices led_frame() changed -- same mechanism every
        # mode switch already uses, no extra code needed here.
        return True

    def set_window(self, center=None, extent=None) -> None:
        if center is not None:
            self.controller.window.center = tuple(center)
        if extent is not None:
            self.controller.window.extent = tuple(extent)
        self.controller.rebuild()

    def on_key(self, x: int, y: int, edge: int) -> None:
        self.controller.on_key(x, y, edge)

    def on_analog(self, raw) -> None:
        self.controller.on_analog(raw)

    def on_lidar(self, mm4) -> None:
        self.controller.on_lidar(mm4)

    def led_frame(self):
        if self._last_led_frame is not None:
            return self._last_led_frame
        from . import leds
        return leds.blank()

    def state(self) -> dict:
        return {
            "preset": self.preset,
            "center": list(self.controller.window.center),
            "extent": list(self.controller.window.extent),
            "snap_radius": self.controller.snap_radius,
            "n_catalog_points": len(self.manifest_store.catalog()),
            "fsr_config": self.controller.fsr_config.to_json(),
        }


# ---------------------------------------------------------------------------
# Mode 3 -- MIDI
# ---------------------------------------------------------------------------

MAJOR_SCALE = (0, 2, 4, 5, 7, 9, 11)


class MidiMode(Mode):
    name = "midi"

    # FSR pads (task 4): 4 notes on the pad channel, velocity = pressure*127;
    # held pressure -> channel aftertouch, off by default (no per-note
    # aftertouch on plain MIDI -- channel aftertouch is the honest option).
    DEFAULT_FSR_NOTES = (36, 38, 42, 46)

    def __init__(self, midiout, base_note: int = 36, scale=MAJOR_SCALE, channel: int = 0,
                 fsr_notes=None, fsr_aftertouch: bool = False):
        self.midiout = midiout
        self.base_note = base_note
        self.scale = list(scale)
        self.channel = channel
        self.cc_map = {"N": 1, "S": 2, "E": 3, "W": 4}
        self.lidar_cc = 74
        self.held_notes: dict[tuple[int, int], int] = {}

        self.sequencer = Sequencer(n_tracks=4, n_steps=8)
        self.sequencer.on_step = self._on_seq_step
        self.track_notes = {t: base_note + t * 4 for t in range(self.sequencer.n_tracks)}
        self.track_channels = {t: channel for t in range(self.sequencer.n_tracks)}

        self.fsr_config = FsrConfig()
        self._fsr_channels = [FsrChannel() for _ in range(N_FSR)]
        self.fsr_notes = list(fsr_notes) if fsr_notes else list(self.DEFAULT_FSR_NOTES)
        self.fsr_aftertouch = fsr_aftertouch
        self._fsr_held: dict[int, bool] = {}
        self.fsr_pressure = [0.0] * N_FSR

    def note_for_pad(self, x: int, y: int) -> int:
        row_major = y * 8 + x
        n = len(self.scale)
        octave, deg = divmod(row_major, n)
        return self.base_note + 12 * octave + self.scale[deg]

    def on_key(self, x: int, y: int, edge: int) -> None:
        if edge:
            note = self.note_for_pad(x, y)
            self.held_notes[(x, y)] = note
            self.midiout.note_on(note, vel=100, ch=self.channel)
        else:
            note = self.held_notes.pop((x, y), None)
            if note is not None:
                self.midiout.note_off(note, ch=self.channel)

    def on_analog(self, raw) -> None:
        for name, raw_val in zip(CHANNEL_NAMES, raw[:4]):
            cc = self.cc_map.get(name)
            if cc is None:
                continue
            val = max(0, min(127, round(raw_val / 4095.0 * 127)))
            self.midiout.cc(cc, val, ch=self.channel)

        for i, raw_val in enumerate(raw[4:4 + N_FSR]):
            event, pressure = self._fsr_channels[i].update(raw_val, self.fsr_config)
            self.fsr_pressure[i] = pressure
            note = self.fsr_notes[i] if i < len(self.fsr_notes) else None
            if note is None:
                continue
            # FSRs never trigger notes (locked rule, see DirectLabMode.on_analog);
            # they may only modulate a note that is already held.
            if event == "release":
                self._fsr_held.pop(i, None)
            elif self.fsr_aftertouch and self._fsr_held.get(i):
                self.midiout.channel_aftertouch(max(0, min(127, round(pressure * 127))), ch=self.channel)

    def on_lidar(self, mm4) -> None:
        valid = [m for m in mm4 if 0 <= m < 0xFFFF]
        if not valid:
            return
        nearest = max(100, min(2000, min(valid)))
        val = max(0, min(127, round((1.0 - (nearest - 100) / 1900.0) * 127)))
        self.midiout.cc(self.lidar_cc, val, ch=self.channel)

    def _on_seq_step(self, track: int, step: int, vel: float) -> None:
        note = self.track_notes.get(track)
        if note is None:
            return
        ch = self.track_channels.get(track, self.channel)
        self.midiout.note_on(note, vel=max(1, min(127, round(vel * 127))), ch=ch)

    def panic(self) -> None:
        self.sequencer.stop_transport()
        self.midiout.all_notes_off()
        self.held_notes.clear()
        self._fsr_held.clear()

    def led_frame(self):
        from . import leds
        frame = leds.blank()
        for x in range(8):
            for y in range(8):
                if (x, y) in self.held_notes:
                    frame[leds.idx(x, y)] = leds.WARM_WHITE
        return frame

    # -- output port picker (task 3) ---------------------------------------

    def list_ports(self) -> list[str]:
        return self.midiout.list_ports()

    def select_port(self, name: str) -> bool:
        return self.midiout.select_port(name)

    def state(self) -> dict:
        return {
            "base_note": self.base_note,
            "scale": self.scale,
            "channel": self.channel,
            "cc_map": self.cc_map,
            "lidar_cc": self.lidar_cc,
            "sequencer": self.sequencer.to_json(),
            "track_notes": self.track_notes,
            "midi_available": self.midiout.available,
            "port_name": getattr(self.midiout, "port_name", None),
            "ports": self.list_ports(),
            "fsr_notes": self.fsr_notes,
            "fsr_aftertouch": self.fsr_aftertouch,
            "fsr_pressure": self.fsr_pressure,
            "fsr_config": self.fsr_config.to_json(),
        }

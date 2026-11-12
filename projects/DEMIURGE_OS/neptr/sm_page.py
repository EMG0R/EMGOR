"""SoundMagic spectral rack page (menu 20 NORRIS) for the NEPTR Phase 4 UI.

Everything the page needs lives here so neptrPhase4_UI.py only delegates:
registry loading, layout, hit-testing, drawing, the focused-effect strip, the
deep param sheet and the RACK sheet. The UI module is passed in (`m`) for its
palette, fonts and geometry, which avoids a circular import and works whether
the UI runs as __main__ or is imported by tests.

Protocol (see docs/ui.md):
  UI -> engine (127.0.0.1:7770):  /st fx 0|1      /sp fx p val(float)
  engine -> UI (8000):            /sm_state fx on  /sm_p fx p val  /sm_hero fx k idx  /sm_pedal fx idx
The engine is authoritative for on-state. Param values are shown
optimistically while dragging, then snap to the /sm_p echo.
"""

import json
import os
import time

from pythonosc.udp_client import SimpleUDPClient

REG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "csound", "spectral", "sm_params.json")
SM_HOST = "127.0.0.1"
SM_PORT = int(os.environ.get("NEPTR_SM_OSC_PORT", "7770"))
RACK_FX = 23            # reserved slot: /sp 23 0 <idx> = rack default FFT index
LONG_PRESS = 0.45       # s
SM_RATE = 1.0 / 30.0
SAMPLE_RATE = 48000.0
AXIS_PX = 14
CLOSE_PULL_PX = 160

GRID_COLS = 5
TILE_GAP = 8
TILE_H = 140
CREDIT_H = 34      # credit line above the grid (the old legacy 5-slot row is gone)


# ------------------------------------------------------------------ registry

class Registry:
    def __init__(self, path=REG_PATH):
        with open(path) as f:
            d = json.load(f)
        self.raw = d
        self.stride = d["stride"]
        self.fft_sizes = d["fft_sizes"]
        self.credit = d.get("credit", "")
        self.effects = d["effects"]
        self.by_fx = {e["fx"]: e for e in self.effects}
        self.spectral = [e for e in self.effects if e["kind"] == "spectral"]
        self.n_params = sum(len(e["params"]) for e in self.effects)

    def param(self, fx, p):
        return self.by_fx[fx]["params"][p]

    def fft_param(self, fx):
        for p in self.by_fx[fx]["params"]:
            if p["name"] == "FFT Size":
                return p
        return None


def fmt_value(prm, v):
    """Value text with unit; enums show their label, bools ON/OFF."""
    if prm["kind"] == "enum":
        labels = prm["enum"]
        i = max(0, min(len(labels) - 1, int(round(v))))
        return labels[i]
    if prm["kind"] == "bool":
        return "ON" if v >= 0.5 else "OFF"
    span = prm["max"] - prm["min"]
    if span >= 200:
        s = "%d" % round(v)
    elif span >= 20:
        s = "%.1f" % v
    else:
        s = "%.2f" % v
    unit = prm.get("unit", "")
    if unit and unit != "on/off":
        s += " " + unit
    return s


def norm_of(prm, v):
    span = (prm["max"] - prm["min"]) or 1.0
    return max(0.0, min(1.0, (v - prm["min"]) / span))


def value_of(prm, frac):
    frac = max(0.0, min(1.0, frac))
    v = prm["min"] + frac * (prm["max"] - prm["min"])
    if prm["kind"] in ("bool", "enum"):
        v = float(round(v))
    return v


# ---------------------------------------------------------------------- page

class SmPage:
    def __init__(self, m, registry=None, sender=None):
        self.m = m
        self.reg = registry or Registry()
        self.vals = {}
        for e in self.reg.effects:
            for p in e["params"]:
                self.vals[e["fx"] * self.reg.stride + p["p"]] = p["default"]
        self.vals[RACK_FX * self.reg.stride] = 2.0
        self.on = [0] * 24
        # hero slots per effect (engine giSMhero k=0..6), seeded from the registry;
        # /sm_hero rewrites them live. -1 = empty slot. pedal: local p or -1.
        self.hero_slots = {e["fx"]: (list(e["heroes"]) + [-1] * 7)[:7] for e in self.reg.effects}
        self.pedal = {e["fx"]: e["pedal"] for e in self.reg.effects}
        self.rev = 0
        self.focus = self.reg.spectral[0]["fx"]
        self.sheet = None            # None | ('fx', fx) | ('rack',)
        self.scroll = 0
        self.sender = sender         # callable(addr, *args) override (tests)
        self._client = None
        self._sent_at = 0.0
        self.lp = None               # [target, t0] long-press armed
        self.lp_fired = False
        self.drag = None             # ('hero', fx, p, rect) while a strip fader is held
        self.dragging_idx = None
        self.tile_rects = []         # [(rect, fx|'rack')]
        self.fader_rects = []        # [(rect, fx, p)]
        self.strip_title = None
        self.strip_power = None
        self._sh = None              # sheet gesture state
        self._rows = []

    # ---- OSC out ----
    def _send(self, addr, *args):
        if self.sender is not None:
            self.sender(addr, *args)
            return
        try:
            if self._client is None:
                self._client = SimpleUDPClient(SM_HOST, SM_PORT)
            self._client.send_message(addr, list(args))
        except OSError:
            pass

    def send_toggle(self, fx, on):
        self._send("/st", int(fx), int(bool(on)))

    def send_param(self, fx, p, v, final=False):
        now = time.time()
        if final or now - self._sent_at >= SM_RATE:
            self._sent_at = now
            self._send("/sp", int(fx), int(p), float(v))
            return True
        return False

    # ---- state in (engine echoes) ----
    def on_state(self, fx, on):
        fx = int(fx)
        if 0 <= fx < 24:
            self.on[fx] = 1 if int(on) else 0
            self.rev += 1
            return True
        return False

    def on_param(self, fx, p, v):
        fx, p = int(fx), int(p)
        idx = fx * self.reg.stride + p
        if fx == RACK_FX and p == 0:
            self.vals[idx] = float(v)
        elif idx not in self.vals:
            return False
        elif self.dragging_idx == idx:
            return False            # the finger owns it; echo reconciles on release
        self.vals[idx] = float(v)
        self.rev += 1
        return self.is_visible(fx, p)

    def heroes_of(self, fx):
        """Local param indices of the shown heroes (first 3 filled slots)."""
        return [p for p in self.hero_slots.get(fx, []) if p >= 0][:3]

    def _local_p(self, fx, idx):
        idx = int(idx)
        e = self.reg.by_fx.get(fx)
        if e is None or idx < 0:
            return -1
        p = idx - fx * self.reg.stride
        return p if 0 <= p < len(e["params"]) else -1

    def on_hero(self, fx, k, idx):
        fx, k = int(fx), int(k)
        if fx not in self.hero_slots or not 0 <= k <= 6:
            return False
        p = self._local_p(fx, idx)
        if self.hero_slots[fx][k] == p:
            return False
        self.hero_slots[fx][k] = p
        self.rev += 1
        return True

    def on_pedal(self, fx, idx):
        fx = int(fx)
        if fx not in self.pedal:
            return False
        p = self._local_p(fx, idx)
        if self.pedal[fx] == p:
            return False
        self.pedal[fx] = p
        self.rev += 1
        return True

    def is_visible(self, fx, p):
        if self.sheet is not None:
            return self.sheet == ('fx', fx) or (self.sheet == ('rack',) and fx == RACK_FX)
        e = self.reg.by_fx.get(fx)
        if e is None or e["kind"] != "spectral":
            return False
        return p in self.heroes_of(fx)

    def get(self, fx, p):
        return self.vals[fx * self.reg.stride + p]

    def set_local(self, fx, p, v):
        self.vals[fx * self.reg.stride + p] = float(v)
        self.rev += 1

    # ---- derived ----
    def fft_index(self, fx):
        prm = self.reg.fft_param(fx)
        if prm is None:
            return int(round(self.vals[RACK_FX * self.reg.stride]))
        return int(round(self.get(fx, prm["p"])))

    def latency_s(self):
        sizes = set()
        for e in self.reg.spectral:
            if self.on[e["fx"]]:
                i = max(0, min(len(self.reg.fft_sizes) - 1, self.fft_index(e["fx"])))
                sizes.add(self.reg.fft_sizes[i])
        return sum(2.0 * (n / 4.0) / SAMPLE_RATE for n in sizes)

    def signature(self):
        return ('sm', self.rev, self.focus, self.sheet)

    # ------------------------------------------------------------- layout
    def layout(self):
        """Pure geometry for the page (no drawing). Returns a dict of Rects."""
        m = self.m
        pygame = m.pygame
        top = m.CONTENT_TOP
        pad = m.PAD_X
        avail = m.W - 2 * pad
        credit_y = top + 14
        grid_top = top + CREDIT_H
        tw = (avail - (GRID_COLS - 1) * TILE_GAP) // GRID_COLS
        tiles = []
        ids = [e["fx"] for e in self.reg.spectral] + ['rack']
        for k, tid in enumerate(ids):
            r, c = divmod(k, GRID_COLS)
            rect = pygame.Rect(pad + c * (tw + TILE_GAP),
                               grid_top + r * (TILE_H + TILE_GAP), tw, TILE_H)
            tiles.append((rect, tid))
        rows = (len(ids) + GRID_COLS - 1) // GRID_COLS
        grid_bot = grid_top + rows * TILE_H + (rows - 1) * TILE_GAP
        strip_top = grid_bot + 14
        strip_bot = m.CONTENT_BOTTOM - m.EXPR_H - 4
        title = pygame.Rect(pad, strip_top, avail - 130, 54)
        power = pygame.Rect(pad + avail - 120, strip_top, 120, 54)
        faders = []
        fx = self.focus
        heroes = self.heroes_of(fx)
        colw = avail // 3
        for k, p in enumerate(heroes):
            x0 = pad + k * colw
            tr = pygame.Rect(x0 + 34, strip_top + 100, colw - 68, strip_bot - strip_top - 100 - 56)
            faders.append((tr, fx, p))
        return {"credit_y": credit_y, "tiles": tiles,
                "title": title, "power": power, "faders": faders,
                "strip": (strip_top, strip_bot), "colw": colw}

    def tile_at(self, x, y):
        for rect, tid in self.layout()["tiles"]:
            if rect.collidepoint(x, y):
                return tid
        return None

    # ----------------------------------------------------------- page touch
    def hero_fader_value(self, rect, prm, y):
        frac = 1.0 - (y - rect.y) / max(1, rect.height - 1)
        return value_of(prm, frac)

    def touch_down(self, x, y, now):
        """Claim the gesture if it lands on a strip fader (returns True)."""
        for rect, fx, p in self.layout()["faders"]:
            if rect.inflate(30, 24).collidepoint(x, y):
                prm = self.reg.param(fx, p)
                self.drag = ('hero', fx, p, rect)
                self.dragging_idx = fx * self.reg.stride + p
                v = self.hero_fader_value(rect, prm, y)
                self.set_local(fx, p, v)
                self.send_param(fx, p, v)
                return True
        return False

    def drag_move(self, x, y):
        if not self.drag:
            return
        _, fx, p, rect = self.drag
        prm = self.reg.param(fx, p)
        v = self.hero_fader_value(rect, prm, y)
        self.set_local(fx, p, v)
        self.send_param(fx, p, v)

    def drag_up(self, x, y):
        if not self.drag:
            return
        _, fx, p, rect = self.drag
        prm = self.reg.param(fx, p)
        v = self.hero_fader_value(rect, prm, y)
        self.set_local(fx, p, v)
        self.send_param(fx, p, v, final=True)
        self.drag = None
        self.dragging_idx = None

    # ---- tile taps / long press ----
    def on_press(self, action, now):
        """Finger-down on a registered hit: arm a long-press for tile hits."""
        self.lp_fired = False
        if action and action[0] in ('sm_tile', 'sm_name'):
            self.lp = [action[1], now]
        else:
            self.lp = None

    def lp_deadline(self):
        return self.lp[1] + LONG_PRESS if self.lp else None

    def tick(self, now, finger_down, moved):
        if self.lp is None:
            return False
        if not finger_down or moved:
            self.lp = None
            return False
        if now - self.lp[1] >= LONG_PRESS:
            tid = self.lp[0]
            self.lp = None
            self.lp_fired = True
            self.open_sheet(tid)
            return True
        return False

    def consume_lp_fired(self):
        f, self.lp_fired = self.lp_fired, False
        self.lp = None
        return f

    def tap(self, action):
        kind = action[0]
        if kind == 'sm_tile':
            tid = action[1]
            if tid == 'rack':
                self.open_sheet('rack')
                return
            self.focus = tid
            self.send_toggle(tid, not self.on[tid])
            self.rev += 1
        elif kind == 'sm_name':           # already-focused tile's name zone / strip title
            self.open_sheet(action[1])
        elif kind == 'sm_power':
            self.send_toggle(action[1], not self.on[action[1]])

    # ---------------------------------------------------------------- sheet
    def open_sheet(self, tid):
        self.sheet = ('rack',) if tid == 'rack' else ('fx', int(tid))
        self.scroll = 0
        self._sh = None
        self._rows = self.build_rows()
        self.rev += 1

    def close_sheet(self):
        if self.sheet is not None:
            self.sheet = None
            self._sh = None
            self.dragging_idx = None
            self.rev += 1

    SHEET_TOP = 124
    SHEET_FOOT = 56
    ROW_H = {'float': 112, 'bool': 84, 'enum': 96, 'chips': 150, 'lat': 120}

    def build_rows(self):
        """One row per param (deep sheet) -- generated from the registry."""
        if self.sheet is None:
            return []
        rows = []
        y = 0
        if self.sheet == ('rack',):
            rows.append({'p': 0, 'kind': 'chips', 'y': 0, 'h': self.ROW_H['chips']})
            rows.append({'p': -1, 'kind': 'lat', 'y': self.ROW_H['chips'], 'h': self.ROW_H['lat']})
            return rows
        fx = self.sheet[1]
        for prm in self.reg.by_fx[fx]["params"]:
            kind = prm["kind"]
            if kind == 'enum' and prm["name"] == "FFT Size":
                kind = 'chips'
            h = self.ROW_H[kind]
            rows.append({'p': prm["p"], 'kind': kind, 'y': y, 'h': h})
            y += h
        return rows

    def content_h(self):
        return sum(r['h'] for r in self._rows) + 16

    def viewport_h(self):
        return self.m.H - self.SHEET_TOP - self.SHEET_FOOT

    def max_scroll(self):
        return max(0, self.content_h() - self.viewport_h())

    def _row_prm(self, row):
        if self.sheet == ('rack',):
            return {"p": 0, "name": "Default FFT size", "min": 0.0, "max": 7.0,
                    "default": 2.0, "unit": "", "kind": "enum",
                    "enum": [str(s) for s in self.reg.fft_sizes]}
        return self.reg.param(self.sheet[1], row['p'])

    def _sheet_fx(self):
        return RACK_FX if self.sheet == ('rack',) else self.sheet[1]

    def row_geom(self, row):
        """Screen rects for a row at the current scroll."""
        m = self.m
        pygame = m.pygame
        y0 = self.SHEET_TOP + row['y'] - self.scroll
        x0, x1 = 28, m.W - 28
        g = {'row': pygame.Rect(x0, y0, x1 - x0, row['h'])}
        k = row['kind']
        if k == 'float':
            g['track'] = pygame.Rect(x0, y0 + 56, x1 - x0, 40)
        elif k == 'enum':
            g['dec'] = pygame.Rect(x0, y0 + 40, 90, 50)
            g['inc'] = pygame.Rect(x1 - 90, y0 + 40, 90, 50)
        elif k == 'chips':
            n = len(self.reg.fft_sizes)
            cw = (x1 - x0 - 3 * 8) // 4
            for i in range(n):
                r, c = divmod(i, 4)
                g[('chip', i)] = pygame.Rect(x0 + c * (cw + 8), y0 + 38 + r * 54, cw, 46)
        return g

    def sheet_hit(self, x, y):
        """-> (kind, ...) for a point on the sheet, or None."""
        if y < self.SHEET_TOP:
            if x < 190:
                return ('back',)
            if x > self.m.W - 190 and self.sheet and self.sheet[0] == 'fx':
                return ('onoff',)
            return None
        if y > self.m.H - self.SHEET_FOOT:
            return None
        for i, row in enumerate(self._rows):
            g = self.row_geom(row)
            if not g['row'].collidepoint(x, y):
                continue
            k = row['kind']
            if k == 'float' and g['track'].inflate(0, 40).collidepoint(x, y):
                return ('fader', i)
            if k == 'bool':
                return ('bool', i)
            if k == 'enum':
                if g['dec'].inflate(20, 20).collidepoint(x, y):
                    return ('step', i, -1)
                if g['inc'].inflate(20, 20).collidepoint(x, y):
                    return ('step', i, 1)
            if k == 'chips':
                for j in range(len(self.reg.fft_sizes)):
                    if g[('chip', j)].inflate(6, 6).collidepoint(x, y):
                        return ('chip', i, j)
            return None
        return None

    def _write(self, i, v, final=False):
        row = self._rows[i]
        fx = self._sheet_fx()
        p = row['p']
        self.set_local(fx, p, v)
        self.send_param(fx, p, v, final=final)

    def _fader_value(self, i, x):
        row = self._rows[i]
        tr = self.row_geom(row)['track']
        frac = (x - tr.x) / max(1, tr.width - 1)
        return value_of(self._row_prm(row), frac)

    def sheet_down(self, x, y, now):
        t = self.sheet_hit(x, y)
        self._sh = {'x0': x, 'y0': y, 'ly': y, 'axis': None, 'target': t,
                    'scroll0': self.scroll}
        if t and t[0] == 'fader':
            row = self._rows[t[1]]
            self.dragging_idx = self._sheet_fx() * self.reg.stride + row['p']

    def sheet_move(self, x, y):
        s = self._sh
        if s is None:
            return False
        dx, dy = x - s['x0'], y - s['y0']
        if s['axis'] is None and max(abs(dx), abs(dy)) >= AXIS_PX:
            fader = s['target'] and s['target'][0] == 'fader'
            s['axis'] = 'h' if (fader and abs(dx) >= abs(dy)) else 'v'
        if s['axis'] == 'h':
            self._write(s['target'][1], self._fader_value(s['target'][1], x))
            return True
        if s['axis'] == 'v':
            self.scroll = max(0, min(self.max_scroll(), self.scroll - (y - s['ly'])))
            s['ly'] = y
            return True
        return False

    def sheet_up(self, x, y):
        s, self._sh = self._sh, None
        self.dragging_idx = None
        if s is None:
            return
        t = s['target']
        if s['axis'] == 'h':
            self._write(t[1], self._fader_value(t[1], x), final=True)
            return
        if s['axis'] == 'v':
            if s['scroll0'] == 0 and self.scroll == 0 and (y - s['y0']) >= CLOSE_PULL_PX:
                self.close_sheet()
            return
        if t is None:
            return
        k = t[0]
        if k == 'back':
            self.close_sheet()
        elif k == 'onoff':
            fx = self.sheet[1]
            self.send_toggle(fx, not self.on[fx])
        elif k == 'fader':
            self._write(t[1], self._fader_value(t[1], x), final=True)
        elif k == 'bool':
            row = self._rows[t[1]]
            cur = self.get(self._sheet_fx(), row['p'])
            self._write(t[1], 0.0 if cur >= 0.5 else 1.0, final=True)
        elif k == 'step':
            row = self._rows[t[1]]
            prm = self._row_prm(row)
            cur = int(round(self.get(self._sheet_fx(), row['p'])))
            nv = max(int(prm["min"]), min(int(prm["max"]), cur + t[2]))
            self._write(t[1], float(nv), final=True)
        elif k == 'chip':
            self._write(t[1], float(t[2]), final=True)

    # -------------------------------------------------------------- drawing
    def render_page(self, ui, c):
        m = self.m
        pygame = m.pygame
        lay = self.layout()
        self.tile_rects = lay["tiles"]
        self.fader_rects = lay["faders"]
        # credit line
        m.blit_centered(c, m.font(24).render("SOUNDMAGIC  ·  Michael Norris", True, m.OFF_TEXT),
                        m.W / 2, lay["credit_y"])
        # tiles
        for rect, tid in lay["tiles"]:
            self._draw_tile(ui, c, rect, tid)
        self._draw_strip(ui, c, lay)

    def _draw_tile(self, ui, c, rect, tid):
        m = self.m
        pygame = m.pygame
        if tid == 'rack':
            act = ('sm_tile', 'rack')
            pressed = ui._pressed(act)
            m.rounded_slot(c, rect, pressed)
            col = m.BG if pressed else m.TRACK_ACTIVE
            m.blit_centered(c, m.font(36).render("RACK", True, col), rect.centerx, rect.centery - 12)
            ms = self.latency_s() * 1000.0
            m.blit_centered(c, m.font(26).render("%.0f ms" % ms, True, col if pressed else m.OFF_TEXT),
                            rect.centerx, rect.centery + 22)
            ui.hit.append((rect, act))
            return
        e = self.reg.by_fx[tid]
        on = bool(self.on[tid])
        tact = ('sm_tile', tid)
        if ui._pressed(tact):
            on = not on
        soon = not e.get("built", e["phase"] < 2)
        focused = tid == self.focus
        m.rounded_slot(c, rect, on, outline=(m.EMPTY_OUTLINE if soon else m.OFF_OUTLINE))
        if focused:
            pygame.draw.rect(c, m.ACCENT, rect.inflate(6, 6), 4, border_radius=m.RADIUS + 3)
        if on:
            col = m.BG
        else:
            col = m.OFF_TEXT if not soon else m.OFF_OUTLINE
        m.blit_centered(c, m.text_fit(e["code"], 32, col, rect.width - 10),
                        rect.centerx, rect.y + 36)
        if soon:
            m.blit_centered(c, m.font(20).render("soon", True,
                            m.OFF_OUTLINE if not on else m.ACCENT_DIM),
                            rect.centerx, rect.y + 64)
        # mini fader (hero P1)
        hl = self.heroes_of(tid)
        h0 = hl[0] if hl else None
        fr = norm_of(e["params"][h0], self.get(tid, h0)) if h0 is not None else 0.0
        bar = pygame.Rect(rect.x + 12, rect.bottom - 26, rect.width - 24, 10)
        pygame.draw.rect(c, m.ACCENT_DIM if on else (14, 0, 16), bar, border_radius=5)
        if fr > 0:
            fill = pygame.Rect(bar.x, bar.y, max(6, int(bar.width * fr)), bar.height)
            pygame.draw.rect(c, m.ACCENT if on else (m.OFF_OUTLINE if not soon else m.EMPTY_OUTLINE),
                             fill, border_radius=5)
        if focused:
            name = pygame.Rect(rect.x, rect.y, rect.width, int(rect.height * 0.55))
            low = pygame.Rect(rect.x, name.bottom, rect.width, rect.height - name.height)
            ui.hit.append((name, ('sm_name', tid)))
            ui.hit.append((low, tact))
        else:
            ui.hit.append((rect, tact))

    def _draw_strip(self, ui, c, lay):
        m = self.m
        pygame = m.pygame
        fx = self.focus
        e = self.reg.by_fx[fx]
        on = bool(self.on[fx])
        tr = lay["title"]
        pygame.draw.rect(c, m.OFF_OUTLINE, tr, 2, border_radius=m.RADIUS)
        m.blit_centered(c, m.text_fit("%s  ·  ALL PARAMS" % e["title"].upper(), 30,
                                      m.TRACK_ACTIVE, tr.width - 20), tr.centerx, tr.centery)
        ui.hit.append((tr, ('sm_name', fx)))
        pr = lay["power"]
        pact = ('sm_power', fx)
        pon = on != ui._pressed(pact)
        m.rounded_slot(c, pr, pon)
        m.blit_centered(c, m.font(34).render("ON" if on else "OFF", True,
                                             m.BG if pon else m.OFF_TEXT), pr.centerx, pr.centery)
        ui.hit.append((pr, pact))
        colors = m.SLIDER_COLORS
        for k, (rect, _fx, p) in enumerate(lay["faders"]):
            prm = e["params"][p]
            v = self.get(fx, p)
            fr = norm_of(prm, v)
            col = colors[k % len(colors)] if on else tuple(int(q * 0.45) for q in colors[k % len(colors)])
            cx = rect.centerx
            colw = lay["colw"]
            m.blit_centered(c, m.text_fit(prm["name"], 28, m.FG if on else m.OFF_TEXT, colw - 12),
                            cx, lay["strip"][0] + 80)
            gap = 4
            fh = int((rect.height - 2 * gap) * fr)
            if fh > 0:
                pygame.draw.rect(c, col, pygame.Rect(rect.x + gap, rect.bottom - gap - fh,
                                                     rect.width - 2 * gap, fh))
            pygame.draw.rect(c, m.TRACK_ACTIVE if on else m.TRACK_DIM, rect, 2)
            m.blit_centered(c, m.text_fit(fmt_value(prm, v), 36, m.FG if on else m.OFF_TEXT, colw - 12),
                            cx, rect.bottom + 30)
            if self.pedal.get(fx) == p:
                m.blit_centered(c, m.font(20).render("PEDAL", True, m.ACCENT if on else m.OFF_TEXT),
                                cx, lay["strip"][0] + 100 - 8)

    # ---- sheet drawing ----
    def render_sheet(self, ui, c):
        m = self.m
        pygame = m.pygame
        c.blit(m.bg_gradient(), (0, 0))
        rack = self.sheet == ('rack',)
        fx = self._sheet_fx()
        if not self._rows:
            self._rows = self.build_rows()
        self.scroll = max(0, min(self.max_scroll(), self.scroll))
        # rows (clipped to the viewport)
        vp = pygame.Rect(0, self.SHEET_TOP, m.W, self.viewport_h())
        old = c.get_clip()
        c.set_clip(vp)
        for i, row in enumerate(self._rows):
            g = self.row_geom(row)
            if g['row'].bottom < vp.top or g['row'].top > vp.bottom:
                continue
            self._draw_row(c, row, g, fx, rack)
        c.set_clip(old)
        # scroll thumb
        if self.max_scroll() > 0:
            th = max(40, int(vp.height * vp.height / self.content_h()))
            ty = vp.y + int((vp.height - th) * self.scroll / self.max_scroll())
            pygame.draw.rect(c, m.OFF_OUTLINE, pygame.Rect(m.W - 8, ty, 4, th), border_radius=2)
        # header
        pygame.draw.rect(c, m.BG, pygame.Rect(0, 0, m.W, self.SHEET_TOP - 4))
        back = pygame.Rect(16, 20, 150, 70)
        pygame.draw.rect(c, m.OFF_OUTLINE, back, 2, border_radius=m.RADIUS)
        m.blit_centered(c, m.font(36).render("‹ BACK", True, m.TRACK_ACTIVE), back.centerx, back.centery)
        title = "RACK" if rack else self.reg.by_fx[fx]["title"].upper()
        m.blit_centered(c, m.text_fit(title, 40, m.FG, m.W - 400), m.W / 2, 56)
        if not rack:
            pr = pygame.Rect(m.W - 166, 20, 150, 70)
            on = bool(self.on[fx])
            m.rounded_slot(c, pr, on)
            m.blit_centered(c, m.font(36).render("ON" if on else "OFF", True,
                                                 m.BG if on else m.OFF_TEXT), pr.centerx, pr.centery)
        pygame.draw.line(c, m.FG, (m.PAD_X, self.SHEET_TOP - 6), (m.W - m.PAD_X, self.SHEET_TOP - 6), 1)
        # footer credit
        pygame.draw.rect(c, m.BG, pygame.Rect(0, m.H - self.SHEET_FOOT, m.W, self.SHEET_FOOT))
        m.blit_centered(c, m.font(26).render("SoundMagic Spectral · Michael Norris", True, m.OFF_TEXT),
                        m.W / 2, m.H - self.SHEET_FOOT // 2)

    def _draw_row(self, c, row, g, fx, rack):
        m = self.m
        pygame = m.pygame
        k = row['kind']
        r = g['row']
        pygame.draw.line(c, m.EMPTY_OUTLINE, (r.x, r.bottom - 1), (r.right, r.bottom - 1), 1)
        if k == 'lat':
            ms = self.latency_s() * 1000.0
            m.blit_centered(c, m.font(30).render("CHAIN LATENCY", True, m.OFF_TEXT), r.centerx, r.y + 34)
            m.blit_centered(c, m.font(64).render("%.1f ms" % ms, True, m.FG), r.centerx, r.y + 82)
            return
        prm = self._row_prm(row)
        v = self.get(fx, row['p'])
        label = prm["name"]
        c.blit(m.text_fit(label, 34, m.TRACK_ACTIVE, r.width - 260), (r.x, r.y + 10))
        vs = m.font(34).render(fmt_value(prm, v), True, m.FG)
        if k not in ('chips', 'bool'):
            c.blit(vs, (r.right - vs.get_width(), r.y + 10))
        if k == 'float':
            tr = g['track']
            self._h_slider(c, tr, norm_of(prm, v))
        elif k == 'bool':
            on = v >= 0.5
            tgl = pygame.Rect(r.right - 130, r.y + 28, 130, 44)
            m.rounded_slot(c, tgl, on)
            m.blit_centered(c, m.font(30).render("ON" if on else "OFF", True,
                                                 m.BG if on else m.OFF_TEXT), tgl.centerx, tgl.centery)
        elif k == 'enum':
            for key, glyph in (('dec', '<'), ('inc', '>')):
                pygame.draw.rect(c, m.TRACK_ACTIVE, g[key], 2, border_radius=m.RADIUS)
                m.blit_centered(c, m.font(44).render(glyph, True, m.TRACK_ACTIVE),
                                g[key].centerx, g[key].centery)
            m.blit_centered(c, m.text_fit(fmt_value(prm, v), 40, m.FG, r.width - 230),
                            r.centerx, g['dec'].centery)
        elif k == 'chips':
            cur = int(round(v))
            for j, size in enumerate(self.reg.fft_sizes):
                cr = g[('chip', j)]
                on = j == cur
                m.rounded_slot(c, cr, on)
                m.blit_centered(c, m.text_fit(str(size), 30, m.BG if on else m.OFF_TEXT, cr.width - 6),
                                cr.centerx, cr.centery)

    def _h_slider(self, c, tr, frac):
        m = self.m
        pygame = m.pygame
        pygame.draw.rect(c, (14, 0, 16), tr, border_radius=m.RADIUS)
        fw = int((tr.width - 10) * frac)
        if fw > 0:
            pygame.draw.rect(c, m.SLIDER_COLORS[0], pygame.Rect(tr.x + 5, tr.y + 5, fw, tr.height - 10),
                             border_radius=m.RADIUS - 6)
        pygame.draw.rect(c, m.OFF_OUTLINE, tr, 2, border_radius=m.RADIUS)
        tx = tr.x + 5 + fw
        pygame.draw.rect(c, m.TRACK_ACTIVE,
                         pygame.Rect(max(tr.x + 4, min(tr.right - 12, tx - 4)), tr.y + 4, 8, tr.height - 8),
                         border_radius=3)


def ui_menu_index(m):
    return m.NORRIS_MENU

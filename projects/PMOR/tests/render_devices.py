#!/usr/bin/env python3
"""render_devices.py [OUTDIR] [--save] [--src DIR] -- offscreen render + design-system checks for the PMOR device patchers (no Max, no screen capture).

Draws every presentation-mode box of max/devices/*.maxpat (and every nested editor) onto a PNG at 2x with the real fonts (Arial via Pillow when installed; otherwise
a rectangle-only render and character-width estimates). Editors with pages (drums / synth / sampler editors, the control lane's FX banks) get one PNG per page.
It is a model of Max's drawing, not a screenshot: widget shapes are schematic, text metrics are Arial at fontsize px, dynamic strings use SAMPLES below.

Checks (the design system of make_devices.py, see max/UI_AUDIT.md):
  clipping  every presentation rect lies inside the device (devicewidth x height) / the editor window
  overlap   non-background widgets do not overlap (text counted by its real extent), per page
  contrast  every text colour vs the surface it sits on: >= 7:1 (the type scale has no small print; DISABLED is exempt)
  palette   every colour attribute is one of make_devices.THEME; panels are surfaces (GROUND / PANEL / RAISED / DIVIDER) only; live.* objects carry the ACTIVE-state colours
  font      one family (FONT), sizes only from the type scale (10 / 11 / 12 / 14)
  grid      every presentation x / y / w / h is a multiple of 4 (hairline dividers, <= 2 px thick, excepted)
  fit       a text's measured width (and wrapped line count) fits its box; menus fit their longest item
  layout    the card is inset 8 px left / right, starts under the 32 px header; the title is centred; device height 169
Usage: render_devices.py [OUTDIR] [--save]   --save writes the committed renders to max/ui_renders/.  --src DIR renders the .maxpat files of another folder (before / after audits).
Exit 1 on any problem.
"""
import glob, importlib.util, json, math, os, re, struct, sys, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
_spec = importlib.util.spec_from_file_location("make_devices", os.path.join(ROOT, "max", "devices", "make_devices.py"))
MD = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(MD)
THEME = MD.THEME
SURFACES = [THEME[k] for k in ("GROUND", "PANEL", "RAISED", "DIVIDER")]
COLOR_ATTRS = ["bgcolor", "textcolor", "textoncolor", "textoffcolor", "needlecolor", "outlinecolor", "checkedcolor", "elementcolor", "bgoncolor", "bgfillcolor", "knobcolor",
               "dialcolor", "activedialcolor", "activefgdialcolor", "fgdialcolor", "activeneedlecolor", "panelcolor", "activebgcolor", "activebgoncolor", "activetextcolor",
               "activetextoncolor", "lcdbgcolor", "lcdcolor", "inactivelcdcolor", "hltcolor", "hlttextcolor", "tricolor", "bordercolor", "focusbordercolor", "labeltextcolor",
               "arrowcolor", "framecolor", "uncheckedcolor", "tabcolor", "htabcolor", "htextcolor"]
LIVE_ACTIVE = {"live.text": ["activebgcolor", "activebgoncolor", "activetextcolor", "activetextoncolor"],
               "live.menu": ["activebgcolor", "textcolor", "lcdbgcolor", "lcdcolor", "hltcolor", "hlttextcolor"],
               "live.dial": ["activedialcolor", "activeneedlecolor", "needlecolor", "dialcolor", "textcolor"]}
TEXT_CLASSES = ("comment", "umenu", "textbutton", "live.text", "live.menu", "flonum", "textedit", "tab")
SCALE = 2
BG_CLASSES = {"panel"}
SIZES = (MD.T_LABEL, MD.T_BODY, MD.T_HEAD, MD.T_TITLE)
NEED = 7.0

# realistic runtime strings (the patcher stores placeholders); longest plausible value per widget
SAMPLES = {"status": "STANDALONE (unplugged)", "selinfo": "pad 12 KICK", "sndhead": "KICK sounds of pad 12", "samples": "pull KICK_808 12/40 left 28", "transport": "play bpm 124.0 clk follow cpu 31% ovr 0 drop 0",
           "joinline": "joined 1536  latency 21.3 ms", "poolinfo": "pull KICK_808 12/40 left 28", "store": "patch 3  store ok", "unknown": "+12 unknown params", "ver": "fw 59aa159 proto 12",
           "saveinfo": "overwrite ROOM? click save again", "fxsel": "FX1.01 ST4", "seed": "seed 1234567",
           "snapline": "synced: 112 params written to PMOR patch 12, 3 clamped by PMOR", "snaphint": "load on PMOR = write the set's params onto PMOR's current patch (12)"}
READOUT = "2200 Hz"

_FC = {}


def font(size, bold=False):
    from PIL import ImageFont
    k = (size, bold)
    if k not in _FC:
        paths = ["/System/Library/Fonts/Supplemental/Arial Bold.ttf" if bold else "/System/Library/Fonts/Supplemental/Arial.ttf",
                 "/Library/Fonts/Arial Bold.ttf" if bold else "/Library/Fonts/Arial.ttf",
                 "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf" if bold else "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
                 "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"]
        for p in paths:
            if os.path.exists(p):
                _FC[k] = ImageFont.truetype(p, int(round(size * SCALE)))
                break
        else:
            raise OSError("no Arial-metric font found")
    return _FC[k]


try:
    from PIL import Image, ImageDraw
    font(10)
    HAVE_PIL = True
except Exception:
    HAVE_PIL = False


def lum(c):
    def f(v):
        return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
    return 0.2126 * f(c[0]) + 0.7152 * f(c[1]) + 0.0722 * f(c[2])


def ratio(a, b):
    la, lb = lum(a), lum(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


def rgb(c):
    return tuple(int(max(0, min(1, v)) * 255 + 0.5) for v in c[:3])


def same(a, b):
    return all(abs(x - y) < 6e-3 for x, y in zip(a[:3], b[:3]))


def tw(text, size, bold=False):
    """text width in device px"""
    if HAVE_PIL:
        return font(size, bold).getlength(text) / SCALE
    return 0.55 * size * len(text)


def lh(size):
    return int(round(size * 1.25))


def wrap(text, w, size, bold=False):
    lines, cur = [], ""
    for word in text.split(" "):
        t = (cur + " " + word) if cur else word
        if tw(t, size, bold) <= w or not cur:
            cur = t
        else:
            lines.append(cur)
            cur = word
    lines.append(cur)
    return lines


_BANKS = None


SCOPE = [""]
_PAGES = None
LAYER_PAGE = 10                                       # pmor_bank.js: bank pages >= 10 are the layers of the drum synth (tab 0)
DS_LAYERS = json.load(open(os.path.join(ROOT, "max", "devices", "dslayers.json")))     # drum synth layers per engine (KICK SNARE HAT PERC), from the schema
DS_ENG = ["KICK", "SNARE", "HAT", "PERC"]
DS_PAD = [12, 14, 2, 0]                                # a pad of each engine, shown selected in that engine's renders
SELECTED = {"pc_12", "sc_3", "ag_2", "sl_5"}          # drawn in their selected state (ACCENT) in the renders


def page_of(w):
    """editor page of a bank control / its label / readout (None = always visible or not a bank widget)"""
    global _BANKS, _PAGES
    if _PAGES is None:
        _PAGES = json.load(open(os.path.join(ROOT, "max", "devices", "pages.json")))
    pv = _PAGES.get(SCOPE[0], {}).get(w.get("varname", ""))
    if pv is not None:
        return pv
    if _BANKS is None:
        _BANKS = json.load(open(os.path.join(ROOT, "max", "devices", "banks.json")))
    h = re.fullmatch(r"bh(\d+)_.*", w.get("varname", ""))
    if h:
        return int(h.group(1))
    f = re.fullmatch(r"p(\d+)", w.get("varname", ""))
    if f and 5120 <= int(f.group(1)) < 5248:
        return (int(f.group(1)) - 5120) // 16            # FX engage cells: one bank at a time (pmor_fx.js)
    m = re.fullmatch(r"(?:bk[rnmt]?|bl|bv)_(.+)", w.get("varname", ""))
    if not m or m.group(1) not in _BANKS:
        return None
    pg = _BANKS[m.group(1)][2]
    return pg if pg >= 0 else None


PAGE_NAMES = {"PMOR_lane_drums_p_editor": ["SOUND", "GROUP", "MIX", "SEQ", "DEEP"], "PMOR_lane_synth_p_editor": ["OSC", "AMP", "FILTER", "GEN", "LFO"],
              "PMOR_lane_sampler_p_editor": ["SLOT", "MIX", "SEQ", "ENV"], "PMOR_lane_control": ["bank1", "bank2"]}
PAGE_LIMIT = {"PMOR_lane_control": 2}


def getbg(b, default):
    v = b.get("bgfillcolor")
    if isinstance(v, dict):
        return v["color"]
    return b.get("bgcolor", default)


def items_of(b):
    return [i for i in (b.get("items") or []) if i != ","]


def sample_text(b):
    vn = b.get("varname") or ""
    if vn.startswith("bv_"):
        return READOUT
    if vn == "selinfo" and SCOPE[0] in ("synth", "sampler"):
        return "algo 2 WAVE" if SCOPE[0] == "synth" else "slot 5"
    if vn in SAMPLES:
        return SAMPLES[vn]
    return b.get("text") or ""


def widget_text(b, page=None):
    """(text shown in the idle state, longest text it can show)"""
    cls = b["maxclass"]
    if cls == "comment":
        t = sample_text(b)
        return t, t
    if cls in ("umenu", "live.menu"):
        its = items_of(b)
        if not its and cls == "live.menu":
            its = list((b.get("saved_attribute_attributes") or {}).get("valueof", {}).get("parameter_enum", []))
        if not its:
            its = ["(none)"]
        shown = its[page] if (b.get("varname") == "pg" and page is not None and page < len(its)) else its[0]
        return shown, max(its, key=lambda s: tw(s, b.get("fontsize", 11)))
    if cls in ("textbutton", "live.text"):
        t = b.get("text") or ""
        longest = max([t, b.get("texton") or ""], key=lambda s: len(s))
        return t, longest
    if cls == "tab":
        its = list(b.get("tabs") or [])
        return its[page] if page is not None and page < len(its) else (its[0] if its else ""), ""
    if cls == "flonum":
        return "120.00", "120.00"
    if cls == "textedit":
        return b.get("text") or "NAME", "WWWWWWWWW"
    return "", ""


def justify_of(b):
    return {0: "l", 1: "c", 2: "r"}.get(b.get("textjustification", 0), "l")


def text_extent(b, page=None):
    """presentation rect of the text actually drawn (comments only: they are wider than their text)"""
    r = b["presentation_rect"]
    t = widget_text(b, page)[1]
    size, bold = b.get("fontsize", 12), bool(b.get("fontface", 0))
    wd = min(r[2], tw(t, size, bold) + 4)
    j = justify_of(b)
    x = r[0] if j == "l" else (r[0] + (r[2] - wd) / 2 if j == "c" else r[0] + r[2] - wd)
    return [x, r[1], wd, r[3]]


def surface_under(b, panels, bg):
    r = b["presentation_rect"]
    under = bg
    for pnl in panels:                                      # the topmost panel containing the widget's centre
        q = pnl["presentation_rect"]
        if q[0] <= r[0] + r[2] / 2 <= q[0] + q[2] and q[1] <= r[1] + r[3] / 2 <= q[1] + q[3] and min(q[2], q[3]) > 2:
            under = getbg(pnl, bg)
    return under


def text_pair(b, under):
    """list of (text colour, surface) the widget's text is drawn in (idle + on state)"""
    cls = b["maxclass"]
    out = []
    if cls == "comment":
        out.append((b.get("textcolor"), b.get("bgcolor") if b.get("bgcolor") and b.get("bgcolor")[3] > 0 else under))
    elif cls in ("umenu", "flonum", "textedit"):
        out.append((b.get("textcolor"), getbg(b, under)))
    elif cls == "tab":
        out.append((b.get("textcolor"), b.get("tabcolor")))
        out.append((b.get("htextcolor"), b.get("htabcolor")))
    elif cls == "textbutton":
        out.append((b.get("textcolor"), b.get("bgcolor")))
        if b.get("texton"):
            out.append((b.get("textoncolor", b.get("textcolor")), b.get("bgoncolor", b.get("bgcolor"))))
    elif cls == "live.text":
        out.append((b.get("activetextcolor"), b.get("activebgcolor")))
        out.append((b.get("activetextoncolor"), b.get("activebgoncolor")))
    elif cls == "live.menu":
        out.append((b.get("textcolor"), b.get("activebgcolor")))
    return [(t, s) for t, s in out if t is not None and s is not None]


def subs(p, name):
    for b in p["boxes"]:
        b = b["box"]
        if isinstance(b.get("patcher"), dict):
            yield name + "_" + (b.get("text") or "patcher").replace(" ", "_"), b["patcher"]


def check(path, outdir):
    base = os.path.splitext(os.path.basename(path))[0].replace(" ", "_")
    top = json.load(open(path))["patcher"]
    res = []
    for nm, p in [(base, top)] + list(subs(top, base)):
        res.extend(check_one(p, nm, outdir))
    return res


def pages_of(widgets, pname):
    pg = sorted({page_of(w) for w in widgets if page_of(w) is not None})
    pg = pg[:PAGE_LIMIT.get(pname, len(pg))]
    return pg or [None]


def draw_widget(d, b, bg, page, layer_pg=None):
    S = SCALE
    cls, r = b["maxclass"], b["presentation_rect"]
    x0, y0, x1, y1 = [v * S for v in (r[0], r[1], r[0] + r[2], r[1] + r[3])]
    size, bold = b.get("fontsize", 12), bool(b.get("fontface", 0))

    def txt(text, col, j="l", pad=4, rect=None, sz=size, bd=bold, vcenter=False):
        rect = rect or (x0, y0, x1, y1)
        f = font(sz, bd)
        lines = wrap(text, (rect[2] - rect[0]) / S - pad, sz, bd) if cls == "comment" else [text]
        ty = rect[1] + (2 * S if not vcenter else ((rect[3] - rect[1]) - len(lines) * lh(sz) * S) / 2)
        for ln in lines:
            wd = f.getlength(ln)
            tx = rect[0] + pad / 2 * S if j == "l" else (rect[0] + ((rect[2] - rect[0]) - wd) / 2 if j == "c" else rect[2] - wd - pad / 2 * S)
            d.text((tx, ty), ln, font=f, fill=rgb(col))
            ty += lh(sz) * S

    if cls == "panel":
        rad = int(b.get("rounded", 0) * S)
        c = rgb(getbg(b, bg))
        if rad:
            d.rounded_rectangle((x0, y0, x1 - 1, y1 - 1), radius=rad, fill=c)
        else:
            d.rectangle((x0, y0, x1 - 1, y1 - 1), fill=c)
    elif cls == "comment":
        t = sample_text(b)
        if b.get("bgcolor") and b["bgcolor"][3] > 0:
            d.rectangle((x0, y0, x1 - 1, y1 - 1), fill=rgb(b["bgcolor"]))
        txt(t, b.get("textcolor", [1, 1, 1, 1]), justify_of(b)[0])
    elif cls in ("umenu", "live.menu"):
        fillc = getbg(b, bg) if cls == "umenu" else b.get("activebgcolor", b.get("lcdbgcolor", bg))
        d.rounded_rectangle((x0, y0, x1 - 1, y1 - 1), radius=3 * S, fill=rgb(fillc))
        shown, _ = widget_text(b, page)
        tc = b.get("textcolor", [0.8, 0.8, 0.8, 1])
        txt(shown, tc, "l", pad=10, rect=(x0, y0, x1 - 14 * S, y1), vcenter=True)
        ax, ay = x1 - 9 * S, (y0 + y1) / 2
        d.polygon([(ax - 3 * S, ay - 1.5 * S), (ax + 3 * S, ay - 1.5 * S), (ax, ay + 2.5 * S)], fill=rgb(b.get("tricolor", b.get("arrowcolor", THEME["TEXT2"]))))
    elif cls == "tab":
        its = list(b.get("tabs") or [])
        n = max(1, len(its))
        for i, t in enumerate(its):
            tx0 = x0 + (x1 - x0) * i / n
            tx1 = x0 + (x1 - x0) * (i + 1) / n - 4 * S
            on = (page == i) if page is not None else (i == 0)
            d.rounded_rectangle((tx0, y0, tx1, y1 - 1), radius=4 * S, fill=rgb(b["htabcolor"] if on else b["tabcolor"]))
            f = font(size, bold)
            wd = f.getlength(t)
            d.text((tx0 + (tx1 - tx0 - wd) / 2, y0 + ((y1 - y0) - lh(size) * S) / 2), t, font=f, fill=rgb(b["htextcolor"] if on else b["textcolor"]))
    elif cls in ("textbutton", "live.text"):
        sel = (b.get("varname") in SELECTED and not (layer_pg is not None and str(b.get("varname")).startswith("pc_"))) or (layer_pg is not None and b.get("varname") in ("ly_%d" % ((layer_pg - LAYER_PAGE) & 15), "pc_%d" % DS_PAD[(layer_pg - LAYER_PAGE) >> 4]))
        fillc = (b.get("bgoncolor") if sel else b.get("bgcolor")) if cls == "textbutton" else b.get("activebgcolor", b.get("bgcolor"))
        tc = (b.get("textoncolor") if sel else b.get("textcolor")) if cls == "textbutton" else b.get("activetextcolor", b.get("textcolor"))
        d.rounded_rectangle((x0, y0, x1 - 1, y1 - 1), radius=int((b.get("rounded", 4) or 4) * S), fill=rgb(fillc))
        txt(b.get("text") or "", tc, "c", pad=4, vcenter=True)
    elif cls in ("flonum", "textedit"):
        d.rounded_rectangle((x0, y0, x1 - 1, y1 - 1), radius=3 * S, fill=rgb(getbg(b, bg)), outline=rgb(THEME["DIVIDER"]), width=S)
        txt(widget_text(b)[0], b.get("textcolor"), "l", pad=8, vcenter=True)
    elif cls in ("dial", "live.dial"):
        cx, cy, rad = (x0 + x1) / 2, (y0 + y1) / 2, min(x1 - x0, y1 - y0) / 2 - S
        ring = b.get("outlinecolor") if cls == "dial" else b.get("activedialcolor", b.get("dialcolor"))
        if cls == "dial":
            d.ellipse((cx - rad, cy - rad, cx + rad, cy + rad), fill=rgb(b.get("bgcolor", bg)), outline=rgb(ring), width=max(S, int(rad / 8)))
        else:
            d.ellipse((cx - rad, cy - rad, cx + rad, cy + rad), outline=rgb(THEME["DIVIDER"]), width=max(S, int(rad / 6)))
            d.arc((cx - rad, cy - rad, cx + rad, cy + rad), 135, 135 + 150, fill=rgb(ring), width=max(S, int(rad / 6)))
        a = math.radians(135 + 150)
        nc = b.get("needlecolor") if cls == "dial" else b.get("activeneedlecolor", b.get("needlecolor"))
        d.line((cx, cy, cx + math.cos(a) * rad * 0.85, cy + math.sin(a) * rad * 0.85), fill=rgb(nc or ring), width=max(S, int(rad / 8)))
    elif cls == "toggle":
        d.rounded_rectangle((x0, y0, x1 - 1, y1 - 1), radius=3 * S, fill=rgb(b.get("bgcolor", bg)), outline=rgb(b.get("elementcolor", THEME["DIVIDER"])), width=S)
    elif cls == "slider":
        d.rounded_rectangle((x0, y0, x1 - 1, y1 - 1), radius=3 * S, fill=rgb(b.get("bgcolor", bg)))
        kx = x0 + (x1 - x0) * 0.4
        d.rounded_rectangle((kx - 3 * S, y0 + S, kx + 3 * S, y1 - S - 1), radius=2 * S, fill=rgb(b.get("knobcolor", THEME["TEXT1"])))


def check_one(p, pname, outdir):
    if "devicewidth" in p:
        W, H = int(p["devicewidth"]), int(p["openrect"][3])
        kind = "device"
    else:
        W, H = int(p["rect"][2]), int(p["rect"][3])
        kind = "editor"
    SCOPE[0] = ""
    for bx in p["boxes"]:
        t = bx["box"].get("text") or ""
        if t.startswith("js pmor_bank.js"):
            w_ = [x for x in t.split()[2:] if x != "gated"]
            SCOPE[0] = w_[0] if w_ else ""
    tabs_ = [bx["box"] for bx in p["boxes"] if bx["box"]["maxclass"] == "tab"]
    if tabs_:
        PAGE_NAMES[pname] = list(tabs_[0]["tabs"])
    bg = p.get("bgcolor", [0, 0, 0, 1])
    widgets = [b["box"] for b in p["boxes"] if b["box"].get("presentation") == 1]
    panels = [b for b in widgets if b["maxclass"] in BG_CLASSES]
    top = [b for b in widgets if b["maxclass"] not in BG_CLASSES]
    problems = []

    def nm(b):
        return b.get("varname") or b.get("text") or b["maxclass"]

    def add(kindname, msg):
        problems.append("%s: %s" % (kindname, msg))

    for b in widgets:                                        # ---- per-widget checks (page independent)
        r = b["presentation_rect"]
        if r[0] < 0 or r[1] < 0 or r[0] + r[2] > W or r[1] + r[3] > H:
            add("clipping", "'%s' %s outside %dx%d" % (nm(b), r, W, H))
        hair = b["maxclass"] == "panel" and (min(r[2], r[3]) <= 2 or r[:2] == [0, 0])      # hairline dividers; the full-device ground (169 px is Live's height)
        if not hair and any(abs(v - round(v / MD.GRID) * MD.GRID) > 1e-6 for v in r):
            add("grid", "'%s' rect %s is not on the %d px grid" % (nm(b), r, MD.GRID))
        if b["maxclass"] in TEXT_CLASSES:
            if b.get("fontsize") not in SIZES:
                add("font", "'%s' fontsize %s not in the type scale %s" % (nm(b), b.get("fontsize"), list(SIZES)))
            if b.get("fontname") != MD.FONT:
                add("font", "'%s' fontname %r is not %r" % (nm(b), b.get("fontname"), MD.FONT))
        for a in LIVE_ACTIVE.get(b["maxclass"], []):
            if a not in b:
                add("palette", "'%s' (%s) lacks %s: Live paints its own theme colour in the active state" % (nm(b), b["maxclass"], a))
        for a in COLOR_ATTRS:
            v = b.get(a)
            if isinstance(v, dict):
                v = v.get("color")
            if v is None:
                continue
            if not any(same(v, c) for c in THEME.values()):
                add("palette", "'%s' %s=%s is not a THEME colour" % (nm(b), a, [round(x, 3) for x in v]))
            if b["maxclass"] == "panel" and a in ("bgcolor", "bgfillcolor") and not any(same(v, c) for c in SURFACES):
                add("palette", "'%s' panel colour %s is not a surface" % (nm(b), a))
        if b["maxclass"] in ("umenu", "toggle", "dial", "slider", "flonum") and not any(a in b for a in ("bgcolor", "bgfillcolor")):
            add("palette", "'%s' (%s) has no background colour (would draw Max default grey)" % (nm(b), b["maxclass"]))
        under = surface_under(b, panels, bg)
        for tc, sf in text_pair(b, under):
            cr = ratio(tc, sf)
            if cr < NEED and not same(tc, THEME["DISABLED"]):
                add("contrast", "'%s' %.2f < %.1f" % (nm(b), cr, NEED))
        if b["maxclass"] in TEXT_CLASSES:
            shown, longest = widget_text(b)
            size, bold = b.get("fontsize", 12), bool(b.get("fontface", 0))
            if b["maxclass"] == "comment":
                if longest:
                    lines = wrap(longest, r[2] - 4, size, bold)
                    if len(lines) * lh(size) > r[3] + 1:
                        add("fit", "'%s' text %r needs %d line(s) x %d px, box is %d px high (%d wide)" % (nm(b), longest[:40], len(lines), lh(size), r[3], r[2]))
            elif longest:
                pad = {"umenu": 26, "live.menu": 26, "textbutton": 10, "live.text": 4, "flonum": 10, "textedit": 10}[b["maxclass"]]
                if tw(longest, size, bold) + pad > r[2]:
                    add("fit", "'%s' text %r is %.0f px, box %d wide (pad %d)" % (nm(b), longest[:30], tw(longest, size, bold), r[2], pad))
                if lh(size) > r[3]:
                    add("fit", "'%s' text %d px high in a %d px box" % (nm(b), lh(size), r[3]))
    titles = [b for b in widgets if b.get("varname") == "title"]
    if kind == "device":                                     # ---- layout
        cards = [q for q in panels if min(q["presentation_rect"][2:]) > 2][1:2]
        for q in cards:
            r = q["presentation_rect"]
            if r[0] != 8 or W - (r[0] + r[2]) != 8 or r[1] != 32:
                add("layout", "card %s must start at y 32 with 8 px side margins" % (r,))
        if not titles:
            add("layout", "no title widget (varname title)")
        if H != 169:
            add("layout", "device height %d != 169" % H)
    for b in titles:
        r = b["presentation_rect"]
        if abs((r[0] + r[2] / 2) - W / 2) > 0.5 or b.get("textjustification") != 1:
            add("layout", "title is not centred (centre %.1f of %.1f, justification %s)" % (r[0] + r[2] / 2, W / 2, b.get("textjustification")))
    outs = []                                                # ---- per page: overlap + render
    os.makedirs(outdir, exist_ok=True)
    for pg in pages_of(top, pname):
        lyr = pg is not None and pg >= LAYER_PAGE            # pages 10 + engine*16 + layer: that layer of that engine's drum synth, shown on tab 0 together with everything of tab 0
        vp = (lambda b: page_of(b) in (None, 0, pg)) if lyr else (lambda b: page_of(b) is None or page_of(b) == pg)
        vis = [b for b in top if vp(b)]
        if lyr:                                              # what pmor_bank.js does for the selected pad of this engine: layer chips + section title
            ge, li = (pg - LAYER_PAGE) >> 4, (pg - LAYER_PAGE) & 15
            vv = []
            for b in vis:
                vn = b.get("varname", "")
                m = re.fullmatch(r"ly_(\d+)", vn)
                if m and int(m.group(1)) >= len(DS_LAYERS[ge]):
                    continue
                if m:
                    b = dict(b, text=DS_LAYERS[ge][int(m.group(1))])
                elif vn == "dshead":
                    b = dict(b, text="%s SYNTH OF PAD %d  (pick a layer)" % (DS_ENG[ge], DS_PAD[ge]))
                vv.append(b)
            vis = vv
        pgd = 0 if lyr else pg                                # the page the tab bar / selected chips are drawn for
        for i in range(len(vis)):
            for j in range(i + 1, len(vis)):
                a, b = vis[i], vis[j]
                ea = text_extent(a, pgd) if a["maxclass"] == "comment" else a["presentation_rect"]
                eb = text_extent(b, pgd) if b["maxclass"] == "comment" else b["presentation_rect"]
                if ea[0] < eb[0] + eb[2] and eb[0] < ea[0] + ea[2] and ea[1] < eb[1] + eb[3] and eb[1] < ea[1] + ea[3]:
                    add("overlap", "'%s' and '%s'%s" % (nm(a), nm(b), "" if pg is None else " (page %s)" % pg))
        names = PAGE_NAMES.get(pname)
        suffix = "" if pg is None else "_" + ("PAD_" + DS_ENG[(pg - LAYER_PAGE) >> 4] + "_" + DS_LAYERS[(pg - LAYER_PAGE) >> 4][(pg - LAYER_PAGE) & 15] if lyr else (names[pg] if names and pg < len(names) else "page%d" % pg))
        out = os.path.join(outdir, pname + suffix + ".png")
        if HAVE_PIL:
            im = Image.new("RGB", (W * SCALE, H * SCALE), rgb(bg))
            dr = ImageDraw.Draw(im)
            for b in panels:
                if vp(b):
                    draw_widget(dr, b, bg, pgd, pg if lyr else None)
            for b in vis:
                draw_widget(dr, b, bg, pgd, pg if lyr else None)
            if tabs_:
                pass
            im.save(out)
        else:
            png_rects(out, W, H, bg, panels, vis)
        outs.append((out, [], len(widgets)))
    outs[0] = (outs[0][0], problems, len(widgets))
    return outs


def png_rects(path, w, h, bg, panels, vis):
    """stdlib fallback: filled rectangles only"""
    px = bytearray(bytes(rgb(bg)) * (w * SCALE * h * SCALE))
    for b in panels + vis:
        r = b["presentation_rect"]
        col = rgb(getbg(b, bg)) if b["maxclass"] == "panel" else rgb(b.get("textcolor") or b.get("bgcolor") or [.5, .5, .5, 1])
        for y in range(int(r[1] * SCALE), min(h * SCALE, int((r[1] + (r[3] if b["maxclass"] == "panel" else 2)) * SCALE))):
            for x in range(int(r[0] * SCALE), min(w * SCALE, int((r[0] + r[2]) * SCALE))):
                px[(y * w * SCALE + x) * 3:(y * w * SCALE + x) * 3 + 3] = bytes(col)
    raw = b"".join(b"\x00" + bytes(px[y * w * SCALE * 3:(y + 1) * w * SCALE * 3]) for y in range(h * SCALE))

    def chunk(t, dd):
        return struct.pack(">I", len(dd)) + t + dd + struct.pack(">I", zlib.crc32(t + dd) & 0xFFFFFFFF)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w * SCALE, h * SCALE, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    argv = sys.argv[1:]
    src = None
    if "--src" in argv:
        i = argv.index("--src")
        src = argv[i + 1]
        del argv[i:i + 2]
    args = [a for a in argv if not a.startswith("--")]
    save = "--save" in argv
    outdir = args[0] if args else (os.path.join(ROOT, "max", "ui_renders") if save else "ui_render")
    here = src or os.path.join(ROOT, "max", "devices")
    bad = 0
    for f in sorted(glob.glob(os.path.join(here, "*.maxpat"))):
        for out, problems, n in check(f, outdir):
            print("%s: %d presentation widgets -> %s" % (os.path.basename(f), n, out))
            for pr in problems:
                print("  PROBLEM", pr)
                bad += 1
    print("render_devices: %d problem(s)%s" % (bad, "" if HAVE_PIL else " (rectangle-only render: install Pillow for text renders and real text metrics)"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

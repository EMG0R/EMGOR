#!/usr/bin/env python3
"""make_devices.py -- writes the PMOR Max-for-Live device patchers (.maxpat JSON), pmor_ui.js, and with --amxd the .amxd wrappers.
   python3 make_devices.py [--amxd]        (.maxpat + pmor_ui.js -> max/devices, .amxd -> max/build)
Devices: PMOR core (iiii: engine + bus FX, perform view + editor) and PMOR lane drums / synth / sampler / control (mmmm).
Architecture (spec): three lane tracks (drums ch1 / synth ch2 incl. pitch bend / sampler ch3) + the CONTROL lane (ch4 control CCs incl. the random-FX seed
CC110-113, ch5 FX engage CCs) forward their MIDI unchanged to the ONE `PMOR core` device over the global [send pmor_midi]; the core runs the engine.
FX: the CONTROL lane carries the FX browser (bank / effect menus + the 128 engage cells + distortion algo as REAL Live parameters, pmor_fx.js navigates).
FX engage/params on the Mac come ONLY from PMOR (ch5 engage CCs, CCs, and synced params over CDC); the core never records or plays FX loops (g_render_notes_only).
A recorded automation lane moving a cell -> setparam -> SET_PARAM to PMOR (PMOR does not echo it back; its ack feeds the core). One mechanism.
Editors open in their own window only on request (pmor_edit.js). While closed they draw/poll nothing: pmor_ui.js / pmor_browser.js instances inside an editor are
`gated` (cache only), and no saved patcher contains a scheduler object (metro/qmetro/delay/pipe/...): all timing is js Tasks that exist only while needed
(pmor_session.js while connected, pmor_edit.js while an editor is open, pmor_join.js while armed). Session: perform = HELLO STUDIO, editor open = EDIT.
Every control is a plain UI object (dial/toggle/umenu/flonum), except the FX cells of the control lane (live.text/live.menu, see livectl); PMOR is the source of truth and
no value is restored from the Live set (parameter_initial). Controls are named p<id> (schema id); synth dials are s<k> (re-targeted per algo by pmor_ui.js).
Dial range: output = min + mult*step, steps 0..size-1 (floats: size 1001, mult=(max-min)/1000; integer params: size=max-min+1, mult 1).
The saved patchers contain NO port object: pmor_port.js creates it only when "connect" is switched on.
Theme: dark violet (see THEME below): GROUND / PANEL / RAISED surfaces, TEXT1 / TEXT2, ACCENT, one colour per engine, WARN / DANGER / OK. 4 px grid, 32 px header, one font.
"""
import json, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(os.path.dirname(HERE), "build")
SCH = json.load(open(os.path.join(HERE, "schema.json")))
E = {e[0]: e for e in SCH["schema"]}
# ---- THEME: one palette, one role per colour (mirrored in the //@THEME block of pmor_ui.js; tests/render_devices.py checks every colour attribute against it) ----------
# Dark violet. Text colours are >= 7:1 on every surface they sit on (GROUND / PANEL / RAISED); fills that carry dark text (engine colours, ACCENT, DANGER, WARN, OK) are >= 7:1 against GROUND.
def _hex(h):
    return [round(int(h[i:i + 2], 16) / 255.0, 4) for i in (1, 3, 5)] + [1.0]


THEME_HEX = {
    "GROUND":   "#0D0919",   # device ground, header strip, input wells (menus, dials, toggles, text fields)
    "PANEL":    "#221A3F",   # cards that group controls
    "RAISED":   "#3A2E73",   # neutral button fill (idle action)
    "DIVIDER":  "#5B4B9E",   # hairlines between zones (never text)
    "TEXT1":    "#F3EEFF",   # primary text: titles, values, menu items, button labels
    "TEXT2":    "#C9BCF0",   # secondary text: control labels, captions, hints
    "ACCENT":   "#BE9CFF",   # active / selected / primary action; identity of the core device
    "DRUMS":    "#FF8AD1",   # engine identity: drums (title, dial arcs, primary button of the drums device)
    "SYNTH":    "#7DB8FF",   # engine identity: synth
    "SAMPLER":  "#FFA866",   # engine identity: sampler
    "CONTROL":  "#4FD9C4",   # engine identity: control / FX
    "WARN":     "#FFD84D",   # warning / armed state
    "DANGER":   "#FF7A8A",   # destructive action (delete, empty bin)
    "OK":       "#A6E85A",   # connected / healthy
    "DISABLED": "#6E60A0",   # disabled control (pmor_ui.js dims firmware-unknown params); exempt from contrast rules
    "GRP_KICK":  "#3D1A33",  # drum group tints (surfaces behind TEXT1, >= 10:1): the 4 groups of PMOR's pad grid. Pad cells and the sound chips of the selected pad's group
    "GRP_SNARE": "#1B2F4D",
    "GRP_HAT":   "#4A3217",
    "GRP_PERC":  "#123F3A",
}
THEME = {k: _hex(v) for k, v in THEME_HEX.items()}
GROUND, PANEL, RAISED, DIVIDER, TEXT1, TEXT2, ACCENT = (THEME[k] for k in ("GROUND", "PANEL", "RAISED", "DIVIDER", "TEXT1", "TEXT2", "ACCENT"))
C_DRUMS, C_SYNTH, C_SAMPLER, C_CONTROL, WARN, DANGER, OK, DISABLED = (THEME[k] for k in ("DRUMS", "SYNTH", "SAMPLER", "CONTROL", "WARN", "DANGER", "OK", "DISABLED"))
GRP_TINT = [THEME["GRP_KICK"], THEME["GRP_SNARE"], THEME["GRP_HAT"], THEME["GRP_PERC"]]     # by drum group index (KICK SNARE HAT PERC)
FONT = "Arial"                                  # the one font family (Live substitutes its UI font; Arial is what Max shows outside Live)
T_TITLE, T_HEAD, T_BODY, T_LABEL = 14.0, 12.0, 11.0, 10.0       # the type scale: title (bold) / heading (bold) / body / label
GRID = 4                                        # every presentation rect is a multiple of 4
SOFT = (1 | 8 | 64)                  # flags that mean integer steps: QUANT, ENUM, INT


def scale_attrs(lo, hi, flags):
    """dial attributes for a schema range. output = min + mult*step."""
    if flags & SOFT:
        return {"min": lo, "size": int(round(hi - lo)) + 1, "mult": 1.0, "floatoutput": 0}
    return {"min": lo, "size": 1001, "mult": round((hi - lo) / 1000.0, 12), "floatoutput": 1}


def fill(c):
    """panel / umenu background: Max 9 panel and umenu take `bgfillcolor` (a colour dict), NOT bgcolor (measured: `getattrnames`)"""
    return {"angle": 270.0, "autogradient": 0, "color": list(c), "color1": list(c), "color2": list(c), "proportion": 0.39, "type": "color"}


class P:
    def __init__(self, w, h, gated=False):
        self.boxes, self.lines, self.n, self.w, self.h = [], [], 0, w, h
        self.gated = gated
        self.tx = self.ui = self.br = self.bk = None
        self.aux = 0
        self.page, self.dx, self.dy, self.scope, self.pages, self.hn = None, 0, 0, "", {}, 0     # tabbed editors: current page, presentation offset, bank scope, page table
        self.params = {}                                   # live.* parameter registry (obj id -> [longname, shortname, 0]) for the patcher's `parameters` block

    def add(self, cls, x, y, w, h, text=None, outs=1, ins=1, pres=None, **kw):
        self.n += 1
        b = {"id": "obj-%d" % self.n, "maxclass": cls, "numinlets": ins, "numoutlets": outs,
             "outlettype": kw.pop("outlettype", [""] * outs), "patching_rect": [x, y, w, h]}
        if text is not None:
            b["text"] = text
        b.update(kw)
        if pres:
            b["presentation"] = 1
            b["presentation_rect"] = [pres[0] + self.dx, pres[1] + self.dy, pres[2], pres[3]]
            if self.page is not None:                      # a tabbed editor: every presentation object belongs to the page being built (pmor_bank.js shows one page)
                vn = b.get("varname")
                if vn is None:
                    self.hn += 1
                    vn = b["varname"] = "bh%d_x%d" % (self.page, self.hn)
                elif not re.match(r"(bh\d+_|bk|bl_|bv_)", vn):
                    self.pages[vn] = self.page
                if self.page != 0:
                    b["hidden"] = 1
        self.boxes.append({"box": b})
        return b["id"]

    def hid(self, text, outs=1, ins=1, cls="newobj", **kw):
        """engine-room object laid out in a column under the presentation area"""
        self.aux += 1
        x, y = 20 + (self.aux // 24) * 190, 760 + (self.aux % 24) * 28
        return self.add(cls, x, y, 180, 22, text, outs=outs, ins=ins, **kw)

    def byvar(self, name):
        for b in self.boxes:
            if b["box"].get("varname") == name:
                return b["box"]["id"]
        raise KeyError(name)

    def wire(self, a, ao, b, bi):
        self.lines.append({"patchline": {"source": [a, ao], "destination": [b, bi]}})

    def body(self, **extra):
        d = {"fileversion": 1, "appversion": {"major": 9, "minor": 0, "revision": 8, "architecture": "x64", "modernui": 1},
             "classnamespace": "box", "rect": [60.0, 80.0, 900.0, 640.0], "openinpresentation": 1,
             "bglocked": 0, "bgcolor": GROUND, "editing_bgcolor": GROUND, "boxes": self.boxes, "lines": self.lines}
        d.update(extra)
        return d

    def json(self, kind="iiii"):
        proj = {"version": 1, "creationdate": 0, "modificationdate": 0, "viewrect": [0.0, 0.0, 300.0, 500.0], "autoorganize": 1,
                "hideprojectwindow": 1, "showdependencies": 1, "autolocalize": 0, "contents": {"patchers": {}}, "layout": {},
                "searchpath": {}, "detailsvisible": 0, "amxdtype": struct.unpack(">I", kind.encode())[0], "readonly": 0,
                "devpathtype": 0, "devpath": ".", "sortmode": 0, "viewmode": 0, "includepackages": 0}
        return {"patcher": self.body(devicewidth=float(self.w), openrect=[0.0, 0.0, float(self.w), float(self.h)], project=proj,
                                     dependency_cache=[], parameters=dict(self.params, inherited_shortname=1), is_mpe=0, latency=0, minimum_live_version="",
                                     minimum_max_version="", platform_compatibility=0, autosave=0)}

    def editor_json(self):
        return self.body(rect=[100.0, 100.0, float(self.w), float(self.h)])


def label(p, text, x, y, w, color=TEXT2, size=T_LABEL, bold=False, h=16, just=0, present=True, **kw):
    """static / dynamic text. just: 0 left, 1 centre, 2 right. One font family, sizes from the type scale only. present=False: the comment exists (pmor_bank.js writes it) but is not drawn."""
    return p.add("comment", x, y, w, h, text, outs=0, ins=1, pres=[x, y, w, h] if present else None, textcolor=color, fontsize=size, fontname=FONT,
                 fontface=1 if bold else 0, textjustification=just, **kw)


def title(p, text, color, w=200):
    """the device / editor title: centred in the 32 px header strip (same place in all five devices)"""
    x = ((p.w - w) // 8) * 4                      # x and the width are multiples of 4 and the box is centred on the device
    return label(p, text, x, 4, p.w - 2 * x, color, T_TITLE, True, h=24, just=1, varname="title")


def divider(p, x, y, w, h):
    """1 px hairline between zones (a background panel in DIVIDER)"""
    return p.add("panel", x, y, w, h, outs=0, ins=1, pres=[x, y, w, h], bgfillcolor=fill(DIVIDER), border=0, rounded=0, background=1)


def menu(p, x, y, w, items, varname, h=24, color=TEXT1, well=GROUND, present=True, **kw):
    """plain umenu on the input-well surface (GROUND on a PANEL card, PANEL in the GROUND header); items [] = filled at runtime"""
    its = sum([[i, ","] for i in items], [])[:-1] if items else []
    return p.add("umenu", x, y, w, h, outs=3, ins=1, outlettype=["int", "", ""], items=its, pres=[x, y, w, h] if present else None, textcolor=color, bgfillcolor=fill(well),
                 fontsize=T_BODY, fontname=FONT, rounded=4, varname=varname, **kw)


def panels(p, inner=None, rr=8):
    """GROUND under everything (the header strip is just the ground), PANEL cards on top"""
    p.add("panel", 0, 0, p.w, p.h, outs=0, ins=1, pres=[0, 0, p.w, p.h], bgfillcolor=fill(GROUND), border=0, rounded=0, background=1)
    for r in (inner or []):
        p.add("panel", r[0], r[1], r[2], r[3], outs=0, ins=1, pres=list(r), bgfillcolor=fill(PANEL), border=0, rounded=rr, background=1)


BUTTON_KINDS = {"neutral": (RAISED, TEXT1, ACCENT, GROUND), "primary": (None, GROUND, TEXT1, GROUND), "danger": (DANGER, GROUND, WARN, GROUND)}


def button(p, text, x, y, w, h=24, kind="neutral", color=ACCENT, size=T_BODY, bold=False, texton=None, **kw):
    """action button. neutral = RAISED fill / TEXT1 text; primary = `color` fill (ACCENT, or the device's engine colour) / GROUND text; danger = DANGER fill.
    Pressed (on) state: neutral -> ACCENT, primary -> TEXT1, danger -> WARN."""
    off, toff, on, ton = BUTTON_KINDS[kind]
    off = color if off is None else off
    return p.add("textbutton", x, y, w, h, text, outs=3, ins=1, outlettype=["", "", "int"], pres=[x, y, w, h], texton=texton or text, textcolor=toff, textoncolor=ton,
                 bgcolor=off, bgoncolor=on, usebgoncolor=1, fontsize=size, fontname=FONT, fontface=1 if bold else 0, rounded=4, **kw)


def msgbox(p, text):
    return p.hid(text, cls="message", ins=2)


# ---- shared engine-room parts ------------------------------------------------------------------------------
def tx(p):
    """the ONE path from this scope's controls to [send pmor_tx]: pmor_guard.js (drops every `setparam` during the load grace period and any that equals what PMOR
    holds, see pmor_guard.js). It also hears PMOR's `param` stream (route param) to know what PMOR holds."""
    if p.tx is None:
        p.tx = p.hid("js pmor_guard.js", outs=2, outlettype=["", ""], varname="guard")
        snd = p.hid("send pmor_tx", outs=0)
        p.wire(p.tx, 0, snd, 0)
        rx = p.hid("receive pmor_rx"); rp = p.hid("route param", outs=2, outlettype=["", ""]); pp = p.hid("prepend param", ins=2)
        p.wire(rx, 0, rp, 0); p.wire(rp, 0, pp, 0); p.wire(pp, 0, p.tx, 0)
    return p.tx


def ui(p):
    """pmor_ui.js: updates controls from `receive pmor_rx` (silently) and turns synth dial moves into setparam."""
    if p.ui is None:
        rx = p.hid("receive pmor_rx")
        p.ui = p.hid("js pmor_ui.js" + (" gated" if p.gated else ""), outs=3, outlettype=["", "", ""], varname="ui")
        p.wire(rx, 0, p.ui, 0)
        p.wire(p.ui, 0, tx(p), 0)
        rs = p.hid("receive pmor_ui_status"); rst = p.hid("route st", outs=2, outlettype=["", ""]); p.wire(rs, 0, rst, 0); p.wire(rst, 0, p.ui, 0)      # schema stream + info (compat); see ST_NOTE
    return p.ui


# ST_NOTE: messages to [send pmor_ui_status] travel wrapped as `st <msg...>` and are unwrapped by [route st]. Sent bare, a `preset ...` message
# reached the editor's js with NO arguments in Live 12.4.7 (Max treats `preset` as its own preset-protocol message), so the editor's preset menu
# stayed empty while the core's directly wired browser got the full message.
def browser(p, engine=0, follow=""):
    if p.br is None:
        rs = p.hid("receive pmor_ui_status"); rst = p.hid("route st", outs=2, outlettype=["", ""]); p.wire(rs, 0, rst, 0); rs = rst   # see ST_NOTE
        p.br = p.hid("js pmor_browser.js %d%s%s" % (engine, " gated" if p.gated else "", (" " + follow) if follow else ""), outs=8, outlettype=[""] * 8, varname="br")
        p.wire(rs, 0, p.br, 0)
        p.wire(p.br, 1, tx(p), 0)
    return p.br


def ctl(p, pid, x, y, w=24, h=None, label_=None, lw=80, color=TEXT1, items=None, kind=None, ly=None):
    """schema-driven control named p<pid>; change -> prepend setparam <pid> -> send pmor_tx. Returns box id. The label (TEXT2, centred on the control) sits at `ly`."""
    e = E[pid]
    lo, hi, flags = e[2], e[3], e[5]
    ph = h or w
    if items is not None:
        c = menu(p, x, y, w, items, "p%d" % pid, h=ph)
    elif (flags & SOFT) and lo == 0 and hi == 1:
        c = p.add("toggle", x, y, w, ph, outs=1, ins=1, outlettype=["int"], pres=[x, y, w, ph], bgcolor=GROUND, checkedcolor=color, elementcolor=color, bordercolor=DIVIDER,
                  varname="p%d" % pid)
    elif kind == "flonum":
        c = p.add("flonum", x, y, w, ph, outs=2, ins=1, outlettype=["", "bang"], pres=[x, y, w, ph], bgcolor=GROUND, textcolor=TEXT1, bordercolor=DIVIDER, tricolor=TEXT2,
                  ignoreclick=1, fontsize=T_BODY, fontname=FONT, varname="p%d" % pid)
        if label_:
            label(p, label_, x + (w - lw) // 2, ly if ly is not None else y + ph, lw, TEXT2, just=1)
        return c
    else:
        a = scale_attrs(lo, hi, flags)
        c = p.add("dial", x, y, w, ph, outs=1, ins=1, outlettype=["float"], pres=[x, y, w, ph], needlecolor=TEXT1, outlinecolor=color, bgcolor=GROUND,
                  varname="p%d" % pid, **a)
    pp = p.hid("prepend setparam %d" % pid, ins=2)
    if items is not None and lo != 0:                 # a umenu emits its item INDEX; the schema value of item i is lo + i
        off = p.hid("+ %d" % int(lo), ins=2, outs=1, outlettype=["int"])
        p.wire(c, 0, off, 0); p.wire(off, 0, pp, 0)
    else:
        p.wire(c, 0, pp, 0)
    p.wire(pp, 0, tx(p), 0)
    if label_:
        label(p, label_, x + (w - lw) // 2, ly if ly is not None else y + ph, lw, TEXT2, just=1)
    return c


DIST_NAMES = ["TANH", "FOLD", "FUZZ", "WARM", "FL", "CRUSH"]      # firmware DIST_NAMES (pmor_core/fx.cpp)


def live_menu_attrs():
    """every colour a live.menu paints in Live (active state + inactive/zombie), so Live's own theme never shows through"""
    return dict(activebgcolor=GROUND, lcdbgcolor=GROUND, textcolor=TEXT1, lcdcolor=TEXT1, inactivelcdcolor=DISABLED, hltcolor=ACCENT, hlttextcolor=GROUND, tricolor=TEXT2,
                bordercolor=DIVIDER, focusbordercolor=ACCENT, fontsize=T_BODY, fontname=FONT)


def live_text_attrs(color):
    """live.text toggle: off = GROUND well with TEXT1, on = `color` fill with GROUND text; inactive (device off) = DISABLED text"""
    return dict(bgcolor=GROUND, bgoncolor=RAISED, activebgcolor=GROUND, activebgoncolor=color, textcolor=DISABLED, textoffcolor=DISABLED, activetextcolor=TEXT1,
                activetextoncolor=GROUND, bordercolor=DIVIDER, focusbordercolor=ACCENT, rounded=4, fontsize=T_LABEL, fontname=FONT)


def live_dial_attrs(color):
    """live.dial: value arc in the engine colour, face GROUND, needle TEXT1 (activedialcolor = value arc, activefgdialcolor = dial face, per the Max 9 refpage)"""
    return dict(dialcolor=DISABLED, activedialcolor=color, fgdialcolor=GROUND, activefgdialcolor=GROUND, needlecolor=TEXT1, activeneedlecolor=TEXT1, textcolor=TEXT1,
                bordercolor=DIVIDER, focusbordercolor=ACCENT, fontsize=T_LABEL, fontname=FONT)


def livectl(p, pid, x, y, w, h, color, label_=None, enum=None):
    """FX control as a REAL Live parameter (automatable; Live records PMOR's moves and plays automation back). Varname p<pid> like every schema control, so
    pmor_ui.js sets it SILENTLY (`set`) when PMOR reports a value (PMOR = source of truth, no echo), and a user / automation move -> setparam <pid> -> PMOR.
    Engage (ids 5120..5247) = live.text toggle, 0/1 (off/on); the distortion algo = live.menu (enum). Not stored in the Live set (initial 0, restored on load)."""
    e = E[pid]
    group = e[6]
    if enum is None:
        ln = "FX%d.%02d %s" % (int(group[2:]) + 1, (pid & 15) + 1, e[1])
        c = p.add("live.text", x, y, w, h, outs=2, ins=1, outlettype=["", ""], pres=[x, y, w, h], parameter_enable=1, mode=1, text=e[1], texton=e[1],
                  varname="p%d" % pid, saved_attribute_attributes={"valueof": {"parameter_longname": ln, "parameter_shortname": e[1], "parameter_type": 2, "parameter_mmax": 1.0,
                      "parameter_enum": ["off", "on"], "parameter_initial_enable": 1, "parameter_initial": [0.0], "parameter_speedlim": 0.0}}, **live_text_attrs(color))
    else:
        ln = "FX Distortion algo"
        c = p.add("live.menu", x, y, w, h, outs=3, ins=1, outlettype=["", "", "float"], pres=[x, y, w, h], parameter_enable=1, varname="p%d" % pid,
                  saved_attribute_attributes={"valueof": {"parameter_longname": ln, "parameter_shortname": "Dist algo", "parameter_type": 2, "parameter_mmax": float(len(enum) - 1),
                      "parameter_enum": enum, "parameter_initial_enable": 1, "parameter_initial": [0.0], "parameter_speedlim": 0.0}}, **live_menu_attrs())
    p.params[c] = [ln, ln.split(" ", 1)[1] if enum is None else "Dist algo", 0]
    pp = p.hid("prepend setparam %d" % pid, ins=2)
    p.wire(c, 0, pp, 0); p.wire(pp, 0, tx(p), 0)
    return c


# ---- bank controls: "engine -> sound -> params" (pmor_bank.js) --------------------------------------------------
PAGES = {}                        # editor scope -> {varname: page} (non-bank objects of tabbed pages; pmor_bank.js PAGEOF, render_devices.py)
BANK = {}                         # name -> [kind, k, page, dyn]  (every bank control of every device; written into pmor_bank.js)
TIME_K = {4, 5, 7, 10, 11, 13, 16, 17}      # synth k: attack decay release | filter attack decay release | glide in out  (seconds)


def hold(p, text, x, y, w, arm, go, dst):
    """press-and-hold button (danger kind): textbutton (momentary) -> [sel 1 0]: 1 (press) -> `arm`, 0 (release) -> `go`. The js side demands >= 2 s between the two, so a click does nothing."""
    b = button(p, text, x, y, w, 24, "danger", mode=0)
    sl = p.hid("sel 1 0", outs=3, outlettype=["bang", "bang", ""])
    ma, mg = msgbox(p, arm), msgbox(p, go)
    p.wire(b, 0, sl, 0); p.wire(sl, 0, ma, 0); p.wire(sl, 1, mg, 0); p.wire(ma, 0, dst, 0); p.wire(mg, 0, dst, 0)
    return b


def drum_group(pad):
    r, c = pad // 4, pad % 4
    if r < 2:
        return 3 if c < 2 else 2        # PERC / HAT
    return 0 if c < 2 else 1            # KICK / SNARE


def unit_of(e):
    """display unit of a schema entry: s Hz st dB % or ''"""
    pid, name, lo, hi, df, fl, grp = e
    dom, idx = pid >> 10, pid & 1023
    if dom == 1 and idx < 256:
        k = idx & 31
        if k in TIME_K or name == "IDXDECAY":
            return "s"
        if k == 8:
            return "Hz"
        if k == 15:
            return "st"
    if pid in (1281, 2308, 3328, 6146) or (dom == 2 and ((idx < 256 and (idx & 7) == 4) or 256 <= idx <= 259)):
        return "dB"
    if pid == 6151:
        return "ms"                                             # PD_GLOBAL CLKLEAD: follower clock lead
    if pid == 1282:
        return "Hz"                                             # SYN.LFO rate
    if dom == 2 and 320 <= idx < 384:
        return "x"                                              # DRUM.DEEP: multiplier on the authored recipe value
    if dom == 3 and idx >= 512 and (idx & 7) in (3, 4):
        return "ms"                                             # sampler ATTACK / RELEASE
    if fl & 4:
        return "%"
    return ""


def is_log(e):
    u = unit_of(e)
    return e[2] > 0 and ((u in ("s", "Hz")) or (e[5] & 128) or (e[1] == "RATIO" and e[3] / e[2] > 10))     # 128 = PF_LOG


def targets(kind, k):
    if kind == "id":
        return [k]
    if kind == "syn":
        return [1024 + a * 32 + k for a in range(8)]
    if kind == "pad":
        return [2048 + pd * 8 + k for pd in range(16)]
    if kind == "grp":
        return [2320 + g * 8 + k for g in range(4)]
    if kind == "slot":
        return [3072 + s * 8 + k for s in range(16)]
    if kind == "deep":                          # DRUM.DEEPg: 8 recipe knobs per drum group (ids 2368 + g*16 + k)
        return [2368 + g * 16 + k for g in range(4)]
    if kind == "slotx":                         # SMP.SLOTn extras: START END LOOP ATTACK RELEASE (ids 3584 + s*8 + k)
        return [3584 + s * 8 + k for s in range(16)]
    if kind.startswith("dsyn"):                 # drum synth param k of ENGINE n ("dsyn<n>", n = drum group): pad p -> domain 7 + p/4, index (p%4)*256 + k, only the pads of that engine
        return [i for i in (((7 + pd // 4) << 10) | ((pd % 4) * 256 + k) for pd in range(16) if drum_group(pd) == int(kind[4:])) if i in E]
    raise KeyError(kind)


def live_attrs(longname, short, lo, hi, e0, mode, exp=None):
    """saved_attribute_attributes for a live.dial / live.text / live.menu parameter. No parameter_initial: nothing is restored into PMOR (and pmor_guard.js drops load-time emits)."""
    v = {"parameter_longname": longname, "parameter_shortname": short, "parameter_mmin": float(lo), "parameter_mmax": float(hi), "parameter_initial_enable": 0,
         "parameter_speedlim": 0.0}
    return {"valueof": v}


def bank_ctl(p, name, kind, k, x, y, w, color, mode="d", page=-1, lbl=None, items=None, live=False, h=None, longname=None, lw=60, readout=True, ly=None, show_label=True, well=GROUND, show=True):
    """One bound control + label + readout. mode: d plain dial (normalized), r live.dial real units (fixed range), n live.dial normalized, t toggle (live.text if live),
    m menu (live.menu if live). Returns the control box id. The label (TEXT2) and the readout (TEXT1) are 16 px rows, centred on the control, `lw` wide; the label
    sits at `ly` (default: right under the control) so controls of different heights on one row share one baseline."""
    saved_page = p.page
    if page >= 0:
        p.page = page
    if getattr(p, "lane", False):
        name = "L" + name                       # a lane's perform view and its editor are different scopes with different pages: separate names
    ids = targets(kind, k)
    es = [E[i] for i in ids]
    dyn = len({e[1] for e in es}) > 1
    lo, hi, fl = es[0][2], es[0][3], es[0][5]
    if mode == "r":
        assert all((e[2], e[3], e[5]) == (lo, hi, fl) for e in es), "bank %s: range differs across targets" % name
    spec = [kind, k, page, 1 if dyn else 0]
    assert BANK.setdefault(name, spec) == spec, "bank name %s reused with another binding" % name
    h = h or w
    pr = [x, y, w, h] if show else None             # show=False: the control exists (Live parameter / bank target) but is not drawn in this view
    vn = {"d": "bk_", "r": "bkr_", "n": "bkn_", "t": "bkt_", "m": "bkm_"}[mode] + name
    text = lbl or es[0][1]
    unit = unit_of(es[0])
    longname = longname or text
    short = text[:15]
    exp = 3.0 if (is_log(es[0]) and mode == "r") else 1.0
    if mode == "d":
        a = scale_attrs(0.0, 1.0, 0)
        c = p.add("dial", x, y, w, h, outs=1, ins=1, outlettype=["float"], pres=pr, needlecolor=TEXT1, outlinecolor=color, bgcolor=GROUND, varname=vn, **a)
        out = "bc"
    elif mode in ("r", "n"):
        isint = bool(fl & SOFT) and mode == "r"
        rlo, rhi = (lo, hi) if mode == "r" else (0.0, 1.0)
        va = live_attrs(longname, short, rlo, rhi, None, mode)["valueof"]
        va.update({"parameter_type": 1 if isint else 0, "parameter_exponent": exp, "parameter_unitstyle": 9 if (mode == "r" and unit in ("s", "Hz", "st")) else (0 if isint else 1)})
        if mode == "r" and unit in ("s", "Hz", "st"):
            va["parameter_units"] = {"s": "%.3f s", "Hz": "%.0f Hz", "st": "%.0f st"}[unit]
        c = p.add("live.dial", x, y, w, h, outs=2, ins=1, outlettype=["", "float"], pres=pr, parameter_enable=1, showname=0, shownumber=0, varname=vn,
                  saved_attribute_attributes={"valueof": va}, **live_dial_attrs(color))
        p.params[c] = [longname, short, 0]
        out = "bc" if mode == "n" else "br"
    elif mode == "t":
        if live:
            va = live_attrs(longname, short, 0, 1, None, mode)["valueof"]
            va.update({"parameter_type": 2, "parameter_enum": ["off", "on"]})
            c = p.add("live.text", x, y, w, h, outs=2, ins=1, outlettype=["", ""], pres=pr, parameter_enable=1, mode=1, text=text, texton=text, varname=vn,
                      saved_attribute_attributes={"valueof": va}, **live_text_attrs(color))
            p.params[c] = [longname, short, 0]
        else:
            c = p.add("toggle", x, y, w, h, outs=1, ins=1, outlettype=["int"], pres=pr, bgcolor=GROUND, checkedcolor=color, elementcolor=color, bordercolor=DIVIDER,
                      varname=vn)
        out = "br"
    else:   # menu
        its = items
        if live:
            va = live_attrs(longname, short, 0, len(its) - 1, None, mode)["valueof"]
            va.update({"parameter_type": 2, "parameter_enum": list(its)})
            c = p.add("live.menu", x, y, w, h, outs=3, ins=1, outlettype=["", "", "float"], pres=pr, parameter_enable=1, varname=vn,
                      saved_attribute_attributes={"valueof": va}, **live_menu_attrs())
            p.params[c] = [longname, short, 0]
        else:
            c = menu(p, x, y, w, its, vn, h=h, well=well, present=show)
        out = "bm"
    pp = p.hid("prepend %s %s" % (out, name), ins=2)
    p.wire(c, 0, pp, 0); p.wire(pp, 0, bank(p), 0)
    lx = x + (w - lw) // 2
    ly = y + h if ly is None else ly
    if show_label:
        label(p, text, lx, ly, lw, TEXT2, T_LABEL, just=1, varname="bl_" + name, present=show)
    if readout:
        p.add("comment", lx, ly + 16, lw, 16, "-", outs=0, ins=1, pres=[lx, ly + 16, lw, 16] if show else None, textcolor=TEXT1, fontsize=T_LABEL, fontname=FONT, textjustification=1,
              varname="bv_" + name)
    p.page = saved_page
    return c


def bank(p):
    """pmor_bank.js for this patcher scope: PMOR values in (pmor_rx `param`), selection broadcasts both ways (pmor_ui_sel), schema stream (compat), setparam out -> guard."""
    if p.bk is None:
        rx = p.hid("receive pmor_rx")
        p.bk = p.hid("js pmor_bank.js" + (" gated" if p.gated else "") + ((" " + p.scope) if p.scope else ""), outs=3, outlettype=["", "", ""], varname="bank")
        p.wire(rx, 0, p.bk, 0)
        p.wire(p.bk, 0, tx(p), 0)
        rs = p.hid("receive pmor_ui_status"); rst = p.hid("route st", outs=2, outlettype=["", ""]); p.wire(rs, 0, rst, 0); p.wire(rst, 0, p.bk, 0)     # see ST_NOTE
        rsel = p.hid("receive pmor_ui_sel"); ssel = p.hid("send pmor_ui_sel", outs=0)
        p.wire(rsel, 0, p.bk, 0); p.wire(p.bk, 2, ssel, 0)
    return p.bk


def selector(p, var, kind, items, x, y, w, color=TEXT1, h=24, well=GROUND):
    """selection menu (algo / pad / slot): a plain umenu -> `pick <kind> <index>` -> pmor_bank.js. Host-side UI state only; never stored, never sent."""
    c = menu(p, x, y, w, items, var, h=h, color=color, well=well)
    pk = p.hid("prepend pick %s" % kind, ins=2)
    p.wire(c, 0, pk, 0); p.wire(pk, 0, bank(p), 0)
    return c


_HN = [0]


def hdr(e, text, page, x, y, w, color=TEXT1, just=0):
    """a section heading of an editor page (varname bh<page>_<n>, pmor_bank.js hides it with the page)"""
    _HN[0] += 1
    saved, e.page = e.page, page
    c = label(e, text, x, y, w, color, T_HEAD, True, varname="bh%d_%d" % (page, _HN[0]), just=just)
    e.page = saved
    return c


def pad_items():
    return ["pad %d  %s" % (pd, SCH["drum_groups"][drum_group(pd)]) for pd in range(16)]


# ---- component set (the only widgets the five devices are built from) -------------------------------------------
#   header strip (note left, title centred) | card | dial (bank_ctl) | selector menu | EXPAND column (strip) | tab bar + page card (window) | grid cell (cell) | section heading (hdr)
def tabbar(e, tabs):
    """the window's page selector: one `tab` object across the window, page n -> pmor_bank.js `page n` (shows the objects of that page, hides the rest)"""
    c = e.add("tab", 16, 36, e.w - 32, 28, outs=3, ins=1, outlettype=["int", "", ""], pres=[16, 36, e.w - 32, 28], tabs=list(tabs), bgcolor=GROUND, tabcolor=PANEL, htabcolor=ACCENT,
              textcolor=TEXT2, htextcolor=GROUND, rounded=4, fontsize=T_BODY, fontname=FONT, varname="tabs")
    pp = e.hid("prepend page", ins=2)
    e.wire(c, 0, pp, 0); e.wire(pp, 0, bank(e), 0)


def frame(e, name, color, tabs, scope, note=True):
    """editor window frame: 32 px header (selection note left, centred title), 32 px tab row, PANEL card y 72..H - 8. Page content is laid out in `card space`:
    e.dy = 40, so y 40 is the first content row (heading) and y 32 the card's top edge."""
    e.scope = scope
    panels(e, [(8, 72, e.w - 16, e.h - 80)], 8)
    title(e, name, color)
    if note:
        label(e, "", 16, 8, 200, TEXT2, varname="selinfo")
    tabbar(e, tabs)
    e.dy = 40


def cell(p, text, x, y, w, h, tint, name, msg, dst, live=True):
    """grid cell: a momentary textbutton in `tint`, selected state (set by pmor_bank.js) ACCENT with GROUND text; click -> [sel 1] -> `msg` -> dst. Used for pads, sounds, algorithms, slots."""
    b = p.add("textbutton", x, y, w, h, text, outs=3, ins=1, outlettype=["", "", "int"], pres=[x, y, w, h], texton=text, textcolor=TEXT1, textoncolor=GROUND, bgcolor=tint,
              bgoncolor=ACCENT, usebgoncolor=1, fontsize=T_BODY, fontname=FONT, rounded=4, mode=0, varname=name, ignoreclick=0 if live else 1)
    if live:
        sl = p.hid("sel 1", outs=2, outlettype=["bang", ""]); m = msgbox(p, msg)
        p.wire(b, 0, sl, 0); p.wire(sl, 0, m, 0); p.wire(m, 0, dst, 0)
    return b


def expand_button(p, color):
    """the primary action of every strip: a tall engine-colour column at the right edge (x W - 112, 96 x 112), EXPAND <-> MINIMIZE; returns the button.
    Press -> [sel 1 0] -> `show` / `hide` -> pmor_edit.js; pmor_edit.js sets the button back (silent `set`) when the window is closed by hand."""
    b = button(p, "EXPAND", p.w - 112, 40, 96, 112, "primary", color, size=T_TITLE, bold=True, mode=1, texton="MINIMIZE", varname="expand")
    return b


def edit_wiring(p, btn, full=False, lane_js=None, sess=None):
    """EXPAND / MINIMIZE -> pmor_edit.js; its window state back to the button; its window rectangle <-> a pattr saved in the Live set (UNVERIFIED in Live: without it the window
    just opens at the editor's default rect). Returns the pmor_edit.js box."""
    sl = p.hid("sel 1 0", outs=3, outlettype=["bang", "bang", ""])
    ms, mh = msgbox(p, "show"), msgbox(p, "hide")
    ej = p.hid("js pmor_edit.js" + (" full" if full else ""), outs=4, outlettype=["", "", "", ""], varname="edit")
    p.wire(btn, 0, sl, 0); p.wire(sl, 0, ms, 0); p.wire(sl, 1, mh, 0); p.wire(ms, 0, ej, 0); p.wire(mh, 0, ej, 0)
    p.wire(ej, 0, tx(p), 0); p.wire(ej, 2, btn, 0)
    pa = p.hid("pattr pmor_winrect @parameter_enable 1", outs=3, outlettype=["", "", ""], varname="winrect",
               saved_attribute_attributes={"valueof": {"parameter_longname": "PMOR editor window", "parameter_shortname": "PMOR win", "parameter_type": 3, "parameter_invisible": 1}})
    p.params[pa] = ["PMOR editor window", "PMOR win", 0]
    rl = p.hid("prepend rectload", ins=2)
    p.wire(ej, 3, pa, 0); p.wire(pa, 0, rl, 0); p.wire(rl, 0, ej, 0)
    if lane_js:
        p.wire(ej, 1, lane_js, 0)
    if sess:
        p.wire(ej, 1, sess, 0)
    return ej


def seq_page(e, base, n_ids, page, color, y0=72, per_row=12, pitch=96):
    """every SEQ param of one engine (REC CLEAR MODE PROB DENSITY [POLY OCTAVE OCTPROB ARP ARPRATE] BARS STEPS). Rows of `per_row` controls on a 96 px pitch, each row
    centred on the editor (x = 360 +- n*48); dials 40, toggles 24, menus 80; labels share one baseline per row (y0 + 40)."""
    rows = [list(range(base, base + n_ids))[i:i + per_row] for i in range(0, n_ids, per_row)]
    for r, ids in enumerate(rows):
        x0 = 360 - len(ids) * pitch // 2
        yy = y0 + r * 80
        for i, pid in enumerate(ids):
            nm = E[pid][1]
            cx = x0 + i * pitch
            if nm == "MODE":
                bank_ctl(e, "q%d" % pid, "id", pid, cx + 8, yy + 8, 80, color, "m", page, items=["MANUAL", "PLAY", "GEN"], h=24, lbl="MODE", lw=pitch, ly=yy + 40)
            elif E[pid][3] == 1 and E[pid][2] == 0 and (E[pid][5] & SOFT):
                bank_ctl(e, "q%d" % pid, "id", pid, cx + 36, yy + 8, 24, color, "t", page, h=24, lbl=nm, lw=pitch, ly=yy + 40)
            elif pid == 4137:      # ARPRATE 1..4 (enum, lo 1)
                bank_ctl(e, "q%d" % pid, "id", pid, cx + 8, yy + 8, 80, color, "m", page, items=["1/4", "1/8", "1/16", "1/32"], h=24, lbl=nm, lw=pitch, ly=yy + 40)
            else:
                bank_ctl(e, "q%d" % pid, "id", pid, cx + 28, yy, 40, color, "d", page, lbl=nm, lw=pitch, ly=yy + 40)


def save_row(e, y, color, follow, engine):
    """SAVE AS USER PRESET strip (drums + synth editors), one 60 px card at `y`: heading, name field (pmor_browser.js sanitises to A-Z 0-9 space - , <= 9), slot picker (user slots
    only, the browser fills it from the PULL_FILE R lists; an existing user slot needs a second click on `save`), save button, status; the note under the row. The browser follows the
    pad (drums) / algo (synth) selection through pmor_ui_sel and, for synth, PMOR's own algo value through pmor_rx. All outbound traffic is the browser's outlet 1 -> pmor_guard.js (`savepreset`)."""
    br = browser(e, engine, follow)
    r = y + 8
    label(e, "SAVE USER PRESET", 16, r + 4, 136, TEXT1, T_HEAD, True)
    ne = e.add("textedit", 152, r, 112, 24, outs=4, ins=1, outlettype=["", "int", "", ""], pres=[152, r, 112, 24], bgcolor=GROUND, textcolor=TEXT1, bordercolor=DIVIDER,
               fontsize=T_BODY, fontname=FONT, border=1, rounded=4, varname="savename")
    pn = e.hid("prepend savename", ins=2); e.wire(ne, 0, pn, 0); e.wire(pn, 0, br, 0)
    sm = menu(e, 272, r, 152, ["(connect)"], "saveslot")
    pk = e.hid("prepend saveslotpick", ins=2); e.wire(sm, 0, pk, 0); e.wire(pk, 0, br, 0); e.wire(br, 7, sm, 0)
    sb = button(e, "save", 432, r, 64, 24, "primary", color)
    tb = e.hid("t b b", outs=2, outlettype=["bang", "bang"]); bg = msgbox(e, "bang"); ds = msgbox(e, "dosave")
    e.wire(sb, 0, tb, 0); e.wire(tb, 1, bg, 0); e.wire(bg, 0, ne, 0); e.wire(tb, 0, ds, 0); e.wire(ds, 0, br, 0)        # name first (textedit `bang` re-sends its text), then dosave
    si = label(e, "", 504, r + 4, 200, TEXT1, T_LABEL, varname="saveinfo")
    rr = e.hid("route saveinfo", outs=2, outlettype=["", ""]); e.wire(br, 3, rr, 0); e.wire(rr, 0, si, 0)
    label(e, ("saves the CURRENT pad's sound as a user preset of its group (EDIT mode for the save)" if follow == "drums" else "saves the current algo's sound as a user preset (slots 5..31; 0..4 are factory)")
          + "; an existing slot needs a second click.", 16, r + 28, 688, TEXT2)
    rs = e.hid("receive pmor_ui_sel"); ps = e.hid("prepend selui", ins=2); e.wire(rs, 0, ps, 0); e.wire(ps, 0, br, 0)
    if follow == "synth":
        rx = e.hid("receive pmor_rx"); rp = e.hid("route param", outs=2, outlettype=["", ""]); pp = e.hid("prepend saveparam", ins=2)
        e.wire(rx, 0, rp, 0); e.wire(rp, 0, pp, 0); e.wire(pp, 0, br, 0)
    return br



def _build_ds():
    """the drum synth as the schema gives it, per ENGINE (= drum group KICK SNARE HAT PERC): [(layer, [(k, name)])] in schema order. Pad p's params: domain 7 + p/4, index (p%4)*256 + k, group
    '<ENGINE>.P<pad>.<LAYER>'. Every pad of an engine has the same fields; names / layers / ranges are asserted identical across the engine's pads (nothing is hardcoded about counts)."""
    out, ranges = [], {}
    for g in range(4):
        seen, layers = None, {}
        for pd in [q for q in range(16) if drum_group(q) == g]:
            base = ((7 + pd // 4) << 10) | ((pd % 4) * 256)
            cur = {}
            for e in sorted((x for x in SCH["schema"] if base <= x[0] < base + 256), key=lambda x: x[0]):
                en, pp, ly = e[6].split(".")
                assert en == SCH["drum_groups"][g] and pp == "P%d" % pd, "drum synth group %s of id %d is not <ENGINE>.P<pad>.<LAYER>" % (e[6], e[0])
                cur.setdefault(ly, []).append((e[0] - base, e[1]))
                assert ranges.setdefault(e[0] - base, tuple(e[2:4]) + (e[5],)) == tuple(e[2:4]) + (e[5],), "drum synth field %d range differs between pads" % (e[0] - base)
            assert seen is None or cur == seen, "engine %d: pads differ in their synth fields" % g
            seen = cur
        out.append(list(seen.items()))
    return out


DS = _build_ds()
DS_ROWS = max((len(ks) + 12) // 13 for lays in DS for _, ks in lays)
DS_NCHIP = max(len(lays) for lays in DS)


def drums_editor():
    """drums window: PAD (4x4 grid mirroring PMOR's pads + the selected pad's group sounds + its params) / GROUP / MIX / SEQ / DEEP / SAVE"""
    e = P(720, 568 + DS_ROWS * 68, gated=True)
    frame(e, "PMOR drums", C_DRUMS, ["PAD", "GROUP", "MIX", "SEQ", "DEEP", "SAVE"], "drums")
    br = bank(e)
    label(e, "values come from PMOR; every edit is sent to PMOR, which keeps it", 16, 500 + DS_ROWS * 68, 688, TEXT2, just=1)
    # page 0: pads. TL PERC, TR HAT, BL KICK, BR SNARE (drum_group); a cell shows the pad's sound name in its group tint; the sound chips on the right belong to the selected pad's group
    hdr(e, "PADS  (click to select)", 0, 16, 40, 312, C_DRUMS)
    e.page = 0
    label(e, "", 344, 40, 360, C_DRUMS, T_HEAD, True, varname="sndhead")
    for pd in range(16):
        g = drum_group(pd)
        cell(e, SCH["drum_sounds"][g][0], 16 + (pd % 4) * 80, 64 + (pd // 4) * 56, 72, 48, GRP_TINT[g], "pc_%d" % pd, "pick pad %d" % pd, br)
    for i in range(24):
        cell(e, SCH["drum_sounds"][0][i], 344 + (i % 4) * 92, 64 + (i // 4) * 36, 84, 28, GRP_TINT[0], "sc_%d" % i, "bm sound %d" % i, br)
    for i, g in enumerate([0, 1, 2, 3]):                                    # legend: the four group tints
        cell(e, SCH["drum_groups"][g], 16 + i * 80, 288, 72, 24, GRP_TINT[g], "lg_%d" % g, "", br, live=False)
    bank_ctl(e, "sound", "pad", 5, 0, 0, 96, C_DRUMS, "m", 0, h=24, items=SCH["drum_sounds"][0], lbl="SOUND", show=False)
    for i, k in enumerate([0, 1, 2, 4]):
        bank_ctl(e, "pad%d" % k, "pad", k, 136 + i * 112 - 20, 328, 40, C_DRUMS, "d", 0, lw=104, ly=368)
    bank_ctl(e, "pad3", "pad", 3, 584 - 16, 336, 32, C_DRUMS, "t", 0, h=32, lbl="REV", lw=104, ly=368)
    # the selected pad's full synth, FROM THE SCHEMA (domains 7..10, groups <ENGINE>.P<pad>.<LAYER>): the engine of the pad (KICK SNARE HAT PERC) names the section, its layers are the chips, one layer shows at a time;
    # bank pages 10 + engine*16 + layer: pmor_bank.js shows the controls of the selected pad's engine and chosen layer on tab 0
    e.page = 0
    label(e, "", 16, 412, 688, C_DRUMS, T_HEAD, True, varname="dshead")                         # "<ENGINE> synth of pad n  (pick a layer)", set by pmor_bank.js
    for i in range(DS_NCHIP):
        cell(e, "-", 16 + (i % 7) * 96, 436 + (i // 7) * 28, 88, 24, RAISED, "ly_%d" % i, "pick layer %d" % i, br)
    e.page = None
    for g in range(4):
        for li, (ly, ks) in enumerate(DS[g]):
            for j, (k, nm) in enumerate(ks):
                bank_ctl(e, "ds%s_%d" % (SCH["drum_groups"][g], k), "dsyn%d" % g, k, 16 + (j % 13) * 52 + 8, 500 + (j // 13) * 68, 32, C_DRUMS, "d", 10 + g * 16 + li, lw=48)
    hdr(e, "engine depth of the selected pad's group (KICK / SNARE / HAT / PERC)", 1, 16, 40, 688, C_DRUMS)
    for k in range(4):
        bank_ctl(e, "grp%d" % k, "grp", k, 160 + k * 112, 72, 64, C_DRUMS, "d", 1, lw=104)
    hdr(e, "group volumes and master", 2, 16, 40, 688, C_DRUMS)
    for i, (pid, nm) in enumerate([(2304, "KICK"), (2305, "SNARE"), (2306, "HAT"), (2307, "PERC"), (2308, "MASTER")]):
        bank_ctl(e, "mix%d" % pid, "id", pid, 104 + i * 112, 72, 64, C_DRUMS, "d", 2, lbl=nm + " VOL", lw=104)
    hdr(e, "drums sequencer / generative", 3, 16, 40, 688, C_DRUMS)
    seq_page(e, 4096, 7, 3, C_DRUMS, y0=80, per_row=7)
    hdr(e, "deep recipe of the selected pad's group (1.00x = as authored)", 4, 16, 40, 688, C_DRUMS)
    for k in range(8):
        bank_ctl(e, "deep%d" % k, "deep", k, 36 + k * 84, 72, 56, C_DRUMS, "d", 4, lw=72, ly=128)
    e.page = 5
    save_row(e, 40, C_DRUMS, "drums", 0)
    e.page = None
    PAGES[e.scope] = e.pages
    return e


def synth_editor():
    """synth window: SOUND (algorithms as grouped cards: header = algorithm, line = its four knobs) / OSC / AMP / FILTER / GEN / LFO / SAVE"""
    e = P(720, 324, gated=True)
    frame(e, "PMOR synth", C_SYNTH, ["SOUND", "OSC", "AMP", "FILTER", "GEN", "LFO", "SAVE"], "synth")
    br = bank(e)
    label(e, "values come from PMOR; every edit is sent to PMOR, which keeps it", 16, 252, 688, TEXT2, just=1)
    hdr(e, "ALGORITHMS  (click to select; each lists the four knobs K1..K4 it gives)", 0, 16, 40, 688, C_SYNTH)
    bank_ctl(e, "algo", "id", 1280, 0, 0, 96, C_SYNTH, "m", 0, h=24, items=SCH["synth_algos"], lbl="ALGO", readout=False, show_label=False, show=False)
    e.page = 0
    for a in range(8):
        x, y = 16 + (a % 2) * 348, 64 + (a // 2) * 44
        cell(e, SCH["synth_algos"][a], x, y, 340, 24, RAISED, "ag_%d" % a, "bm algo %d" % a, br)
        label(e, "   ".join(E[1024 + a * 32 + k][1] for k in range(4)), x, y + 24, 340, TEXT2, just=1)
    hdr(e, "oscillator: algorithm knobs + volume, then pitch / glide / width", 1, 16, 40, 688, C_SYNTH)
    for i in range(4):
        bank_ctl(e, "k%d" % i, "syn", i, 104 + i * 112, 72, 64, C_SYNTH, "d", 1, lw=104)
    bank_ctl(e, "vol", "id", 1281, 552, 72, 64, C_SYNTH, "d", 1, lw=104, lbl="VOLUME")
    for i, k in enumerate([15, 16, 17, 18, 19]):
        bank_ctl(e, "o%d" % k, "syn", k, 116 + i * 112, 176, 40, C_SYNTH, "d", 1, lw=104)
    hdr(e, "amp envelope", 2, 16, 40, 688, C_SYNTH)
    for i, k in enumerate([4, 5, 6, 7]):
        bank_ctl(e, "a%d" % k, "syn", k, 160 + i * 112, 72, 64, C_SYNTH, "d", 2, lw=104)
    hdr(e, "filter: cutoff / resonance, filter envelope", 3, 16, 40, 688, C_SYNTH)
    for i, k in enumerate([8, 9, 14]):
        bank_ctl(e, "f%d" % k, "syn", k, 216 + i * 112, 72, 64, C_SYNTH, "d", 3, lw=104)
    for i, k in enumerate([10, 11, 12, 13]):
        bank_ctl(e, "f%d" % k, "syn", k, 172 + i * 112, 176, 40, C_SYNTH, "d", 3, lw=104)
    hdr(e, "synth sequencer / generative", 4, 16, 40, 688, C_SYNTH)
    seq_page(e, 4128, 12, 4, C_SYNTH, y0=72, per_row=6)
    hdr(e, "LFO (engine-wide, depth 0 = off)", 5, 16, 40, 688, C_SYNTH)
    bank_ctl(e, "lforate", "id", 1282, 160, 72, 64, C_SYNTH, "d", 5, lbl="RATE", lw=104, ly=136)
    bank_ctl(e, "lfodepth", "id", 1283, 272, 72, 64, C_SYNTH, "d", 5, lbl="DEPTH", lw=104, ly=136)
    bank_ctl(e, "lfoshape", "id", 1284, 376, 92, 96, C_SYNTH, "m", 5, items=["SINE", "TRI", "SQUARE", "S&H"], h=24, lbl="SHAPE", lw=104, ly=136)
    bank_ctl(e, "lfodest", "id", 1285, 488, 92, 96, C_SYNTH, "m", 5, items=["CUTOFF", "PITCH", "AMP"], h=24, lbl="DEST", lw=104, ly=136)
    e.page = 6
    save_row(e, 40, C_SYNTH, "synth", 4)
    e.page = None
    PAGES[e.scope] = e.pages
    return e


def sampler_editor():
    """sampler window: SLOT (4x4 slot grid + the selected slot's params) / MIX / SEQ / ENV / POOL (sample pool -> selected slot)"""
    e = P(720, 364, gated=True)
    frame(e, "PMOR sampler", C_SAMPLER, ["SLOT", "MIX", "SEQ", "ENV", "POOL"], "sampler")
    br = bank(e)
    label(e, "values come from PMOR; every edit is sent to PMOR, which keeps it", 16, 300, 688, TEXT2, just=1)
    hdr(e, "SLOTS  (click to select)", 0, 16, 40, 312, C_SAMPLER)
    hdr(e, "sound of the selected slot", 0, 344, 40, 360, C_SAMPLER)
    e.page = 0
    for i in range(16):
        cell(e, str(i), 16 + (i % 4) * 80, 64 + (i // 4) * 56, 72, 48, RAISED, "sl_%d" % i, "pick slot %d" % i, br)
    for i, k in enumerate([0, 1, 2]):
        bank_ctl(e, "s%d" % k, "slot", k, 392 + i * 88 - 20, 136, 40, C_SAMPLER, "d", 0, lw=88, ly=176)
    bank_ctl(e, "s3", "slot", 3, 656 - 16, 140, 32, C_SAMPLER, "t", 0, h=32, lbl="REVERSE", lw=88, ly=176)
    hdr(e, "sampler mix", 1, 16, 40, 688, C_SAMPLER)
    for i, pid in enumerate([3328, 3329, 3330, 3331]):
        bank_ctl(e, "smix%d" % pid, "id", pid, 160 + i * 112, 72, 64, C_SAMPLER, "d", 1, lw=104)
    hdr(e, "sampler sequencer / generative", 2, 16, 40, 688, C_SAMPLER)
    seq_page(e, 4160, 7, 2, C_SAMPLER, y0=80, per_row=7)
    hdr(e, "playback window and envelope of the selected slot", 3, 16, 40, 688, C_SAMPLER)
    bank_ctl(e, "x0", "slotx", 0, 104, 72, 64, C_SAMPLER, "d", 3, lw=104, ly=136)
    bank_ctl(e, "x1", "slotx", 1, 216, 72, 64, C_SAMPLER, "d", 3, lw=104, ly=136)
    bank_ctl(e, "x2", "slotx", 2, 344, 88, 32, C_SAMPLER, "t", 3, h=32, lw=104, ly=136)
    bank_ctl(e, "x3", "slotx", 3, 440, 72, 64, C_SAMPLER, "d", 3, lw=104, ly=136)
    bank_ctl(e, "x4", "slotx", 4, 552, 72, 64, C_SAMPLER, "d", 3, lw=104, ly=136)
    e.page = 4
    bj = browser(e, 0)
    hdr(e, "SAMPLE POOL -> selected slot", 4, 16, 48, 240, C_SAMPLER)
    pm = menu(e, 264, 44, 320, ["(empty pool)"], "pool")
    pp = e.hid("prepend poolpick", ins=2)
    e.wire(pm, 0, pp, 0); e.wire(pp, 0, bj, 0); e.wire(bj, 2, pm, 0)
    ab = button(e, "assign to slot", 592, 44, 112, 24, "primary", C_SAMPLER)
    ma = msgbox(e, "assign"); e.wire(ab, 0, ma, 0); e.wire(ma, 0, bj, 0)
    label(e, "pool: idle", 16, 80, 688, TEXT1, T_BODY, varname="poolinfo")
    r = e.hid("route poolinfo store trans", outs=4, outlettype=["", "", "", ""])
    e.wire(bj, 3, r, 0); e.wire(r, 0, e.byvar("poolinfo"), 0)
    e.wire(br, 1, bj, 0)                      # selecting a slot tells the browser which slot an assign targets (slotpick)
    label(e, "assign needs EDIT mode (opening this window switches the session to EDIT)", 16, 104, 688, TEXT2)
    e.page = None
    PAGES[e.scope] = e.pages
    return e


METERS = [("FSR", 1), ("LIDAR", 2), ("ACCEL X", 3), ("ACCEL Y", 4), ("JOY X", 5), ("JOY Y", 6), ("FADER", 7)]


def control_editor():
    """control window: METERS (the live control CCs of ch4 + the random-FX seed) / FX (where the FX controls are)"""
    e = P(720, 316, gated=True)
    frame(e, "PMOR control", C_CONTROL, ["METERS", "FX"], "control", note=False)
    bank(e)
    e.page = 0
    hdr(e, "control CCs (ch4)", 0, 16, 40, 400, C_CONTROL)
    for i, (nm, cc) in enumerate(METERS):
        y = 64 + i * 28
        label(e, nm, 16, y + 4, 80, TEXT2)
        e.add("slider", 104, y + 8, 320, 8, outs=1, ins=1, outlettype=["float"], pres=[104, y + 8, 320, 8], size=128.0, min=0.0,
              bgcolor=GROUND, knobcolor=C_CONTROL, elementcolor=DIVIDER, bordercolor=GROUND, varname="m%d" % cc)
    hdr(e, "random-FX seed", 0, 464, 40, 240, C_CONTROL)
    label(e, "seed -", 464, 68, 240, TEXT1, T_BODY, varname="seed")
    label(e, "live CC stream (ch4 / ch5 recorded as MIDI)", 464, 92, 240, TEXT2)
    e.page = 1
    hdr(e, "FX controls", 1, 16, 40, 688, C_CONTROL)
    label(e, "FX controls are in the lane: bank / effect browser, Live-automatable.", 16, 72, 688, TEXT1, T_BODY)
    label(e, "Record them in a clip (automation lane) and edit there; PMOR stays the source of truth.", 16, 96, 688, TEXT2)
    e.page = None
    PAGES[e.scope] = e.pages
    return e


# ---- strips (the device views, 169 px high): header (routing note left, centred title) + PANEL card holding the essentials + the EXPAND column ----------------
def lane_common(name, chans, color, editor, w):
    p = P(w, 169)
    p.lane = True
    panels(p, [(8, 32, w - 16, 128)], 8)
    title(p, name, color)
    label(p, ("any ch -> ch %s" if ("meters" not in chans and " " not in chans) else "ch %s") % chans.replace(" meters", "") + " -> PMOR core", 16, 8, 200, TEXT2)
    eb = expand_button(p, color)
    ed = p.hid("p editor", outs=0, ins=0, varname="editor")           # no inlet: it is opened by pmor_edit.js (subpatcher().front())
    for b in p.boxes:
        if b["box"]["id"] == ed:
            b["box"]["patcher"] = editor.editor_json()
    mi = p.hid("midiin", outlettype=["int"])
    js = p.hid("js pmor_lane.js " + chans + ("" if "meters" in chans or " " in chans else " remap"), varname="lanejs")   # single-channel lanes re-channel any input to their PMOR channel
    sn = p.hid("send pmor_midi", outs=0)
    p.wire(mi, 0, js, 0); p.wire(js, 0, sn, 0)
    edit_wiring(p, eb, lane_js=js if "meters" in chans else None)
    return p


COL = 72                                  # strip dial column: dial 40 + label 16 + readout 16 under a 24 px selector row; EXPAND column x W - 112


def strip_dials(p, specs, color, left=16, right=None):
    """a centred row of `specs` = [(name, kind, k, longname, lbl)] live.dials of 40 px on 72 px columns, y 72 (label 112, readout 128); the region is x left..right"""
    right = right if right is not None else p.w - 124
    x0 = ((left + right - len(specs) * COL) // 2) // 4 * 4
    for i, (nm, kind, k, ln, lb) in enumerate(specs):
        bank_ctl(p, nm, kind, k, x0 + i * COL + 16, 72, 40, color, "r", lbl=lb, longname=ln, lw=COL)


def drums_lane():
    p = lane_common("PMOR drums", "1", C_DRUMS, drums_editor(), 500)
    selector(p, "selpad", "pad", pad_items(), 16, 40, 364)
    strip_dials(p, [("mix%d" % pid, "id", pid, "Drums %s Volume" % nm.title(), nm) for pid, nm in [(2304, "KICK"), (2305, "SNARE"), (2306, "HAT"), (2307, "PERC"), (2308, "MASTER")]], C_DRUMS)
    # the rest of the perform controls stay Live parameters (automation lists) but are only in the window: selected pad sound / length / pitch / tone / volume / reverb, group depths
    for nm, k, ln in [("sound", 5, "Sound"), ("pad0", 0, "Length"), ("pad1", 1, "Pitch"), ("pad2", 2, "Tone"), ("pad4", 4, "Volume")]:
        bank_ctl(p, nm, "pad", k, 16, 72, 40, C_DRUMS, "r", longname="Drums Sel " + ln, lw=COL, show=False)
    bank_ctl(p, "pad3", "pad", 3, 16, 72, 40, C_DRUMS, "t", live=True, h=24, longname="Drums Sel Reverb", lbl="REV", lw=COL, show=False)
    for k in range(4):
        bank_ctl(p, "grp%d" % k, "grp", k, 16, 72, 40, C_DRUMS, "r", longname="Drums Group Depth %d" % (k + 1), lw=COL, show=False)
    label(p, "", 16, 8, 8, TEXT2, varname="selinfo", present=False)
    return p


def synth_lane():
    p = lane_common("PMOR synth", "2", C_SYNTH, synth_editor(), 500)
    bank_ctl(p, "algo", "id", 1280, 16, 40, 364, C_SYNTH, "m", live=True, h=24, items=SCH["synth_algos"], longname="Synth Algorithm", lbl="ALGO", readout=False, show_label=False)
    strip_dials(p, [("vol", "id", 1281, "Synth Volume", "VOLUME"), ("p8", "syn", 8, "Synth Cutoff", None), ("p9", "syn", 9, "Synth Resonance", None), ("p7", "syn", 7, "Synth Release", None)], C_SYNTH)
    for i in range(4):
        bank_ctl(p, "k%d" % i, "syn", i, 16, 72, 40, C_SYNTH, "n", longname="Synth Algo Knob %d (0-1)" % (i + 1), lw=COL, show=False)
    for k, ln in [(4, "Attack"), (5, "Decay"), (6, "Sustain"), (14, "Filter Env Amount")]:
        bank_ctl(p, "p%d" % k, "syn", k, 16, 72, 40, C_SYNTH, "r", longname="Synth " + ln, lw=COL, show=False)
    label(p, "", 16, 8, 8, TEXT2, varname="selinfo", present=False)
    return p


def sampler_lane():
    p = lane_common("PMOR sampler", "3", C_SAMPLER, sampler_editor(), 500)
    selector(p, "selslot", "slot", ["slot %d" % i for i in range(16)], 16, 40, 364)
    bank_ctl_specs = [("smix3328", "id", 3328, "Sampler Volume", "VOLUME"), ("s0", "slot", 0, "Sampler Sel Pitch", None), ("s1", "slot", 1, "Sampler Sel Formant", None),
                      ("s2", "slot", 2, "Sampler Sel Cutoff", None)]
    strip_dials(p, bank_ctl_specs, C_SAMPLER)
    bank_ctl(p, "s3", "slot", 3, 16, 72, 40, C_SAMPLER, "t", live=True, h=24, longname="Sampler Sel Reverse", lbl="REVERSE", lw=COL, show=False)
    for pid, ln in [(3329, "Width"), (3330, "W Prob"), (3331, "Poly")]:
        bank_ctl(p, "smix%d" % pid, "id", pid, 16, 72, 40, C_SAMPLER, "r", lbl=ln.upper(), longname="Sampler " + ln, lw=COL, show=False)
    label(p, "", 16, 8, 8, TEXT2, varname="selinfo", present=False)
    return p


def control_lane():
    """ch4 control CCs + seed CC110-113, ch5 FX engage CCs: forwarded to the core. Strip: FX browser (bank / effect menus, the bank's 16 engage cells and the distortion algo, all Live
    parameters, see livectl) + the per-bank FX params AMOUNT / LIDAR / MIX; the meters and the seed are in the window."""
    p = lane_common("PMOR control", "4 5 6 meters", C_CONTROL, control_editor(), 748)
    divider(p, 404, 40, 1, 112)
    X = 16
    label(p, "FX", X, 44, 28, C_CONTROL, T_HEAD, True)
    bk = menu(p, X + 32, 40, 72, ["bank %d" % (b + 1) for b in range(8)], "fxbank")
    ef = menu(p, X + 112, 40, 80, SCH["fx_banks"][0], "fxeff")
    label(p, "dist", X + 200, 44, 36, TEXT2, just=2)
    livectl(p, 5248, X + 240, 40, 140, 24, C_CONTROL, enum=DIST_NAMES)
    label(p, "FX1.01 ...", X, 64, 380, TEXT1, just=1, varname="fxsel")
    for pd in range(16):
        for b in range(8):
            livectl(p, 5120 + b * 16 + pd, X + (pd % 8) * 48, 84 + (pd // 8) * 28, 44, 24, C_CONTROL)
    label(p, "cells = engage on / off; each is a Live parameter: automate it", X, 136, 380, TEXT2, just=1)
    X2 = 412
    label(p, "bank FX params", X2, 40, 216, C_CONTROL, T_HEAD, True, just=1)
    for b in range(8):
        for i, (nm, pid0, lo_b) in enumerate([("AMOUNT", 5250, 0), ("LIDAR", 5260, 0), ("MIX", 5270, 1)]):
            if b < lo_b:
                continue
            bank_ctl(p, "fxb%d%s" % (b, nm), "id", pid0 + b, X2 + i * COL + 16, 64, 40, C_CONTROL, "r", page=b, lbl=nm, longname="FX Bank %d %s" % (b + 1, nm.title()), lw=COL)
    pgb = p.hid("prepend page", ins=2)
    p.wire(bk, 0, pgb, 0); p.wire(pgb, 0, bank(p), 0)
    ui(p)
    fx = p.hid("js pmor_fx.js", outs=0)
    rx = p.hid("receive pmor_rx"); p.wire(rx, 0, fx, 0)
    pb, pe = p.hid("prepend bank", ins=2), p.hid("prepend eff", ins=2)
    p.wire(bk, 0, pb, 0); p.wire(pb, 0, fx, 0); p.wire(ef, 0, pe, 0); p.wire(pe, 0, fx, 0)
    return p




def core_editor():
    """core window: LINK (connection mirror) / SAMPLES (pool) / PRESETS / GLOBAL / BIN. Each tab is laid out in card space (y 40 = first row); the old two-card layout is re-centred per tab."""
    e = P(776, 288, gated=True)
    frame(e, "PMOR core", ACCENT, ["LINK", "SAMPLES", "PRESETS", "GLOBAL", "BIN"], "core", note=False)
    br = browser(e, 0)
    bank(e)
    # ---- LINK: mode / port (they drive the session logic of the device strip through [send pmor_lnk_in]), pull progress, transport, join. The strip's streams are mirrored here.
    e.page = 0
    hdr(e, "LINK: connection, pull progress, transport", 0, 16, 40, 744, ACCENT)
    label(e, "mode", 16, 76, 40, TEXT2, just=2)
    mm = menu(e, 64, 72, 96, ["STUDIO", "EDIT"], "mode")
    label(e, "port", 176, 76, 40, TEXT2, just=2)
    pm_ = menu(e, 224, 72, 96, ["auto", "a", "b", "c", "d", "e", "f"], "port")
    lin = e.hid("send pmor_lnk_in", outs=0)
    for mnu, word in ((mm, "pmode"), (pm_, "port")):
        pp_ = e.hid("prepend " + word, ins=2); e.wire(mnu, 1, pp_, 0); e.wire(pp_, 0, lin, 0)
    bar = e.add("slider", 16, 112, 744, 8, outs=1, ins=1, outlettype=["float"], pres=[16, 112, 744, 8], size=101.0, min=0.0, bgcolor=GROUND, knobcolor=ACCENT,
                elementcolor=DIVIDER, bordercolor=GROUND, varname="progress")
    smp = label(e, "samples: idle", 16, 124, 744, TEXT2, varname="samples")
    trn = label(e, "stop bpm - clk - cpu - ovr - drop -", 16, 144, 744, TEXT2, varname="transport")
    jnl = label(e, "join -", 16, 164, 744, TEXT2, varname="joinline")
    for nm, dst in (("mode", mm), ("progress", bar), ("samples", smp), ("transport", trn), ("joinline", jnl)):
        rcv = e.hid("receive pmor_lnk_" + nm); e.wire(rcv, 0, dst, 0)
    # ---- SAMPLES (content centred: x + 192)
    e.page, e.dx, e.dy = 1, 192, 40
    label(e, "SAMPLE POOL", 16, 40, 360, TEXT1, T_HEAD, True)
    pm = menu(e, 16, 64, 288, ["(empty pool)"], "pool")
    pp = e.hid("prepend poolpick", ins=2)
    e.wire(pm, 0, pp, 0); e.wire(pp, 0, br, 0); e.wire(br, 2, pm, 0)
    label(e, "[x] marked  * cached  - not  ! bad  xN = same audio", 16, 100, 288, TEXT2)
    btn = [("mark", "selcur", 312, 64, 64, "neutral", br), ("copies", "expandcur", 312, 96, 64, "neutral", br),
           ("pull", "pull", 16, 128, 84, "primary", br), ("pull patch", "pull current", 108, 128, 84, "primary", tx(e)), ("pull all", "pull all", 200, 128, 84, "primary", tx(e)),
           ("pull off", "pull off", 292, 128, 84, "neutral", tx(e)), ("refresh pool", "pool", 16, 160, 84, "neutral", tx(e)), ("clear marks", "selclear", 108, 160, 84, "neutral", br)]
    for t, m, x, y, w, kd, dst in btn:
        b = button(e, t, x, y, w, 24, kd)
        mb = msgbox(e, m); e.wire(b, 0, mb, 0)
        e.wire(mb, 0, dst, 0)
    hold(e, "HOLD 2 s: delete marked", 200, 160, 176, "delarm", "delgo", br)
    info = label(e, "pool: idle", 16, 188, 360, TEXT1, T_BODY, h=32, varname="poolinfo")
    # ---- PRESETS (x - 192)
    e.page, e.dx, e.dy = 2, -192, 40
    label(e, "PRESETS", 400, 40, 360, TEXT1, T_HEAD, True)
    names = SCH["drum_groups"] + ["syn " + a for a in SCH["synth_algos"]]
    em = menu(e, 400, 64, 160, names, "engine")
    pd = menu(e, 568, 64, 88, ["pad %d" % i for i in range(16)], "pad")
    pe, pq = e.hid("prepend engine", ins=2), e.hid("prepend padsel", ins=2)
    e.wire(em, 0, pe, 0); e.wire(pe, 0, br, 0); e.wire(pd, 0, pq, 0); e.wire(pq, 0, br, 0); e.wire(br, 4, em, 0); e.wire(br, 5, pd, 0)
    label(e, "pad: drums only", 664, 68, 96, TEXT2)
    pr = menu(e, 400, 96, 264, [], "presets")
    pk = e.hid("prepend presetpick", ins=2)
    e.wire(pr, 0, pk, 0); e.wire(pk, 0, br, 0); e.wire(br, 0, pr, 0)
    ap = button(e, "preset", 672, 96, 88, 24, "primary")
    ma = msgbox(e, "dopreset"); e.wire(ap, 0, ma, 0); e.wire(ma, 0, br, 0)
    rl = button(e, "reload presets", 400, 128, 112, 24)
    mr = msgbox(e, "presets"); e.wire(rl, 0, mr, 0); e.wire(mr, 0, tx(e), 0)
    rsb = button(e, "resync", 520, 128, 88, 24)
    mg = msgbox(e, "getpatch"); e.wire(rsb, 0, mg, 0); e.wire(mg, 0, tx(e), 0)
    label(e, "f = factory   u = user", 616, 132, 144, TEXT2)
    st = label(e, "patch -  store -", 400, 164, 240, TEXT1, T_BODY, varname="store")
    rt = button(e, "store retry", 648, 160, 112, 24)
    ms = msgbox(e, "storeretry"); e.wire(rt, 0, ms, 0); e.wire(ms, 0, tx(e), 0)
    r = e.hid("route poolinfo store trans", outs=4, outlettype=["", "", "", ""])
    e.wire(br, 3, r, 0); e.wire(r, 0, info, 0); e.wire(r, 1, st, 0)
    # ---- GLOBAL (card space y 40 = old y 240)
    e.page, e.dx, e.dy = 3, 0, -160
    label(e, "GLOBAL", 16, 240, 200, TEXT1, T_HEAD, True)
    ctl(e, 6144, 20, 272, 72, 24, "BPM (follow)", 80, kind="flonum", ly=308)
    ctl(e, 6149, 100, 272, 72, 24, "SCALE", 80, items=SCH["scales"], ly=308)
    for i, (pid, lb) in enumerate([(6148, "KEY"), (6146, "FADER"), (6150, "METRO"), (6147, "INGAIN"), (6145, "SWING")]):
        ctl(e, pid, 16 + (i + 2) * 80 + 20, 264, 40, label_=lb, lw=80, color=ACCENT, ly=308)
    bank_ctl(e, "clklead", "id", 6151, 20, 324, 40, ACCENT, "d", 3, lbl="CLKLEAD", lw=72, ly=364, readout=False, show_label=False)
    label(e, "clock lead (ms): + plays earlier, to match Live's click", 72, 336, 340, TEXT1, T_BODY, varname="clkleadtxt")
    label(e, "-", 416, 336, 80, TEXT1, T_BODY, varname="bv_clklead")
    divider(e, 580, 244, 1, 120)
    label(e, "UNKNOWN TO THIS DEVICE", 588, 240, 172, TEXT2, just=1)
    label(e, "", 588, 256, 172, TEXT1, varname="unknown", just=1)
    menu(e, 588, 276, 172, ["(none)"], "unk")
    label(e, "fw - proto -", 588, 304, 172, TEXT2, varname="ver", just=1)
    label(e, "MIX volumes and the SEQUENCER params live in the lane editors (drums / synth / sampler): one place per job", 16, 392, 744, TEXT2, just=1)
    # ---- BIN (card space y 40 = old y 432)
    e.page, e.dx, e.dy = 4, 0, -352
    label(e, "RECYCLE BIN", 16, 432, 200, TEXT1, T_HEAD, True)
    bm = menu(e, 16, 452, 352, ["(bin: refresh)"], "bin")
    bs = e.hid("prepend binsel", ins=2); e.wire(bm, 0, bs, 0); e.wire(bs, 0, br, 0); e.wire(br, 6, bm, 0)
    for t, m, x, kd in [("refresh", "bin", 376, "neutral"), ("restore", "binrestore", 468, "primary")]:
        bt = button(e, t, x, 452, 84, 24, kd); mb = msgbox(e, m); e.wire(bt, 0, mb, 0); e.wire(mb, 0, br, 0)
    hold(e, "HOLD 2 s: empty bin", 560, 452, 200, "binemptyarm", "binemptygo", br)
    label(e, "deleted pool samples land here first: name, deleted when, boots left (auto-purge after 25 reboots). restore returns it to the pool.", 16, 484, 744, TEXT2)
    
    e.page, e.dx, e.dy = None, 0, 0
    ui(e)
    PAGES[e.scope] = e.pages
    return e


def setvar(p, bid, name, **attrs):
    """name (and extra attributes of) an already created box"""
    for b in p.boxes:
        if b["box"]["id"] == bid:
            b["box"]["varname"] = name
            b["box"].update(attrs)


def sync_block(p, x, w, proto):
    """set <-> PMOR patch sync (pmor_snap.js): heading, status line, hint, and (only on `patchdiff`) the two buttons. The set's copy of the patch lives in a pattr (saved in the Live set):
    proto outlet 2 `snapshot <text>` -> pattr -> `snapload <text>` -> proto (identical text = no-op, so the echo is harmless). UNVERIFIED in Live: if the pattr does not persist, nothing is
    restored and the first connect adopts PMOR's patch."""
    label(p, "set: no copy yet", x, 56, w, TEXT1, T_LABEL, varname="snapline")
    label(p, "", x, 72, w, TEXT2, T_LABEL, varname="snaphint")
    js = p.hid("js pmor_snap.js", outs=3, outlettype=["", "", ""], varname="snap")
    p.wire(proto, 2, js, 0)
    rs = p.hid("route snapshot", outs=2, outlettype=["", ""])
    pa = p.hid("pattr pmor_snapshot @parameter_enable 1", outs=3, outlettype=["", "", ""], varname="snapstore",
               saved_attribute_attributes={"valueof": {"parameter_longname": "PMOR set snapshot", "parameter_shortname": "PMOR snap", "parameter_type": 3, "parameter_invisible": 1}})
    p.params[pa] = ["PMOR set snapshot", "PMOR snap", 0]
    sl = p.hid("prepend snapload", ins=2)
    p.wire(proto, 2, rs, 0); p.wire(rs, 0, pa, 0); p.wire(pa, 0, sl, 0); p.wire(sl, 0, proto, 0)
    ab = button(p, "adopt PMOR's", x, 128, 120, 24, hidden=1)
    setvar(p, ab, "snap_adopt")
    ma = msgbox(p, "adopt"); p.wire(ab, 0, ma, 0); p.wire(ma, 0, js, 0)
    wb = hold(p, "HOLD 2 s: load on PMOR", x + 128, 128, 168, "writearm", "writego", js)
    setvar(p, wb, "snap_write", hidden=1)
    p.wire(js, 0, p.byvar("snapline"), 0); p.wire(js, 1, p.byvar("snaphint"), 0); p.wire(js, 2, proto, 0)


def core():
    """PERFORM view only: status, mode, connect, port, progress, transport line, join line, editor button. Everything else is in the editor.
    Header: routing note left, centred title, editor button right (same strip as the lanes). Card rows (x 16..364, 348 wide): status 40, mode / connect / port 60, progress 92, three
    centred text lines 104 / 120 / 136."""
    p = P(520, 169)
    panels(p, [(8, 32, 504, 128)])
    title(p, "PMOR core", ACCENT)
    label(p, "engine + USB host link", 16, 8, 140, TEXT2)
    edbtn = expand_button(p, ACCENT)
    status = label(p, "disconnected", 16, 40, 384, TEXT1, T_BODY, varname="status")
    # one big toggle button (the 22 px toggle was mis-clicked); a textbutton stores no state, so connect is always OFF on load
    tog = p.add("textbutton", 16, 92, 384, 28, "CONNECT", outs=3, ins=1, outlettype=["", "", "int"], pres=[16, 92, 384, 28], texton="CONNECTED",
                mode=1, bgcolor=RAISED, bgoncolor=OK, textcolor=TEXT1, textoncolor=GROUND, usebgoncolor=1, fontsize=T_BODY, fontname=FONT, rounded=4, varname="connect")
    # engine room
    ed = p.hid("p editor", outs=0, ins=0, varname="editor")           # no inlet: opened by pmor_edit.js
    for b in p.boxes:
        if b["box"]["id"] == ed:
            b["box"]["patcher"] = core_editor().editor_json()
    t0 = p.hid("loadbang", outlettype=["bang"])
    init = p.hid("cachedir ~/Music/PMOR_Cache", cls="message", ins=2)
    portjs = p.hid("js pmor_port.js", outs=0)
    proto = p.hid("pmor.proto", outs=3, outlettype=["", "", ""], varname="proto")
    coren = p.hid("pmor.core~", outs=2, outlettype=["signal", "signal"], varname="core")
    po1, po2 = p.hid("plugout~ 1", outs=0), p.hid("plugout~ 2", outs=0)
    rx = p.hid("receive pmor_midi"); pm = p.hid("prepend midi", ins=2)
    where = p.hid("js pmor_where.js", outlettype=[""])
    sess = p.hid("js pmor_session.js", outs=5, outlettype=["", "", "", "", ""])
    join = p.hid("js pmor_join.js", outs=2, outlettype=["", ""])
    pconn = p.hid("prepend connect", ins=2)
    rt = p.hid("route sample progress", outs=3, outlettype=["", "", ""])
    sset_prog = p.hid("prepend set samples:", ins=2)
    unp = p.hid("unpack s 0 0 0", outs=4, outlettype=["", "int", "int", "int"])
    pct = p.hid("expr 100.*$f1/max(1.,$f2)", ins=2)
    rtx = p.hid("receive pmor_tx")
    snd_rx = p.hid("send pmor_rx", outs=0); snd_st = p.hid("send pmor_ui_status", outs=0)
    brj = p.hid("js pmor_browser.js", outs=8, outlettype=[""] * 8)
    rtt = p.hid("route trans", outs=2, outlettype=["", ""])
    p.wire(t0, 0, init, 0); p.wire(init, 0, proto, 0)       # pmor_where.js decides the cache dir on its own loadbang and on connect
    p.wire(where, 0, proto, 0)                                    # sessiondir <set>/PMOR or bare (proto falls back to ~/Music/PMOR_Cache)
    p.wire(proto, 1, coren, 0); p.wire(proto, 1, snd_rx, 0); p.wire(pm, 0, coren, 0); p.wire(rx, 0, pm, 0)
    p.wire(rtx, 0, proto, 0)
    p.wire(coren, 0, po1, 0); p.wire(coren, 1, po2, 0)
    # connection logic (pmor_session.js): connect toggle is the only thing that opens/closes the port
    p.wire(tog, 0, pconn, 0); p.wire(pconn, 0, sess, 0); p.wire(tog, 0, join, 0)   # textbutton left outlet = toggle state 0/1 (outlet 2 is mouse-over: it disconnected on hover-out)
    lin = p.hid("receive pmor_lnk_in"); p.wire(lin, 0, sess, 0)       # mode / port picked in the window's LINK tab
    p.wire(sess, 0, portjs, 0); p.wire(sess, 1, proto, 0); p.wire(sess, 2, status, 0); p.wire(sess, 3, where, 0)
    def mirror(src, so, name):                                       # a status stream also goes to the window's LINK tab
        sd = p.hid("send pmor_lnk_" + name, outs=0); p.wire(src, so, sd, 0)
    mirror(sess, 4, "mode")
    # proto status stream
    tempo = p.hid("js pmor_tempo.js", outs=1, outlettype=[""])           # Live song tempo -> TEMPO (connect, tempo change, playback start)
    p.wire(tog, 0, tempo, 0); p.wire(proto, 2, tempo, 0); p.wire(tempo, 0, proto, 0)
    p.wire(proto, 2, sess, 0); p.wire(proto, 2, join, 0); pst = p.hid("prepend st", ins=2); p.wire(proto, 2, pst, 0); p.wire(pst, 0, snd_st, 0); p.wire(proto, 2, brj, 0); p.wire(proto, 2, rt, 0)
    p.wire(rt, 0, sset_prog, 0); mirror(sset_prog, 0, "samples")
    p.wire(rt, 1, unp, 0); p.wire(unp, 1, pct, 0); p.wire(unp, 2, pct, 1); mirror(pct, 0, "progress")
    p.wire(brj, 3, rtt, 0); mirror(rtt, 0, "transport")
    p.wire(join, 0, proto, 0); mirror(join, 1, "joinline")
    # EXPAND / MINIMIZE -> pmor_edit.js (front, restore rect, gate helpers, dumpparams/pool/presets) -> session EDIT while open
    edit_wiring(p, edbtn, full=True, sess=sess)
    sync_block(p, 16, 384, proto)
    return p


DEVICES = [("PMOR core", core, "iiii"), ("PMOR lane drums", drums_lane, "mmmm"), ("PMOR lane synth", synth_lane, "mmmm"),
           ("PMOR lane sampler", sampler_lane, "mmmm"), ("PMOR lane control", control_lane, "mmmm")]


DEVICE_JS = ["pmor_snap.js", "pmor_bank.js", "pmor_guard.js", "pmor_tempo.js", "pmor_lane.js", "pmor_port.js", "pmor_where.js", "pmor_ui.js", "pmor_browser.js", "pmor_edit.js", "pmor_session.js", "pmor_join.js", "pmor_fx.js"]   # copy next to the .amxd / into Max 9/Library


# ---- pmor_ui.js (generated: carries the synth schema table) -----------------------------------------------
UI_JS = r'''// pmor_ui.js -- GENERATED by make_devices.py. One instance per patcher scope.
// Inlet messages: `param <id> <value>` (from [receive pmor_rx]) sets the control named p<id> SILENTLY (message "set": no echo);
// Synth / drum / sampler param controls are NOT here any more: they are bank controls bound through the selection (pmor_bank.js). This script serves the p<id>
// controls (global params, FX cells) and the compat stream.
// Argument `gated` (editor instances, see pmor_edit.js): caches `param` values but touches no control while the editor window is closed.
// Outlets: unused (kept for wiring compatibility).
inlets = 1; outlets = 3;
var UMENU_LO = %(UMLO)s;   // schema id -> lo, for enum params whose lo != 0
var SOFT = 73;       // QUANT|ENUM|INT
var cache = {}, open_ = 1;
for (var ai = 1; ai < jsarguments.length; ai++) if (String(jsarguments[ai]) === "gated") open_ = 0;   // editor instance: silent until `editor 1`
function o(n) { return this.patcher.getnamed(n); }
// A [dial] takes `set` in STEPS (output = min + mult*step), not in output units, and a [umenu] in item INDEX (schema value - its lo).
function put(c, v, id) {
    if (!c) return;
    if (c.maxclass === "dial") {
        var mn = Number(c.getattr("min")), mu = Number(c.getattr("mult")), sz = Number(c.getattr("size"));
        var st = (v - mn) / (mu || 1); st = Math.max(0, Math.min(sz - 1, st));
        c.message("set", st);
    } else if (c.maxclass === "umenu") c.message("set", v - (UMENU_LO[id] || 0));
    else c.message("set", v);
}
function param(id, v) {
    cache[id] = v; if (!KS[id]) { vals[id] = v; if (open_ && have && have[id]) showUnknown(); }
    if (!open_) return;                                                              // gated + closed: remember only, draw nothing
    draw(id, v);
}
function draw(id, v) {
    put(o("p" + id), v, id);
}
// pmor_edit.js tells a gated instance when its editor window opens / closes. Opening redraws once from the cache (PMOR's re-emitted
// `dumpparams` then refreshes it); closing stops all drawing again.
function editor(v) {
    open_ = v | 0;
    if (!open_) return;
    for (var k in cache) draw(k | 0, cache[k]);
    if (seen) applySchema();
    showFw();
}
// ---- compat (no version lock): PMOR announces the ids it has after every sync: schemabegin n / schema id name min max def flags group / schemaend n.
// Controls whose id PMOR does not list are DISABLED (dimmed + ignoreclick) and re-enabled by a later sync that lists them; an empty schema (very old
// firmware) disables nothing; `param` for an id without a control is ignored (never an error). Ids PMOR has that this device's compiled-in schema
// lacks are counted into the comment `unknown` and listed (name + value) in the umenu `unk`; the comment `ver` shows "fw .. proto ..".
var KNOWN = %(KNOWN)s;   // ids of the compiled-in schema
var KS = {}; for (var ki = 0; ki < KNOWN.length; ki++) KS[KNOWN[ki]] = 1;
var pend = null, pendN = {}, have = null, haveN = {}, seen = false, orig = {}, vals = {}, fwtxt = "fw - proto -";
var ATTRS = { dial: ["needlecolor", "outlinecolor"], toggle: ["checkedcolor", "elementcolor"], umenu: ["textcolor"], flonum: ["textcolor"] };
//@THEME-BEGIN (generated from make_devices.THEME: the design-system palette, one role per colour; only DISABLED is used here)
var THEME = %(THEME)s;
//@THEME-END
var DIM = THEME.DISABLED;     // a control the firmware does not list is painted DISABLED
function schemabegin(n) { pend = {}; pendN = {}; }
function schema(id, name) { if (!pend) { pend = {}; pendN = {}; } id = id | 0; pend[id] = 1; pendN[id] = String(name); }
function schemaend(n) {
    var c = 0, k;
    for (k in pend) c++;
    have = c ? pend : null; haveN = pendN; pend = null; seen = true;     // empty schema = old firmware: everything stays enabled
    if (open_) applySchema();
}
function enabledId(id) { return have === null || have[id] === 1; }
function setEnabled(oo, vn, on) {
    var a = ATTRS[oo.maxclass], i;
    if (!(vn in orig)) { orig[vn] = {}; if (a) for (i = 0; i < a.length; i++) orig[vn][a[i]] = oo.getattr(a[i]); }
    if (a) for (i = 0; i < a.length; i++) {
        if (on) oo.message.apply(oo, [a[i]].concat(orig[vn][a[i]])); else oo.message.apply(oo, [a[i]].concat(DIM));
    }
    oo.setboxattr("ignoreclick", on ? 0 : 1);
}
function applySchema() {
    var oo = this.patcher.firstobject;
    while (oo) {
        var vn = oo.varname, m = /^p(\d+)$/.exec(vn || "");
        if (m) setEnabled(oo, vn, enabledId(m[1] | 0));
        oo = oo.nextobject;
    }
    showUnknown();
}
function unknownIds() { var ids = []; if (have) for (var k in have) if (!KS[k]) ids.push(k | 0); ids.sort(function (a, b) { return a - b; }); return ids; }
function showUnknown() {
    var ids = unknownIds(), c = o("unknown"), u = o("unk");
    if (c) c.message("set", have === null ? "" : (ids.length ? "+" + ids.length + " unknown params" : "all params known"));
    if (u) {
        u.message("clear");
        for (var i = 0; i < ids.length && i < 64; i++) {
            var v = vals[ids[i]];
            u.message("append", (haveN[ids[i]] || ("id" + ids[i])).replace(/ /g, "_") + "  " + (v === undefined ? "-" : Math.round(v * 1000) / 1000));
        }
        if (!ids.length) u.message("append", "(none)");
    }
}
function parseFw(t) {
    var f = /fw[= ]([^ ]+)/.exec(t), p = /proto[= ]([0-9]+)/.exec(t);
    return (f || p) ? "fw " + (f ? f[1] : "-") + " proto " + (p ? p[1] : "-") : "";
}
function showFw() { if (!open_) return; var c = o("ver"); if (c) c.message("set", fwtxt); }
function info() { var s = parseFw(arrayfromargs(arguments).join(" ")); if (s) { fwtxt = s; showFw(); } }
function fw(f, p) { fwtxt = "fw " + f + " proto " + (p === undefined ? "-" : p); showFw(); }
function anything() { }                  // the rest of the status stream
if (typeof module !== "undefined") module.exports = { param: param, schemabegin: schemabegin, schema: schema, schemaend: schemaend, editor: editor, info: info, unknownIds: unknownIds };   // node test
'''


def write_ui_js():
    umlo = {e[0]: e[2] for e in SCH["schema"] if (e[5] & 8) and e[2] != 0}
    open(os.path.join(HERE, "pmor_ui.js"), "w").write(UI_JS % {"UMLO": json.dumps(umlo), "KNOWN": json.dumps(sorted(E)), "THEME": json.dumps(THEME, sort_keys=True)})


def write_bank_tables():
    """pmor_bank.js is hand-written; only the tables between //@TABLES-BEGIN / //@TABLES-END are generated: BIND (every bank control: kind, k, page, dynamic label),
    SCHR (id -> [name, lo, hi, flags, unit, log] for the synth / drum / sampler / seq / global params), drum sound names, group names. banks.json mirrors BIND for the tests."""
    f = os.path.join(HERE, "pmor_bank.js")
    s = open(f).read()
    a, b = s.index("//@TABLES-BEGIN"), s.index("//@TABLES-END")
    a = s.index("\n", a) + 1
    # domains synth/drums/sampler/seq/global, plus the per-bank FX params (AMOUNT/LIDAR/MIX = FX ids 130..159)
    schr = {e[0]: [e[1], e[2], e[3], e[5], unit_of(e), 1 if is_log(e) else 0] for e in SCH["schema"] if (e[0] >> 10) in (1, 2, 3, 4, 6) or ((e[0] >> 10) == 5 and 130 <= (e[0] & 1023) < 160)}
    for e in SCH["schema"]:       # drum synth params (domains 7..10): ranges are the same for every pad / engine (asserted in the DSL build), one entry per field index k at 7168 + k; pmor_bank.js maps any pad onto it
        if 7 <= (e[0] >> 10) <= 10:
            schr[7168 + (e[0] & 255)] = [e[1], e[2], e[3], e[5], unit_of(e), 1 if is_log(e) else 0]
    cellc = {"ACCENT": THEME["ACCENT"], "GROUND": THEME["GROUND"], "TEXT1": THEME["TEXT1"], "RAISED": THEME["RAISED"], "TINT": GRP_TINT}
    blk = "var BIND = %s, SCHR = %s, DSOUNDS = %s, GROUPS = %s, ALGOS = %s, CELLC = %s, PAGEOF = %s, DSL = %s;\n" % (
        json.dumps(BANK, sort_keys=True), json.dumps(schr, sort_keys=True), json.dumps(SCH["drum_sounds"]), json.dumps(SCH["drum_groups"]), json.dumps(SCH["synth_algos"]),
        json.dumps(cellc, sort_keys=True), json.dumps(PAGES, sort_keys=True), json.dumps([[ly for ly, _ in DS[g]] for g in range(4)]))
    json.dump([[ly for ly, _ in DS[g]] for g in range(4)], open(os.path.join(HERE, "dslayers.json"), "w"))
    json.dump(PAGES, open(os.path.join(HERE, "pages.json"), "w"), indent=1, sort_keys=True)
    open(f, "w").write(s[:a] + blk + s[b:])
    json.dump(BANK, open(os.path.join(HERE, "banks.json"), "w"), indent=1, sort_keys=True)


def write_browser_names():
    """pmor_browser.js is hand-written; only the schema name tables between //@NAMES-BEGIN / //@NAMES-END are generated (fallback names of unnamed presets)."""
    f = os.path.join(HERE, "pmor_browser.js")
    s = open(f).read()
    a, b = s.index("//@NAMES-BEGIN"), s.index("//@NAMES-END")
    a = s.index("\n", a) + 1
    blk = "var DRUM_SOUNDS = %s, SYN_PRESETS = %s;\n" % (json.dumps(SCH["drum_sounds"]), json.dumps(SCH["synth_presets"]))
    open(f, "w").write(s[:a] + blk + s[b:])


def write_fx_names():
    """pmor_fx.js is hand-written; only the effect-name table between //@NAMES-BEGIN / //@NAMES-END is generated from the schema"""
    f = os.path.join(HERE, "pmor_fx.js")
    s = open(f).read()
    a, b = s.index("//@NAMES-BEGIN"), s.index("//@NAMES-END")
    a = s.index("\n", a) + 1
    open(f, "w").write(s[:a] + "var NAMES = %s;\n" % json.dumps(SCH["fx_banks"]) + s[b:])


def amxd(patcher_json, kind):
    """Best-known .amxd container: 'ampf' + type chunk + 'meta' + 'ptch'(JSON). UNVERIFIED against Live from this machine."""
    body = (json.dumps(patcher_json, indent=2) + "\n").encode() + b"\0"
    return (b"ampf" + struct.pack("<I", 4) + kind.encode() + b"meta" + struct.pack("<I", 4) + struct.pack("<I", 0)
            + b"ptch" + struct.pack("<I", len(body)) + body)


if __name__ == "__main__":
    write_ui_js()
    write_browser_names()
    write_fx_names()
    if "--amxd" in sys.argv:
        os.makedirs(BUILD, exist_ok=True)
    built = [(name, fn(), kind) for name, fn, kind in DEVICES]
    write_bank_tables()                       # after the devices: BANK is filled by bank_ctl
    for name, p, kind in built:
        d = p.json(kind)
        txt = json.dumps(d, indent=2) + "\n"
        json.loads(txt)
        open(os.path.join(HERE, name + ".maxpat"), "w").write(txt)
        if "--amxd" in sys.argv:
            open(os.path.join(BUILD, name + ".amxd"), "wb").write(amxd(d, kind))
        print("wrote", name + ".maxpat", "(%d boxes, %d lines)" % (len(p.boxes), len(p.lines)))
    print("DEVICE_JS:", " ".join(DEVICE_JS))

/* demiurge-web — the rig, in your hand.
 *
 * Information architecture, and why it is the way round it is:
 *
 * The panel on the device is NEPTR-first. You are standing on it, so the 19
 * effect menus are the whole screen and the system settings hide behind a
 * companion button. A phone is the second surface, picked up deliberately,
 * usually while the rig is already running — so this is the reverse.
 * DEMIURGE is the top level: patch, audio, interface, patcher, clock,
 * network, system, each a menu row that states its own live value. NEPTR is
 * one destination inside that menu, and going into it puts the instrument —
 * menus, slots, faders, looper, expression — under your thumb.
 *
 * Everything is a screen stack over the URL hash, so the phone's own back
 * gesture works and a screen can be reloaded straight into.
 *
 * Vanilla JS, no frameworks, no external assets.
 */
"use strict";

const $ = id => document.getElementById(id);
const DASH = "—";
const view = $("view");

/* ── live data ────────────────────────────────────────────────────────────
   Two feeds, deliberately separate. The system frame is a 2 s poll of the
   Pi; engine state is pushed the moment the engine says something, because a
   slot has to light like a pedal switch, not like a dashboard. */

let frame = {};          // latest system frame
let neptr = null;        // latest engine snapshot ({} until the first push)
let model = null;        // the instrument's shape, fetched once
let hist = [];
const HIST_MAX = 300;

fetch("/api/history").then(r => r.json()).then(h => {
  hist = h.slice(-HIST_MAX);
  if (hist.length) { frame = hist[hist.length - 1]; refresh(); }
}).catch(() => {});
fetch("/api/status").then(r => r.json()).then(f => {
  if (f && f.ts) { frame = f; refresh(); }
}).catch(() => {});
fetch("/api/neptr/model").then(r => r.json()).then(m => {
  model = m;
  // A NEPTR screen cannot draw a single slot without this, so any screen
  // opened before it landed was showing a placeholder. Build it again now
  // rather than leaving the user on "Loading…" — deep links into
  // #/neptr/6/4 are the normal case on a phone, not the exception.
  rebuild();
}).catch(() => toast("Could not load the instrument. Reload the page."));

const es = new EventSource("/api/events");
es.onmessage = e => {
  $("banner").classList.add("hidden");
  frame = JSON.parse(e.data);
  hist.push(frame);
  if (hist.length > HIST_MAX) hist.shift();
  refresh();
};
es.onopen = () => $("banner").classList.add("hidden");
es.onerror = () => $("banner").classList.remove("hidden");

const nes = new EventSource("/api/neptr/events");
nes.onmessage = e => { neptr = JSON.parse(e.data); refresh(); };

/* ── small helpers ───────────────────────────────────────────────────────── */

function el(tag, cls, text) {
  const n = document.createElement(tag);
  if (cls) n.className = cls;
  if (text != null) n.textContent = text;
  return n;
}

function row(name, opts) {
  opts = opts || {};
  const b = el(opts.tag || "button", "row" + (opts.cls ? " " + opts.cls : ""));
  if (b.tagName === "BUTTON") b.type = "button";
  if (opts.lead) b.appendChild(opts.lead);
  const box = el("span", "name");
  box.appendChild(document.createTextNode(name));
  if (opts.sub) box.appendChild(el("span", "sub", opts.sub));
  b.appendChild(box);
  if (opts.tag_) b.appendChild(el("span", "tag", opts.tag_));
  if (opts.chev) b.appendChild(el("span", "chev", "›"));
  if (opts.on) b.classList.add("on");
  if (opts.onclick) b.onclick = opts.onclick;
  if (opts.disabled) b.disabled = true;   // .empty is for a slot with no effect
  return b;
}

function toast(msg) {
  const t = $("toast");
  t.textContent = msg;
  t.classList.remove("hidden");
  clearTimeout(t._tmr);
  t._tmr = setTimeout(() => t.classList.add("hidden"), 4200);
}

function post(path, body, quiet) {
  return fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body || {}),
  }).then(async r => {
    if (!r.ok && !quiet) {
      const txt = await r.text();
      let msg = txt;
      try { msg = JSON.parse(txt).output || txt; } catch (err) { /* raw */ }
      toast(msg || (path + " failed (" + r.status + ")"));
    }
    return r;
  }).catch(e => { if (!quiet) toast(String(e)); });
}

/* Anything that can take sound away asks first, in the same words the button
   used. */
function confirmSheet(msg, okLabel, fire) {
  $("sheet-msg").textContent = msg;
  $("sheet-ok").textContent = okLabel;
  $("sheet").classList.remove("hidden");
  $("sheet-ok").onclick = () => { $("sheet").classList.add("hidden"); fire(); };
}
$("sheet-cancel").onclick = () => $("sheet").classList.add("hidden");
$("sheet").onclick = e => {
  if (e.target === $("sheet")) $("sheet").classList.add("hidden");
};

/* A press previews the state the control is ABOUT to have and only commits on
   release — the panel's own gesture model, and the reason a mis-landed thumb
   can slide off a fuzz without engaging it. */
function pressable(node, fire, opts) {
  opts = opts || {};
  let armed = false;
  const slop = 16;
  let sx = 0, sy = 0;
  node.addEventListener("pointerdown", e => {
    armed = true; sx = e.clientX; sy = e.clientY;
    node.classList.add("preview");
    if (opts.down) opts.down();
    node.setPointerCapture(e.pointerId);
  });
  node.addEventListener("pointermove", e => {
    if (!armed) return;
    if (Math.abs(e.clientX - sx) > slop || Math.abs(e.clientY - sy) > slop) {
      armed = false;
      node.classList.remove("preview");
      if (opts.up) opts.up();
    }
  });
  const end = () => {
    node.classList.remove("preview");
    if (!armed) return;
    armed = false;
    if (opts.up) opts.up();
    if (fire) fire();
  };
  node.addEventListener("pointerup", end);
  node.addEventListener("pointercancel", () => {
    node.classList.remove("preview");
    if (armed && opts.up) opts.up();
    armed = false;
  });
}

function armButton(btn, label, fire) {
  const orig = btn.textContent;
  btn.onclick = () => {
    if (btn.classList.contains("armed")) {
      btn.classList.remove("armed");
      btn.textContent = orig;
      clearTimeout(btn._tmr);
      fire();
    } else {
      btn.classList.add("armed");
      btn.textContent = label;
      btn._tmr = setTimeout(() => {
        btn.classList.remove("armed");
        btn.textContent = orig;
      }, 3000);
    }
  };
}

/* ── router ──────────────────────────────────────────────────────────────── */

const ROUTES = [];
let screen = null;       // { title, build(), update() }
let depth = 0;

function route(pattern, title, make) { ROUTES.push({ pattern, title, make }); }

function litSegs(r) { return r.pattern.filter(p => !p.startsWith(":")).length; }

function go(hash) { location.hash = hash; }

function navigate() {
  const parts = (location.hash.replace(/^#\/?/, "") || "").split("/")
                  .filter(s => s.length);
  const wasDeeper = parts.length < depth;
  depth = parts.length;
  // A literal segment beats a placeholder: /neptr/panel is the panel, not
  // menu number "panel". Without this the first pattern registered wins, and
  // /neptr/:menu — registered long before them — swallows every named NEPTR
  // screen. Ties keep registration order, because sort() is stable.
  const hit = ROUTES.filter(r => r.pattern.length === parts.length
      && r.pattern.every((p, i) => p.startsWith(":") || p === parts[i]))
    .sort((a, b) => litSegs(b) - litSegs(a))[0];
  const args = hit ? hit.pattern.map((p, i) => p.startsWith(":")
    ? parts[i] : null).filter(a => a !== null) : [];

  view.textContent = "";
  const box = el("div", "screen" + (wasDeeper ? " back" : ""));
  view.appendChild(box);

  if (!hit) { go("/"); return; }
  screen = hit.make.apply(null, args) || {};
  screen.root = box;
  $("back").hidden = parts.length === 0;
  $("back").onclick = () => history.back();
  if (screen.build) screen.build(box);
  setTitle(typeof screen.title === "function" ? screen.title() : hit.title);
  refresh();
  window.scrollTo(0, 0);
}
/* Faders register themselves as they are built, so the registry is cleared
   whenever a screen is torn down. */
function navigateReset() { faderNodes = []; closeSensorEvents(); navigate(); }
window.addEventListener("hashchange", navigateReset);

function setTitle(t) { $("title").textContent = t; }

function refresh() {
  // The bar always answers "is it making sound".
  const note = $("barnote");
  if (frame.audio === "active") { note.textContent = "AUDIO"; note.style.color = ""; }
  else if (frame.audio == null) { note.textContent = ""; }
  else { note.textContent = "AUDIO OFF"; note.style.color = "var(--alert)"; }
  if (screen && screen.update) {
    try { screen.update(); } catch (e) { /* a stale screen is not fatal */ }
  }
}

/* ── HOME: the state of the rig, then the menu ───────────────────────────── */

route([], "DEMIURGE", () => {
  let hero, list;
  return {
    build(box) {
      hero = el("div", "hero");
      box.appendChild(hero);
      list = el("div");
      box.appendChild(list);
    },
    update() {
      hero.textContent = "";
      hero.appendChild(el("div", "lbl", "Loaded patch"));
      hero.appendChild(el("div", "patch big", frame.patch || "none"));
      const chain = (frame.chains || []).map(c =>
        (frame.has_input ? "IN › " : "") + c.join(" › ") + " › OUT");
      if (chain.length) {
        const cw = el("div", "chain scrollx", chain.join("   /   "));
        hero.appendChild(cw);
      }
      const stats = el("div", "stats");
      stats.appendChild(stat("Engine", frame.audio === "active" ? "running" : "stopped",
                             frame.audio === "active" ? "live" : "bad"));
      stats.appendChild(stat("Temp", frame.temp_c != null
        ? frame.temp_c.toFixed(1) + "°" : DASH));
      stats.appendChild(stat("Power", frame.pwr || DASH));
      stats.appendChild(stat("Xruns/min", frame.xrun_60s != null
        ? String(frame.xrun_60s) : DASH));
      hero.appendChild(stats);

      list.textContent = "";
      const a = frame.audio_settings || {};
      const iface = (frame.interfaces || []).find(i => i.active);
      const menu = [
        ["NEPTR", neptrSummary(), "/neptr"],
        ["Patch", frame.patch || "none", "/patch"],
        ["Audio", a.mode ? (MODE_LABEL[a.mode.value] || a.mode.value) + " · "
          + (a.quantum ? a.quantum.value : "?") + " · "
          + (a.rate ? a.rate.value : "?") + " Hz" : "unavailable", "/audio"],
        ["Interface", iface ? (iface.display || iface.name) : "none attached",
         "/interface"],
      ];
      if (frame.graph) menu.push(["Patcher", graphSummary(frame.graph), "/patcher"]);
      menu.push(["Clock", frame.sync
        ? frame.sync.role.toUpperCase() + " · " + frame.sync.bpm.toFixed(1)
          + " BPM" : "unavailable", "/clock"]);
      menu.push(["Network", frame.wifi && frame.wifi.ssid
        ? frame.wifi.ssid + " · " + (frame.wifi.ip || "no address")
        : "not connected", "/net"]);
      menu.push(["System", (frame.host || "pi") + " · " + upFmt(frame.uptime_s),
                 "/system"]);
      for (const [name, sub, href] of menu) {
        list.appendChild(row(name, { sub, chev: true, onclick: () => go(href) }));
      }
    },
  };
});

function stat(label, value, dotCls) {
  const s = el("div", "stat");
  s.appendChild(el("div", "lbl", label));
  const b = el("b");
  if (dotCls) { b.appendChild(el("i", "dot " + dotCls)); b.append(" "); }
  b.append(value);
  s.appendChild(b);
  return s;
}

function upFmt(s) {
  if (s == null) return DASH;
  const m = Math.floor(s / 60), h = Math.floor(m / 60), d = Math.floor(h / 24);
  if (d) return "up " + d + "d " + (h % 24) + "h";
  if (h) return "up " + h + "h " + (m % 60) + "m";
  return "up " + m + "m";
}

function neptrSummary() {
  if (!model) return "loading";
  if (!neptr) return "engine link unknown";
  if (!neptr.live) return "control ready · state feed held by the panel";
  const on = Object.values(neptr.states || {}).filter(v => v === 0).length;
  const menuName = (neptr.menu != null && model.menus[neptr.menu])
    ? model.menus[neptr.menu].name : null;
  return (menuName ? menuName + " · " : "")
    + on + " effect" + (on === 1 ? "" : "s") + " engaged";
}

function graphSummary(g) {
  if (g.mode !== "sync") return "sync layer off";
  const warn = (g.warnings || []).length;
  if (warn) return warn + " port" + (warn === 1 ? "" : "s") + " connected to nothing";
  return "in " + (g.in_path || "?") + " · out " + (g.out_path || "?");
}

/* ── NEPTR: the instrument ───────────────────────────────────────────────── */

/* One line, always visible inside NEPTR, saying exactly what this page can and
   cannot know. The engine broadcasts its state to one UDP port and the panel
   binds it for as long as it runs, so with the panel up this page can drive
   the rig perfectly but cannot see it. Saying so is the whole point: a slot
   that lit up because we ASSUMED our tap worked would be a lie the moment a
   footswitch disagreed. */
function linkStrip() {
  const d = el("div", "link");
  if (!neptr) { d.textContent = "Connecting to the engine…"; return d; }
  const live = neptr.live;
  d.appendChild(el("i", "dot " + (live ? "live" : "")));
  if (live && neptr.synced) {
    d.append("Live — every slot below is lit by the engine itself.");
  } else if (live) {
    d.append("Listening. Waiting for the engine's first state broadcast.");
    const b = el("button", "");
    b.style.cssText = "margin-left:auto;background:none;border:0;color:var(--accent);font:inherit;cursor:pointer";
    b.textContent = "Ask again";
    b.onclick = () => post("/api/neptr/sync");
    d.appendChild(b);
  } else {
    d.classList.add("bad");
    d.append("Controls work. Lit state is unknown — the on-device screen "
             + "holds the engine's state feed. Turn that screen off to see it here.");
  }
  return d;
}

/* NEPTR menu order: neptr/menus.json is the source of truth for the engine's
   menu-cycle order. Drag cards (pointer events: mouse and phone touch), hide
   or show, Save -> PUT /api/neptr/menus (writes the file, regenerates the
   engine include, pushes the order to the running engine over OSC). */
route(["neptr", "menus"], "Menu order", () => {
  let doc = null, listEl, statusEl, saveBtn, dirty = false;
  const order = () => Array.from(listEl.children).map(c => ({
    id: +c.dataset.id, hidden: c.dataset.hidden === "1" }));
  const setStatus = (t, alert) => {
    statusEl.textContent = t;
    statusEl.className = "note" + (alert ? " alert" : "");
  };
  const markDirty = () => {
    dirty = true; saveBtn.disabled = false;
    setStatus("Unsaved changes");
    renumber();
  };
  function renumber() {
    let n = 0;
    for (const c of listEl.children) {
      const hid = c.dataset.hidden === "1";
      c.style.opacity = hid ? ".45" : "1";
      c._pos.textContent = hid ? "—" : String(++n);
      c._eye.textContent = hid ? "Show" : "Hide";
    }
  }
  function card(m) {
    const c = el("div", "row");
    c.dataset.id = m.id; c.dataset.hidden = m.hidden ? "1" : "0";
    c.style.cssText = "display:flex;align-items:center;gap:12px;user-select:none";
    c._pos = el("span", "tag", "");
    c._pos.style.minWidth = "1.6em";
    const grip = el("span", "chev", "☰");
    grip.style.cssText = "touch-action:none;cursor:grab;padding:8px 6px;font-size:20px";
    const name = el("span", "name", m.name);
    const sub = el("span", "sub", "menu " + m.id + " · " +
      (m.conditional ? "only while device present (stays last)"
        : m.name === "NORRIS" ? "touch page (SoundMagic rack)"
        : m.slots.filter(x => x !== "—").length + " effects"));
    name.appendChild(sub);
    c._eye = el("button", "tag", "Hide");
    c._eye.type = "button";
    c._eye.style.cssText = "padding:8px 10px;cursor:pointer";
    c._eye.onclick = () => {
      c.dataset.hidden = c.dataset.hidden === "1" ? "0" : "1"; markDirty();
    };
    grip.addEventListener("pointerdown", ev => {
      ev.preventDefault();
      grip.setPointerCapture(ev.pointerId);
      c.style.background = "var(--raise-2)";
      c.style.position = "relative"; c.style.zIndex = "2";
      const move = e => {
        const kids = Array.from(listEl.children);
        for (const k of kids) {
          if (k === c) continue;
          const r = k.getBoundingClientRect();
          if (e.clientY > r.top && e.clientY < r.bottom) {
            const after = kids.indexOf(k) > kids.indexOf(c);
            listEl.insertBefore(c, after ? k.nextSibling : k);
            markDirty();
            break;
          }
        }
      };
      const up = () => {
        grip.removeEventListener("pointermove", move);
        grip.removeEventListener("pointerup", up);
        grip.removeEventListener("pointercancel", up);
        c.style.background = ""; c.style.position = ""; c.style.zIndex = "";
      };
      grip.addEventListener("pointermove", move);
      grip.addEventListener("pointerup", up);
      grip.addEventListener("pointercancel", up);
    });
    c.append(c._pos, grip, name, c._eye);
    return c;
  }
  function load() {
    return fetch("/api/neptr/menus").then(r => r.json()).then(d => {
      if (!d.menus) throw new Error(d.error || "no menus");
      doc = d; listEl.textContent = "";
      for (const m of d.menus) listEl.appendChild(card(m));
      dirty = false; saveBtn.disabled = true; renumber();
      setStatus("Loaded " + d.menus.length + " menus. Order = the swipe/encoder cycle.");
    }).catch(e => setStatus("Could not load menus.json: " + e, true));
  }
  return {
    build(box) {
      box.appendChild(el("div", "lbl", "Menu cycle order"));
      listEl = el("div"); listEl.style.marginTop = "10px";
      box.appendChild(listEl);
      saveBtn = el("button", "row", "Save to engine");
      saveBtn.type = "button"; saveBtn.style.marginTop = "14px";
      saveBtn.disabled = true;
      saveBtn.onclick = () => {
        setStatus("Saving…");
        fetch("/api/neptr/menus", { method: "PUT",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ menus: order() }) })
          .then(r => r.json().then(j => ({ ok: r.ok, j })))
          .then(({ ok, j }) => {
            if (!ok) { setStatus(j.error || "save failed", true); return; }
            dirty = false; saveBtn.disabled = true;
            setStatus("Saved. " + (j.generated === false ? "Generator FAILED: " + j.generator + ". " : "")
              + (j.pushed ? "Pushed to engine (" + j.cycle.length + " menus live)."
                          : "Engine push failed - file saved, takes effect at next restart."),
              j.generated === false || !j.pushed);
          }).catch(e => setStatus(String(e), true));
      };
      box.appendChild(saveBtn);
      statusEl = el("div", "note", "Loading…");
      box.appendChild(statusEl);
      box.appendChild(el("div", "note",
        "Menu numbers never change; only the order and visibility do. " +
        "Moving effects between menus is a later phase."));
      load();
    },
  };
});

/* NEPTR spectral rack (SoundMagic): every effect, every parameter, plus the
   hero-pin curation flow. Registry = GET /api/neptr/sm/registry
   (neptr/csound/spectral/sm_params.json). Writes: POST /sm/param -> /sp,
   POST /sm/state -> /st, PUT /sm/heroes -> sm_heroes.json + regen + live
   /sh /shp. Values come back via the engine's 8005 mirror; until it is heard
   the page shows what it last sent and marks it unconfirmed. */
route(["neptr", "spectral"], "Spectral rack", () => {
  let reg = null, st = null, statusEl, mirrorEl, listEl, timer = null;
  const me = {};
  const ctl = {};            // abs idx -> { set(v, confirmed) }
  const onCtl = {};          // fx -> { set(on, confirmed) }
  const pinState = {};       // fx -> { heroes:[p|-1 x7], pedal, rows:[fn] }
  const open = new Set();
  const PHASE = { 1: "live", 2: "phase 2", 3: "phase 3" };
  const SLOTS = 7;

  const put = (path, body) => fetch(path, { method: "PUT",
    headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) })
    .then(r => r.json().then(j => ({ ok: r.ok, j })));
  const postJ = (path, body) => fetch(path, { method: "POST",
    headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) })
    .then(r => r.json().then(j => ({ ok: r.ok, j })))
    .catch(e => ({ ok: false, j: { error: String(e) } }));

  const logTaper = q => q.min > 0 && q.max / q.min > 100;
  const toFrac = (q, v) => {
    if (q.max === q.min) return 0;
    return logTaper(q)
      ? Math.log(Math.max(v, q.min) / q.min) / Math.log(q.max / q.min)
      : (v - q.min) / (q.max - q.min);
  };
  const fromFrac = (q, f) => logTaper(q)
    ? q.min * Math.pow(q.max / q.min, f) : q.min + f * (q.max - q.min);
  const fmt = (q, v) => {
    const span = q.max - q.min;
    const whole = Number.isInteger(q.min) && Number.isInteger(q.max) && span >= 20;
    const dec = whole || Math.abs(v) >= 1000 ? 0 : (span >= 20 ? 1 : 2);
    return v.toFixed(dec) + (q.unit && q.unit !== "on/off" ? " " + q.unit : "");
  };
  const sender = q => {       // throttle: <= 1 post per 90 ms, last value wins
    let last = 0, pend = null, tmr = null;
    const fire = () => { tmr = null; last = Date.now();
      const b = pend; pend = null;
      if (b) postJ("/api/neptr/sm/param", b).then(r => {
        if (!r.ok) setStatus(r.j.error || "param failed", true); }); };
    return (fx, v) => { pend = { fx, p: q.p, val: v };
      const wait = 90 - (Date.now() - last);
      if (wait <= 0) fire(); else if (tmr === null) tmr = setTimeout(fire, wait); };
  };
  const setStatus = (t, alert) => {
    statusEl.textContent = t; statusEl.className = "note" + (alert ? " alert" : "");
  };

  /* ---- one parameter row ---- */
  function paramRow(e, q) {
    const wrap = el("div", "sm-p");
    const head = el("div", "head");
    head.appendChild(el("span", "lbl", q.name));
    const val = el("span", "val", DASH);
    head.appendChild(val);
    wrap.appendChild(head);
    const send = sender(q);
    let busy = false, kindSet;
    const commit = v => { ctl[q.idx].cur = v; send(e.fx, v); };

    if (q.kind === "bool") {
      const sw = el("button", "sm-sw"); sw.type = "button";
      sw.setAttribute("role", "switch");
      sw.onclick = () => { const v = sw.classList.contains("on") ? 0 : 1;
        kindSet(v); commit(v); };
      wrap.appendChild(sw);
      kindSet = v => { sw.classList.toggle("on", v >= 0.5);
        sw.setAttribute("aria-checked", v >= 0.5 ? "true" : "false");
        val.textContent = v >= 0.5 ? "ON" : "off"; };
    } else if (q.name === "FFT Size" && q.enum) {
      const chips = el("div", "sm-chips");
      const bs = q.enum.map((lab, i) => { const b = el("button", "sm-chipb", lab);
        b.type = "button"; b.onclick = () => { kindSet(i); commit(i); };
        chips.appendChild(b); return b; });
      wrap.appendChild(chips);
      kindSet = v => { bs.forEach((b, i) => b.classList.toggle("on", i === Math.round(v)));
        val.textContent = q.enum[Math.round(v)] || DASH; };
    } else if (q.kind === "enum") {
      const sel = el("select", "sm-sel");
      q.enum.forEach((lab, i) => { const o = el("option", null, lab); o.value = i;
        sel.appendChild(o); });
      sel.onchange = () => { const v = +sel.value; kindSet(v); commit(v); };
      wrap.appendChild(sel);
      kindSet = v => { sel.value = String(Math.round(v));
        val.textContent = q.enum[Math.round(v)] || DASH; };
    } else {
      const rg = el("input", "sm-range"); rg.type = "range";
      rg.min = 0; rg.max = 1000; rg.step = 1;
      rg.oninput = () => { busy = true; const v = fromFrac(q, rg.value / 1000);
        val.textContent = fmt(q, v); commit(v); };
      rg.onchange = () => { busy = false; };
      wrap.appendChild(rg);
      kindSet = v => { if (busy) return;
        rg.value = Math.round(toFrac(q, v) * 1000); val.textContent = fmt(q, v); };
    }
    ctl[q.idx] = { cur: null, set(v, confirmed) {
      this.cur = v; kindSet(v);
      wrap.classList.toggle("unc", !confirmed);
    } };
    ctl[q.idx].set(q.default, false);
    wrap.classList.add("dflt");

    /* pin controls: hero slot P1..P7 (unique per effect) + one pedal pick */
    const pin = el("div", "sm-pin");
    const hs = el("select", "sm-sel sm-hs"); hs.title = "hero slot";
    const pb = el("button", "sm-pedal", "PEDAL"); pb.type = "button";
    const ps = pinState[e.fx];
    const sync = () => {
      const k = ps.heroes.indexOf(q.p);
      hs.textContent = "";
      const none = el("option", null, "no hero"); none.value = "-1"; hs.appendChild(none);
      for (let s = 0; s < SLOTS; s++) {
        const holder = ps.heroes[s];
        const o = el("option", null, "P" + (s + 1) +
          (holder >= 0 && holder !== q.p ? " (" + e.params[holder].name + ")" : ""));
        o.value = String(s);
        hs.appendChild(o);
      }
      hs.value = String(k);
      pb.classList.toggle("on", ps.pedal === q.p);
      wrap.classList.toggle("pinned", k >= 0 || ps.pedal === q.p);
    };
    hs.onchange = () => {
      const s = +hs.value;
      const old = ps.heroes.indexOf(q.p);
      if (old >= 0) ps.heroes[old] = -1;
      if (s >= 0) {
        const holder = ps.heroes[s];
        if (holder >= 0 && holder !== q.p) {      // slot taken: swap holder to my old slot
          ps.heroes[s] = q.p; if (old >= 0) ps.heroes[old] = holder;
        } else ps.heroes[s] = q.p;
      }
      ps.save();
    };
    pb.onclick = () => { ps.pedal = q.p; ps.save(); };
    pin.append(hs, pb);
    wrap.appendChild(pin);
    ps.rows.push(sync); sync();
    return wrap;
  }

  /* ---- one effect card ---- */
  function effectCard(e) {
    const ps = pinState[e.fx] = { heroes: Array(SLOTS).fill(-1), pedal: e.pedal,
      rows: [], tmr: null };
    e.heroes.forEach((p, k) => { if (k < SLOTS) ps.heroes[k] = p; });
    ps.save = () => {
      ps.rows.forEach(f => f());
      pinNote.textContent = "saving pins…";
      clearTimeout(ps.tmr);
      ps.tmr = setTimeout(() => put("/api/neptr/sm/heroes",
        { fx: e.fx, heroes: ps.heroes.map(h => h < 0 ? null : h), pedal: ps.pedal })
        .then(({ ok, j }) => {
          pinNote.textContent = ok ? "pins saved" + (j.pushed ? " · pushed live" : " · engine push failed")
                                   : (j.error || "save failed");
          pinNote.className = "note" + (ok && j.pushed ? "" : " alert");
        }).catch(err => { pinNote.textContent = String(err); }), 500);
    };
    const card = el("div", "sm-card");
    const head = el("div", "sm-head");
    const tgl = el("button", "sm-sw"); tgl.type = "button";
    tgl.setAttribute("role", "switch");
    onCtl[e.fx] = { set(on, confirmed) {
      tgl.classList.toggle("on", !!on); tgl.setAttribute("aria-checked", on ? "true" : "false");
      card.classList.toggle("lit", !!on); head.classList.toggle("unc", !confirmed);
    } };
    onCtl[e.fx].set(0, false);
    tgl.onclick = () => { const on = tgl.classList.contains("on") ? 0 : 1;
      onCtl[e.fx].set(on, false);
      postJ("/api/neptr/sm/state", { fx: e.fx, on }).then(r => {
        if (!r.ok) setStatus(r.j.error || "toggle failed", true); }); };
    const title = el("button", "sm-title"); title.type = "button";
    title.appendChild(el("span", "n", e.title.replace(/^Spectral /, "")));
    title.appendChild(el("span", "badge ph" + e.phase, PHASE[e.phase] || ("phase " + e.phase)));
    title.appendChild(el("span", "chev", "›"));
    head.append(tgl, title);
    card.appendChild(head);
    const body = el("div", "sm-body hidden");
    const pinNote = el("div", "note", "");
    let built = false;
    title.onclick = () => {
      if (!built) {
        built = true;
        const bar = el("div", "btnrow");
        const rs = el("button", "btn", "reset to his defaults"); rs.type = "button";
        rs.onclick = () => postJ("/api/neptr/sm/reset", { fx: e.fx }).then(r => {
          if (!r.ok) { setStatus(r.j.error || "reset failed", true); return; }
          e.params.forEach(q => ctl[q.idx].set(q.default, false));
          setStatus(e.title + ": defaults sent"); });
        bar.appendChild(rs); body.appendChild(bar);
        body.appendChild(el("div", "note",
          "Pin a param to a hero slot (P1-P7 = encoder order, P1 is the tile fader) and mark one PEDAL. Saved automatically, pushed live."));
        body.appendChild(pinNote);
        for (const q of e.params) body.appendChild(paramRow(e, q));
        if (st) applyState();
      }
      const isOpen = body.classList.toggle("hidden") === false;
      card.classList.toggle("open", isOpen);
      if (isOpen) open.add(e.fx); else open.delete(e.fx);
    };
    card.appendChild(body);
    card._open = () => { if (body.classList.contains("hidden")) title.onclick(); };
    return card;
  }

  function applyState() {
    if (!st) return;
    const m = st.mirror || {};
    mirrorEl.textContent = m.live
      ? "Engine mirror live on :" + m.port + " — values confirmed"
      : "Engine mirror not heard (:" + m.port + ") — showing what this page sent (unconfirmed)";
    mirrorEl.className = "note" + (m.live ? "" : " alert");
    for (const [fx, o] of Object.entries(st.on || {}))
      if (onCtl[fx]) onCtl[fx].set(o.on, o.confirmed);
    for (const [idx, o] of Object.entries(st.vals || {})) {
      const c = ctl[idx];
      if (c) c.set(o.v, o.confirmed);
    }
  }
  function poll() {
    if (screen !== me) { clearInterval(timer); return; }
    fetch("/api/neptr/sm/state").then(r => r.json())
      .then(s => { st = s; applyState(); }).catch(() => {});
  }

  Object.assign(me, {
    build(box) {
      mirrorEl = el("div", "note", "");
      statusEl = el("div", "note", "Loading registry…");
      box.append(mirrorEl, statusEl);
      listEl = el("div"); box.appendChild(listEl);
      box.appendChild(el("div", "note",
        "SoundMagic Spectral by Michael Norris (michaelnorris.info). Phase-2/3 effects accept settings now; they make sound once their kernels ship."));
      fetch("/api/neptr/sm/registry").then(r => r.json()).then(r => {
        if (!r.effects) throw new Error(r.error || "no registry");
        reg = r; statusEl.textContent = reg.effects.length + " effects · " +
          reg.effects.reduce((n, e) => n + e.params.length, 0) + " params";
        const groups = [["spectral", "Spectral"], ["time", "Time-domain"]];
        for (const [kind, label] of groups) {
          const es = reg.effects.filter(e => e.kind === kind);
          const lbl = el("div", "lbl", label + " · " + es.length);
          lbl.style.cssText = "margin:18px 0 8px";
          listEl.appendChild(lbl);
          for (const e of es) listEl.appendChild(effectCard(e));
        }
        const fftRow = el("div", "sm-card");
        fftRow.style.padding = "12px 14px";
        fftRow.appendChild(el("div", "lbl", "Rack default FFT size"));
        const chips = el("div", "sm-chips");
        reg.fft_sizes.forEach((n, i) => { const b = el("button", "sm-chipb", String(n));
          b.type = "button"; b.onclick = () => {
            chips.childNodes.forEach((c, j) => c.classList.toggle("on", i === j));
            postJ("/api/neptr/sm/param", { fx: 23, p: 0, val: i }); };
          chips.appendChild(b); });
        fftRow.appendChild(chips); listEl.appendChild(fftRow);
        poll(); timer = setInterval(poll, 700);
      }).catch(err => setStatus("Could not load the registry: " + err, true));
    },
  });
  return me;
});

/* NEPTR index: menu order, spectral rack, panel, sensors, then the menus. */
route(["neptr"], "NEPTR", () => {
  let strip, list;
  return {
    build(box) {
      strip = el("div"); box.appendChild(strip);
      list = el("div"); box.appendChild(list);
    },
    update() {
      strip.textContent = "";
      strip.appendChild(linkStrip());
      list.textContent = "";
      if (!model) { list.appendChild(el("div", "note", "Loading the instrument…")); return; }
      // Tools first, in the order they are reached for at the rig, then the
      // generated per-menu pages (model = neptr_model.py, 21 menus).
      list.appendChild(el("div", "lbl", "Rig"));
      const tools = el("div"); tools.style.marginTop = "10px";
      tools.appendChild(row("Menu order", {
        sub: "Drag to reorder the menu cycle · hide menus · live to the engine",
        chev: true, onclick: () => go("/neptr/menus"),
      }));
      tools.appendChild(row("Spectral rack", {
        sub: "SoundMagic: 23 effects, every param · hero pins · pedal picks",
        chev: true, onclick: () => go("/neptr/spectral"),
      }));
      tools.appendChild(row("Virtual panel", {
        sub: "Encoders, buttons and sensors — no Teensy required",
        chev: true, onclick: () => go("/neptr/panel"),
      }));
      tools.appendChild(row("Sensors", {
        sub: sensorsSummary(), chev: true,
        onclick: () => go("/neptr/sensors"),
      }));
      list.appendChild(tools);
      const menuLbl = el("div", "lbl", "Effect menus");
      menuLbl.style.marginTop = "20px";
      list.appendChild(menuLbl);
      const menuWrap = el("div"); menuWrap.style.marginTop = "10px";
      for (const m of model.menus) {
        const lit = m.slots.filter(s => !s.empty && isOn(s.code)).length;
        const known = neptr && neptr.live;
        const sub = m.looper ? "5 loop channels"
          : m.name === "NORRIS" ? "SoundMagic rack · touch page"
          : (known ? (lit ? lit + " engaged" : "all bypassed")
                   : m.slots.filter(s => !s.empty).length + " effects");
        // Where the panel is standing right now gets a quiet word, not a
        // colour: it is information, and the one alarm colour is spoken for.
        const onPanel = neptr && neptr.menu === m.index;
        const r = row(m.name, {
          sub, chev: true, tag_: onPanel ? "on panel" : "",
          onclick: () => go("/neptr/" + m.index),
        });
        if (known && lit) r.classList.add("on");
        menuWrap.appendChild(r);
      }
      list.appendChild(menuWrap);
    },
  };
});

function isOn(code) {
  return !!(neptr && neptr.states && neptr.states[code] === 0);
}
function stateKnown(code) {
  return !!(neptr && neptr.live && neptr.states && code in neptr.states);
}

/* A menu: five slots, drawn in the panel's own order (top row is hardware
   slot 4) so muscle memory carries over, plus the expression strip the panel
   pins under every menu. */
route(["neptr", ":menu"], "", menuIdx => {
  const idx = parseInt(menuIdx, 10);
  let slots = [], strip, spec;
  return {
    title: () => (model && model.menus[idx]) ? model.menus[idx].name : "NEPTR",
    build(box) {
      if (!model || !model.menus[idx]) {
        box.appendChild(el("div", "note", "Loading…"));
        return;
      }
      spec = model.menus[idx];
      setTitle(spec.name);
      strip = el("div"); box.appendChild(strip);
      if (spec.name === "NORRIS") {
        // Menu 20 is the touch-only SoundMagic rack page: no slot rows here.
        const sr = row("Spectral rack", {
          sub: "All 23 SoundMagic effects · every param · hero pins",
          chev: true, onclick: () => go("/neptr/spectral"),
        });
        sr.style.marginBottom = "10px";
        box.appendChild(sr);
      }

      const list = el("div");
      for (const s of (spec.name === "NORRIS" ? [] : spec.slots)) {
        if (s.empty) {
          const r = row("—", { cls: "slot empty", disabled: true });
          list.appendChild(r); slots.push(null);
          continue;
        }
        const r = row(s.label, {
          cls: "slot", lead: el("span", "hw num", "s" + s.hw),
          sub: spec.looper ? "hold to overdub · tap to play/stop" : "",
        });
        if (spec.looper) {
          // Looper rows are HELD buttons: the engine classifies tap /
          // double-tap / 0.4 s hold from the real press and release, so the
          // gesture has to be passed through with its true timing.
          bindHold(r, idx, s.row);
        } else {
          pressable(r, () => post("/api/neptr/slot", { menu: idx, row: s.row }),
                    { down: () => r.classList.toggle("on"),
                      up: () => r.classList.toggle("on") });
          if (s.params.length) {
            const chev = el("span", "chev", "›");
            chev.style.padding = "0 4px";
            chev.onpointerdown = e => e.stopPropagation();
            chev.onclick = e => {
              e.stopPropagation();
              go("/neptr/" + idx + "/" + s.row);
            };
            r.appendChild(chev);
          }
        }
        list.appendChild(r);
        slots.push(r);
      }
      box.appendChild(list);

      const pager = el("div", "pager");
      const prev = el("button", null, "‹ PREV MENU");
      const next = el("button", null, "NEXT MENU ›");
      prev.onclick = () => pageMenu(idx, -1);
      next.onclick = () => pageMenu(idx, 1);
      pager.appendChild(prev); pager.appendChild(next);
      box.appendChild(pager);

      box.appendChild(exprStrip());
    },
    update() {
      if (!spec) return;
      strip.textContent = "";
      strip.appendChild(linkStrip());
      spec.slots.forEach((s, i) => {
        const r = slots[i];
        if (!r || s.empty) return;
        if (spec.looper) {
          const st = neptr && neptr.looper ? neptr.looper.states[s.row] : null;
          r.classList.toggle("on", st === 1 || st === 2);
          r.classList.toggle("unknown", !neptr || !neptr.live);
          const tag = r.querySelector(".tag");
          const text = LOOP_STATE[st] || (neptr && neptr.live ? "empty" : "?");
          if (tag) tag.textContent = text;
          else r.insertBefore(el("span", "tag", text), r.querySelector(".chev"));
        } else {
          r.classList.toggle("on", isOn(s.code));
          r.classList.toggle("unknown", !stateKnown(s.code));
        }
      });
    },
  };
});

const LOOP_STATE = { 0: "empty", 1: "playing", 2: "recording", 3: "stopped" };

function bindHold(node, menu, rowIdx) {
  node.addEventListener("pointerdown", e => {
    node.setPointerCapture(e.pointerId);
    node.classList.add("preview");
    post("/api/neptr/hold", { menu, row: rowIdx, down: true }, true);
  });
  const up = () => {
    node.classList.remove("preview");
    post("/api/neptr/hold", { menu, row: rowIdx, down: false }, true);
  };
  node.addEventListener("pointerup", up);
  node.addEventListener("pointercancel", up);
}

/* Paging sends the engine's own menu prev/next button CC — the same press the
   panel's swipe sends — and then follows to the neighbouring screen. */
function pageMenu(idx, delta) {
  const n = model ? model.menus.length : 19;
  post("/api/neptr/menu", { dir: delta > 0 ? "next" : "prev" }, true);
  go("/neptr/" + ((idx + delta + n) % n));
}

/* The expression pedal: absolute CC29, exactly what the physical pedal sends.
   It is the one control here with no echo from the engine, so its thumb is
   local by necessity — and labelled as such rather than pretending. */
function exprStrip() {
  const wrap = el("div", "fader expr");
  wrap.style.setProperty("--f", "var(--f3)");
  const head = el("div", "head");
  head.appendChild(el("span", "lbl", "Expression pedal"));
  const val = el("span", "val", "—");
  head.appendChild(val);
  wrap.appendChild(head);
  const track = el("div", "track");
  const fill = el("div", "fill");
  track.appendChild(fill);
  wrap.appendChild(track);
  wrap.appendChild(el("div", "note", "Absolute CC29, same as the pedal. "
    + "The engine sends no echo for it, so this thumb is local to your phone."));

  let last = 0;
  bindDrag(track, wrap, frac => {
    const v = Math.round(frac * 127);
    fill.style.width = (frac * 100) + "%";
    val.textContent = String(v);
    if (v !== last) { last = v; return { value: v }; }
    return null;
  }, body => post("/api/neptr/expr", body, true));
  return wrap;
}

/* One drag implementation for every fader on the page. `onmove` maps a 0..1
   position to a request body (or null to send nothing); `send` posts it,
   throttled, with the final position always delivered. */
function bindDrag(track, wrap, onmove, send) {
  let dragging = false, pending = null, timer = null;
  const RATE = 90;                    // ms between posts while dragging

  const flush = () => {
    timer = null;
    if (pending) { send(pending); pending = null; }
  };
  const push = body => {
    if (!body) return;
    pending = body;
    if (timer === null) { flush(); timer = setTimeout(flush, RATE); }
  };
  const at = e => {
    const r = track.getBoundingClientRect();
    return Math.max(0, Math.min(1, (e.clientX - r.left) / Math.max(1, r.width)));
  };
  track.addEventListener("pointerdown", e => {
    if (track.dataset.unavailable) return;
    dragging = true;
    track.setPointerCapture(e.pointerId);
    wrap.classList.add("grabbing");
    push(onmove(at(e)));
    e.preventDefault();
  });
  track.addEventListener("pointermove", e => {
    if (!dragging) return;
    push(onmove(at(e)));
  });
  const end = e => {
    if (!dragging) return;
    dragging = false;
    wrap.classList.remove("grabbing");
    clearTimeout(timer); timer = null;
    const body = onmove(at(e));
    if (body) send(body); else if (pending) send(pending);
    pending = null;
  };
  track.addEventListener("pointerup", end);
  track.addEventListener("pointercancel", end);
}

/* A slot's parameters. Every fader here is a control loop over relative
   encoder detents, and every one of them is gated by the engine's arming
   rule: a detent edits whichever effect was toggled LAST, so a fader aimed at
   an effect that is not armed would silently edit a different pedal. That is
   surfaced, not worked around. */
route(["neptr", ":menu", ":row"], "", (menuIdx, rowIdx) => {
  const mi = parseInt(menuIdx, 10), ri = parseInt(rowIdx, 10);
  let spec, slot, strip, armBox, faders = [];
  return {
    title: () => (model && model.menus[mi] && model.menus[mi].slots[ri])
      ? model.menus[mi].slots[ri].label : "NEPTR",
    build(box) {
      if (!model || !model.menus[mi]) return;
      spec = model.menus[mi];
      slot = spec.slots[ri];
      if (!slot) { go("/neptr/" + mi); return; }
      setTitle(slot.label);

      strip = el("div"); box.appendChild(strip);

      const toggle = row(slot.label, {
        cls: "slot", lead: el("span", "hw num", "s" + slot.hw),
        sub: "Tap to engage · arms the knobs",
      });
      pressable(toggle, () => post("/api/neptr/slot", { menu: mi, row: ri }),
                { down: () => toggle.classList.toggle("on"),
                  up: () => toggle.classList.toggle("on") });
      box.appendChild(toggle);
      faders.push({ node: toggle, kind: "toggle" });

      armBox = el("div"); box.appendChild(armBox);

      if (!slot.params.length) {
        box.appendChild(el("div", "note",
          "This slot has no parameters — it is a straight bypass."));
        return;
      }
      const sec = el("section");
      sec.id = "params";
      sec.appendChild(el("div", "lbl", "Parameters"));
      slot.params.forEach((p, i) => {
        sec.appendChild(buildFader(slot, p, i, mi, ri));
        faders.push({ param: p });
      });
      box.appendChild(sec);
      this.sec = sec;
    },
    update() {
      if (!spec || !slot) return;
      strip.textContent = "";
      strip.appendChild(linkStrip());
      const t = faders[0] && faders[0].node;
      if (t) {
        t.classList.toggle("on", isOn(slot.code));
        t.classList.toggle("unknown", !stateKnown(slot.code));
      }
      armBox.textContent = "";
      const arm = armNote(slot);
      if (arm) armBox.appendChild(arm);
      // A fader that the engine would refuse must not LOOK live. When the
      // knobs are armed to another effect the whole set goes inert behind
      // the note above, which is the thing to act on.
      if (this.sec) this.sec.classList.toggle("gated",
                                              armState(slot) === "elsewhere");
      for (const f of faderNodes) f.sync();
    },
  };
});

/* Does the engine currently have THIS effect's knobs armed? Three answers,
   and the third one matters: unknown is not the same as no. */
function armState(slot) {
  const toggles = new Set(slot.params.map(p => p.toggle).filter(t => t != null));
  if (!neptr || !neptr.live) return "unknown";
  if (neptr.last_toggle == null) return "unknown";
  if (!toggles.size) return "n/a";
  return toggles.has(neptr.last_toggle) ? "armed" : "elsewhere";
}

function armNote(slot) {
  const st = armState(slot);
  if (st === "armed" || st === "n/a") return null;
  const d = el("div", "arm");
  if (st === "elsewhere") {
    d.append("The engine's knobs are armed to a different effect, so these "
             + "faders would edit that one.");
    const b = el("button", null, "ARM THIS");
    b.onclick = () => {
      // Arming is not a special mechanism: it is the slot toggle, which is
      // what sets the engine's gkLastToggle in the first place.
      const on = isOn(slot.code);
      confirmSheet(on
        ? slot.label + " is already engaged. Arming it means toggling it off "
          + "and back on — that will be audible."
        : "Engage " + slot.label + " to arm its knobs?",
        "Engage", () => post("/api/neptr/slot",
                             { menu: currentMenu(), row: slot.row }));
    };
    d.appendChild(b);
  } else {
    d.append("Without the engine's state feed, arming is unknown and there is "
             + "no value for a drag to aim at. Tap the slot above to arm it, "
             + "then use − and + — one encoder detent each, the same as one "
             + "click of the knob.");
  }
  return d;
}

function currentMenu() {
  const parts = location.hash.split("/");
  return parseInt(parts[2], 10);
}

let faderNodes = [];

function buildFader(slot, p, i, mi, ri) {
  const wrap = el("div", "fader");
  wrap.style.setProperty("--f", "var(--f" + (i % 4) + ")");
  const head = el("div", "head");
  head.appendChild(el("span", "lbl", p.label));
  const val = el("span", "val", DASH);
  head.appendChild(val);
  wrap.appendChild(head);

  const track = el("div", "track");
  const fill = el("div", "fill");
  const edge = el("div", "edge");
  const target = el("div", "target hidden");
  track.appendChild(fill); track.appendChild(edge); track.appendChild(target);

  const holder = el("div", "steps");
  const minus = el("button", null, "−");
  const plus = el("button", null, "+");
  holder.appendChild(minus); holder.appendChild(track); holder.appendChild(plus);
  wrap.appendChild(holder);
  minus.onclick = () => step(-1);
  plus.onclick = () => step(1);
  function step(dir) {
    post("/api/neptr/step",
         { code: slot.code, param: p.name, dir: dir > 0 ? "up" : "down" });
  }

  if (p.cc == null) {
    // 19 of the instrument's faders have no encoder in the engine at all.
    // They are shown, because the parameter is real and the engine reports
    // its value — but they are inert, and saying so beats a fader that does
    // nothing when you drag it.
    track.dataset.unavailable = "1";
    minus.disabled = plus.disabled = true;
    minus.style.opacity = plus.style.opacity = ".3";
    wrap.appendChild(el("div", "note",
      "No encoder path in the engine — this one is read-only here, and "
      + "on the panel too."));
  } else {
    bindDrag(track, wrap, frac => {
      const v = p.lo + (p.hi - p.lo) * frac;
      target.classList.remove("hidden");
      target.style.left = (frac * 100) + "%";
      val.textContent = fmtParam(p, v);
      return { code: slot.code, param: p.name, value: v };
    }, body => post("/api/neptr/param", body).then(r => {
      if (r && r.status === 409) {
        // No feed, or armed elsewhere: fall back to honest single steps.
        target.classList.add("hidden");
      }
    }));
  }

  const entry = {
    sync() {
      // Dragging is a closed loop over the engine's echo. With no echo there
      // is no loop, so the track stops offering itself and the two step
      // buttons — one detent each, exactly one click of the knob — are the
      // honest controls left.
      if (p.cc != null) {
        const canChase = !!(neptr && neptr.live);
        if (canChase) delete track.dataset.unavailable;
        else track.dataset.unavailable = "1";
      }
      const raw = neptr && neptr.params ? neptr.params[p.addr] : undefined;
      if (raw == null) {
        val.textContent = DASH;
        fill.style.width = "0";
        edge.style.left = "0";
        return;
      }
      const frac = Math.max(0, Math.min(1,
        (raw - p.lo) / ((p.hi - p.lo) || 1)));
      fill.style.width = (frac * 100) + "%";
      edge.style.left = (frac * 100) + "%";
      if (!wrap.classList.contains("grabbing")) {
        val.textContent = fmtParam(p, raw);
        target.classList.add("hidden");
      }
    },
  };
  faderNodes.push(entry);
  entry.sync();
  return wrap;
}

/* Stepped params read as words, not as 3.00 — the panel does the same, and
   "1+2+3" tells you which tape heads are live where a number does not. */
function fmtParam(p, v) {
  if (p.steps) {
    const idx = Math.round(v * (p.scale || 1));
    return p.steps[Math.max(0, Math.min(p.steps.length - 1, idx))];
  }
  return v.toFixed(p.decimals);
}

/* ── NEPTR: sensor mapping (demiurge-io) ─────────────────────────────────
 *
 * A second, separate process (demiurge-io) owns the physical sensors. It
 * reads ~/demiurge/sensors.json and hot-reloads on mtime change, and it
 * broadcasts live conditioned readings that this server's SensorIn listener
 * turns into GET /api/neptr/sensors' `live` field. So this page's write path
 * is "edit a config, PUT the whole file" — there is no per-field API, same
 * doctrine as the rest of the app (demiurge-audio owns live.conf, this
 * server is the one writer of sensors.json).
 *
 * The headline feature is one sensor driving several destinations over
 * different slices of its travel — a footswitch's heel doing one thing and
 * its toe another. That only reads clearly as a picture, so the zone editor
 * is a two-handle slider over a 0..1 track, and a sensor's page draws every
 * one of its maps on that same track at once so a gap or an overlap is
 * something you SEE, not something you have to compute from four numbers.
 */

const KNOWN_SENSORS = [
  ["tof1", "LiDAR 1"], ["tof2", "LiDAR 2"], ["exp", "Expression pedal"],
  ["fsr1", "Pressure pad 1"], ["fsr2", "Pressure pad 2"],
  ["ax", "Accel X"], ["ay", "Accel Y"], ["az", "Accel Z"],
  ["jx", "Joystick X"], ["jy", "Joystick Y"],
  ["ml", "Piezo mic L"], ["mr", "Piezo mic R"],
];
const DROPOUT_OPTS = [["hold", "HOLD"], ["zero", "ZERO"]];
const OUTSIDE_OPTS = [["hold", "HOLD"], ["clamp", "CLAMP"]];
const CURVE_OPTS = [["lin", "LIN"], ["exp", "EXP"], ["log", "LOG"],
                    ["scurve", "S-CURVE"]];
const MODE_OPTS = [["continuous", "CONTINUOUS"], ["gate", "GATE"]];
const COMBINE_OPTS = [["replace", "REPLACE"], ["add", "ADD"], ["max", "MAX"],
                      ["min", "MIN"]];

let sensorsData = { sources: [], maps: [], live: {} };
let exprTargets = null;
let sensorEvents = null;

fetch("/api/neptr/sensors").then(r => r.json()).then(d => {
  sensorsData = d; rebuild();
}).catch(() => {});
fetch("/api/neptr/expr-targets").then(r => r.json()).then(t => {
  exprTargets = t; rebuild();
}).catch(() => {});

/* Sensor values now ride their own SSE stream (/api/neptr/sensors/events),
   pushed by demiurge_web.py the moment demiurge-io's OSC broadcast changes
   anything, instead of the old 200 ms GET /api/neptr/sensors poll. That poll
   was 5 Hz — visibly laggy for calibrating a LiDAR by waving a hand at it —
   and it re-fetched the whole sources/maps config on every tick just to read
   a few floats. The stream carries ONLY the live values map; config still
   comes from the one-shot GET above and the PUT save path. Opened when a
   sensor screen is entered, closed on navigateReset() so we never hold a
   stream open on an unrelated page. */
function openSensorEvents() {
  closeSensorEvents();
  sensorEvents = new EventSource("/api/neptr/sensors/events");
  sensorEvents.onmessage = e => {
    sensorsData = Object.assign({}, sensorsData, { live: JSON.parse(e.data) });
    refresh();
  };
}
function closeSensorEvents() {
  if (sensorEvents) { sensorEvents.close(); sensorEvents = null; }
}

function put(path, body) {
  return fetch(path, {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  }).then(async r => {
    const txt = await r.text();
    let data = {};
    try { data = JSON.parse(txt); } catch (e) { /* raw */ }
    if (!r.ok) toast((data && data.error) || (path + " failed (" + r.status + ")"));
    return { ok: r.ok, data };
  }).catch(e => { toast(String(e)); return { ok: false, data: {} }; });
}

/* Every edit — calibration, a new map, a deleted map — writes the WHOLE
   sources+maps file. There is no per-field endpoint, so this is the one
   place that talks to the server; the sensor and map screens just build the
   next version of the arrays and hand it here. */
function saveSensors(cfg) {
  return put("/api/neptr/sensors",
            { sources: cfg.sources, maps: cfg.maps }).then(res => {
    if (res.ok) {
      sensorsData = Object.assign({}, sensorsData,
                                  { sources: cfg.sources, maps: cfg.maps });
      toast("Saved.");
    }
    return res.ok;
  });
}

function sourceById(id) { return sensorsData.sources.find(s => s.id === id); }
function mapsFor(id) { return sensorsData.maps.filter(m => m.source === id); }
function targetLabel(t) {
  if (!t) return "—";
  const found = exprTargets && exprTargets.find(x => x.id === t.id);
  return found ? found.label : "target " + t.id;
}
function genId(prefix) {
  return prefix + "_" + Date.now().toString(36)
       + Math.random().toString(36).slice(2, 6);
}
function sensorsSummary() {
  const n = sensorsData.sources.length, m = sensorsData.maps.length;
  if (!n) return "none configured";
  return n + " sensor" + (n === 1 ? "" : "s") + " · " + m + " map"
       + (m === 1 ? "" : "s");
}

function numField(label, value, onChange, step) {
  const wrap = el("div", "field");
  wrap.appendChild(el("span", "lbl", label));
  const input = el("input");
  input.type = "number";
  input.inputMode = "decimal";
  if (step) input.step = step;
  input.value = value;
  input.addEventListener("input", () => {
    const v = parseFloat(input.value);
    if (!isNaN(v)) onChange(v);
  });
  wrap.appendChild(input);
  return wrap;
}
function textField(label, value, onChange) {
  const wrap = el("div", "field");
  wrap.appendChild(el("span", "lbl", label));
  const input = el("input");
  input.type = "text";
  input.value = value || "";
  input.addEventListener("input", () => onChange(input.value));
  wrap.appendChild(input);
  return wrap;
}
function segField(label, options, value, onChange) {
  const wrap = el("div", "field");
  wrap.appendChild(el("span", "lbl", label));
  const seg = el("div", "seg");
  seg.style.marginTop = "8px";
  for (const [v, text] of options) {
    const b = el("button", v === value ? "on" : "", text);
    b.type = "button";
    b.onclick = () => {
      onChange(v);
      for (const c of seg.children) c.classList.toggle("on", c === b);
    };
    seg.appendChild(b);
  }
  wrap.appendChild(seg);
  return wrap;
}
function toggleRow(label, sub, value, onChange) {
  const r = row(label, { cls: "slot", sub, on: value });
  r.onclick = () => { onChange(!value); r.classList.toggle("on"); value = !value; };
  return r;
}

/* A bottom sheet listing the 35 expression destinations — the same overlay
   mechanism as the wifi password sheet, one more instance of it. */
function pickDestination(onPick) {
  const sheet = $("picksheet"), list = $("pick-list");
  $("pick-msg").textContent = "Destination";
  list.textContent = "";
  if (!exprTargets || !exprTargets.length) {
    list.appendChild(el("div", "note", "Loading the destination list…"));
  }
  for (const t of (exprTargets || [])) {
    list.appendChild(row(t.label, {
      sub: (t.writes || []).join(", "),
      onclick: () => { sheet.classList.add("hidden"); onPick(t); },
    }));
  }
  sheet.classList.remove("hidden");
  $("pick-cancel").onclick = () => sheet.classList.add("hidden");
  sheet.onclick = e => { if (e.target === sheet) sheet.classList.add("hidden"); };
}

/* One sensor's whole travel, drawn once: every map that reads this sensor,
   as a bar spanning its [zone_lo, zone_hi], stacked into lanes so two maps
   covering the same slice sit visibly side by side rather than hiding one
   another. The live marker rides on top, moved by update() only — the bars
   themselves are only ever rebuilt when the map list actually changes. */
function buildTravel(sourceId) {
  const maps = mapsFor(sourceId);
  const wrap = el("div", "travel");
  const lanes = [];
  for (const m of maps.slice().sort((a, b) => a.zone_lo - b.zone_lo)) {
    let lane = lanes.find(l => l.every(x => x.zone_hi <= m.zone_lo
                                           || x.zone_lo >= m.zone_hi));
    if (!lane) { lane = []; lanes.push(lane); }
    lane.push(m);
  }
  const track = el("div", "travel-track");
  track.style.height = (Math.max(1, lanes.length) * 26 + 10) + "px";
  lanes.forEach((lane, li) => {
    lane.forEach(m => {
      const seg = el("div", "travel-seg" + (m.enabled ? "" : " off"),
                    targetLabel(m.target));
      seg.style.left = (m.zone_lo * 100) + "%";
      seg.style.width = Math.max(2, (m.zone_hi - m.zone_lo) * 100) + "%";
      seg.style.top = (li * 26 + 6) + "px";
      seg.style.setProperty("--f", "var(--f"
        + (Math.abs(hashCode(m.id)) % 4) + ")");
      seg.title = targetLabel(m.target) + " · "
        + Math.round(m.zone_lo * 100) + "–" + Math.round(m.zone_hi * 100) + "%";
      seg.onclick = () => go("/neptr/sensors/" + sourceId + "/" + m.id);
      track.appendChild(seg);
    });
  });
  const marker = el("div", "travel-live hidden");
  track.appendChild(marker);
  wrap.appendChild(track);
  if (!maps.length) {
    wrap.appendChild(el("div", "note",
      "No maps yet — this sensor drives nothing. Add one below."));
  }
  return { wrap, marker };
}
function hashCode(s) {
  let h = 0;
  for (let i = 0; i < s.length; i++) h = (h * 31 + s.charCodeAt(i)) | 0;
  return h;
}

/* The zone editor: a two-handle drag over 0..1, the same pointer-capture
   idiom as bindDrag/pressable elsewhere on the page. */
function buildZoneSlider(zoneLo, zoneHi, onChange) {
  const wrap = el("div", "zonepick");
  const track = el("div", "track");
  const fill = el("div", "fill zonefill");
  const hLo = el("div", "handle lo");
  const hHi = el("div", "handle hi");
  track.appendChild(fill); track.appendChild(hLo); track.appendChild(hHi);
  wrap.appendChild(track);
  let lo = zoneLo, hi = zoneHi;
  function paint() {
    fill.style.left = (lo * 100) + "%";
    fill.style.width = Math.max(0, (hi - lo) * 100) + "%";
    hLo.style.left = (lo * 100) + "%";
    hHi.style.left = (hi * 100) + "%";
  }
  paint();
  function posAt(e) {
    const r = track.getBoundingClientRect();
    return Math.max(0, Math.min(1, (e.clientX - r.left) / Math.max(1, r.width)));
  }
  function bindHandle(node, isLo) {
    node.addEventListener("pointerdown", e => {
      node.setPointerCapture(e.pointerId);
      const move = e2 => {
        const p = posAt(e2);
        if (isLo) lo = Math.min(p, hi); else hi = Math.max(p, lo);
        paint();
        onChange(lo, hi);
      };
      const up = () => {
        node.removeEventListener("pointermove", move);
        node.removeEventListener("pointerup", up);
        node.removeEventListener("pointercancel", up);
      };
      node.addEventListener("pointermove", move);
      node.addEventListener("pointerup", up);
      node.addEventListener("pointercancel", up);
      e.preventDefault();
    });
  }
  bindHandle(hLo, true);
  bindHandle(hHi, false);
  return { wrap };
}

function addSensor(id, label) {
  if (sourceById(id)) { go("/neptr/sensors/" + id); return; }
  const src = { id, label, units: "", in_lo: 0, in_hi: 1, dropout: "hold",
               deadzone: 0.02, smooth_ms: 20, enabled: true };
  saveSensors({ sources: sensorsData.sources.concat([src]),
               maps: sensorsData.maps })
    .then(ok => { if (ok) go("/neptr/sensors/" + id); });
}

/* Sensors index: every configured sensor with a live mini-meter, a
   calibration summary, and how many maps it drives; below that, whichever
   of the 12 known physical inputs have not been given a source yet. */
route(["neptr", "sensors"], "Sensors", () => {
  let list;
  return {
    build(box) {
      openSensorEvents();
      list = el("div"); box.appendChild(list);
    },
    update() {
      list.textContent = "";
      list.appendChild(el("div", "note",
        "Live values are demiurge-io's own broadcast. Calibration and "
        + "mapping changes take effect the moment they're saved — "
        + "demiurge-io hot-reloads the file, no restart."));
      const configured = sensorsData.sources || [];
      const cSec = el("section");
      cSec.appendChild(el("div", "lbl", "Configured"));
      const cwrap = el("div"); cwrap.style.marginTop = "10px";
      if (!configured.length) {
        cwrap.appendChild(el("div", "note", "No sensors configured yet — "
          + "add one below."));
      }
      for (const s of configured) {
        const v = sensorsData.live ? sensorsData.live[s.id] : null;
        const n = mapsFor(s.id).length;
        const bar = el("div", "bar mini");
        const fill = el("i");
        if (v != null) fill.style.width = (Math.max(0, Math.min(1, v)) * 100) + "%";
        bar.appendChild(fill);
        cwrap.appendChild(row(s.label, {
          lead: bar, chev: true, cls: s.enabled ? "" : "empty",
          sub: (s.units ? s.in_lo + "–" + s.in_hi + " " + s.units + " · " : "")
             + n + " map" + (n === 1 ? "" : "s"),
          onclick: () => go("/neptr/sensors/" + s.id),
        }));
      }
      cSec.appendChild(cwrap);
      list.appendChild(cSec);

      const known = KNOWN_SENSORS.filter(([id]) => !sourceById(id));
      if (known.length) {
        const aSec = el("section");
        aSec.appendChild(el("div", "lbl", "Add a sensor"));
        const awrap = el("div"); awrap.style.marginTop = "10px";
        for (const [id, label] of known) {
          awrap.appendChild(row(label, {
            sub: id, tag_: "+ add",
            onclick: () => addSensor(id, label),
          }));
        }
        aSec.appendChild(awrap);
        list.appendChild(aSec);
      }
    },
  };
});

/* A sensor's page: calibration, its travel picture, and the list of maps
   reading it. Calibration inputs are bound to a local draft object built
   ONCE in build() — the ~20 Hz sensor-events push must not blow away a
   field the user is mid-way through typing, so update() only ever touches
   the meter and the live marker, never the form. */
route(["neptr", "sensors", ":id"], "", sid => {
  let meterBox, marker;
  return {
    title: () => (sourceById(sid) || {}).label || sid,
    build(box) {
      openSensorEvents();
      const existing = sourceById(sid);
      const known = KNOWN_SENSORS.find(k => k[0] === sid);
      const draft = existing ? Object.assign({}, existing) : {
        id: sid, label: known ? known[1] : sid, units: "", in_lo: 0, in_hi: 1,
        dropout: "hold", deadzone: 0.02, smooth_ms: 20, enabled: true,
      };
      setTitle(draft.label);

      meterBox = el("div"); box.appendChild(meterBox);

      const cal = el("section");
      cal.appendChild(el("div", "lbl", "Calibration"));
      const fields = el("div"); fields.style.marginTop = "10px";
      fields.appendChild(textField("Label", draft.label, v => draft.label = v));
      fields.appendChild(numField("Range low", draft.in_lo, v => draft.in_lo = v, 0.1));
      fields.appendChild(numField("Range high", draft.in_hi, v => draft.in_hi = v, 0.1));
      fields.appendChild(textField("Units", draft.units, v => draft.units = v));
      fields.appendChild(segField("Dropout handling", DROPOUT_OPTS, draft.dropout,
                                  v => draft.dropout = v));
      fields.appendChild(numField("Deadzone (0–1)", draft.deadzone,
                                  v => draft.deadzone = v, 0.01));
      fields.appendChild(numField("Smoothing (ms)", draft.smooth_ms,
                                  v => draft.smooth_ms = v, 1));
      fields.appendChild(toggleRow("Enabled", "Reporting to demiurge-io",
                                   draft.enabled, v => draft.enabled = v));
      cal.appendChild(fields);

      const saveRow = el("div", "btnrow");
      const save = el("button", "btn", "Save calibration");
      save.onclick = () => {
        const sources = sensorsData.sources.filter(s => s.id !== sid).concat([draft]);
        saveSensors({ sources, maps: sensorsData.maps })
          .then(ok => { if (ok) rebuild(); });
      };
      saveRow.appendChild(save);
      if (existing) {
        const del = el("button", "btn danger", "Remove sensor");
        armButton(del, "Confirm remove", () => {
          const sources = sensorsData.sources.filter(s => s.id !== sid);
          const maps = sensorsData.maps.filter(m => m.source !== sid);
          saveSensors({ sources, maps }).then(ok => { if (ok) go("/neptr/sensors"); });
        });
        saveRow.appendChild(del);
      }
      cal.appendChild(saveRow);
      box.appendChild(cal);

      const trav = el("section");
      const th = el("div", "sechead");
      th.appendChild(el("span", "lbl", "Travel & maps"));
      const addBtn = el("button", null, "+ ADD MAP");
      addBtn.type = "button";
      addBtn.style.cssText = "background:none;border:0;color:var(--accent);"
        + "font:700 11px/1 system-ui,sans-serif;letter-spacing:.08em;cursor:pointer";
      addBtn.onclick = () => go("/neptr/sensors/" + sid + "/new");
      th.appendChild(addBtn);
      trav.appendChild(th);
      trav.appendChild(el("div", "note",
        "Every map that reads this sensor, laid out over its 0–1 travel — "
        + "a gap is a dead zone, a stack is several maps sharing one slice."));
      const built = buildTravel(sid);
      marker = built.marker;
      trav.appendChild(built.wrap);
      box.appendChild(trav);

      const mapsList = el("div"); mapsList.style.marginTop = "14px";
      const own = mapsFor(sid);
      if (own.length) {
        for (const m of own) {
          mapsList.appendChild(row(targetLabel(m.target), {
            sub: Math.round(m.zone_lo * 100) + "–" + Math.round(m.zone_hi * 100)
               + "% · " + m.curve + (m.invert ? " · inverted" : "")
               + (m.enabled ? "" : " · off"),
            chev: true, cls: m.enabled ? "" : "empty",
            onclick: () => go("/neptr/sensors/" + sid + "/" + m.id),
          }));
        }
        box.appendChild(mapsList);
      }
    },
    update() {
      meterBox.textContent = "";
      const v = sensorsData.live ? sensorsData.live[sid] : null;
      meterBox.appendChild(meter("Live", v, x => Math.round(x * 100) + "%",
                                 x => x * 100));
      if (marker) {
        if (v == null) marker.classList.add("hidden");
        else {
          marker.classList.remove("hidden");
          marker.style.left = (Math.max(0, Math.min(1, v)) * 100) + "%";
        }
      }
    },
  };
});

/* One map: which destination, which slice of the sensor's travel, and how
   that slice's 0..1 becomes the destination's own range. */
route(["neptr", "sensors", ":id", ":mapId"], "", (sid, mapId) => {
  let isNew, draft, targetRow;
  return {
    title: () => isNew ? "New map" : targetLabel(draft.target),
    build(box) {
      isNew = mapId === "new";
      const existing = isNew ? null
        : sensorsData.maps.find(m => m.id === mapId && m.source === sid);
      if (!isNew && !existing) { go("/neptr/sensors/" + sid); return; }
      draft = existing ? JSON.parse(JSON.stringify(existing)) : {
        id: genId("m"), source: sid, enabled: true, zone_lo: 0, zone_hi: 1,
        outside: "hold", invert: false, curve: "lin", out_lo: 0, out_hi: 1,
        mode: "continuous", combine: "replace", slew_ms: 30,
        target: { sink: "csound", kind: "expr",
                 id: (exprTargets && exprTargets[0]) ? exprTargets[0].id : 0 },
      };
      setTitle(isNew ? "New map" : targetLabel(draft.target));

      targetRow = row(targetLabel(draft.target), {
        sub: "Destination — tap to change", chev: true,
      });
      targetRow.onclick = () => pickDestination(t => {
        draft.target = { sink: "csound", kind: "expr", id: t.id };
        targetRow.querySelector(".name").firstChild.textContent = t.label;
        setTitle(t.label);
      });
      box.appendChild(targetRow);

      const zsec = el("section");
      zsec.appendChild(el("div", "lbl",
        "Zone — the slice of the sensor's travel that drives this"));
      const zwrap = el("div"); zwrap.style.marginTop = "10px";
      const zlabel = el("div", "note mono",
        Math.round(draft.zone_lo * 100) + "–" + Math.round(draft.zone_hi * 100) + "%");
      const zoneCtl = buildZoneSlider(draft.zone_lo, draft.zone_hi, (lo, hi) => {
        draft.zone_lo = lo; draft.zone_hi = hi;
        zlabel.textContent = Math.round(lo * 100) + "–" + Math.round(hi * 100) + "%";
      });
      zwrap.appendChild(zoneCtl.wrap);
      zwrap.appendChild(zlabel);
      zsec.appendChild(zwrap);
      zsec.appendChild(segField("Outside the zone", OUTSIDE_OPTS, draft.outside,
                                v => draft.outside = v));
      box.appendChild(zsec);

      const sec2 = el("section");
      sec2.appendChild(el("div", "lbl", "Shape"));
      const s2 = el("div"); s2.style.marginTop = "10px";
      s2.appendChild(segField("Curve", CURVE_OPTS, draft.curve, v => draft.curve = v));
      s2.appendChild(toggleRow("Invert", "Flip which end of the zone reads as 0",
                               draft.invert, v => draft.invert = v));
      s2.appendChild(numField("Output low", draft.out_lo, v => draft.out_lo = v, 0.01));
      s2.appendChild(numField("Output high", draft.out_hi, v => draft.out_hi = v, 0.01));
      s2.appendChild(segField("Mode", MODE_OPTS, draft.mode, v => draft.mode = v));
      s2.appendChild(segField("Combine with other maps on this destination",
                              COMBINE_OPTS, draft.combine, v => draft.combine = v));
      s2.appendChild(numField("Slew (ms)", draft.slew_ms, v => draft.slew_ms = v, 1));
      s2.appendChild(toggleRow("Enabled", "", draft.enabled, v => draft.enabled = v));
      sec2.appendChild(s2);
      box.appendChild(sec2);

      const btnrow = el("div", "btnrow");
      const save = el("button", "btn", isNew ? "Add map" : "Save map");
      save.onclick = () => {
        const others = sensorsData.maps.filter(m => m.id !== draft.id);
        saveSensors({ sources: sensorsData.sources, maps: others.concat([draft]) })
          .then(ok => { if (ok) go("/neptr/sensors/" + sid); });
      };
      btnrow.appendChild(save);
      if (!isNew) {
        const del = el("button", "btn danger", "Delete map");
        armButton(del, "Confirm delete", () => {
          const maps = sensorsData.maps.filter(m => m.id !== draft.id);
          saveSensors({ sources: sensorsData.sources, maps })
            .then(ok => { if (ok) go("/neptr/sensors/" + sid); });
        });
        btnrow.appendChild(del);
      }
      box.appendChild(btnrow);
    },
    update() {},
  };
});

/* ── NEPTR: virtual hardware panel ───────────────────────────────────────
 *
 * A bare Pi has no Teensy — no encoders, no buttons, no LiDAR, no expression
 * pedal — so there is nothing to plug a phone's touches into except this
 * page. It speaks the two protocols the real hardware speaks, not an
 * invented shortcut:
 *
 *   controls -> POST /api/neptr/sim/cc, which lands as a raw OSC "/cc" on
 *   the engine's control-input port. The physical CC numbers are the
 *   Teensy's own (TEENSYY.ino's ENC_CC/BTN_CC/ENCBTN_CC/JOY_BTN_CC), on
 *   channel 1, one message per click — never the model's logical numbers.
 *
 *   sensors -> POST /api/neptr/sim/sensor with a RAW reading in the
 *   sensor's own native units (mm, ADC counts, raw int16…), which the
 *   server forwards verbatim to demiurge-io. Calibration and mapping still
 *   happen for real; this only replaces the transducer.
 *
 * The page is a DRAWING of the instrument, not a list of its controls: the
 * cast resin shell with its blue rim glow, the seven brass encoders where
 * they actually sit, the d-pad and face buttons either side of the recessed
 * LCD, the LiDAR ports, the stick, the two pressure pads on their ribbons, and
 * the expression pedal off to the side. Muscle memory built here is muscle
 * memory for the real panel — which is the only reason to reach for a phone
 * at all when the hardware is missing. See buildInstrument() below.
 */

const SIM_CH = 1;
const SIM_ENC_CC = [20, 21, 22, 23, 24, 25, 26];
const SIM_ENCBTN_CC = [70, 71, 72, 73, 74, 75, 76];
const SIM_BTN_CC = [60, 61, 62, 63, 64, 65, 66, 67, 68];
const SIM_JOYBTN_CC = 77;

const SIM_SENSORS = [
  { id: "tof1", label: "LiDAR 1", lo: 20, hi: 460, unit: "mm", dropout: true, start: 240 },
  { id: "tof2", label: "LiDAR 2", lo: 20, hi: 460, unit: "mm", dropout: true, start: 240 },
  { id: "exp", label: "Expression pedal", lo: 128, hi: 3992, unit: "", start: 128 },
  { id: "fsr1", label: "Pressure pad 1", lo: 2500, hi: 4000, unit: "", start: 2500 },
  { id: "fsr2", label: "Pressure pad 2", lo: 2500, hi: 4000, unit: "", start: 2500 },
  { id: "jx", label: "Joystick X", lo: 0, hi: 4095, unit: "", start: 2048 },
  { id: "jy", label: "Joystick Y", lo: 0, hi: 4095, unit: "", start: 2048 },
  { id: "ax", label: "Accel X", lo: -17000, hi: 17000, unit: "", start: 0 },
  { id: "ay", label: "Accel Y", lo: -17000, hi: 17000, unit: "", start: 0 },
  { id: "az", label: "Accel Z", lo: -17000, hi: 17000, unit: "", start: 0 },
  { id: "ml", label: "Piezo mic L", lo: 0, hi: 800, unit: "", start: 0 },
  { id: "mr", label: "Piezo mic R", lo: 0, hi: 800, unit: "", start: 0 },
];

/* The sensors with nothing to touch on the shell — they are inside the box,
   so the drawing has no control for them and they keep the plain faders. */
const SIM_INTERNAL = new Set(["ax", "ay", "az", "ml", "mr"]);

function simCc(cc, val) { post("/api/neptr/sim/cc", { ch: SIM_CH, cc, val }); }
function simSensor(id, raw) {
  return post("/api/neptr/sim/sensor", { id, raw }, true);
}

/* Press-and-hold that keeps firing — the "sweep a parameter" gesture a real
   encoder gives for free by being turned continuously. First fire is
   immediate (a tap must not wait out the interval), then it repeats until
   release. */
function repeatable(node, fire) {
  let timer = null;
  const stop = () => {
    if (timer === null) return;
    clearInterval(timer); timer = null;
    node.classList.remove("preview");
  };
  node.addEventListener("pointerdown", e => {
    stop();
    grabAndRelease(node, e, stop);
    node.classList.add("preview");
    fire();
    timer = setInterval(fire, 130);
  });
}

/* A momentary control: down sends the press value, up sends the release
   value — the same finger-down/finger-up shape as bindHold(), generalised
   for the sim's own press/release pairs (encoder buttons, panel buttons,
   the joystick button). */
function pressRelease(node, down, up) {
  let held = false;
  const end = () => {
    if (!held) return;
    held = false;
    node.classList.remove("preview");
    up();
  };
  node.addEventListener("pointerdown", e => {
    end();
    held = true;
    grabAndRelease(node, e, end);
    node.classList.add("preview");
    down();
  });
}

/* Capture the pointer, and make ABSOLUTELY sure the release lands.
 *
 * A control that misses its pointerup is not a cosmetic bug here: a held
 * encoder keeps firing detents into the engine forever, and a held toggle
 * leaves CC 6x stuck at 127. Capture alone is not enough — capture can be
 * lost to a gesture, a context menu, a window blur or a page that scrolls
 * out from under the finger — so the release is also armed on the window and
 * on lostpointercapture, all one-shot so nothing accumulates across screens.
 */
function grabAndRelease(node, e, release) {
  try { node.setPointerCapture(e.pointerId); } catch (err) { /* no capture */ }
  const once = { once: true };
  node.addEventListener("pointerup", release, once);
  node.addEventListener("pointercancel", release, once);
  node.addEventListener("lostpointercapture", release, once);
  window.addEventListener("pointerup", release, once);
  window.addEventListener("pointercancel", release, once);
  window.addEventListener("blur", release, once);
}

/* The keyboard's version of the same momentary gesture. A held key repeats
   its keydown, so the press edge is latched — a toggle must not machine-gun
   127s at the engine just because a thumb stayed on the space bar — and a
   blur while held releases, so focus never escapes with a control stuck
   down. */
function keyPress(node, down, up) {
  let held = false;
  const release = () => { if (!held) return; held = false; node.classList.remove("preview"); if (up) up(); };
  node.addEventListener("keydown", e => {
    if (e.key !== "Enter" && e.key !== " ") return;
    e.preventDefault();
    if (held) return;
    held = true; node.classList.add("preview"); down();
  });
  node.addEventListener("keyup", e => {
    if (e.key === "Enter" || e.key === " ") release();
  });
  node.addEventListener("blur", release);
}

/* Arrow keys on a continuous control. Shift takes a coarse step, so a
   0..4095 ADC range is reachable without a hundred key presses. */
function keyAdjust(node, step, apply) {
  node.addEventListener("keydown", e => {
    let d = 0;
    if (e.key === "ArrowRight" || e.key === "ArrowUp") d = 1;
    else if (e.key === "ArrowLeft" || e.key === "ArrowDown") d = -1;
    else return;
    e.preventDefault();
    apply(d * (e.shiftKey ? step * 5 : step));
  });
}

/* Rate-limit a stream of sensor writes the way bindDrag() does, but for the
   controls that move in two axes at once (the joystick) or that are dragged
   over the drawing rather than over a fader track. The first call goes
   straight out and the last one always lands. */
function throttled(rate, fn) {
  let pending = null, timer = null;
  const flush = () => {
    timer = null;
    if (!pending) return;
    const args = pending; pending = null;
    fn.apply(null, args);
    timer = setTimeout(flush, rate);
  };
  return function () { pending = arguments; if (timer === null) flush(); };
}

function fmtSimVal(spec, raw) {
  return Math.round(raw) + (spec.unit ? " " + spec.unit : "");
}

/* One sensor's slider: the same bindDrag() drag loop the parameter faders
   and the expression pedal already use, but the track's ends are the
   sensor's RAW native-unit range, not 0..1 — dragging the middle of a
   LiDAR's slider should read close to 240mm, not "50%".
   Used now only for the sensors with no control on the shell — the
   accelerometer and the piezo mics are inside the box, so there is nothing
   to draw for them. Those live in a narrow column beside the instrument, so
   they are built compact: same drag loop, less furniture. */
function buildSimSensor(spec, colorIdx, compact) {
  const wrap = el("div", compact ? "fader compact" : "fader");
  wrap.style.setProperty("--f", "var(--f" + (colorIdx % 4) + ")");
  const head = el("div", "head");
  head.appendChild(el("span", "lbl", spec.label));
  const val = el("span", "val", fmtSimVal(spec, spec.start));
  head.appendChild(val);
  wrap.appendChild(head);

  const track = el("div", "track");
  const fill = el("div", "fill");
  track.appendChild(fill);
  wrap.appendChild(track);
  const frac0 = (spec.start - spec.lo) / (spec.hi - spec.lo);
  fill.style.width = (frac0 * 100) + "%";

  bindDrag(track, wrap, frac => {
    const raw = Math.round(spec.lo + (spec.hi - spec.lo) * frac);
    fill.style.width = (frac * 100) + "%";
    val.textContent = fmtSimVal(spec, raw);
    return { id: spec.id, raw };
  }, body => simSensor(body.id, body.raw));

  if (spec.dropout) {
    const drop = el("button", "dropout-btn", "HOLD FOR DROPOUT (raw −1)");
    drop.type = "button";
    drop.onclick = () => {
      val.textContent = "dropout";
      fill.style.width = "0%";
      simSensor(spec.id, -1);
    };
    wrap.appendChild(drop);
  }
  return wrap;
}

/* ── the instrument, drawn ────────────────────────────────────────────────
 *
 * Everything below is one picture of the real NEPTR: a cast resin slab lit
 * from its rim, seven brass encoders, a d-pad and a face-button diamond
 * either side of a recessed LCD with the big ARCADE button under it,
 * two LiDAR ports at the top, a joystick, two pressure pads on
 * ribbons, and the expression pedal sitting off to the side where it sits on
 * the floor. It is inline SVG — no assets, no libraries — because the whole
 * point is that a phone loads it over a hotspot next to a rig with no
 * internet.
 *
 * The drawing is scenery; the CONTROL is a transparent hit shape laid over
 * it (class "hit"), which is what takes the pointer, the tab stop and the
 * focus ring. That split is deliberate: the artwork can be re-shaped freely
 * without touching a single line of the wire protocol underneath, and the
 * wire protocol is exactly what it was — the Teensy's own CCs on channel 1,
 * raw native-unit sensor readings, nothing invented.
 */

const SVGNS = "http://www.w3.org/2000/svg";

/* The instrument's own palette, read off the physical unit. Mirrored as CSS
   custom properties in style.css for the parts drawn in HTML; kept literal
   here so the SVG is self-contained. */
const NP = {
  resin: "#4B3A9E", resinDeep: "#2A1F63", glow: "#1E5BFF",
  brass: "#C9A227", brassHi: "#F2DE9B", brassLo: "#7A5F12",
  steel: "#C6CBD4", lcdBg: "#061033", lcdBar: "#2E6BFF",
};

function sv(tag, attrs, parent) {
  const n = document.createElementNS(SVGNS, tag);
  for (const k in attrs || {}) n.setAttribute(k, attrs[k]);
  if (parent) parent.appendChild(n);
  return n;
}

function svText(parent, x, y, text, cls) {
  const t = sv("text", { x, y, class: cls || "np-lbl" }, parent);
  t.textContent = text;
  return t;
}

/* A hit shape that is a range rather than a button: role="slider" is only
   honest with the numbers attached, so they are set up front and kept
   current by the control's own setter. */
function svSlider(node, lo, hi, now) {
  node.setAttribute("role", "slider");
  node.setAttribute("aria-valuemin", lo);
  node.setAttribute("aria-valuemax", hi);
  node.setAttribute("aria-valuenow", now);
  return node;
}

/* An invisible, focusable, pointer-taking shape over the artwork. */
function svHit(parent, tag, attrs, label) {
  const n = sv(tag, attrs, parent);
  n.setAttribute("class", "hit");
  n.setAttribute("fill", "transparent");
  n.setAttribute("tabindex", "0");
  n.setAttribute("role", "button");
  n.setAttribute("aria-label", label);
  return n;
}

/* Half of an annulus — the generous left/right thumb targets on a knob,
   shaped like the part of the knob you would actually grab. */
function npWedge(cx, cy, R, ri, dir) {
  const a = dir > 0 ? 1 : 0, b = dir > 0 ? 0 : 1;
  return "M " + cx + " " + (cy - R) + " A " + R + " " + R + " 0 0 " + a + " "
       + cx + " " + (cy + R) + " L " + cx + " " + (cy + ri) + " A " + ri + " "
       + ri + " 0 0 " + b + " " + cx + " " + (cy - ri) + " Z";
}

/* The shell outline: wider than tall, shield-ish, asymmetric on purpose —
   the real one was cast, not milled. */
const NP_BODY =
  "M 500 30 C 668 30 806 38 884 80 C 954 118 976 200 966 296 "
  + "C 958 384 944 458 910 542 C 878 622 796 668 672 674 "
  + "C 596 678 542 672 500 672 C 458 672 396 680 316 672 "
  + "C 196 662 118 614 88 534 C 56 448 42 378 36 292 "
  + "C 30 198 54 114 124 78 C 204 34 340 30 500 30 Z";

const NP_DEFS =
  '<defs>'
  + '<radialGradient id="npResin" cx="50%" cy="34%" r="78%">'
  + '<stop offset="0" stop-color="#5744B6"/><stop offset=".5" stop-color="#4B3A9E"/>'
  + '<stop offset=".82" stop-color="#2A1F63"/><stop offset="1" stop-color="#1B1447"/>'
  + '</radialGradient>'
  + '<radialGradient id="npBrass" cx="34%" cy="26%" r="82%">'
  + '<stop offset="0" stop-color="#F2DE9B"/><stop offset=".34" stop-color="#E3C864"/>'
  + '<stop offset=".68" stop-color="#C9A227"/><stop offset="1" stop-color="#7A5F12"/>'
  + '</radialGradient>'
  + '<linearGradient id="npBrassRim" x1="0" y1="0" x2="0" y2="1">'
  + '<stop offset="0" stop-color="#F2DE9B"/><stop offset="1" stop-color="#6B520D"/>'
  + '</linearGradient>'
  + '<radialGradient id="npCap" cx="38%" cy="30%" r="76%">'
  + '<stop offset="0" stop-color="#EBD48F"/><stop offset="1" stop-color="#8A6C15"/>'
  + '</radialGradient>'
  + '<radialGradient id="npSpec" cx="50%" cy="50%" r="50%">'
  + '<stop offset="0" stop-color="#fff" stop-opacity=".7"/>'
  + '<stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient>'
  + '<radialGradient id="npDome" cx="36%" cy="26%" r="80%">'
  + '<stop offset="0" stop-color="#4C4C58"/><stop offset=".6" stop-color="#1A1A22"/>'
  + '<stop offset="1" stop-color="#08080C"/></radialGradient>'
  + '<linearGradient id="npSteel" x1="0" y1="0" x2="0" y2="1">'
  + '<stop offset="0" stop-color="#EEF2F7"/><stop offset=".45" stop-color="#C6CBD4"/>'
  + '<stop offset="1" stop-color="#7C838F"/></linearGradient>'
  + '<linearGradient id="npPad" x1="0" y1="0" x2="0" y2="1">'
  + '<stop offset="0" stop-color="#FCFCFE"/><stop offset="1" stop-color="#B4BAC5"/>'
  + '</linearGradient>'
  + '<linearGradient id="npTread" x1="0" y1="0" x2="0" y2="1">'
  + '<stop offset="0" stop-color="#525661"/><stop offset="1" stop-color="#171A20"/>'
  + '</linearGradient>'
  + '<linearGradient id="npGlass" x1="0" y1="0" x2="0" y2="1">'
  + '<stop offset="0" stop-color="#9DC0FF" stop-opacity=".14"/>'
  + '<stop offset=".5" stop-color="#9DC0FF" stop-opacity="0"/></linearGradient>'
  + '<filter id="npGlow" x="-25%" y="-25%" width="150%" height="150%">'
  + '<feGaussianBlur stdDeviation="10" result="b"/>'
  + '<feMerge><feMergeNode in="b"/><feMergeNode in="b"/></feMerge></filter>'
  + '<filter id="npGlowFar" x="-30%" y="-30%" width="160%" height="160%">'
  + '<feGaussianBlur stdDeviation="20"/></filter>'
  + '<filter id="npSoft" x="-40%" y="-40%" width="180%" height="180%">'
  + '<feGaussianBlur stdDeviation="14"/></filter>'
  + '<filter id="npShadow" x="-40%" y="-40%" width="180%" height="180%">'
  + '<feDropShadow dx="0" dy="5" stdDeviation="5" flood-color="#000" flood-opacity=".55"/>'
  + '</filter>'
  + '<clipPath id="npClip"><path d="' + NP_BODY + '"/></clipPath>'
  + '</defs>';

/* The resin itself: rim glow outside, body fill, then everything that only
   exists INSIDE the casting — the blue bleed off the LED strip, the cloudy
   marbling, and the print striations along the bottom edge — clipped to the
   outline so nothing leaks past the shell. */
function npShell() {
  let striae = "";
  for (let y = 592; y <= 664; y += 8) {
    const o = (0.05 + (y - 592) / 72 * 0.13).toFixed(3);
    striae += '<path d="M0 ' + y + ' Q 500 ' + (y + 7) + ' 1000 ' + y
            + '" fill="none" stroke="#9DC0FF" stroke-width="2" opacity="' + o + '"/>';
  }
  return NP_DEFS
    + '<g class="np-glow">'
    + '<path d="' + NP_BODY + '" stroke="' + NP.glow
    + '" fill="none" stroke-width="16" opacity=".42" filter="url(#npGlowFar)"/>'
    + '<path d="' + NP_BODY + '" fill="none" stroke="#4E86FF" stroke-width="7"'
    + ' opacity=".7" filter="url(#npGlow)"/></g>'
    + '<path d="' + NP_BODY + '" fill="url(#npResin)"/>'
    + '<g clip-path="url(#npClip)">'
    + '<path d="' + NP_BODY + '" fill="none" stroke="#2E6BFF" stroke-width="30"'
    + ' opacity=".5" filter="url(#npSoft)"/>'
    + '<path d="' + NP_BODY + '" fill="none" stroke="#A8C7FF" stroke-width="6"'
    + ' opacity=".5" filter="url(#npGlow)"/>'
    + '<ellipse cx="330" cy="210" rx="190" ry="112" fill="#8E7BE8" opacity=".12"'
    + ' filter="url(#npSoft)"/>'
    + '<ellipse cx="700" cy="430" rx="210" ry="130" fill="#3A2A86" opacity=".33"'
    + ' filter="url(#npSoft)"/>'
    + '<ellipse cx="520" cy="640" rx="300" ry="80" fill="#1B1447" opacity=".45"'
    + ' filter="url(#npSoft)"/>'
    + striae
    + '</g>'
    + '<path d="' + NP_BODY + '" fill="none" stroke="#8FB4FF" stroke-width="2.5"'
    + ' opacity=".8"/>';
}

/* Where every control sits on the shell, in viewBox units. One table, so the
   layout can be re-read at a glance and moved without hunting through the
   builders. Encoder order is the physical one: 1 and 2 are the top corners,
   3..7 the bottom row, left to right. */
const NP_ENC = [
  { x: 205, y: 166, r: 32 }, { x: 795, y: 166, r: 32 },
  { x: 208, y: 552, r: 28 }, { x: 354, y: 552, r: 28 },
  { x: 500, y: 552, r: 28 }, { x: 646, y: 552, r: 28 },
  { x: 792, y: 552, r: 28 },
];
/* The nine panel buttons, laid out the way the canonical button map draws
   them (demiurge/games/docs/BUTTONS.md): a 4-way D-PAD diamond on the left,
   a face-button diamond on the right, and the big ARCADE button centred
   under the screen. They are momentary pushbuttons, not switches — `b` is
   the firmware name b1..b9, so the wire CC is SIM_BTN_CC[b - 1] = 59 + b.

            b4 UP                                  b9 A
       b5 LEFT   b3 RIGHT        b1 ARCADE     b6 X     b8 B
            b2 DOWN                                  b7 Y
*/
const NP_BTN_R = 17;
const NP_BTN = [
  // left cluster — the d-pad, centred on 250,340
  { b: 4, role: "UP",     x: 250, y: 282, r: NP_BTN_R },
  { b: 5, role: "LEFT",   x: 188, y: 340, r: NP_BTN_R },
  { b: 3, role: "RIGHT",  x: 312, y: 340, r: NP_BTN_R },
  { b: 2, role: "DOWN",   x: 250, y: 398, r: NP_BTN_R },
  // right cluster — the face buttons, centred on 750,340
  { b: 9, role: "A",      x: 750, y: 282, r: NP_BTN_R },
  { b: 6, role: "X",      x: 688, y: 340, r: NP_BTN_R },
  { b: 8, role: "B",      x: 812, y: 340, r: NP_BTN_R },
  { b: 7, role: "Y",      x: 750, y: 398, r: NP_BTN_R },
  // The big one. It sits upper-RIGHT, where the panel actually carries it —
  // opposite the stick. The small JOY BTN moved above the stick.
  { b: 1, role: "ARCADE", x: 694, y: 244, r: 27 },
];
const NP_SCR = { x: 382, y: 218, w: 236, h: 164 };

function buildInstrument() {
  /* Local mirror of what this page has told demiurge-io. The engine sends no
     echo for a simulated transducer, so this is honestly the phone's own
     copy — same rule as the expression fader elsewhere on the page. */
  const vals = {};
  for (const s of SIM_SENSORS) vals[s.id] = s.start;
  const spec = {};
  for (const s of SIM_SENSORS) spec[s.id] = s;

  const wrap = el("div", "np-instrument");
  const stage = el("div", "np-stage");
  wrap.appendChild(stage);

  const readout = el("div", "np-readout");
  wrap.appendChild(readout);
  const status = el("div", "np-status mono", "Ready — nothing sent yet.");
  status.setAttribute("aria-live", "polite");
  wrap.appendChild(status);

  /* Drawing the panel must not TALK to demiurge-io: the builders set every
     control to its rest position on the way past, and a screen that opened
     by writing seven sensors would move the rig just by being looked at. */
  let booting = true;
  const senders = {};
  function push(id, raw) {
    if (!senders[id]) senders[id] = throttled(60, r => simSensor(id, r));
    senders[id](raw);
  }
  const chips = {};
  const bars = {};
  function paint(id) {
    const s = spec[id], v = vals[id];
    if (chips[id]) chips[id].textContent = v < 0 ? "dropout" : fmtSimVal(s, v);
    const b = bars[id];
    if (b) {
      const f = v < 0 ? 0 : Math.max(0, Math.min(1, (v - s.lo) / (s.hi - s.lo)));
      b.rect.setAttribute("height", Math.max(2, f * b.max));
      b.rect.setAttribute("y", b.base - Math.max(2, f * b.max));
      b.val.textContent = v < 0 ? "--" : Math.round(v);
    }
  }
  /* One write path for every sensor on the shell: remember it, redraw it,
     say what happened, send it. */
  function setSensor(id, raw, note) {
    vals[id] = raw;
    paint(id);
    if (booting) return;
    push(id, raw);
    if (note) status.textContent = note;
  }
  function say(msg) { status.textContent = msg; }

  /* ── the shell ───────────────────────────────────────────────────────── */

  const body = sv("svg", {
    viewBox: "0 0 1000 700", class: "np-svg np-body",
    role: "group", "aria-label": "NEPTR panel",
  });
  body.innerHTML = npShell();
  stage.appendChild(body);

  /* ── LCD ─────────────────────────────────────────────────────────────── */

  const scr = sv("g", null, body);
  const S = NP_SCR;
  sv("rect", { x: S.x - 13, y: S.y - 13, width: S.w + 26, height: S.h + 26,
               rx: 20, fill: "#0B0922", stroke: "#1A1640", "stroke-width": 3,
               filter: "url(#npShadow)" }, scr);
  sv("rect", { x: S.x - 5, y: S.y - 5, width: S.w + 10, height: S.h + 10,
               rx: 12, fill: "#04030E" }, scr);
  sv("rect", { x: S.x, y: S.y, width: S.w, height: S.h, rx: 7,
               fill: NP.lcdBg }, scr);
  const scrMenu = svText(scr, S.x + 14, S.y + 30, DASH, "np-scr-menu");
  const scrArm = svText(scr, S.x + 14, S.y + 50, "no effect armed", "np-scr-sub");

  const BAR_IDS = ["tof1", "tof2", "fsr1", "fsr2", "jx", "jy", "exp"];
  const BAR_TXT = ["TOF1", "TOF2", "FSR1", "FSR2", "JX", "JY", "EXP"];
  /* Derived from S, never hardcoded: the bar row has to fit INSIDE the screen
     at whatever size the shell gives it. Fixed pitch/width silently overflowed
     the glass when the screen shrank, and the overhang read as a second panel
     bolted to the side of the LCD. Bottom margin leaves room for the label and
     value under each bar. */
  const bPad = 12, bPitch = (S.w - bPad * 2) / BAR_IDS.length;
  const bW = Math.round(bPitch * 0.56);
  const bBase = S.y + S.h - 36, bTop = S.y + 58, bMax = bBase - bTop;
  BAR_IDS.forEach((id, i) => {
    const bx = Math.round(S.x + bPad + i * bPitch + (bPitch - bW) / 2);
    sv("rect", { x: bx, y: bTop, width: bW, height: bMax, rx: 3,
                 fill: "#0D1940", opacity: ".9" }, scr);
    const r = sv("rect", { x: bx, y: bBase - 2, width: bW, height: 2, rx: 3,
                           fill: NP.lcdBar, class: "np-bar" }, scr);
    svText(scr, bx + bW / 2, bBase + 16, BAR_TXT[i], "np-scr-lbl");
    const v = svText(scr, bx + bW / 2, bBase + 30, "0", "np-scr-val");
    bars[id] = { rect: r, val: v, base: bBase, max: bMax };
  });
  sv("rect", { x: S.x, y: S.y, width: S.w, height: S.h, rx: 7,
               fill: "url(#npGlass)", "pointer-events": "none" }, scr);

  /* ── brass encoders ──────────────────────────────────────────────────── */

  NP_ENC.forEach((p, i) => {
    const name = "ENC " + (i + 1);
    const g = sv("g", { class: "np-knob" }, body);
    const R = p.r;
    sv("ellipse", { cx: p.x, cy: p.y + R * 0.2, rx: R * 1.04, ry: R * 0.9,
                    fill: "#000", opacity: ".5", filter: "url(#npSoft)" }, g);
    sv("circle", { cx: p.x, cy: p.y, r: R, fill: "url(#npBrass)",
                   stroke: "url(#npBrassRim)", "stroke-width": 3 }, g);
    sv("circle", { cx: p.x, cy: p.y, r: R * 0.88, fill: "none",
                   stroke: NP.brassLo, "stroke-width": 1.2, opacity: ".55" }, g);
    const ind = sv("g", { class: "np-ind" }, g);
    sv("rect", { x: p.x - 4, y: p.y - R * 0.86, width: 8, height: R * 0.76,
                 rx: 4, fill: "#3B2D06", opacity: ".9" }, ind);
    sv("rect", { x: p.x - 1.3, y: p.y - R * 0.84, width: 2.6, height: R * 0.72,
                 rx: 1.3, fill: NP.brassHi, opacity: ".5" }, ind);
    sv("circle", { cx: p.x, cy: p.y, r: R * 0.4, fill: "url(#npCap)",
                   stroke: "#6B520D", "stroke-width": 1.5 }, g);
    sv("ellipse", { cx: p.x - R * 0.3, cy: p.y - R * 0.4, rx: R * 0.46,
                    ry: R * 0.3, fill: "url(#npSpec)", "pointer-events": "none",
                    transform: "rotate(-24 " + (p.x - R * 0.3) + " "
                               + (p.y - R * 0.4) + ")" }, g);
    svText(g, p.x, p.y + R + 22, name, "np-lbl");
    const readV = svText(g, p.x, p.y + R + 40, "0 clk", "np-val");

    let angle = 0, clicks = 0;
    function turn(dir) {
      angle += dir * 15; clicks += dir;
      ind.setAttribute("transform", "rotate(" + angle + " " + p.x + " " + p.y + ")");
      readV.textContent = (clicks > 0 ? "+" : "") + clicks + " clk";
      simCc(SIM_ENC_CC[i], dir > 0 ? 127 : 0);
      say(name + " · " + (dir > 0 ? "clockwise" : "counter-clockwise")
          + " · CC " + SIM_ENC_CC[i] + " = " + (dir > 0 ? 127 : 0));
    }
    const ri = R * 0.42;
    const ccw = svHit(g, "path", { d: npWedge(p.x, p.y, R, ri, -1) },
                      name + " — turn counter-clockwise");
    const cw = svHit(g, "path", { d: npWedge(p.x, p.y, R, ri, 1) },
                     name + " — turn clockwise");
    repeatable(ccw, () => turn(-1));
    repeatable(cw, () => turn(1));
    keyPress(ccw, () => turn(-1), null);
    keyPress(cw, () => turn(1), null);
    const cap = svHit(g, "circle", { cx: p.x, cy: p.y, r: ri },
                      name + " button, CC " + SIM_ENCBTN_CC[i]);
    const capDown = () => {
      simCc(SIM_ENCBTN_CC[i], 127);
      say(name + " button down · CC " + SIM_ENCBTN_CC[i] + " = 127");
    };
    const capUp = () => simCc(SIM_ENCBTN_CC[i], 0);
    pressRelease(cap, capDown, capUp);
    keyPress(cap, capDown, capUp);
  });

  /* ── the nine pushbuttons ─────────────────────────────────────────────
     Momentary, not switches: a domed cap sitting in a steel ring, which
     sinks and darkens while it is held and springs back on release. Each
     one is labelled with its ROLE, because the role is what the rest of the
     system knows it by — the firmware's b-number is an implementation
     detail and "SW 6" tells a thumb nothing. */

  NP_BTN.forEach(p => {
    const cc = SIM_BTN_CC[p.b - 1];
    const name = p.role;
    const R = p.r, big = R > NP_BTN_R;
    const g = sv("g", { class: "np-btn" }, body);
    sv("ellipse", { cx: p.x, cy: p.y + 5, rx: R * 1.02, ry: R * 0.8,
                    fill: "#000", opacity: ".5", filter: "url(#npSoft)" }, g);
    /* the ring the cap sits in, and the shadowed well inside it */
    sv("circle", { cx: p.x, cy: p.y, r: R, fill: "url(#npSteel)",
                   stroke: "#5C6270", "stroke-width": 1.2 }, g);
    sv("circle", { cx: p.x, cy: p.y, r: R - 5, fill: "#0C0A18",
                   stroke: "#6E7684", "stroke-width": 1 }, g);
    const cap = sv("g", { class: "np-dome" }, g);
    sv("circle", { cx: p.x, cy: p.y, r: R - 8,
                   fill: big ? "url(#npCap)" : "url(#npDome)",
                   stroke: big ? "#6B520D" : "#3A3A46",
                   "stroke-width": 1.2 }, cap);
    sv("ellipse", { cx: p.x - R * 0.22, cy: p.y - R * 0.26, rx: R * 0.34,
                    ry: R * 0.22, fill: "url(#npSpec)" }, cap);
    /* the "pressed" shading, painted over the cap only while it is held */
    const sunk = sv("circle", { cx: p.x, cy: p.y, r: R - 8, fill: "#000",
                                opacity: "0", "pointer-events": "none" }, cap);
    svText(g, p.x, p.y + R + 17, name, "np-lbl small");

    const down = () => {
      cap.setAttribute("transform", "translate(0 3)");
      sunk.setAttribute("opacity", ".5");
      simCc(cc, 127);
      say(name + " down · CC " + cc + " = 127");
    };
    const up = () => {
      cap.setAttribute("transform", "translate(0 0)");
      sunk.setAttribute("opacity", "0");
      simCc(cc, 0);
    };
    const hit = svHit(g, "circle", { cx: p.x, cy: p.y, r: R + 4 },
                      name + " button, CC " + cc);
    pressRelease(hit, down, up);
    keyPress(hit, down, up);
  });

  /* ── LiDAR ports ─────────────────────────────────────────────────────
     The port itself is the dropout: a hand over the lens is exactly how a
     real ToF reading goes away, and raw −1 is what demiurge-io then sees.
     The engraved slot under it is the distance. */
  [["tof1", 452, 104, 374, 466], ["tof2", 548, 104, 534, 626]].forEach(t => {
    const id = t[0], px = t[1], py = t[2], x0 = t[3], x1 = t[4];
    const sp = spec[id], name = sp.label;
    const g = sv("g", { class: "np-tof" }, body);
    sv("circle", { cx: px, cy: py, r: 17, fill: "#0A0918",
                   stroke: "#2A2550", "stroke-width": 2.5 }, g);
    sv("circle", { cx: px, cy: py, r: 15, fill: "#05040E" }, g);
    sv("circle", { cx: px, cy: py, r: 15, fill: "none", stroke: "#4E86FF",
                   "stroke-width": 1, opacity: ".45" }, g);
    sv("ellipse", { cx: px - 5, cy: py - 6, rx: 6, ry: 4, fill: "url(#npSpec)",
                    "pointer-events": "none" }, g);

    const ty = 152, tw = x1 - x0;
    sv("rect", { x: x0, y: ty, width: tw, height: 16, rx: 8, fill: "#150F35",
                 stroke: "#2C2560", "stroke-width": 1.5 }, g);
    const fill = sv("rect", { x: x0 + 2, y: ty + 2, width: 10, height: 12,
                              rx: 6, fill: NP.lcdBar, class: "np-fill" }, g);
    svText(g, (x0 + x1) / 2, ty - 8, name.toUpperCase(), "np-lbl");
    const rv = svText(g, (x0 + x1) / 2, ty + 34, "240 mm", "np-val");

    let track = null;
    function setMm(raw) {
      const f = Math.max(0, Math.min(1, (raw - sp.lo) / (sp.hi - sp.lo)));
      fill.setAttribute("width", Math.max(10, f * (tw - 4)));
      rv.textContent = Math.round(raw) + " mm";
      if (track) track.setAttribute("aria-valuenow", Math.round(raw));
      setSensor(id, raw, name + " · " + Math.round(raw) + " mm");
    }
    function setFrac(f) { setMm(Math.round(sp.lo + (sp.hi - sp.lo) * f)); }
    setFrac((sp.start - sp.lo) / (sp.hi - sp.lo));

    track = svHit(g, "rect", { x: x0, y: ty - 10, width: tw, height: 36, rx: 8 },
                  name + " distance, " + sp.lo + " to " + sp.hi + " mm");
    svSlider(track, sp.lo, sp.hi, vals[id]);
    bindDrag(track, g, f => { setFrac(f); return null; }, () => {});
    keyAdjust(track, 10, d => setMm(Math.max(sp.lo, Math.min(sp.hi, vals[id] + d))));

    const port = svHit(g, "circle", { cx: px, cy: py, r: 26 },
                       name + " port — hold to force a dropout, raw minus one");
    const drop = () => {
      let held = vals[id];
      if (held < 0) held = sp.start;
      port._held = held;
      rv.textContent = "dropout";
      fill.setAttribute("width", 10);
      setSensor(id, -1, name + " · DROPOUT (raw −1)");
    };
    const undrop = () => setMm(port._held == null ? sp.start : port._held);
    pressRelease(port, drop, undrop);
    keyPress(port, drop, undrop);
  });

  /* ── joystick ────────────────────────────────────────────────────────── */

  (function () {
    const cx = 300, cy = 250, R = 26;
    const g = sv("g", { class: "np-joy" }, body);
    sv("circle", { cx, cy: cy + 5, r: R + 3, fill: "#000", opacity: ".5",
                   filter: "url(#npSoft)" }, g);
    sv("circle", { cx, cy, r: R, fill: "#12102A", stroke: "url(#npSteel)",
                   "stroke-width": 3 }, g);
    sv("circle", { cx, cy, r: R - 7, fill: "#0A0918" }, g);
    const dome = sv("g", { class: "np-dome" }, g);
    sv("circle", { cx, cy, r: R * 0.66, fill: "url(#npDome)" }, dome);
    sv("ellipse", { cx: cx - R * 0.2, cy: cy - R * 0.24, rx: R * 0.3,
                    ry: R * 0.2, fill: "url(#npSpec)" }, dome);
    // Labelled ABOVE the stick — below it is the d-pad.

    /* One rule for both input paths, so the drag and the arrow keys can
       never disagree: fx/fy are SCREEN direction, right and UP positive,
       and both axes count UP. Screen-right raises jx, screen-up raises jy;
       ArrowRight raises jx, ArrowUp raises jy. The dome moves the way the
       thumb does, which is why its y offset is negated — SVG y grows
       downward. */
    function setXY(fx, fy, note) {
      fx = Math.max(-1, Math.min(1, fx));
      fy = Math.max(-1, Math.min(1, fy));
      dome.setAttribute("transform", "translate(" + (fx * R * 0.34).toFixed(1)
                        + " " + (-fy * R * 0.34).toFixed(1) + ")");
      const jx = Math.round(2048 + fx * 2047);
      const jy = Math.round(2048 + fy * 2047);
      vals.jx = jx; vals.jy = jy;
      paint("jx"); paint("jy");
      push("jx", jx); push("jy", jy);
      hit.setAttribute("aria-label",
                       "Joystick — jx " + jx + ", jy " + jy
                       + ". Drag, or nudge with the arrow keys.");
      if (note !== false) say("Joystick · jx " + jx + " · jy " + jy);
    }
    const hit = svHit(g, "circle", { cx, cy, r: R },
                      "Joystick — drag for jx and jy, arrow keys to nudge");
    hit.setAttribute("role", "group");
    let drag = false;
    const at = e => {
      const r = hit.getBoundingClientRect();
      return [(e.clientX - (r.left + r.width / 2)) / (r.width / 2),
              (e.clientY - (r.top + r.height / 2)) / (r.height / 2)];
    };
    const rest = () => { if (!drag) return; drag = false; setXY(0, 0); };
    hit.addEventListener("pointerdown", e => {
      drag = true;
      grabAndRelease(hit, e, rest);
      const p = at(e); setXY(p[0], -p[1]);
      e.preventDefault();
    });
    hit.addEventListener("pointermove", e => {
      if (!drag) return;
      const p = at(e); setXY(p[0], -p[1]);
    });
    let kx = 0, ky = 0;
    hit.addEventListener("keydown", e => {
      let dx = 0, dy = 0;
      if (e.key === "ArrowLeft") dx = -1; else if (e.key === "ArrowRight") dx = 1;
      else if (e.key === "ArrowUp") dy = 1; else if (e.key === "ArrowDown") dy = -1;
      else if (e.key === "Home" || e.key === " ") { e.preventDefault(); kx = ky = 0; setXY(0, 0); return; }
      else return;
      e.preventDefault();
      const st = e.shiftKey ? 0.5 : 0.12;
      kx = Math.max(-1, Math.min(1, kx + dx * st));
      ky = Math.max(-1, Math.min(1, ky + dy * st));
      setXY(kx, ky);
    });
  }());

  /* ── the round control opposite the stick: the joystick's own button ─── */

  (function () {
    const cx = 322, cy = 194, R = 13;
    const g = sv("g", { class: "np-jbtn" }, body);
    sv("circle", { cx, cy: cy + 5, r: R + 2, fill: "#000", opacity: ".5",
                   filter: "url(#npSoft)" }, g);
    sv("circle", { cx, cy, r: R, fill: "url(#npSteel)" }, g);
    sv("circle", { cx, cy, r: R - 6, fill: "#14121F", stroke: "#6E7684",
                   "stroke-width": 1.5 }, g);
    const cap = sv("g", { class: "np-dome" }, g);
    sv("circle", { cx, cy, r: R - 12, fill: "url(#npDome)" }, cap);
    sv("ellipse", { cx: cx - R * 0.2, cy: cy - R * 0.24, rx: R * 0.26,
                    ry: R * 0.17, fill: "url(#npSpec)" }, cap);
    svText(g, cx, cy - R - 14, "JOY BTN", "np-lbl");
    const hit = svHit(g, "circle", { cx, cy, r: R },
                      "Joystick button, CC " + SIM_JOYBTN_CC);
    const down = () => {
      cap.setAttribute("transform", "translate(0 3) scale(1)");
      simCc(SIM_JOYBTN_CC, 127);
      say("Joystick button down · CC " + SIM_JOYBTN_CC + " = 127");
    };
    const up = () => {
      cap.setAttribute("transform", "translate(0 0)");
      simCc(SIM_JOYBTN_CC, 0);
    };
    pressRelease(hit, down, up);
    keyPress(hit, down, up);
  }());

  /* ── pressure pads ────────────────────────────────────────────────────
     Disc plus ribbon, as on the shell. BOTH ribbons read left to right —
     drag right, get more pressure — even though the two discs face
     opposite ways on the panel. They used to be mirrored, each rising as
     you dragged TOWARD its own disc, which is prettier but means one of
     them runs backwards against every other slider on the page; matching
     the universal slider convention beats matching the panel's symmetry. */
  [["fsr1", 336, 352, 452, 268], ["fsr2", 664, 548, 648, 732]].forEach(t => {
    const id = t[0], dx = t[1], x0 = t[2], x1 = t[3], lx = t[4];
    const sp = spec[id], name = id.toUpperCase();
    const g = sv("g", { class: "np-fsr" }, body);
    const dy = 482, ty = 474, tw = x1 - x0;

    sv("rect", { x: x0, y: ty, width: tw, height: 16, rx: 8, fill: "#150F35",
                 stroke: "#2C2560", "stroke-width": 1.5 }, g);
    const fill = sv("rect", { x: x0 + 2, y: ty + 2, width: 0, height: 12, rx: 6,
                              fill: NP.lcdBar, class: "np-fill" }, g);

    sv("circle", { cx: dx, cy: dy + 4, r: 25, fill: "#000", opacity: ".5",
                   filter: "url(#npSoft)" }, g);
    sv("circle", { cx: dx, cy: dy, r: 24, fill: "url(#npSteel)" }, g);
    const pad = sv("circle", { cx: dx, cy: dy, r: 23, fill: "url(#npPad)",
                               class: "np-pad" }, g);
    const hot = sv("circle", { cx: dx, cy: dy, r: 23, fill: "#7FB0FF",
                               opacity: "0", "pointer-events": "none" }, g);
    sv("ellipse", { cx: dx - 8, cy: dy - 9, rx: 9, ry: 6, fill: "url(#npSpec)",
                    "pointer-events": "none" }, g);
    // Outboard of the disc: the screen bezel is above it and the bottom
    // encoder row is below it.
    svText(g, lx, dy + 6, name, "np-lbl");

    let track = null;
    function setP(p, note) {
      p = Math.max(0, Math.min(1, p));
      const raw = Math.round(sp.lo + (sp.hi - sp.lo) * p);
      const w = p * (tw - 4);
      // Both ribbons light from their left-hand end, so pressure always
      // reads as "further right" on either side of the panel.
      fill.setAttribute("width", w);
      pad.setAttribute("r", 23 - 3 * p);
      hot.setAttribute("r", 23 - 3 * p);
      hot.setAttribute("opacity", (p * 0.55).toFixed(2));
      if (track) track.setAttribute("aria-valuenow", raw);
      setSensor(id, raw, note === false ? null : name + " · raw " + raw);
    }
    setP(0, false);

    track = svHit(g, "rect", { x: x0, y: ty - 12, width: tw, height: 40, rx: 8 },
                  name + " ribbon — drag right for more pressure");
    svSlider(track, sp.lo, sp.hi, vals[id]);
    bindDrag(track, g, f => { setP(f); return null; }, () => {});
    keyAdjust(track, 100, d =>
      setP(((vals[id] < 0 ? sp.lo : vals[id]) + d - sp.lo) / (sp.hi - sp.lo)));

    const disc = svHit(g, "circle", { cx: dx, cy: dy, r: 33 },
                       name + " pad — press for full pressure");
    pressRelease(disc, () => setP(1), () => setP(0));
    keyPress(disc, () => setP(1), () => setP(0));
  });

  /* ── the expression pedal, off to the side where it lives ────────────── */

  (function () {
    const sp = spec.exp;
    const p = sv("svg", { viewBox: "0 0 200 300", class: "np-svg np-pedal",
                          role: "group", "aria-label": "Expression pedal" });
    p.innerHTML = NP_DEFS.replace(/id="np/g, 'id="npp');
    stage.appendChild(p);

    sv("ellipse", { cx: 100, cy: 268, rx: 78, ry: 16, fill: "#000",
                    opacity: ".5", filter: "url(#nppSoft)" }, p);
    sv("path", { d: "M32 262 L168 262 L176 206 Q176 196 164 196 L36 196 "
                 + "Q24 196 24 206 Z", fill: "#1B1E25", stroke: "#3A3F4A",
                 "stroke-width": 2 }, p);
    sv("circle", { cx: 100, cy: 205, r: 6, fill: "#4A505C" }, p);

    const tre = sv("g", { class: "np-treadle" }, p);
    sv("path", { d: "M34 208 L166 208 L156 92 Q155 80 142 80 L58 80 "
                 + "Q45 80 44 92 Z", fill: "url(#nppTread)", stroke: "#565C68",
                 "stroke-width": 2 }, tre);
    for (let i = 0; i < 7; i++) {
      const y = 96 + i * 16;
      const inset = 10 - i * 0.6;
      sv("rect", { x: 48 + inset, y: y, width: 104 - inset * 2, height: 5,
                   rx: 2.5, fill: "#0D0F14", opacity: ".7" }, tre);
    }
    sv("path", { d: "M58 84 L142 84 L140 96 L60 96 Z", fill: "#fff",
                 opacity: ".07" }, tre);

    const lbl = svText(p, 100, 292, "EXP · 128", "np-val");

    let hit = null;
    function setFrac(f, note) {
      f = Math.max(0, Math.min(1, f));
      const raw = Math.round(sp.lo + (sp.hi - sp.lo) * f);
      tre.setAttribute("transform", "rotate(" + (-15 + f * 21).toFixed(1)
                       + " 100 208)");
      lbl.textContent = "EXP · " + raw;
      if (hit) hit.setAttribute("aria-valuenow", raw);
      setSensor("exp", raw, note === false ? null : "Expression pedal · raw " + raw);
    }
    setFrac(0, false);

    hit = svHit(p, "rect", { x: 24, y: 60, width: 152, height: 160, rx: 12 },
                "Expression pedal — drag down to open, "
                + sp.lo + " to " + sp.hi);
    svSlider(hit, sp.lo, sp.hi, vals.exp);
    let drag = false;
    const at = e => {
      const r = hit.getBoundingClientRect();
      return Math.max(0, Math.min(1, (e.clientY - r.top) / Math.max(1, r.height)));
    };
    const end = () => { drag = false; };
    hit.addEventListener("pointerdown", e => {
      drag = true;
      grabAndRelease(hit, e, end);
      setFrac(at(e));
      e.preventDefault();
    });
    hit.addEventListener("pointermove", e => { if (drag) setFrac(at(e)); });
    keyAdjust(hit, 128, d =>
      setFrac((vals.exp + d - sp.lo) / (sp.hi - sp.lo)));
  }());

  /* ── the readout, in HTML, at a size a phone can actually read ───────── */

  BAR_IDS.forEach(id => {
    const c = el("span", "np-chip");
    c.appendChild(el("b", null, spec[id].label));
    const v = el("i", null, fmtSimVal(spec[id], vals[id]));
    c.appendChild(v);
    chips[id] = v;
    readout.appendChild(c);
  });
  BAR_IDS.forEach(paint);
  booting = false;

  return {
    node: wrap,
    /* The two things this screen cannot fake, straight off the engine feed:
       where the panel is standing, and what it has armed. */
    update() {
      const menuName = (model && neptr && neptr.menu != null && model.menus[neptr.menu])
        ? model.menus[neptr.menu].name : DASH;
      scrMenu.textContent = menuName;
      const armed = armedEffectLabel();
      // The screen is 288 units wide and does not scroll — same as the real
      // one, which truncates rather than reflows.
      scrArm.textContent = armed
        ? "ARMED · " + (armed.length > 24 ? armed.slice(0, 23) + "…" : armed)
        : "no effect armed";
    },
  };
}

/* The one thing this screen cannot fake: which effect the engine actually
   has armed. `neptr.last_toggle` is an arming-toggle id, not a slot code —
   the same lookup armState() does elsewhere on the page, over every slot's
   params, since that toggle id is only ever attached to a param. */
function armedEffectLabel() {
  if (!model || !neptr || neptr.last_toggle == null) return null;
  for (const m of model.menus) {
    for (const s of m.slots) {
      if (!s.empty && s.params.some(p => p.toggle === neptr.last_toggle)) {
        return s.label;
      }
    }
  }
  return null;
}

route(["neptr", "panel"], "Virtual panel", () => {
  let strip, inst;
  return {
    build(box) {
      strip = el("div"); box.appendChild(strip);

      /* The instrument and the sensors that live INSIDE it, side by side as
         one unit: a narrow column of compact sliders to the left of the
         shell. They have no control on the drawing because there is nothing
         on the shell to draw — but they are part of the same box, not a
         separate section further down the page. On a phone the column drops
         above the shell instead of squeezing it (see .np-unit in the CSS). */
      const unit = el("div", "np-unit");
      const col = el("aside", "np-internals");
      col.appendChild(el("div", "lbl", "Internal sensors"));
      col.appendChild(el("div", "note",
        "Inside the box, so nothing on the shell to touch. Raw values."));
      const list = el("div", "np-int-list");
      SIM_SENSORS.filter(s => SIM_INTERNAL.has(s.id))
        .forEach((s, i) => list.appendChild(buildSimSensor(s, i, true)));
      col.appendChild(list);
      unit.appendChild(col);
      inst = buildInstrument();
      unit.appendChild(inst.node);
      box.appendChild(unit);

      box.appendChild(el("div", "note",
        "The instrument, drawn. Every control here sends the exact CC a Teensy "
        + "click would, and raw sensor readings demiurge-io conditions for "
        + "real. Safe because there is no physical hardware attached to "
        + "compete with it. Hold a LiDAR port to force a dropout (raw −1)."));
    },
    update() {
      strip.textContent = "";
      strip.appendChild(linkStrip());
      inst.update();
    },
  };
});

/* ── PATCH ───────────────────────────────────────────────────────────────── */

route(["patch"], "Patch", () => ({
  build(box) { this.box = box; },
  update() {
    const box = this.box;
    box.textContent = "";
    const list = frame.patches || [];
    if (!list.length) {
      box.appendChild(el("div", "note alert",
        "No patches found. ~/demiurge/sets is empty or unreadable."));
      return;
    }
    box.appendChild(el("div", "lbl", "Sets in ~/demiurge/sets"));
    const wrap = el("div");
    wrap.style.paddingTop = "10px";
    for (const p of list) {
      const active = frame.patch && p.set === frame.patch;
      wrap.appendChild(row(p.set, {
        on: active, tag_: p.power || "", chev: !active,
        sub: active ? "loaded" : "",
        onclick: () => { if (!active) confirmSheet(
          "Load " + p.set + "? This restarts the audio engine.",
          "Load", () => post("/api/patch", { name: p.set })); },
      }));
    }
    box.appendChild(wrap);
  },
}));

/* ── AUDIO ───────────────────────────────────────────────────────────────── */

const ENGINE_ROWS = [["mode", "Mode"], ["bits", "Format"],
                     ["quantum", "Buffer"], ["rate", "Sample rate"]];
const BITS_LABEL = { "16": "16-bit", "32": "32-bit (recommended)", float: "float" };
const MODE_LABEL = { sync: "SYNC LAYER", bypass: "BYPASS" };
const ENGINE_CONFIRM = new Set(["mode", "rate"]);

function engineLabel(key, value) {
  if (key === "bits") return BITS_LABEL[value] || value;
  if (key === "mode") return MODE_LABEL[value] || String(value).toUpperCase();
  return value;
}

route(["audio"], "Audio", () => ({
  build(box) { this.box = box; },
  update() {
    const box = this.box;
    box.textContent = "";
    const a = frame.audio_settings;
    if (!a) {
      box.appendChild(el("div", "note alert",
        "Engine settings unavailable — demiurge-audio is not installed."));
    } else {
      for (const [key, label] of ENGINE_ROWS) {
        const s = a[key];
        if (!s) continue;
        const sec = el("section");
        const head = el("div", "sechead");
        head.appendChild(el("span", "lbl", label));
        head.appendChild(el("span", "mono", engineLabel(key, s.value)));
        sec.appendChild(head);
        const g = el("div", "grid2");
        for (const opt of s.options) {
          const active = opt === s.value;
          g.appendChild(row(engineLabel(key, opt), {
            on: active, disabled: active,
            onclick: () => setEngine(key, label, opt),
          }));
        }
        sec.appendChild(g);
        // The CLI's own sentence, verbatim. It is what stops this page
        // implying that BUFFER buys latency in bypass mode, where it does not.
        sec.appendChild(el("div", "note", s.effect));
        box.appendChild(sec);
      }
    }
    const sec = el("section");
    sec.appendChild(el("div", "lbl", "Engine service"));
    const r1 = el("div", "btnrow");
    const start = el("button", "btn", "Start");
    start.onclick = () => post("/api/audio", { verb: "start" });
    const stop = el("button", "btn danger", "Stop");
    const restart = el("button", "btn danger", "Restart");
    armButton(stop, "Confirm stop", () => post("/api/audio", { verb: "stop" }));
    armButton(restart, "Confirm restart",
              () => post("/api/audio", { verb: "restart" }));
    r1.appendChild(start); r1.appendChild(stop); r1.appendChild(restart);
    sec.appendChild(r1);
    box.appendChild(sec);
  },
}));

function setEngine(key, label, value) {
  if (!ENGINE_CONFIRM.has(key)) { post("/api/audio-set", { key, value }); return; }
  confirmSheet("Set " + label + " to " + engineLabel(key, value)
               + "? This restarts the audio engine.",
               "Set", () => post("/api/audio-set", { key, value }));
}

/* ── INTERFACE ───────────────────────────────────────────────────────────── */

route(["interface"], "Interface", () => ({
  build(box) { this.box = box; },
  update() {
    const box = this.box;
    box.textContent = "";
    const list = frame.interfaces || [];
    if (!list.length) {
      box.appendChild(el("div", "note alert", "No interface attached."));
      return;
    }
    for (const it of list) {
      const bits = [];
      if (it.chosen) bits.push("chosen in live.conf");
      if (it.inuse) bits.push("in use");
      box.appendChild(row(it.display || it.name, {
        on: it.active, sub: bits.join(" · "),
        tag_: it.active ? "playing" : "",
        onclick: () => { if (!it.active) confirmSheet(
          "Switch to " + (it.display || it.name)
          + "? This restarts the audio engine.",
          "Switch", () => post("/api/interface", { name: it.name })); },
      }));
    }
  },
}));

/* ── PATCHER (the live audio graph) ──────────────────────────────────────── */

const PATH_LABEL = { direct: "DIRECT", loopback: "LOOPBACK", none: "NOTHING" };
const PATH_NOTE = {
  direct: "straight to the interface, no extra latency",
  loopback: "via the loopback, one extra buffer each way",
  none: "silence",
};
const GRAPH_NO_GRAPH = {
  bypass: "The sync layer is off, so there is no graph to patch — the "
        + "chain program owns the interface directly.",
  nopipewire: "The audio graph is not running, so there is nothing to patch.",
};
let graphPick = null, graphCut = null;
const ARM_MS = 4000;

route(["patcher"], "Patcher", () => ({
  build(box) { this.box = box; },
  update() {
    const box = this.box;
    box.textContent = "";
    const g = frame.graph;
    if (!g) {
      box.appendChild(el("div", "note", "The patcher is unavailable."));
      return;
    }
    if (g.mode !== "sync") {
      box.appendChild(el("div", "note",
        GRAPH_NO_GRAPH[g.mode] || "The patcher is unavailable."));
      return;
    }
    // The fact no other screen can tell you: whether each direction is wired
    // straight through or takes the long way round the loopback.
    for (const [label, p] of [["Input", g.in_path], ["Output", g.out_path]]) {
      box.appendChild(row(label, {
        tag_: PATH_LABEL[p] || DASH, sub: PATH_NOTE[p] || "",
        cls: p === "none" ? "alert" : "", disabled: true, tag: "div",
      }));
    }
    for (const w of g.warnings || []) {
      box.appendChild(el("div", "note alert", w.text));
    }

    const links = el("section");
    links.appendChild(el("div", "lbl", "Links — tap to cut"));
    for (const l of g.links || []) {
      if (l.path === "internal" && !l.flags.includes("engine-in")
          && !l.flags.includes("engine-out")) continue;
      const armed = graphCut && graphCut.src === l.src && graphCut.dst === l.dst
                    && Date.now() < graphCut.until;
      const onEngine = (g.ports || []).some(
        p => p.kind === "engine" && (p.name === l.src || p.name === l.dst));
      links.appendChild(row(
        armed ? (onEngine ? "Tap again to cut — this can be silence"
                          : "Tap again to cut")
              : portLabel(g, l.src) + " → " + portLabel(g, l.dst), {
          cls: armed ? "alert" : "", tag_: armed ? "" : PATH_LABEL[l.path] || l.path,
          onclick: () => {
            if (armed) { graphCut = null;
              post("/api/graph/disconnect", { src: l.src, dst: l.dst }); }
            else { graphCut = { src: l.src, dst: l.dst, until: Date.now() + ARM_MS };
              refresh(); }
          },
        }));
    }
    box.appendChild(links);

    const ports = el("section");
    ports.appendChild(el("div", "lbl", graphPick
      ? "From " + portLabel(g, graphPick.name) + " — pick the other end"
      : "Ports — tap one, then another, to connect"));
    for (const p of g.ports || []) {
      if (p.kind !== "engine" && p.kind !== "interface") continue;
      if (p.display.startsWith("MON")) continue;
      const dangling = p.kind === "engine" && p.links === 0;
      const picked = graphPick && graphPick.name === p.name;
      ports.appendChild(row(portLabel(g, p.name), {
        cls: (dangling ? "alert " : "") + (picked ? "on" : ""),
        tag_: dangling ? "connected to nothing"
                       : p.dir.toUpperCase() + " " + p.links,
        onclick: () => pickPort(g, p),
      }));
    }
    box.appendChild(ports);

    const reset = el("button", "btn danger", "Reset to default wiring");
    armButton(reset, "Confirm reset", () => post("/api/graph/reset", {}));
    const rr = el("div", "btnrow"); rr.appendChild(reset);
    box.appendChild(rr);
  },
}));

function portLabel(g, name) {
  const p = (g.ports || []).find(x => x.name === name);
  if (!p) return name;
  const n = (g.nodes || []).find(x => x.name === p.node);
  return (n ? n.display : p.node) + " · " + p.display;
}

function pickPort(g, port) {
  if (graphPick && graphPick.name === port.name) { graphPick = null; }
  else if (!graphPick) { graphPick = port; }
  else if (graphPick.dir === port.dir) {
    toast("Pick " + (graphPick.dir === "out" ? "an input" : "an output")
          + " to finish this link.");
    return;
  } else {
    const src = graphPick.dir === "out" ? graphPick : port;
    const dst = graphPick.dir === "out" ? port : graphPick;
    graphPick = null;
    post("/api/graph/connect", { src: src.name, dst: dst.name });
    return;
  }
  refresh();
}

/* ── CLOCK ───────────────────────────────────────────────────────────────── */

const SYNC_ROLES = ["opgorator", "demiurge"];

route(["clock"], "Clock", () => ({
  build(box) { this.box = box; },
  update() {
    const box = this.box;
    box.textContent = "";
    const s = frame.sync;
    if (!s) {
      box.appendChild(el("div", "note", "Clock status unavailable."));
    } else {
      const hero = el("div", "hero");
      hero.appendChild(el("div", "lbl", "Tempo"));
      hero.appendChild(el("div", "patch big", s.bpm.toFixed(1) + " BPM"));
      const st = el("div", "stats");
      st.appendChild(stat("Master", s.role.toUpperCase()));
      st.appendChild(stat("Source", s.source_alive ? "live" : "idle",
                          s.source_alive ? "live" : ""));
      hero.appendChild(st);
      box.appendChild(hero);
    }
    box.appendChild(el("div", "lbl", "Who holds the clock"));
    const g = el("div", "grid2");
    g.style.marginTop = "10px";
    for (const role of SYNC_ROLES) {
      const active = !!(s && s.role === role);
      g.appendChild(row(role.toUpperCase(), {
        on: active, disabled: active,
        onclick: () => confirmSheet(
          "Make " + role.toUpperCase() + " the clock master?",
          "Set master", () => post("/api/sync", { role })),
      }));
    }
    box.appendChild(g);
  },
}));

/* ── NETWORK ─────────────────────────────────────────────────────────────── */

route(["net"], "Network", () => ({
  build(box) { this.box = box; },
  update() {
    const box = this.box;
    box.textContent = "";
    const w = frame.wifi;
    const hero = el("div", "hero");
    hero.appendChild(el("div", "lbl", "Connected to"));
    hero.appendChild(el("div", "patch big", (w && w.ssid) || "nothing"));
    const st = el("div", "stats");
    st.appendChild(stat("Address", (w && w.ip) || DASH));
    st.appendChild(stat("State", w ? w.state : DASH,
                        w && w.state === "connected" ? "live" : "bad"));
    hero.appendChild(st);
    box.appendChild(hero);

    box.appendChild(el("div", "lbl", "Networks in range"));
    const list = el("div");
    list.style.paddingTop = "10px";
    for (const n of frame.wifi_list || []) {
      list.appendChild(row(n.ssid, {
        on: n.active, disabled: n.active,
        sub: n.saved ? "saved" : "",
        tag_: n.signal + "%" + (n.security === "open" ? " · open" : ""),
        onclick: () => joinWifi(n),
      }));
    }
    box.appendChild(list);
    const scan = el("button", "btn", "Scan again");
    scan.onclick = () => { toast("Scanning…"); post("/api/wifi/scan", {}); };
    const rr = el("div", "btnrow"); rr.appendChild(scan);

    const disc = el("button", "btn", "Disconnect");
    disc.disabled = !(w && w.ssid);
    disc.onclick = () => confirmSheet("Disconnect from " + w.ssid + "?", "Disconnect",
      () => post("/api/wifi/disconnect", {}));
    rr.appendChild(disc);
    box.appendChild(rr);

    const rr2 = el("div", "btnrow");
    const radioOff = el("button", "btn", "Wifi off");
    radioOff.onclick = () => confirmSheet(
      "Turn wifi off? Nothing will auto-reconnect until you turn it back on.",
      "Turn off", () => post("/api/wifi/radio", { on: false }));
    const radioOn = el("button", "btn", "Wifi on");
    radioOn.onclick = () => post("/api/wifi/radio", { on: true });
    rr2.appendChild(radioOff); rr2.appendChild(radioOn);
    box.appendChild(rr2);
  },
}));

function joinWifi(n) {
  if (n.saved || n.security === "open") {
    confirmSheet("Join " + n.ssid + "?", "Join",
                 () => post("/api/wifi/join", { ssid: n.ssid }));
    return;
  }
  askPassword(n.ssid);
}

/* The password lives only in the input and the request body: never in a URL,
   never stored, and the server hands it to demiurge-wifi on stdin so it never
   reaches any process argv or log. */
function askPassword(ssid) {
  const sheet = $("pwsheet"), input = $("pw-input"), reveal = $("pw-reveal");
  $("pw-msg").textContent = "Password for " + ssid;
  input.value = ""; reveal.checked = false; input.type = "password";
  sheet.classList.remove("hidden");
  input.focus();
  reveal.onchange = () => { input.type = reveal.checked ? "text" : "password"; };
  const close = () => { input.value = ""; sheet.classList.add("hidden"); };
  $("pw-cancel").onclick = close;
  $("pw-ok").onclick = () => {
    const password = input.value;
    if (password.length < 8 || password.length > 63) {
      toast("The password must be 8 to 63 characters.");
      return;
    }
    close();
    post("/api/wifi/join", { ssid, password });
  };
  input.onkeydown = e => { if (e.key === "Enter") $("pw-ok").click(); };
}

/* ── SYSTEM ──────────────────────────────────────────────────────────────── */

route(["system"], "System", () => ({
  build(box) { this.box = box; },
  update() {
    const box = this.box;
    box.textContent = "";

    const sec = el("section");
    sec.appendChild(el("div", "lbl", "Power ceiling"));
    const seg = el("div", "seg");
    seg.style.marginTop = "10px";
    for (const [lvl, label] of [["low", "LOW"], ["med", "MED"], ["high", "HIGH"]]) {
      const b = el("button", frame.pwr && frame.pwr.toLowerCase() === lvl ? "on" : "",
                   label);
      b.onclick = () => post("/api/power", { level: lvl });
      seg.appendChild(b);
    }
    sec.appendChild(seg);
    box.appendChild(sec);

    const m = el("section");
    m.appendChild(el("div", "lbl", "Right now"));
    m.appendChild(meter("Temp", frame.temp_c, v => v.toFixed(1) + "°C",
                        v => 100 * v / 85, 70, 80));
    const fr = frame.freq || {};
    m.appendChild(meter("Clock", fr.cur, v => Math.round(v / 1000) + " MHz",
                        v => 100 * (v - (fr.min || 0))
                             / Math.max(1, (fr.max || v) - (fr.min || 0))));
    m.appendChild(meter("Load", frame.load ? frame.load.load1 : null,
                        v => v.toFixed(2),
                        () => frame.load.pct, 70, 90));
    if (frame.mem) {
      const used = frame.mem.total_kb - frame.mem.avail_kb;
      m.appendChild(meter("Memory", used, v => Math.round(v / 1024) + " MB",
                          v => 100 * v / frame.mem.total_kb, 70, 90));
    }
    box.appendChild(m);

    const t = el("section");
    const th = el("div", "sechead");
    th.appendChild(el("span", "lbl", "Throttling"));
    th.appendChild(el("span", "mono", frame.throttled ? frame.throttled.hex : DASH));
    t.appendChild(th);
    const chips = el("div", "chips");
    for (const n of ["UNDERVOLT", "ARM-CAP", "THROTTLE", "TEMP-LIM"]) {
      const now = frame.throttled && frame.throttled.now.includes(n);
      const past = frame.throttled && frame.throttled.past.includes(n);
      chips.appendChild(el("span", "chip" + (now ? " now" : past ? " past" : ""), n));
    }
    t.appendChild(chips);
    t.appendChild(el("div", "note", "Filled means right now; dim amber means it "
      + "happened since boot."));
    box.appendChild(t);

    if (frame.touch && frame.touch.detected) {
      const tt = el("section");
      tt.appendChild(el("div", "lbl", "Touchscreen → MIDI"));
      const g = el("div", "grid2");
      g.style.marginTop = "10px";
      for (const verb of ["on", "off"]) {
        const active = frame.touch.on === (verb === "on");
        g.appendChild(row(verb.toUpperCase(), {
          on: active, disabled: active,
          onclick: () => post("/api/touch", { verb }),
        }));
      }
      tt.appendChild(g);
      tt.appendChild(el("div", "note", frame.touch.name + " · ch"
        + frame.touch.channel + " CC" + frame.touch.base + "–"
        + (frame.touch.base + 15)));
      box.appendChild(tt);
    }

    const sp = el("section");
    sp.appendChild(el("div", "lbl", "Last ten minutes"));
    const grid = el("div", "sparks");
    grid.style.marginTop = "10px";
    const specs = [["Temp °C", f => f.temp_c, "--accent"],
                   ["Load", f => f.load ? f.load.load1 : null, "--f1"],
                   ["Xruns/min", f => f.xrun_60s, "--alert"],
                   ["EXT 5V", f => f.ext5v, "--f3"]];
    for (const [label, pick, color] of specs) {
      const w = el("div", "spark");
      w.appendChild(el("div", "lbl", label));
      const cv = el("canvas");
      cv.width = 300; cv.height = 44;
      w.appendChild(cv);
      grid.appendChild(w);
      drawSpark(cv, hist.map(pick),
                getComputedStyle(document.documentElement)
                  .getPropertyValue(color).trim());
    }
    sp.appendChild(grid);
    box.appendChild(sp);

    const info = el("div", "note");
    info.textContent = (frame.host || "?") + " · " + upFmt(frame.uptime_s)
      + " · " + (frame.ncores || "?") + " cores · governor "
      + (frame.gov || "?") + " · core "
      + (frame.volt_core != null ? frame.volt_core.toFixed(2) + " V" : "?");
    box.appendChild(info);
  },
}));

function meter(label, value, fmt, pct, warn, crit) {
  const d = el("div", "meter");
  d.appendChild(el("span", "lbl", label));
  d.appendChild(el("span", "v", value == null ? DASH : fmt(value)));
  const bar = el("div", "bar");
  const i = el("i");
  if (value != null) {
    const p = Math.max(0, Math.min(100, pct(value)));
    i.style.width = p + "%";
    if (crit != null && p >= crit) i.className = "crit";
    else if (warn != null && p >= warn) i.className = "warn";
  }
  bar.appendChild(i);
  d.appendChild(bar);
  return d;
}

function drawSpark(cv, vals, color) {
  const ctx = cv.getContext("2d");
  ctx.clearRect(0, 0, cv.width, cv.height);
  const pts = vals.filter(v => v != null);
  if (pts.length < 2) return;
  let lo = Math.min.apply(null, pts), hi = Math.max.apply(null, pts);
  if (hi - lo < 1e-9) hi = lo + 1;
  ctx.strokeStyle = color || "#ca2aeb";
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  let started = false;
  vals.forEach((v, i) => {
    if (v == null) { started = false; return; }
    const x = i * (cv.width - 2) / Math.max(1, vals.length - 1) + 1;
    const y = cv.height - 3 - (v - lo) / (hi - lo) * (cv.height - 6);
    if (started) ctx.lineTo(x, y); else { ctx.moveTo(x, y); started = true; }
  });
  ctx.stroke();
}

/* ── boot ────────────────────────────────────────────────────────────────── */

/* Re-entering the router is how a screen is rebuilt; `depth` is preserved so
   the rebuild does not animate as a new push. */
function rebuild() {
  const d = depth;
  navigateReset();
  depth = d;
}

navigateReset();

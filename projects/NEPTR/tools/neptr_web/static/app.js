"use strict";
// NEPTR web: vanilla JS, pointer events only (no HTML5 DnD: iOS Safari has none).
const $ = (s, r = document) => r.querySelector(s);
const el = (t, p = {}, ...k) => { const e = document.createElement(t); for (const [a, v] of Object.entries(p)) { if (a === "class") e.className = v; else if (a.startsWith("on")) e.addEventListener(a.slice(2), v); else if (v !== false && v != null) e.setAttribute(a, v === true ? "" : v); } for (const c of k.flat()) if (c != null) e.append(c.nodeType ? c : document.createTextNode(c)); return e; };
const api = async (path, body) => { const r = await fetch(path, body === undefined ? {} : { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) }); const j = await r.json(); if (!r.ok) throw new Error(j.error || r.status); return j; };
let toastT; const toast = (m, bad) => { const t = $("#toast"); t.textContent = m; t.className = bad ? "bad" : ""; t.hidden = false; clearTimeout(toastT); toastT = setTimeout(() => t.hidden = true, 2200); };
const fail = e => toast(String(e.message || e), true);

// slot kinds (DUODECIMUS show layer): nam = NAM-only, guitar = guitar only, full = all three, toggle = not a preset
const KIND = n => [1, 3, 5].includes(n) ? "nam" : n === 7 ? "vocalbtn" : n === 11 ? "blooperbtn" : n >= 13 ? "full" : "guitar";
const hasG = d => d.kind === "guitar" || d.kind === "full", hasN = d => d.kind === "nam" || d.kind === "full", hasV = d => d.kind === "full";
const S = { tab: localStorage.getItem("tab") || "presets", data: null, sel: null, draft: null, open: {}, live: null, touching: false, expanded: {} };
const FX = id => S.data.effects.find(e => e.id === id);
const namLabel = n => (n || "").replace(/^\d+_/, "").replace(/_/g, " ");
const short = n => n.replace(/^gk/, "");
const fmt = v => Math.abs(v) >= 100 ? v.toFixed(0) : Math.abs(v) >= 10 ? v.toFixed(1) : v.toFixed(2);

async function load() { S.data = await api("/api/state"); S.live = S.data.live; render(); }
function setLink(ok) { const c = $("#link"); c.textContent = ok ? "live" : "offline"; c.className = "chip" + (ok ? " ok" : ""); }

// ---------- draft ----------
async function select(slot) {
  if (S.draft && S.draft.dirty && !confirm("Discard unsaved edits to slot " + S.draft.slot + "?")) return;
  const d = await api("/api/slot/" + slot);
  const meta = S.data.slots[slot - 1];
  S.sel = slot;
  const steps = d.seq ? d.seq.map(x => ({ ...x })) : [];
  const nm = steps.filter(x => x.type === 3).pop(), vc = steps.filter(x => x.type === 2).pop();
  // three sections of one slot: GUITAR (type 1 steps), NAM (one type 3 step or null), VOCAL (null | 0 | 1)
  S.draft = { slot, kind: KIND(slot), occupied: d.occupied, name: meta.name || "", seq: steps.filter(x => x.type === 1), nam: nm ? { type: 3, a: nm.a, b: nm.b, c: nm.c } : null, vocal: vc ? (vc.a ? 1 : 0) : null, legacy: d.occupied && !d.seq && KIND(slot) !== "nam", derived: d.derived, vals: d.vals, touched: {}, dirty: false };
  render();
}
const dirty = () => { S.draft.dirty = true; const b = $("#savebtn"); if (b) b.classList.add("dirty"); };
const stepLabel = s => FX(s.a) ? FX(s.a).name : "fx " + s.a;
// a legacy slot becomes a sequence the moment NAM/VOCAL is edited: keep its derived guitar order
const unlegacy = d => { if (d.legacy) { d.seq = (d.derived || []).map(x => ({ ...x })); d.legacy = false; } };
// NAM and VOCAL go FIRST: the NAM step takes the pedal-owner slot, so the last GUITAR step (VOLUME) must still run after it
const composeSeq = d => [...(hasN(d) && d.nam ? [{ type: 3, a: d.nam.a, b: d.nam.b, c: d.nam.c }] : []), ...(hasV(d) && d.vocal != null ? [{ type: 2, a: d.vocal }] : []), ...(hasG(d) ? d.seq : [])];
function addStep(st) { if (!S.draft) return toast("select a preset first", true); if (!hasG(S.draft)) return toast("slot " + S.draft.slot + " has no guitar section", true); if (composeSeq(S.draft).length >= 16) return toast("16 steps max", true); unlegacy(S.draft); S.draft.seq.push(st); dirty(); render(); }
async function saveDraft() {
  const d = S.draft; if (!d) return;
  const body = { name: d.name, vals: d.touched };
  if (!d.legacy || d.nam || d.vocal != null) { unlegacy(d); body.seq = composeSeq(d); }
  try { await api("/api/slot/" + d.slot + "/save", body); toast("saved " + d.slot); d.dirty = false; await load(); await select(d.slot); } catch (e) { fail(e); }
}

// ---------- widgets ----------
function slider(label, min, max, val, onInput, opts = {}) {
  const log = opts.log && min > 0;
  const toPos = v => log ? Math.log(v / min) / Math.log(max / min) * 1000 : (v - min) / (max - min) * 1000;
  const fromPos = p => log ? min * Math.pow(max / min, p / 1000) : min + p / 1000 * (max - min);
  const out = el("output", {}, fmt(val));
  const inp = el("input", { type: "range", min: 0, max: 1000, step: 1, value: Math.round(toPos(Math.max(min, Math.min(max, val)))) });
  const go = v => { out.textContent = fmt(v); onInput(v, false); };
  inp.addEventListener("input", () => go(fromPos(+inp.value)));
  inp.addEventListener("change", () => onInput(fromPos(+inp.value), true));
  inp.addEventListener("pointerdown", () => S.touching = true);
  const up = () => setTimeout(() => S.touching = false, 400);
  inp.addEventListener("pointerup", up); inp.addEventListener("pointercancel", up);
  const row = el("div", { class: "sl" }, el("label", {}, label), inp, out);
  row.set = v => { if (!S.touching) { inp.value = Math.round(toPos(Math.max(min, Math.min(max, v)))); out.textContent = fmt(v); } };
  return row;
}
function paramSliders(e) {
  const d = S.draft; if (!d) return el("div", { class: "note" }, "Select a preset in PRESETS to edit values.");
  if (!e.params.length) return el("div", { class: "note" }, "No encoder parameters for this effect.");
  return el("div", {}, e.params.map(p => {
    const cur = d.touched[p.name] ?? d.vals[p.name] ?? (p.min + p.max) / 2;
    return slider(short(p.name), p.min, p.max, cur, v => { d.touched[p.name] = v; d.vals[p.name] = v; dirty(); }, { log: p.log });
  }), el("div", { class: "note" }, d.occupied ? "Applied when you recall this preset." : "Slot is empty: values start from the rig's current state on first SAVE."));
}

// ---------- PRESETS ----------
function tile(sl) {
  const n = sl.slot, label = n <= 12 ? n : n - 12, k = KIND(n);
  if (k === "vocalbtn" || k === "blooperbtn") return el("button", { class: "tile empty", disabled: true }, el("b", {}, String(label)), el("span", {}, k === "vocalbtn" ? "VOCAL" : "BLOOPER"));
  return el("button", { class: "tile" + (sl.occupied ? "" : " empty") + (S.sel === n ? " sel" : ""), onclick: () => select(n) },
    el("b", {}, String(label)), el("span", {}, sl.occupied ? (sl.name || (sl.seq ? sl.seq.length + " steps" : "legacy")) : "empty"));
}
function viewPresets(v) {
  v.append(el("h2", {}, "DUODECIMUS"), el("div", { class: "grid duo" }, S.data.slots.slice(0, 12).map(tile)),
    el("h2", {}, "ONBOARD"), el("div", { class: "grid onb" }, S.data.slots.slice(12).map(tile)));
  const d = S.draft; if (!d) { v.append(el("p", { class: "note" }, "Tap a slot.")); return; }
  const c = el("div", { class: "card" });
  c.append(el("h2", { style: "margin-top:0" }, (d.slot <= 12 ? "DUODECIMUS " + d.slot : "ONBOARD " + (d.slot - 12))));
  c.append(el("input", { type: "text", maxlength: 24, placeholder: "name", value: d.name, autocomplete: "off", oninput: e => { d.name = e.target.value; dirty(); } }));
  c.append(el("div", { class: "row", style: "margin-top:8px" },
    el("button", { class: "pri", onclick: () => api("/api/slot/" + d.slot + "/recall", {}).then(() => toast("loaded " + d.slot)).catch(fail), disabled: !d.occupied || d.dirty }, "LOAD"),
    el("button", { id: "savebtn", class: d.dirty ? "dirty" : "", onclick: saveDraft }, "SAVE"),
    d.kind === "nam" ? null : el("button", { onclick: () => { if (confirm("Capture the rig's current state into slot " + d.slot + " (records activation order from the pedalboard)?")) api("/api/slot/" + d.slot + "/capture", {}).then(() => { toast("captured"); setTimeout(load, 1500); }).catch(fail); } }, "CAPTURE")));
  c.append(el("div", { class: "row", style: "margin-top:8px" },
    el("button", { onclick: () => { const s = prompt("Copy from slot (1-17):"); if (s) api("/api/slot/" + d.slot + "/copy", { from: +s }).then(() => load().then(() => select(d.slot))).catch(fail); } }, "COPY FROM"),
    el("button", { class: "danger", disabled: !d.occupied, onclick: () => { if (confirm("Clear slot " + d.slot + "?")) api("/api/slot/" + d.slot + "/clear", {}).then(() => { S.draft = null; return load(); }).catch(fail); } }, "CLEAR")));
  const sect = (t, on, f) => { if (on) { c.append(el("h2", {}, t)); f(); } };
  sect("GUITAR", hasG(d), () => {
  if (d.legacy) {
    c.append(el("p", { class: "note" }, "Legacy preset: restored as captured, VOLUME takes the pedal. Make a sequence to control order."),
      el("button", { class: "pri", onclick: () => { unlegacy(d); dirty(); render(); } }, "MAKE SEQUENCE FROM CURRENT"));
  } else {
    const list = el("div", { id: "seq" });
    d.seq.forEach((s, i) => list.append(stepRow(s, i)));
    c.append(list);
    if (!d.seq.length) c.append(el("p", { class: "note" }, "Empty: every guitar effect is off after recall."));
    c.append(el("p", { class: "note" }, "Steps run top to bottom as if pressed. The last effect that claims the pedal owns it: put VOLUME last. Drag the grip to reorder."),
      el("button", { class: "pri", style: "width:100%", onclick: addSheet }, "+ ADD EFFECT"));
  }
  });
  sect("NAM", hasN(d), () => {
  c.append(namSection(d));
  });
  sect("VOCAL", hasV(d), () => {
  const seg = (lab, val) => el("button", { class: "radio" + (d.vocal === val ? " on" : ""), onclick: () => { if (d.legacy) unlegacy(d); d.vocal = val; dirty(); render(); } }, el("i"), lab);
  c.append(seg("LEAVE VOCAL ALONE", null), seg("VOCAL ON", 1), seg("VOCAL OFF", 0));
  });
  v.append(c);
}
function namSection(d) {
  const w = el("div", {});
  const lv = el("button", { class: "radio" + (!d.nam ? " on" : ""), onclick: () => { if (d.legacy) unlegacy(d); d.nam = null; dirty(); render(); } }, el("i"), "LEAVE AMP ALONE");
  const live = S.live && S.live.nam;
  const set = el("button", { class: "radio" + (d.nam ? " on" : ""), onclick: () => { if (d.nam) return; unlegacy(d); d.nam = { type: 3, a: live ? Math.max(0, live.profile) : 5, b: live ? live.inGain : 0, c: live ? live.outGain : 0 }; dirty(); render(); } }, el("i"), "SET AMP");
  w.append(lv, set);
  if (d.nam) {
    const n = d.nam;
    w.append(el("div", { class: "sub" }, el("select", { onchange: e => { n.a = +e.target.value; dirty(); } }, S.data.nam.map((m, k) => el("option", { value: k, selected: k === n.a }, namLabel(m)))),
      slider("IN dB", -12, 18, n.b, v => { n.b = v; dirty(); }), slider("OUT dB", -18, 18, n.c, v => { n.c = v; dirty(); })));
  }
  return w;
}
function stepRow(s, i) {
  const r = el("div", { class: "step", "data-i": i });
  const grip = el("div", { class: "grip" }, "≡");
  r.append(grip, el("span", { class: "num" }, String(i + 1)), el("span", { class: "lab" }, stepLabel(s)));
  if (s.type === 1 && FX(s.a) && FX(s.a).claims_pedal) r.append(el("span", { class: "badge" }, "PEDAL"));
  if (s.type === 1 && FX(s.a) && FX(s.a).params.length) r.append(el("button", { class: "ghost", onclick: () => { const k = S.draft.slot + ":" + i; S.expanded[k] = !S.expanded[k]; render(); } }, "⚙"));
  r.append(el("button", { class: "ghost danger", onclick: () => { S.draft.seq.splice(i, 1); dirty(); render(); } }, "✕"));
  if (s.type === 1 && S.expanded[S.draft.slot + ":" + i]) r.append(el("div", { class: "sub" }, paramSliders(FX(s.a))));
  dragInit(grip, r);
  return r;
}
function dragInit(grip, row) {
  let on = false, raf = 0, y = 0;
  const list = () => $("#seq");
  const tick = () => { if (!on) return; if (y < 90) scrollBy(0, -12); else if (y > innerHeight - 140) scrollBy(0, 12); place(); raf = requestAnimationFrame(tick); };
  const place = () => {
    const items = [...list().children].filter(c => c !== row);
    let before = null;
    for (const it of items) { const b = it.getBoundingClientRect(); if (y < b.top + b.height / 2) { before = it; break; } }
    if (before !== row.nextSibling) list().insertBefore(row, before);
  };
  grip.addEventListener("pointerdown", e => { on = true; y = e.clientY; grip.setPointerCapture(e.pointerId); row.classList.add("drag"); raf = requestAnimationFrame(tick); e.preventDefault(); });
  grip.addEventListener("pointermove", e => { if (on) y = e.clientY; });
  const end = () => {
    if (!on) return; on = false; cancelAnimationFrame(raf); row.classList.remove("drag");
    const order = [...list().children].map(c => +c.dataset.i);
    if (order.some((o, k) => o !== k)) { S.draft.seq = order.map(o => S.draft.seq[o]); dirty(); render(); }
  };
  grip.addEventListener("pointerup", end); grip.addEventListener("pointercancel", end);
}
function addSheet() {
  const sh = $("#sheet"); sh.hidden = false; sh.replaceChildren();
  const close = () => { sh.hidden = true; };
  sh.append(el("div", { class: "row" }, el("button", { onclick: close }, "CLOSE")));
  const q = el("input", { type: "text", placeholder: "search effects", autocomplete: "off", style: "margin-top:10px", oninput: () => fill() });
  const box = el("div", { style: "margin-top:10px" });
  sh.append(q, box);
  const fill = () => {
    box.replaceChildren();
    const t = q.value.trim().toLowerCase();
    const add = (label, st) => box.append(el("button", { class: "opt", onclick: () => { close(); addStep(st); } }, label));
    let menu = "";
    for (const e of S.data.effects) {
      if (t && !(e.name.toLowerCase().includes(t) || e.menu.toLowerCase().includes(t))) continue;
      if (e.menu !== menu) { menu = e.menu; box.append(el("h2", {}, menu)); }
      add(e.name + (e.generative ? "  (generative)" : ""), { type: 1, a: e.id });
    }
  };
  fill();
}

// ---------- GUITAR ----------
function viewGuitar(v) {
  const d = S.draft;
  v.append(el("p", { class: "note" }, d ? "Editing preset slot " + d.slot + (d.name ? " — " + d.name : "") + ". Switch buttons act on the rig now; + adds to its sequence; tap a name for values." : "Switch buttons act on the rig now. Select a preset in PRESETS to edit sequence and values."));
  let menu = "";
  for (const e of S.data.effects) {
    if (e.menu !== menu) { menu = e.menu; v.append(el("h2", {}, menu)); }
    const on = S.live && S.live.on && S.live.on[e.id];
    const open = S.open[e.id];
    v.append(el("div", { class: "fx" },
      el("button", { class: "nm" + (open ? " open" : ""), onclick: () => { S.open[e.id] = !open; render(); } }, e.name),
      el("button", { class: "tog" + (on ? " on" : ""), "data-fx": e.id, onclick: () => api("/api/fx", { id: e.id, on: !(S.live && S.live.on && S.live.on[e.id]) }).then(() => { if (S.live) S.live.on[e.id] = !on; render(); setTimeout(poll, 2300); }).catch(fail) }, on ? "ON" : "OFF"),
      el("button", { disabled: !d || !hasG(d), onclick: () => addStep({ type: 1, a: e.id }) }, "+")));
    if (open) v.append(el("div", { class: "pad" }, paramSliders(e)));
  }
}

// ---------- NAM ----------
function viewNam(v) {
  const L = (S.live && S.live.nam) || { profile: -1, bypass: false, inGain: 0, outGain: 0 };
  const cur = { p: L.profile, i: L.inGain, o: L.outGain };
  S.namCur = S.namCur || cur;
  const n = S.namCur;
  const send = () => (S.namT = Date.now(), api("/api/nam", { profile: n.p, in: n.i, out: n.o }).catch(fail));
  v.append(el("p", { class: "note" }, "One amp is active. Changes apply to the rig now and persist across reboots."));
  v.append(el("div", { class: "row" }, el("button", { class: L.bypass ? "" : "pri", onclick: () => api("/api/nam_byp", { bypass: !L.bypass }).then(() => { L.bypass = !L.bypass; render(); setTimeout(poll, 2300); }).catch(fail) }, L.bypass ? "AMP BYPASSED" : "AMP ON")));
  v.append(slider("IN dB", -12, 18, n.i, (x, fin) => { n.i = x; S.namT = Date.now(); if (fin) send(); }), slider("OUT dB", -18, 18, n.o, (x, fin) => { n.o = x; S.namT = Date.now(); if (fin) send(); }));
  S.data.nam.forEach((name, k) => {
    if (k % 5 === 0) v.append(el("h2", {}, S.data.nam_rows[k / 5] || ""));
    v.append(el("button", { class: "radio" + (n.p === k ? " on" : ""), onclick: () => { n.p = k; send(); render(); } }, el("i"), namLabel(name)));
  });
  v.append(el("button", { class: "pri", style: "width:100%;margin-top:10px", disabled: !S.draft || !hasN(S.draft), onclick: () => {
    const d = S.draft; unlegacy(d); d.nam = { type: 3, a: Math.max(0, n.p), b: n.i, c: n.o };
    dirty(); toast("NAM section set in slot " + d.slot + " (SAVE to keep)"); } }, !S.draft ? "SELECT A PRESET FIRST" : hasN(S.draft) ? "SAVE INTO PRESET " + S.draft.slot : "SLOT " + S.draft.slot + " HAS NO NAM"));
}

// ---------- VOCAL ----------
function viewVocal(v) {
  const L = (S.live && S.live.voc) || { on: true, level: 1 };
  v.append(el("button", { id: "vocbtn", class: "big" + (L.on ? " on" : ""), onclick: () => api("/api/vocal", { on: !L.on }).then(() => { S.vocT = Date.now(); L.on = !L.on; render(); setTimeout(poll, 2300); }).catch(fail) }, L.on ? "ON" : "OFF"));
  let t = 0;
  const sl = slider("LEVEL", 0, 4, L.level, (x, fin) => { S.vocT = Date.now(); const now = Date.now(); if (fin || now - t > 90) { t = now; api("/api/vocal", { level: x }).catch(fail); } });
  sl.id = "voclevel"; v.append(el("h2", {}, "LEVEL"), sl,
    el("p", { class: "note" }, "Same value as DUODECIMUS encoder 7 (range 0-4). The rig reports changes within about 2 s."),
    el("button", { class: "pri", style: "width:100%;margin-top:10px", disabled: !S.draft || !hasV(S.draft), onclick: () => { const d = S.draft; unlegacy(d); d.vocal = (S.live && S.live.voc ? S.live.voc.on : L.on) ? 1 : 0; dirty(); toast("VOCAL section set in slot " + d.slot + " (SAVE to keep)"); } }, !S.draft ? "SELECT A PRESET FIRST" : hasV(S.draft) ? "SAVE VOCAL STATE INTO PRESET " + S.draft.slot : "SLOT " + S.draft.slot + " HAS NO VOCAL"));
}

// ---------- shell ----------
function render() {
  if (!S.data) return;
  const v = $("#view"); const y = scrollY; v.replaceChildren();
  document.querySelectorAll("#tabs button").forEach(b => b.classList.toggle("on", b.dataset.tab === S.tab));
  ({ presets: viewPresets, guitar: viewGuitar, nam: viewNam, vocal: viewVocal })[S.tab](v);
  scrollTo(0, y);
}
document.querySelectorAll("#tabs button").forEach(b => b.addEventListener("click", () => { S.tab = b.dataset.tab; try { localStorage.setItem("tab", S.tab); } catch (e) {} S.namCur = null; render(); scrollTo(0, 0); }));
async function poll() {
  try {
    const l = await api("/api/live"); setLink(true);
    if (!l.on || S.touching) return;
    S.live = l;
    if (S.tab === "vocal" && Date.now() - (S.vocT || 0) > 4000) { const b = $("#vocbtn"); if (b) { b.className = "big" + (l.voc.on ? " on" : ""); b.textContent = l.voc.on ? "ON" : "OFF"; } const s = $("#voclevel"); if (s) s.set(l.voc.level); }
    else if (S.tab === "guitar") document.querySelectorAll("button[data-fx]").forEach(b => { const on = !!l.on[b.dataset.fx]; b.className = "tog" + (on ? " on" : ""); b.textContent = on ? "ON" : "OFF"; });
    else if (S.tab === "nam" && Date.now() - (S.namT || 0) > 5000) { const j = JSON.stringify(l.nam); if (j !== S.namJ) { S.namJ = j; S.namCur = null; render(); } }
  } catch (e) { setLink(false); }
}
load().catch(fail);
setInterval(poll, 2000);

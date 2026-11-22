// NEPTR phone-ui v1 SCAFFOLD. Vanilla, event-driven only: no timers/rAF running idle.
// The one exception is the INPUTS screen "simulate" button, which runs a timer ONLY while it is toggled on.
(function () {
  "use strict";
  var M = window.MOCK, $ = function (s) { return document.querySelector(s); };
  var state = {
    patch: M.patches.filter(function (p) { return p.active; })[0].id,
    vals: JSON.parse(JSON.stringify(M.params)),
    maps: M.mappings.slice(),
    learn: null,           // {src} while waiting for a param touch
    held: null,            // input id held down
    live: {}               // mock live input values
  };
  function el(tag, cls, txt) { var e = document.createElement(tag); if (cls) e.className = cls; if (txt != null) e.textContent = txt; return e; }
  function clamp(v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

  // ---- fader: custom-drawn, pointer-driven, repaints only on input ----
  function fader(name, get, set, color) {
    var f = el("div", "fader"), bar = el("i"), l = el("span", null, name.toUpperCase()), v = el("em");
    f.style.setProperty("--c", color); f.append(bar, l, v);
    function paint() { var x = get(); bar.style.width = (x * 100) + "%"; v.textContent = x.toFixed(2); }
    function ptr(e) { var r = f.getBoundingClientRect(); set(clamp((e.clientX - r.left) / r.width)); paint(); }
    f.addEventListener("pointerdown", function (e) { f.setPointerCapture(e.pointerId); ptr(e); f._d = 1; });
    f.addEventListener("pointermove", function (e) { if (f._d) ptr(e); });
    f.addEventListener("pointerup", function () { f._d = 0; });
    paint(); return f;
  }
  var COLS = ["var(--f0)", "var(--f1)", "var(--f2)", "var(--f3)"];

  // ---- screen 1: patches ----
  function drawPatches() {
    var s = $("#patches"); s.textContent = "";
    s.append(el("h2", null, "PATCHES (FOLDER = UI NAME)"));
    M.patches.forEach(function (p) {
      var c = el("button", "card" + (p.id === state.patch ? " on" : ""), p.id.replace(/_/g, " "));
      c.append(el("small", null, p.id + "/index.html · " + p.note));
      c.onclick = function () { state.patch = p.id; draw(); };
      s.append(c);
    });
    s.append(el("h2", null, "ACTIVE: " + state.patch + " (PARAMS 0–1)"));
    var ps = state.vals[state.patch], i = 0, any = false;
    Object.keys(ps).forEach(function (k) {
      any = true;
      s.append(fader(k, function () { return ps[k]; }, function (x) { ps[k] = x; }, COLS[i++ % 4]));
    });
    if (!any) s.append(el("p", "hint", "Blank patch. Edit its index.html to draw a UI."));
  }

  // ---- screen 2: live mapping editor ----
  function drawMapping() {
    var s = $("#mapping"); s.textContent = "";
    s.append(el("h2", null, "MAP · HOLD INPUT, THEN TOUCH A PARAM"));
    var hint = el("p", "hint", state.held ? "Holding " + state.held + ": tap a param below to map it." : "Hold an input button, then tap a param.");
    s.append(hint);
    var inp = el("div"); inp.style.cssText = "display:grid;grid-template-columns:1fr 1fr;gap:8px";
    M.devices.forEach(function (d) { d.inputs.forEach(function (i) {
      var id = d.id + "/" + i, b = el("button", state.held === id ? "on" : "", id);
      // hold = pointerdown, release = pointerup/cancel
      b.addEventListener("pointerdown", function (e) { b.setPointerCapture(e.pointerId); state.held = id; drawMapping(); });
      b.addEventListener("pointerup", function () { state.held = null; drawMapping(); });
      b.addEventListener("pointercancel", function () { state.held = null; drawMapping(); });
      inp.append(b);
    }); });
    s.append(inp);
    s.append(el("h2", null, "PARAMS · " + state.patch));
    Object.keys(state.vals[state.patch]).forEach(function (k) {
      var b = el("button", "card", k.toUpperCase()); b.style.padding = "12px 14px";
      b.addEventListener("pointerdown", function () {
        if (!state.held) return;
        var dst = state.patch + "/" + k;
        state.maps = state.maps.filter(function (m) { return m.dst !== dst; });
        state.maps.push({src: state.held, dst: dst});
        drawMapping();
      });
      s.append(b);
    });
    s.append(el("h2", null, "ACTIVE MAPPINGS (" + state.maps.length + ")"));
    state.maps.forEach(function (m, idx) {
      var r = el("div", "map"); r.append(el("span", null, m.src + "  →  "), el("b", null, m.dst));
      var x = el("button", "rec", "×"); x.style.cssText = "padding:2px 10px;flex:0";
      x.onclick = function () { state.maps.splice(idx, 1); drawMapping(); };
      r.append(x); s.append(r);
    });
  }

  // ---- screen 3: input monitor ----
  var simTimer = null, meters = {};
  function drawMonitor() {
    var s = $("#monitor"); s.textContent = ""; meters = {};
    s.append(el("h2", null, "INPUT MONITOR (MOCK)"));
    var b = el("button", simTimer ? "on" : "", simTimer ? "SIMULATING" : "SIMULATE INPUTS");
    b.onclick = function () { toggleSim(); drawMonitor(); };
    s.append(b);
    M.devices.forEach(function (d) {
      var c = el("div", "card"), h = el("div", "dev");
      h.append(el("span", null, d.name), el("span", d.ok ? "ok" : "stale", d.ok ? "LIVE" : "NO SIGNAL"));
      c.append(h);
      d.inputs.forEach(function (i) {
        var id = d.id + "/" + i, row = el("div"); row.style.marginTop = "8px";
        row.append(el("small", null, i), el("div", "meter"));
        var bar = el("i"); row.lastChild.append(bar); meters[id] = bar; bar.style.width = ((state.live[id] || 0) * 100) + "%";
        c.append(row);
      });
      s.append(c);
    });
  }
  function toggleSim() {
    if (simTimer) { clearInterval(simTimer); simTimer = null; return; }
    simTimer = setInterval(function () {
      var t = Date.now() / 1000;
      Object.keys(meters).forEach(function (id, n) {
        var v = .5 + .5 * Math.sin(t * (.6 + n * .23));
        state.live[id] = v; meters[id].style.width = (v * 100) + "%";
      });
    }, 100);
  }

  // ---- shell ----
  var screens = {patches: drawPatches, mapping: drawMapping, monitor: drawMonitor}, cur = "patches";
  function draw() { screens[cur](); $("#where").textContent = cur === "patches" ? state.patch : cur; }
  function show(n) {
    if (simTimer && n !== "monitor") toggleSim();   // never leave a timer running off-screen
    cur = n;
    ["patches", "mapping", "monitor"].forEach(function (k) { $("#" + k).hidden = k !== n; });
    document.querySelectorAll("nav button").forEach(function (b) { b.classList.toggle("on", b.dataset.s === n); });
    draw();
  }
  document.querySelectorAll("nav button").forEach(function (b) { b.onclick = function () { show(b.dataset.s); }; });
  show("patches");
})();

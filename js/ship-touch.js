/* ship-touch.js — on-screen touch controls for ship mode (Q36). Pure maths (node-testable) + one pixel-drawn canvas overlay.
   The overlay never owns game state: it turns touches into the same events the keyboard / mouse path already handles
   (synthetic keydown/keyup on window, mousedown/up on document) plus host.look(dx,dy) and host.throttle(d).
   Layout: floating left stick (move / fly), right-side drag = look, pads Z F T E SPACE SHIFT FIRE RMB and a hold-ESC exit pad,
   two-finger pinch on the view = throttle in flight. A held pad is a held key, so Z/T/ESC hold variants work by holding the pad.
   Zero idle CPU: the canvas redraws only when something changed (rAF is scheduled on demand), listeners are passive-free pointer events. */

export var STICK_R = 0.11;          // stick radius as a fraction of min(W,H)
export var STICK_DEAD = 0.18;       // normalised dead zone
export var LOOK_GAIN = 1.6;         // px of drag -> mouse-delta px
export var PINCH_GAIN = 2.2;        // throttle change per unit of relative pinch spread
export var REPEAT_MS = 250;         // a stick key re-pressed within this of its last press goes out as a key repeat (no double-tap roll/flip)
export var IDLE_MS = 10000;         // no touch for this long -> the sync poll stops until the next touchstart
export var POLL_MS = 300;

/* stickVec: finger offset (dx,dy px) from the stick origin -> {x,y} in [-1,1] (y up), dead zone removed, length clamped to 1. */
export function stickVec(dx, dy, radius, dead) {
  var d0 = dead === undefined ? STICK_DEAD : dead, len = Math.sqrt(dx * dx + dy * dy), n, k;
  if (!(radius > 0) || !(len > 0)) return { x: 0, y: 0 };
  n = Math.min(1, len / radius);
  if (n < d0) return { x: 0, y: 0 };
  k = ((n - d0) / (1 - d0)) / len * radius;
  return { x: dx * k / radius, y: -dy * k / radius };
}
/* stickKeys: analog vector -> held WASD booleans (threshold 0.35 so a light push does not count). */
export function stickKeys(v, th) {
  var t = th === undefined ? 0.35 : th;
  return { KeyW: v.y > t, KeyS: v.y < -t, KeyA: v.x < -t, KeyD: v.x > t };
}
/* padLayout: pad circles for a W x H screen. Right-bottom cluster; y grows downward. */
export function padLayout(W, H) {
  var m = Math.min(W, H), r = Math.max(22, Math.round(m * 0.075)), g = r * 2.25, cx = W - r * 1.6 - g * 1.2, cy = H - r * 1.7, p = [];
  function add(code, label, x, y, rr, extra) { var o = { code: code, label: label, x: x, y: y, r: rr || r }; if (extra) for (var k in extra) o[k] = extra[k]; p.push(o); }
  add('Space', 'SPC', W - r * 1.7, cy, r * 1.15);
  add('FIRE', 'FIRE', W - r * 1.7 - g * 1.05, cy - r * 0.2, r * 1.15, { mouse: 0 });
  add('ShiftLeft', 'SHF', W - r * 1.7, cy - g * 1.05, r);
  add('RMB', 'RMB', W - r * 1.7 - g * 1.05, cy - g * 1.25, r, { mouse: 2 });
  add('KeyZ', 'Z', cx - g * 0.1, cy - g * 2.15, r * 0.9);
  add('KeyF', 'F', cx - g * 1.1, cy - g * 2.15, r * 0.9);
  add('KeyT', 'T', cx - g * 2.1, cy - g * 2.15, r * 0.9);
  add('KeyE', 'E', cx - g * 3.1, cy - g * 2.15, r * 0.9);
  add('KeyP', 'P', cx - g * 4.1, cy - g * 2.15, r * 0.9);                 // photo mode
  add('Enter', 'CHAT', cx - g * 5.1, cy - g * 2.15, r * 0.9);             // opens the chat/command line (Enter)
  add('Escape', 'EXIT', r * 1.6, r * 1.6, r * 0.85, { hold: true });     // top-left: hold 3 s (the game's own Esc timer)
  return p;
}
export function hitPad(pads, x, y) {
  var i, p, dx, dy;
  for (i = pads.length - 1; i >= 0; i--) { p = pads[i]; dx = x - p.x; dy = y - p.y; if (dx * dx + dy * dy <= p.r * p.r * 1.21) return p; }
  return null;
}
/* pinchThrottle: relative spread change between two finger distances -> throttle delta (spread up = faster). */
export function pinchThrottle(d0, d1) {
  if (!(d0 > 8) || !(d1 > 0)) return 0;
  return (d1 - d0) / d0 * PINCH_GAIN;
}
/* keyDebounce: stick jitter across the 0.35 threshold re-presses A/D/S fast, which the game reads as a double-tap (roll / flip).
   press(code, now) -> true when this press must be sent with repeat:true (the game then sets the key but skips tap detection).
   A deliberate second press after REPEAT_MS goes through as a normal keydown. State: last press time per code. */
export function keyDebounce(ms) {
  var win = ms === undefined ? REPEAT_MS : ms, last = Object.create(null);
  return {
    press: function (code, now) { var r = code in last && now - last[code] < win; last[code] = now; return r; },
    reset: function () { last = Object.create(null); }
  };
}
/* pollGate: the slow sync poll runs only while a touch happened in the last IDLE_MS.
   touch(now) -> true when the poll was stopped and must be (re)started; tick(now) -> true to keep polling, false to stop. */
export function pollGate(idle) {
  var lim = idle === undefined ? IDLE_MS : idle, last = 0, on = false;
  return {
    touch: function (now) { last = now; if (on) return false; on = true; return true; },
    tick: function (now) { if (!on) return false; if (now - last >= lim) { on = false; return false; } return true; },
    running: function () { return on; }
  };
}
export function isTouchDevice() {
  try { return !!(typeof window !== 'undefined' && window.matchMedia && window.matchMedia('(pointer: coarse)').matches); } catch (e) { return false; }
}

/* createTouch(host): host = { active(): bool, surface: HTMLElement (the renderer canvas), key(code, down), mouse(button, down), look(dx, dy), throttle(d) }.
   Returns { dispose, sync }. sync() re-reads host.active() and shows/hides the overlay. The module runs its own slow poll (POLL_MS) of sync,
   stopped after IDLE_MS without a touch and restarted by the next touchstart (pollGate). */
export function createTouch(host) {
  var cv = document.createElement('canvas'), cx = cv.getContext('2d'), W = 0, H = 0, dpr = 1, pads = [], dirty = true, raf = 0, shown = false;
  var stick = null;                       // { id, ox, oy, x, y, keys }
  var looks = {};                         // pointerId -> { x, y }
  var padHold = {};                       // pointerId -> pad
  var pinch = null;                       // { a, b, d }
  var held = Object.create(null);         // code -> true while our synthetic key is down
  var deb = keyDebounce(), gate = pollGate(), timer = 0;
  cv.style.cssText = 'position:fixed;left:0;top:0;width:100vw;height:100vh;pointer-events:none;z-index:40;display:none';
  document.body.appendChild(cv);
  try { host.surface.style.touchAction = 'none'; } catch (e0) { /* ignore */ }

  function resize() {
    W = window.innerWidth; H = window.innerHeight; dpr = Math.min(2, window.devicePixelRatio || 1);
    cv.width = Math.round(W * dpr); cv.height = Math.round(H * dpr); pads = padLayout(W, H); dirty = true; want();
  }
  function want() { if (dirty && !raf && shown) raf = requestAnimationFrame(draw); }
  function circle(x, y, r, fill, line) { cx.beginPath(); cx.arc(x, y, r, 0, 6.2832); if (fill) { cx.fillStyle = fill; cx.fill(); } if (line) { cx.strokeStyle = line; cx.lineWidth = 2; cx.stroke(); } }
  function draw() {
    raf = 0; if (!dirty || !shown) return; dirty = false;
    var i, p, on, held2 = false;
    cx.setTransform(dpr, 0, 0, dpr, 0, 0); cx.clearRect(0, 0, W, H);
    cx.font = Math.round(Math.min(W, H) * 0.03) + "px Monocraft, ui-monospace, monospace"; cx.textAlign = 'center'; cx.textBaseline = 'middle';
    for (i = 0; i < pads.length; i++) {
      p = pads[i]; on = !!(held[p.code]);
      circle(p.x, p.y, p.r, on ? 'rgba(168,85,247,0.35)' : 'rgba(13,2,33,0.72)', on ? '#A855F7' : 'rgba(168,85,247,0.45)');
      cx.fillStyle = '#E0AAFF'; cx.fillText(p.label, p.x, p.y);
    }
    if (stick) {
      var R = Math.min(W, H) * STICK_R, kx = stick.x - stick.ox, ky = stick.y - stick.oy, l = Math.sqrt(kx * kx + ky * ky);
      if (l > R) { kx *= R / l; ky *= R / l; }
      circle(stick.ox, stick.oy, R, 'rgba(13,2,33,0.55)', 'rgba(168,85,247,0.45)');
      circle(stick.ox + kx, stick.oy + ky, R * 0.42, 'rgba(13,2,33,0.72)', '#A855F7');
    }
  }
  function mark() { dirty = true; want(); }

  function setKey(code, down, stickKey) {
    if (!!held[code] === down) return;
    held[code] = down;
    host.key(code, down, down && stickKey ? deb.press(code, performance.now()) : false);
  }
  function setStickKeys(k) { setKey('KeyW', k.KeyW, 1); setKey('KeyS', k.KeyS, 1); setKey('KeyA', k.KeyA, 1); setKey('KeyD', k.KeyD, 1); }
  function pollTick() { if (!gate.tick(performance.now())) { clearInterval(timer); timer = 0; return; } api.sync(); }
  function wake() {
    if (gate.touch(performance.now())) { if (timer) clearInterval(timer); timer = setInterval(pollTick, POLL_MS); }
    api.sync();
  }
  function padDown(p) {
    if (p.mouse !== undefined) { held[p.code] = true; host.mouse(p.mouse, true); } else setKey(p.code, true);
  }
  function padUp(p) {
    if (p.mouse !== undefined) { held[p.code] = false; host.mouse(p.mouse, false); } else setKey(p.code, false);
  }
  function releaseAll() {
    var k, i;
    for (i = 0; i < pads.length; i++) if (held[pads[i].code]) padUp(pads[i]);
    for (k in held) if (held[k]) setKey(k, false);
    stick = null; looks = {}; padHold = {}; pinch = null; deb.reset(); mark();
  }
  function pinchTwo() {
    var ids = Object.keys(looks);
    if (ids.length < 2) return null;
    var a = looks[ids[0]], b = looks[ids[1]];
    return Math.sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
  }

  function onDown(e) {
    if (e.pointerType !== 'touch') return;
    wake();
    if (!shown || !host.active()) return;
    if (e.target !== host.surface) return;                         // overlays (inventory, menus, chat) keep their own native taps
    var p = hitPad(pads, e.clientX, e.clientY), id = e.pointerId;
    e.preventDefault();
    if (p) { padHold[id] = p; padDown(p); mark(); return; }
    if (e.clientX < W * 0.45 && !stick) { stick = { id: id, ox: e.clientX, oy: e.clientY, x: e.clientX, y: e.clientY }; mark(); return; }
    looks[id] = { x: e.clientX, y: e.clientY };
    if (Object.keys(looks).length === 2) pinch = { d: pinchTwo() };
  }
  function onMove(e) {
    if (e.pointerType !== 'touch') return;
    var id = e.pointerId, l, d, dx, dy;
    if (stick && stick.id === id) {
      stick.x = e.clientX; stick.y = e.clientY;
      setStickKeys(stickKeys(stickVec(stick.x - stick.ox, stick.y - stick.oy, Math.min(W, H) * STICK_R)));
      mark(); return;
    }
    l = looks[id];
    if (!l) return;
    dx = e.clientX - l.x; dy = e.clientY - l.y; l.x = e.clientX; l.y = e.clientY;
    if (pinch) { d = pinchTwo(); if (d !== null) { host.throttle(pinchThrottle(pinch.d, d)); pinch.d = d; } return; }
    host.look(dx * LOOK_GAIN, dy * LOOK_GAIN);
  }
  function onUp(e) {
    if (e.pointerType !== 'touch') return;
    var id = e.pointerId;
    if (padHold[id]) { padUp(padHold[id]); delete padHold[id]; mark(); }
    if (stick && stick.id === id) { stick = null; setStickKeys({ KeyW: false, KeyS: false, KeyA: false, KeyD: false }); mark(); }
    if (looks[id]) { delete looks[id]; pinch = null; }
  }
  window.addEventListener('pointerdown', onDown, { capture: true, passive: false });
  window.addEventListener('pointermove', onMove, true);
  window.addEventListener('pointerup', onUp, true);
  window.addEventListener('pointercancel', onUp, true);
  window.addEventListener('resize', resize);
  window.addEventListener('blur', releaseAll);
  resize();

  var api = {
    sync: function () {
      var a = !!host.active();
      if (a === shown) return;
      shown = a; cv.style.display = a ? 'block' : 'none';
      if (!a) releaseAll(); else { dirty = true; want(); }
    },
    dispose: function () {
      releaseAll(); if (timer) clearInterval(timer); timer = 0;
      window.removeEventListener('pointerdown', onDown, true); window.removeEventListener('pointermove', onMove, true);
      window.removeEventListener('pointerup', onUp, true); window.removeEventListener('pointercancel', onUp, true);
      window.removeEventListener('resize', resize); window.removeEventListener('blur', releaseAll);
      if (raf) cancelAnimationFrame(raf);
      if (cv.parentNode) cv.parentNode.removeChild(cv);
    }
  };
  wake();
  return api;
}

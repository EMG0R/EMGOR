// GORMAPPER sync — editor ↔ output live link via BroadcastChannel "gormapper",
// with a 2s polling fallback against /api/scene/active when BC is unavailable.
//
// Message shapes (both directions use the same channel):
//   { type: 'scene',    name, scene, editMode, selectedId }   editor → output
//   { type: 'edit',     editMode, selectedId }                editor → output
//   { type: 'activate', name }                                editor → output
//   { type: 'hello' }                                         output → editor
//     (a freshly opened output asks the editor to re-send current state)

const CHANNEL = 'gormapper';

export function createSync(handlers) {
  const h = handlers || {};
  let bc = null;
  if (typeof BroadcastChannel !== 'undefined') {
    bc = new BroadcastChannel(CHANNEL);
    bc.onmessage = (ev) => {
      const msg = ev.data;
      if (!msg || typeof msg !== 'object') return;
      if (msg.type === 'scene' && h.onScene) h.onScene(msg);
      else if (msg.type === 'edit' && h.onEdit) h.onEdit(msg);
      else if (msg.type === 'activate' && h.onActivate) h.onActivate(msg);
      else if (msg.type === 'hello' && h.onHello) h.onHello(msg);
    };
  }

  function post(msg) {
    if (bc) bc.postMessage(msg);
  }

  // throttled scene broadcast (~30/s while dragging)
  let pending = null;
  let lastSent = 0;
  let timer = null;
  function sendScene(name, scene, editMode, selectedId) {
    pending = { type: 'scene', name, scene, editMode, selectedId };
    const now = performance.now();
    const wait = Math.max(0, 33 - (now - lastSent));
    if (timer) return;
    timer = setTimeout(() => {
      timer = null;
      lastSent = performance.now();
      post(pending);
      pending = null;
    }, wait);
  }

  function sendEdit(editMode, selectedId) {
    post({ type: 'edit', editMode, selectedId });
  }

  function sendActivate(name) {
    post({ type: 'activate', name });
  }

  function sendHello() {
    post({ type: 'hello' });
  }

  return { available: !!bc, sendScene, sendEdit, sendActivate, sendHello };
}

// Throttled push of live edit state to the server (~25/s max), which
// SSE-broadcasts it to every output window. This is what keeps the Electron
// overlay (a separate browser — no BroadcastChannel) following the editor in
// real time. Returns push(payload) where payload = {name, scene, editMode,
// selectedId} — the same shape as the 'scene' BroadcastChannel message.
export function createLivePush() {
  let pending = null;
  let inFlight = false;
  let timer = null;

  async function flush() {
    timer = null;
    if (!pending || inFlight) return;
    const body = JSON.stringify(pending);
    pending = null;
    inFlight = true;
    try {
      await fetch('/api/live', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body,
      });
    } catch (e) { /* server down — next edit retries */ }
    inFlight = false;
    if (pending && !timer) timer = setTimeout(flush, 40);
  }

  return function push(payload) {
    pending = payload;
    if (!timer) timer = setTimeout(flush, 40);
  };
}

// Server-sent events from /api/events: pattern hot-reload + scene activation.
// handler receives { type: 'pattern', id, exists } | { type: 'active', name }
//                | { type: 'live', name, scene, editMode, selectedId }.
// EventSource reconnects automatically if the server restarts.
export function subscribeEvents(handler) {
  if (typeof EventSource === 'undefined') return null;
  const es = new EventSource('/api/events');
  es.onmessage = (ev) => {
    try {
      const msg = JSON.parse(ev.data);
      if (msg && msg.type) handler(msg);
    } catch (e) { /* ignore malformed */ }
  };
  return es;
}

// Output-side fallback: poll the active scene every `ms` when BroadcastChannel
// is unavailable. Calls onScene({name, scene}) whenever the JSON changes.
export function startActivePolling(onScene, ms) {
  let lastJson = null;
  async function tick() {
    try {
      const r = await fetch('/api/scene/active');
      if (r.ok) {
        const data = await r.json();
        const json = JSON.stringify(data);
        if (json !== lastJson) {
          lastJson = json;
          onScene(data);
        }
      }
    } catch (e) { /* server briefly down — keep polling */ }
  }
  tick();
  return setInterval(tick, ms || 2000);
}

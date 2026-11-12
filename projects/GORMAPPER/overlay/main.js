// GORMAPPER — standalone Mac app. One double-click runs the whole stack:
//   · the HTTP/SSE server, in-process (server.js required as a module from
//     GORMAPPER_HOME — the live repo files, so pattern hot-reload, scene JSON
//     and git tracking keep working; nothing is frozen into the bundle)
//   · the transparent click-through overlay on the projector display
//   · a normal editor window on the primary display (replaces Chrome)
//   · the cursor fence helper (Cmd+Alt+F toggles)
//
//   npm start                      dev run
//   npm run dist                   package → ../dist/GORMAPPER.app
//   --display=N --no-fence --smoke same flags as before
//
// GORMAPPER_HOME env var (or "home" in config.json) overrides the repo path.
// Closing the editor window keeps the app running — reopen from the Dock or
// Cmd+Shift+E. Cmd+Q quits everything (server, fence, windows).

'use strict';

const { app, BrowserWindow, screen, globalShortcut, dialog, Menu } = require('electron');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const http = require('http');

const DEFAULT_HOME = '/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/gormapper';
const PORT = 8711;
const SERVER = 'http://localhost:' + PORT;

// ---- config -----------------------------------------------------------------

function loadConfig() {
  const cfg = {
    display: null,
    url: SERVER + '/output?mode=overlay',
    fence: true,
    smoke: false,
    home: process.env.GORMAPPER_HOME || DEFAULT_HOME,
  };
  try {
    Object.assign(cfg, JSON.parse(fs.readFileSync(path.join(__dirname, 'config.json'), 'utf8')));
  } catch (e) { /* no config.json — fine */ }
  if (process.env.GORMAPPER_HOME) cfg.home = process.env.GORMAPPER_HOME;
  for (const arg of process.argv.slice(1)) {
    const m = /^--display=(\d+)$/.exec(arg);
    if (m) cfg.display = parseInt(m[1], 10);
    else if (arg === '--no-fence') cfg.fence = false;
    else if (arg === '--smoke') cfg.smoke = true;
    else if (arg.startsWith('--url=')) cfg.url = arg.slice(6);
    else if (arg.startsWith('--home=')) cfg.home = arg.slice(7);
  }
  return cfg;
}

const cfg = loadConfig();
const log = (...a) => console.log('[overlay]', ...a);

// only one GORMAPPER app at a time; a second launch focuses the editor
if (!app.requestSingleInstanceLock()) {
  app.quit();
} else {
  app.on('second-instance', () => createEditorWindow());
}

// ---- embedded server --------------------------------------------------------

let serverMode = null;   // 'started' | 'reused'

function startBackend() {
  return new Promise((resolve, reject) => {
    const serverPath = path.join(cfg.home, 'server.js');
    if (!fs.existsSync(serverPath)) {
      dialog.showErrorBox('GORMAPPER home not found',
        'Expected the gormapper repo at:\n\n' + cfg.home
        + '\n\nserver.js is missing there. Set the GORMAPPER_HOME environment'
        + ' variable (or "home" in overlay/config.json) to the repo path.');
      app.quit();
      reject(new Error('GORMAPPER_HOME missing'));
      return;
    }
    let backend;
    try {
      backend = require(serverPath);   // live repo file — nothing bundled
    } catch (e) {
      dialog.showErrorBox('GORMAPPER server failed to load', String(e.stack || e));
      app.quit();
      reject(e);
      return;
    }
    backend.start(PORT, (err, mode) => {
      if (err) {
        dialog.showErrorBox('GORMAPPER server failed to start', err.message);
        app.quit();
        reject(err);
        return;
      }
      serverMode = mode;
      log('server ' + mode + ' (home: ' + cfg.home + ')');
      resolve(mode);
    });
  });
}

// ---- tiny HTTP helper -------------------------------------------------------

function getJSON(urlPath) {
  return new Promise((resolve) => {
    http.get(SERVER + urlPath, (res) => {
      let body = '';
      res.on('data', (c) => { body += c; });
      res.on('end', () => {
        try { resolve(JSON.parse(body)); } catch (e) { resolve(null); }
      });
    }).on('error', () => resolve(null));
  });
}

// ---- display selection ------------------------------------------------------

function pickDisplay() {
  const displays = screen.getAllDisplays();
  const primary = screen.getPrimaryDisplay();
  displays.forEach((d, i) => {
    log(`display ${i}: ${d.bounds.width}x${d.bounds.height} at (${d.bounds.x},${d.bounds.y})`
      + (d.id === primary.id ? ' [primary]' : ''));
  });
  if (cfg.display != null && displays[cfg.display]) return displays[cfg.display];
  return displays.find((d) => d.id !== primary.id) || primary;
}

// ---- overlay window ---------------------------------------------------------

let overlayWin = null;

function createOverlayWindow(display) {
  const b = display.bounds;
  overlayWin = new BrowserWindow({
    x: b.x, y: b.y, width: b.width, height: b.height,
    frame: false,
    transparent: true,
    hasShadow: false,
    resizable: false,
    movable: false,
    focusable: false,           // never steals focus; hotkeys are global
    fullscreenable: false,
    skipTaskbar: true,
    show: false,
    webPreferences: { backgroundThrottling: false },
  });
  overlayWin.setAlwaysOnTop(true, 'screen-saver');
  overlayWin.setVisibleOnAllWorkspaces(true, { visibleOnFullScreen: true });
  overlayWin.setIgnoreMouseEvents(true);   // fully click-through

  overlayWin.webContents.on('did-finish-load', () => {
    log('overlay page loaded:', cfg.url);
    overlayWin.show();
  });
  overlayWin.webContents.on('did-fail-load', (ev, code, desc) => {
    log(`overlay page load failed (${desc}) — retrying in 2s`);
    setTimeout(() => { if (overlayWin && !overlayWin.isDestroyed()) overlayWin.loadURL(cfg.url); }, 2000);
  });
  overlayWin.webContents.on('console-message', (ev, level, message) => {
    if (message.includes('[gormapper]')) log('overlay page:', message);
  });
  overlayWin.loadURL(cfg.url);
  log(`overlay window on display at (${b.x},${b.y}) ${b.width}x${b.height}, click-through, screen-saver level`);
}

// ---- editor window (replaces Chrome) ----------------------------------------

let editorWin = null;

function createEditorWindow() {
  if (editorWin && !editorWin.isDestroyed()) {
    editorWin.show();
    editorWin.focus();
    return;
  }
  const wa = screen.getPrimaryDisplay().workArea;
  editorWin = new BrowserWindow({
    x: wa.x + 40, y: wa.y + 40,
    width: Math.min(1500, wa.width - 80),
    height: Math.min(960, wa.height - 80),
    title: 'GORMAPPER',
    webPreferences: { backgroundThrottling: false },
  });
  editorWin.loadURL(SERVER + '/editor');
  editorWin.webContents.on('did-finish-load', () => log('editor window loaded'));
  // the editor page's unsaved-changes beforeunload guard silently vetoes
  // window closing in Electron (no dialog is shown). Autosave already
  // protects every edit in the app, so always allow the close to proceed.
  editorWin.webContents.on('will-prevent-unload', (ev) => ev.preventDefault());
  // closing the editor window quits the whole app — software closed means
  // the wall goes dark (overlay, server, fence all stop together)
  editorWin.on('closed', () => { editorWin = null; app.quit(); });
  log('editor window on primary display');
}

// ---- menu -------------------------------------------------------------------
// Note: no undo/redo roles in Edit — the editor page owns Cmd+Z / Shift+Cmd+Z
// (a menu accelerator would swallow the key before the page sees it).

function buildMenu() {
  Menu.setApplicationMenu(Menu.buildFromTemplate([
    {
      label: 'GORMAPPER',
      submenu: [
        { label: 'About GORMAPPER', role: 'about' },
        { type: 'separator' },
        { label: 'Toggle Cursor Fence', accelerator: 'CommandOrControl+Alt+F', click: toggleFence },
        { type: 'separator' },
        { role: 'hide' }, { role: 'hideOthers' },
        { type: 'separator' },
        { role: 'quit' },
      ],
    },
    {
      label: 'Edit',
      submenu: [
        { role: 'cut' }, { role: 'copy' }, { role: 'paste' }, { role: 'selectAll' },
      ],
    },
    {
      label: 'Window',
      submenu: [
        { label: 'Editor', accelerator: 'CommandOrControl+Shift+E', click: createEditorWindow },
        { role: 'minimize' }, { role: 'close' },
      ],
    },
  ]));
}

// ---- cursor fence -----------------------------------------------------------

let fenceProc = null;
let fenceEnabled = cfg.fence;
let fenceDisplay = null;
let lastRect = null;
let fenceSendTimer = null;

function startFence(display) {
  fenceDisplay = display;
  if (!cfg.fence) { log('cursor fence disabled (--no-fence)'); return; }
  const bin = path.join(__dirname, 'fence', 'fence');
  if (!fs.existsSync(bin)) {
    log('cursor fence helper not built — run: sh overlay/fence/build.sh (needs swiftc). Continuing without fence.');
    fenceEnabled = false;
    return;
  }
  fenceProc = spawn(bin, [], { stdio: ['pipe', 'inherit', 'pipe'] });
  fenceProc.stderr.on('data', (d) => {
    const s = d.toString();
    process.stderr.write('[fence] ' + s);
    if (s.includes('FENCE_PANIC_OFF')) {
      // panic key M: stay disarmed until an explicit Cmd+Alt+F — a later SSE
      // scene update must NOT re-arm on its own
      fenceEnabled = false;
      log('fence disabled by panic key M — Cmd+Alt+F re-enables');
    }
    if (s.includes('FENCE_NO_PERMISSION')) {
      log('--------------------------------------------------------------');
      log('The cursor fence needs the one-time Accessibility permission:');
      log('  System Settings → Privacy & Security → Accessibility');
      log('  → enable GORMAPPER, then relaunch the app.');
      log('The overlay keeps running fine without the fence.');
      log('--------------------------------------------------------------');
    }
  });
  fenceProc.on('exit', (code) => {
    log('fence helper exited' + (code ? ' (code ' + code + ')' : ''));
    fenceProc = null;
  });
  log('cursor fence helper started (Cmd+Alt+F toggles)');
}

function fenceWrite(obj) {
  if (!fenceProc || !fenceProc.stdin.writable) return;
  try { fenceProc.stdin.write(JSON.stringify(obj) + '\n'); } catch (e) { /* died */ }
}

// scene coords (scene.width x scene.height) → CG global points. Verified for
// this arrangement (primary 1512x982@2x at 0,0; projector 1920x1080@1x at
// -99,-1080): Electron display.bounds and CGDisplayBounds/CGEvent locations
// are the SAME space (points, origin top-left of primary, y down) — no
// conversion needed, including negative origins. `fence --print-mouse`
// re-proves it on any arrangement.
function computeFenceRect(scene) {
  if (!scene) return null;
  if (scene.scene && !Array.isArray(scene.layers)) scene = scene.scene;   // draft-shaped
  if (!Array.isArray(scene.layers)) return null;
  const zone = scene.layers.find((l) =>
    l.visible !== false && l.content && l.content.kind === 'screen'
    && Array.isArray(l.points) && l.points.length >= 3);
  if (!zone) return null;
  let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
  for (const [x, y] of zone.points) {
    if (x < x0) x0 = x; if (y < y0) y0 = y;
    if (x > x1) x1 = x; if (y > y1) y1 = y;
  }
  const b = fenceDisplay.bounds;
  const sx = b.width / (scene.width || 1920);
  const sy = b.height / (scene.height || 1080);
  return {
    sceneBox: { x: x0, y: y0, w: x1 - x0, h: y1 - y0 },
    x: b.x + x0 * sx, y: b.y + y0 * sy,
    w: (x1 - x0) * sx, h: (y1 - y0) * sy,
  };
}

// Escape corridor: the strip from the zone edge that faces the primary
// display out to that display edge, as wide as the zone itself ("exit
// straight out of the screen area"). The fence region = zone ∪ corridor, so
// the cursor can always travel home but never into the other decorated sides.
function computeCorridor(rect) {
  const b = fenceDisplay.bounds;
  const p = screen.getPrimaryDisplay().bounds;
  if (fenceDisplay.id === screen.getPrimaryDisplay().id) return { corridor: null, edge: 'none' };
  const dx = (p.x + p.width / 2) - (b.x + b.width / 2);
  const dy = (p.y + p.height / 2) - (b.y + b.height / 2);
  let edge, corridor = null;
  if (Math.abs(dy) >= Math.abs(dx)) {
    edge = dy > 0 ? 'bottom' : 'top';
    const h = edge === 'bottom' ? (b.y + b.height) - (rect.y + rect.h) : rect.y - b.y;
    if (h > 0) {
      corridor = { x: rect.x, y: edge === 'bottom' ? rect.y + rect.h : b.y, w: rect.w, h };
    }
  } else {
    edge = dx > 0 ? 'right' : 'left';
    const w = edge === 'right' ? (b.x + b.width) - (rect.x + rect.w) : rect.x - b.x;
    if (w > 0) {
      corridor = { x: edge === 'right' ? rect.x + rect.w : b.x, y: rect.y, w, h: rect.h };
    }
  }
  return { corridor, edge };
}

function updateFence(scene, immediate) {
  if (!fenceDisplay) return;
  lastRect = computeFenceRect(scene);
  clearTimeout(fenceSendTimer);
  if (immediate) sendFenceState();
  else fenceSendTimer = setTimeout(sendFenceState, 100);   // debounce drag streams
}

const fr = (r) => r ? `(${Math.round(r.x)},${Math.round(r.y)} ${Math.round(r.w)}x${Math.round(r.h)})` : 'none';

function sendFenceState() {
  fenceSendTimer = null;
  const b = fenceDisplay.bounds;
  const geom = lastRect
    ? 'scene bbox ' + fr(lastRect.sceneBox) + ' -> global zone ' + fr(lastRect)
    : 'no screen zone in scene';
  if (!fenceEnabled || !lastRect) {
    log('fence send: DISARMED (' + (!fenceEnabled ? 'toggled off' : 'no screen zone') + ') — ' + geom);
    fenceWrite({ enabled: false });
    return;
  }
  const { corridor, edge } = computeCorridor(lastRect);
  log('fence send: ' + geom
    + '  corridor ' + fr(corridor) + ' (exit edge: ' + edge + ')'
    + '  display ' + fr({ x: b.x, y: b.y, w: b.width, h: b.height }));
  fenceWrite({
    display: { x: b.x, y: b.y, w: b.width, h: b.height },
    rect: { x: lastRect.x, y: lastRect.y, w: lastRect.w, h: lastRect.h },
    corridor,
  });
}

function toggleFence() {
  fenceEnabled = !fenceEnabled;
  log('cursor fence ' + (fenceEnabled ? 'ON' : 'OFF'));
  sendFenceState();
}

// ---- follow the scene (SSE from the server) ---------------------------------

async function loadActiveSceneForFence() {
  const live = await getJSON('/api/live');
  if (live && live.scene) {
    log('fence source: live edit state (' + (live.name || '?') + ')');
    updateFence(live.scene, true);          // send immediately — no debounce
    return;
  }
  const data = await getJSON('/api/scene/active');
  if (data && data.scene) {
    log('fence source: active scene "' + data.name + '"');
    updateFence(data.scene, true);          // send immediately — no debounce
  } else {
    log('fence source: no scene available yet');
  }
}

function subscribeSSE() {
  const req = http.get(SERVER + '/api/events', (res) => {
    log('SSE connected — following live edits + scene switches');
    let buf = '';
    res.on('data', (chunk) => {
      buf += chunk.toString();
      let i;
      while ((i = buf.indexOf('\n\n')) >= 0) {
        const block = buf.slice(0, i);
        buf = buf.slice(i + 2);
        const line = block.split('\n').find((l) => l.startsWith('data: '));
        if (!line) continue;
        let msg = null;
        try { msg = JSON.parse(line.slice(6)); } catch (e) { continue; }
        if (!msg) continue;
        if (msg.type === 'live' && msg.scene) updateFence(msg.scene);
        else if (msg.type === 'active') loadActiveSceneForFence();
      }
    });
    res.on('end', retrySSE);
  });
  req.on('error', retrySSE);
}

let sseRetryTimer = null;
function retrySSE() {
  if (sseRetryTimer) return;
  sseRetryTimer = setTimeout(() => { sseRetryTimer = null; subscribeSSE(); }, 2000);
}

// ---- smartGOR lightserver presence ------------------------------------------
// While GORMAPPER is open we keepalive the lightserver's canvas tap so it
// streams the room LED ring (UDP :21325 -> /api/ring -> "LED ring" zones).
// The tap self-expires (ttl 15s) if we vanish, and we DELETE it on quit — the
// Pi never wastes a cycle rendering for a projector that isn't on.

const LIGHTSERVER_HOST = 'gorcave.local';
const LIGHTSERVER_PORT = 8800;
let tapTimer = null;
let tapWasUp = null;   // tri-state so we only log transitions

function tapRequest(method, body, cb) {
  const data = body ? JSON.stringify(body) : null;
  const req = require('http').request({
    host: LIGHTSERVER_HOST, port: LIGHTSERVER_PORT,
    path: '/taps/gormapper', method,
    headers: data ? { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(data) } : {},
    timeout: 3000,
  }, (res) => { res.resume(); if (cb) cb(res.statusCode >= 200 && res.statusCode < 300); });
  req.on('error', () => { if (cb) cb(false); });
  req.on('timeout', () => { req.destroy(); if (cb) cb(false); });
  if (data) req.write(data);
  req.end();
}

function startTapKeepalive() {
  const beat = () => tapRequest('POST', { port: 21325, semantic_port: 21326, fps_divisor: 2, ttl: 15 }, (ok) => {
    if (ok !== tapWasUp) {
      log(ok ? 'lightserver tap: streaming room ring -> LED ring zones'
             : 'lightserver unreachable — LED ring zones dark (retrying quietly)');
      tapWasUp = ok;
    }
  });
  beat();
  tapTimer = setInterval(beat, 5000);
}

function stopTapKeepalive() {
  clearInterval(tapTimer);
  tapTimer = null;
  tapRequest('DELETE', null, () => {});   // best-effort; ttl covers the rest
}

// ---- app --------------------------------------------------------------------

app.whenReady().then(async () => {
  try {
    await startBackend();
  } catch (e) {
    return;   // dialog already shown, app quitting
  }
  buildMenu();
  const display = pickDisplay();
  createOverlayWindow(display);
  createEditorWindow();
  startFence(display);
  await loadActiveSceneForFence();
  subscribeSSE();

  if (!globalShortcut.register('CommandOrControl+Alt+F', toggleFence)) {
    log('could not register Cmd+Alt+F for the fence toggle');
  }
  startTapKeepalive();

  if (cfg.smoke) {
    log('smoke mode — quitting in 8s');
    setTimeout(() => {
      log('smoke OK: server=' + serverMode
        + ' overlay=' + (overlayWin && !overlayWin.isDestroyed())
        + ' editor=' + (editorWin && !editorWin.isDestroyed())
        + ' fence=' + (fenceProc ? 'running' : 'off'));
      app.quit();
    }, 8000);
  }
});

// dock click (or reopening the app) brings the editor back
app.on('activate', () => {
  if (app.isReady()) createEditorWindow();
});

// closing the editor leaves the overlay running — never quit here
app.on('window-all-closed', () => { app.quit(); });

app.on('will-quit', () => {
  globalShortcut.unregisterAll();
  if (fenceProc) { try { fenceProc.stdin.end(); } catch (e) { /* ignore */ } }
  stopTapKeepalive();   // tell the Pi to stop rendering for us right away
  // in-process server dies with the app; nothing else to clean up
});

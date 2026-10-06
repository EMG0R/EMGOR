/* ship-net.js — NO MANS GOR multiplayer client. See docs/multiplayer-plan.md.
   Lazy-imported by ship.js after mount. Knows nothing about ship.js internals:
   everything it needs comes through `hooks`:
     getState() 'docked'|'piloting'|'away'      getPose() {x,y,z,qx,qy,qz,qw,v,st,hp}
     getPrefs() {name,color}                    L (ship length, world units)
     buildGhost() Group                         tint(group, hexInt)
     dockSlot(angle, outPos, outQuat)           enter()
     onRemoteFire(id, w, ox,oy,oz, dx,dy,dz)    onKill(victimName, byMe)
     galaxyScale (docked ghost hull length)     onChat(from, text)   onGor(from, text)
   Client-authoritative, relay is a dumb fan-out. With nobody else online (or the
   relay unreachable) the game is the single-player game: no waits, no HUD errors. */

const PROD_URL = 'wss://gorcave.taila85593.ts.net/nmg';
const LOCAL_URL = 'ws://127.0.0.1:8796';
const DEFAULT_COLOR = 0x8a5cff;
const RENDER_DELAY = 150, EXTRAP_MAX = 300, SNAP_DIST = 50000;
const CONNECT_TIMEOUT = 2000, RETRY_MS = 60000, RETRY_LONG_MS = 300000, FADE_MS = 3000;
const POS_MS = 100, HB_MS = 5000, FIRE_MIN_MS = 100;

let current = null;     // the one live instance (module-level wrappers below)

function readId() {
    var id = '';
    try { id = localStorage.getItem('nmg-id') || ''; } catch (e) { /* storage unavailable */ }
    if (!/^[A-Za-z0-9]{12}$/.test(id)) {
        id = '';
        var cs = 'abcdefghijklmnopqrstuvwxyz0123456789';
        for (var i = 0; i < 12; i++) id += cs[Math.floor(Math.random() * cs.length)];
        try { localStorage.setItem('nmg-id', id); } catch (e) { /* ignore */ }
    }
    // dev hook: ?mpA / ?mpB makes a distinct id per tab of one browser profile (test only)
    var m = /[?&]mp([A-Za-z0-9]{1,6})/.exec(location.search);
    if (m) id = id.slice(0, 12 - m[1].length) + m[1];
    return id;
}
function relayUrl() {
    var h = location.hostname;
    var local = h === 'localhost' || h === '127.0.0.1';
    if (local) {
        try { var o = localStorage.getItem('nmg-relay'); if (o && /^wss?:\/\//.test(o)) return o; } catch (e) { /* ignore */ }
        return LOCAL_URL;
    }
    return PROD_URL;
}
function hex6(c) { return '#' + ('000000' + (c >>> 0).toString(16)).slice(-6); }
function r2(v) { return Math.round(v * 100) / 100; }
function r4(v) { return Math.round(v * 10000) / 10000; }

export function connect(engine, hooks) {
    if (current) current.destroy();
    var THREE = engine.THREE, scene = engine.scene, camera = engine.camera;
    var id = readId();
    var prefs = (hooks.getPrefs && hooks.getPrefs(id)) || {};
    var myName = prefs.name || ('PILOT-' + id.slice(-4)).toUpperCase();
    var myColor = prefs.color != null ? prefs.color : DEFAULT_COLOR;
    var myHs = typeof prefs.hs === 'string' ? prefs.hs.slice(0, 40) : '';     // rev 26: hull signature (ship-hull hullSignature) so ghosts rebuild the same upgraded hull
    var myStyle = typeof prefs.style === 'string' ? prefs.style.slice(0, 16) : '';      // rev 30: character style id ('' = PILOT); rides in hi + foot pos so ghosts re-skin
    var myState = 'docked', myMode = 'fly';      // rev 14: myMode fly | landed | foot (sub-state of 'piloting')

    var ws = null, open = false, kicked = false, destroyed = false, fails = 0;
    var retryTimer = 0, hbTimer = 0, lastAttempt = 0, lastSent = 0, lastPos = 0, lastFire = 0, infoLogged = false;
    var offset = 0, offsetOk = false, bestRtt = 1e9, epoch = 0, clockOn = false;
    var fadeAt = 0;                       // set when the link drops: ghosts fade over FADE_MS then are disposed
    var rosterCbs = [];
    var ghosts = new Map(), sorted = [];

    // ─── DOM pip ────────────────────────────────────────────────
    var pip = document.createElement('div');
    pip.className = 'ship-online'; pip.style.display = 'none';
    document.body.appendChild(pip);
    var pipN = 0;
    function updatePip() {
        var n = open ? ghosts.size + 1 : 0;
        if (n === pipN) return;
        pipN = n;
        if (n > 1) { pip.textContent = 'ONLINE ' + n; pip.style.display = ''; } else pip.style.display = 'none';
    }

    // ─── ghosts ─────────────────────────────────────────────────
    var glowTex = (function () {
        var cv = document.createElement('canvas'); cv.width = cv.height = 64;
        var c = cv.getContext('2d'), rg = c.createRadialGradient(32, 32, 0, 32, 32, 32);
        rg.addColorStop(0, 'rgba(255,255,255,1)'); rg.addColorStop(0.3, 'rgba(255,255,255,0.55)'); rg.addColorStop(1, 'rgba(255,255,255,0)');
        c.fillStyle = rg; c.fillRect(0, 0, 64, 64);
        return new THREE.CanvasTexture(cv);
    })();
    var vP = new THREE.Vector3(), qP = new THREE.Quaternion(), vF = new THREE.Vector3(), qQ = new THREE.Quaternion();

    function makeGhost(rec) {
        var g = { id: rec.id, name: '', color: -1, s: 'docked', hp: 100, hpShown: 100, st: 0, samples: [], fsamples: [], mode: 'fly', human: null, mk2: null, root: new THREE.Group(), hull: null, glow: null, label: null, nameEl: null, pipU: null, shown: false, lblShown: true, lx: -1e9, ly: -1e9, placed: false, sc: 0 };
        g.root.name = 'ship-ghost'; g.root.visible = false;
        try { g.hull = hooks.buildGhost(); } catch (e) { g.hull = null; }
        if (g.hull) { g.hull.traverse(function (o) { o.frustumCulled = false; }); g.root.add(g.hull); }
        g.glow = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, blending: THREE.AdditiveBlending, depthTest: true, depthWrite: false, transparent: true, color: DEFAULT_COLOR }));
        g.glow.frustumCulled = false;
        g.root.add(g.glow);
        scene.add(g.root);
        var b = document.createElement('button');
        b.className = 'ship-ghost-label'; b.type = 'button';
        b.innerHTML = '<span></span><i><u></u></i>';
        g.nameEl = b.firstChild; g.pipU = b.querySelector('u');
        document.body.appendChild(b);
        g.label = b;
        return g;
    }
    function disposeGhost(g) {
        scene.remove(g.root);
        if (g.human) { scene.remove(g.human.group); try { g.human.dispose(); } catch (e) { /* ignore */ } g.human = null; }
        g.root.traverse(function (o) {
            if (o.geometry) o.geometry.dispose();
            if (o.material) { if (o.material.map === glowTex) o.material.map = null; o.material.dispose(); }
        });
        if (g.label.parentNode) g.label.parentNode.removeChild(g.label);
    }
    function applyLook(g, rec) {
        var nm = rec.name || g.name || 'PILOT';
        if (typeof rec.style === 'string') setGhostStyle(g, rec.style.slice(0, 16));
        if (typeof rec.hs === 'string' && rec.hs !== (g.hs || '') && hooks.buildGhost) {      // rev 26: the sender's upgraded hull
            g.hs = rec.hs;
            try {
                var nh = hooks.buildGhost(rec.hs);
                if (nh) { if (g.hull) { g.root.remove(g.hull); g.hull.traverse(function (o) { if (o.geometry) o.geometry.dispose(); }); } g.hull = nh; nh.traverse(function (o) { o.frustumCulled = false; }); g.root.add(nh); if (g.color >= 0) hooks.tint(nh, g.color); }
            } catch (e) { /* keep the current hull */ }
        }
        if (nm !== g.name) { g.name = nm; g.nameEl.textContent = nm; }
        if (rec.color != null && rec.color !== g.color) {
            g.color = rec.color;
            if (g.hull) { try { hooks.tint(g.hull, g.color); } catch (e) { /* keep default livery */ } }
            g.glow.material.color.setHex(g.color);
            g.label.style.color = hex6(g.color); g.label.style.borderColor = hex6(g.color) + '66';
        }
    }
    function pushFoot(g, p, now) {          // rev 14: on-foot players: a separate sample list so their landed ship stays where it parked
        var last = g.fsamples[g.fsamples.length - 1];
        if (last) { var dx = p.x - last.x, dy = p.y - last.y, dz = p.z - last.z; if (dx * dx + dy * dy + dz * dz > SNAP_DIST * SNAP_DIST) g.fsamples.length = 0; }
        g.fsamples.push({ t: now, x: p.x, y: p.y, z: p.z, qx: p.qx, qy: p.qy, qz: p.qz, qw: p.qw, v: 0 });
        if (g.fsamples.length > 3) g.fsamples.shift();
        g.fst = p.st | 0;
    }
    function pushSample(g, p, now) {
        var last = g.samples[g.samples.length - 1];
        if (last) {
            var dx = p.x - last.x, dy = p.y - last.y, dz = p.z - last.z;
            if (dx * dx + dy * dy + dz * dz > SNAP_DIST * SNAP_DIST) g.samples.length = 0;     // teleport/respawn: no lerp across it
        }
        g.samples.push({ t: now, x: p.x, y: p.y, z: p.z, qx: p.qx, qy: p.qy, qz: p.qz, qw: p.qw, v: g.sf ? 0 : (p.v || 0) });
        if (g.samples.length > 3) g.samples.shift();
        g.st = p.st | 0;
        if (typeof p.hp === 'number') g.hp = p.hp;
    }
    // interpolated pose at render time rt into vP/qP
    function sampleAt(g, rt, arr) {
        var s = arr || g.samples, n = s.length, a, b, k;
        if (n === 1 || rt <= s[0].t) { a = s[0]; vP.set(a.x, a.y, a.z); qP.set(a.qx, a.qy, a.qz, a.qw); return; }
        b = s[n - 1];
        if (rt >= b.t) {
            // buffer ran dry: extrapolate along forward by speed, max 300 ms, then freeze
            var dt = Math.min(rt - b.t, EXTRAP_MAX) / 1000;
            qP.set(b.qx, b.qy, b.qz, b.qw);
            vF.set(0, 0, -1).applyQuaternion(qP);
            vP.set(b.x, b.y, b.z).addScaledVector(vF, b.v * dt);
            return;
        }
        for (var i = n - 2; i >= 0; i--) { if (s[i].t <= rt) { a = s[i]; b = s[i + 1]; break; } }
        if (!a) { a = s[0]; b = s[1]; }
        k = (rt - a.t) / Math.max(1, b.t - a.t);
        vP.set(a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, a.z + (b.z - a.z) * k);
        qP.set(a.qx, a.qy, a.qz, a.qw);
        qQ.set(b.qx, b.qy, b.qz, b.qw);
        qP.slerp(qQ, k);
    }

    var tmpPos = new THREE.Vector3(), tmpQuat = new THREE.Quaternion();
    function update(dt, dockA) {
        if (!ghosts.size) { if (pip.style.display !== 'none') updatePip(); return; }
        var now = performance.now(), L = hooks.L || 1;
        var fade = 1;
        if (fadeAt) {
            fade = 1 - (now - fadeAt) / FADE_MS;
            if (fade <= 0) { dropGhosts(); return; }
        }
        var n = sorted.length, rt = now - RENDER_DELAY, W = window.innerWidth, H = window.innerHeight;
        for (var i = 0; i < n; i++) {
            var g = ghosts.get(sorted[i]);
            if (!g) continue;
            var flying = g.s === 'piloting' && g.samples.length > 0;
            if (flying) {
                sampleAt(g, rt); tmpPos.copy(vP); tmpQuat.copy(qP);
                if (g.sf && !(hooks.stationToWorld && hooks.stationToWorld(tmpPos, tmpQuat))) { g.root.visible = false; if (g.lblShown) { g.lblShown = false; g.label.style.display = 'none'; } continue; }
            }
            else { hooks.dockSlot((dockA || 0) + 2 * Math.PI * (i + 1) / (n + 1), tmpPos, tmpQuat); }
            var foot = g.mode === 'foot' && g.fsamples.length > 0 && hooks.buildHuman;
            var dead = flying && !foot && (g.st & 4) !== 0;
            var gl = foot ? (g.fst & 8) : (flying ? (g.st & 8) : 0);      // rev 23: pose bit 8 = fries glow (light blue)
            g.root.position.copy(tmpPos); g.root.quaternion.copy(tmpQuat);
            // minimum on-screen size so a ghost reads from the orbit camera too
            var dist = camera.position.distanceTo(tmpPos);
            // rev 8: galaxy scale while their owner is docked/away, true L (with the min on-screen rule) while flying; log-lerped between
            var want = flying ? Math.max(L, dist * 0.008) : (hooks.galaxyScale || L);
            g.sc = g.sc > 0 ? Math.exp(Math.log(g.sc) + (Math.log(want) - Math.log(g.sc)) * Math.min(1, 4 * Math.max(dt, 0.016))) : want;
            var sc = g.sc;
            g.root.scale.setScalar(sc);
            g.glow.scale.setScalar(Math.max(0.5 * L, dist * 0.03) / sc);
            g.glow.material.opacity = fade * (dead ? 0 : 0.9);
            if ((gl && !foot) !== !!g.glHull) { g.glHull = !!(gl && !foot); g.glow.material.color.setHex(g.glHull ? 0x7FD8FF : (g.color >= 0 ? g.color : DEFAULT_COLOR)); }
            if (g.glHull) { g.glow.scale.multiplyScalar(2.4); g.glow.material.opacity = fade * (dead ? 0 : 1); }
            var vis = !dead;
            if (g.hull) g.hull.visible = vis && fade > 0.35;       // opaque shader hull: cannot alpha-fade, so drop it early
            g.root.visible = true;
            // rev 14: on foot -> a humanoid at their own pose (0.09 L tall; never smaller than a few pixels), label + mark follow the human
            if (foot) {
                if (!g.human) { try { g.human = hooks.buildHuman(g.color >= 0 ? g.color : DEFAULT_COLOR, g.style || ''); scene.add(g.human.group); } catch (e) { g.human = null; } }
                if (g.human) {
                    sampleAt(g, rt, g.fsamples);
                    if (g.sf && hooks.stationToWorld) hooks.stationToWorld(vP, qP);
                    var hs = Math.max(0.09 * L, camera.position.distanceTo(vP) * 0.004);
                    g.human.group.position.copy(vP); g.human.group.quaternion.copy(qP); g.human.group.scale.setScalar(hs);
                    g.human.group.visible = fade > 0.35;
                    var fs = g.fst | 0;
                    g.human.update(dt, { moving: !!(fs & 1), running: !!(fs & 2), airborne: !!(fs & 4), speed: (fs & 2) ? 1 : ((fs & 1) ? 0.5 : 0), facing: 0 });
                    g.mk2 = g.human.group; tmpPos.copy(vP);
                    g.glow.material.opacity = 0;
                    if (gl) {
                        if (!g.hglow) { g.hglow = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, color: 0x7FD8FF })); g.hglow.position.set(0, 0.55, 0); g.hglow.scale.setScalar(2.6); g.hglow.frustumCulled = false; g.human.group.add(g.hglow); }
                        g.hglow.visible = true;
                    } else if (g.hglow) g.hglow.visible = false;
                }
            } else {
                if (g.human) g.human.group.visible = false;
                g.mk2 = null;
            }
            g.hpShown += (g.hp - g.hpShown) * Math.min(1, 6 * Math.max(dt, 0.016));
            // label
            vP.copy(tmpPos).project(camera);
            var show = vP.z < 1 && vP.z > -1 && Math.abs(vP.x) < 1.1 && Math.abs(vP.y) < 1.1;
            if (show !== g.lblShown) { g.lblShown = show; g.label.style.display = show ? '' : 'none'; }
            if (show) {
                var x = (vP.x * 0.5 + 0.5) * W, y = (-vP.y * 0.5 + 0.5) * H;
                if (Math.abs(x - g.lx) >= 0.1 || Math.abs(y - g.ly) >= 0.1) {
                    g.lx = x; g.ly = y;
                    g.label.style.transform = 'translate(' + x.toFixed(1) + 'px,' + y.toFixed(1) + 'px) translate(-50%, 18px)';
                }
                g.label.style.opacity = (fade < 1 || dead) ? String(fade * (dead ? 0.35 : 1)) : '';
                g.pipU.style.transform = 'scaleX(' + Math.max(0, Math.min(1, g.hpShown / 100)).toFixed(3) + ')';
                g.pipU.parentNode.style.display = flying ? '' : 'none';
            }
        }
        updatePip();
    }

    function rebuildSorted() { sorted = Array.from(ghosts.keys()).sort(); }
    function dropGhosts() {
        ghosts.forEach(disposeGhost); ghosts.clear(); sorted = []; fadeAt = 0; updatePip();
    }

    // ─── clock ──────────────────────────────────────────────────
    function sharedNow() { return (Date.now() + offset - epoch) / 1000; }
    // The shared orbital clock is a single SNAP of engine time to the relay's epoch clock, never a slew (a slew
    // whirls every orbit). It only happens where a cut is invisible: before the first frames / during the intro,
    // or at a ship enter()/exit() transition (setState below). Otherwise it stays pending.
    function maybeClock(transition) {
        if (clockOn || !offsetOk || !epoch || ghosts.size < 1 || typeof engine.setClock !== 'function') return;
        var G = window.EMGOR_GALAXY, st = G && G.state;
        if (!transition && st && !st.intro && st.frame >= 2) return;
        clockOn = true;
        engine.setClock(sharedNow);                      // time = (relay clock); camera/yaw/pitch untouched
        if (hooks.onClockSnap) { try { hooks.onClockSnap(); } catch (e) { /* ignore */ } }
    }
    function clockOff() { if (clockOn) { clockOn = false; if (typeof engine.setClock === 'function') engine.setClock(null); } }

    // ─── socket ─────────────────────────────────────────────────
    function send(o) {
        if (!open || !ws) return false;
        try { ws.send(JSON.stringify(o)); lastSent = performance.now(); return true; } catch (e) { return false; }
    }
    function sendHi() { var o = { t: 'hi', v: 1, id: id, name: myName, color: myColor }; if (myHs) o.hs = myHs; if (myStyle) o.style = myStyle; send(o); }
    function setStyle(s) { s = typeof s === 'string' ? s.slice(0, 16) : ''; if (s === myStyle) return; myStyle = s; lastPos = 0; sendHi(); }
    function setGhostStyle(g, s) { s = s || ''; if (s === (g.style || '')) return; g.style = s; if (g.human && hooks.skinHuman) { try { hooks.skinHuman(g.human, s); } catch (e) { /* keep the current skin */ } } }
    function setHullSig(sig) { sig = typeof sig === 'string' ? sig.slice(0, 40) : ''; if (sig === myHs) return; myHs = sig; sendHi(); }

    function schedule(ms) {
        clearTimeout(retryTimer);
        if (destroyed || kicked) return;
        retryTimer = setTimeout(attempt, ms);
    }
    function attempt() {
        if (destroyed || kicked || open || ws) return;
        lastAttempt = performance.now();
        var a = { sock: null, done: false, timer: 0, wasOpen: false };
        function lost(code) {
            if (a.done) return;
            a.done = true; clearTimeout(a.timer);
            var was = a.wasOpen;
            if (ws === a.sock) ws = null;
            open = false;
            try { if (a.sock.readyState < 2) a.sock.close(); } catch (e) { /* ignore */ }
            clockOff();
            if (code === 4000) {                           // same id opened elsewhere: this tab goes single-player for good
                kicked = true;
                if (!infoLogged) { infoLogged = true; console.info('[ship-net] this pilot id is online in another tab; single-player here'); }
            } else {
                fails = was ? 0 : fails + 1;
                if (!infoLogged) { infoLogged = true; console.info('[ship-net] relay unavailable, playing single-player'); }
                schedule(fails >= 3 ? RETRY_LONG_MS : RETRY_MS);
            }
            if (ghosts.size && !fadeAt) fadeAt = performance.now();
            if (!ghosts.size) updatePip();
        }
        var sock;
        try { sock = new WebSocket(relayUrl()); } catch (e) { a.sock = { readyState: 3, close: function () {} }; lost(0); return; }
        a.sock = sock; ws = sock;
        a.timer = setTimeout(function () { if (!a.wasOpen) lost(0); }, CONNECT_TIMEOUT);
        sock.onopen = function () {
            if (a.done) return;
            clearTimeout(a.timer);
            a.wasOpen = true; open = true; fails = 0; fadeAt = 0; lastSent = performance.now();
            offsetOk = false; bestRtt = 1e9;
            sendHi();
            if (hooks.onOpen) { try { hooks.onOpen(); } catch (e) { /* ignore */ } }
            [0, 400, 800].forEach(function (d) { setTimeout(function () { send({ t: 'ping', c: performance.now() }); }, d); });
            if (myState === 'piloting') lastPos = 0;
        };
        sock.onmessage = function (ev) {
            if (a.done || typeof ev.data !== 'string') return;
            var m; try { m = JSON.parse(ev.data); } catch (e) { return; }
            if (m && typeof m.t === 'string') onMsg(m);
        };
        sock.onerror = function () { /* onclose follows */ };
        sock.onclose = function (ev) { lost(ev && ev.code); };
    }

    function onMsg(m) {
        var now = performance.now(), g;
        switch (m.t) {
            case 'roster': {
                if (typeof m.epoch === 'number') epoch = m.epoch;
                var seen = {}, ships = Array.isArray(m.ships) ? m.ships : [];
                for (var i = 0; i < ships.length; i++) {
                    var r = ships[i];
                    if (!r || typeof r.id !== 'string' || r.id === id) continue;
                    seen[r.id] = 1;
                    g = ghosts.get(r.id);
                    if (!g) { g = makeGhost(r); ghosts.set(r.id, g); }
                    applyLook(g, r);
                    g.s = r.s === 'piloting' ? 'piloting' : (r.s === 'away' ? 'away' : 'docked');
                    if (g.s === 'piloting' && typeof r.x === 'number' && !g.samples.length) pushSample(g, r, now);
                    if (g.s !== 'piloting') { g.samples.length = 0; g.fsamples.length = 0; g.mode = 'fly'; }
                }
                ghosts.forEach(function (gg, gid) { if (!seen[gid]) { disposeGhost(gg); ghosts.delete(gid); } });
                fadeAt = 0;
                rebuildSorted();
                maybeClock();
                for (var k = 0; k < rosterCbs.length; k++) { try { rosterCbs[k](Array.from(ghosts.values())); } catch (e) { /* ignore */ } }
                break;
            }
            case 'pos':
                g = ghosts.get(m.id);
                if (!g) return;
                if (g.s !== 'piloting') g.s = 'piloting';
                var sf = m.v === -1 || m.frame === 'station';      // rev 22: station frame (x/y/z are station-local L units; the relay cannot carry a frame id, v = -1 is the flag)
                if (!!g.sf !== sf) { g.samples.length = 0; g.fsamples.length = 0; }
                g.sf = sf;
                g.mode = m.mode === 'foot' ? 'foot' : (m.mode === 'landed' ? 'landed' : 'fly');
                if (g.mode === 'foot') setGhostStyle(g, typeof m.style === 'string' ? m.style.slice(0, 16) : '');
                if (g.mode === 'foot') pushFoot(g, m, now); else { g.fsamples.length = 0; pushSample(g, m, now); }
                break;
            case 'chat':
                if (typeof m.text === 'string' && hooks.onChat) hooks.onChat(String(m.from || 'PILOT').slice(0, 24), m.text.slice(0, 200));
                break;
            case 'gor':
                if (m.from === 'neptr' && typeof m.text === 'string' && hooks.onGor) hooks.onGor('neptr', m.text.slice(0, 600));
                break;
            case 'bye':
                g = ghosts.get(m.id);
                if (g) { disposeGhost(g); ghosts.delete(m.id); rebuildSorted(); }
                break;
            case 'fire':
                if (m.id !== id && hooks.onRemoteFire) hooks.onRemoteFire(m.id, m.w | 0, m.x, m.y, m.z, m.dx, m.dy, m.dz);
                break;
            case 'kill':
                if (m.id !== id && hooks.onKill) {
                    g = ghosts.get(m.id);
                    hooks.onKill(g ? g.name : 'PILOT', m.by === id);
                }
                break;
            case 'prof.ok': if (hooks.onProf) hooks.onProf({ ok: true, name: m.name }); break;
            case 'prof.err': if (hooks.onProf) hooks.onProf({ ok: false, why: String(m.why || 'bad') }); break;
            case 'prof': if (hooks.onProf && m.data && typeof m.data === 'object') hooks.onProf({ ok: true, loaded: true, name: m.name, data: m.data }); break;
            case 'disc': if (hooks.onDisc) hooks.onDisc({ kind: m.kind, id: m.id, name: m.name, by: m.by, at: m.at, won: true }); break;
            case 'disc.no': if (hooks.onDisc) hooks.onDisc({ kind: m.kind, id: m.id, name: m.name, by: m.by, won: false }); break;
            case 'disc.all': if (hooks.onDisc && Array.isArray(m.list)) hooks.onDisc({ list: m.list }); break;
            case 'event': if (hooks.onEvent) hooks.onEvent({ kind: String(m.kind || ''), minutes: +m.minutes || 0, left: +m.left || 0, planetId: m.planetId }); break;
            case 'pong': {
                var rtt = now - m.c;
                if (rtt >= 0 && rtt < bestRtt && typeof m.now === 'number') { bestRtt = rtt; offset = m.now + rtt / 2 - Date.now(); offsetOk = true; maybeClock(); }
                break;
            }
        }
    }

    // ─── public ─────────────────────────────────────────────────
    function sendPos() {
        if (!open || myState !== 'piloting') return;
        var now = performance.now();
        if (now - lastPos < POS_MS) return;
        lastPos = now;
        var p = hooks.getPose();
        var o = { t: 'pos', id: id, x: r2(p.x), y: r2(p.y), z: r2(p.z), qx: r4(p.qx), qy: r4(p.qy), qz: r4(p.qz), qw: r4(p.qw), v: Math.round(p.v * 10) / 10, st: p.st | 0, hp: Math.max(0, Math.min(255, Math.round(p.hp))) };
        if (p.v === -1) o.frame = 'station';
        if (myMode !== 'fly') o.mode = myMode;          // rev 14: 'landed' | 'foot' (foot: x/y/z/q are the HUMAN's pose, st bits 1 moving 2 running 4 airborne)
        if (myMode === 'foot') { o.x = r4(p.x); o.y = r4(p.y); o.z = r4(p.z); if (myStyle) { o.style = myStyle; o.st = (o.st | 16); } }
        send(o);
    }
    function setMode(m) { myMode = m === 'foot' || m === 'landed' ? m : 'fly'; lastPos = 0; }
    function setState(s) {
        myState = s;
        if (s !== 'piloting') myMode = 'fly';
        maybeClock(true);                                  // enter/exit is the invisible cut for a pending clock snap
        if (s === 'piloting') { lastPos = 0; return; }
        send({ t: 'st', s: s === 'away' ? 'away' : 'docked' });
    }
    function sendFire(w, ox, oy, oz, dx, dy, dz) {
        if (!open) return;
        var now = performance.now();
        if (now - lastFire < FIRE_MIN_MS) return;
        lastFire = now;
        send({ t: 'fire', id: id, w: w | 0, x: r2(ox), y: r2(oy), z: r2(oz), dx: r4(dx), dy: r4(dy), dz: r4(dz) });
    }
    function sendKill(by) { send({ t: 'kill', by: by }); }
    function sendChat(text) { return send({ t: 'chat', text: String(text).slice(0, 200) }); }
    // rev 27: relay persistence + shared state (server/nmg-relay/README.md). All return false when offline.
    function profSave(name, key, data) { return send({ t: 'prof.save', name: String(name).toLowerCase(), key: String(key), data: data }); }
    function profLoad(name, key) { return send({ t: 'prof.load', name: String(name).toLowerCase(), key: String(key) }); }
    function discClaim(kind, did, name, by) { return send({ t: 'disc.claim', kind: kind, id: String(did).slice(0, 48), name: String(name).slice(0, 24), by: String(by).slice(0, 24) }); }
    function discList() { return send({ t: 'disc.list' }); }
    function eventNow() { return send({ t: 'event.now' }); }
    function sendGor(text) { return send({ t: 'gor', text: String(text).slice(0, 400) }); }
    function setName(n) { myName = String(n); sendHi(); }
    function setColor(c) { myColor = c & 0xffffff; sendHi(); }
    function onRoster(fn) { if (typeof fn === 'function') rosterCbs.push(fn); }
    function destroy() {
        if (destroyed) return;
        destroyed = true;
        clearTimeout(retryTimer); clearInterval(hbTimer);
        document.removeEventListener('visibilitychange', onVis);
        var s = ws; ws = null; open = false;
        if (s) { try { s.close(); } catch (e) { /* ignore */ } }
        clockOff(); dropGhosts();
        if (pip.parentNode) pip.parentNode.removeChild(pip);
        if (current === net) current = null;
    }

    hbTimer = setInterval(function () {
        if (open && performance.now() - lastSent >= HB_MS) send({ t: 'hb' });
    }, 1000);
    function onVis() {
        if (document.hidden || destroyed || kicked || open || ws) return;
        if (fails === 0 || performance.now() - lastAttempt >= RETRY_MS) { clearTimeout(retryTimer); attempt(); }   // fails===0: relay dropped us while the tab was throttled
    }
    document.addEventListener('visibilitychange', onVis);

    var net = {
        get offline() { return !open; },
        get online() { return open; },
        get id() { return id; },
        get ghosts() { return ghosts; },
        get clockOn() { return clockOn; },
        update: update, sendPos: sendPos, setState: setState, setMode: setMode, sendFire: sendFire, sendKill: sendKill, sendChat: sendChat, sendGor: sendGor, setHullSig: setHullSig, setStyle: setStyle, profSave: profSave, profLoad: profLoad, discClaim: discClaim, discList: discList, eventNow: eventNow,
        setName: setName, setColor: setColor, onRoster: onRoster, destroy: destroy
    };
    current = net;
    attempt();
    return net;
}

// thin module-level wrappers (plan's export list); they act on the live instance
export function setHullSig(sig) { if (current && current.setHullSig) current.setHullSig(sig); }
export function setStyle(s) { if (current && current.setStyle) current.setStyle(s); }
export function sendPos() { if (current) current.sendPos(); }
export function onRoster(fn) { if (current) current.onRoster(fn); }
export function setName(n) { if (current) current.setName(n); }
export function setColor(c) { if (current) current.setColor(c); }
export function destroy() { if (current) current.destroy(); }

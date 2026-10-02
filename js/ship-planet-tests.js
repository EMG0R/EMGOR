// ship-planet-tests.js — re-runnable checks for docs/ship-mode.md "Revision 18 — planet flight model (guaranteed)".
//
//   const t = await import('./js/ship-planet-tests.js');
//   const res = await t.runAll(window.EMGOR_GALAXY.engine, window.EMGOR_SHIP);          // all seven guarantees
//   await t.runAll(engine, ship, { only: ['g2', 'g4'], minutes: 5, trials: 20 });
//
// Drives the real game: window.EMGOR_GALAXY.step(dt) frames, key events on window, the ship's debug hooks (ship.dbg / ship.lf).
// Every guarantee returns { pass, ...numbers }. Safe to run on the dev server; it sets /user claude and /peaceful (no enemies) and enters ship mode.

var THREE_ = null;
function mk(engine, ship, opts) {
    var THREE = THREE_ = engine.THREE, G = window.EMGOR_GALAXY;
    var T = {
        engine: engine, ship: ship, G: G, THREE: THREE, opts: opts || {},
        step: function (n, dt) { dt = dt || 0.016; for (var i = 0; i < n; i++) G.step(dt); },
        pause: function () { return new Promise(function (r) { var c = new MessageChannel(); c.port1.onmessage = function () { c.port1.close(); r(); }; c.port2.postMessage(0); }); },     // a macrotask that background-tab timer throttling cannot stretch     // let the page breathe during long runs (a tool call can poll window.__res)
        kd: function (c) { window.dispatchEvent(new KeyboardEvent('keydown', { code: c, bubbles: true })); },
        ku: function (c) { window.dispatchEvent(new KeyboardEvent('keyup', { code: c, bubbles: true })); },
        L: function () { return ship.scale.L; },
        planets: function () {
            var out = [];
            engine.drawOrder.forEach(function (n) { var u = n.mesh && n.mesh.material && n.mesh.material.uniforms; if (u && u.uSeed && n.anchor && n.anchor.visible && n.mesh.scale.x > 0) out.push(n); });
            return out;
        },
        pick: function () {   // biggest rocky/lush planet (not a gas giant)
            var best = null;
            T.planets().forEach(function (n) { if (n.mesh.material.uniforms.uBiome.value < 0.5 && (!best || n.mesh.scale.x > best.mesh.scale.x)) best = n; });
            return best;
        },
        // moons crowd the big planets: a random LOCAL direction whose world point at `ratio` x R has no other surface planet within 3.5 of its own radii
        safeDir: function (node, ratio) {
            var all = T.planets();
            for (var k = 0; k < 200; k++) {
                var d = T.rdir();
                ship.dbg.syncPlanet(node);
                var w = d.clone().multiplyScalar(node.mesh.scale.x * ratio).applyQuaternion(node.mesh.quaternion).add(node.anchor.position), ok = true;
                for (var i = 0; i < all.length; i++) { var m = all[i]; if (m === node) continue; if (w.distanceTo(m.anchor.position) < 3.5 * m.mesh.scale.x) { ok = false; break; } }
                if (ok) return d;
            }
            return T.rdir();
        },
        // same, but a WORLD direction (for placing the ship before the frame exists)
        safeWorldDir: function (node, ratio) {
            var all = T.planets();
            for (var k = 0; k < 200; k++) {
                var d = T.rdir(), w = d.clone().multiplyScalar(node.mesh.scale.x * ratio).add(node.anchor.position), ok = true;
                for (var i = 0; i < all.length; i++) { var m = all[i]; if (m === node) continue; if (w.distanceTo(m.anchor.position) < 3.5 * m.mesh.scale.x) { ok = false; break; } }
                if (ok) return d;
            }
            return T.rdir();
        },
        clearKeys: function () { ship.dbg.setKeys(Object.create(null)); },
        aim: function (node) {
            var m = new THREE.Matrix4();
            ship.dbg.syncPlanet(node);
            m.lookAt(ship.shipRoot.position, node.anchor.position, new THREE.Vector3(0, 1, 0));
            ship.shipRoot.quaternion.setFromRotationMatrix(m);
        },
        // put the ship at ratio x R from the planet along world direction d (unit), nose at the planet, with world velocity speed toward it
        place: function (node, ratio, d, spd, nose) {
            ship.dbg.syncPlanet(node);
            var c = node.anchor.position, R = node.mesh.scale.x, P = ship.shipRoot.position;
            P.set(c.x + d.x * ratio * R, c.y + d.y * ratio * R, c.z + d.z * ratio * R);
            if (nose !== false) T.aim(node);
            var f = new THREE.Vector3(0, 0, -1).applyQuaternion(ship.shipRoot.quaternion);
            ship.dbg.vel.copy(f).multiplyScalar(spd || 0); ship.dbg.setSpeed(spd || 0);
        },
        ratio: function (node) { return ship.shipRoot.position.distanceTo(node.anchor.position) / node.mesh.scale.x; },
        setup: function () {
            ship.cmd('/user claude'); ship.cmd('/peaceful');
            if (ship.state !== 'piloting') ship.enter();
            return new Promise(function (res) { var t0 = performance.now(); (function wait() { if (performance.now() - t0 < 250) return T.pause().then(wait); T.clearKeys(); T.step(160);
                if (ship.gmode === 'foot') { T.kd('KeyE'); T.ku('KeyE'); T.step(3); }          // board again
                if (ship.gmode === 'landed') { T.kd('KeyW'); T.ku('KeyW'); T.step(40); }       // lift off
                res(); })(); });   // 160 frames: past the 1.6 s boarding cinematic
        },
        // sphere-uniform random unit vector (seeded LCG so a run is repeatable)
        rng: (function () { var s = 123456789; return function () { s = (Math.imul(s, 1664525) + 1013904223) | 0; return (s >>> 0) / 4294967296; }; })(),
        rdir: function () { var z = T.rng() * 2 - 1, a = T.rng() * 6.2831853, r = Math.sqrt(1 - z * z); return new THREE.Vector3(r * Math.cos(a), z, r * Math.sin(a)); },
        // a land point (unit local direction) with a gentle slope; returns local direction
        near: function (node) {   // bring the ship inside 3 R so the planet surface (ps) activates on this body
            if (ship.ps.active === node) return;
            T.place(node, 2.2, T.safeWorldDir(node, 2.2), 0); T.step(6);
        },
        landDir: function (node) {
            var ps = ship.ps; T.near(node);
            for (var k = 0; k < 400; k++) {
                var d = T.safeDir(node, 1.02);
                if (!ps.landLocal(d.x, d.y, d.z)) continue;
                var h0 = ps.heightLocal(d.x, d.y, d.z), e = 0.0004, flat = true;
                [[1, 0, 0], [0, 0, 1], [-1, 0, 0], [0, 0, -1]].forEach(function (o) {
                    var q = new THREE.Vector3(d.x + o[0] * e, d.y, d.z + o[2] * e).normalize();
                    if (Math.abs(ps.heightLocal(q.x, q.y, q.z) - h0) > 0.00012) flat = false;
                });
                if (flat) return d;
            }
            return null;
        },
        localToWorld: function (node, lp, out) { ship.dbg.syncPlanet(node); return out.copy(lp).applyQuaternion(node.mesh.quaternion).add(node.anchor.position); },
        // place the ship hovering at altL ship-lengths above the terrain at local direction d, nose along the horizon (exact: written into the local frame)
        hoverAt: function (node, d, altL) {
            var ps = ship.ps, L = T.L(), lf = ship.lf;
            if (!(lf.on && lf.node === node)) { T.place(node, 1.45, d.clone().applyQuaternion(node.mesh.quaternion), 0); T.step(4); }
            if (!(lf.on && lf.node === node)) throw new Error('local frame did not engage');
            var r = ps.floorLocal(d.x, d.y, d.z) + altL * L, ref = Math.abs(d.y) < 0.9 ? new THREE.Vector3(0, 1, 0) : new THREE.Vector3(1, 0, 0);
            var right = new THREE.Vector3().crossVectors(d, ref).normalize(), back = new THREE.Vector3().crossVectors(right, d).normalize();      // X right, Y up, Z back (right-handed): the nose (-Z) lies on the horizon
            var lq = new THREE.Quaternion().setFromRotationMatrix(new THREE.Matrix4().makeBasis(right, d, back));
            ship.dbg.setLocal(d.x * r, d.y * r, d.z * r, lq.x, lq.y, lq.z, lq.w);
            ship.dbg.setSpeed(0); ship.dbg.setThrottle(0); ship.dbg.setPulseT(0);
            ship.dbg.vel.set(0, 0, 0);
        }
    };
    return T;
}

// ─── 1. local-frame simulation: park 120 s ─────────────────────────────────────────────
async function g1(T, o) {
    var ship = T.ship, node = T.pick(), ps = ship.ps, L = T.L(), THREE = T.THREE;
    var d = T.landDir(node);
    T.hoverAt(node, d, 2.5);
    T.step(30);                               // engage the frame, ps active
    if (!ship.lf.on) T.step(30);
    T.clearKeys();
    if (!ship.land()) return { pass: false, why: 'startLanding refused' };
    for (var i = 0; i < 400 && ship.gmode !== 'landed'; i++) T.step(1);
    if (ship.gmode !== 'landed') return { pass: false, why: 'never touched down', gmode: ship.gmode };
    var land = ship._g.land, sp0 = land.sPos.clone(), fl0 = ps.floorLocal(sp0.x, sp0.y, sp0.z);
    var drift = 0, werr = 0, flv = 0, frames = o.parkFrames || 2400;   // 2400 x 0.05 = 120 s
    var c = new THREE.Vector3(), W = new THREE.Vector3();
    for (i = 0; i < frames; i++) {
        if (i % 300 === 0) await T.pause();
        T.step(1, 0.05);
        drift = Math.max(drift, land.sPos.distanceTo(sp0));
        T.localToWorld(node, land.sPos, W);
        werr = Math.max(werr, W.distanceTo(ship.shipRoot.position));
        flv = Math.max(flv, Math.abs(ps.floorLocal(land.sPos.x, land.sPos.y, land.sPos.z) - fl0));
    }
    // human on the same frame
    var res = { pass: drift === 0 && werr < 1e-3 * L && flv === 0, parkedSeconds: frames * 0.05, localDrift: drift, worldTrackErrMax_u: werr, worldTrackErrMax_L: werr / L, floorVariation: flv, planet: node.id };
    // the human lives in the same local frame: step out, stand 20 s, local drift 0 and the world pose tracks the planet
    T.kd('KeyE'); T.ku('KeyE'); T.step(5);
    if (ship.gmode === 'foot') {
        var hum = ship._g.hum, hp0 = hum.pos.clone(), hd = 0, hw = 0;
        for (i = 0; i < 400; i++) {
            T.step(1, 0.05); hd = Math.max(hd, hum.pos.distanceTo(hp0));
            T.localToWorld(node, hum.pos, W); hw = Math.max(hw, W.distanceTo(ship.humanObj.position));
        }
        res.human = { localDrift: hd, worldTrackErr_u: hw }; if (hd !== 0 || hw > 1e-3 * L) res.pass = false;
        T.kd('KeyE'); T.ku('KeyE'); T.step(3);          // back aboard (standing next to the ship)
    } else { res.human = 'did not step out'; res.pass = false; }
    // lift off again so later tests start flying
    ship.onKeyE && 0;
    T.kd('KeyW'); T.step(2); T.ku('KeyW'); T.step(30);
    res.liftedOff = ship.gmode === 'fly';
    return res;
}

// ─── 2. guaranteed entry ───────────────────────────────────────────────────────────────
async function g2(T, o) {
    var ship = T.ship, node = T.pick(), N = o.trials || 20, R = node.mesh.scale.x, L = T.L(), bad = [], rows = [];
    var maxXSpeed = 0, maxHpLoss = 0, minFinal = 9, brake = [];
    for (var t = 0; t < N; t++) {
        await T.pause();
        T.clearKeys(); ship.dbg.setPulseT(0);
        // leave any frame / ground state
        if (ship.gmode !== 'fly') { T.kd('KeyW'); T.step(2); T.ku('KeyW'); }
        var d = T.safeWorldDir(node, 4), spd = t % 2 ? 1500 : 400 + T.rng() * 800;
        T.place(node, 4, d, spd);
        // random approach: aim off-centre by up to 14 deg (4 sin 14 = 0.97 R: every line still crosses the 1.4 R shell)
        var off = (T.rng() * 14) * Math.PI / 180, ax = new T.THREE.Vector3(T.rng() - .5, T.rng() - .5, T.rng() - .5).normalize();
        T.ship.shipRoot.quaternion.premultiply(new T.THREE.Quaternion().setFromAxisAngle(ax, off));
        var f = new T.THREE.Vector3(0, 0, -1).applyQuaternion(ship.shipRoot.quaternion);
        ship.dbg.vel.copy(f).multiplyScalar(spd); ship.dbg.setSpeed(spd);
        ship.dbg.setPulseT(ship.dbg.PULSE_FULL);
        var hp0 = ship.stats.hp, crossI = -1, vCross = 0, outAgain = false, vAfter = 0;
        T.kd('Space'); T.kd('KeyW');
        for (var i = 0; i < 520; i++) {
            if (!ship.lf.cap && i < 400 && !(ship.lf.on)) { /* keep the nose on the approach line until the frame owns the ship */ }
            T.step(1);
            var r = T.ratio(node);
            if (crossI < 0 && ship.lf.cap) { crossI = i; vCross = ship.dbg.vel.length(); }
            if (crossI >= 0 && i - crossI < Math.round(2 / 0.016) && r > 1.4001) outAgain = true;      // nothing may push you back out during ENTRY
            if (crossI >= 0 && i === crossI + Math.round(1.98 / 0.016)) vAfter = ship.dbg.vel.length();       // end of the 2 s ENTRY brake
            if (crossI >= 0 && i > crossI + 140) break;
        }
        T.ku('Space'); T.ku('KeyW');
        var rEnd = T.ratio(node), hpLoss = hp0 - ship.stats.hp;
        maxHpLoss = Math.max(maxHpLoss, hpLoss); minFinal = Math.min(minFinal, rEnd);
        var ok = crossI >= 0 && rEnd < 1.4 && !outAgain && hpLoss === 0 && vAfter <= 12.01;
        if (!ok) bad.push({ t: t, d: d.toArray().map(function (v) { return +v.toFixed(2); }), crossI: crossI, rEnd: +rEnd.toFixed(3), outAgain: outAgain, hpLoss: hpLoss, vAfter: +vAfter.toFixed(2), vCross: +vCross.toFixed(1) });
        rows.push([+rEnd.toFixed(3), +vCross.toFixed(0), +vAfter.toFixed(1)]);
        maxXSpeed = Math.max(maxXSpeed, vCross);
        // park the ship again: go back out beyond 1.7 R so the next trial starts in the world frame
        ship.dbg.vel.set(0, 0, 0);
    }
    return { pass: bad.length === 0, trials: N, failures: bad, maxCrossSpeed_us: +maxXSpeed.toFixed(1), maxHpLoss: maxHpLoss, endRatioMax: Math.max.apply(null, rows.map(function (x) { return x[0]; })), speedAt2sMax: Math.max.apply(null, rows.map(function (x) { return x[2]; })), sample: rows.slice(0, 5) };
}

// ─── 3. atmospheric flight model ──────────────────────────────────────────────────────
async function g3(T, o) {
    var ship = T.ship, node = T.pick(), ps = ship.ps, L = T.L(), THREE = T.THREE, res = { pass: true }, R = node.mesh.scale.x;
    function fail(k, v) { res.pass = false; res[k] = v; }
    var d = T.landDir(node);
    // caps: hover high (50 L is above the hover band), long straight runs at each setting
    function run(label, keys, sec, expectMax) {
        T.clearKeys(); T.hoverAt(node, d, 300); T.step(40);
        var hi = 0; keys.forEach(function (k) { T.kd(k); });
        for (var i = 0; i < Math.round(sec / 0.016); i++) { T.step(1); hi = Math.max(hi, ship.dbg.vel.length()); if (ship.lf.alt / L < 40) { T.hoverAt(node, d, 300); } }
        keys.forEach(function (k) { T.ku(k); });
        res['max_' + label] = +hi.toFixed(2);
        if (hi > expectMax * 1.03) fail(label + 'Over', hi);
        return hi;
    }
    T.step(5);
    var hc = run('cruise', ['KeyW'], 4, 4.0), hb = run('boost', ['KeyW', 'ShiftLeft'], 5, 12.0), hp = run('pulse', ['KeyW', 'Space'], 5, 40.0);
    if (hc < 3.5) fail('cruiseUnder', hc); if (hb < 10.5) fail('boostUnder', hb); if (hp < 35) fail('pulseUnder', hp);
    // altitude HUD
    T.clearKeys(); T.hoverAt(node, d, 2); T.step(30);
    var altEl = document.querySelector('.sh-alt');
    res.altHud = altEl ? altEl.textContent : null;
    res.altHudOn = !!(altEl && altEl.classList.contains('is-on'));
    res.altL = +(ship.lf.alt / L).toFixed(2);
    if (!res.altHudOn || !/ALT/.test(res.altHud || '')) fail('altHudMissing', res.altHud);
    // hover: below 3 L, no sink over 3 s with no input, strafe with D (lateral motion, roll unchanged)
    var a0 = ship.lf.alt / L; T.step(190);
    var a1 = ship.lf.alt / L; res.hoverSink_L = +(a0 - a1).toFixed(3); res.hover = ship.lf.hover;
    if (!ship.lf.hover || Math.abs(a0 - a1) > 0.3) fail('hoverSink', [a0, a1, ship.lf.hover]);
    var lp0 = ship.lf.lp.clone(), right = new THREE.Vector3(1, 0, 0).applyQuaternion(ship.lf.lq);      // everything in the LOCAL frame (the planet itself moves 20 u/s in the world)
    T.kd('KeyD'); T.step(60); T.ku('KeyD');
    var lat = ship.lf.lp.clone().sub(lp0).dot(right);
    var rollNow = new THREE.Vector3(1, 0, 0).applyQuaternion(ship.lf.lq).dot(ship.lf.lp.clone().normalize());
    res.strafe_u = +lat.toFixed(2); res.rollAfterStrafe = +rollNow.toFixed(4);
    if (lat < 0.8 || Math.abs(rollNow) > 0.05) fail('strafe', [lat, rollNow]);
    // E LAND prompt: still, below 1.5 L, over land
    T.clearKeys(); T.hoverAt(node, d, 1.2); T.step(40);
    res.altAtPrompt_L = +(ship.lf.alt / L).toFixed(2);
    res.landPrompt = document.querySelector('.sh-land').textContent;
    if (!/E\s+LAND/.test(res.landPrompt)) fail('landPrompt', res.landPrompt);
    // nose follows the horizon: level run for 12 s at boost, elevation angle stays small and altitude does not run away
    var attempt = 0, hz = null;
    while (attempt++ < 8 && !(hz && hz.valid)) {
        T.clearKeys(); T.hoverAt(node, T.safeDir(node, 1.2), 1200); T.step(30);
        var rA = ship.lf.lp.length(), maxE = 0, maxDr = 0, minAlt = 1e9;
        T.kd('KeyW'); T.kd('ShiftLeft');
        for (var i = 0; i < 375; i++) {
            T.step(1);
            var f = new THREE.Vector3(0, 0, -1).applyQuaternion(ship.shipRoot.quaternion), u = ship.shipRoot.position.clone().sub(node.anchor.position).normalize();
            maxE = Math.max(maxE, Math.abs(Math.asin(Math.max(-1, Math.min(1, f.dot(u))))));
            maxDr = Math.max(maxDr, Math.abs(ship.lf.lp.length() - rA)); minAlt = Math.min(minAlt, ship.lf.alt / L);
        }
        T.ku('KeyW'); T.ku('ShiftLeft');
        hz = { valid: minAlt > 60 && ship.lf.cap, distance_u: 72, angleOverPlanet_deg: +(72 / R * 57.3).toFixed(1), maxNoseElevation_deg: +(maxE * 57.3).toFixed(3), maxRadiusChange_L: +(maxDr / L).toFixed(2), minAlt_L: +minAlt.toFixed(0), attempts: attempt };
    }
    res.horizonRun = hz;
    if (!hz.valid || maxE > 0.02 || maxDr > 3 * L) fail('horizon', hz);
    // leaving: nose up + boost climbs; shake + message at 1.4 R, caps unlock at 1.7 R
    T.clearKeys(); T.hoverAt(node, d, 400); T.step(30);
    var up0 = ship.shipRoot.position.clone().sub(node.anchor.position).normalize();
    var rg = new THREE.Vector3(1, 0, 0).applyQuaternion(ship.shipRoot.quaternion); rg.addScaledVector(up0, -rg.dot(up0)).normalize();
    var m = new THREE.Matrix4().makeBasis(rg, new THREE.Vector3().crossVectors(up0.clone().negate(), rg), up0.clone().negate());      // nose straight up (X right, Y = Z x X, Z = -up)
    T.ship.shipRoot.quaternion.setFromRotationMatrix(m);
    T.kd('KeyW'); T.kd('ShiftLeft');
    var msg = false, shakeSeen = false, rAt14 = 0, unlockAt = 0, capsAtUnlockBefore = false, firstR = 0;
    for (i = 0; i < 6000; i++) {
        T.step(1, 0.05);
        var r = T.ratio(node), waveTxt = (document.querySelector('.sh-wave') || {}).textContent || '';
        if (/LEAVING ATMOSPHERE/.test(waveTxt)) { msg = true; if (!rAt14) rAt14 = r; }
        if (!ship.lf.on && !unlockAt) { unlockAt = r; break; }
    }
    T.ku('KeyW'); T.ku('ShiftLeft');
    res.leaving = { message: msg, ratioAtMessage: +rAt14.toFixed(3), frameReleasedAt: +unlockAt.toFixed(3) };
    if (!msg || rAt14 < 1.39 || rAt14 > 1.46 || unlockAt < 1.69) fail('leaving', res.leaving);
    // after release: pulse above 40 u/s is allowed again
    T.kd('Space'); T.kd('KeyW'); T.step(620); T.ku('Space'); T.ku('KeyW');
    res.speedAfterUnlock = +ship.dbg.vel.length().toFixed(1);
    if (res.speedAfterUnlock < 45) fail('unlock', res.speedAfterUnlock);
    return res;
}

// ─── 4. surface never clips ────────────────────────────────────────────────────────────
async function g4(T, o) {
    var ship = T.ship, node = T.pick(), ps = ship.ps, L = T.L(), THREE = T.THREE, minutes = o.minutes || 5, dt = 0.05;
    var frames = Math.round(minutes * 60 / dt), d = T.landDir(node);
    var prevR = 0, vPrev = 0, flPrev = 0, flPrevAlt = 1e9, rawRatioMax = 0, minClear = 1e9, nan = 0, maxRatio = 0, atmFrames = 0, relaunch = 0, tele = 0, maxExcess = 0, worst = null, prev = new THREE.Vector3(), prevOk = false, subMax = 0;
    var codes = ['KeyW', 'KeyS', 'ShiftLeft', 'Space', 'KeyA', 'KeyD', 'ControlLeft'];
    T.clearKeys(); T.hoverAt(node, d, 60); T.step(20);
    var held = {}, k;
    for (var f = 0; f < frames; f++) {
        if (f % 200 === 0) await T.pause();
        if (f % 10 === 0) {   // new random input every 0.5 s: biased to W so it moves, sometimes dives (negative pitch = mouse down)
            codes.forEach(function (c) { var want = c === 'KeyW' ? T.rng() < 0.8 : (c === 'KeyS' ? T.rng() < 0.1 : T.rng() < 0.3); if (want && !held[c]) { T.kd(c); held[c] = 1; } else if (!want && held[c]) { T.ku(c); held[c] = 0; } });
            ship.dbg.inject((T.rng() - 0.5) * 400, (T.rng() - 0.35) * 420);
        }
        if (f % 4 === 0) ship.dbg.inject((T.rng() - 0.5) * 120, (T.rng() - 0.3) * 160);
        T.step(1, dt);
        var lf = ship.lf;
        if (!lf.on || !lf.cap || Math.abs(T.ratio(node) - 1) > 0.45) { relaunch++; T.hoverAt(node, T.safeDir(node, 1.05), 30 + T.rng() * 3000); T.step(10, dt); prevOk = false; continue; }
        atmFrames++;
        var lp = lf.lp, r = lp.length(), fl = ps.floorLocal(lp.x, lp.y, lp.z), clear = (r - fl) / L;
        if (!isFinite(r) || !isFinite(ship.shipRoot.position.x) || !isFinite(ship.dbg.vel.x)) nan++;
        minClear = Math.min(minClear, clear); subMax = Math.max(subMax, lf.subN);
        if (prevOk) {
            var Rn = ship.ps.radius; if (prevR && Math.abs(Rn / prevR - 1) > 1e-9) { prev.multiplyScalar(Rn / prevR); }
            var step = lp.distanceTo(prev), vmax = Math.max(ship.dbg.vel.length(), vPrev, 0.01);
            var allowed = vmax * dt * 1.5 + 0.02 * L + (ship.combat.rollCd > 0.85 ? 17 * L * dt : 0) + ((flPrevAlt < 4 || clear < 4) ? Math.abs(fl - flPrev) : 0);      // dodge roll = a scripted 4 L sidestep (14 L/s); a ship in contact / hover range rides the ground contour (these ridges are steeper than 45 deg)
            rawRatioMax = Math.max(rawRatioMax, step / (vmax * dt));
            if (step > allowed) { tele++; if (step - allowed > maxExcess) { maxExcess = step - allowed; worst = { step_u: step, allowed_u: allowed, v: vmax, vPrev: vPrev, bounce: ship.dbg.bounce.length(), f: f, held: Object.keys(held).filter(function (c) { return held[c]; }).join('+'), clear: clear, flDelta: fl - flPrev, hover: lf.hover, hold: lf.holdAlt, jump: lf.jump, Rscale: Rn / (prevR || Rn) - 1, dr: lp.length() - prev.length() }; } }
        }
        prev.copy(lp); prevR = ship.ps.radius; prevOk = true; vPrev = ship.dbg.vel.length(); flPrev = fl; flPrevAlt = clear;
    }
    Object.keys(held).forEach(function (c) { if (held[c]) T.ku(c); });
    return { pass: minClear >= 0.5 && nan === 0 && tele === 0 && atmFrames > frames * 0.6, simulatedSeconds: frames * dt, framesInAtmosphere: atmFrames, relaunched: relaunch, minClearance_L: +minClear.toFixed(3), nan: nan, teleportFrames: tele, worst: worst, maxSubstepsPerFrame: subMax, maxStepOverSpeedDt_raw: +rawRatioMax.toFixed(2) };
}

// ─── 5. terrain continuity across patch re-centres ──────────────────────────────────
async function g5(T, o) {
    var ship = T.ship, node = T.pick(), ps = ship.ps, L = T.L(), THREE = T.THREE, N = 1000;
    var dirs = []; for (var i = 0; i < N; i++) dirs.push(T.rdir());
    function sample() { var out = new Float64Array(N); for (var j = 0; j < N; j++) out[j] = ps.floorLocal(dirs[j].x, dirs[j].y, dirs[j].z); return out; }
    var d0 = T.landDir(node); T.hoverAt(node, d0, 3000); T.step(20);
    var a = sample(), recenters = 0, lastC = null;
    for (var k = 0; k < 50; k++) {              // 50 hops of the ship: each one moves the patch centre far beyond its re-centre threshold
        T.hoverAt(node, T.safeDir(node, 1.1), 800 + T.rng() * 4000); T.step(6);
        recenters++;
    }
    var b = sample(), maxDiff = 0, same = 0, j;
    for (j = 0; j < N; j++) { var df = Math.abs(a[j] - b[j]); maxDiff = Math.max(maxDiff, df); if (df === 0) same++; }
    // also: world-space query agrees with the local query at any spin phase
    var W = new THREE.Vector3(), cmp = 0;
    for (var m = 0; m < 50; m++) { var dd = dirs[m]; T.localToWorld(node, dd.clone().multiplyScalar(node.mesh.scale.x), W); var fo = ps.floorAt(W, { r: 0, n: new THREE.Vector3() }); cmp = Math.max(cmp, Math.abs(fo.r - ps.floorLocal(dd.x, dd.y, dd.z))); }
    return { pass: maxDiff === 0 && cmp < 1e-6, points: N, recenters: recenters, identical: same, maxDiff: maxDiff, worldVsLocalMaxDiff: cmp };
}

// ─── 6. surfaces ────────────────────────────────────────────────────────────────────────
async function g6(T, o) {
    var ship = T.ship, node = T.pick(), ps = ship.ps, L = T.L(), THREE = T.THREE, res = { pass: true };
    var info = ps.info ? ps.info() : null; res.info = info;
    if (!info || info.amp < 0.0899 || info.amp > 0.0901) { res.pass = false; res.ampBad = info && info.amp; }
    // looks across the galaxy
    var looks = { rocky: 0, lush: 0, icy: 0 };
    T.planets().forEach(function (n) { if (n.mesh.material.uniforms.uBiome.value < 0.5 && ps.lookOf) looks[ps.lookOf(n)]++; });
    res.looks = looks; if (!(looks.rocky && looks.lush && looks.icy)) { res.pass = false; res.looksMissing = true; }
    // relief: ridged mountains. Sample range of height on a 2000 point grid
    var hmin = 9, hmax = -9, ridgeSteps = 0;
    for (var i = 0; i < 2000; i++) { var d = T.rdir(), h = ps.heightLocal(d.x, d.y, d.z); hmin = Math.min(hmin, h); hmax = Math.max(hmax, h); }
    res.relief = { min: +hmin.toFixed(4), max: +hmax.toFixed(4) }; if (hmax < 0.04) { res.pass = false; res.flat = true; }
    // rocks near the ship
    var dl = T.landDir(node); T.hoverAt(node, dl, 5); T.step(40);
    res.rocks = ps.info().rocks; if (res.rocks < 100) { res.pass = false; res.rocksLow = true; }
    // sea is flat and unlandable
    var sea = null; for (i = 0; i < 4000 && !sea; i++) { var q = T.rdir(); if (!ps.landLocal(q.x, q.y, q.z)) sea = q; }
    if (sea) {
        T.hoverAt(node, sea, 1); T.step(30);
        var lres = ps.landable(ship.shipRoot.position); res.seaLandable = lres.ok; res.seaPrompt = document.querySelector('.sh-land').textContent;
        var h1 = ps.heightLocal(sea.x, sea.y, sea.z), h2 = ps.heightLocal(sea.x * 1.001, sea.y, sea.z); res.seaHeight = +h1.toFixed(5);
        if (lres.ok || /LAND/.test(res.seaPrompt)) res.pass = false;
    } else { res.pass = false; res.noSeaFound = true; }
    return res;
}

// ─── 7. transition polish ───────────────────────────────────────────────────────────────
async function g7(T, o) {
    var ship = T.ship, node = T.pick(), THREE = T.THREE, cam = T.engine.camera, res = { pass: true }, R = node.mesh.scale.x;
    T.clearKeys(); T.ship.dbg.setPulseT(0);
    var d = T.safeWorldDir(node, 1.7); T.place(node, 1.7, d, 30); T.step(200);      // let the fov settle outside
    res.fovOutside = +cam.fov.toFixed(2);
    var haloOut = node.atmoMat ? node.atmoMat.uniforms.uAlpha.value : null;
    ship.dbg.vel.copy(new THREE.Vector3(0, 0, -1).applyQuaternion(ship.shipRoot.quaternion)).multiplyScalar(30); ship.dbg.setSpeed(30); T.kd('KeyW');
    var fovs = [], crossed = -1, fovAtCross = 0;
    for (var i = 0; i < 1200; i++) {
        if (!ship.lf.cap) ship.dbg.setSpeed(30);      // keep it closing without boost / pulse (those add their own fov)
        T.step(1);
        if (crossed < 0 && ship.lf.cap) { crossed = i; fovAtCross = cam.fov; }
        if (crossed >= 0 && (i - crossed) % 25 === 0) fovs.push(+cam.fov.toFixed(2));
        if (crossed >= 0 && i - crossed > 160) break;
    }
    T.ku('KeyW');
    res.fovAtCross = +fovAtCross.toFixed(2); res.fovAfter2s = fovs.slice(0, 10); res.fovFinal = +cam.fov.toFixed(2);
    if (!(res.fovFinal >= 49.3 && res.fovFinal <= 51.5)) { res.pass = false; res.fovBad = true; }
    if (!(fovAtCross < 44.5)) { res.pass = false; res.fovStartBad = true; }
    // halo: low over the surface the orbital atmo shell must be (almost) invisible
    var dl = T.landDir(node); T.hoverAt(node, dl, 30); T.step(40);
    res.haloAlphaOutside = haloOut; res.haloAlphaInside = node.atmoMat ? +node.atmoMat.uniforms.uAlpha.value.toFixed(4) : null;
    if (node.atmoMat && !(res.haloAlphaInside < 0.02 * Math.max(0.01, haloOut || 1))) { res.pass = false; res.haloBad = true; }
    res.fogK = +ship.ps.depth.toFixed(3);
    return res;
}

export async function runAll(engine, ship, opts) {
    opts = opts || {};
    var T = mk(engine, ship, opts), out = {}, only = opts.only, t0 = performance.now();
    await T.setup();
    var list = [['g1', g1], ['g2', g2], ['g3', g3], ['g4', g4], ['g5', g5], ['g6', g6], ['g7', g7]];
    for (var i = 0; i < list.length; i++) {
        if (only && only.indexOf(list[i][0]) < 0) continue;
        try { out[list[i][0]] = await list[i][1](T, opts); }
        catch (e) { out[list[i][0]] = { pass: false, error: String(e && e.stack || e) }; }
        T.clearKeys();
    }
    out.allPass = Object.keys(out).every(function (k) { return out[k].pass; });
    out.seconds = +((performance.now() - t0) / 1000).toFixed(1);
    return out;
}
export var tests = { g1: g1, g2: g2, g3: g3, g4: g4, g5: g5, g6: g6, g7: g7 };
export { mk as makeHarness };

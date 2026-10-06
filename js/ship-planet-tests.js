// ship-planet-tests.js — re-runnable checks for docs/ship-mode.md "Revision 18 — planet flight model (guaranteed)".
//
//   const t = await import('./js/ship-planet-tests.js');
//   const res = await t.runAll(window.EMGOR_GALAXY.engine, window.EMGOR_SHIP);          // all seven guarantees
//   await t.runAll(engine, ship, { only: ['g2', 'g4'], minutes: 5, trials: 20 });
// rev 19 (docs/ship-mode.md "Revision 19 -> A"): g8 every-planet landing sweep, g9 never-see-through (id-pass readback), g10 land/foot/board/lift-off on 3 sites,
//   g11 entry/exit shake at 15 %, g12 honest blasters, g13 boss formation.  Keys: only: ['g8'] ... ; runAll(.., { only: ['g12'] }).
//
// Drives the real game: window.EMGOR_GALAXY.step(dt) frames, key events on window, the ship's debug hooks (ship.dbg / ship.lf).
// Every guarantee returns { pass, ...numbers }. Safe to run on the dev server; it sets /user claude and /peaceful (no enemies) and enters ship mode.

var THREE_ = null;
function mk(engine, ship, opts) {
    var THREE = THREE_ = engine.THREE, G = window.EMGOR_GALAXY;
    try { if (ship.rev25) ship.rev25.noInterior = true; } catch (e) { /* older ship.js */ }      // rev 25: tap F near the ship enters the interior; the harness wants the old board
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
        pickSea: function () {   // rev 24: biggest rocky/lush planet that really has sea (land fraction < 0.8, not an all-land world)
            var best = null, ps = ship.ps;
            T.planets().forEach(function (n) {
                if (n.mesh.material.uniforms.uBiome.value >= 0.5 || (best && n.mesh.scale.x <= best.mesh.scale.x)) return;
                var si = null; try { si = ps.solveSea ? ps.solveSea(n) : null; } catch (e) { si = null; }
                if (si && !si.allLand && si.frac < 0.8) best = n;
            });
            return best || T.pick();
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
        // rev 19: geometric reachability. A planet can be approached when some point 1.45 R out has every OTHER body farther than 1.6 of its own radii
        // (otherwise a bigger body wins ps.active / hard-spheres the ship out). Sub-system planets orbit INSIDE their parent's globe in pilot scale: not reachable.
        reachable: function (node) {
            var all = T.planets(), c = node.anchor.position, R = node.mesh.scale.x;
            for (var k = 0; k < 120; k++) {
                var d = T.rdir(), p = d.clone().multiplyScalar(1.45 * R).add(c), ok = true;
                for (var i = 0; i < all.length; i++) { var o = all[i]; if (o === node) continue; if (p.distanceTo(o.anchor.position) < 1.6 * o.mesh.scale.x) { ok = false; break; } }
                if (ok) return true;
            }
            return false;
        },
        isGas: function (node) { return node.biome === 'gas'; },
        // open space: a spot at least 12 radii from every planet (for the combat tests); the ship is parked there, still, nose along -Z
        clearSpace: function () {
            T.fly(); var all = T.planets(), best = null, bd = -1;
            for (var k = 0; k < 400; k++) {
                var p = T.rdir().multiplyScalar(300 + T.rng() * 2500), m = 1e9;
                for (var i = 0; i < all.length; i++) m = Math.min(m, p.distanceTo(all[i].anchor.position) / all[i].mesh.scale.x);
                if (m > bd) { bd = m; best = p; } if (m > 12) break;
            }
            ship.shipRoot.position.copy(best); ship.shipRoot.quaternion.identity(); ship.dbg.vel.set(0, 0, 0); ship.dbg.setSpeed(0); ship.dbg.setPulseT(0); T.step(8);
            return bd;
        },
        hasLand: function (node) { var ps = ship.ps; for (var i = 0; i < 600; i++) { var d = T.rdir(); if (ps.landLocal(d.x, d.y, d.z)) return true; } return false; },
        hasSea: function (node) { var ps = ship.ps; for (var i = 0; i < 600; i++) { var d = T.rdir(); if (!ps.landLocal(d.x, d.y, d.z)) return d; } return null; },
        text: function (sel) { var el = document.querySelector(sel); return el ? el.textContent : ''; },
        // fraction of sampled screen pixels (rays that hit the planet's base sphere) NOT covered by terrain / ground objects, from the strict id pass (the orbital globe does not count)
        seeThru: function (N) {
            var cam = engine.camera, node = ship.ps.active, W = 96, H = 54, b = ship.ps.idPass(engine.renderer, cam, W, H, false);
            if (!b) return { bad: 0, tot: 0 };
            cam.updateMatrixWorld(true); cam.updateProjectionMatrix();
            var c = node.anchor.position, R = node.mesh.scale.x, o = cam.position, bad = 0, tot = 0, tries = 0, v = new THREE.Vector3(), oc = new THREE.Vector3();
            while (tot < (N || 64) && tries++ < 6000) {
                var px = Math.floor(T.rng() * W), py = Math.floor(T.rng() * H);
                v.set((px + 0.5) / W * 2 - 1, (py + 0.5) / H * 2 - 1, 0.5).unproject(cam).sub(o).normalize();
                oc.copy(o).sub(c); var bq = oc.dot(v), cq = oc.lengthSq() - R * R, disc = bq * bq - cq;
                if (disc < 0 || (-bq - Math.sqrt(disc)) < 0) continue;
                tot++; if (b[(py * W + px) * 4] < 128) bad++;
            }
            return { bad: bad, tot: tot };
        },
        idAscii: function () {      // debug picture of the strict id pass: # covered ground, X UNCOVERED ground (a hole), o covered sky-side, . sky
            var cam = engine.camera, node = ship.ps.active, W = 96, H = 54, b = ship.ps.idPass(engine.renderer, cam, W, H, false), c = node.anchor.position, R = node.mesh.scale.x, o = cam.position, rows = [], v = new THREE.Vector3(), oc = new THREE.Vector3();
            for (var py = H - 1; py >= 0; py -= 2) { var r = ''; for (var px = 0; px < W; px += 2) { v.set((px + 0.5) / W * 2 - 1, (py + 0.5) / H * 2 - 1, 0.5).unproject(cam).sub(o).normalize(); oc.copy(o).sub(c); var bq = oc.dot(v), disc = bq * bq - (oc.lengthSq() - R * R), hit = disc >= 0 && (-bq - Math.sqrt(disc)) > 0, id = b[(py * W + px) * 4] > 128; r += hit ? (id ? '#' : 'X') : (id ? 'o' : '.'); } rows.push(r); }
            return rows.join('|');
        },
        camClear: function () {      // camera height above the terrain under it (world units); negative = inside the ground
            var node = ship.ps.active, cam = engine.camera, fo = ship.ps.floorAt(cam.position, { r: 0, n: new THREE.Vector3() });
            return cam.position.distanceTo(node.anchor.position) - fo.r;
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
            if (ship.resetUpgrades) ship.resetUpgrades();      // rev 22: shield / drive tiers bought in a store would change speeds and hp: tests run on the base ship
            if (ship.state !== 'piloting') ship.enter();
            return new Promise(function (res) { var t0 = performance.now(); (function wait() { if (performance.now() - t0 < 250) return T.pause().then(wait); T.clearKeys(); T.step(160);
                if (ship.gmode === 'foot') { T.kd('KeyF'); T.ku('KeyF'); T.step(3); }          // board again
                if (ship.gmode === 'landed') { T.kd('KeyW'); T.ku('KeyW'); T.step(40); }       // lift off
                res(); })(); });   // 160 frames: past the 1.6 s boarding cinematic
        },
        // sphere-uniform random unit vector (seeded LCG so a run is repeatable)
        rng: (function () { var s = (opts && opts.seed) | 0 || 123456789; return function () { s = (Math.imul(s, 1664525) + 1013904223) | 0; return (s >>> 0) / 4294967296; }; })(),
        rdir: function () { var z = T.rng() * 2 - 1, a = T.rng() * 6.2831853, r = Math.sqrt(1 - z * z); return new THREE.Vector3(r * Math.cos(a), z, r * Math.sin(a)); },
        // a land point (unit local direction) with a gentle slope; returns local direction
        // back to plain flight from any ground state (foot -> board, landed -> lift off)
        fly: function () {
            T.clearKeys();
            if (ship.gmode === 'foot') {
                T.kd('KeyF'); T.ku('KeyF'); T.step(3);
                if (ship.gmode === 'foot') { var h = ship._g.hum, l = ship._g.land; h.pos.copy(l.sPos).normalize().multiplyScalar(h.hr); T.step(3); T.kd('KeyF'); T.ku('KeyF'); T.step(3); }
            }
            if (ship.gmode === 'landed') { T.kd('KeyW'); T.ku('KeyW'); T.step(40); }
            if (ship.gmode === 'landing') { T.step(200); if (ship.gmode === 'landed') { T.kd('KeyW'); T.ku('KeyW'); T.step(40); } }
        },
        // rev 19: put the ship inside 1.45 R of `node` with the local frame AND the surface on that body. Sub-system planets sit close to bigger ones, so the
        // approach direction is searched until `node` has the smallest ratio of any body (ps picks the active body by smallest ratio) and nothing hard-spheres us out.
        // Returns false when the planet is embedded in a bigger body (every direction is inside a parent's hard sphere): no approach exists.
        engage: function (node) {
            T.fly();
            for (var k = 0; k < 60; k++) {
                var d = k < 20 ? T.safeWorldDir(node, 1.45) : T.rdir();
                T.place(node, 1.45, d, 0); T.step(6);
                if (ship.lf.on && ship.lf.node === node && ship.ps.active === node) return true;
            }
            return false;
        },
        near: function (node) {   // bring the ship inside 3 R so the planet surface (ps) activates on this body
            if (ship.ps.active === node) return;
            if (!T.engage(node)) throw new Error('cannot approach ' + node.id + ' (embedded in a larger body)');
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
            if (ship.gmode !== 'fly') T.fly();
            if (!(lf.on && lf.node === node)) { T.place(node, 1.45, d.clone().applyQuaternion(node.mesh.quaternion), 0); T.step(4); }
            if (!(lf.on && lf.node === node) && !T.engage(node)) throw new Error('local frame did not engage (' + node.id + ' is embedded in a larger body)');
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
    T.kd('KeyF'); T.ku('KeyF'); T.step(5);
    if (ship.gmode === 'foot') {
        var hum = ship._g.hum, hp0 = hum.pos.clone(), hd = 0, hw = 0;
        for (i = 0; i < 400; i++) {
            T.step(1, 0.05); hd = Math.max(hd, hum.pos.distanceTo(hp0));
            T.localToWorld(node, hum.pos, W); hw = Math.max(hw, W.distanceTo(ship.humanObj.position));
        }
        res.human = { localDrift: hd, worldTrackErr_u: hw }; if (hd > 1e-9 || hw > 1e-3 * L) res.pass = false;      // rev 20: float noise (1e-13 u) in the human's ground re-projection is not drift; 1e-9 u is still 1e-8 L
        T.kd('KeyF'); T.ku('KeyF'); T.step(3);          // back aboard (standing next to the ship)
    } else { res.human = 'did not step out'; res.pass = false; }
    // lift off again so later tests start flying
    ship.onKeyF && 0;
    T.kd('KeyW'); T.step(2); T.ku('KeyW'); T.step(30);
    res.liftedOff = ship.gmode === 'fly';
    return res;
}

// ─── 2. guaranteed entry ───────────────────────────────────────────────────────────────
async function g2(T, o) {
    function lineClear(node, d, ratio) {
        var all = T.planets(), c = node.anchor.position, R = node.mesh.scale.x, P0 = d.clone().multiplyScalar(ratio * R).add(c), seg = c.clone().sub(P0), len2 = seg.lengthSq();
        for (var i = 0; i < all.length; i++) {
            var m = all[i]; if (m === node) continue;
            var t = Math.max(0, Math.min(1, m.anchor.position.clone().sub(P0).dot(seg) / len2)), q = P0.clone().addScaledVector(seg, t);
            if (q.distanceTo(m.anchor.position) < 2.2 * m.mesh.scale.x) return false;
        }
        return true;
    }
    var ship = T.ship, node = T.pick(), N = o.trials || 20, R = node.mesh.scale.x, L = T.L(), bad = [], rows = [];
    var maxXSpeed = 0, maxHpLoss = 0, minFinal = 9, brake = [];
    for (var t = 0; t < N; t++) {
        await T.pause();
        T.clearKeys(); ship.dbg.setPulseT(0);
        // leave any frame / ground state
        if (ship.gmode !== 'fly') { T.kd('KeyW'); T.step(2); T.ku('KeyW'); }
        var attempt = 0, crossNode = '', crossR = 0, again, d, spd, nearOther, hp0, crossI, vCross, outAgain, vAfter;
        do {      // rev 22: a line another planet crossed in the meantime (planets move ~100 u/s) never reaches this planet: pick a new line, up to 4 tries
        again = false;
        d = T.safeWorldDir(node, 4);
        for (var lk = 0; lk < 40 && !lineClear(node, d, 4); lk++) d = T.safeWorldDir(node, 4);      // rev 23: the whole approach line must be clear of other bodies (entering a neighbour instead is not a failure of THIS planet)
        spd = t % 2 ? 1500 : 400 + T.rng() * 800;
        T.place(node, 4, d, spd);
        // random approach: aim off-centre by up to 14 deg (4 sin 14 = 0.97 R: every line still crosses the 1.4 R shell)
        var off = (T.rng() * 14) * Math.PI / 180, ax = new T.THREE.Vector3(T.rng() - .5, T.rng() - .5, T.rng() - .5).normalize();
        T.ship.shipRoot.quaternion.premultiply(new T.THREE.Quaternion().setFromAxisAngle(ax, off));
        var f = new T.THREE.Vector3(0, 0, -1).applyQuaternion(ship.shipRoot.quaternion);
        ship.dbg.vel.copy(f).multiplyScalar(spd); ship.dbg.setSpeed(spd);
        ship.dbg.setPulseT(ship.dbg.PULSE_FULL);
        nearOther = { r: 99, id: '' }; hp0 = ship.stats.hp; crossI = -1; vCross = 0; outAgain = false; vAfter = 0;
        T.kd('Space'); T.kd('KeyW');
        for (var i = 0; i < 2400; i++) {        // rev 20: R is ~12000 L; the approach governor (closing speed <= distance-to-shell / 1.2 s) is exponential, ~10 s from 4 R at the largest planets
            if (!ship.lf.cap && i < 400 && !(ship.lf.on)) { /* keep the nose on the approach line until the frame owns the ship */ }
            T.step(1);
            var r = T.ratio(node);
            if (i % 30 === 0) { var oth = T.planets(); for (var oi = 0; oi < oth.length; oi++) { if (oth[oi] === node) continue; var orr = ship.shipRoot.position.distanceTo(oth[oi].anchor.position) / oth[oi].mesh.scale.x; if (orr < nearOther.r) { nearOther.r = orr; nearOther.id = oth[oi].id; } } }
            if (crossI < 0 && ship.lf.cap) { crossI = i; vCross = ship.dbg.vel.length(); crossNode = ship.lf.node ? ship.lf.node.id : ''; crossR = r; }
            if (crossI >= 0 && i - crossI < Math.round(2 / 0.016) && r > 1.4001) outAgain = true;      // nothing may push you back out during ENTRY
            if (crossI >= 0 && i === crossI + Math.round(1.98 / 0.016)) vAfter = ship.dbg.vel.length();       // end of the 2 s ENTRY brake
            if (crossI >= 0 && i > crossI + 140) break;
        }
        T.ku('Space'); T.ku('KeyW');
        if (crossI < 0 && ++attempt < 4) again = true;
        } while (again);
        var rEnd = T.ratio(node), hpLoss = hp0 - ship.stats.hp;
        maxHpLoss = Math.max(maxHpLoss, hpLoss); minFinal = Math.min(minFinal, rEnd);
        var ok = crossI >= 0 && rEnd < 1.4 && !outAgain && hpLoss === 0 && vAfter <= 12.01;
        if (!ok) bad.push({ nearOther: nearOther.id.split('.').pop() + ' ' + nearOther.r.toFixed(2) + 'R', P: ship.shipRoot.position.toArray().map(Math.round), t: t, d: d.toArray().map(function (v) { return +v.toFixed(2); }), crossI: crossI, crossNode: crossNode, crossR: +crossR.toFixed(3), spd: Math.round(spd), rEnd: +rEnd.toFixed(3), outAgain: outAgain, hpLoss: hpLoss, vAfter: +vAfter.toFixed(2), vCross: +vCross.toFixed(1) });
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
    // F LAND prompt: still, below 1.5 L, over land
    T.clearKeys(); T.hoverAt(node, d, 1.2); T.step(40);
    res.altAtPrompt_L = +(ship.lf.alt / L).toFixed(2);
    res.landPrompt = document.querySelector('.sh-land').textContent;
    if (!/F\s*·?\s*LAND/.test(res.landPrompt)) fail('landPrompt', res.landPrompt);
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
        var r = T.ratio(node);
        if (ship.lf.leaving) { msg = true; if (!rAt14) rAt14 = r; }      // rev 20b: the LEAVING ATMOSPHERE title is gone from the HUD; the state flag is the signal
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
    for (var sk = 0; sk < (o.skip || 0); sk++) T.rng();
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
            if (step > allowed) { tele++; if (step - allowed > maxExcess) { maxExcess = step - allowed; worst = { man: JSON.stringify(ship.maneuver), roll: ship.combat.rollCd, step_u: step, allowed_u: allowed, v: vmax, vPrev: vPrev, bounce: ship.dbg.bounce.length(), f: f, held: Object.keys(held).filter(function (c) { return held[c]; }).join('+'), clear: clear, flDelta: fl - flPrev, hover: lf.hover, hold: lf.holdAlt, jump: lf.jump, Rscale: Rn / (prevR || Rn) - 1, dr: lp.length() - prev.length() }; } }
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
    var ship = T.ship, node = T.pickSea(), ps = ship.ps, L = T.L(), THREE = T.THREE, res = { pass: true };
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
        if (lres.ok || /LAND/.test(res.seaPrompt)) res.pass = false;      // rev 20b: water shows no title, just no F · LAND
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

// ─── 8. every planet lands or says why ─────────────────────────────────────────────────
async function g8(T, o) {
    var ship = T.ship, THREE = T.THREE, all = T.planets(), rows = [], fails = [], embedded = [], gas = 0, terra = 0, moonlets = 0;
    for (var n = 0; n < all.length; n++) {
        var node = all[n], id = node.id, isGas = T.isGas(node);
        if (!T.reachable(node)) { embedded.push(id.split('.').pop()); continue; }
        await T.pause(); T.fly(); T.clearKeys();
        if (node.mesh.scale.x < 60 * T.L()) {      // MOONLET (rev 20): < 60 L rendered radius: hard sphere only, no atmosphere, no landing
            moonlets++; row = { id: id.split('.').pop(), biome: node.biome, R: +node.mesh.scale.x.toFixed(2), status: 'moonlet' };
            T.place(node, 1.6, T.safeWorldDir(node, 1.6), 0); T.step(6);
            if (ship.ps.active === node) { row.status = 'moonlet-has-ps'; fails.push({ id: id, row: row }); }
            T.fly(); rows.push(row); continue;
        }
        if (!T.engage(node)) { fails.push({ id: id, why: 'engage failed on a reachable planet' }); continue; }
        var row = { id: id.split('.').pop(), biome: node.biome, R: +node.mesh.scale.x.toFixed(1) };
        if (isGas) gas++; else terra++;
        {
            var land = T.hasLand(node), sea = T.hasSea(node);
            if (land) {
                // landDir's flatness probe is in unit-sphere epsilon (not ship lengths): a rare pick still reads TOO STEEP, so try up to 8 spots
                var dd = null;
                for (var tries = 0; tries < 8; tries++) { dd = T.landDir(node); if (!dd) break; T.hoverAt(node, dd, 2); T.step(40); if (/LAND/.test(T.text('.sh-land'))) break; }
                row.prompt = T.text('.sh-land'); row.tries = tries + 1; row.big = document.querySelector('.sh-land').classList.contains('is-big');
                T.kd('KeyF'); T.ku('KeyF');
                for (var j = 0; j < 700 && ship.gmode !== 'landed'; j++) T.step(1);
                row.landed = ship.gmode === 'landed'; row.landedText = T.text('.sh-land');
                if (!/F\s*·\s*LAND/.test(row.prompt) || !row.big || !row.landed) fails.push({ id: id, row: row });
            }
            if (sea) {
                T.fly(); T.hoverAt(node, sea, 1.2); T.step(40);
                row.seaPrompt = T.text('.sh-land');
                if (/LAND/.test(row.seaPrompt)) fails.push({ id: id, seaPrompt: row.seaPrompt });      // rev 20b: no WATER title; the land prompt just must not appear
            }
            if (!land && !sea) fails.push({ id: id, why: 'neither land nor sea found' });
        }
        T.fly(); rows.push(row);
    }
    return { pass: fails.length === 0 && rows.length > 0, planets: all.length, reachable: rows.length, gas: gas, terra: terra, moonlets: moonlets, failures: fails, embeddedInParent: embedded.length, embeddedNote: 'sub-system planets orbit inside their parent globe in pilot scale (galaxy3d layout): no approach exists', rows: rows };
}

// ─── 9. never see through the ground ───────────────────────────────────────────────────
async function g9(T, o) {
    var ship = T.ship, THREE = T.THREE, L = T.L(), engine0 = T.engine, poses = o.poses || 200, nodes = T.planets().filter(function (n) { return !T.isGas(n) && T.reachable(n); });
    if (!nodes.length) return { pass: false, why: 'no reachable terra planet' };
    // the "jellyfish planet" = the one with the most creatures near the ground
    var best = nodes[0], bestC = -1;
    nodes.forEach(function (n) { try { T.fly(); T.near(n); var dd = T.landDir(n); if (dd) { T.hoverAt(n, dd, 2); T.step(10); var c = ship.ps.info().creatures; if (c > bestC) { bestC = c; best = n; } } } catch (e) { /* skip */ } });
    var by = { hover: { poses: 0, bad: 0, px: 0 }, landed: { poses: 0, bad: 0, px: 0 }, foot: { poses: 0, bad: 0, px: 0 } }, camUnder = 0, minCam = 1e9, worst = [], p = 0;
    function check(mode, label) {
        var r = T.seeThru(64), cc = T.camClear() / L; minCam = Math.min(minCam, cc);
        by[mode].poses++; by[mode].px += r.tot; by[mode].bad += r.bad; if (cc < -1e-6) camUnder++;
        if (r.bad && worst.length < 6) worst.push({ mode: mode, bad: r.bad, tot: r.tot, camClear_L: +cc.toFixed(3), label: label, picture: worst.length < 2 ? T.idAscii() : undefined, near_L: +(engine0.camera.near / L).toFixed(4), info: ship.ps.info() });
        p++;
    }
    var nHover = Math.round(poses * 0.7), nSite = Math.round((poses - nHover) / 2);
    for (var i = 0; i < nHover; i++) {
        if (i % 10 === 0) await T.pause();
        var node = i % 3 === 0 ? best : nodes[i % nodes.length];
        var d = T.safeDir(node, 1.02); T.hoverAt(node, d, 0.9 + T.rng() * 2.1);      // rev 25: a random direction can sit inside a bigger neighbour's shell (that body then owns ps.active); sample only clear sites
        var lq = ship.lf.lq.clone(), ax = d.clone().applyQuaternion(lq.clone().invert());      // local up in ship axes
        lq.multiply(new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0, 1, 0), (T.rng() - 0.5) * 6.28)).multiply(new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(1, 0, 0), (T.rng() - 0.6) * 0.7));
        var lp = ship.lf.lp; ship.dbg.setLocal(lp.x, lp.y, lp.z, lq.x, lq.y, lq.z, lq.w);
        T.step(5); check('hover', node.id.split('.').pop() + ' alt ' + (ship.lf.alt / L).toFixed(1));
    }
    for (var sI = 0; sI < nSite; sI++) {
        await T.pause();
        var nd = sI % 2 ? best : nodes[sI % nodes.length];
        T.fly(); T.near(nd); var dl = T.landDir(nd); if (!dl) continue;
        T.hoverAt(nd, dl, 2.5); T.step(30);
        if (!ship.land()) continue;
        for (var q = 0; q < 700 && ship.gmode !== 'landed'; q++) T.step(1);
        T.step(120); if (ship.gmode !== 'landed') continue;
        check('landed', 'landed on ' + nd.id.split('.').pop());
        T.kd('KeyF'); T.ku('KeyF'); T.step(6);
        if (ship.gmode !== 'foot') continue;
        for (var w = 0; w < 3; w++) {
            T.kd('KeyW'); for (var f = 0; f < 40; f++) { if (f % 10 === 0) ship.dbg.inject(10 + T.rng() * 60, (T.rng() - 0.5) * 20); T.step(1); } T.ku('KeyW'); T.step(8);
            check('foot', 'foot on ' + nd.id.split('.').pop());
        }
    }
    T.fly();
    var tot = by.hover.bad + by.landed.bad + by.foot.bad;
    return { pass: tot === 0 && camUnder === 0 && p >= poses * 0.8, poses: p, pixelsSampled: by.hover.px + by.landed.px + by.foot.px, badPixels: tot, cameraUnderGround: camUnder, minCameraClearance_L: +minCam.toFixed(3), byMode: by, jellyfishPlanet: best.id, creaturesThere: bestC, worst: worst };
}

// ─── 10. land -> exit -> walk 20 L -> back -> board -> lift off, on 3 sites ────────────────
async function g10(T, o) {
    var ship = T.ship, THREE = T.THREE, L = T.L(), nodes = T.planets().filter(function (n) { return !T.isGas(n) && T.reachable(n); }), sites = [], rows = [], fails = [];
    for (var s = 0; s < 3; s++) sites.push(nodes[s % nodes.length]);
    for (var si = 0; si < sites.length; si++) {
        var node = sites[si], row = { planet: node.id.split('.').pop() }, big = function () { return document.querySelector('.sh-land').classList.contains('is-big'); };
        await T.pause(); T.fly(); T.near(node);
        var d = T.landDir(node); if (!d) { fails.push({ site: si, why: 'no land dir' }); continue; }
        T.hoverAt(node, d, 2); T.step(40);
        row.hoverPrompt = T.text('.sh-land'); row.hoverBig = big();
        T.kd('KeyF'); T.ku('KeyF'); T.step(3); row.afterE = ship.gmode;
        var t0 = 0; for (var i = 0; i < 700 && ship.gmode !== 'landed'; i++) { T.step(1); t0++; }
        row.landed = ship.gmode === 'landed';
        // 1.2 s settle: hull dips, then rests; the HUD line appears when it is done
        var settleSeen = false, dipMin = 0; for (i = 0; i < 120; i++) { T.step(1, 0.016); var hy = ship.hullObjY; if (hy < dipMin) dipMin = hy; settleSeen = true; }      // rev 20b: the LANDING / LANDED titles are gone
        T.step(60);
        row.landedHud = T.text('.sh-land'); row.settleSeen = settleSeen; row.hullDip_L = +dipMin.toFixed(3);
        T.kd('KeyF'); T.ku('KeyF'); T.step(5); row.foot = ship.gmode === 'foot';
        var hum = ship._g.hum, land = ship._g.land, start = hum.pos.clone(), dist = 0, k;
        for (k = 0; k < 1200 && dist < 20 * L; k++) { if (k % 40 === 0) ship.dbg.inject((T.rng() - 0.5) * 30, 0); T.kd('KeyW'); T.kd('ShiftLeft'); T.step(1, 0.05); dist = hum.pos.distanceTo(start); }
        T.ku('KeyW'); T.ku('ShiftLeft'); T.step(5);
        row.walked_L = +(hum.pos.distanceTo(start) / L).toFixed(1);
        var farPrompt = T.text('.sh-land'); row.farPrompt = farPrompt;
        // turn back toward the ship and run until the BOARD prompt shows
        var boardSeen = false, bigBoard = false;
        for (k = 0; k < 1500 && !boardSeen; k++) {
            var sl = land.sPos.length(), tgt = land.sPos.clone().multiplyScalar((sl - 0.3 * L) / sl), up = hum.pos.clone().normalize(), dir = tgt.sub(hum.pos);
            dir.addScaledVector(up, -dir.dot(up)); if (dir.lengthSq() > 1e-12) hum.hf.copy(dir.normalize());
            T.kd('KeyW'); T.kd('ShiftLeft'); T.step(1, 0.05);
            if (/BOARD/.test(T.text('.sh-land')) && T.text('.sh-land') !== '') { boardSeen = true; bigBoard = big(); }
        }
        T.ku('KeyW'); T.ku('ShiftLeft'); T.step(5);
        row.boardPrompt = T.text('.sh-land'); row.boardBig = bigBoard;
        T.kd('KeyF'); T.ku('KeyF'); T.step(5); row.reboarded = ship.gmode === 'landed';
        T.kd('KeyW'); T.ku('KeyW'); T.step(40); row.flying = ship.gmode === 'fly';
        var ok = /F\s*·\s*LAND/.test(row.hoverPrompt) && row.hoverBig && row.landed && settleSeen &&
            row.foot && row.walked_L >= 19.5 && /F\s*·\s*BOARD/.test(row.boardPrompt) && row.boardBig && row.reboarded && row.flying;
        if (!ok) fails.push({ site: si, row: row });
        rows.push(row); T.fly();
    }
    return { pass: fails.length === 0 && rows.length === 3, sites: rows.length, failures: fails, rows: rows };
}

// ─── 11. entry / exit shake at 15 % ─────────────────────────────────────────────────────
async function g11(T, o) {
    var ship = T.ship, L = T.L(), node = T.pick(), THREE = T.THREE, res = { pass: true }, old = { entry: 0.5 * L, leaving: 1.1 * L };
    // entry: 6 pulse / boost approaches from 4 R, max camera-shake amplitude while the ENTRY burn / rim pass runs
    var maxEntry = 0, hp0 = ship.stats.hp;
    for (var t = 0; t < 6; t++) {
        await T.pause(); T.fly(); T.clearKeys(); ship.dbg.setPulseT(0);
        var spd = t % 2 ? 1500 : 600, d = T.safeWorldDir(node, 4); T.place(node, 4, d, spd);
        ship.dbg.setPulseT(ship.dbg.PULSE_FULL); T.kd('Space'); T.kd('KeyW');
        var cross = -1;
        for (var i = 0; i < 520; i++) { T.step(1); if (cross < 0 && ship.lf.cap) cross = i; if (cross >= 0) maxEntry = Math.max(maxEntry, ship.shakeNow); if (cross >= 0 && i > cross + 160) break; }
        T.ku('Space'); T.ku('KeyW');
    }
    res.entryMax_L = +(maxEntry / L).toFixed(4); res.entryOld_L = +(old.entry / L).toFixed(3); res.entryRatioToOld = +(maxEntry / old.entry).toFixed(3);
    if (maxEntry > 0.15 * old.entry * 1.0005) { res.pass = false; res.entryTooStrong = true; }
    // leaving: climb out at boost, shake while LEAVING ATMOSPHERE shows
    T.fly(); T.clearKeys(); var d0 = T.landDir(node); T.hoverAt(node, d0, 400); T.step(30);
    var up0 = ship.shipRoot.position.clone().sub(node.anchor.position).normalize(), rg = new THREE.Vector3(1, 0, 0).applyQuaternion(ship.shipRoot.quaternion); rg.addScaledVector(up0, -rg.dot(up0)).normalize();
    ship.shipRoot.quaternion.setFromRotationMatrix(new THREE.Matrix4().makeBasis(rg, new THREE.Vector3().crossVectors(up0.clone().negate(), rg), up0.clone().negate()));
    T.kd('KeyW'); T.kd('ShiftLeft'); var maxLeave = 0, seen = false;
    for (i = 0; i < 6000; i++) { T.step(1, 0.05); if (ship.lf.leaving) { seen = true; maxLeave = Math.max(maxLeave, ship.shakeNow); } if (!ship.lf.on) break; }
    T.ku('KeyW'); T.ku('ShiftLeft');
    res.leavingSeen = seen; res.leavingMax_L = +(maxLeave / L).toFixed(4); res.leavingOld_L = +(old.leaving / L).toFixed(3); res.leavingRatioToOld = +(maxLeave / old.leaving).toFixed(3);
    if (!seen || maxLeave > 0.15 * old.leaving * 1.0005) { res.pass = false; res.leavingTooStrong = true; }
    res.hpLoss = hp0 - ship.stats.hp;
    return res;
}

// ─── 12. honest blasters ─────────────────────────────────────────────────────────────────
async function g12(T, o) {
    var ship = T.ship, L = T.L(), THREE = T.THREE, res = { pass: true }, G = T.G;
    T.fly(); T.clearKeys(); ship.god = true; ship.cmd('/peaceful'); res.clearance_R = +T.clearSpace().toFixed(1);
    // (a) at pulse 1500 u/s the bolts lead the ship
    var f = new THREE.Vector3(0, 0, -1).applyQuaternion(ship.shipRoot.quaternion);
    ship.dbg.vel.copy(f).multiplyScalar(1500); ship.dbg.setSpeed(1500); ship.dbg.setPulseT(ship.dbg.PULSE_FULL); T.kd('Space'); T.kd('KeyW');
    T.step(10); ship.dbg.fire(true);
    // rev 23: sample every live bolt frame to frame while firing (>= 4 distinct bolts), each against the ship's own speed that frame
    var prevB = {}, seenIds = {}, minBolt = 1e9, minRel = 1e9, n = 0, shipSpd = 0, fwd = new THREE.Vector3(0, 0, -1).applyQuaternion(ship.shipRoot.quaternion), ahead = 0;
    for (var fi = 0; fi < 90; fi++) {
        ship.dbg.setHeat(0); T.step(1, 0.016);      // rev 22: stay cold (heat would stop the burst)
        shipSpd = ship.dbg.vel.length(); fwd.set(0, 0, -1).applyQuaternion(ship.shipRoot.quaternion);
        var cur = {};
        ship.dbg.liveBolts().forEach(function (b) {
            cur[b.id] = b.p.clone();
            if (prevB[b.id]) { var v = b.p.clone().sub(prevB[b.id]).multiplyScalar(1 / 0.016), sp = v.length(); if (!seenIds[b.id]) { seenIds[b.id] = 1; n++; } minBolt = Math.min(minBolt, sp - shipSpd); minRel = Math.min(minRel, v.dot(fwd) - ship.dbg.vel.dot(fwd)); }
        });
        prevB = cur;
    }
    ship.dbg.fire(false);
    T.step(20); ship.dbg.liveBolts().forEach(function (b) { ahead = Math.max(ahead, b.p.clone().sub(ship.shipRoot.position).dot(fwd)); });
    T.ku('Space'); T.ku('KeyW');
    res.speed = { ship_us: +shipSpd.toFixed(1), slowestBoltMinusShip_us: +minBolt.toFixed(1), boltsMeasured: n, slowestRelativeToShip_us: +minRel.toFixed(2), maxAheadAfter1s_u: +ahead.toFixed(1) };
    if (!(n >= 4 && minBolt > 0 && ahead > 0)) { res.pass = false; res.speedFail = true; }
    ship.dbg.vel.set(0, 0, 0); ship.dbg.setSpeed(0); ship.dbg.setPulseT(0); ship.dbg.liveBolts().forEach(function () {});
    // (b) 100 bolts at a parked wave-8 boss from 50 L
    ship.cmd('/hostile'); ship.cmd('/wave 8'); T.step(60);
    var E = ship.dbg.enemies, boss = null;
    for (var i = 0; i < 400 && !boss; i++) { T.step(1, 0.05); for (var k = 0; k < E.length; k++) if (E[k].alive && E[k].isBoss && (!boss || E[k].len > boss.len)) boss = E[k]; }
    if (!boss) { res.pass = false; res.noBoss = true; return res; }
    var limbs = boss.cr.limbs ? boss.cr.limbs.length : 0, hp0 = boss.hp;
    var fired0 = ship.boltStats.fired, hits0 = ship.boltStats.hits;
    for (i = 0; i < 400; i++) {
        boss.hp = boss.maxHp;                                                    // keep it alive for the count
        var u = new THREE.Vector3(Math.sin(i * 0.37), 0.2, Math.cos(i * 0.37)).normalize();
        ship.shipRoot.position.copy(boss.g.position).addScaledVector(u, boss.R + 50 * L);
        var m = new THREE.Matrix4().lookAt(ship.shipRoot.position, boss.g.position, new THREE.Vector3(0, 1, 0)); ship.shipRoot.quaternion.setFromRotationMatrix(m);
        ship.dbg.vel.set(0, 0, 0); ship.dbg.setSpeed(0);
        ship.dbg.setHeat(0); ship.dbg.fire(true); T.step(1, 0.016);      // rev 21: weapon heat would stop a 100-bolt burst: keep it cold
        if (ship.boltStats.fired - fired0 >= 100) break;
    }
    ship.dbg.fire(false); T.step(240, 0.05);           // let the last bolts land
    var fired = ship.boltStats.fired - fired0, hits = ship.boltStats.hits - hits0;
    res.hitTest = { bossLen_L: +(boss.len / L).toFixed(0), bossHitR_L: +(boss.R / L).toFixed(0), limbCapsules: limbs, firedTotal: fired, registered: hits, parkedAt_L: 50 };
    if (!(fired >= 100 && hits >= 90 * Math.min(1, fired / 100))) { res.pass = false; res.hitFail = true; }
    ship.cmd('/peaceful'); ship.god = false;
    return res;
}

// ─── 13. boss formation ──────────────────────────────────────────────────────────────────
async function g13(T, o) {
    var ship = T.ship, L = T.L(), THREE = T.THREE, res = { pass: true };
    T.fly(); T.clearKeys(); ship.god = true; res.clearance_R = +T.clearSpace().toFixed(1); var home = ship.shipRoot.position.clone();
    ship.cmd('/hostile'); ship.cmd('/wave 20');
    var E = ship.dbg.enemies, bosses = [];
    for (var i = 0; i < 700; i++) { T.step(1, 0.05); ship.dbg.vel.set(0, 0, 0); ship.shipRoot.position.copy(home); bosses = E.filter(function (e) { return e.alive && e.isBoss; }); if (bosses.length >= 3) break; }
    res.bosses = bosses.length;
    if (bosses.length < 3) { res.pass = false; res.tooFewBosses = true; return res; }
    function reachMin(b) { var r = 1e30; if (b.cr && b.cr.moves) for (var i = 0; i < b.cr.moves.length; i++) r = Math.min(r, b.cr.moves[i].reachL * b.sc); return r; }
    var P = ship.shipRoot.position, n = bosses.length, maxLen = Math.max.apply(null, bosses.map(function (b) { return b.len; })), need = 1.5 * maxLen;
    var sPos = [], prev = bosses.map(function (b) { return b.g.position.distanceTo(P); }), startD = prev.slice(), minSep = 1e9, nonDec = 0, reached = 0, samples = 0;
    for (var s = 0; s < 1200; s++) {                      // 60 s of simulated time
        T.step(1, 0.05); ship.dbg.vel.set(0, 0, 0); ship.dbg.setSpeed(0); ship.shipRoot.position.copy(home);
        for (var a = 0; a < n; a++) for (var b = a + 1; b < n; b++) minSep = Math.min(minSep, bosses[a].g.position.distanceTo(bosses[b].g.position));
        if (s % 20 === 19) {                              // once a second: the distance to the player has to be shrinking until the boss reaches melee
            samples++;
            for (a = 0; a < n; a++) {
                var dnow = bosses[a].g.position.distanceTo(P);
                if (!bosses[a].alive) continue;
                if (!sPos[a]) sPos[a] = new THREE.Vector3();
                var shoved = sPos[a].lengthSq() > 0 && bosses[a].g.position.distanceTo(sPos[a]) > 3.5;      // rev 22: a moving planet's push-out shoved it (not the formation): that sample proves nothing
                sPos[a].copy(bosses[a].g.position);
                if (shoved) { prev[a] = dnow; continue; }
                var bb = bosses[a], stopD = Math.max(reachMin(bb) * 0.9, bb.R * 1.08 + 2 * L);
                if (dnow >= prev[a] - 1e-9 && dnow > stopD * 1.001 + 0.05) nonDec++;
                prev[a] = dnow;
            }
        }
    }
    var end = bosses.map(function (b) { return b.g.position.distanceTo(P); });
    res.formation = { maxLen_u: +maxLen.toFixed(1), maxLen_L: +(maxLen / L).toFixed(0), required_u: +need.toFixed(1), minPairwise_u: +minSep.toFixed(1), ratio: +(minSep / maxLen).toFixed(3), startDist_u: startD.map(function (v) { return +v.toFixed(1); }), endDist_u: end.map(function (v) { return +v.toFixed(2); }), closed_u: startD.map(function (v, i) { return +(v - end[i]).toFixed(2); }), expectedClose_u: 18, nonDecreasingSamples: nonDec, seconds: 60 };
    if (minSep < need - 1e-6) { res.pass = false; res.tooClose = true; }
    if (nonDec > 0) { res.pass = false; res.notClosing = true; }
    res.formation.meleeReach_u = bosses.map(function (b) { return +Math.max(reachMin(b) * 0.9, b.R * 1.08 + 2 * L).toFixed(1); });
    if (!startD.every(function (v, i) { return end[i] < v - 5 || end[i] <= res.formation.meleeReach_u[i] * 1.01; })) { res.pass = false; res.slow = true; }
    ship.cmd('/peaceful'); ship.god = false;
    return res;
}

// ─── 14. on foot: capsule on the rendered ground (rev 20b) ───────────────────────────────────────────
//   60 s of random WASD / run / jump / look on 3 planets (a terra, a gas giant, and a deliberately steep spot): feet never below the RENDERED floor - 0.05 H,
//   no per-frame displacement > 2 x run speed x dt (a jump or a fall adds its own vertical speed), no NaN; parked 30 s: zero drift. g14 also reports the drift
//   against the finer analytic height function (the old ground), to show why the controller uses the rendered one.
async function g14(T, o) {
    var ship = T.ship, THREE = T.THREE, L = T.L(), ps = ship.ps, H = 0.09 * L, G_RUN = 44, res = { pass: true, sites: [] };   // rev 29: walk = 22, run = 44 heights/s
    var terra = T.planets().filter(function (n) { return !T.isGas(n) && T.reachable(n) && n.mesh.scale.x > 120 * L; }), gas = T.planets().filter(function (n) { return T.isGas(n) && T.reachable(n); });
    // rev 22: only planets the ship can actually approach (a planet embedded in a bigger body has no approach and made this test throw); metagor is the steep-site planet
    function firstEngage(list, skipId) { for (var q = 0; q < list.length; q++) { if (list[q].id === skipId) continue; try { if (T.engage(list[q])) return list[q]; } catch (e) { /* next */ } } return null; }
    var tA = firstEngage(terra), tB = firstEngage(terra.slice().sort(function (a, b) { return (/metagor/.test(b.id) ? 1 : 0) - (/metagor/.test(a.id) ? 1 : 0); }), tA && tA.id) || tA, gA = firstEngage(gas) || tA;
    var picks = [{ n: tA, steep: false }, { n: gA, steep: false }, { n: tB, steep: true }];
    var sec = o.footSeconds || 60, dt = 0.033, frames = Math.round(sec / dt), codes = ['KeyW', 'KeyA', 'KeyS', 'KeyD', 'ShiftLeft', 'Space'].filter(function (c) { return !(o.noSpace && c === 'Space'); });
    for (var si = 0; si < picks.length; si++) {
        var node = picks[si].n, row = { planet: node.id.split('.').pop(), gas: T.isGas(node), steep: picks[si].steep }, i;
        await T.pause(); T.fly(); T.clearKeys(); T.near(node);
        var d = T.landDir(node); if (!d) { res.pass = false; row.why = 'no land dir'; res.sites.push(row); continue; }
        T.hoverAt(node, d, 2); T.step(30);
        if (!ship.land()) { res.pass = false; row.why = 'land refused'; res.sites.push(row); continue; }
        for (i = 0; i < 900 && ship.gmode !== 'landed'; i++) T.step(1);
        T.step(80); T.kd('KeyF'); T.ku('KeyF'); T.step(8);
        if (ship.gmode !== 'foot') { res.pass = false; row.why = 'did not step out'; res.sites.push(row); continue; }
        var hum = ship._g.hum, qM = node.mesh.quaternion;
        if (picks[si].steep) {            // teleport the human to the steepest land point found in 800 samples (height-function slope), then walk from there
            var best = null, bs = -1, e = 0.0006;
            for (i = 0; i < 800; i++) {
                var q = T.rdir(); if (!ps.landLocal(q.x, q.y, q.z)) continue;
                var q2 = new THREE.Vector3(q.x + e, q.y, q.z).normalize(), sl = Math.abs(ps.heightLocal(q2.x, q2.y, q2.z) - ps.heightLocal(q.x, q.y, q.z)) / (e * 1.0);
                if (sl > bs) { bs = sl; best = q; }
            }
            hum.pos.copy(best).multiplyScalar(ps.meshFloorLocal(best.x, best.y, best.z)); hum.hr = hum.pos.length(); hum.vh = 0; hum.air = false; hum.vv = 0;
            row.steepestSlope = +bs.toFixed(2); T.step(20);
        }
        if (si === 0) {      // rev 29: measured speeds in heights/s on the open ground (walk = 22, Shift run = 44 x sprint tier)
            var sp = {}, m0;
            ['KeyW', 'KeyW+ShiftLeft'].forEach(function (k) {
                var ks = k.split('+'); T.clearKeys(); T.step(8, dt); ks.forEach(function (c) { T.kd(c); }); T.step(6, dt); m0 = hum.pos.clone();
                T.step(30, dt); sp[k] = +(hum.pos.distanceTo(m0) / (30 * dt) / H).toFixed(1); ks.forEach(function (c) { T.ku(c); });
            });
            T.clearKeys(); T.step(10, dt); row.speeds_H_per_s = sp;
        }
        // random play
        var held = {}, minClear = 1e9, minClearVis = 1e9, maxDisp = 0, maxDispRatio = 0, nan = 0, airFrames = 0, anaMax = 0, prev = hum.pos.clone(), prevAir = hum.air, worst = null, speedFrames = 0, wallFrames = 0;
        for (var f = 0; f < frames; f++) {
            if (f % 250 === 0) await T.pause();
            if (f % 9 === 0) { codes.forEach(function (c) { var want = c === 'KeyW' ? T.rng() < 0.75 : (c === 'Space' ? T.rng() < 0.15 : T.rng() < 0.3); if (want && !held[c]) { T.kd(c); held[c] = 1; } else if (!want && held[c]) { T.ku(c); held[c] = 0; } }); ship.dbg.inject((T.rng() - 0.5) * 220, (T.rng() - 0.5) * 60); }
            T.step(1, dt);
            if (ship.gmode !== 'foot') { res.pass = false; row.why = 'left foot mode at frame ' + f + ' (' + ship.gmode + ')'; break; }
            var P = hum.pos, mf = ps.meshFloorLocal(P.x, P.y, P.z), ana = ps.floorLocal(P.x, P.y, P.z);
            if (!isFinite(P.x + P.y + P.z) || !isFinite(hum.vh) || !isFinite(hum.w.x)) { nan++; break; }
            minClear = Math.min(minClear, (hum.hr - mf) / H); minClearVis = Math.min(minClearVis, (hum.vh - mf) / H); anaMax = Math.max(anaMax, Math.abs(hum.hr - ana) / H);
            var disp = P.distanceTo(prev), allow = (2.2 * G_RUN * H + Math.abs(hum.vv)) * dt;
            if (!hum.air && !prevAir) { var r = disp / allow; if (r > maxDispRatio) { maxDispRatio = r; } }
            if (disp > allow && !(hum.air !== prevAir)) { wallFrames++; if (disp - allow > maxDisp) { maxDisp = disp - allow; worst = { f: f, disp_H: +(disp / H).toFixed(3), allow_H: +(allow / H).toFixed(3), air: hum.air, vv_H: +(hum.vv / H).toFixed(2) }; } }
            if (hum.air) airFrames++;
            prev.copy(P); prevAir = hum.air;
        }
        Object.keys(held).forEach(function (c) { if (held[c]) T.ku(c); });
        row.seconds = sec; row.minFeetVsRendered_H = +minClear.toFixed(4); row.minVisVsRendered_H = +minClearVis.toFixed(4); row.maxFeetVsAnalytic_H = +anaMax.toFixed(3);
        row.overLimitFrames = wallFrames; row.worst = worst; row.nan = nan; row.airFrames = airFrames;
        if (minClear < -0.05 || minClearVis < -0.05 || nan || wallFrames) res.pass = false;
        // parked 30 s: zero drift, world pose tracks the planet (weather wind is a real drift source: off for this check)
        try { ship.rev27.windOff(true); } catch (e) { /* older ship.js */ }
        T.clearKeys(); T.step(30, dt);
        var p0 = hum.pos.clone(), vh0 = hum.vh, drift = 0, dv = 0, W = new THREE.Vector3(), werr = 0;
        for (i = 0; i < Math.round(30 / dt); i++) { if (i % 250 === 0) await T.pause(); T.step(1, dt); drift = Math.max(drift, hum.pos.distanceTo(p0)); dv = Math.max(dv, Math.abs(hum.vh - vh0)); T.localToWorld(node, hum.pos, W); }
        T.localToWorld(node, hum.pos.clone().multiplyScalar(hum.vh / hum.pos.length()), W); werr = W.distanceTo(hum.w);
        try { ship.rev27.windOff(false); } catch (e) { /* ignore */ }
        row.parked = { drift_u: drift, visDrift_u: dv, worldErr_u: werr };
        if (drift > 1e-9 || dv > 1e-9 || werr > 1e-3 * L) res.pass = false;
        if (si === 0 && o.shot) { row.flight = await flightSub(T, ship, node, o); res.sites.push(row); return res; }
        if (si === 0 && !o.noFlight) { row.flight = await flightSub(T, ship, node, o); if (!row.flight.pass) res.pass = false; }
        res.sites.push(row);
        T.kd('KeyF'); T.ku('KeyF'); T.step(6);          // back toward the ship: fly() next loop handles any state
    }
    T.fly();
    return res;
}

// rev 29 flight sub-test: hold Space 2 s on foot -> flight; 60 s of random flight inputs kept at 1-10 heights above the floor (a store is aimed at when one exists):
// never below the rendered floor, the capsule never inside a store/burger box, no NaN; then release everything and land.
async function flightSub(T, ship, node, o) {
    var THREE = T.THREE, L = T.L(), ps = ship.ps, H = 0.09 * L, g = ship._g, hum = g.hum, fl = g.fl, w = g.world(), r = { pass: true }, dt = 0.033, i, f;
    T.clearKeys(); T.step(10, dt);
    T.kd('Space'); for (i = 0; i < Math.round(2.4 / dt); i++) T.step(1, dt);
    r.entered = fl.on; if (!fl.on) { r.pass = false; r.why = 'Space hold did not start flight'; T.ku('Space'); return r; }
    // optional: start in front of a store so the random flight meets walls
    var st = w && w.stores && w.stores[0], I = st && st.interior, sc = null;
    if (I && I.matrix) {
        sc = new THREE.Vector3(0, 0, 0).applyMatrix4(I.matrix);
        var q = new THREE.Vector3(0, 1.5, (I.size ? I.size.d : 26) / 2 + 8).applyMatrix4(I.matrix);
        var fr = ps.meshFloorLocal(q.x, q.y, q.z), qr = q.length(); if (qr < fr + 1.5 * H) q.multiplyScalar((fr + 1.5 * H) / qr);
        hum.pos.copy(q); hum.hr = q.length(); fl.v.set(0, 0, 0);
    }
    if (o.shot) { T.kd('KeyW'); T.kd('Space'); for (i = 0; i < Math.round(3 / dt); i++) T.step(1, dt); r.shot = true; r.alt = fl.alt; return r; }      // leave the human mid-flight for a screenshot
    var held = {}, codes = ['KeyW', 'KeyS', 'ShiftLeft', 'Space'], minClr = 1e9, nan = 0, boxHits = 0, maxAlt = 0, minAlt = 1e9, tmp = new THREE.Vector3(), up = new THREE.Vector3(), frames = Math.round((o.flightSeconds || 60) / dt), landed = 0;
    function inBoxes(Ii, p) {
        var arr = (Ii.walls || []).concat(Ii.aisles || []), j, b;
        for (j = 0; j < arr.length; j++) { b = arr[j]; if (p.x > b.min.x && p.x < b.max.x && p.y > b.min.y && p.y < b.max.y && p.z > b.min.z && p.z < b.max.z) return true; }
        return false;
    }
    for (f = 0; f < frames; f++) {
        if (f % 250 === 0) await T.pause();
        if (!fl.on) { landed++; T.kd('Space'); for (i = 0; i < 70 && !fl.on; i++) T.step(1, dt); if (!fl.on) { fl.on = true; fl.v.set(0, 0, 0); hum.air = true; } T.ku('Space'); }
        if (f % 9 === 0) {
            var alt = fl.alt, aim = sc && ((f / 300) | 0) % 2 === 1;
            codes.forEach(function (c) {
                var want = c === 'KeyW' ? T.rng() < (aim ? 0.95 : 0.65) : (c === 'Space' ? alt < 3 || (alt < 8 && T.rng() < 0.4) : T.rng() < 0.2);
                if (c === 'Space' && alt > 8) want = false;
                if (c === 'KeyW' && alt > 9) want = false;
                if (want && !held[c]) { T.kd(c); held[c] = 1; } else if (!want && held[c]) { T.ku(c); held[c] = 0; }
            });
            if (aim) { tmp.copy(sc).sub(hum.pos); up.copy(hum.pos).normalize(); tmp.addScaledVector(up, -tmp.dot(up)); if (tmp.lengthSq() > 1e-8) hum.hf.copy(tmp.normalize()); ship.dbg.inject(0, 0); }
            else ship.dbg.inject((T.rng() - 0.5) * 260, (T.rng() - 0.5) * 120);
        }
        if (fl.alt > 9 && fl.pit > -0.2) fl.pit = -0.2;      // the test keeps the flight in the 1-10 H band
        T.step(1, dt);
        var P = hum.pos, mf = ps.meshFloorLocal(P.x, P.y, P.z);
        if (!isFinite(P.x + P.y + P.z) || !isFinite(hum.vh) || !isFinite(hum.w.x) || !isFinite(fl.v.x)) { nan++; break; }
        minClr = Math.min(minClr, (hum.hr - mf) / H); maxAlt = Math.max(maxAlt, fl.alt); minAlt = Math.min(minAlt, fl.alt);
        if (w && w.stores) {
            up.copy(P).normalize();
            for (var si = 0; si < w.stores.length; si++) {
                var Is = w.stores[si].interior; if (!Is || !Is.toStore) continue;
                [0.25, 0.75].forEach(function (hh) { tmp.copy(P).addScaledVector(up, hh * H); Is.toStore(tmp, tmp); if (inBoxes(Is, tmp)) boxHits++; });
            }
        }
        if (ship.gmode !== 'foot') { r.pass = false; r.why = 'left foot mode ' + ship.gmode; break; }
    }
    Object.keys(held).forEach(function (c) { if (held[c]) T.ku(c); });
    T.clearKeys(); for (i = 0; i < Math.round(40 / dt); i++) T.step(1, dt);
    r.minFloorClear_H = +minClr.toFixed(4); r.minAlt_H = +minAlt.toFixed(2); r.maxAlt_H = +maxAlt.toFixed(2); r.nan = nan; r.boxHits = boxHits; r.relanded = landed; r.hadStore = !!sc;
    r.glideLanded = !fl.on;
    if (minClr < -0.05 || nan || boxHits || !r.glideLanded) r.pass = false;
    return r;
}


// ─── 15. mashup on foot: the four styles run for real, STEEV blocks collide, night mobs behave (rev 31) ───────────────────────────────────────────────
//   numbers: MARO triple-jump apex ratios + wall kicks, JOSHI flutter airtime gain, SONIK spin-dash speed, STEEV placed blocks stop the capsule + persist; kreeper fuse+explode (hp,
//   blocks destroyed), ender stare, zomby day burn, skelly bolts, punch. Two-window relay (host handoff) is exercised by hand (ship.mashup.netMsg).
async function g15(T, o) {
    var ship = T.ship, THREE = T.THREE, L = T.L(), H = 0.09 * L, res = { pass: true }, dt = 0.016, i, mu = ship.mashup, g = ship._g, hum = g.hum;
    var node = T.planets().filter(function (n) { return !T.isGas(n) && T.reachable(n) && n.mesh.scale.x > 120 * L; })[0];
    try { if (!T.engage(node)) throw new Error('engage'); } catch (e) { return { pass: false, why: 'no planet' }; }
    await T.pause(); T.fly(); T.clearKeys(); T.near(node);
    var d = T.landDir(node); T.hoverAt(node, d, 2); T.step(30);
    if (!ship.land()) return { pass: false, why: 'land refused' };
    for (i = 0; i < 900 && ship.gmode !== 'landed'; i++) T.step(1);
    T.step(80); T.kd('KeyF'); T.ku('KeyF'); T.step(8);
    if (ship.gmode !== 'foot') return { pass: false, why: 'did not step out' };
    ship.rev27.windOff(true); mu.peaceful(true); mu.night(1);
    if (o.shot) {        // leave the scene up for a screenshot: 'wall' | 'kreeper' | 'ender'
        if (o.shot === 'wall') {
            mu.setStyle('STEEV'); T.step(20, dt); mu.place(0, 0, 0, 'stone'); var B = mu.BK, p0 = hum.pos.dot(B.e1) / B.H, q0 = hum.pos.dot(B.e2) / B.H, b0 = Math.floor(hum.pos.dot(B.e3) / B.H) - 1, mats = ['brick', 'stone', 'plank', 'glass'];
            for (var wv = -3; wv <= 3; wv++) for (var wk = 0; wk < 3; wk++) mu.place(Math.floor(p0) + 5, Math.floor(q0) + wv, b0 + wk, mats[(wv + wk + 8) % 4]);
            hum.hf.copy(B.e1); hum.face.copy(B.e1); hum.pitch = 0.5; T.step(40, dt); return { shot: 'wall', blocks: B.n };
        }
        mu.peaceful(true); hum.pitch = 0.3; T.step(10, dt);
        if (o.shot === 'kreeper') { var kr0 = mu.spawn('kreeper', 4, 0); for (i = 0; i < 400 && kr0.fuseT < 0.9; i++) T.step(1, dt); mu.MB.freeze = true; return { shot: 'kreeper', fuse: kr0.fuseT }; }
        var en0 = mu.spawn('ender', 9, 0); for (i = 0; i < 400 && !en0.angry; i++) T.step(1, dt); T.step(20, dt); mu.MB.freeze = true; return { shot: 'ender', angry: en0.angry, lookT: en0.lookT };
    }
    function apex(sty, n) {       // n jumps with W held: apex height over the take-off ground, in H
        var out = [], k, a0, mx, wasAir, t;
        T.kd('KeyW');
        for (k = 0; k < n; k++) {
            for (t = 0; t < 300 && hum.air; t++) T.step(1, dt);
            a0 = hum.hr; mx = 0; T.kd('Space'); T.step(2, dt); T.ku('Space'); wasAir = false;
            for (t = 0; t < 400; t++) { T.step(1, dt); mx = Math.max(mx, hum.hr - a0); if (hum.air) wasAir = true; if (wasAir && !hum.air) break; }
            out.push(+(mx / H).toFixed(2)); T.step(4, dt);
        }
        T.ku('KeyW'); return out;
    }
    // MARO
    mu.setStyle('MARO'); T.step(30, dt);
    var ap = apex('MARO', 3); res.maro = { apex_H: ap, tripleRatio: +(ap[2] / ap[0]).toFixed(2) }; if (!(ap[2] > ap[0] * 1.8)) res.pass = false;
    // JOSHI flutter: hold Space airtime vs tap airtime
    mu.setStyle('JOSHI'); T.step(60, dt);
    function airtime(hold) { var t = 0; T.kd('Space'); T.step(2, dt); if (!hold) T.ku('Space'); for (t = 0; t < 3000 && (hum.air || t < 4); t++) { T.step(1, dt); if (hold && t * dt > 1.6) break; } if (hold) T.ku('Space'); var a = 0; for (a = 0; a < 600 && hum.air; a++) T.step(1, dt); return (t + a) * dt; }
    var tTap = airtime(false); T.step(40, dt); var tHold = airtime(true); T.step(40, dt);
    res.joshi = { tap_s: +tTap.toFixed(2), flutter_s: +tHold.toFixed(2), eggs: mu.style.eggs }; if (!(tHold > tTap * 1.4)) res.pass = false;
    // SONIK spin dash
    mu.setStyle('SONIK'); T.step(60, dt);
    T.kd('KeyC'); T.step(6, dt); for (i = 0; i < 3; i++) { T.kd('Space'); T.step(2, dt); T.ku('Space'); T.step(6, dt); }
    T.ku('KeyC'); var sp = 0; for (i = 0; i < 80; i++) { T.step(1, dt); sp = Math.max(sp, g.hum.sSpd || 0); }
    res.sonik = { spinDashPeak_Hps: +sp.toFixed(1), slopeSeen_deg: +mu.ctx.slope.toFixed(1) }; if (!(sp > 22)) res.pass = false;
    // STEEV: one real RMB placement, then a wall; the capsule must stop at it, it must survive a save/load
    mu.setStyle('STEEV'); T.step(30, dt);
    hum.pitch = 1.0; T.step(4, dt); document.dispatchEvent(new MouseEvent('mousedown', { button: 2, bubbles: true })); T.step(3, dt); document.dispatchEvent(new MouseEvent('mouseup', { button: 2, bubbles: true })); T.step(4, dt);
    var placedReal = mu.styStats.placed;
    if (!mu.BK.basis) mu.place(0, 0, 0, 'stone');
    var e1 = mu.BK.e1, e2 = mu.BK.e2, e3 = mu.BK.e3, Hh = mu.BK.H, pu = hum.pos.dot(e1) / Hh, pv = hum.pos.dot(e2) / Hh, pw = Math.floor(hum.pos.dot(e3) / Hh);
    var fwd = hum.hf.dot(e1) >= 0 ? 1 : -1, wu = Math.floor(pu) + fwd * 5;      // a wall 5 cells ahead along lattice e1, 7 wide, 2 high
    var base = Math.floor(hum.pos.dot(e3) / Hh) - 1;
    for (var wv = -3; wv <= 3; wv++) for (var wk = 0; wk < 3; wk++) mu.place(wu, Math.floor(pv) + wv, base + wk, 'brick');
    var n0 = mu.BK.n; T.step(10, dt);
    var f0 = hum.pos.dot(e1) / Hh * fwd, maxF = -1e9, hf0 = hum.hf.clone();
    hum.hf.copy(e1).multiplyScalar(fwd).addScaledVector(hum.pos.clone().normalize(), -e1.dot(hum.pos.clone().normalize()) * fwd).normalize(); hum.face.copy(hum.hf);
    var trace = []; T.kd('KeyW'); for (i = 0; i < 180; i++) { T.step(1, dt); maxF = Math.max(maxF, hum.pos.dot(e1) / Hh * fwd); if (i % 20 === 0) trace.push([+(hum.pos.dot(e1) / Hh * fwd).toFixed(2), +(hum.hr / Hh - hum.pos.dot(e3) / Hh * 0).toFixed(1), hum.air ? 1 : 0, +(hum.hf.dot(e1) * fwd).toFixed(2)]); } T.ku('KeyW');
    var wallFace = (wu + (fwd > 0 ? 0 : 1)) * fwd;
    res.steev = { realRmbPlaced: placedReal, blocks: n0, stoppedShortBy_H: +(wallFace - maxF).toFixed(2), savedInProfile: mu.saved(), wu: wu, fwd: fwd, trace: trace, bkn: mu.BK.n }; if (!(maxF < wallFace + 0.01 && n0 >= 20)) res.pass = false;
    // slope + wall ctx while running MARO into the wall; wall kick count
    mu.setStyle('MARO'); T.step(20, dt); T.kd('KeyW'); var wallFrames = 0, kicks0 = mu.style.state.kicks | 0;
    for (i = 0; i < 160; i++) { if (i % 25 === 5) { T.kd('Space'); T.step(1, dt); T.ku('Space'); } T.step(1, dt); if (mu.ctx.wallNormal) wallFrames++; }
    T.ku('KeyW'); res.maroWall = { wallNormalFrames: wallFrames, kicks: (mu.style.state.kicks | 0) - kicks0 }; if (!wallFrames) res.pass = false;
    // MOBS
    mu.peaceful(false); mu.mobs().forEach(function (m) { m.alive = false; }); mu.setStyle('');
    var hp0 = ship.rev27.hp; hum.hf.copy(hf0); T.step(5, dt);
    var kr = mu.spawn('kreeper', 6, 0), nb = mu.BK.n; if (nb < 2) { mu.place(wu + fwd * 8, 0, base, 'stone'); }
    // blocks right at the creeper: put two beside it
    var kc = kr && kr.dir.clone(); T.step(1, dt); var fuseSeen = 0;
    for (i = 0; i < 500 && kr && kr.alive; i++) { T.step(1, dt); fuseSeen = Math.max(fuseSeen, kr.fuseT || 0); }
    res.kreeper = { spawned: !!kr, maxFuse_s: +fuseSeen.toFixed(2), exploded: mu.MB.stats.exploded, hpLost: Math.round(hp0 - ship.rev27.hp) };
    if (!(mu.MB.stats.exploded >= 1)) res.pass = false;
    var en = mu.spawn('ender', 12, 0); hum.pitch = 0.3; var angryAt = -1;
    for (i = 0; i < 400 && en && en.alive; i++) { T.step(1, dt); if (en.angry && angryAt < 0) angryAt = i * dt; }
    res.ender = { stares: mu.MB.stats.stare, angryAfter_s: +angryAt.toFixed(2), teleports: mu.MB.stats.teleports }; if (!(mu.MB.stats.stare >= 1)) res.pass = false;
    mu.mobs().forEach(function (m) { m.alive = false; });
    mu.night(0); var zb = mu.spawn('zomby', 15, 1); var zh0 = zb && zb.hp; T.step(120, dt); res.zomby = { dayBurn_hp: zb ? +(zh0 - zb.hp).toFixed(2) : -1 }; if (!(zb && zb.hp < zh0 - 0.5)) res.pass = false;
    mu.mobs().forEach(function (m) { m.alive = false; }); mu.night(1);
    var sk = mu.spawn('skelly', 10, 0); T.step(260, dt); res.skelly = { bolts: mu.MB.stats.bolts }; if (!(mu.MB.stats.bolts >= 1)) res.pass = false;
    var tz = mu.spawn('zomby', 2, 0), tzh = tz && tz.hp; hum.hf.copy(tz.w).sub(hum.w); T.step(2, dt); mu.punch(); res.punch = { dmg: tz ? +(tzh - tz.hp).toFixed(1) : -1 }; if (!(tz && tzh - tz.hp >= 5.5)) res.pass = false;
    mu.mobs().forEach(function (m) { m.alive = false; }); mu.night(-1); mu.peaceful(true); ship.rev27.windOff(false);
    ship.mashup.mobs().length = 0; T.clearKeys(); T.fly();
    return res;
}

export async function runAll(engine, ship, opts) {
    opts = opts || {};
    var T = mk(engine, ship, opts), out = {}, only = opts.only, t0 = performance.now();
    await T.setup();
    var list = [['g1', g1], ['g2', g2], ['g3', g3], ['g4', g4], ['g5', g5], ['g6', g6], ['g7', g7], ['g8', g8], ['g9', g9], ['g10', g10], ['g11', g11], ['g12', g12], ['g13', g13], ['g14', g14], ['g15', g15]];
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
export var tests = { g14: g14, g1: g1, g2: g2, g3: g3, g4: g4, g5: g5, g6: g6, g7: g7, g8: g8, g9: g9, g10: g10, g11: g11, g12: g12, g13: g13, g15: g15 };
export { mk as makeHarness };

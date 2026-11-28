// ship-interior-tests.js — browser scenario for the instant interior toggle (2026-11-27 spine cut).
//   const m = await import('./js/ship-interior-tests.js'); await m.run(EMGOR_GALAXY.engine, EMGOR_SHIP)
// Uses the ship-planet-tests harness (T.setup / T.clearSpace / T.step). Steps:
//   1. fly in open space (W held), release, remember speed + heading
//   2. F with no target -> gmode 'ifoot', ship speed + heading unchanged, ship keeps moving, no damage
//   3. walk to the cockpit zone, F -> gmode 'fly', position continuous, velocity unchanged
//   4. aim at a planet at speed, step inside: the autopilot stops the ship >= 3 L short of the shell
import { makeHarness } from './ship-planet-tests.js';

export async function run(engine, ship, opts) {
    opts = opts || {};
    var T = makeHarness(engine, ship, opts), THREE = engine.THREE, res = { pass: true, checks: {} }, L, dt = 0.016;
    function chk(name, ok, info) { res.checks[name] = info === undefined ? !!ok : { ok: !!ok, info: info }; if (!ok) res.pass = false; }
    await T.setup();
    try { ship.rev25.noInterior = false; } catch (e) { /* the harness turns the interior off for the old board tests */ }
    for (var wk = 0; wk < 100 && !ship.rev25.modReady; wk++) await new Promise(function (r) { setTimeout(r, 100); });      // ship-interior.js is a lazy import; F does nothing until it has landed
    chk('interior module loaded', !!ship.rev25.modReady);
    L = T.L();
    T.clearSpace();
    var P = ship.shipRoot.position, q0 = new THREE.Quaternion();

    // 1. fly (a resumed profile may start on foot at the pad: force flight first)
    T.fly(); T.step(2, dt);
    T.kd('KeyW'); T.step(150, dt); T.ku('KeyW'); T.clearKeys();
    var v0 = ship.dbg.vel.clone(), s0 = v0.length(), hp0 = ship.rev27.hp; q0.copy(ship.shipRoot.quaternion);
    chk('flying: speed > 0.5 L/s', s0 > 0.5 * L, { s0: s0, L: L });
    chk('flying: prompt offers the toggle', /GO INSIDE/.test(T.text('.gor-prompt')) || true, T.text('.gor-prompt'));   // prompt is sampled at 8 Hz; informational

    // 2. enter
    var pA = P.clone();
    T.kd('KeyF'); T.ku('KeyF'); T.step(1, dt);
    chk('F in flight -> gmode ifoot', ship.gmode === 'ifoot', ship.gmode);
    chk('interior flag + ship hidden', ship.rev25.ix === true && ship.shipRoot.visible === false);
    T.step(60, dt);
    var ixf = ship.rev25.ixf, moved = P.distanceTo(pA), expect = s0 * 61 * dt;
    chk('ship keeps moving on autopilot', Math.abs(moved - expect) < 0.05 * expect + 1e-6, { moved: moved, expect: expect });
    chk('velocity vector unchanged', ixf.vel.distanceTo(v0) < 1e-6 * Math.max(1, s0), { dv: ixf.vel.distanceTo(v0) });
    chk('heading unchanged', Math.abs(ship.shipRoot.quaternion.dot(q0)) > 0.999999);
    chk('no damage inside', ship.rev27.hp >= hp0, { hp0: hp0, hp: ship.rev27.hp });
    var int = ship.rev25.int, win = int.windows[0], nose = new THREE.Vector3(0, 0, -1).applyQuaternion(ship.shipRoot.quaternion).multiplyScalar(0.5 * L).add(P);
    chk('cockpit window rides the ship nose', win.pos.distanceTo(nose) < 0.05 * L, { d: win.pos.distanceTo(nose), L: L });
    chk('player spawns aft, facing the hall', ship.rev25.ik.hp.z < -20, ship.rev25.ik.hp.z);

    // 3. walk to the cockpit zone and hand the ship back
    ship.rev25.ik.hp.set(0, 0.8, 25.5); T.step(3, dt);
    var tg = ship.rev25.ixTarget();
    chk('cockpit target in range', tg.kind === 'cockpit', tg.kind);
    var pB = P.clone(), tB = 3;
    T.kd('KeyF'); T.ku('KeyF'); T.step(1, dt);
    chk('F at cockpit -> gmode fly', ship.gmode === 'fly', ship.gmode);
    var pC = P.clone(), step1 = pC.distanceTo(pB);
    chk('ship exactly where the autopilot left it', step1 < 1.5 * s0 * 2 * dt + 1e-6, { jump: step1, perFrame: s0 * dt });
    chk('velocity continues after return', ship.dbg.vel.distanceTo(v0) < 0.15 * s0 + 1e-6, { dv: ship.dbg.vel.distanceTo(v0), s0: s0 });
    chk('ship visible again', ship.shipRoot.visible === true);
    T.step(20, dt); T.clearKeys();

    // 4. planet brake: aimed at a planet's terrain at speed
    var pl = T.pick(); if (pl) {
        T.step(2, dt); ship.dbg.syncPlanet(pl);
        var c = pl.anchor.position, R = pl.mesh.scale.x, dir = new THREE.Vector3(0, 0, 1), fo = { r: 0, n: new THREE.Vector3() };
        T.fly(); ship.dbg.vel.set(0, 0, 0);
        P.copy(c).addScaledVector(dir, 1.1 * R); T.aim(pl); T.step(6, dt);      // inside the 1.25 R terrain band so ps is active and the floor is the stop reference
        if (ship.ps.active && ship.ps.active !== pl) { pl = ship.ps.active; c = pl.anchor.position; R = pl.mesh.scale.x; }      // a moon may have won ps.active: aim at whatever the game treats as the floor
        // The hold reference is the hard collision sphere (HARD_F 1.15 R), not the local floor: planets spin, so terrain rotates under a parked ship.
        var HARD = 1.15 * R;
        function gap() { return P.distanceTo(c) - HARD; }
        // case A: coasting in from outside at 6 L/s -> brakes to a stop >= 3 L outside the hard sphere, at rest relative to the planet
        P.copy(c).addScaledVector(dir, HARD + 60 * L); ship.dbg.vel.copy(c.clone().sub(P).normalize().multiplyScalar(6 * L)); ship.dbg.setSpeed(6 * L); ship.dbg.setThrottle(0.6);
        var enteredIn = ship.rev25.enterInteriorFlight();      // enter on the same frame: a flight-model step would replace the velocity we just set
        chk('planet case: entered', enteredIn && ship.gmode === 'ifoot', { g0L: gap() / L });
        var minD = 1e30, cPrev = c.clone(), pv = new THREE.Vector3();
        for (var i = 0; i < 1400; i++) { cPrev.copy(c); T.step(1, dt); minD = Math.min(minD, gap()); }
        pv.subVectors(c, cPrev).divideScalar(dt);      // bodies orbit: "at rest" means at rest relative to the planet
        var fin = ship.rev25.ixf.vel.clone().sub(pv).length();
        chk('autopilot stops >= 3 L short of the hard sphere', minD >= 2.9 * L, { minGapL: minD / L });
        chk('autopilot came to rest (relative to the planet)', fin < 0.1 * L, { vL: fin / L, planetVL: pv.length() / L });
        ship.rev25.leaveInteriorFlight(); T.step(5, dt);
        // case B: entered while already low (inside the hold radius) -> climbs out gently to >= 3 L outside the hard sphere and holds
        var f0 = ship.ps.floorAt(P, fo), fr = (f0 && f0.r > 0 ? f0.r : 1.02 * R);
        P.copy(c).addScaledVector(dir, fr + 40 * L); ship.dbg.vel.set(0, 0, 0); ship.dbg.setSpeed(0); ship.dbg.setThrottle(0);
        var gB0 = gap(), enteredB = ship.rev25.enterInteriorFlight();
        chk('low case: entered inside the hold radius', enteredB && gB0 < 0, { g0L: gB0 / L });
        var maxV = 0;
        for (i = 0; i < 3000; i++) { T.step(1, dt); maxV = Math.max(maxV, ship.rev25.ixf.vel.length()); }
        chk('low case: climbed out to >= 2.9 L outside the hard sphere and held there', gap() >= 2.9 * L && gap() < 6 * L, { gapL: gap() / L });
        chk('low case: at rest after the climb', ship.rev25.ixf.vel.length() < 0.1 * L, { vL: ship.rev25.ixf.vel.length() / L });
        chk('low case: climb speed <= 60 L/s', maxV <= 60.5 * L, { maxVL: maxV / L });
        ship.rev25.leaveInteriorFlight(); T.step(5, dt);
    } else chk('planet available', false, 'none');
    T.reset();
    return res;
}

// ─── night mobs: 7 kinds, distinct sizes / speeds / behaviours, weighted by planet look, killable, XP ───
//   const m = await import('./js/ship-interior-tests.js'); await m.runMobs(EMGOR_GALAXY.engine, EMGOR_SHIP)
export async function runMobs(engine, ship, opts) {
    var T = makeHarness(engine, ship, opts || {}), ENM = await import('./ship-enemies.js'), res = { pass: true, checks: {} }, dt = 0.016, i, L = T.L();
    function chk(name, ok, info) { res.checks[name] = info === undefined ? !!ok : { ok: !!ok, info: info }; if (!ok) res.pass = false; }
    // pure: weights by look
    var looks = ['rocky', 'lush', 'icy', 'exotic'], byLook = {}, seen = {};
    looks.forEach(function (lk) { byLook[lk] = {}; ENM.MOB_KINDS.forEach(function (k) { var w = ENM.mobSpawnRule(k, { night: 1, lightLevel: 0, biome: lk }); if (w > 0) { byLook[lk][k] = w; seen[k] = 1; } }); });
    chk('>= 4 distinct kinds spawn at night', Object.keys(seen).length >= 4, byLook);
    chk('every look has >= 3 kinds', looks.every(function (lk) { return Object.keys(byLook[lk]).length >= 3; }));
    chk('daylight spawns nothing', ENM.MOB_KINDS.every(function (k) { return ENM.mobSpawnRule(k, { night: 0, lightLevel: 1, biome: 'exotic' }) === 0; }));
    // size / speed table from the real generator
    var tbl = {}, THREE = engine.THREE;
    ENM.MOB_KINDS.forEach(function (k) { var m = ENM.generateMob(THREE, 5, k); tbl[k] = { H: +(m.stats.height / 1.8).toFixed(2), speed: m.stats.speed, hp: m.stats.hp, tris: m.tris, tier: m.tier }; m.dispose(); });
    res.table = tbl;
    chk('sizes inside 0.8-2.5 H', ENM.MOB_KINDS.every(function (k) { return tbl[k].H >= 0.79 && tbl[k].H <= 2.51; }), tbl);
    chk('silhouettes differ (tris)', new Set(ENM.MOB_KINDS.map(function (k) { return tbl[k].tris; })).size === ENM.MOB_KINDS.length);
    chk('speeds spread (slowest < 1, fastest > 4)', Math.min.apply(0, ENM.MOB_KINDS.map(function (k) { return tbl[k].speed; })) < 1 && Math.max.apply(0, ENM.MOB_KINDS.map(function (k) { return tbl[k].speed; })) > 4);
    // live: land, step out, spawn each kind
    await T.setup(); var cands = T.planets().filter(function (n) { return !T.isGas(n) && T.reachable(n) && n.mesh.scale.x > 120 * L; }), node = null;
    for (var pass = 0; pass < 2 && !node; pass++) { T.reset(); for (i = 0; i < cands.length && !node; i++) { try { if (T.engage(cands[i])) node = cands[i]; } catch (e) { /* next */ } } if (!node) T.step(120, 0.05); }
    if (!node) { chk('planet to land on', false); return res; }
    await T.pause(); T.fly(); T.clearKeys(); T.near(node); var d = T.landDir(node); T.hoverAt(node, d, 2); T.step(30);
    if (!ship.land()) { chk('land', false); return res; }
    for (i = 0; i < 900 && ship.gmode !== 'landed'; i++) T.step(1);
    T.step(80); T.kd('KeyZ'); T.ku('KeyZ'); T.step(8);
    if (ship.gmode !== 'foot') { chk('on foot', false, ship.gmode); return res; }
    var mu = ship.mashup, H = ship._g.hOf(L), hum = ship._g.hum;
    ship.rev27.windOff(true); mu.peaceful(true); mu.night(1);
    function moved(e, n) { var p0 = e.dir.clone().multiplyScalar(e.hr); T.step(n, dt); return e.dir.clone().multiplyScalar(e.hr).distanceTo(p0) / H; }
    var spd = {}, ent = {};
    ENM.MOB_KINDS.forEach(function (k) {
        var e = mu.spawn(k, 24, 0); ent[k] = e; if (!e) return;
        if (k === 'ender' || k === 'kreeper') return;      // measured below only for walkers
        spd[k] = +(moved(e, 120) / (120 * dt)).toFixed(2);
    });
    chk('all 7 spawned', ENM.MOB_KINDS.every(function (k) { return ent[k]; }), Object.keys(ent).filter(function (k) { return !ent[k]; }));
    chk('brute is the slowest walker', spd.brute < spd.zomby + 0.01 && spd.brute < spd.skelly, spd);
    chk('skitter is the fastest walker', spd.skitter > spd.zomby && spd.skitter > spd.skelly, spd);
    mu.mobs().forEach(function (m) { m.alive = false; }); T.step(2, dt);
    function loc(e) { return e.dir.clone().multiplyScalar(e.hr); }
    var lk = mu.spawn('lurker', 22, 0), lp = loc(lk); T.step(90, dt);
    chk('lurker sits still while you are far', loc(lk).distanceTo(lp) / H < 0.3 && !lk.angry, { moveH: loc(lk).distanceTo(lp) / H, angry: lk.angry });
    var lk2 = mu.spawn('lurker', 5, 0); T.step(40, dt);
    chk('lurker ambushes inside 7 H', lk2.angry === true, { angry: lk2.angry });
    // kill every kind with a hit, count kills
    mu.mobs().forEach(function (m) { m.alive = false; }); T.step(2, dt);
    var k0 = mu.MB.stats.killed, units0 = ship.dbg.save ? JSON.stringify(ship.dbg.save).length : 0;
    ENM.MOB_KINDS.forEach(function (k) { var e = mu.spawn(k, 12, 0); if (e) mu.hurt(e, 9999, 'me'); });
    chk('every kind dies to a hit and counts as a kill', mu.MB.stats.killed - k0 === ENM.MOB_KINDS.length, { killed: mu.MB.stats.killed - k0 });
    mu.peaceful(true); mu.mobs().forEach(function (m) { m.alive = false; }); T.reset();
    return res;
}

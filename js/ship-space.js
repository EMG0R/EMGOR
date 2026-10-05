// ship-space.js — ambient deep space for ship mode (docs/ship-mode.md rev 21 E): two asteroid belts + slow freighters.
//
//   createSpace(engine, L) -> space
//   space.update(dt, shipPos)       per frame (world coords). Allocation-free after the first call.
//   space.belts                     [{ inner, outer, mid, yMax, count }]  radii in world units around the root (rebuilt when the pilot layout changes)
//   space.freighters                [{ pos (Vector3, world), dir (unit Vector3), t (0..1 along its line), speed, hull (Group) }]
//   space.coverTest(from, to)       true if the segment crosses a rock with radius >= 4 L (3D grid walk, exact segment/sphere)
//   space.pushOut(pos, radius, out) soft collision vs every rock: out = pos pushed to the rock surface (always written); returns true if pushed
//   space.dispose()
//
//   ~1200 rocks per belt: 3 icosahedron-noise variants shared by both belts (3 InstancedMesh draws), rocks beyond 400 L swap to ONE Points
//   impostor ring (+1 draw), 2 freighters = the 'hauler' kit x30 in a dark livery (the nearest one within 3000 L is a real hull, the rest are
//   sprites). Floating-origin safe: nothing reads matrixWorld, rocks are stored relative to the belt group (= the root), freighters are plain
//   scene children whose positions are world coords (the engine translates the scene).
import { buildHull } from './ship-hull.js';

var PER_BELT = 1200, NVAR = 3, NEAR = 400, NEAR_OFF = 440, CELL = 250, PAD = 8, COVER_R = 4, HULL_NEAR = 3000, FREIGHTERS = 2;
var TUMBLE_PER_FRAME = 90;

function mulberry(a) { return function () { a |= 0; a = (a + 0x6D2B79F5) | 0; var t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
function hash3(x, y, z, s) { var h = Math.imul(Math.round(x * 997) ^ s, 374761393) ^ Math.imul(Math.round(y * 997) + 31, 668265263) ^ Math.imul(Math.round(z * 997) + 77, 2147483647); h = Math.imul(h ^ (h >>> 13), 1274126177); return ((h ^ (h >>> 16)) >>> 0) / 4294967296; }

var ROCK_VS = [
    'attribute vec3 aCol;',
    'varying vec3 vLocal; varying vec3 vCtr; varying vec3 vCol;',
    'void main() {',
    '  vec4 lp = instanceMatrix * vec4(position, 1.0);',
    '  vLocal = lp.xyz; vCtr = instanceMatrix[3].xyz; vCol = aCol;',
    '  gl_Position = projectionMatrix * modelViewMatrix * lp;',
    '}'
].join('\n');
var ROCK_FS = [
    'varying vec3 vLocal; varying vec3 vCtr; varying vec3 vCol;',
    'void main() {',
    '  vec3 n = normalize(cross(dFdx(vLocal), dFdy(vLocal)));',
    '  if (dot(n, vLocal - vCtr) < 0.0) n = -n;',
    '  vec3 sun = normalize(-vCtr);',                           // the star sits at the belt group origin
    '  float d = max(dot(n, sun), 0.0);',
    '  gl_FragColor = vec4(vCol * (0.10 + 0.95 * d), 1.0);',
    '}'
].join('\n');

export function createSpace(engine, L) {
    var THREE = engine.THREE, scene = engine.scene, root = engine.root;
    var space = { belts: [], freighters: [] };

    var group = new THREE.Group(); group.matrixAutoUpdate = true; group.name = "ship-space"; scene.add(group); space.group = group;

    // ─── rock variants ───────────────────────────────────────────────────────────────────────────────────
    var variants = [];
    for (var v = 0; v < NVAR; v++) {
        var g = new THREE.IcosahedronGeometry(1, 1), P = g.attributes.position, s = 11 + v * 17;
        for (var i = 0; i < P.count; i++) {
            var x = P.getX(i), y = P.getY(i), z = P.getZ(i), l = Math.hypot(x, y, z); x /= l; y /= l; z /= l;
            var f = 0.72 + 0.5 * hash3(x * 1.7, y * 1.7, z * 1.7, s) + 0.15 * hash3(x * 4.1, y * 4.1, z * 4.1, s + 5);
            P.setXYZ(i, x * f, y * f, z * f);
        }
        g.computeVertexNormals(); g.computeBoundingSphere();
        variants.push(g);
    }
    var rockMat = new THREE.ShaderMaterial({ vertexShader: ROCK_VS, fragmentShader: ROCK_FS });
    var N = PER_BELT * 2;
    var rx = new Float32Array(N), ry = new Float32Array(N), rz = new Float32Array(N), rr = new Float32Array(N);
    var sx = new Float32Array(N), sy = new Float32Array(N), sz = new Float32Array(N);            // per-axis squash
    var ax = new Float32Array(N), ay = new Float32Array(N), az = new Float32Array(N), rate = new Float32Array(N), ph = new Float32Array(N);
    var varOf = new Uint8Array(N), slotOf = new Int32Array(N), near = new Uint8Array(N), cnt = [0, 0, 0];
    var rng0 = mulberry(0x5EED2);
    for (var q = 0; q < N; q++) {                                  // variant + tumble axis never change; positions are laid by layout()
        varOf[q] = Math.floor(rng0() * NVAR); slotOf[q] = cnt[varOf[q]]++;
        var u = rng0() * 2 - 1, a = rng0() * 6.2831853, rho = Math.sqrt(1 - u * u);
        ax[q] = rho * Math.cos(a); ay[q] = u; az[q] = rho * Math.sin(a);
        rate[q] = (0.02 + 0.07 * rng0()) * (rng0() < 0.5 ? -1 : 1); ph[q] = rng0() * 6.2831853;
    }
    space.rocks = { x: rx, y: ry, z: rz, r: rr, center: null, count: N };      // belt-local rock centres (+ center = world origin of that frame), read-only
    var meshes = [], colAttr = [];
    for (v = 0; v < NVAR; v++) {
        var im = new THREE.InstancedMesh(variants[v], rockMat, Math.max(1, cnt[v]));
        im.frustumCulled = false; im.matrixAutoUpdate = true; im.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
        var ca = new THREE.InstancedBufferAttribute(new Float32Array(Math.max(1, cnt[v]) * 3), 3);
        variants[v].setAttribute('aCol', ca); colAttr.push(ca);
        group.add(im); meshes.push(im);
    }
    // impostor ring: every rock + the freighters
    var NP = N + FREIGHTERS, FAR = 1e15;
    var pPos = new Float32Array(NP * 3);
    for (q = 0; q < NP; q++) pPos[q * 3 + 1] = FAR;
    var pGeo = new THREE.BufferGeometry(); var pAttr = new THREE.BufferAttribute(pPos, 3); pAttr.setUsage(THREE.DynamicDrawUsage);
    pGeo.setAttribute('position', pAttr);
    var cv = document.createElement('canvas'); cv.width = cv.height = 32; var cx = cv.getContext('2d');
    var gr = cx.createRadialGradient(16, 16, 0, 16, 16, 16); gr.addColorStop(0, 'rgba(255,255,255,1)'); gr.addColorStop(0.6, 'rgba(255,255,255,0.7)'); gr.addColorStop(1, 'rgba(255,255,255,0)');
    cx.fillStyle = gr; cx.fillRect(0, 0, 32, 32);
    var tex = new THREE.CanvasTexture(cv);
    var pMat = new THREE.PointsMaterial({ size: 9 * L, sizeAttenuation: true, map: tex, color: 0x8a7e72, transparent: true, depthWrite: false, fog: false });
    var points = new THREE.Points(pGeo, pMat); points.frustumCulled = false; points.renderOrder = 1; group.add(points);

    // ─── layout: belts between consecutive root orbits ───────────────────────────────────────────────
    var cen = new THREE.Vector3(), tmpM = new THREE.Matrix4(), tmpQ = new THREE.Quaternion(), tmpP = new THREE.Vector3(), tmpS = new THREE.Vector3(), zeroM = new THREE.Matrix4().makeScale(0, 0, 0);
    var tmpAx = new THREE.Vector3();
    var grid = new Map(), layoutKey = [0, 0, 0, 0], layoutT = 1e9, layoutOk = false, spanR = 1;
    var radii = [];

    function readCenter() { var an = root.anchor; if (an && an.position) cen.set(an.position.x, an.position.y, an.position.z); else cen.set(root.wx || 0, root.ny || 0, root.wy || 0); }
    function readRadii() {
        radii.length = 0; var ks = root.kids || [];
        for (var i = 0; i < ks.length; i++) { var k = ks[i]; if (k.wx === undefined) continue; var r = Math.hypot(k.wx - (root.wx || 0), k.wy - (root.wy || 0)); if (r > 0 && isFinite(r)) radii.push(r); }
        radii.sort(function (p, q) { return p - q; });
    }
    var radiiS = [], ext = { y: 0, r0: 0, r1: 0 };
    function layout() {
        readRadii(); readCenter();
        // planets wobble on their orbits: low-pass the sorted radii so the belts only move when the layout really changes (pilot blend)
        if (radiiS.length !== radii.length) { radiiS = radii.slice(); } else for (var ri = 0; ri < radii.length; ri++) radiiS[ri] += (radii[ri] - radiiS[ri]) * 0.12;
        for (ri = 0; ri < radii.length; ri++) radii[ri] = radiiS[ri];
        var rmax = radii.length ? radii[radii.length - 1] : root.sysR || 1;
        spanR = rmax * 1.15;
        var gaps = [];
        for (var i = 0; i + 1 < radii.length; i++) gaps.push({ lo: radii[i], hi: radii[i + 1], g: radii[i + 1] - radii[i] });
        gaps.sort(function (p, q) { return q.g - p.g; });
        var pick = gaps.slice(0, 2);
        if (pick.length < 2) { var base = radii.length ? radii[0] : (root.sysR || 1) * 0.5; while (pick.length < 2) { var lo = base * (1 + 0.6 * pick.length), hi = lo * 1.5; pick.push({ lo: lo, hi: hi, g: hi - lo }); } }
        pick.sort(function (p, q) { return p.lo - q.lo; });
        var key = [pick[0].lo + pick[0].hi, pick[0].g, pick[1].lo + pick[1].hi, pick[1].g];
        var same = layoutOk; for (i = 0; i < 4; i++) if (Math.abs(key[i] / (layoutKey[i] || 1) - 1) > (i & 1 ? 0.35 : 0.06)) same = false;
        group.position.copy(cen); group.updateMatrixWorld(true);
        if (same) return;
        layoutKey = key; layoutOk = true;
        space.belts.length = 0;
        grid.clear();
        var lists = new Map();
        for (var b = 0; b < 2; b++) {
            var mid = (pick[b].lo + pick[b].hi) / 2, half = pick[b].g * 0.11, yMax = Math.max(60 * L, pick[b].g * 0.012);
            space.belts.push({ inner: mid - half, outer: mid + half, mid: mid, yMax: yMax, count: PER_BELT });
            var rng = mulberry(0xBE17 + b * 7919), clusters = [], nc = 32;
            for (i = 0; i < nc; i++) clusters.push({ a: rng() * 6.2831853, r: mid + (rng() * 2 - 1) * half * 0.8, y: (rng() * 2 - 1) * yMax * 0.5, s: (120 + rng() * 140) * L });
            for (i = 0; i < PER_BELT; i++) {
                var id = b * PER_BELT + i, px, py, pz;
                if (i < 800) {                                      // clustered fields
                    var c = clusters[i % nc], gx = rng() + rng() + rng() - 1.5, gy = rng() + rng() + rng() - 1.5, gz = rng() + rng() + rng() - 1.5;
                    px = Math.cos(c.a) * c.r + gx * c.s * 2; pz = Math.sin(c.a) * c.r + gz * c.s * 2; py = c.y + gy * c.s * 1.2;
                } else {                                            // scattered along the whole ring
                    var aa = rng() * 6.2831853, rrr = mid + (rng() * 2 - 1) * half;
                    px = Math.cos(aa) * rrr; pz = Math.sin(aa) * rrr; py = (rng() * 2 - 1) * yMax;
                }
                var sr = rng(), rad = (sr < 0.7 ? 1 + 4 * rng() : sr < 0.95 ? 5 + 10 * rng() : 15 + 25 * rng()) * L;
                rx[id] = px; ry[id] = py; rz[id] = pz; rr[id] = rad;
                var rp0 = Math.hypot(px, pz); if (id === 0) { ext.y = 0; ext.r0 = 1e30; ext.r1 = 0; }
                ext.y = Math.max(ext.y, Math.abs(py) + rad); ext.r0 = Math.min(ext.r0, rp0 - rad); ext.r1 = Math.max(ext.r1, rp0 + rad);
                sx[id] = 0.75 + 0.5 * rng(); sy[id] = 0.7 + 0.5 * rng(); sz[id] = 0.75 + 0.5 * rng();
                var tone = 0.22 + 0.22 * rng(), warm = rng();
                var ca2 = colAttr[varOf[id]].array, o = slotOf[id] * 3;
                ca2[o] = tone * (1 + 0.25 * warm); ca2[o + 1] = tone * (0.92 + 0.05 * warm); ca2[o + 2] = tone * (0.85 - 0.1 * warm);
                near[id] = 2;                                       // force a matrix write on the next lod pass
                // spatial hash: every cell the padded sphere overlaps
                var rp = rad * 1.3 + PAD * L, cell = CELL * L;
                for (var ix = Math.floor((px - rp) / cell); ix <= Math.floor((px + rp) / cell); ix++)
                    for (var iy = Math.floor((py - rp) / cell); iy <= Math.floor((py + rp) / cell); iy++)
                        for (var iz = Math.floor((pz - rp) / cell); iz <= Math.floor((pz + rp) / cell); iz++) {
                            var k = ckey(ix, iy, iz), l = lists.get(k); if (!l) lists.set(k, l = []); l.push(id);
                        }
            }
        }
        lists.forEach(function (l, k) { grid.set(k, Int32Array.from(l)); });
        for (v = 0; v < NVAR; v++) colAttr[v].needsUpdate = true;
        lodT = 1e9;
    }
    function ckey(ix, iy, iz) { return ((ix + 32768) * 65536 + (iz + 32768)) * 256 + (iy + 128); }

    // ─── per-frame ─────────────────────────────────────────────────────────────────────────────────────
    var lodT = 0, tumbleI = 0, clock = 0, dirty = [false, false, false], pDirty = false;
    var nearL = NEAR * L, nearOffL = NEAR_OFF * L;
    function writeRock(id) {
        var mi = meshes[varOf[id]], ang = clock * rate[id] + ph[id];
        tmpAx.set(ax[id], ay[id], az[id]); tmpQ.setFromAxisAngle(tmpAx, ang);
        tmpP.set(rx[id], ry[id], rz[id]); tmpS.set(rr[id] * sx[id], rr[id] * sy[id], rr[id] * sz[id]);
        tmpM.compose(tmpP, tmpQ, tmpS); mi.setMatrixAt(slotOf[id], tmpM); dirty[varOf[id]] = true;
    }
    function hideRock(id) { meshes[varOf[id]].setMatrixAt(slotOf[id], zeroM); dirty[varOf[id]] = true; }

    // ─── freighters ─────────────────────────────────────────────────────────────────────────────────────
    var darkened = false;
    function darken(h) {
        h.traverse(function (o) {
            if (o.geometry && o.geometry.attributes && o.geometry.attributes.color) {
                var c = o.geometry.attributes.color, a = c.array; for (var i = 0; i < a.length; i += 3) { var l = (a[i] + a[i + 1] + a[i + 2]) / 3; a[i] = l * 0.2 + a[i] * 0.12; a[i + 1] = l * 0.2 + a[i + 1] * 0.12; a[i + 2] = l * 0.24 + a[i + 2] * 0.12; }
                c.needsUpdate = true;
            }
            o.frustumCulled = false;
        });
    }
    var upV = new THREE.Vector3(0, 1, 0), zeroV = new THREE.Vector3();
    function spawn(f, first) {
        var rng = mulberry((f.seed = (f.seed * 1664525 + 1013904223) >>> 0));
        var a = rng() * 6.2831853, b = a + Math.PI + (rng() * 2 - 1) * 0.7, R = spanR;
        var yA = (rng() * 2 - 1) * R * 0.04, yB = yA + (rng() * 2 - 1) * R * 0.03;
        f.ax = cen.x + Math.cos(a) * R; f.ay = cen.y + yA; f.az = cen.z + Math.sin(a) * R;
        f.bx = cen.x + Math.cos(b) * R; f.by = cen.y + yB; f.bz = cen.z + Math.sin(b) * R;
        var dx = f.bx - f.ax, dy = f.by - f.ay, dz = f.bz - f.az, len = Math.hypot(dx, dy, dz);
        f.len = len; f.dir.set(dx / len, dy / len, dz / len);
        f.speed = Math.max(len / (480 + 240 * rng()), 20 * L);       // 8-12 min per crossing, never slower than 20 L/s
        f.t = first ? 0.15 + 0.7 * rng() : 0;
        tmpM.lookAt(zeroV, f.dir, upV); f.hull.quaternion.setFromRotationMatrix(tmpM);   // nose (-Z) along the line
    }
    for (var fi = 0; fi < FREIGHTERS; fi++) {
        var hull = buildHull(THREE, { kind: 'hauler' }); darken(hull); hull.scale.setScalar(30 * L); hull.visible = false; scene.add(hull);
        var fr = { pos: new THREE.Vector3(), dir: new THREE.Vector3(), t: 0, speed: 0, hull: hull, seed: 0xF4E1 + fi * 977, len: 1, ax: 0, ay: 0, az: 0, bx: 0, by: 0, bz: 0 };
        space.freighters.push(fr);
    }
    var freightInit = false;

    var shipLX = 0, shipLY = 0, shipLZ = 0, nearHull = -1;
    space.update = function (dt, shipPos) {
        clock += dt; layoutT += dt;
        if (layoutT > 2) { layoutT = 0; layout(); if (!freightInit) { freightInit = true; for (var i = 0; i < FREIGHTERS; i++) spawn(space.freighters[i], true); } }
        if (!layoutOk) return;
        group.position.copy(cen);                           // keep the star-centred frame glued even if the root moves
        shipLX = shipPos.x - cen.x; shipLY = shipPos.y - cen.y; shipLZ = shipPos.z - cen.z;
        var id, d2, a2 = nearL * nearL, b2 = nearOffL * nearOffL;
        for (id = 0; id < N; id++) {                       // LOD swap with hysteresis
            var dx = rx[id] - shipLX, dy = ry[id] - shipLY, dz = rz[id] - shipLZ; d2 = dx * dx + dy * dy + dz * dz;
            var st = near[id];
            if (st !== 1 && d2 < a2) { near[id] = 1; writeRock(id); pPos[id * 3 + 1] = FAR; pDirty = true; }
            else if (st !== 0 && (st === 2 || d2 > b2) && d2 >= a2) { near[id] = 0; hideRock(id); pPos[id * 3] = rx[id]; pPos[id * 3 + 1] = ry[id]; pPos[id * 3 + 2] = rz[id]; pDirty = true; }
        }
        // slow tumble: a rolling slice of the near rocks per frame (steps are invisible at 0.02-0.09 rad/s)
        var did = 0, scanned = 0;
        while (did < TUMBLE_PER_FRAME && scanned < 600) { tumbleI = (tumbleI + 1) % N; scanned++; if (near[tumbleI] === 1) { writeRock(tumbleI); did++; } }
        for (var v2 = 0; v2 < NVAR; v2++) if (dirty[v2]) { meshes[v2].instanceMatrix.needsUpdate = true; dirty[v2] = false; }
        // freighters
        var best = -1, bestD = HULL_NEAR * L * HULL_NEAR * L;
        for (var i2 = 0; i2 < FREIGHTERS; i2++) {
            var f = space.freighters[i2];
            f.t += f.speed * dt / f.len;
            if (f.t >= 1) spawn(f, false);
            f.pos.set(f.ax + (f.bx - f.ax) * f.t, f.ay + (f.by - f.ay) * f.t, f.az + (f.bz - f.az) * f.t);
            f.hull.position.copy(f.pos);
            var ex = f.pos.x - shipPos.x, ey = f.pos.y - shipPos.y, ez = f.pos.z - shipPos.z, de = ex * ex + ey * ey + ez * ez;
            if (de < bestD) { bestD = de; best = i2; }
        }
        for (i2 = 0; i2 < FREIGHTERS; i2++) {
            var fh = space.freighters[i2], showHull = i2 === best;
            if (fh.hull.visible !== showHull) fh.hull.visible = showHull;
            var po = (N + i2) * 3;
            if (showHull) pPos[po + 1] = FAR; else { pPos[po] = fh.pos.x - cen.x; pPos[po + 1] = fh.pos.y - cen.y; pPos[po + 2] = fh.pos.z - cen.z; }
            pDirty = true;
        }
        if (pDirty) { pAttr.needsUpdate = true; pDirty = false; }
    };

    // ─── queries ─────────────────────────────────────────────────────────────────────────────────────────
    space.coverTest = function (from, to) {
        if (!layoutOk) return false;
        var cell = CELL * L, x0 = from.x - cen.x, y0 = from.y - cen.y, z0 = from.z - cen.z, dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
        var len2 = dx * dx + dy * dy + dz * dz; if (len2 < 1e-12) return false;
        var yMax = ext.y + 10 * L;
        if ((y0 > yMax && y0 + dy > yMax) || (y0 < -yMax && y0 + dy < -yMax)) return false;
        // radial reject: planar distance from the star at both ends vs the outermost / innermost belt (+ closest approach)
        var bo = ext.r1 + 10 * L, bi = ext.r0 - 10 * L;
        var tpl = -(x0 * dx + z0 * dz) / Math.max(dx * dx + dz * dz, 1e-9); tpl = tpl < 0 ? 0 : tpl > 1 ? 1 : tpl;
        var cxp = x0 + dx * tpl, czp = z0 + dz * tpl, minR = Math.hypot(cxp, czp), r0 = Math.hypot(x0, z0), r1 = Math.hypot(x0 + dx, z0 + dz);
        if (minR > bo && r0 > bo && r1 > bo) return false;
        if (Math.max(r0, r1) < bi) return false;
        // 3D DDA over the cell grid
        var ix = Math.floor(x0 / cell), iy = Math.floor(y0 / cell), iz = Math.floor(z0 / cell);
        var sX = dx > 0 ? 1 : -1, sY = dy > 0 ? 1 : -1, sZ = dz > 0 ? 1 : -1;
        var tdX = dx !== 0 ? Math.abs(cell / dx) : 1e30, tdY = dy !== 0 ? Math.abs(cell / dy) : 1e30, tdZ = dz !== 0 ? Math.abs(cell / dz) : 1e30;
        var tmX = dx !== 0 ? ((dx > 0 ? (ix + 1) * cell - x0 : x0 - ix * cell) / Math.abs(dx)) : 1e30;
        var tmY = dy !== 0 ? ((dy > 0 ? (iy + 1) * cell - y0 : y0 - iy * cell) / Math.abs(dy)) : 1e30;
        var tmZ = dz !== 0 ? ((dz > 0 ? (iz + 1) * cell - z0 : z0 - iz * cell) / Math.abs(dz)) : 1e30;
        for (var step = 0; step < 400; step++) {
            var lst = grid.get(ckey(ix, iy, iz));
            if (lst) for (var j = 0; j < lst.length; j++) {
                var id = lst[j]; if (rr[id] < COVER_R * L) continue;
                var px = rx[id] - x0, py = ry[id] - y0, pz = rz[id] - z0, t = (px * dx + py * dy + pz * dz) / len2; t = t < 0 ? 0 : t > 1 ? 1 : t;
                var qx = px - dx * t, qy = py - dy * t, qz = pz - dz * t, rad = rr[id] * 0.8;
                if (qx * qx + qy * qy + qz * qz < rad * rad) return true;
            }
            var tn = tmX < tmY ? (tmX < tmZ ? tmX : tmZ) : (tmY < tmZ ? tmY : tmZ);
            if (tn > 1) break;
            if (tmX <= tmY && tmX <= tmZ) { ix += sX; tmX += tdX; } else if (tmY <= tmZ) { iy += sY; tmY += tdY; } else { iz += sZ; tmZ += tdZ; }
        }
        return false;
    };
    space.pushOut = function (pos, radius, out) {
        out.x = pos.x; out.y = pos.y; out.z = pos.z;
        if (!layoutOk) return false;
        var cell = CELL * L, x = pos.x - cen.x, y = pos.y - cen.y, z = pos.z - cen.z, pushed = false;
        var lst = grid.get(ckey(Math.floor(x / cell), Math.floor(y / cell), Math.floor(z / cell)));
        if (!lst) return false;
        for (var j = 0; j < lst.length; j++) {
            var id = lst[j], dx = x - rx[id], dy = y - ry[id], dz = z - rz[id], d2 = dx * dx + dy * dy + dz * dz, rad = rr[id] * 0.85 + radius;
            if (d2 < rad * rad) {
                var d = Math.sqrt(d2) || 1e-6, k = rad / d;
                if (d < 1e-4) { dx = 0; dy = 1; dz = 0; k = rad; d = 1; }
                x = rx[id] + dx * k; y = ry[id] + dy * k; z = rz[id] + dz * k; pushed = true;
            }
        }
        if (pushed) { out.x = x + cen.x; out.y = y + cen.y; out.z = z + cen.z; }
        return pushed;
    };

    space.dispose = function () {
        scene.remove(group); for (var v3 = 0; v3 < NVAR; v3++) { meshes[v3].dispose(); variants[v3].dispose(); }
        rockMat.dispose(); pGeo.dispose(); pMat.dispose(); tex.dispose();
        for (var i3 = 0; i3 < FREIGHTERS; i3++) { var h = space.freighters[i3].hull; scene.remove(h); h.traverse(function (o) { if (o.geometry) o.geometry.dispose(); }); }
        grid.clear();
    };
    space.rocks.center = cen;
    layout();
    for (var i4 = 0; i4 < FREIGHTERS; i4++) spawn(space.freighters[i4], true);
    freightInit = true;
    return space;
}

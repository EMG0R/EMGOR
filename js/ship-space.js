// ship-space.js — ambient deep space for ship mode (docs/ship-mode.md rev 21 E): two asteroid belts + slow freighters.
//
//   createSpace(engine, L) -> space
//   space.update(dt, shipPos)       per frame (world coords). Allocation-free after the first call.
//   space.belts                     [{ inner, outer, mid, yMax, count }]  radii in world units around the root (rebuilt when the pilot layout changes)
//   space.freighters                [{ pos (Vector3, world), dir (unit Vector3), t (0..1 along its line), speed, hull (Group) }]
//   space.coverTest(from, to)       true if the segment crosses a rock with radius >= 4 L (3D grid walk, exact segment/sphere)
//   space.pushOut(pos, radius, out) soft collision vs every rock: out = pos pushed to the rock surface (always written); returns true if pushed
//   space.convoys / hail(id) / attack(id) / hostiles() / crates      rev 26: freighter convoys (1-3 haulers + 2 fighter escorts), trade offers, piracy, reputation deltas
//   space.derelicts [{ pos, quat, deck{walls,floorAt}, crates, guardian{pos,tier}, mouth{trigger}, dockPath(), launchPath(), inside }]   space.station = station (optional, convoy port)
//   makeMarket(seed, opts) exported (station TRADE terminal shares it)
//   rev 27 mining: space.minable() [{id,pos,r,kind,hp}] (pooled objects), hitRock(id,dmg) -> {broken,chunks:[{id,pos,vel,item,n}]}, chunks[], takeChunk(id),
//   mineState() / markMined(state) (respawn 10 real min), scanTargets() [{id,kind,pos,ref}] (pooled); 15 % of rocks are veined; belt dust Points
//   space.dispose()
//
//   ~1200 rocks per belt: 3 icosahedron-noise variants shared by both belts (3 InstancedMesh draws), rocks beyond 400 L swap to ONE Points
//   impostor ring (+1 draw), 2 freighters = the 'hauler' kit x30 in a dark livery (the nearest one within 3000 L is a real hull, the rest are
//   sprites). Floating-origin safe: nothing reads matrixWorld, rocks are stored relative to the belt group (= the root), freighters are plain
//   scene children whose positions are world coords (the engine translates the scene).
import { buildHull } from './ship-hull.js';
import { resourceItem, RESOURCE_BASE } from './ship-items.js';
import { PARTS, stackKeyOf } from './ship-craft.js';

var PER_BELT = 1200, NVAR = 3, NEAR = 400, NEAR_OFF = 440, CELL = 250, PAD = 8, COVER_R = 4, HULL_NEAR = 3000, FREIGHTERS = 2;
var TUMBLE_PER_FRAME = 90;

function mulberry(a) { return function () { a |= 0; a = (a + 0x6D2B79F5) | 0; var t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
function hashStr(s) { var h = 2166136261; for (var i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
// makeMarket(seed, { buyMul, sellMul, nBuy, nSell }) -> { buys:[stackKey], buyPrices:{key:gorCoin}, sells:[{id,item,n,price}] }  deterministic per seed string
export function makeMarket(seed, o) {
    o = o || {}; var r = mulberry(hashStr(String(seed))), pool = [], kinds = Object.keys(RESOURCE_BASE), i;
    for (i = 0; i < kinds.length; i++) pool.push(resourceItem(kinds[i], hashStr(seed + kinds[i]) % 8));
    for (i = 0; i < PARTS.length; i++) pool.push(PARTS[i]);
    for (i = pool.length - 1; i > 0; i--) { var j = Math.floor(r() * (i + 1)), t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
    var nB = o.nBuy == null ? 4 : o.nBuy, nS = o.nSell == null ? 5 : o.nSell, m = { buys: [], buyPrices: {}, sells: [] };
    for (i = 0; i < nB && i < pool.length; i++) { var k = stackKeyOf(pool[i]); m.buys.push(k); m.buyPrices[k] = Math.max(1, Math.round((pool[i].price || 20) * (o.buyMul || 1) * (0.8 + 0.3 * r()))); }
    for (i = nB; i < nB + nS && i < pool.length; i++) m.sells.push({ id: stackKeyOf(pool[i]), item: pool[i], n: 1 + Math.floor(r() * 6), price: Math.max(1, Math.round((pool[i].price || 20) * (o.sellMul || 1.1) * (0.85 + 0.3 * r()))) });
    return m;
}
function hash3(x, y, z, s) { var h = Math.imul(Math.round(x * 997) ^ s, 374761393) ^ Math.imul(Math.round(y * 997) + 31, 668265263) ^ Math.imul(Math.round(z * 997) + 77, 2147483647); h = Math.imul(h ^ (h >>> 13), 1274126177); return ((h ^ (h >>> 16)) >>> 0) / 4294967296; }

var ROCK_VS = [
    'attribute vec3 aCol; attribute vec3 aVein;',
    'varying vec3 vLocal; varying vec3 vCtr; varying vec3 vCol; varying vec3 vVein;',
    'void main() {',
    '  vec4 lp = instanceMatrix * vec4(position, 1.0);',
    '  vLocal = lp.xyz; vCtr = instanceMatrix[3].xyz; vCol = aCol; vVein = aVein;',
    '  gl_Position = projectionMatrix * modelViewMatrix * lp;',
    '}'
].join('\n');
var ROCK_FS = [
    'varying vec3 vLocal; varying vec3 vCtr; varying vec3 vCol; varying vec3 vVein;',
    'void main() {',
    '  vec3 n = normalize(cross(dFdx(vLocal), dFdy(vLocal)));',
    '  if (dot(n, vLocal - vCtr) < 0.0) n = -n;',
    '  vec3 sun = normalize(-vCtr);',                           // the star sits at the belt group origin
    '  float d = max(dot(n, sun), 0.0);',
    '  vec3 col = vCol * (0.10 + 0.95 * d);',
    '  if (dot(vVein, vVein) > 0.0) {',                         // ore / ice / crystal rocks: glowing veins that tumble with the rock
    '    vec3 q = normalize(vLocal - vCtr) * 3.0;',
    '    float w1 = 1.0 - smoothstep(0.0, 0.16, abs(sin(q.x * 4.0 + 2.0 * sin(q.y * 3.0 + q.z * 2.0))));',
    '    float w2 = 1.0 - smoothstep(0.0, 0.12, abs(sin(q.y * 5.0 + 2.0 * sin(q.z * 4.0 - q.x * 2.0))));',
    '    col = mix(col, vVein * 1.6, clamp(max(w1, w2 * 0.8), 0.0, 1.0));',
    '  }',
    '  gl_FragColor = vec4(col, 1.0);',
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
    // mining: 15 % of rocks (seeded) carry veins (per-instance aVein colour; 0 = plain rock)
    var oreKind = new Uint8Array(N), hpArr = new Float32Array(N), rr0 = new Float32Array(N), gone = new Uint8Array(N), goneAt = new Float64Array(N), goneList = [];
    var VEIN_COL = [null, [1.0, 0.62, 0.12], [0.55, 0.9, 1.0], [0.3, 0.95, 1.0]], KIND_NAME = ['', 'ore', 'ice', 'crystal'];
    var veinAttr = [];
    for (v = 0; v < NVAR; v++) { var va = new THREE.InstancedBufferAttribute(new Float32Array(Math.max(1, cnt[v]) * 3), 3); variants[v].setAttribute('aVein', va); veinAttr.push(va); }
    var rngO = mulberry(0x0BE5);
    for (q = 0; q < N; q++) {
        var ro = rngO(), ko = rngO();
        if (ro < 0.15) {
            var kk = ko < 0.6 ? 1 : ko < 0.8 ? 2 : 3; oreKind[q] = kk;
            var vc = VEIN_COL[kk], vo = slotOf[q] * 3, vr = veinAttr[varOf[q]].array; vr[vo] = vc[0]; vr[vo + 1] = vc[1]; vr[vo + 2] = vc[2];
        }
    }
    for (v = 0; v < NVAR; v++) veinAttr[v].needsUpdate = true;
    // impostor ring: every rock + the freighters
    var NP = N + FREIGHTERS + 16, FAR = 1e15;
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
                rx[id] = px; ry[id] = py; rz[id] = pz; rr[id] = rad; rr0[id] = rad; hpArr[id] = 20 + rad / L * 7;
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
        for (var gi = 0; gi < goneList.length; gi++) { var gid = goneList[gi]; rr[gid] = 0; near[gid] = 0; hideRock(gid); pPos[gid * 3 + 1] = FAR; }
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
            if (gone[id]) continue;
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

    //TIER3_HERE
    space.dispose = function () {
        scene.remove(group); for (var v3 = 0; v3 < NVAR; v3++) { meshes[v3].dispose(); variants[v3].dispose(); }
        rockMat.dispose(); pGeo.dispose(); pMat.dispose(); tex.dispose();
        for (var i3 = 0; i3 < FREIGHTERS; i3++) { var h = space.freighters[i3].hull; scene.remove(h); h.traverse(function (o) { if (o.geometry) o.geometry.dispose(); }); }
        grid.clear();
    };

    // ═══ Tier 3 (rev 26): convoys + derelicts ══════════════════════════════════════════════════════════════
    var CONV = 2, DER = 2, FLOORY = -0.5, CEILY = 0.6, CELLS = 1.5, COLS = 4, ROWS = 14, MOUTHZ = 15;
    space.convoys = []; space.crates = []; space.derelicts = []; space.station = null; space.reputation = { corp: 0, dealers: 0 };
    var cvTmp = { A: new THREE.Vector3(), B: new THREE.Vector3(), D: new THREE.Vector3(), R: new THREE.Vector3(), V: new THREE.Vector3(), Q: new THREE.Quaternion(), M: new THREE.Matrix4() };
    var hostileList = [], poolInit = false, pf = [], pe = [], focusKind = '', focusIdx = -1;
    var IMP0 = N + FREIGHTERS;

    function portList() { var a = [null], ks = root.kids || []; for (var i = 0; i < ks.length; i++) if (ks[i].wx !== undefined) a.push(ks[i]); return a; }
    function portPos(k, out) {
        if (!k) { var s = space.station; if (s && s.pos) return out.copy(s.pos); return out.set(cen.x + spanR * 0.35, cen.y, cen.z); }
        var an = k.anchor; if (an && an.position) return out.set(an.position.x, an.position.y, an.position.z);
        return out.set(k.wx || 0, cen.y, k.wy || 0);
    }
    function makeCargo(rng, n) {
        var kinds = Object.keys(RESOURCE_BASE), out = [];
        for (var i = 0; i < n; i++) {
            var it = rng() < 0.6 ? resourceItem(kinds[Math.floor(rng() * kinds.length)], Math.floor(rng() * 8)) : PARTS[Math.floor(rng() * PARTS.length)];
            var key = stackKeyOf(it), ex = null; for (var j = 0; j < out.length; j++) if (out[j].id === key) ex = out[j];
            if (ex) ex.n += 1 + Math.floor(rng() * 3); else out.push({ id: key, item: it, n: 1 + Math.floor(rng() * 6) });
        }
        return out;
    }
    function resetConvoy(c, first) {
        var rng = mulberry((c.seed = (c.seed * 1664525 + 1013904223) >>> 0)), ports = portList(), np = ports.length;
        c.faction = rng() < 0.5 ? 'corp' : 'dealers'; c.state = 'cruising'; c.cargo = makeCargo(rng, 3 + Math.floor(rng() * 3));
        var nF = 1 + Math.floor(rng() * 3); c.ships.length = 0; c.fired = false;
        for (var i = 0; i < nF + 2; i++) {
            var esc = i >= nF;
            c.ships.push({ pos: new THREE.Vector3(1e12, 1e12, 1e12), quat: new THREE.Quaternion(), kind: esc ? 'fighter' : 'hauler', hp: esc ? 60 : 400, alive: true, ang: rng() * 6.283, rate: (0.35 + rng() * 0.3) * (rng() < 0.5 ? -1 : 1), convoy: c, hit: null });
            if (esc) { (function (s) { s.hit = function (dmg) { return hitEscort(s, dmg); }; })(c.ships[i]); }
        }
        var a = Math.floor(rng() * np), b = (a + 1 + Math.floor(rng() * Math.max(1, np - 1))) % np;
        c.route = { a: np > 1 ? a : -1, b: np > 1 ? b : -1, t: first ? 0.1 + 0.7 * rng() : 0, dur: 300 + 180 * rng(), custom: false, A: c.route ? c.route.A : new THREE.Vector3(), B: c.route ? c.route.B : new THREE.Vector3(), ports: ports };
        if (np < 2) { var ang = rng() * 6.283; c.route.custom = true; c.route.A.set(cen.x + Math.cos(ang) * spanR, cen.y, cen.z + Math.sin(ang) * spanR); c.route.B.set(cen.x - Math.cos(ang) * spanR, cen.y, cen.z - Math.sin(ang) * spanR); }
    }
    function hitEscort(s, dmg) {
        if (!s.alive) return 0;
        s.hp -= dmg || 1;
        if (s.hp <= 0) {
            s.alive = false; s.hp = 0;
            var c = s.convoy, left = 0; for (var i = 0; i < c.ships.length; i++) if (c.ships[i].kind === 'fighter' && c.ships[i].alive) left++;
            if (!left && c.state === 'attacked') dropCargo(c);
        }
        return s.hp;
    }
    function dropCargo(c) {
        var f = c.ships[0], per = Math.max(1, Math.ceil(c.cargo.length / 3)), n = 0;
        for (var i = 0; i < c.cargo.length; i += per) {
            space.crates.push({ id: c.id + '-c' + (n++), convoyId: c.id, pos: new THREE.Vector3(f.pos.x + (n - 2) * 6 * L, f.pos.y + (n % 2) * 3 * L, f.pos.z + (n - 1) * 5 * L), stacks: c.cargo.slice(i, i + per), taken: false });
        }
        while (space.crates.length > 24) space.crates.shift();
        c.cargo = []; c.state = 'fled'; cratesDirty = true;
    }
    var cratesDirty = true, crateT = 0;
    var cPos = new Float32Array(24 * 3), cGeo = new THREE.BufferGeometry(), cAttr = new THREE.BufferAttribute(cPos, 3); cAttr.setUsage(THREE.DynamicDrawUsage); cGeo.setAttribute('position', cAttr);
    for (var cq = 0; cq < 24; cq++) cPos[cq * 3 + 1] = FAR;
    var cMat = new THREE.PointsMaterial({ size: 3.5 * L, sizeAttenuation: true, map: tex, color: 0xffb030, transparent: true, depthWrite: false, fog: false });
    var cPoints = new THREE.Points(cGeo, cMat); cPoints.frustumCulled = false; cPoints.renderOrder = 2; scene.add(cPoints);
    function syncCrates() {
        for (var i = 0; i < 24; i++) { var c = space.crates[i]; if (c && !c.taken) { cPos[i * 3] = c.pos.x; cPos[i * 3 + 1] = c.pos.y; cPos[i * 3 + 2] = c.pos.z; } else cPos[i * 3 + 1] = FAR; }
        cAttr.needsUpdate = true; cratesDirty = false;
    }
    for (var ci = 0; ci < CONV; ci++) { var cvv = { id: 'cv' + ci, faction: 'corp', ships: [], cargo: [], route: null, state: 'cruising', seed: 0xC0117 + ci * 4421 }; space.convoys.push(cvv); resetConvoy(cvv, true); }

    function find(id) { for (var i = 0; i < space.convoys.length; i++) if (space.convoys[i].id === id) return space.convoys[i]; return null; }
    space.hail = function (id) {
        var c = find(id); if (!c) return null;
        if (c.state === 'cruising') c.state = 'hailed';
        var mul = c.faction === 'dealers' ? 0.8 : 1.1, sells = [], i;
        for (i = 0; i < c.cargo.length; i++) sells.push({ id: c.cargo[i].id, item: c.cargo[i].item, n: c.cargo[i].n, price: Math.max(1, Math.round((c.cargo[i].item.price || 20) * mul)) });
        var m = makeMarket(c.id + ':' + c.seed, { buyMul: c.faction === 'dealers' ? 1.25 : 1.0, nBuy: 4, nSell: 0 });
        return { convoyId: c.id, faction: c.faction, buys: m.buys, buyPrices: m.buyPrices, sells: sells };
    };
    space.attack = function (id) {
        var c = find(id), d = { corp: 0, dealers: 0 }; if (!c || c.state === 'attacked' || c.state === 'fled') return d;
        c.state = 'attacked';
        if (c.faction === 'corp') { d.corp = -1; d.dealers = 1; } else { d.dealers = -1; d.corp = 1; }
        space.reputation.corp += d.corp; space.reputation.dealers += d.dealers; return d;
    };
    space.hostiles = function () {
        hostileList.length = 0;
        for (var i = 0; i < space.convoys.length; i++) { var c = space.convoys[i]; if (c.state !== 'attacked') continue; for (var j = 0; j < c.ships.length; j++) if (c.ships[j].kind === 'fighter' && c.ships[j].alive) hostileList.push(c.ships[j]); }
        return hostileList;
    };
    // test helper: park convoy / derelict near a world point (does not touch ship.js state)
    space.debugNear = function (what, idx, pos, distL, dir) {
        var d = dir ? dir.clone().normalize() : new THREE.Vector3(0, 0, -1);
        if (what === 'convoy') { var c = space.convoys[idx], r = c.route; r.custom = true; r.t = 0.5; r.dur = 1e6; var ctr = cvTmp.V.copy(pos).addScaledVector(new THREE.Vector3(1, 0, 0).cross(d).length() > 0.1 ? new THREE.Vector3(1, 0, 0) : new THREE.Vector3(0, 1, 0), 0).add(new THREE.Vector3(0, 0, -(distL || 150) * L)); r.A.copy(ctr).addScaledVector(d, -3000 * L); r.B.copy(ctr).addScaledVector(d, 3000 * L); r.t = 0.5; }
        else { var dd = space.derelicts[idx]; dd.override = new THREE.Vector3(pos.x, pos.y, pos.z - (distL || 150) * L); }
    };

    // ─── derelicts ───────────────────────────────────────────────────────────────────────────────────
    var DVS = [
        'attribute vec3 color; attribute float aPh; attribute float aEm; uniform float uTime;',
        'varying vec3 vC; varying float vL; varying vec3 vP;',
        'void main() {',
        '  vC = color; vP = position;',
        '  float f = step(0.28, fract(sin(uTime * (2.0 + aPh * 5.0) + aPh * 91.0) * 437.5));',
        '  float mix1 = 0.7 + 0.3 * sin(uTime * 0.7 + position.z * 0.8 + aPh * 6.0);',
        '  vL = aEm > 0.5 ? (0.15 + 1.1 * f) : (0.38 + 0.4 * mix1 * (0.6 + 0.4 * f));',
        '  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);',
        '}'
    ].join('\n');
    var DFS = ['varying vec3 vC; varying float vL; varying vec3 vP;', 'void main() {', '  gl_FragColor = vec4(vC * vL, 1.0);', '}'].join('\n');
    var dUni = { uTime: { value: 0 } };
    var dMat = new THREE.ShaderMaterial({ vertexShader: DVS, fragmentShader: DFS, uniforms: dUni, side: THREE.DoubleSide });
    function mergeBoxes(list) {
        var tot = 0, i; var gs = [];
        for (i = 0; i < list.length; i++) {
            var b = list[i], g = new THREE.BoxGeometry(b.sx, b.sy, b.sz).toNonIndexed();
            if (b.rz) g.rotateZ(b.rz); if (b.rx) g.rotateX(b.rx); g.translate(b.x, b.y, b.z); gs.push(g); tot += g.attributes.position.count;
        }
        var P = new Float32Array(tot * 3), C = new Float32Array(tot * 3), PH = new Float32Array(tot), EM = new Float32Array(tot), o = 0, col = new THREE.Color();
        for (i = 0; i < gs.length; i++) {
            var pa = gs[i].attributes.position.array, n = gs[i].attributes.position.count; col.setHex(list[i].c);
            P.set(pa, o * 3); for (var k = 0; k < n; k++) { C[(o + k) * 3] = col.r; C[(o + k) * 3 + 1] = col.g; C[(o + k) * 3 + 2] = col.b; PH[o + k] = list[i].ph; EM[o + k] = list[i].em ? 1 : 0; }
            o += n; gs[i].dispose();
        }
        var g2 = new THREE.BufferGeometry(); g2.setAttribute('position', new THREE.BufferAttribute(P, 3)); g2.setAttribute('color', new THREE.BufferAttribute(C, 3));
        g2.setAttribute('aPh', new THREE.BufferAttribute(PH, 1)); g2.setAttribute('aEm', new THREE.BufferAttribute(EM, 1));
        return g2;
    }
    var derHullBase = null;
    function buildDerelict(di) {
        var rng = mulberry(0xD3AD + di * 7717), T3 = THREE, boxes = [], ext = [], walls = [];
        var cx0 = -COLS * CELLS / 2, cz0 = -ROWS * CELLS / 2 + 0.0, hx = COLS * CELLS / 2, hz = ROWS * CELLS / 2;
        // spanning-tree maze: open[r][c] bits E(1) S(2)
        var open = [], r, c, i, seen = [];
        for (r = 0; r < ROWS; r++) { open.push(new Array(COLS).fill(0)); seen.push(new Array(COLS).fill(false)); }
        var stack = [[ROWS - 1, 1]]; seen[ROWS - 1][1] = true;
        while (stack.length) {
            var cur = stack[stack.length - 1], nb = [];
            [[0, 1], [0, -1], [1, 0], [-1, 0]].forEach(function (dd) { var rr2 = cur[0] + dd[0], cc = cur[1] + dd[1]; if (rr2 >= 0 && rr2 < ROWS && cc >= 0 && cc < COLS && !seen[rr2][cc]) nb.push([rr2, cc, dd]); });
            if (!nb.length) { stack.pop(); continue; }
            var pk = nb[Math.floor(rng() * nb.length)]; seen[pk[0]][pk[1]] = true;
            if (pk[2][1] === 1) open[cur[0]][cur[1]] |= 1; else if (pk[2][1] === -1) open[pk[0]][pk[1]] |= 1; else if (pk[2][0] === 1) open[cur[0]][cur[1]] |= 2; else open[pk[0]][pk[1]] |= 2;
            stack.push([pk[0], pk[1]]);
        }
        for (c = 0; c < COLS - 1; c++) open[0][c] |= 1;            // loot room = the whole far row
        for (i = 0; i < 5; i++) { r = 1 + Math.floor(rng() * (ROWS - 2)); c = Math.floor(rng() * (COLS - 1)); open[r][c] |= 1; }   // a few loops
        var rust = [0x4a4458, 0x54484a, 0x3c4254, 0x5a4a40], T = 0.07, WH = CEILY - FLOORY;
        function wall(name, x0, x1, z0, z1, ph) {
            walls.push({ name: name, min: new T3.Vector3(x0, FLOORY, z0), max: new T3.Vector3(x1, CEILY, z1), field: false });
            boxes.push({ x: (x0 + x1) / 2, y: (FLOORY + CEILY) / 2, z: (z0 + z1) / 2, sx: x1 - x0, sy: WH, sz: z1 - z0, c: rust[Math.floor(rng() * 4)], ph: ph, em: false });
        }
        for (c = 0; c < COLS; c++) { wall('mz-n' + c, cx0 + c * CELLS, cx0 + (c + 1) * CELLS, cz0 - T, cz0 + T, rng()); }
        for (r = 0; r < ROWS; r++) {
            wall('mz-w' + r, cx0 - T, cx0 + T, cz0 + r * CELLS, cz0 + (r + 1) * CELLS, rng());
            wall('mz-e' + r, cx0 + hx * 2 - T, cx0 + hx * 2 + T, cz0 + r * CELLS, cz0 + (r + 1) * CELLS, rng());
            for (c = 0; c < COLS; c++) {
                if (c < COLS - 1 && !(open[r][c] & 1)) wall('mz-v' + r + '-' + c, cx0 + (c + 1) * CELLS - T, cx0 + (c + 1) * CELLS + T, cz0 + r * CELLS, cz0 + (r + 1) * CELLS, rng());
                if (r < ROWS - 1 && !(open[r][c] & 2)) wall('mz-h' + r + '-' + c, cx0 + c * CELLS, cx0 + (c + 1) * CELLS, cz0 + (r + 1) * CELLS - T, cz0 + (r + 1) * CELLS + T, rng());
            }
        }
        // entry lane from the maze's near end (cell col 1) out to the mouth
        var lx0 = cx0 + CELLS, lx1 = cx0 + 2 * CELLS;
        wall('lane-w', lx0 - T, lx0 + T, cz0 + ROWS * CELLS, MOUTHZ, rng()); wall('lane-e', lx1 - T, lx1 + T, cz0 + ROWS * CELLS, MOUTHZ, rng());
        wall('maze-s0', cx0, lx0, cz0 + ROWS * CELLS - T, cz0 + ROWS * CELLS + T, rng()); wall('maze-s1', lx1, cx0 + hx * 2, cz0 + ROWS * CELLS - T, cz0 + ROWS * CELLS + T, rng());
        walls.push({ name: 'mouthField', min: new T3.Vector3(lx0, FLOORY, MOUTHZ - 0.05), max: new T3.Vector3(lx1, CEILY, MOUTHZ + 0.2), field: true });
        // floor, ceiling, light strips
        boxes.push({ x: 0, y: FLOORY - 0.03, z: (cz0 + MOUTHZ) / 2, sx: hx * 2 + 0.3, sy: 0.06, sz: MOUTHZ - cz0 + 0.3, c: 0x2a2630, ph: 0.1, em: false });
        boxes.push({ x: 0, y: CEILY + 0.03, z: (cz0 + MOUTHZ) / 2, sx: hx * 2 + 0.3, sy: 0.06, sz: MOUTHZ - cz0 + 0.3, c: 0x1e1a26, ph: 0.2, em: false });
        for (i = 0; i < 9; i++) { r = Math.floor(rng() * ROWS); c = Math.floor(rng() * COLS); boxes.push({ x: cx0 + (c + 0.5) * CELLS, y: CEILY - 0.02, z: cz0 + (r + 0.5) * CELLS, sx: 0.8, sy: 0.03, sz: 0.12, c: rng() < 0.3 ? 0xff6a4a : 0xbfe8ff, ph: rng(), em: true }); }
        // loot room: 3 crates + a glow strip
        var dd = { index: di, id: 'dr' + di, pos: new T3.Vector3(), quat: new T3.Quaternion(), hullScale: 30, crates: [], inside: false, override: null, mouth: { pos: new T3.Vector3(), dir: new T3.Vector3(0, 0, 1), halfW: CELLS / 2 * L, halfH: 0.55 * L, trigger: null }, deck: { walls: walls, bounds: { min: new T3.Vector3(cx0, FLOORY, cz0), max: new T3.Vector3(cx0 + hx * 2, CEILY, MOUTHZ) }, floorAt: null } };
        var kinds = Object.keys(RESOURCE_BASE), rareP = PARTS[Math.floor(rng() * PARTS.length)];
        var rare = Object.assign({}, rareP, { id: 'rare-' + rareP.id + '-d' + di, name: 'Salvaged ' + rareP.name + ' Mk III', price: rareP.price * 6, tier: 3, rare: true, stackKey: 'rare-' + rareP.id + '-d' + di });
        var lootStacks = [
            [{ id: 'x', item: resourceItem(kinds[Math.floor(rng() * 5)], di * 3 + 1), n: 3 + Math.floor(rng() * 5) }, { id: 'y', item: PARTS[Math.floor(rng() * PARTS.length)], n: 1 + Math.floor(rng() * 3) }],
            [{ id: 'x', item: resourceItem(kinds[Math.floor(rng() * 5)], di * 3 + 2), n: 4 + Math.floor(rng() * 6) }],
            [{ id: 'x', item: rare, n: 1 }, { id: 'y', item: PARTS[Math.floor(rng() * PARTS.length)], n: 2 }]
        ];
        lootStacks.forEach(function (ss) { ss.forEach(function (s) { s.id = stackKeyOf(s.item); }); });
        for (i = 0; i < 3; i++) {
            var lx = cx0 + (i === 2 ? 3.5 : i * 1.0 + 0.5 + 0.0) * CELLS, lz = cz0 + 0.55 * CELLS;
            boxes.push({ x: lx, y: FLOORY + 0.1, z: lz, sx: 0.32, sy: 0.2, sz: 0.24, c: i === 2 ? 0xff5ce1 : 0xffb030, ph: 0.5, em: false });
            boxes.push({ x: lx, y: FLOORY + 0.21, z: lz, sx: 0.34, sy: 0.02, sz: 0.26, c: i === 2 ? 0xffd0f6 : 0xffe9a8, ph: i * 0.3, em: true });
            dd.crates.push({ id: dd.id + '-c' + i, localPos: new T3.Vector3(lx, FLOORY, lz), pos: new T3.Vector3(), stacks: lootStacks[i], taken: false, rare: i === 2 });
        }
        boxes.push({ x: cx0 + hx, y: CEILY - 0.02, z: cz0 + 0.5 * CELLS, sx: hx * 1.6, sy: 0.03, sz: 0.2, c: 0xff6a4a, ph: 0.77, em: true });
        dd.guardian = { pos: new T3.Vector3(), localPos: new T3.Vector3(cx0 + 2.5 * CELLS, FLOORY, cz0 + 0.5 * CELLS), tier: 1 + Math.floor(rng() * 3) };
        dd.interiorMesh = new T3.Mesh(mergeBoxes(boxes), dMat); dd.interiorMesh.frustumCulled = false;
        // exterior: broken fin shards + flickering port lights
        for (i = 0; i < 6; i++) { var sd = i < 3 ? -1 : 1; ext.push({ x: sd * (3.2 + rng() * 2.5), y: 1.5 + rng() * 2.5, z: 7 + rng() * 6, sx: 1.4 + rng() * 2, sy: 0.12, sz: 1 + rng() * 2.5, rz: sd * (0.4 + rng() * 0.9), rx: (rng() - 0.5) * 0.7, c: 0x2c2832, ph: rng(), em: false }); }
        for (i = 0; i < 14; i++) { var sd2 = i & 1 ? 1 : -1; ext.push({ x: sd2 * 2.6, y: 0.4 + (i % 3) * 0.2, z: -9 + i * 1.4, sx: 0.05, sy: 0.18, sz: 0.4, c: rng() < 0.3 ? 0xff6a4a : 0xbfe8ff, ph: rng(), em: true }); }
        dd.extMesh = new T3.Mesh(mergeBoxes(ext), dMat); dd.extMesh.frustumCulled = false;
        var g = new T3.Group(); g.scale.setScalar(L); g.visible = false; g.matrixAutoUpdate = true;
        if (!derHullBase) { derHullBase = buildHull(THREE, { kind: 'hauler' }); darken(derHullBase); darken(derHullBase); }
        dd.hull = derHullBase.clone(); dd.hull.scale.setScalar(30); g.add(dd.hull); g.add(dd.extMesh); g.add(dd.interiorMesh); dd.interiorMesh.visible = false;
        dd.group = g; scene.add(g);
        dd.a = rng() * 6.283; dd.belt = di % 2; dd.y = (rng() * 2 - 1) * 20 * L; dd.rate = (1.5 + rng()) * L * (rng() < 0.5 ? -1 : 1);
        cvTmp.M.makeRotationY(rng() * 6.283); dd.baseQ = new T3.Quaternion().setFromRotationMatrix(cvTmp.M);
        dd.toLocal = function (wp, out) { return out.copy(wp).sub(dd.pos).applyQuaternion(cvTmp.Q.copy(dd.quat).invert()).multiplyScalar(1 / L); };
        dd.toWorld = function (lp, out) { return out.copy(lp).multiplyScalar(L).applyQuaternion(dd.quat).add(dd.pos); };
        var _l = new T3.Vector3();
        dd.deck.floorAt = function (wp, out) {
            var o = out || { point: new T3.Vector3(), normal: new T3.Vector3(), height: 0, inside: false }, l = dd.toLocal(wp, _l), b = dd.deck.bounds;
            o.inside = l.x >= b.min.x && l.x <= b.max.x && l.z >= b.min.z && l.z <= b.max.z; o.height = (l.y - FLOORY) * L;
            dd.toWorld(_l.set(l.x, FLOORY, l.z), o.point); o.normal.set(0, 1, 0).applyQuaternion(dd.quat); return o;
        };
        dd.mouth.trigger = function (sp) { if (!sp) return false; var l = dd.toLocal(sp, _l); return Math.abs(l.x) < 2.2 && Math.abs(l.y) < 2.2 && l.z > MOUTHZ - 4 && l.z < MOUTHZ + 16; };
        var path = function (rev) {
            var lx = (lx0 + lx1) / 2, pts = [[lx, 0, MOUTHZ + 60], [lx, 0, MOUTHZ + 30], [lx, 0, MOUTHZ + 10], [lx, FLOORY + 0.3, MOUTHZ - 1], [lx, FLOORY + 0.2, MOUTHZ - 3]];
            var ps = rev ? pts.slice().reverse() : pts; if (rev) ps.push([lx, 0, MOUTHZ + 120]);
            dd.group.updateMatrixWorld(true);
            return new THREE.CatmullRomCurve3(ps.map(function (q) { return new T3.Vector3(q[0], q[1], q[2]); }), false, 'centripetal').getSpacedPoints(48).map(function (q) { return dd.toWorld(q, q); });
        };
        dd.dockPath = function () { return path(false); }; dd.launchPath = function () { return path(true); };
        dd.setInside = function (b) { if (dd.inside === b) return; dd.inside = b; dd.interiorMesh.visible = b; dd.hull.visible = !b; };
        return dd;
    }
    for (var di = 0; di < DER; di++) space.derelicts.push(buildDerelict(di));

    function updateTier3(dt, shipPos) {
        dUni.uTime.value = clock;
        var i, j, up = upV;
        // convoys
        for (i = 0; i < CONV; i++) {
            var c = space.convoys[i], r = c.route;
            if (!r.custom) { var pl = r.ports; portPos(pl[r.a] || null, cvTmp.A); portPos(pl[r.b] || null, cvTmp.B); } else { cvTmp.A.copy(r.A); cvTmp.B.copy(r.B); }
            cvTmp.D.subVectors(cvTmp.B, cvTmp.A); var len = cvTmp.D.length() || 1; cvTmp.D.multiplyScalar(1 / len);
            var sp = len / r.dur * (c.state === 'hailed' ? 0.25 : c.state === 'attacked' ? 0.5 : c.state === 'fled' ? 4 : 1);
            r.t += sp * dt / len; if (r.t >= 1) { resetConvoy(c, false); continue; }
            var u = 0.08 + 0.84 * r.t; cvTmp.R.crossVectors(cvTmp.D, up); if (cvTmp.R.lengthSq() < 1e-6) cvTmp.R.set(1, 0, 0); cvTmp.R.normalize();
            cvTmp.M.lookAt(zeroV, cvTmp.D, up); cvTmp.Q.setFromRotationMatrix(cvTmp.M);
            var fi = 0, ei = 0;
            for (j = 0; j < c.ships.length; j++) {
                var s = c.ships[j];
                if (s.kind === 'hauler') { s.pos.copy(cvTmp.A).addScaledVector(cvTmp.D, len * u - fi * 70 * L); s.quat.copy(cvTmp.Q); fi++; continue; }
                if (!s.alive) { ei++; continue; }
                if (c.state === 'attacked') {
                    s.ang += s.rate * dt; var R = 90 * L;
                    cvTmp.V.set(shipPos.x + Math.cos(s.ang) * R, shipPos.y + Math.sin(s.ang * 0.7) * R * 0.25, shipPos.z + Math.sin(s.ang) * R);
                    var ox = s.pos.x, oy = s.pos.y, oz = s.pos.z, k = Math.min(1, dt * 1.6);
                    if (s.pos.x > 1e11) s.pos.copy(cvTmp.A).addScaledVector(cvTmp.D, len * u);
                    s.pos.lerp(cvTmp.V, k);
                    cvTmp.V.set(s.pos.x - ox, s.pos.y - oy, s.pos.z - oz);
                    if (cvTmp.V.lengthSq() > 1e-12) { cvTmp.V.normalize(); cvTmp.M.lookAt(zeroV, cvTmp.V, up); s.quat.setFromRotationMatrix(cvTmp.M); }
                } else { s.pos.copy(cvTmp.A).addScaledVector(cvTmp.D, len * u - 20 * L).addScaledVector(cvTmp.R, (ei ? -1 : 1) * 45 * L); s.quat.copy(cvTmp.Q); }
                ei++;
            }
        }
        // derelicts
        for (i = 0; i < DER; i++) {
            var d = space.derelicts[i], bt = space.belts[d.belt];
            if (d.override) d.pos.copy(d.override); else if (bt) { d.a += d.rate * dt / Math.max(bt.mid, 1); d.pos.set(cen.x + Math.cos(d.a) * bt.mid, cen.y + d.y, cen.z + Math.sin(d.a) * bt.mid); } else continue;
            d.quat.copy(d.baseQ); d.group.position.copy(d.pos); d.group.quaternion.copy(d.quat);
            d.group.updateMatrixWorld(true);
            d.toWorld(cvTmp.V.set(0, 0, MOUTHZ), d.mouth.pos); d.mouth.dir.set(0, 0, 1).applyQuaternion(d.quat);
            for (j = 0; j < d.crates.length; j++) d.toWorld(d.crates[j].localPos, d.crates[j].pos);
            d.toWorld(d.guardian.localPos, d.guardian.pos);
            var inB = false; if (shipPos) { d.toLocal(shipPos, cvTmp.V); var bb = d.deck.bounds; inB = cvTmp.V.x > bb.min.x && cvTmp.V.x < bb.max.x && cvTmp.V.z > bb.min.z && cvTmp.V.z < MOUTHZ - 0.1 && Math.abs(cvTmp.V.y) < 2; }
            d.setInside(inB);
        }
        // focus = nearest convoy / derelict within HULL_NEAR
        var best = HULL_NEAR * L * HULL_NEAR * L, fk = '', fx = -1, dd2;
        for (i = 0; i < CONV; i++) { dd2 = space.convoys[i].ships[0].pos.distanceToSquared(shipPos); if (dd2 < best) { best = dd2; fk = 'c'; fx = i; } }
        for (i = 0; i < DER; i++) { dd2 = space.derelicts[i].pos.distanceToSquared(shipPos); if (dd2 < best) { best = dd2; fk = 'd'; fx = i; } }
        if (!poolInit && fk === 'c') {
            poolInit = true;
            for (i = 0; i < 2; i++) { var h = buildHull(THREE, { kind: 'hauler' }); darken(h); h.scale.setScalar(30 * L); h.visible = false; scene.add(h); pf.push(h); var h2 = buildHull(THREE, { kind: 'fighter' }); h2.traverse(function (o) { o.frustumCulled = false; }); h2.scale.setScalar(8 * L); h2.visible = false; scene.add(h2); pe.push(h2); }
        }
        if (poolInit) for (i = 0; i < 2; i++) { pf[i].visible = false; pe[i].visible = false; }
        for (i = 0; i < DER; i++) { var dv = space.derelicts[i]; dv.group.visible = fk === 'd' && fx === i; }
        for (i = 0; i < CONV; i++) {
            var cc = space.convoys[i], show = fk === 'c' && fx === i && poolInit, nf = 0, ne = 0;
            for (j = 0; j < cc.ships.length; j++) {
                var sh = cc.ships[j], po = (IMP0 + i * 5 + j) * 3, hull = null;
                if (show && sh.alive) { if (sh.kind === 'hauler') { if (nf < 2) hull = pf[nf]; nf++; } else { if (ne < 2) hull = pe[ne]; ne++; } }
                if (hull) { hull.visible = true; hull.position.copy(sh.pos); hull.quaternion.copy(sh.quat); pPos[po + 1] = FAR; }
                else if (sh.alive) { pPos[po] = sh.pos.x - cen.x; pPos[po + 1] = sh.pos.y - cen.y; pPos[po + 2] = sh.pos.z - cen.z; } else pPos[po + 1] = FAR;
            }
            for (j = cc.ships.length; j < 5; j++) pPos[(IMP0 + i * 5 + j) * 3 + 1] = FAR;
        }
        for (i = 0; i < DER; i++) { var dq = space.derelicts[i], pq = (IMP0 + CONV * 5 + i) * 3; if (dq.group.visible) pPos[pq + 1] = FAR; else { pPos[pq] = dq.pos.x - cen.x; pPos[pq + 1] = dq.pos.y - cen.y; pPos[pq + 2] = dq.pos.z - cen.z; } }
        pDirty = true; pAttr.needsUpdate = true;
        crateT += dt; if (cratesDirty || crateT > 0.4) { crateT = 0; syncCrates(); }
    }
    var _upd = space.update;
    space.update = function (dt, shipPos) { _upd(dt, shipPos); if (layoutOk) { updateTier3(dt, shipPos); updateMining(dt, shipPos); } };
    var _disp = space.dispose;
    space.dispose = function () {
        _disp(); scene.remove(dustPts); dustGeo.dispose(); dustMat.dispose(); scene.remove(chPts); chGeo.dispose(); chMat.dispose(); scene.remove(cPoints); cGeo.dispose(); cMat.dispose(); dMat.dispose();
        space.derelicts.forEach(function (d) { scene.remove(d.group); d.interiorMesh.geometry.dispose(); d.extMesh.geometry.dispose(); });
        poolInit && pf.concat(pe).forEach(function (h) { scene.remove(h); h.traverse(function (o) { if (o.geometry) o.geometry.dispose(); }); });
    };

    // ─── mining (rev 27): ore/ice/crystal rocks, chunks, respawn, scanner, belt dust ─────────────────────────
    var MINE_R = 300, MINE_MAX = 40, RESPAWN_MS = 600000, CH_MAX = 64, CH_LIFE = 20;
    var minPool = [], minList = [], EMPTY = [], MISS = { broken: false, chunks: EMPTY };
    space.chunks = [];
    var chPos = new Float32Array(CH_MAX * 3), chCol = new Float32Array(CH_MAX * 3);
    var chGeo = new THREE.BufferGeometry(); chGeo.setAttribute('position', new THREE.BufferAttribute(chPos, 3)); chGeo.setAttribute('color', new THREE.BufferAttribute(chCol, 3));
    var chMat = new THREE.PointsMaterial({ size: 2.2 * L, sizeAttenuation: true, map: tex, vertexColors: true, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, fog: false });
    var chPts = new THREE.Points(chGeo, chMat); chPts.frustumCulled = false; chPts.renderOrder = 2; group.add(chPts);
    var chSeq = 0, chRng = mulberry(0xC40C);
    function hexRGB(h, o) { o[0] = ((h >> 16) & 255) / 255; o[1] = ((h >> 8) & 255) / 255; o[2] = (h & 255) / 255; }
    var rgbT = [0, 0, 0];
    space.minable = function () {
        minList.length = 0; if (!layoutOk) return minList;
        var m2 = MINE_R * L * MINE_R * L;
        for (var id = 0; id < N && minList.length < MINE_MAX; id++) {
            if (!oreKind[id] || gone[id]) continue;
            var dx = rx[id] - shipLX, dy = ry[id] - shipLY, dz = rz[id] - shipLZ; if (dx * dx + dy * dy + dz * dz > m2) continue;
            var o = minPool[minList.length] || (minPool[minList.length] = { id: 0, pos: new THREE.Vector3(), r: 0, kind: '', hp: 0 });
            o.id = id; o.pos.set(rx[id] + cen.x, ry[id] + cen.y, rz[id] + cen.z); o.r = rr[id]; o.kind = KIND_NAME[oreKind[id]]; o.hp = hpArr[id]; minList.push(o);
        }
        return minList;
    };
    space.hitRock = function (id, dmg) {
        if (!oreKind[id] || gone[id]) return MISS;
        hpArr[id] -= dmg; if (hpArr[id] > 0) return MISS;
        var kind = KIND_NAME[oreKind[id]], nC = 3 + Math.floor(chRng() * 3), chunks = [], sz = rr0[id] / L;
        for (var i = 0; i < nC; i++) {
            var a = chRng() * 6.2832, u = chRng() * 2 - 1, rho = Math.sqrt(1 - u * u), sp = (4 + chRng() * 8) * L;
            var ch = { id: 'ch' + (chSeq++), pos: new THREE.Vector3(rx[id] + cen.x, ry[id] + cen.y, rz[id] + cen.z), vel: new THREE.Vector3(rho * Math.cos(a) * sp, u * sp, rho * Math.sin(a) * sp), item: resourceItem(kind, 'belt'), n: 1 + Math.floor(chRng() * 2 + sz / 8), age: 0 };
            chunks.push(ch); if (space.chunks.length >= CH_MAX) space.chunks.shift(); space.chunks.push(ch);
        }
        gone[id] = 1; goneAt[id] = Date.now() + RESPAWN_MS; goneList.push(id); rr[id] = 0; near[id] = 0; hideRock(id); pPos[id * 3 + 1] = FAR; pDirty = true;
        return { broken: true, chunks: chunks };
    };
    space.takeChunk = function (id) {
        for (var i = 0; i < space.chunks.length; i++) if (space.chunks[i].id === id) { var c = space.chunks[i]; space.chunks.splice(i, 1); return c; }
        return null;
    };
    space.mineState = function () { var o = []; for (var i = 0; i < goneList.length; i++) o.push([goneList[i], goneAt[goneList[i]]]); return o; };
    space.markMined = function (st) {
        if (!st) return; var now = Date.now();
        for (var i = 0; i < st.length; i++) {
            var id = st[i][0], at = st[i][1]; if (!(id >= 0 && id < N) || !oreKind[id] || gone[id] || at <= now) continue;
            gone[id] = 1; goneAt[id] = at; goneList.push(id); rr[id] = 0; near[id] = 0; hideRock(id); pPos[id * 3 + 1] = FAR; pDirty = true;
        }
    };
    var scanPool = [], scanList = [];
    function scanSlot(i) { return scanPool[i] || (scanPool[i] = { id: '', kind: '', pos: new THREE.Vector3(), ref: null }); }
    space.scanTargets = function () {
        scanList.length = 0; var i, o;
        for (var id = 0; id < N; id++) {
            if (!oreKind[id] || gone[id]) continue;
            o = scanSlot(scanList.length); o.id = id; o.kind = KIND_NAME[oreKind[id]]; o.ref = null; o.pos.set(rx[id] + cen.x, ry[id] + cen.y, rz[id] + cen.z); scanList.push(o);
        }
        for (i = 0; i < space.crates.length; i++) { var c = space.crates[i]; if (c.taken) continue; o = scanSlot(scanList.length); o.id = c.id; o.kind = 'crate'; o.ref = c; o.pos.copy(c.pos); scanList.push(o); }
        for (i = 0; i < space.derelicts.length; i++) {
            var d = space.derelicts[i]; o = scanSlot(scanList.length); o.id = d.id; o.kind = 'derelict'; o.ref = d; o.pos.copy(d.pos); scanList.push(o);
            for (var j = 0; j < d.crates.length; j++) if (!d.crates[j].taken) { o = scanSlot(scanList.length); o.id = d.crates[j].id; o.kind = 'crate'; o.ref = d.crates[j]; o.pos.copy(d.crates[j].pos); scanList.push(o); }
        }
        for (i = 0; i < space.convoys.length; i++) { var cv2 = space.convoys[i]; o = scanSlot(scanList.length); o.id = cv2.id; o.kind = 'convoy'; o.ref = cv2; o.pos.copy(cv2.ships[0].pos); scanList.push(o); }
        return scanList;
    };
    // belt dust: 800 motes wrapped in a box around the ship, only visible inside / near a belt
    var DUST = 800, DBOX = 700 * L, dOff = new Float32Array(DUST * 3), dVel = new Float32Array(DUST * 3), dPos = new Float32Array(DUST * 3), rngD = mulberry(0xD057);
    for (var di2 = 0; di2 < DUST * 3; di2++) { dOff[di2] = (rngD() * 2 - 1) * DBOX; dVel[di2] = (rngD() * 2 - 1) * 1.5 * L; }
    var dustGeo = new THREE.BufferGeometry(), dustAttr = new THREE.BufferAttribute(dPos, 3); dustAttr.setUsage(THREE.DynamicDrawUsage); dustGeo.setAttribute('position', dustAttr);
    var dustMat = new THREE.PointsMaterial({ size: 1.1 * L, sizeAttenuation: true, map: tex, color: 0xb8aa98, transparent: true, opacity: 0, depthWrite: false, fog: false });
    var dustPts = new THREE.Points(dustGeo, dustMat); dustPts.frustumCulled = false; dustPts.visible = false; group.add(dustPts);
    function updateMining(dt, shipPos) {
        var i, now = Date.now();
        for (i = goneList.length - 1; i >= 0; i--) {                       // respawn
            var gid = goneList[i]; if (goneAt[gid] > now) continue;
            gone[gid] = 0; rr[gid] = rr0[gid]; hpArr[gid] = 20 + rr0[gid] / L * 7; near[gid] = 2; goneList[i] = goneList[goneList.length - 1]; goneList.pop();
        }
        var ch = space.chunks, k = 0;                                      // chunks: drift, fade, expire
        for (i = 0; i < ch.length; i++) {
            var c = ch[i]; c.age += dt;
            if (c.age >= CH_LIFE) { ch[i] = ch[ch.length - 1]; ch.pop(); i--; continue; }
            var dmp = Math.max(0, 1 - 0.6 * dt); c.vel.multiplyScalar(dmp); c.pos.addScaledVector(c.vel, dt);
            var f = c.age > CH_LIFE - 5 ? (CH_LIFE - c.age) / 5 : 1; hexRGB(c.item.color, rgbT);
            chPos[k * 3] = c.pos.x - cen.x; chPos[k * 3 + 1] = c.pos.y - cen.y; chPos[k * 3 + 2] = c.pos.z - cen.z;
            chCol[k * 3] = rgbT[0] * f; chCol[k * 3 + 1] = rgbT[1] * f; chCol[k * 3 + 2] = rgbT[2] * f; k++;
        }
        chGeo.setDrawRange(0, k); chGeo.attributes.position.needsUpdate = true; chGeo.attributes.color.needsUpdate = true; chPts.visible = k > 0;
        // dust
        var rp = Math.hypot(shipLX, shipLZ), best = 1e30;
        for (i = 0; i < space.belts.length; i++) { var bt = space.belts[i], dd = rp < bt.inner ? bt.inner - rp : rp > bt.outer ? rp - bt.outer : 0; if (dd < best) best = dd; }
        var op = best > 400 * L ? 0 : (1 - best / (400 * L)) * 0.55; if (Math.abs(shipLY) > ext.y + 400 * L) op = 0;
        dustMat.opacity = op; dustPts.visible = op > 0.01;
        if (dustPts.visible) {
            for (i = 0; i < DUST * 3; i += 3) {
                for (var a = 0; a < 3; a++) { var o = dOff[i + a] + dVel[i + a] * dt; if (o > DBOX) o -= 2 * DBOX; else if (o < -DBOX) o += 2 * DBOX; dOff[i + a] = o; }
                dPos[i] = shipLX + dOff[i]; dPos[i + 1] = shipLY + dOff[i + 1]; dPos[i + 2] = shipLZ + dOff[i + 2];
            }
            dustAttr.needsUpdate = true;
        }
    }
    try { if (typeof location !== 'undefined' && /[?&](convoy|derelict|mine)\b/.test(location.search)) window.__space = space; } catch (e) { }

    space.rocks.center = cen;
    layout();
    for (var i4 = 0; i4 < FREIGHTERS; i4++) spawn(space.freighters[i4], true);
    freightInit = true;
    return space;
}

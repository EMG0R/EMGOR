// ship-fx.js — star streaks, thruster exhaust cones, bolts (core + halo), muzzle flash, impact sparks.
// API: const fx = createFx(THREE, scene, camera, L)
//   fx.setMotion(vel, speedNorm, pulsing)   fx.exhaust(n) -> Object3D[]   fx.setExhaust(i, intensity 0 idle..1 boost..2 pulse)
//   fx.spawnBolt(pos, dir, color, speed, life) -> id (-1 if full)   fx.updateBolts(dt)   fx.boltPos(id, out)   fx.killBolt(id)
//   fx.flash(pos, color)   fx.impact(pos, color, size)   fx.update(dt, camera)   fx.dispose()
// fx.setQuality(t 0..3)  (perf rev 17, ship-perf.js): streak count 200/320/480/600, exhaust cone radial segments 5/6/8/8,
//   sparks per impact 6/8/12/12. New instances start at globalThis.EMGOR_PERF_TIER (set by the quality manager), default 3.
// fx.setPixelRatio(pr): the renderer's pixel ratio, so point sprites stay the same on-screen size when the DPR cap changes.
// Culling note: every object here is positioned or displaced in its vertex shader (streaks wrap in a camera box, bolts and
// sparks are instanced billboards), so geometry bounds are meaningless and frustumCulled stays false; only the cones cull.
// Exhaust cones are authored in world units of L (length 0.6L idle .. 6L pulse); parent scale multiplies on top.
// Every shader includes the logdepthbuf chunks (renderer uses logarithmicDepthBuffer). Draw calls: 1 streaks + N cones
// + 1 bolts + 1 flash/sparks points. No per-frame allocation (module-scope scratch).

var _v = null, _c = null;   // scratch, created once on first createFx
var BOLT_CAP = 160, FLASH_CAP = 8, BURST_CAP = 40, BURST_N = 12, STAR_N = 600;
var Q_STREAKS = [200, 320, 480, 600], Q_CONESEG = [5, 6, 8, 8], Q_SPARKS = [6, 8, 12, 12];

var STREAK_VS = [
'uniform vec3 uCamPos; uniform vec3 uVelDir; uniform float uStretch; uniform float uBox; uniform float uAlpha; uniform float uL;',
'attribute float aEnd; attribute float aRnd;',
'varying float vA;',
'#include <common>',
'#include <logdepthbuf_pars_vertex>',
'void main() {',
'  vec3 rel = mod(position * uBox - uCamPos, uBox) - 0.5 * uBox;',
'  vec3 w = cameraPosition + rel;',
'  float len = uStretch * (0.5 + aRnd);',
'  w -= uVelDir * len * aEnd;',
'  float edge = 1.0 - smoothstep(0.34, 0.5, max(abs(rel.x), max(abs(rel.y), abs(rel.z))) / uBox);',
'  float near = smoothstep(1.5 * uL, 6.0 * uL, length(rel));',
'  vA = uAlpha * edge * near * (0.45 + 0.55 * aRnd) * (1.0 - 0.9 * aEnd);',
'  vec4 mv = viewMatrix * vec4(w, 1.0);',
'  gl_Position = projectionMatrix * mv;',
'  #include <logdepthbuf_vertex>',
'}'].join('\n');
var STREAK_FS = [
'varying float vA;',
'#include <logdepthbuf_pars_fragment>',
'void main() {',
'  #include <logdepthbuf_fragment>',
'  gl_FragColor = vec4(vec3(0.75, 0.92, 1.0) * 2.2, vA);',
'}'].join('\n');

var CONE_VS = [
'uniform float uTime; uniform float uIntensity; uniform float uPhase;',
'varying vec2 vUv; varying vec3 vN; varying vec3 vV;',
'#include <common>',
'#include <logdepthbuf_pars_vertex>',
'void main() {',
'  vUv = uv;',
'  float t = uv.y;',
'  vec3 p = position;',
'  float wob = sin(uTime * 31.0 + uPhase + p.z * 6.0) * 0.06 + sin(uTime * 17.0 + atan(p.y, p.x) * 3.0) * 0.05;',
'  p.xy *= 1.0 + wob * (0.4 + t);',
'  vec4 mv = modelViewMatrix * vec4(p, 1.0);',
'  vN = normalize(normalMatrix * normal);',
'  vV = normalize(-mv.xyz);',
'  gl_Position = projectionMatrix * mv;',
'  #include <logdepthbuf_vertex>',
'}'].join('\n');
var CONE_FS = [
'uniform float uTime; uniform float uIntensity; uniform float uPhase; uniform vec3 uBase; uniform vec3 uTip;',
'varying vec2 vUv; varying vec3 vN; varying vec3 vV;',
'#include <logdepthbuf_pars_fragment>',
'float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }',
'void main() {',
'  #include <logdepthbuf_fragment>',
'  float t = clamp(vUv.y, 0.0, 1.0);',
'  float rim = abs(dot(normalize(vN), normalize(vV)));',
'  float body = smoothstep(0.0, 0.7, rim);',
'  float fl = 0.78 + 0.22 * sin(uTime * 48.0 + uPhase + t * 14.0) + 0.18 * (hash(vec2(floor(uTime * 40.0), floor(t * 9.0) + uPhase)) - 0.5);',
'  float a = pow(1.0 - t, 1.15) * smoothstep(0.0, 0.1, t + 0.02) * body * fl;',
'  vec3 col = mix(uBase, uTip, smoothstep(0.05, 0.6, t));',
'  col = mix(col, vec3(1.0), pow(1.0 - t, 5.0) * body * 0.35);',
'  float k = 0.6 + 0.9 * uIntensity;',
'  gl_FragColor = vec4(col * k * 0.9, a * min(1.0, 0.4 + 0.3 * uIntensity));',
'}'].join('\n');

var BOLT_VS = [
'uniform vec3 uTrue; attribute vec3 aPos; attribute vec3 aDir; attribute vec3 aColor; attribute vec2 aSize;',
'varying vec2 vP; varying vec3 vC;',
'#include <common>',
'#include <logdepthbuf_pars_vertex>',
'void main() {',
'  vP = position.xy * 2.0; vC = aColor;',
'  vec4 c = viewMatrix * vec4(aPos - uTrue + cameraPosition, 1.0);',
'  vec3 d = mat3(viewMatrix) * aDir;',
'  vec2 dd = d.xy; float dl = length(dd);',
'  dd = dl > 1e-4 ? dd / dl : vec2(1.0, 0.0);',
'  vec2 sd = vec2(-dd.y, dd.x);',
'  float lenW = aSize.x * (0.25 + 0.75 * dl);',
'  vec3 mv = c.xyz + vec3(dd * position.x * lenW + sd * position.y * aSize.y, 0.0);',
'  if (aSize.x <= 0.0) mv = vec3(0.0, 0.0, 1e5);',
'  gl_Position = projectionMatrix * vec4(mv, 1.0);',
'  #include <logdepthbuf_vertex>',
'}'].join('\n');
var BOLT_FS = [
'varying vec2 vP; varying vec3 vC;',
'#include <logdepthbuf_pars_fragment>',
'void main() {',
'  #include <logdepthbuf_fragment>',
'  float v2 = vP.y * vP.y;',
'  float along = 1.0 - pow(abs(vP.x), 3.0);',
'  float core = exp(-v2 * 40.0) * along;',
'  float halo = exp(-v2 * 3.5) * along * (1.0 - vP.x * vP.x * 0.5);',
'  vec3 col = vC * halo * 1.7 + mix(vC, vec3(1.0), 0.8) * core * 3.0;',
'  gl_FragColor = vec4(col, clamp(halo * 0.8 + core, 0.0, 1.0));',
'}'].join('\n');

var PTS_VS = [
'attribute vec3 aOrigin; attribute vec3 aVel; attribute vec3 aColor; attribute float aT; attribute float aSize;',
'uniform float uScale; uniform float uL; uniform vec3 uTrue;',
'varying vec3 vC; varying float vT;',
'#include <common>',
'#include <logdepthbuf_pars_vertex>',
'void main() {',
'  vC = aColor; vT = aT;',
'  float age = max(aT, 0.0);',
'  vec3 p = aOrigin + aVel * (1.0 - exp(-4.0 * age)) * 0.25;',
'  vec4 mv = viewMatrix * vec4(p - uTrue + cameraPosition, 1.0);',
'  gl_Position = projectionMatrix * mv;',
'  float s = aSize * (aVel == vec3(0.0) ? (1.0 + age * 0.6) : (1.0 - 0.7 * age));',
'  gl_PointSize = aT < 0.0 ? 0.0 : clamp(s * uScale / max(-mv.z, 1e-6), 1.0, 160.0);',
'  #include <logdepthbuf_vertex>',
'}'].join('\n');
var PTS_FS = [
'varying vec3 vC; varying float vT;',
'#include <logdepthbuf_pars_fragment>',
'void main() {',
'  #include <logdepthbuf_fragment>',
'  vec2 q = gl_PointCoord * 2.0 - 1.0; float r2 = dot(q, q);',
'  if (r2 > 1.0) discard;',
'  float g = exp(-r2 * 4.0), core = exp(-r2 * 16.0);',
'  float f = (1.0 - vT); f *= f;',
'  gl_FragColor = vec4((vC * g * 1.6 + vec3(1.0) * core * 0.9) * 1.6, g * f);',
'}'].join('\n');

export function createFx(THREE, scene, camera, L) {
    if (!_v) { _v = new THREE.Vector3(); _c = new THREE.Color(); }
    var time = 0, disposed = false;
    var fx = {};
    var geoms = [], mats = [], objs = [];
    function track(o, g, m) { if (g) geoms.push(g); if (m) mats.push(m); if (o) objs.push(o); }

    // ── streaks ────────────────────────────────────────────────
    var BOX = 60 * L;
    var sg = new THREE.BufferGeometry();
    var sp = new Float32Array(STAR_N * 2 * 3), se = new Float32Array(STAR_N * 2), sr = new Float32Array(STAR_N * 2);
    for (var i = 0; i < STAR_N; i++) {
        var x = Math.random(), y = Math.random(), z = Math.random(), r = Math.random();
        for (var k = 0; k < 2; k++) {
            var j = i * 2 + k;
            sp[j * 3] = x; sp[j * 3 + 1] = y; sp[j * 3 + 2] = z;
            se[j] = k; sr[j] = r;
        }
    }
    sg.setAttribute('position', new THREE.BufferAttribute(sp, 3));
    sg.setAttribute('aEnd', new THREE.BufferAttribute(se, 1));
    sg.setAttribute('aRnd', new THREE.BufferAttribute(sr, 1));
    var smat = new THREE.ShaderMaterial({
        vertexShader: STREAK_VS, fragmentShader: STREAK_FS, transparent: true, depthWrite: false, depthTest: false,
        blending: THREE.AdditiveBlending,
        uniforms: {
            uCamPos: { value: new THREE.Vector3() }, uVelDir: { value: new THREE.Vector3(0, 0, -1) },
            uStretch: { value: 0 }, uBox: { value: BOX }, uAlpha: { value: 0 }, uL: { value: L }
        }
    });
    var streaks = new THREE.LineSegments(sg, smat);
    streaks.frustumCulled = false; streaks.renderOrder = 20; streaks.visible = false;
    scene.add(streaks); track(streaks, sg, smat);
    fx.streaks = streaks;
    var mSpeed = 0, mPulse = 0, mPulseTgt = 0, mSpeedTgt = 0;
    fx.setMotion = function (vel, speedNorm, pulsing) {
        var l = vel.length();
        if (l > 1e-9) smat.uniforms.uVelDir.value.copy(vel).multiplyScalar(1 / l);
        mSpeedTgt = speedNorm; mPulseTgt = pulsing ? 1 : 0;
    };

    // ── exhaust ────────────────────────────────────────────────
    var cones = [], coneTgt = [], coneCur = [];
    var coneGeos = {};
    function coneGeoFor(seg) {
        var g = coneGeos[seg];
        if (!g) {
            g = new THREE.ConeGeometry(0.16, 1, seg, 1, true);
            g.translate(0, 0.5, 0); g.rotateX(Math.PI / 2);    // base at origin, apex toward +Z
            coneGeos[seg] = g; geoms.push(g);
        }
        return g;
    }
    var coneGeo = coneGeoFor(8);
    fx.exhaust = function (n) {
        var out = [];
        for (var i = 0; i < n; i++) {
            var m = new THREE.ShaderMaterial({
                vertexShader: CONE_VS, fragmentShader: CONE_FS, transparent: true, depthWrite: false,
                side: THREE.DoubleSide, blending: THREE.AdditiveBlending,
                uniforms: {
                    uTime: { value: 0 }, uIntensity: { value: 0 }, uPhase: { value: Math.random() * 20 },
                    uBase: { value: new THREE.Color(0x7DE8F0) }, uTip: { value: new THREE.Color(0x7B2FBE) }
                }
            });
            var mesh = new THREE.Mesh(coneGeo, m);
            mesh.frustumCulled = true; mesh.renderOrder = 15;
            mesh.scale.set(L, L, 0.6 * L);
            mats.push(m); cones.push(mesh); coneTgt.push(0); coneCur.push(0); out.push(mesh);
        }
        return out;
    };
    fx.setExhaust = function (i, intensity) { if (i >= 0 && i < cones.length) coneTgt[i] = intensity; };
    function coneLen(t) { return t <= 1 ? 0.6 + 1.9 * (t < 0 ? 0 : t) : 2.5 + 3.5 * (t > 2 ? 1 : t - 1); }

    // ── bolts ──────────────────────────────────────────────────
    var bg = new THREE.InstancedBufferGeometry();
    var q = new THREE.PlaneGeometry(1, 1);
    bg.index = q.index; bg.setAttribute('position', q.getAttribute('position'));
    var bPos = new Float32Array(BOLT_CAP * 3), bDir = new Float32Array(BOLT_CAP * 3), bCol = new Float32Array(BOLT_CAP * 3), bSize = new Float32Array(BOLT_CAP * 2);
    var bSpd = new Float32Array(BOLT_CAP), bLife = new Float32Array(BOLT_CAP), bAlive = new Uint8Array(BOLT_CAP);
    var aPosA = new THREE.InstancedBufferAttribute(bPos, 3), aDirA = new THREE.InstancedBufferAttribute(bDir, 3),
        aColA = new THREE.InstancedBufferAttribute(bCol, 3), aSizeA = new THREE.InstancedBufferAttribute(bSize, 2);
    [aPosA, aDirA, aColA, aSizeA].forEach(function (a) { a.setUsage(THREE.DynamicDrawUsage); });
    bg.setAttribute('aPos', aPosA); bg.setAttribute('aDir', aDirA); bg.setAttribute('aColor', aColA); bg.setAttribute('aSize', aSizeA);
    var bmat = new THREE.ShaderMaterial({ vertexShader: BOLT_VS, fragmentShader: BOLT_FS, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide, uniforms: { uTrue: { value: new THREE.Vector3() } } });
    var bolts = new THREE.InstancedMesh(bg, bmat, BOLT_CAP);
    bolts.frustumCulled = false; bolts.renderOrder = 18; bolts.count = 0;
    scene.add(bolts); track(bolts, bg, bmat); geoms.push(q);
    var bHigh = 0, bCursor = 0;
    fx.bolts = bolts;
    fx.spawnBolt = function (pos, dir, color, speed, life) {
        var id = -1;
        for (var n = 0; n < BOLT_CAP; n++) {
            var s = (bCursor + n) % BOLT_CAP;
            if (!bAlive[s]) { id = s; break; }
        }
        if (id < 0) return -1;
        bCursor = (id + 1) % BOLT_CAP;
        bAlive[id] = 1;
        bPos[id * 3] = pos.x; bPos[id * 3 + 1] = pos.y; bPos[id * 3 + 2] = pos.z;
        var dl = dir.length() || 1;
        bDir[id * 3] = dir.x / dl; bDir[id * 3 + 1] = dir.y / dl; bDir[id * 3 + 2] = dir.z / dl;
        _c.set(color === undefined ? 0xffffff : color);
        bCol[id * 3] = _c.r; bCol[id * 3 + 1] = _c.g; bCol[id * 3 + 2] = _c.b;
        bSize[id * 2] = 3.2 * L; bSize[id * 2 + 1] = 1.2 * L;
        bSpd[id] = speed; bLife[id] = life;
        if (id + 1 > bHigh) bHigh = id + 1;
        aPosA.needsUpdate = aDirA.needsUpdate = aColA.needsUpdate = aSizeA.needsUpdate = true;
        return id;
    };
    fx.killBolt = function (id) {
        if (id < 0 || id >= BOLT_CAP || !bAlive[id]) return;
        bAlive[id] = 0; bSize[id * 2] = 0; aSizeA.needsUpdate = true;
        while (bHigh > 0 && !bAlive[bHigh - 1]) bHigh--;
    };
    fx.boltPos = function (id, out) {
        if (id < 0 || id >= BOLT_CAP || !bAlive[id]) return null;
        return out.set(bPos[id * 3], bPos[id * 3 + 1], bPos[id * 3 + 2]);
    };
    fx.updateBolts = function (dt) {
        for (var i = 0; i < bHigh; i++) {
            if (!bAlive[i]) continue;
            var s = bSpd[i] * dt;
            bPos[i * 3] += bDir[i * 3] * s; bPos[i * 3 + 1] += bDir[i * 3 + 1] * s; bPos[i * 3 + 2] += bDir[i * 3 + 2] * s;
            bLife[i] -= dt;
            if (bLife[i] <= 0) fx.killBolt(i);
        }
        bolts.count = bHigh; aPosA.needsUpdate = true;
    };

    // ── flash + impact sparks (one Points: 8 flash + 40*12 sparks) ─
    var TOT = FLASH_CAP + BURST_CAP * BURST_N;
    var pg = new THREE.BufferGeometry();
    var pO = new Float32Array(TOT * 3), pV = new Float32Array(TOT * 3), pC = new Float32Array(TOT * 3), pT = new Float32Array(TOT).fill(-1), pS = new Float32Array(TOT);
    var pPos = new Float32Array(TOT * 3);
    pg.setAttribute('position', new THREE.BufferAttribute(pPos, 3));
    var aO = new THREE.BufferAttribute(pO, 3), aT = new THREE.BufferAttribute(pT, 1), aS = new THREE.BufferAttribute(pS, 1),
        aC = new THREE.BufferAttribute(pC, 3), aV = new THREE.BufferAttribute(pV, 3);
    aT.setUsage(THREE.DynamicDrawUsage);
    pg.setAttribute('aOrigin', aO); pg.setAttribute('aVel', aV); pg.setAttribute('aColor', aC); pg.setAttribute('aT', aT); pg.setAttribute('aSize', aS);
    var pmat = new THREE.ShaderMaterial({
        vertexShader: PTS_VS, fragmentShader: PTS_FS, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
        uniforms: { uScale: { value: 800 }, uL: { value: L }, uTrue: { value: new THREE.Vector3() } }
    });
    var pts = new THREE.Points(pg, pmat);
    pts.frustumCulled = false; pts.renderOrder = 19;
    scene.add(pts); track(pts, pg, pmat);
    var fAge = new Float32Array(FLASH_CAP).fill(-1), fCur = 0;      // flash age in s
    var xAge = new Float32Array(BURST_CAP).fill(-1), xCur = 0;      // burst age in s
    var xN = new Uint8Array(BURST_CAP).fill(BURST_N), burstN = BURST_N;   // sparks actually emitted per burst (quality)
    var FLASH_T = 0.08, BURST_T = 0.4;
    fx.flash = function (pos, color) {
        var f = fCur; fCur = (fCur + 1) % FLASH_CAP;
        fAge[f] = 0; _c.set(color === undefined ? 0xffffff : color);
        pO[f * 3] = pos.x; pO[f * 3 + 1] = pos.y; pO[f * 3 + 2] = pos.z;
        pV[f * 3] = pV[f * 3 + 1] = pV[f * 3 + 2] = 0;
        pC[f * 3] = _c.r; pC[f * 3 + 1] = _c.g; pC[f * 3 + 2] = _c.b;
        pS[f] = 2.2 * L; pT[f] = 0;
        aO.needsUpdate = aV.needsUpdate = aC.needsUpdate = aS.needsUpdate = aT.needsUpdate = true;
    };
    fx.impact = function (pos, color, size) {
        size = size || 1;
        var b = xCur; xCur = (xCur + 1) % BURST_CAP;
        xAge[b] = 0; _c.set(color === undefined ? 0xffffff : color);
        var base = FLASH_CAP + b * BURST_N;
        xN[b] = burstN;
        for (var i = 0; i < BURST_N; i++) {
            var p = base + i;
            var u = Math.random() * 2 - 1, th = Math.random() * 6.2832, sq = Math.sqrt(1 - u * u);
            var sp2 = (6 + Math.random() * 14) * L * size;
            pO[p * 3] = pos.x; pO[p * 3 + 1] = pos.y; pO[p * 3 + 2] = pos.z;
            pV[p * 3] = sq * Math.cos(th) * sp2; pV[p * 3 + 1] = u * sp2; pV[p * 3 + 2] = sq * Math.sin(th) * sp2;
            pC[p * 3] = _c.r; pC[p * 3 + 1] = _c.g; pC[p * 3 + 2] = _c.b;
            pS[p] = (0.35 + Math.random() * 0.4) * L * size; pT[p] = i < burstN ? 0 : -1;
        }
        aO.needsUpdate = aV.needsUpdate = aC.needsUpdate = aS.needsUpdate = aT.needsUpdate = true;
    };

    // ── quality (perf rev 17) ──────────────────────────────────
    var pixelRatio = window.devicePixelRatio || 1, tier = 3;
    fx.setPixelRatio = function (pr) { if (pr > 0) pixelRatio = pr; };
    fx.setQuality = function (t) {
        t = t < 0 ? 0 : t > 3 ? 3 : Math.round(t); tier = t;
        sg.setDrawRange(0, Q_STREAKS[t] * 2);                       // 2 verts per streak
        var g = coneGeoFor(Q_CONESEG[t]);
        for (var i = 0; i < cones.length; i++) if (cones[i].geometry !== g) cones[i].geometry = g;
        coneGeo = g;
        burstN = Q_SPARKS[t];
    };
    Object.defineProperty(fx, 'quality', { get: function () { return tier; } });
    var _gt = typeof globalThis !== 'undefined' ? globalThis.EMGOR_PERF_TIER : undefined;
    if (typeof _gt === 'number') fx.setQuality(_gt);
    var _gp = typeof globalThis !== 'undefined' ? globalThis.EMGOR_PERF_PR : undefined;
    if (typeof _gp === 'number') fx.setPixelRatio(_gp);

    // ── update ─────────────────────────────────────────────────
    fx.update = function (dt, cam) {
        if (disposed) return;
        cam = cam || camera; time += dt;
        var ce = cam.matrixWorld.elements;
        var u = smat.uniforms;
        u.uCamPos.value.set(ce[12], ce[13], ce[14]);   // TRUE world cam (wrap anchor); placement uses built-in cameraPosition
        bmat.uniforms.uTrue.value.set(ce[12], ce[13], ce[14]); pmat.uniforms.uTrue.value.set(ce[12], ce[13], ce[14]);   // floating origin: shader subtracts this, adds shifted cameraPosition
        var k = 1 - Math.exp(-dt * 6);
        mSpeed += (mSpeedTgt - mSpeed) * k;
        mPulse += (mPulseTgt - mPulse) * (1 - Math.exp(-dt * (mPulseTgt > mPulse ? 14 : 5)));
        var a = (mSpeed - 0.3) / 0.7; a = a < 0 ? 0 : a > 1 ? 1 : a;
        u.uAlpha.value = a * (0.55 + 0.45 * mPulse);
        u.uStretch.value = L * (0.15 + 0.85 * a * a * 1.2 + 11 * mPulse);
        streaks.visible = a > 0.002;
        for (var i = 0; i < cones.length; i++) {
            coneCur[i] += (coneTgt[i] - coneCur[i]) * (1 - Math.exp(-dt * 10));
            var c = cones[i], len = coneLen(coneCur[i]);
            c.scale.set(L * (0.8 + 0.25 * Math.min(coneCur[i], 2)), L * (0.8 + 0.25 * Math.min(coneCur[i], 2)), len * L);
            var un = c.material.uniforms; un.uTime.value = time; un.uIntensity.value = coneCur[i] < 0 ? 0 : coneCur[i];
        }
        // points
        var any = false, i2, j;
        for (i2 = 0; i2 < FLASH_CAP; i2++) if (fAge[i2] >= 0) {
            fAge[i2] += dt; any = true;
            if (fAge[i2] >= FLASH_T) { fAge[i2] = -1; pT[i2] = -1; } else pT[i2] = fAge[i2] / FLASH_T;
        }
        for (i2 = 0; i2 < BURST_CAP; i2++) if (xAge[i2] >= 0) {
            xAge[i2] += dt; any = true;
            var done = xAge[i2] >= BURST_T, t = xAge[i2] / BURST_T, base = FLASH_CAP + i2 * BURST_N;
            if (done) xAge[i2] = -1;
            for (j = 0; j < xN[i2]; j++) pT[base + j] = done ? -1 : t;
        }
        if (any) aT.needsUpdate = true;
        pts.visible = true;
        pmat.uniforms.uScale.value = (window.innerHeight * pixelRatio) * 0.5 / Math.tan((cam.fov || 50) * Math.PI / 360);
        fx.updateBolts(dt);
    };

    fx.dispose = function () {
        disposed = true;
        for (var i = 0; i < objs.length; i++) scene.remove(objs[i]);
        geoms.forEach(function (g) { g.dispose(); });
        mats.forEach(function (m) { m.dispose(); });
        for (i = 0; i < cones.length; i++) if (cones[i].parent) cones[i].parent.remove(cones[i]);
        cones.length = 0;
    };
    return fx;
}

// ship-fx.js — star streaks, thruster exhaust cones, bolts (core + halo), muzzle flash, impact sparks.
// API: const fx = createFx(THREE, scene, camera, L)
//   fx.setMotion(vel, speedNorm, pulsing)   fx.exhaust(n) -> Object3D[]   fx.setExhaust(i, intensity 0 idle..1 boost..2 pulse)
//   fx.spawnBolt(pos, dir, color, speed, life, opts?) -> id (-1 if full); opts {shape:'bolt'|'needle'|'orb'|'shard', size:mult, trail:bool (default on for orb/shard)}   fx.updateBolts(dt)   fx.boltPos(id, out)   fx.killBolt(id)
//   fx.flash(pos, color, size?)   fx.impact(pos, color, size, kind?, opt?) kind 'hit'|'crit'|'burn'|'chain'(opt=to vec3)|'shieldHit'|'explode'
//   fx.muzzle(pos, dir, color, shape)   fx.hitStop() (no-op hook)   fx.update(dt, camera)   fx.dispose()
// rev 26: bolts, trail ghosts and ring/hex/smoke/arc sprites share ONE instanced draw (premultiplied blend: glow additive, smoke real alpha).
// fx.setQuality(t 0..3)  (perf rev 17, ship-perf.js): streak count 200/320/480/600, exhaust cone radial segments 5/6/8/8,
//   sparks per impact 6/8/12/12. New instances start at globalThis.EMGOR_PERF_TIER (set by the quality manager), default 3.
// fx.setPixelRatio(pr): the renderer's pixel ratio, so point sprites stay the same on-screen size when the DPR cap changes.
// Culling note: every object here is positioned or displaced in its vertex shader (streaks wrap in a camera box, bolts and
// sparks are instanced billboards), so geometry bounds are meaningless and frustumCulled stays false; only the cones cull.
// Exhaust cones are authored in world units of L (length 0.6L idle .. 6L pulse); parent scale multiplies on top.
// Every shader includes the logdepthbuf chunks (renderer uses logarithmicDepthBuffer). Draw calls: 1 streaks + N cones
// + 1 bolts + 1 flash/sparks points. No per-frame allocation (module-scope scratch).

var _v = null, _c = null, _c2 = null;   // scratch, created once on first createFx
var BOLT_CAP = 160, FLASH_CAP = 8, BURST_CAP = 56, BURST_N = 12, SPR_CAP = 48, STAR_N = 600;
var Q_STREAKS = [200, 320, 480, 600], Q_CONESEG = [5, 6, 8, 8], Q_SPARKS = [6, 8, 12, 12], Q_SMOKE = [2, 4, 6, 6];

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
// rev 25: a streak never spans more than 40 % of the screen height: project head + tail, shorten the tail if it would
'  vec4 c0 = projectionMatrix * (viewMatrix * vec4(w, 1.0));',
'  vec4 c1 = projectionMatrix * (viewMatrix * vec4(w - uVelDir * len, 1.0));',
'  float tl = 1.0;',
'  if (c0.w > 0.01 && c1.w > 0.01) {',
'    vec2 dd = (c1.xy / c1.w - c0.xy / c0.w) * vec2(projectionMatrix[1][1] / projectionMatrix[0][0], 1.0);',
'    tl = min(1.0, 0.4 / max(length(dd) * 0.5, 1e-4));',
'  }',
'  w -= uVelDir * len * tl * aEnd;',
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
'uniform vec3 uTrue; uniform float uTime; attribute vec3 aPos; attribute vec3 aDir; attribute vec3 aColor; attribute vec2 aSize; attribute vec4 aMisc;',
'varying vec2 vP; varying vec3 vC; varying vec4 vM; varying float vAsp;',
'#include <common>',
'#include <logdepthbuf_pars_vertex>',
'void main() {',
'  vP = position.xy * 2.0; vC = aColor; vM = aMisc; vAsp = 1.0;',
'  vec4 c = viewMatrix * vec4(aPos - uTrue + cameraPosition, 1.0);',
'  vec3 d = mat3(viewMatrix) * aDir;',
'  vec2 dd = d.xy; float dl = length(dd);',
'  dd = dl > 1e-4 ? dd / dl : vec2(1.0, 0.0);',
'  vec2 sd = vec2(-dd.y, dd.x);',
'  float sh = aMisc.x; vec2 off;',
'  if (sh > 2.5 && sh < 3.5) {',                                  // shard: rotating quad
'    float a = aMisc.w + mod(uTime, 600.0) * aMisc.y; float cs = cos(a), sn = sin(a);',
'    vec2 q = position.xy * aSize; off = vec2(cs * q.x - sn * q.y, sn * q.x + cs * q.y);',
'  } else if (sh > 3.5 && sh < 6.5) {',                           // ring / hex / smoke sprites (smoke grows)
'    float g = sh > 5.5 ? 0.45 + 0.9 * aMisc.z : 1.0;',
'    off = position.xy * aSize * g;',
'  } else {',                                                     // bolt / needle / orb / arc: along travel dir
'    float lenW = aSize.x * (0.25 + 0.75 * dl);',
'    if (sh > 1.5 && sh < 2.5) vAsp = lenW / max(aSize.y, 1e-6);',
'    off = dd * position.x * lenW + sd * position.y * aSize.y;',
'  }',
'  vec3 mv = c.xyz + vec3(off, 0.0);',
'  if (aSize.x <= 0.0) mv = vec3(0.0, 0.0, 1e5);',
'  gl_Position = projectionMatrix * vec4(mv, 1.0);',
'  #include <logdepthbuf_vertex>',
'}'].join('\n');
// Blend is premultiplied (src ONE, dst ONE_MINUS_SRC_ALPHA): glow shapes output alpha 0 (pure additive), smoke outputs real alpha (darkens).
var BOLT_FS = [
'uniform float uTime;',
'varying vec2 vP; varying vec3 vC; varying vec4 vM; varying float vAsp;',
'#include <logdepthbuf_pars_fragment>',
'void main() {',
'  #include <logdepthbuf_fragment>',
'  float sh = vM.x; vec3 rgb = vec3(0.0); float sa = 0.0; vec2 p = vP; float v2 = p.y * p.y;',
'  float tm = mod(uTime, 600.0);',
'  if (sh < 0.5) {',                                              // bolt
'    float along = 1.0 - pow(abs(p.x), 3.0);',
'    float core = exp(-v2 * 40.0) * along;',
'    float halo = exp(-v2 * 3.5) * along * (1.0 - p.x * p.x * 0.5);',
'    vec3 col = vC * halo * 1.7 + mix(vC, vec3(1.0), 0.8) * core * 3.0;',
'    rgb = col * clamp(halo * 0.8 + core, 0.0, 1.0) * vM.z;',
'  } else if (sh < 1.5) {',                                       // needle: long thin hot core, hot tip
'    float along = 1.0 - pow(abs(p.x), 6.0);',
'    float vy = p.y / (0.3 + 0.7 * smoothstep(-1.0, 0.7, p.x));',
'    float core = exp(-vy * vy * 70.0) * along;',
'    float halo = exp(-vy * vy * 7.0) * along * 0.4;',
'    float hs = exp(-(p.x - 0.8) * (p.x - 0.8) * 50.0 - v2 * 25.0);',
'    rgb = (vC * halo * 1.8 + mix(vC, vec3(1.0), 0.9) * core * 3.2 + vec3(1.0) * hs * 1.4) * vM.z;',
'  } else if (sh < 2.5) {',                                       // orb: pulsing round halo + trailing wisp
'    float hq = 0.32 * vAsp; vec2 q = vec2(p.x * vAsp - hq, p.y);',
'    float pul = 1.0 + 0.14 * sin(tm * 13.0 + vM.w);',
'    float r2 = dot(q, q) / (pul * pul);',
'    float halo = exp(-r2 * 2.6), core = exp(-r2 * 14.0);',
'    float tt = clamp(-q.x / (vAsp + hq), 0.0, 1.0);',
'    float wisp = q.x < 0.0 ? exp(-q.y * q.y * (9.0 + 30.0 * tt)) * pow(1.0 - tt, 2.0) * (0.55 + 0.2 * sin(tm * 20.0 + q.x * 6.0 + vM.w)) : 0.0;',
'    float ef = 1.0 - smoothstep(0.72, 1.0, max(abs(p.x), abs(p.y)));',
'    rgb = (vC * (halo * 1.4 + wisp * 1.3) + mix(vC, vec3(1.0), 0.75) * core * 2.4) * ef * vM.z;',
'  } else if (sh < 3.5) {',                                       // shard: angular faceted kite + sparkle
'    float d = abs(p.x) * 1.5 + abs(p.y) * 0.8;',
'    float body = 1.0 - smoothstep(0.64, 0.72, d);',
'    float facet = p.x * 0.8 + p.y * 0.3 > 0.0 ? 1.15 : 0.6;',
'    float rim = smoothstep(0.45, 0.7, d) * body;',
'    float halo = exp(-dot(p, p) * 3.0) * 0.4;',
'    float spk = max(exp(-abs(p.x * p.y) * 55.0) * (0.45 + 0.55 * sin(tm * 26.0 + vM.w)), 0.0) * (1.0 - smoothstep(0.3, 1.0, length(p)));',
'    rgb = (vC * (body * facet * 1.1 + rim * 0.9 + halo) + vec3(1.0) * (spk * 1.6 + body * (facet - 0.6) * 0.5)) * vM.z;',
'  } else if (sh < 4.5) {',                                       // ring flash
'    float t = vM.z, r = length(p); float s = 1.0 - pow(1.0 - t, 3.0);',
'    float k = (r - s * 0.92) * (14.0 - 7.0 * t);',
'    float ring = exp(-k * k);',
'    float in0 = exp(-r * r * 7.0) * max(0.0, 1.0 - t * 3.5);',
'    rgb = (vC * ring * 2.4 + vec3(ring * 0.7) + mix(vC, vec3(1.0), 0.7) * in0) * pow(1.0 - t, 1.5);',
'  } else if (sh < 5.5) {',                                       // hex shield ripple
'    float t = vM.z; float a0 = vM.w; float cs = cos(a0), sn = sin(a0);',
'    vec2 h = vec2(cs * p.x - sn * p.y, sn * p.x + cs * p.y);',
'    float hd = max(abs(h.x) * 0.866 + abs(h.y) * 0.5, abs(h.y));',
'    float front = 0.15 + 0.85 * (1.0 - pow(1.0 - t, 2.0));',
'    float ring = exp(-pow((hd - front) * 10.0, 2.0));',
'    float g1 = abs(sin(h.x * 14.0)), g2 = abs(sin(dot(h, vec2(0.5, 0.866)) * 14.0)), g3 = abs(sin(dot(h, vec2(-0.5, 0.866)) * 14.0));',
'    float grid = exp(-min(min(g1, g2), g3) * 7.0);',
'    float fill = smoothstep(front, 0.0, hd) * (0.2 + 0.9 * grid) * 0.55;',
'    rgb = (vC * (ring * 2.2 + fill * 1.6) + vec3(ring * 0.4 + fill * 0.2)) * pow(1.0 - t, 1.2) * (1.0 - smoothstep(0.8, 1.0, hd));',
'  } else if (sh < 6.5) {',                                       // dark smoke puff (real alpha)
'    float t = vM.z; float ang = atan(p.y, p.x);',
'    float r = length(p) * (1.0 + 0.28 * sin(ang * 3.0 + vM.w * 7.0) + 0.15 * sin(ang * 5.0 - vM.w * 3.0));',
'    float a = smoothstep(1.0, 0.25, r) * 0.62 * (1.0 - smoothstep(0.35, 1.0, t)) * smoothstep(0.0, 0.08, t + 0.04) * (1.0 - smoothstep(0.6, 0.95, length(p)));',
'    vec3 col = mix(vC * 1.1, vec3(0.035, 0.03, 0.04), smoothstep(0.0, 0.3, t));',
'    rgb = col * a; sa = a;',
'  } else {',                                                     // chain arc: jagged lightning line
'    float t = vM.z; float q = floor(tm * 45.0);',
'    float env = 1.0 - p.x * p.x;',
'    float j = 0.42 * env * (sin(p.x * 19.0 + vM.w * 37.0 + q * 5.1) * 0.6 + sin(p.x * 47.0 + vM.w * 11.0 + q * 2.3) * 0.4);',
'    float dy = p.y - j;',
'    float core = exp(-dy * dy * 160.0), halo = exp(-dy * dy * 14.0) * 0.5;',
'    float f = (1.0 - t) * (0.7 + 0.3 * sin(tm * 90.0)) * (1.0 - pow(abs(p.x), 8.0));',
'    rgb = (vC * halo * 1.6 + mix(vC, vec3(1.0), 0.85) * core * 3.0) * f;',
'  }',
'  gl_FragColor = vec4(rgb, sa);',
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
    if (!_v) { _v = new THREE.Vector3(); _c = new THREE.Color(); _c2 = new THREE.Color(0xff7a18); }
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
        vertexShader: STREAK_VS, fragmentShader: STREAK_FS, transparent: true, depthWrite: false, depthTest: true,
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
    var atmoD = 0;
    fx.setAtmo = function (depth) { atmoD = depth > 0 ? (depth > 1 ? 1 : depth) : 0; };   // rev 25: planet atmosphere depth 0..1 (0 = space, default); streaks fade out by depth 0.2
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

    // ── bolts (+ trail ghosts + impact sprites: ONE InstancedMesh, one draw call) ──
    // slots: [0,BOLT_CAP) bolts · [GH0, GH0+3*BOLT_CAP) trail ghosts (3 per bolt) · [SPR0, SPR0+SPR_CAP) sprites (ring/hex/smoke/arc)
    var GH0 = BOLT_CAP, SPR0 = BOLT_CAP * 4, TOTB = SPR0 + SPR_CAP;
    var bg = new THREE.InstancedBufferGeometry();
    var q = new THREE.PlaneGeometry(1, 1);
    bg.index = q.index; bg.setAttribute('position', q.getAttribute('position'));
    var bPos = new Float32Array(TOTB * 3), bDir = new Float32Array(TOTB * 3), bCol = new Float32Array(TOTB * 3), bSize = new Float32Array(TOTB * 2), bMisc = new Float32Array(TOTB * 4);
    var bSpd = new Float32Array(BOLT_CAP), bLife = new Float32Array(BOLT_CAP), bAlive = new Uint8Array(BOLT_CAP), bTrail = new Uint8Array(BOLT_CAP), bGap = new Float32Array(BOLT_CAP);
    var aPosA = new THREE.InstancedBufferAttribute(bPos, 3), aDirA = new THREE.InstancedBufferAttribute(bDir, 3),
        aColA = new THREE.InstancedBufferAttribute(bCol, 3), aSizeA = new THREE.InstancedBufferAttribute(bSize, 2), aMiscA = new THREE.InstancedBufferAttribute(bMisc, 4);
    [aPosA, aDirA, aColA, aSizeA, aMiscA].forEach(function (a) { a.setUsage(THREE.DynamicDrawUsage); });
    bg.setAttribute('aPos', aPosA); bg.setAttribute('aDir', aDirA); bg.setAttribute('aColor', aColA); bg.setAttribute('aSize', aSizeA); bg.setAttribute('aMisc', aMiscA);
    var bmat = new THREE.ShaderMaterial({
        vertexShader: BOLT_VS, fragmentShader: BOLT_FS, transparent: true, depthWrite: false, side: THREE.DoubleSide,
        blending: THREE.CustomBlending, blendEquation: THREE.AddEquation, blendSrc: THREE.OneFactor, blendDst: THREE.OneMinusSrcAlphaFactor,
        uniforms: { uTrue: { value: new THREE.Vector3() }, uTime: { value: 0 } }
    });
    var bolts = new THREE.InstancedMesh(bg, bmat, TOTB);
    bolts.frustumCulled = false; bolts.renderOrder = 18; bolts.count = 0;
    scene.add(bolts); track(bolts, bg, bmat); geoms.push(q);
    var bHigh = 0, bCursor = 0, trailN = 0;
    fx.bolts = bolts;
    var SH_ID = { bolt: 0, needle: 1, orb: 2, shard: 3 };
    var SH_L = [3.2, 6.5, 4.4, 1.9], SH_W = [1.2, 0.55, 2.0, 1.9], GH_A = [0.55, 0.32, 0.16];
    function ghostPos(id) {
        var g = (GH0 + id * 3) * 3, gap = bGap[id], dx = bDir[id * 3], dy = bDir[id * 3 + 1], dz = bDir[id * 3 + 2];
        for (var k = 0; k < 3; k++) {
            var m = gap * (k + 1);
            bPos[g + k * 3] = bPos[id * 3] - dx * m; bPos[g + k * 3 + 1] = bPos[id * 3 + 1] - dy * m; bPos[g + k * 3 + 2] = bPos[id * 3 + 2] - dz * m;
        }
    }
    fx.spawnBolt = function (pos, dir, color, speed, life, opts) {
        var id = -1;
        for (var n = 0; n < BOLT_CAP; n++) {
            var s = (bCursor + n) % BOLT_CAP;
            if (!bAlive[s]) { id = s; break; }
        }
        if (id < 0) return -1;
        bCursor = (id + 1) % BOLT_CAP;
        bAlive[id] = 1;
        var sid = 0, sz = 1, tr = false;
        if (opts) {
            var si = SH_ID[opts.shape]; if (si !== undefined) sid = si;
            if (opts.size > 0) sz = opts.size;
            tr = opts.trail !== undefined ? !!opts.trail : sid >= 2;   // orb / shard trail by default
        }
        bPos[id * 3] = pos.x; bPos[id * 3 + 1] = pos.y; bPos[id * 3 + 2] = pos.z;
        var dl = dir.length() || 1;
        bDir[id * 3] = dir.x / dl; bDir[id * 3 + 1] = dir.y / dl; bDir[id * 3 + 2] = dir.z / dl;
        _c.set(color === undefined ? 0xffffff : color);
        bCol[id * 3] = _c.r; bCol[id * 3 + 1] = _c.g; bCol[id * 3 + 2] = _c.b;
        bSize[id * 2] = SH_L[sid] * L * sz; bSize[id * 2 + 1] = SH_W[sid] * L * sz;
        var seed = Math.random() * 100, spin = sid === 3 ? (Math.random() < 0.5 ? -1 : 1) * (7 + Math.random() * 5) : 0;
        bMisc[id * 4] = sid; bMisc[id * 4 + 1] = spin; bMisc[id * 4 + 2] = 1; bMisc[id * 4 + 3] = seed;
        bSpd[id] = speed; bLife[id] = life;
        if (tr) {
            bTrail[id] = 1; trailN++; bGap[id] = (sid === 2 ? 1.5 : 1.1) * L * sz;
            for (var k = 0; k < 3; k++) {
                var g = GH0 + id * 3 + k, f = 1 - 0.17 * (k + 1);
                bDir[g * 3] = bDir[id * 3]; bDir[g * 3 + 1] = bDir[id * 3 + 1]; bDir[g * 3 + 2] = bDir[id * 3 + 2];
                bCol[g * 3] = _c.r; bCol[g * 3 + 1] = _c.g; bCol[g * 3 + 2] = _c.b;
                bSize[g * 2] = bSize[id * 2] * f; bSize[g * 2 + 1] = bSize[id * 2 + 1] * f;
                bMisc[g * 4] = sid; bMisc[g * 4 + 1] = spin; bMisc[g * 4 + 2] = GH_A[k]; bMisc[g * 4 + 3] = seed;
            }
            ghostPos(id);
        }
        if (id + 1 > bHigh) bHigh = id + 1;
        aPosA.needsUpdate = aDirA.needsUpdate = aColA.needsUpdate = aSizeA.needsUpdate = aMiscA.needsUpdate = true;
        return id;
    };
    fx.killBolt = function (id) {
        if (id < 0 || id >= BOLT_CAP || !bAlive[id]) return;
        bAlive[id] = 0; bSize[id * 2] = 0; aSizeA.needsUpdate = true;
        if (bTrail[id]) { bTrail[id] = 0; trailN--; for (var k = 0; k < 3; k++) bSize[(GH0 + id * 3 + k) * 2] = 0; }
        while (bHigh > 0 && !bAlive[bHigh - 1]) bHigh--;
    };
    fx.boltPos = function (id, out) {
        if (id < 0 || id >= BOLT_CAP || !bAlive[id]) return null;
        return out.set(bPos[id * 3], bPos[id * 3 + 1], bPos[id * 3 + 2]);
    };

    // sprites: ring / hex / smoke / arc, pooled ring buffer, animated by progress t in aMisc.z
    var sAge = new Float32Array(SPR_CAP).fill(-1), sDur = new Float32Array(SPR_CAP), sVel = new Float32Array(SPR_CAP * 3), sCur = 0, sprN = 0;
    function addSprite(shape, px, py, pz, dx, dy, dz, color, sx, sy, dur, vx, vy, vz) {
        var s = sCur; sCur = (sCur + 1) % SPR_CAP; if (sAge[s] < 0) sprN++;
        var id = SPR0 + s;
        _c.set(color === undefined ? 0xffffff : color);
        bPos[id * 3] = px; bPos[id * 3 + 1] = py; bPos[id * 3 + 2] = pz;
        bDir[id * 3] = dx; bDir[id * 3 + 1] = dy; bDir[id * 3 + 2] = dz;
        bCol[id * 3] = _c.r; bCol[id * 3 + 1] = _c.g; bCol[id * 3 + 2] = _c.b;
        bSize[id * 2] = sx; bSize[id * 2 + 1] = sy;
        bMisc[id * 4] = shape; bMisc[id * 4 + 1] = 0; bMisc[id * 4 + 2] = 0; bMisc[id * 4 + 3] = Math.random() * 100;
        sAge[s] = 0; sDur[s] = dur; sVel[s * 3] = vx; sVel[s * 3 + 1] = vy; sVel[s * 3 + 2] = vz;
        aPosA.needsUpdate = aDirA.needsUpdate = aColA.needsUpdate = aSizeA.needsUpdate = aMiscA.needsUpdate = true;
    }
    function updateSprites(dt) {
        if (!sprN) return;
        var drag = 1 - 2 * dt; if (drag < 0) drag = 0;
        for (var s = 0; s < SPR_CAP; s++) {
            if (sAge[s] < 0) continue;
            var id = SPR0 + s;
            sAge[s] += dt;
            if (sAge[s] >= sDur[s]) { sAge[s] = -1; bSize[id * 2] = 0; sprN--; continue; }
            bMisc[id * 4 + 2] = sAge[s] / sDur[s];
            sVel[s * 3] *= drag; sVel[s * 3 + 1] *= drag; sVel[s * 3 + 2] *= drag;
            bPos[id * 3] += sVel[s * 3] * dt; bPos[id * 3 + 1] += sVel[s * 3 + 1] * dt; bPos[id * 3 + 2] += sVel[s * 3 + 2] * dt;
        }
        aPosA.needsUpdate = aMiscA.needsUpdate = aSizeA.needsUpdate = true;
    }
    fx.updateBolts = function (dt) {
        for (var i = 0; i < bHigh; i++) {
            if (!bAlive[i]) continue;
            var s = bSpd[i] * dt;
            bPos[i * 3] += bDir[i * 3] * s; bPos[i * 3 + 1] += bDir[i * 3 + 1] * s; bPos[i * 3 + 2] += bDir[i * 3 + 2] * s;
            if (bTrail[i]) ghostPos(i);
            bLife[i] -= dt;
            if (bLife[i] <= 0) fx.killBolt(i);
        }
        updateSprites(dt);
        bolts.count = (trailN > 0 || sprN > 0) ? TOTB : bHigh; aPosA.needsUpdate = true;
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
    var xDur = new Float32Array(BURST_CAP).fill(0.4);
    var xN = new Uint8Array(BURST_CAP).fill(BURST_N), burstN = BURST_N, smokeN = 6;   // per-burst spark count / smoke puffs (quality)
    var FLASH_T = 0.08, BURST_T = 0.4;
    fx.hitStop = function () {};   // hook retained (ship.js may override / call)
    fx.flash = function (pos, color, size) {
        var f = fCur; fCur = (fCur + 1) % FLASH_CAP;
        fAge[f] = 0; _c.set(color === undefined ? 0xffffff : color);
        pO[f * 3] = pos.x; pO[f * 3 + 1] = pos.y; pO[f * 3 + 2] = pos.z;
        pV[f * 3] = pV[f * 3 + 1] = pV[f * 3 + 2] = 0;
        pC[f * 3] = _c.r; pC[f * 3 + 1] = _c.g; pC[f * 3 + 2] = _c.b;
        pS[f] = 2.2 * L * (size || 1); pT[f] = 0;
        aO.needsUpdate = aV.needsUpdate = aC.needsUpdate = aS.needsUpdate = aT.needsUpdate = true;
    };
    // spark burst: spd scales velocity, szm scales point size, dur seconds, ember = tint toward orange
    function burst(pos, color, size, spd, szm, dur, ember) {
        var b = xCur; xCur = (xCur + 1) % BURST_CAP;
        xAge[b] = 0; xDur[b] = dur; _c.set(color === undefined ? 0xffffff : color);
        if (ember) _c.lerp(_c2, 0.55);
        var base = FLASH_CAP + b * BURST_N;
        xN[b] = burstN;
        for (var i = 0; i < BURST_N; i++) {
            var p = base + i;
            var u = Math.random() * 2 - 1, th = Math.random() * 6.2832, sq = Math.sqrt(1 - u * u);
            var sp2 = (6 + Math.random() * 14) * L * size * spd;
            pO[p * 3] = pos.x; pO[p * 3 + 1] = pos.y; pO[p * 3 + 2] = pos.z;
            pV[p * 3] = sq * Math.cos(th) * sp2; pV[p * 3 + 1] = u * sp2; pV[p * 3 + 2] = sq * Math.sin(th) * sp2;
            pC[p * 3] = _c.r; pC[p * 3 + 1] = _c.g; pC[p * 3 + 2] = _c.b;
            pS[p] = (0.35 + Math.random() * 0.4) * L * size * szm; pT[p] = i < burstN ? 0 : -1;
        }
        aO.needsUpdate = aV.needsUpdate = aC.needsUpdate = aS.needsUpdate = aT.needsUpdate = true;
    }
    // impact(pos, color, size, kind, opt): kind 'hit' (default) | 'crit' | 'burn' | 'chain' (opt = target vec3 or {to}) | 'shieldHit' | 'explode'
    fx.impact = function (pos, color, size, kind, opt) {
        size = size || 1;
        if (kind === 'crit') {
            burst(pos, color, size * 1.7, 1.3, 1.1, BURST_T, false); burst(pos, 0xffffff, size * 1.0, 0.7, 0.8, BURST_T, false);
            addSprite(4, pos.x, pos.y, pos.z, 1, 0, 0, color, 7 * L * size, 7 * L * size, 0.3, 0, 0, 0);
            fx.flash(pos, color, 1.6 * size);
        } else if (kind === 'burn') {
            burst(pos, color, size * 0.8, 0.35, 0.65, 1.2, true);
        } else if (kind === 'chain') {
            var to = opt && (opt.to || opt);
            if (to && to.x !== undefined) {
                var dx = to.x - pos.x, dy = to.y - pos.y, dz = to.z - pos.z, dl = Math.sqrt(dx * dx + dy * dy + dz * dz) || 1;
                addSprite(7, (pos.x + to.x) * 0.5, (pos.y + to.y) * 0.5, (pos.z + to.z) * 0.5, dx / dl, dy / dl, dz / dl, color, dl, 1.8 * L * size, 0.18, 0, 0, 0);
                fx.flash(to, color, 0.7 * size);
            }
            fx.flash(pos, color, 0.7 * size);
            burst(pos, color, size * 0.6, 0.8, 0.8, 0.3, false);
        } else if (kind === 'shieldHit') {
            addSprite(5, pos.x, pos.y, pos.z, 1, 0, 0, color, 6 * L * size, 6 * L * size, 0.5, 0, 0, 0);
            fx.flash(pos, color, 0.6 * size);
        } else if (kind === 'explode') {
            fx.flash(pos, color, 3 * size);
            burst(pos, color, size * 1.7, 1.6, 1.3, 0.5, false); burst(pos, 0xffffff, size * 1.2, 0.8, 1.0, 0.5, false);
            addSprite(4, pos.x, pos.y, pos.z, 1, 0, 0, color, 11 * L * size, 11 * L * size, 0.4, 0, 0, 0);
            for (var i = 0; i < smokeN; i++) {
                var j = 1.6 * L * size, sp = 1.5 * L * size;
                addSprite(6, pos.x + (Math.random() - 0.5) * j, pos.y + (Math.random() - 0.5) * j, pos.z + (Math.random() - 0.5) * j, 1, 0, 0, color,
                    (3 + Math.random() * 1.6) * L * size, (3 + Math.random() * 1.6) * L * size, 0.9 + Math.random() * 0.6,
                    (Math.random() - 0.5) * sp * 2, (Math.random() - 0.3) * sp * 2, (Math.random() - 0.5) * sp * 2);
            }
        } else {
            burst(pos, color, size, 1, 1, BURST_T, false);
        }
    };
    // muzzle(pos, dir, color, shape): flash variant per bolt shape
    fx.muzzle = function (pos, dir, color, shape) {
        if (shape === 'needle') {
            fx.flash(pos, color, 0.6);
            var dl = dir.length() || 1;
            _v.set(pos.x + dir.x / dl * 2.6 * L, pos.y + dir.y / dl * 2.6 * L, pos.z + dir.z / dl * 2.6 * L);
            fx.spawnBolt(_v, dir, color, 0, 0.06, { shape: 'needle', size: 1.5, trail: false });
        } else if (shape === 'orb') {
            fx.flash(pos, color, 1.5);
            addSprite(4, pos.x, pos.y, pos.z, 1, 0, 0, color, 3.5 * L, 3.5 * L, 0.2, 0, 0, 0);
        } else if (shape === 'shard') {
            fx.flash(pos, color, 1);
            burst(pos, color, 0.4, 1, 0.8, 0.25, false);
        } else fx.flash(pos, color, 1);
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
        burstN = Q_SPARKS[t]; smokeN = Q_SMOKE[t];
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
        bmat.uniforms.uTrue.value.set(ce[12], ce[13], ce[14]); bmat.uniforms.uTime.value = time; pmat.uniforms.uTrue.value.set(ce[12], ce[13], ce[14]);   // floating origin: shader subtracts this, adds shifted cameraPosition
        var k = 1 - Math.exp(-dt * 6);
        mSpeed += (mSpeedTgt - mSpeed) * k;
        mPulse += (mPulseTgt - mPulse) * (1 - Math.exp(-dt * (mPulseTgt > mPulse ? 14 : 5)));
        var a = (mSpeed - 0.3) / 0.7; a = a < 0 ? 0 : a > 1 ? 1 : a;
        var atm = 1 - atmoD / 0.2; atm = atm < 0 ? 0 : atm; atm = atm * atm * (3 - 2 * atm);
        u.uAlpha.value = a * (0.55 + 0.45 * mPulse) * atm;
        u.uStretch.value = L * (0.15 + 0.85 * a * a * 1.2 + 11 * mPulse);
        streaks.visible = a * atm > 0.002;
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
            var done = xAge[i2] >= xDur[i2], t = xAge[i2] / xDur[i2], base = FLASH_CAP + i2 * BURST_N;
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

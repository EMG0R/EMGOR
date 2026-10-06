// ship-planet.js — fly-in planet surfaces for ship mode (docs/ship-mode.md, rev 12 A).
// The surface IS the orbital planet shader: the noise/height functions below are copied
// verbatim from galaxy3d.js PLANET_NOISE / PLANET_FRAG, so continents match from orbit.
// Nothing is allocated until the ship first comes within 3 R of a real body.
//
//   createPlanetSurface(engine, L) -> ps
//   ps.update(dt, shipPos)   per frame (world coords)
//   ps.active                node or null
//   ps.depth                 0 outside the atmosphere (1.4 R) -> 1 at the floor
//   ps.floorAt(pos, out)     {r, n}: surface radius (world units, from planet centre) along
//                            pos's direction + world surface normal; null if no planet active
//   ps.setVisible(bool)      hide/show every near-mode object (the orbital mesh is never hidden)
//   ps.shake                 0..1 rumble envelope of the 1.6-1.4 R rim pass (caller reads it)
//   ps.scar(pos, dir, len, ttl)  glowing line decal on the ground for ttl seconds (max 4 pooled)
//   ps.landable(pos)         {ok, r, n, slope, onPad}: ok = over land, slope < 0.25, inside 12 L altitude
//   ps.outposts              active planet's outposts: {id, name, pos (world), n, pad (Object3D), npcSpot:{pos,facing}, vel, radius}
//                            pos/n/npcSpot refresh at render time (onBeforeRender), vel = world velocity of the pad point
//   rev 14: everything that sits on the ground lives in ONE group ('ride') whose position/quaternion are copied from the
//   anchor/mesh at RENDER time (after updateBodies spun the globe), so the terrain can never lag the painted sphere.
//   ps.setQuality(t 0..3)    perf rev 17 (ship-perf.js): patch grid 64/96/128/128 cells, flora 150/300/500/500, creatures 10/20/40/40.
//                            Grid change swaps the patch geometry; flora/creatures re-seed on the next frame. New surfaces start at
//                            globalThis.EMGOR_PERF_TIER (set by the quality manager), default 3. ps.quality reads the tier back.
//                            The patch / flora / creatures are displaced or instanced in their shaders, so their geometry bounds are
//                            meaningless: they stay frustumCulled=false (hidden via .visible instead); the sky dome is a camera-attached
//                            sphere and likewise uncullable. There is no fog sampling loop (fog is one analytic exp per fragment).
//   rev 18 (local-frame flight, docs/ship-mode.md): ps.floorLocal(x,y,z) / ps.heightLocal(dx,dy,dz) / ps.landLocal(x,y,z) answer in the planet's OBJECT
//   space (pure functions of the local direction: no anchor, no spin, no patch state), ps.radius = R of the active body. ps.info() = {amp, look,
//   rocks, flora, grid, active}; ps.lookOf(node) = 'rocky' | 'lush' | 'icy' from the palette (lookFromPalette, exported). Surface colouring uses
//   slope + height per look (uLook); 300 instanced rocks (100/200/300/300 by quality tier); the orbital halo fades to nothing by 1.1 R.
//   rev 26 (Tier 3 #12/#13): creatures are alive. 4 bodies (0 walker, 1 jelly, 2 ox, 3 strider), 4 behaviours by seed: grazers (herds of 3-6, slow,
//   graze pauses, flee at 10 L), skitterers (fast darts, flee within 6 L), striders (lone, tall, flee 8 L), floaters (jellies; drift, steer toward the
//   lowest terrain = water, curious about the player at 6-30 L). ps.creatures() -> [{id, kind, beh, pos (world), tamed:false, planet}];
//   ps.setCreatureTamed(id, bool [, planetId]) hides/unhides (remembered per planet); ps.setFollower(id, worldPos) shows a tamed one at worldPos (call every
//   frame; hidden again 0.35 s after the last call); ps.glowCreature(id, secs) / ps.glowNear(worldPos, radius, secs) = brief scan glow.
//   Weather: per-planet seeded state machine on the wall clock (clear / wind / storm / aurora, 120-300 s slots, aurora only on cold palettes).
//   ps.weather = {state, t (s in state), wind, storm, aurora (0..1 live intensities)}; ps.wind = Vector3 (WORLD, world units/s the air moves; 0 outside
//   the atmosphere: ship.js adds it to flight / foot drift); ps.setWeather('clear'|'wind'|'storm'|'aurora'|null) forces a state (null = back to the schedule).
//   Visuals: flora sway amplitude, haze streaks (the entry streak pool), darker dome + fog + rain (one LineSegments, <= 1800), aurora ribbon (one mesh).
//   rev 27 (landmarks + night life): ps.landmarks() -> [{id, kind, pos (world), name}] (also in pois() as kind 'landmark'): 2-4 seeded per rocky planet on flat land away from outposts
//   (spire / arch / floating islands [exotic palettes only, slow bob] / geyser field [6 s steam cycle] / crashed hull / monolith ring), ONE instanced draw. Sky dome now draws the ring
//   (analytic plane hit, tilt/colour from node.ringMesh) and the planet's own moons (lit discs from the real child positions). Night: flora + rocks get bioluminescent spots (aBio, scaled
//   by uBio = night), ONE Points draw for fireflies (<= 400, drifting) + geyser steam, ONE haze mesh (0.5 L over the ground, clears radially around the player / exhaust). +3 draw calls.
//   ps.dispose()
//   ps.sampleHeight(dx,dy,dz,uniforms) / NOISE_GLSL / HEIGHT_GLSL exported for verification.

var NEAR_R = 3.0, NEAR_OFF = 3.25;       // enter / leave (x R)
var ATMO_R = 1.4;                         // atmosphere top (x R)
var HALO_A = 2.2, HALO_B = 1.4;           // atmo halo brightens/expands from HALO_A down to HALO_B (x R)
var RIM_A = 1.65, RIM_B = 1.35;           // rim pass (wind streaks + rumble) envelope (x R)
var NSTREAK = 60, NSCAR = 4, SCAR_PTS = 28;
var DOME_L = 600;                         // sky dome radius in ship lengths
var FADE_IN_A = 2.4, FADE_IN_B = 1.8;    // patch alpha 0 -> 1 across these (x R)
var REL_A = 2.4, REL_B = 1.3;             // rev 24: relief amplitude ramps 0 -> 1 across these (x R), so far silhouettes stay the globe's
var SURF_A = 2.0, SURF_B = 1.4;           // rev 24: surface colour grading (slope / biome / detail / patch lighting) blends in across these; at >= 2 R the patch IS the globe's shader
var GRID = 128;                           // patch grid cells per side
var AMP = 0.09;                           // terrain amplitude, fraction of R
var AMP_GAS = 0.03;                       // rev 20: cloud-world relief (gentler, landable anywhere)
var NSPORE = 60;                          // rev 20: floating spores over a gas giant's cloud tops
var PATCH_FOOT_L = 700;                  // rev 20b: on foot the patch only needs to reach past the (~45 L) horizon: 3.7x finer cells, so the rendered surface hugs the height function
var PATCH_MIN_L = 2600;                   // rev 20: the patch always reaches at least this far (L) from the ship: past the horizon at 0.8 L, into the fog
var BIAS = 0.0015;                        // minimum radius above the orbital sphere (no z-fight)
var GAS_DECK = 0.15;                      // rev 19: a gas giant has no surface: its 'floor' is a cloud deck at 1.15 R (the patch grows to it between 1.75 R and 1.45 R)
var FLORA_MAX = 700, CREAT_MAX = 40, ROCK_MAX = 400, SPORE_MAX = 60;
var Q_RAIN = [500, 900, 1400, 1800];
var FLORA_R = 1.3, CREAT_R = 1.2;         // x R
var NPAD = 3;                             // outposts per planet (max)
var PAD_L = 6, BLD_S = 0.55, PAD_FLAT = 15, PAD_BLEND = 22, NPC_OFF = 7.8, BLD_OFF = 11;   // x L
var LAND_ALT = 12, LAND_SLOPE = 0.25, OUT_SEP = 0.4;

// ─── GLSL: noise copied verbatim from galaxy3d PLANET_NOISE ───────────────────
export var NOISE_GLSL = [
    'float nhash(vec3 p) {',
    '    p = fract(p * 0.3183099 + 0.1);',
    '    p *= 17.0;',
    '    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));',
    '}',
    'float vnoise(vec3 x) {',
    '    vec3 i = floor(x);',
    '    vec3 f = fract(x);',
    '    f = f * f * (3.0 - 2.0 * f);',
    '    return mix(mix(mix(nhash(i + vec3(0.0, 0.0, 0.0)), nhash(i + vec3(1.0, 0.0, 0.0)), f.x),',
    '                   mix(nhash(i + vec3(0.0, 1.0, 0.0)), nhash(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),',
    '               mix(mix(nhash(i + vec3(0.0, 0.0, 1.0)), nhash(i + vec3(1.0, 0.0, 1.0)), f.x),',
    '                   mix(nhash(i + vec3(0.0, 1.0, 1.0)), nhash(i + vec3(1.0, 1.0, 1.0)), f.x), f.y), f.z);',
    '}',
    'float fbm(vec3 p) {',
    '    float v = 0.0;',
    '    float a = 0.5;',
    '    for (int i = 0; i < 4; i++) {',
    '        v += a * vnoise(p);',
    '        p = p * 2.07 + vec3(11.3, 7.9, 5.1);',
    '        a *= 0.5;',
    '    }',
    '    return v;',
    '}'
].join('\n');

// terra height as a fraction of R above the sphere (d = unit direction, object space).
// The n expression is the exact one from PLANET_FRAG's terra branch.
export var HEIGHT_GLSL = [
    'uniform vec3 uSeed; uniform float uFreq; uniform float uWarp; uniform float uSeaLevel;',
    'uniform float uAmp; uniform float uBias; uniform float uEps; uniform float uSeaH;',
    'float terraN(vec3 sp) {',
    '    vec3 p = sp * uFreq + uSeed;',
    '    vec3 q = vec3(fbm(p + vec3(0.0, 3.1, 1.7)),',
    '                  fbm(p + vec3(5.2, 1.3, 2.8)),',
    '                  fbm(p + vec3(1.7, 9.2, 4.6)));',
    '    float n = fbm(p + (q - 0.5) * uWarp);',
    '    n += (fbm(p * 3.7 + q * 2.0) - 0.5) * 0.22;',
    '    return n;',
    '}',
    'float hfun(vec3 d) {',
    '    float n = terraN(d);',
    '    float land = smoothstep(uSeaLevel - 0.035, uSeaLevel + 0.035, n);',
    '    float rel = max(n - uSeaH, 0.0);',   // rev 24: relief is measured from the planet's ORIGINAL sea level, so lowering the water (land balance) adds plains, not 24 %-of-R mountains
    '    vec3 pm = vec3(d * (uFreq * 5.3) + (uSeed * 1.7 + 2.3));',   // second octave: ridged crags on the mountains
    '    float rid = 1.0 - abs(2.0 * vnoise(pm) - 1.0);',
    '    return uBias + uAmp * (0.10 * land + rel * (2.2 + 1.6 * rid * smoothstep(0.02, 0.14, rel)));',
    '}'
].join('\n');

var PATCH_VERT = [
    'uniform float uR; uniform vec3 uC; uniform vec3 uT1; uniform vec3 uT2; uniform float uTan; uniform float uRel; uniform float uTime;',
    'varying vec3 vDir; varying vec3 vN; varying vec3 vView; varying float vRim; varying float vH; varying vec3 vUp;',
    '#include <common>',
    '#include <logdepthbuf_pars_vertex>',
    NOISE_GLSL,
    HEIGHT_GLSL,
    'uniform vec3 uPadD[3]; uniform vec3 uPadP[3];',       // outpost pads: local dir, (flat chord, blend chord, height frac)
    'float hfunP(vec3 d) {',
    '    float h = hfun(d);',
    '    for (int i = 0; i < 3; i++) {',
    '        if (uPadP[i].y > 0.0) h = mix(h, uPadP[i].z, 1.0 - smoothstep(uPadP[i].x, uPadP[i].y, length(d - uPadD[i])));',
    '    }',
    '    return uBias + (h - uBias) * uRel;',
    '}',
    'vec3 dirAt(vec2 s) {',
    '    vec2 u = s * (0.04 + 0.96 * abs(s * s * s));',     // rev 20: very dense under the ship (cell ~1.5 L at 0.8 L altitude), coarse at the rim
    '    return normalize(uC + (uT1 * u.x + uT2 * u.y) * uTan);',
    '}',
    'void main() {',
    '    vec2 s = position.xy;',
    '    vec3 d = dirAt(s);',
    '    float rim = max(abs(s.x), abs(s.y));',
    '    float taper = 1.0 - smoothstep(0.8, 1.0, rim);',  // relief fades to the sphere at the rim
    '    float h = hfunP(d);',
    // rev 25: sea surface waves (vertex): two long swells, 0 .. -0.002 R below the flat floor (never above it, so the hover/collision floor stays dry)
    '    float seaV = 1.0 - smoothstep(0.3, 0.55, (h - uBias) / (uAmp * 0.1 + 1e-5));',
    '    float wv = 0.5 + 0.25 * sin(dot(d, vec3(0.9, 0.2, 0.4)) * 190.0 + uTime * 0.6) + 0.25 * sin(dot(d, vec3(-0.3, 0.8, 0.5)) * 330.0 - uTime * 0.85);',
    '    h -= 0.002 * (1.0 - wv) * seaV * uRel;',
    '    h = mix(uBias, h, taper) - 0.02 * smoothstep(0.97, 1.0, rim);',   // rev 19: skirt: the outermost ring dips below the orbital sphere so no sliver of space shows between patch and globe
    '    vH = h;',
    '    vec3 ref = abs(d.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);',
    '    vec3 e1 = normalize(cross(d, ref)); vec3 e2 = cross(d, e1);',
    '    float cell = uTan * (0.04 + 3.84 * max(abs(s.x), abs(s.y)) * max(abs(s.x), abs(s.y)) * max(abs(s.x), abs(s.y))) / 56.0;',   // rev 25: angular size of a patch cell here; the normal probe never goes finer than ~1.3 cells (finer = aliased, streaky slope shading at altitude)
    '    float eps = max(uEps, cell * 1.3);',
    '    vec3 db = normalize(d + e1 * eps); vec3 dc = normalize(d + e2 * eps);',
    '    float hb = mix(uBias, hfunP(db), taper) - 0.02 * smoothstep(0.97, 1.0, rim); float hc = mix(uBias, hfunP(dc), taper) - 0.02 * smoothstep(0.97, 1.0, rim);',
    '    vec3 pa = d * (1.0 + h); vec3 pb = db * (1.0 + hb); vec3 pc = dc * (1.0 + hc);',
    '    vec3 nl = normalize(cross(pb - pa, pc - pa));',
    '    if (dot(nl, d) < 0.0) nl = -nl;',
    '    vDir = d; vRim = rim; vUp = normalize(mat3(modelMatrix) * d);',
    '    vN = normalize(mat3(modelMatrix) * nl);',
    '    vec4 wp = modelMatrix * vec4(d * (uR * (1.0 + h)), 1.0);',
    '    vView = cameraPosition - wp.xyz;',
    '    gl_Position = projectionMatrix * viewMatrix * wp;',
    '    #include <logdepthbuf_vertex>',
    '}'
].join('\n');

// colour logic: PLANET_FRAG verbatim (terra / gas / speckle), then slope shading + fog
var PATCH_FRAG = [
    'uniform vec3 uColHigh; uniform vec3 uColLow; uniform vec3 uColSea; uniform vec3 uAtmo; uniform vec3 uAccent;',
    'uniform float uBiome; uniform float uBandFreq; uniform float uSpeckle; uniform vec3 uLightDir;',
    'uniform vec3 uSeed; uniform float uFreq; uniform float uWarp; uniform float uSeaLevel;',
    'uniform float uFade; uniform float uSurf; uniform vec3 uFogCol; uniform float uFogK; uniform float uLook; uniform float uDetD; uniform float uDetK; uniform float uTime;',
    'varying vec3 vDir; varying vec3 vN; varying vec3 vView; varying float vRim; varying float vH; varying vec3 vUp;',
    '#include <common>',
    '#include <logdepthbuf_pars_fragment>',
    NOISE_GLSL,
    'void main() {',
    '    #include <logdepthbuf_fragment>',
    '    vec3 sp = normalize(vDir);',
    '    vec3 p = sp * uFreq + uSeed;',
    '    vec3 q = vec3(fbm(p + vec3(0.0, 3.1, 1.7)),',
    '                  fbm(p + vec3(5.2, 1.3, 2.8)),',
    '                  fbm(p + vec3(1.7, 9.2, 4.6)));',
    '    vec3 col; float seaM = 0.0; float landM = 1.0; float nn = 0.5; float wM = 0.0;',
    '    if (uBiome < 0.5) {',
    '        float n = fbm(p + (q - 0.5) * uWarp);',
    '        n += (fbm(p * 3.7 + q * 2.0) - 0.5) * 0.22;',
    '        float land = smoothstep(uSeaLevel - 0.035, uSeaLevel + 0.035, n);',
    '        float high = smoothstep(uSeaLevel + 0.12, uSeaLevel + 0.2, n);',
    '        seaM = 1.0 - land; landM = land; nn = n; wM = 1.0 - smoothstep(0.38, 0.5, land);',
    '        col = mix(uColSea, uColLow, land);',
    '        col = mix(col, uColHigh, high);',
    '        col += (fbm(p * 6.3) - 0.5) * 0.07;',
    '    } else {',
    '        float lat = sp.y + (q.x - 0.5) * uWarp * 0.45 + (fbm(p * 2.3) - 0.5) * 0.3;',
    '        float s = sin(lat * uBandFreq + q.y * 2.4);',
    '        float band = smoothstep(-0.55, 0.55, s);',
    '        float storm = smoothstep(0.58, 0.8, fbm(p * 2.6 + q));',
    '        col = mix(uColLow, uColHigh, band);',
    '        col = mix(col, uColSea, storm * 0.65);',
    '        col += (fbm(p * 5.1) - 0.5) * 0.05;',
    '    }',
    '    if (uSpeckle > 0.0) {',
    '        float spk = step(1.0 - uSpeckle * 0.012, vnoise(p * 26.0));',
    '        col += uAccent * spk * 1.7;',
    '    }',
    // rev 24: the GLOBE's own lighting + fresnel on the sphere normal (verbatim PLANET_FRAG), so at >= 2 R the patch is pixel-equal to the painted sphere
    '    vec3 V = normalize(vView);',
    '    float dist = length(vView);',
    '    vec3 Ng = normalize(vUp);',
    '    float ndlG = dot(Ng, uLightDir) * 0.5 + 0.5;',
    '    vec3 gcol = col * mix(0.24, 0.96, smoothstep(0.08, 0.92, ndlG));',
    '    gcol += uAtmo * pow(1.0 - clamp(dot(Ng, V), 0.0, 1.0), 3.6) * 0.3;',
    '    if (uSurf > 0.001) {',
    '    vec3 N = normalize(vN);',
    // rev 18 biome looks (uLook 0 rocky / 1 lush / 2 icy), driven by slope + height on top of the palette; terra only
    '    float slope = 1.0 - clamp(dot(N, normalize(vUp)), 0.0, 1.0);',
    '    float hh = smoothstep(0.03, 0.2, vH);',
    '    float steep = smoothstep(0.035, 0.2, slope);',
    '    float tn = fbm(sp * uFreq * 24.0 + uSeed);',
    '    if (uBiome < 0.5) {',
    '        vec3 rockC = mix(uColLow, uColHigh, 0.4) * vec3(0.42, 0.38, 0.4) + 0.045;',
    '        if (uLook < 0.5) {',                                                   // rocky: dusty plains, banded dark cliffs, pale summits
    '            col = mix(col, col * (0.82 + 0.4 * tn) + 0.02, 0.6);',
    '            col = mix(col, rockC * (0.8 + 0.6 * tn), steep);',
    '            col = mix(col, col * 1.28 + 0.05, hh * (1.0 - steep) * 0.7);',
    '        } else if (uLook < 1.5) {',                                            // lush: saturated lowlands, grey cliffs, snow on the peaks
    '            col = mix(col, uColLow * (1.05 + 0.35 * tn) + 0.015, (1.0 - hh) * 0.6);',
    '            col = mix(col, vec3(0.34, 0.33, 0.35) * (0.8 + 0.5 * tn), steep * 0.85);',
    '            col = mix(col, vec3(0.92, 0.95, 1.0), smoothstep(0.62, 0.9, hh) * (1.0 - steep * 0.8) * 0.85);',
    '        } else {',                                                             // icy: pale snow plains, deep blue crevasse walls, glints
    '            vec3 snow = mix(vec3(0.78, 0.88, 1.0), vec3(0.95, 0.98, 1.0), tn);',
    '            col = mix(col, snow * (0.8 + 0.2 * hh), (1.0 - steep) * 0.62);',
    '            col = mix(col, uAtmo * 0.28 + rockC * 0.35, steep * 0.9);',
    '        }',
    '    }',
    // rev 19: height bands (lowland -> highland hue shift, pale cap above 70 % of the amplitude), 2-octave ground detail, stronger slope + sun shading
    '    if (uBiome < 0.5) {',
    '        vec3 bandC = mix(uColLow, uColHigh, smoothstep(0.02, 0.17, vH));',
    '        col = mix(col, bandC * (0.55 + 0.9 * dot(col, vec3(0.33))) , 0.28 * (1.0 - seaM));',
    '        float cap = smoothstep(0.7, 0.92, hh) * (1.0 - steep * 0.75);',
    '        col = mix(col, mix(uColHigh, vec3(0.93, 0.93, 0.95), 0.62) * 0.95 + 0.04, cap * 0.75 * (1.0 - seaM));',
    '        float dk = 1.0 - smoothstep(uDetD * 0.4, uDetD, dist);',
    '        float dn = vnoise(sp * uDetK / 1.6) * 0.6 + vnoise(sp * uDetK / 0.55) * 0.4;',
    '        col *= 1.0 + (dn - 0.5) * 0.55 * dk * (1.0 - seaM * 0.8);',
    '    }',
    '    if (uBiome > 0.5) {',                                                   // rev 20: cloud world. local latitude stripes (palette bands) + domain-warped swirls + puffs, all in L
    '        vec3 gp = sp * uDetK;',
    '        vec3 w = vec3(fbm(gp / 260.0 + uSeed), fbm(gp / 260.0 + uSeed + 7.3), fbm(gp / 260.0 + uSeed + 3.1));',
    '        float sb = sin(sp.y * uDetK / 38.0 + (w.x - 0.5) * 7.0 + (w.y - 0.5) * 3.0);',
    '        col = mix(col, mix(uColLow, uColHigh, smoothstep(-0.6, 0.6, sb)), 0.7);',
    '        float sw = fbm(gp / 90.0 + (w - 0.5) * 2.4 + vec3(uTime * 0.012));',
    '        col = mix(col * 0.62, col * 1.25 + uAccent * 0.06, smoothstep(0.3, 0.7, sw));',
    '        float dk2 = 1.0 - smoothstep(uDetD * 0.5, uDetD * 1.4, dist);',
    '        float pf = vnoise(gp / 16.0 + (w - 0.5) * 5.0 + vec3(uTime * 0.03));',
    '        col *= 1.0 + (pf - 0.5) * 0.36 * dk2;',
    '        col += uColHigh * smoothstep(0.62, 0.9, sw) * 0.08;',
    '    }',
    // rev 25: water. depth gradient (shallow teal near the coast -> darker deep tone), two moving wave-normal layers, coast foam
    '    vec3 Nw = N; float foam = 0.0; float wAtt = 1.0 - smoothstep(uDetD, uDetD * 5.0, dist);',
    '    if (uBiome < 0.5 && wM > 0.001) {',
    '        float dep = smoothstep(0.0, 1.0, clamp((uSeaLevel - nn) / 0.06, 0.0, 1.0));',
    '        vec3 shallow = uColSea * 1.7 + vec3(0.0, 0.03, 0.03);',
    '        vec3 deepC = uColSea * 0.38;',
    '        col = mix(col, mix(shallow, deepC, dep), wM);',
    '        vec3 gp2 = sp * uDetK;',
    '        vec3 pw = vec3(sin(dot(gp2, vec3(0.9, 0.2, 0.4)) * 0.9 + uTime * 1.3) + sin(dot(gp2, vec3(0.2, -0.7, 0.6)) * 2.3 - uTime * 1.9),',
    '                       sin(dot(gp2, vec3(-0.3, 0.8, 0.5)) * 1.4 - uTime * 1.6) + sin(dot(gp2, vec3(0.7, 0.4, -0.5)) * 3.1 + uTime * 2.4),',
    '                       sin(dot(gp2, vec3(0.5, -0.6, 0.7)) * 2.0 + uTime * 1.1) + sin(dot(gp2, vec3(-0.6, 0.1, 0.8)) * 3.7 - uTime * 2.1));',
    '        pw = pw - N * dot(pw, N);',
    '        Nw = normalize(N + pw * 0.11 * wM * wAtt);',
    '        float cb = smoothstep(0.36, 0.46, landM) * (1.0 - smoothstep(0.5, 0.56, landM));',
    '        float rip = 0.5 + 0.5 * sin(nn * 340.0 - uTime * 1.6 + vnoise(gp2 * 0.4) * 7.0);',
    '        foam = cb * (0.5 + 0.5 * rip) * (1.0 - smoothstep(uDetD * 4.0, uDetD * 14.0, dist));',
    '    }',
    '    float ndl = dot(N, uLightDir) * 0.5 + 0.5;',
    '    float lightAmt = mix(0.14, 0.98, smoothstep(0.2, 0.88, ndl));',
    '    col *= lightAmt;',
    '    float fr = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 3.6);',
    '    col += uAtmo * fr * 0.18;',
    '    if (uBiome < 0.5) {',
    '        vec3 Hh = normalize(uLightDir + V);',
    '        float sp2 = pow(max(dot(Nw, Hh), 0.0), 140.0) * smoothstep(0.3, 0.6, ndl);',
    '        float sp3 = pow(max(dot(Nw, Hh), 0.0), 18.0) * smoothstep(0.3, 0.6, ndl);',
    '        col += wM * (vec3(1.0, 0.97, 0.9) * (sp2 * 1.3 + sp3 * 0.12) + uAtmo * pow(1.0 - clamp(dot(Nw, V), 0.0, 1.0), 3.5) * 0.24 * (0.3 + 0.7 * ndl));',
    '        col = mix(col, vec3(0.95, 0.98, 1.0) * (0.35 + 0.65 * ndl), foam * 0.72);',
    '    }',
    '    if (uBiome < 0.5 && uLook > 1.5) col += vec3(0.8, 0.9, 1.0) * pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 6.0) * 0.25 * (1.0 - steep);',
    '    col = mix(col, col * vec3(0.55, 0.52, 0.58), smoothstep(0.04, 0.2, slope) * step(uBiome, 0.5) * 0.75);',
    '    gcol = mix(gcol, col, uSurf);',
    '    }',
    '    col = gcol;',
    '    float f = 1.0 - exp(-pow(dist * uFogK, 2.0));',
    '    f = min(f, 0.93);',
    '    f *= 1.0 - 0.45 * wM * uSurf;',      // rev 25: water keeps its own (darker) tone at distance, so the horizon line against the sky survives the haze
    '    col = mix(col, uFogCol, f);',
    '    float alpha = uFade * (1.0 - smoothstep(0.8, 1.0, vRim));',
    '    gl_FragColor = vec4(col, alpha);',
    '    #include <tonemapping_fragment>',
    '    #include <colorspace_fragment>',
    '}'
].join('\n');

// instanced flora / creatures share one fragment: flat-lit palette colour + fog
var INST_FRAG = [
    'uniform vec3 uLightDir; uniform vec3 uFogCol; uniform float uFogK;',
    'varying vec3 vCol; varying vec3 vN; varying vec3 vView;',
    '#include <common>',
    '#include <logdepthbuf_pars_fragment>',
    'void main() {',
    '    #include <logdepthbuf_fragment>',
    '    vec3 N = normalize(vN);',
    '    if (!gl_FrontFacing) N = -N;',
    '    float ndl = dot(N, uLightDir) * 0.5 + 0.5;',
    '    float lightAmt = mix(0.3, 1.0, smoothstep(0.08, 0.92, ndl));',
    '    vec3 col = vCol * lightAmt;',
    '    float dist = length(vView);',
    '    float f = min(1.0 - exp(-pow(dist * uFogK, 2.0)), 0.93);',
    '    col = mix(col, uFogCol, f);',
    '    gl_FragColor = vec4(col, 1.0);',
    '    #include <tonemapping_fragment>',
    '    #include <colorspace_fragment>',
    '}'
].join('\n');

var FLORA_FRAG = INST_FRAG.replace('varying vec3 vCol;', 'uniform float uGlow; uniform float uBioN; varying vec3 vBio; varying vec3 vLP; varying float vTop; varying vec3 vCol;')
    .replace('vec3 col = vCol * lightAmt;', 'vec3 col = vCol * lightAmt; col += vCol * uGlow * smoothstep(0.8, 0.95, vTop) * 1.8; float bsp = smoothstep(0.5, 0.72, sin(vLP.x * 13.0 + vLP.y * 9.0) * sin(vLP.z * 11.0 - vLP.y * 7.0) * 0.5 + 0.5); float bk = max(vBio.r, max(vBio.g, vBio.b)) * uBioN * (0.3 + 0.7 * bsp); col = mix(col, vBio * (1.2 + 0.5 * bsp), min(bk, 0.92));');
var FLORA_VERT = [
    'attribute float aVar; attribute float aPart; attribute float aSel; attribute vec3 aCol; attribute vec3 aBio;',
    'uniform float uFadeA; uniform float uFadeB; uniform float uNearA; uniform float uNearB;',
    'varying vec3 vCol; varying vec3 vN; varying vec3 vView; varying float vTop; varying vec3 vBio; varying vec3 vLP;',
    '#include <common>',
    '#include <logdepthbuf_pars_vertex>',
    'void main() {',
    '    vec3 pos = position; vTop = position.y; vBio = aBio; vLP = position;',
    '    vec4 iw = modelMatrix * vec4(instanceMatrix[3].xyz, 1.0);',
    '    float dd = distance(cameraPosition, iw.xyz);',
    '    float k = 1.0 - smoothstep(uFadeA, uFadeB, dd);',
    '    k *= smoothstep(uNearA, uNearB, dd / max(length(instanceMatrix[0].xyz), 1e-9));',   // rev 18: an instance the camera is inside (or about to be) shrinks away
    '    if (abs(aVar - aSel) > 0.5) k = 0.0;',
    '    pos *= k;',
    '    mat4 M = modelMatrix * instanceMatrix;',
    '    vec4 wp = M * vec4(pos, 1.0);',
    '    vN = normalize(mat3(M) * normal);',
    '    vCol = aPart < 0.5 ? aCol * 0.32 : aCol;',
    '    vView = cameraPosition - wp.xyz;',
    '    gl_Position = projectionMatrix * viewMatrix * wp;',
    '    #include <logdepthbuf_vertex>',
    '}'
].join('\n');

var FLORA_VERT_SW = FLORA_VERT.replace('uniform float uFadeA;', 'uniform float uTime; uniform float uSway; uniform float uFadeA;')
    .replace('    pos *= k;', '    float swp = uTime * 1.7 + iw.x * 0.07 + iw.z * 0.05;\n    pos.x += (sin(swp) + 0.5 * sin(swp * 2.3 + 1.3)) * uSway * 0.2 * position.y * position.y;\n    pos.z += cos(swp * 0.8) * uSway * 0.12 * position.y * position.y;\n    pos *= k;');

var CREAT_VERT = [
    'attribute float aVar; attribute float aW; attribute float aPh; attribute float aSel; attribute vec3 aCol; attribute float aPartC;',
    'attribute float aInstPh; attribute float aGlow; attribute float aMov;',
    'uniform float uTime; uniform float uFadeA; uniform float uFadeB;',
    'varying vec3 vCol; varying vec3 vN; varying vec3 vView;',
    '#include <common>',
    '#include <logdepthbuf_pars_vertex>',
    'void main() {',
    '    vec3 pos = position;',
    '    float ph = aInstPh + aPh;',
    '    if (aVar < 0.5) {',                                               // walker: legs lift in a gait (amplitude follows the actual speed)
    '        pos.y += max(0.0, sin(uTime * 7.0 + ph)) * 0.22 * aW * aMov;',
    '        pos.x += sin(uTime * 7.0 + ph) * 0.06 * aW * aMov;',
    '    } else if (aVar < 1.5) {',                                        // jelly: bell pulse + tentacle sway
    '        float pulse = 1.0 + 0.14 * sin(uTime * 3.0 + aInstPh);',
    '        pos.xz *= mix(pulse, 1.0, aW);',
    '        pos.x += sin(uTime * 4.0 + ph + pos.y * 3.0) * 0.14 * aW;',
    '        pos.z += cos(uTime * 3.3 + ph) * 0.1 * aW;',
    '    } else {',                                                        // ox (slow plod) / strider (long stride)
    '        float sp = aVar < 2.5 ? 4.5 : 3.2;',
    '        pos.y += max(0.0, sin(uTime * sp + ph)) * (aVar < 2.5 ? 0.16 : 0.3) * aW * aMov;',
    '        pos.x += sin(uTime * sp + ph) * (aVar < 2.5 ? 0.06 : 0.1) * aW * aMov;',
    '    }',
    '    vec4 iw = modelMatrix * vec4(instanceMatrix[3].xyz, 1.0);',
    '    float dd = distance(cameraPosition, iw.xyz);',
    '    float k = 1.0 - smoothstep(uFadeA, uFadeB, dd);',
    '    if (abs(aVar - aSel) > 0.5) k = 0.0;',
    '    pos *= k;',
    '    mat4 M = modelMatrix * instanceMatrix;',
    '    vec4 wp = M * vec4(pos, 1.0);',
    '    vN = normalize(mat3(M) * normal);',
    '    vCol = aPartC > 0.5 ? aCol * 1.35 : aCol;',                       // eyes / glow spots brighter
    '    vCol = vCol * (1.0 + aGlow * 0.7) + aGlow * vec3(1.0, 0.85, 0.5);',   // scan glow
    '    vView = cameraPosition - wp.xyz;',
    '    gl_Position = projectionMatrix * viewMatrix * wp;',
    '    #include <logdepthbuf_vertex>',
    '}'
].join('\n');

var STREAK_VERT = [
    'attribute vec3 aOth; attribute float aEnd; attribute float aSide;',
    'uniform float uW; varying float vE;',
    'void main() {',
    '    vec3 dir = normalize(position - aOth);',
    '    vec3 perp = normalize(cross(dir, position));',          // position = camera-relative view vector
    '    vE = aEnd;',
    '    vec4 wp = modelMatrix * vec4(position + perp * aSide * uW * (0.35 + 0.65 * aEnd), 1.0);',
    '    gl_Position = projectionMatrix * viewMatrix * wp;',
    '}'
].join('\n');
var STREAK_FRAG = [
    'uniform vec3 uCol; uniform float uAlpha; varying float vE;',
    'void main() { float a = vE * vE * uAlpha; gl_FragColor = vec4(uCol * a, 1.0); }'
].join('\n');


// rev 26 weather: rain (one LineSegments, camera-following box, all motion in the vertex shader) + aurora ribbon (one mesh, sky curtain over the pole)
var RAIN_VERT = [
    'attribute vec3 aS; attribute float aEnd;',
    'uniform float uTime; uniform vec3 uUp; uniform vec3 uRt; uniform vec3 uFw; uniform vec3 uWind; uniform float uW; uniform float uH; uniform float uFall; uniform float uLen;',
    'varying float vA;',
    'void main() {',
    '    float f = 0.85 + 0.3 * fract(aS.x * 7.13);',
    '    float y = fract(aS.y - uTime * uFall * f / uH);',
    '    vec3 vel = -uUp * uFall * f + uWind * 2.0;',
    '    vec3 p = uRt * (aS.x - 0.5) * uW + uFw * (aS.z - 0.5) * uW + uUp * (y - 0.5) * uH;',
    '    p -= vel * (uLen / uFall) * aEnd;',
    '    vA = (1.0 - aEnd * 0.8) * smoothstep(0.0, 0.08, y) * (1.0 - smoothstep(0.85, 1.0, y));',
    '    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(p, 1.0);',
    '}'
].join('\n');
var RAIN_FRAG = [
    'uniform vec3 uCol; uniform float uAlpha; varying float vA;',
    'void main() { gl_FragColor = vec4(uCol * vA * uAlpha, 1.0); }'
].join('\n');
var AUR_VERT = [
    'uniform vec3 uUp; uniform vec3 uRt; uniform vec3 uFw; uniform float uTime; uniform float uAz; uniform float uR;',
    'varying vec2 vUv;',
    '#include <common>',
    '#include <logdepthbuf_pars_vertex>',
    'void main() {',
    '    float u = uv.x, v = uv.y;',
    '    float ang = uAz + (u - 0.5) * 2.7 + sin(u * 6.0 + uTime * 0.21) * 0.10;',
    '    float el = mix(0.14, 0.78, v * 0.6 + v * v * 0.4) + sin(u * 9.0 - uTime * 0.35) * 0.04 * (0.4 + v);',
    '    vec3 dir = cos(el) * (sin(ang) * uRt + cos(ang) * uFw) + sin(el) * uUp;',
    '    vec4 wp = modelMatrix * vec4(dir * uR, 1.0);',
    '    vUv = uv;',
    '    gl_Position = projectionMatrix * viewMatrix * wp;',
    '    #include <logdepthbuf_vertex>',
    '}'
].join('\n');
var AUR_FRAG = [
    'uniform float uI; uniform float uTime; uniform vec3 uColA; uniform vec3 uColB; varying vec2 vUv;',
    '#include <common>',
    '#include <logdepthbuf_pars_fragment>',
    'void main() {',
    '    #include <logdepthbuf_fragment>',
    '    float u = vUv.x, v = vUv.y;',
    '    float ray = pow(0.5 + 0.5 * sin(u * 150.0 + sin(uTime * 0.4 + u * 8.0) * 3.0 + uTime * 0.6), 1.6);',
    '    float band = 0.5 + 0.5 * sin(u * 11.0 - uTime * 0.22 + v * 2.0);',
    '    float a = smoothstep(0.0, 0.12, v) * pow(1.0 - v, 0.9) * smoothstep(0.0, 0.07, u) * smoothstep(0.0, 0.07, 1.0 - u);',
    '    a *= (0.25 + 0.75 * ray) * (0.55 + 0.45 * band);',
    '    vec3 col = mix(uColA, uColB, smoothstep(0.1, 0.95, v));',
    '    gl_FragColor = vec4(col * a * uI * 1.6, 1.0);',
    '}'
].join('\n');

// outposts: pads + buildings share one vertex shader (instance colour x per-vertex shade; shade >= 1.5 = lit window / lamp)
var OUT_VERT = [
    'attribute float aPart; attribute vec3 aCol;',
    'varying vec3 vCol; varying vec3 vN; varying vec3 vView; varying float vGlow;',
    '#include <common>',
    '#include <logdepthbuf_pars_vertex>',
    'void main() {',
    '    mat4 M = modelMatrix * instanceMatrix;',
    '    vec4 wp = M * vec4(position, 1.0);',
    '    vN = normalize(mat3(M) * normal);',
    '    vCol = aCol * aPart; vGlow = step(1.5, aPart);',
    '    vView = cameraPosition - wp.xyz;',
    '    gl_Position = projectionMatrix * viewMatrix * wp;',
    '    #include <logdepthbuf_vertex>',
    '}'
].join('\n');
var OUT_FRAG = [
    'uniform vec3 uLightDir; uniform vec3 uFogCol; uniform float uFogK;',
    'varying vec3 vCol; varying vec3 vN; varying vec3 vView; varying float vGlow;',
    '#include <common>',
    '#include <logdepthbuf_pars_fragment>',
    'void main() {',
    '    #include <logdepthbuf_fragment>',
    '    vec3 N = normalize(vN);',
    '    if (!gl_FrontFacing) N = -N;',
    '    float ndl = dot(N, uLightDir) * 0.5 + 0.5;',
    '    float lightAmt = mix(0.3, 1.0, smoothstep(0.08, 0.92, ndl));',
    '    vec3 col = mix(vCol * lightAmt, vCol, vGlow);',
    '    float f = min(1.0 - exp(-pow(length(vView) * uFogK, 2.0)), 0.93);',
    '    col = mix(col, uFogCol, f * (1.0 - vGlow * 0.6));',
    '    gl_FragColor = vec4(col, 1.0);',
    '    #include <tonemapping_fragment>',
    '    #include <colorspace_fragment>',
    '}'
].join('\n');
// beacon: instanced camera-facing glow, constant-ish angular size so it reads from 3 R, blinks, depth-tested by the globe
var BEAC_VERT = [
    'attribute float aPh;',
    'uniform float uTime; uniform float uR; uniform float uMin;',
    'varying vec2 vUv; varying float vBl;',
    '#include <common>',
    '#include <logdepthbuf_pars_vertex>',
    'void main() {',
    '    vec4 c = modelMatrix * instanceMatrix * vec4(0.0, 0.0, 0.0, 1.0);',
    '    float dist = distance(cameraPosition, c.xyz);',
    '    float size = max(uMin, dist * 0.014);',
    '    vec3 cr = vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);',
    '    vec3 cu = vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);',
    '    vec3 wp = c.xyz + (cr * position.x + cu * position.y) * size;',
    '    vUv = position.xy;',
    '    float blink = 0.5 + 0.5 * sin(uTime * 3.2 + aPh);',
    '    vBl = (0.25 + 0.75 * blink * blink) * (1.0 - smoothstep(2.55 * uR, 3.0 * uR, dist)) * smoothstep(uMin * 1.5, uMin * 8.0, dist);',
    '    gl_Position = projectionMatrix * viewMatrix * vec4(wp, 1.0);',
    '    #include <logdepthbuf_vertex>',
    '}'
].join('\n');
var BEAC_FRAG = [
    'uniform vec3 uCol; varying vec2 vUv; varying float vBl;',
    '#include <common>',
    '#include <logdepthbuf_pars_fragment>',
    'void main() {',
    '    #include <logdepthbuf_fragment>',
    '    float r = length(vUv);',
    '    float a = pow(max(0.0, 1.0 - r), 2.4) + 0.9 * pow(max(0.0, 1.0 - r * 4.0), 2.0);',
    '    gl_FragColor = vec4(mix(uCol, vec3(1.0), 0.35) * a, clamp(a, 0.0, 1.0) * vBl);',
    '}'
].join('\n');

// rev 20: gas-giant spores. Soft glowing spheres, instance matrix = (centre, uniform scale), bob + drift in the vertex shader.
var SPORE_VERT = [
    'attribute vec3 aCol; attribute vec3 aSp;',
    'uniform float uTime; uniform float uFadeA; uniform float uFadeB;',
    'varying vec3 vCol; varying vec3 vN; varying vec3 vView; varying float vK;',
    '#include <common>',
    '#include <logdepthbuf_pars_vertex>',
    'void main() {',
    '    vec3 c = instanceMatrix[3].xyz; float sc = length(instanceMatrix[0].xyz);',
    '    vec3 up = normalize(c);',
    '    vec3 sd = normalize(cross(up, vec3(0.267, 0.802, 0.535))); vec3 sd2 = cross(up, sd);',
    '    float ph = aSp.x, rt = aSp.z;',
    '    c += up * (sin(uTime * rt + ph) * aSp.y) + sd * (sin(uTime * rt * 0.37 + ph * 1.7) * aSp.y * 1.8) + sd2 * (cos(uTime * rt * 0.29 + ph) * aSp.y * 1.8);',
    '    vec4 cw = modelMatrix * vec4(c, 1.0);',
    '    float dd = distance(cameraPosition, cw.xyz);',
    '    vK = (1.0 - smoothstep(uFadeA, uFadeB, dd)) * smoothstep(0.6, 2.4, dd / sc);',
    '    float pl = 1.0 + 0.1 * sin(uTime * 1.7 + ph * 3.0);',
    '    vec4 wp = modelMatrix * vec4(c + position * sc * pl, 1.0);',
    '    vN = normalize(mat3(modelMatrix) * position);',
    '    vCol = aCol; vView = cameraPosition - wp.xyz;',
    '    gl_Position = projectionMatrix * viewMatrix * wp;',
    '    #include <logdepthbuf_vertex>',
    '}'
].join('\n');
var SPORE_FRAG = [
    'uniform vec3 uFogCol; uniform float uFogK;',
    'varying vec3 vCol; varying vec3 vN; varying vec3 vView; varying float vK;',
    '#include <common>',
    '#include <logdepthbuf_pars_fragment>',
    'void main() {',
    '    #include <logdepthbuf_fragment>',
    '    vec3 N = normalize(vN), V = normalize(vView);',
    '    float nv = clamp(dot(N, V), 0.0, 1.0);',
    '    vec3 col = vCol * (0.25 + 0.7 * pow(nv, 1.5)) + vec3(1.0) * 0.12 * pow(nv, 12.0) + vCol * pow(1.0 - nv, 2.5) * 0.35;',
    '    float f = min(1.0 - exp(-pow(length(vView) * uFogK, 2.0)), 0.93);',
    '    col *= 1.0 - f * 0.85;',
    '    gl_FragColor = vec4(col, vK * (0.2 + 0.5 * nv));',
    '    #include <tonemapping_fragment>',
    '    #include <colorspace_fragment>',
    '}'
].join('\n');

var DOME_VERT = [
    'varying vec3 vP;',
    '#include <common>',
    '#include <logdepthbuf_pars_vertex>',
    'void main() {',
    '    vP = position;',
    '    vec4 wp = modelMatrix * vec4(position, 1.0);',
    '    gl_Position = projectionMatrix * viewMatrix * wp;',
    '    #include <logdepthbuf_vertex>',
    '}'
].join('\n');
var DOME_FRAG = [
    'uniform vec3 uFogCol; uniform vec3 uAtmo; uniform vec3 uUp; uniform float uAlpha; uniform float uSun; uniform float uHor; uniform vec3 uSunDir; uniform float uStar; uniform float uStorm;',
    'uniform vec3 uPc; uniform vec3 uRingN; uniform vec3 uRingC; uniform vec2 uRingR; uniform float uRingA; uniform vec4 uMoon[4]; uniform vec3 uMoonC[4];',
    'varying vec3 vP;',
    '#include <common>',
    '#include <logdepthbuf_pars_fragment>',
    'void main() {',
    '    #include <logdepthbuf_fragment>',
    '    vec3 v = normalize(vP);',
    '    float c = dot(v, normalize(uUp));',
    '    float z = clamp((c - uHor) / (1.0 - uHor), 0.0, 1.0);',   // 0 at the geometric horizon
    '    vec3 zen = uAtmo * (0.18 + 0.5 * uSun) * 1.5;',
    '    zen = mix(vec3(dot(zen, vec3(0.33))), zen, 0.6 + 0.4 * smoothstep(0.1, 0.8, z));',    // less saturated near the ground
    '    vec3 col = mix(uFogCol, zen, pow(z, 0.7));',
    '    float night = 1.0 - uSun;',
    '    vec3 sc = floor(v * 260.0); float sh = fract(sin(dot(sc, vec3(12.9898, 78.233, 37.719))) * 43758.5453);',
    '    float star = step(0.9965, sh) * smoothstep(0.35, 0.85, z) * night * (0.35 + 0.65 * fract(sh * 91.7));',
    '    col += vec3(0.85, 0.9, 1.0) * star;',
    '    float wash = smoothstep(uHor + 0.002, uHor + 0.07, c);',     // sky only; the ground below is the patch + fog
    '    float sd = dot(v, normalize(uSunDir));',
    '    float glow = pow(max(sd, 0.0), 2.5) * 0.10 * uSun;',            // rev 19: soft directional brightening toward the light, no disc / sprite
    '    col += uFogCol * glow;',
    '    float seeThru = mix(0.4, 0.84, uSun);',                          // planets / the black hole stay visible through the dome: ghosts by day, clearer at night
    '    float thick = 0.5 + 0.5 * smoothstep(0.0, 0.6, z);',
    '    float alpha = max(uAlpha * wash * seeThru * thick, star * 0.9 * uStar);',
    '    if (uRingA > 0.001) {',                                  // rev 27: the planet ring from the ground: exact ray / ring-plane hit (tilt + colour from node.ringMesh), planet shadow on the far side
    '        float dn = dot(v, uRingN);',
    '        if (abs(dn) > 1e-4) {',
    '            float tr = -dot(uPc, uRingN) / dn;',
    '            if (tr > 0.0) {',
    '                vec3 q = uPc + v * tr; float rq = length(q);',
    '                float rr = smoothstep(uRingR.x, uRingR.x + 0.025, rq) * (1.0 - smoothstep(uRingR.y - 0.025, uRingR.y, rq));',
    '                float bd = 0.55 + 0.25 * sin(rq * 85.0) + 0.2 * sin(rq * 210.0 + 2.0);',
    '                bd *= 1.0 - 0.85 * (1.0 - smoothstep(0.0, 0.014, abs(rq - mix(uRingR.x, uRingR.y, 0.62))));',
    '                float sdq = dot(q, uSunDir); float shd = (sdq < 0.0 && length(q - uSunDir * sdq) < 1.0) ? 0.1 : 1.0;',
    '                float rA = rr * bd * uRingA * shd * wash;',
    '                col = mix(col, uRingC * (0.55 + 0.45 * uSun) + uRingC * 0.15, rA); alpha = max(alpha, rA);',
    '            }',
    '        }',
    '    }',
    '    for (int mi = 0; mi < 4; mi++) {',                       // rev 27: the planet\'s own moons as lit discs (uMoon = dir.xyz + tan(angular radius))
    '        float tm = uMoon[mi].w;',
    '        if (tm > 0.0) {',
    '            float dm = dot(v, uMoon[mi].xyz);',
    '            if (dm > 0.0) {',
    '                vec3 pr = (v / dm - uMoon[mi].xyz) / tm; float rho = length(pr);',
    '                if (rho < 1.0) {',
    '                    vec3 nm = normalize(pr + uMoon[mi].xyz * sqrt(1.0 - rho * rho));',
    '                    float lit = smoothstep(-0.08, 0.18, dot(nm, uSunDir));',
    '                    float ma = (1.0 - smoothstep(0.86, 1.0, rho)) * (0.4 + 0.5 * night) * wash;',
    '                    col = mix(col, uMoonC[mi] * (0.1 + 0.95 * lit) + uFogCol * 0.1, ma); alpha = max(alpha, ma * (0.45 + 0.5 * lit));',
    '                }',
    '            }',
    '        }',
    '    }',
    '    col = mix(col, vec3(dot(col, vec3(0.333))) * 0.42, uStorm * 0.85);',
    '    alpha = mix(alpha, max(alpha, 0.85 * wash * thick), uStorm);',
    '    gl_FragColor = vec4(col, alpha);',
    '    #include <tonemapping_fragment>',
    '    #include <colorspace_fragment>',
    '}'
].join('\n');

// ─── rev 27: landmarks (ONE instanced mesh, 6 kinds merged, aSel picks the kind per instance; kind 2 = floating islands bob) ──────────────────
var LM_VERT = [
    'attribute float aVar; attribute float aPart; attribute float aSel; attribute vec3 aCol; attribute vec3 aAcc; attribute float aPh;',
    'uniform float uTime;',
    'varying vec3 vCol; varying vec3 vN; varying vec3 vView; varying float vGlow;',
    '#include <common>',
    '#include <logdepthbuf_pars_vertex>',
    'void main() {',
    '    vec3 pos = position;',
    '    if (abs(aVar - aSel) > 0.5) pos *= 0.0;',
    '    if (aSel > 1.5 && aSel < 2.5) pos.y += (sin(uTime * 0.45 + aPh + position.x * 0.04) * 1.3 + sin(uTime * 0.21 + aPh * 2.0) * 0.7) * step(10.0, position.y);',   // islands only (the part above the ground)
    '    mat4 M = modelMatrix * instanceMatrix;',
    '    vec4 wp = M * vec4(pos, 1.0);',
    '    vN = normalize(mat3(M) * normal);',
    '    vGlow = step(1.5, aPart); vCol = vGlow > 0.5 ? aAcc * (aPart - 1.0) : aCol * aPart;',
    '    vView = cameraPosition - wp.xyz;',
    '    gl_Position = projectionMatrix * viewMatrix * wp;',
    '    #include <logdepthbuf_vertex>',
    '}'
].join('\n');
var LM_FRAG = [
    'uniform vec3 uLightDir; uniform vec3 uFogCol; uniform float uFogK; uniform float uBioN;',
    'varying vec3 vCol; varying vec3 vN; varying vec3 vView; varying float vGlow;',
    '#include <common>',
    '#include <logdepthbuf_pars_fragment>',
    'void main() {',
    '    #include <logdepthbuf_fragment>',
    '    vec3 N = normalize(vN);',
    '    if (!gl_FrontFacing) N = -N;',
    '    float ndl = dot(N, uLightDir) * 0.5 + 0.5;',
    '    float lightAmt = mix(0.3, 1.0, smoothstep(0.08, 0.92, ndl));',
    '    vec3 col = mix(vCol * lightAmt, vCol * (0.75 + uBioN * 1.4), vGlow);',
    '    float f = min(1.0 - exp(-pow(length(vView) * uFogK, 2.0)), 0.93);',
    '    col = mix(col, uFogCol, f * (1.0 - vGlow * 0.75));',
    '    gl_FragColor = vec4(col, 1.0);',
    '    #include <tonemapping_fragment>',
    '    #include <colorspace_fragment>',
    '}'
].join('\n');

// fireflies (kind 0, drift + blink, night only) and geyser steam (kind 1, 6 s puff cycle): ONE Points draw, positions in the planet frame
var PTS_VERT = [
    'attribute vec4 aP; attribute vec3 aC;',        // aP = kind, phase, speed, size (L)
    'uniform float uTime; uniform float uL; uniform float uScale; uniform float uBio; uniform vec3 uSteam;',
    'varying vec3 vC; varying float vA;',
    '#include <common>',
    '#include <logdepthbuf_pars_vertex>',
    'void main() {',
    '    vec3 p = position; vec3 up = normalize(p);',
    '    vec3 sd = normalize(cross(up, vec3(0.267, 0.802, 0.535))); vec3 sd2 = cross(up, sd);',
    '    float ph = aP.y, size = aP.w * uL, a, fa; bool ff = aP.x < 0.5;',
    '    if (ff) {',
    '        float t = uTime * aP.z;',
    '        p += (sd * sin(t * 0.7 + ph * 6.28) * 1.6 + sd2 * cos(t * 0.53 + ph * 11.0) * 1.6 + up * sin(t * 1.1 + ph * 3.0) * 0.5) * uL;',
    '        a = uBio * (0.3 + 0.7 * smoothstep(0.15, 0.9, 0.5 + 0.5 * sin(uTime * (1.3 + aP.z) + ph * 40.0)));',
    '        vC = aC; fa = 45.0;',
    '    } else {',
    '        float age = fract(uTime / 6.0 + ph);',
    '        float H = 22.0 * (0.75 + 0.25 * sin(ph * 40.0));',
    '        p += up * (0.5 + age * H) * uL + (sd * sin(ph * 30.0 + age * 2.0) + sd2 * cos(ph * 20.0 + age * 1.7)) * age * 3.0 * uL;',
    '        size *= 0.7 + 2.8 * age;',
    '        a = smoothstep(0.0, 0.08, age) * (1.0 - age) * 0.55;',
    '        vC = uSteam; fa = 500.0;',
    '    }',
    '    vec4 wp = modelMatrix * vec4(p, 1.0);',
    '    float dd = distance(cameraPosition, wp.xyz);',
    '    a *= (1.0 - smoothstep(fa * uL, fa * 1.8 * uL, dd)) * step(0.001, aP.w);',
    '    vA = a;',
    '    gl_PointSize = clamp(size * uScale / max(dd, 1e-3), ff ? 3.0 : 4.0, ff ? 12.0 : 150.0);',
    '    gl_Position = projectionMatrix * viewMatrix * wp;',
    '    #include <logdepthbuf_vertex>',
    '}'
].join('\n');
var PTS_FRAG = [
    'varying vec3 vC; varying float vA;',
    '#include <common>',
    '#include <logdepthbuf_pars_fragment>',
    'void main() {',
    '    #include <logdepthbuf_fragment>',
    '    float r = length(gl_PointCoord - 0.5) * 2.0; float m = 1.0 - smoothstep(0.0, 1.0, r); m *= m;',
    '    if (vA * m < 0.004) discard;',
    '    gl_FragColor = vec4(mix(vC, vec3(1.0), m * m * 0.45), clamp(m * vA * 1.7, 0.0, 1.0));',
    '    #include <tonemapping_fragment>',
    '    #include <colorspace_fragment>',
    '}'
].join('\n');

// dust haze: a polar disc around the player that rides the terrain at 0.5 L (heights from the same hfun as the patch); clears radially around the ship
var HAZE_VERT = [
    'uniform float uR; uniform float uRel; uniform float uL; uniform vec3 uHC; uniform vec3 uHT1; uniform vec3 uHT2; uniform float uHAng; uniform vec3 uPl;',
    'varying vec3 vWp; varying float vRad; varying float vPd; varying float vWet;',
    '#include <common>',
    '#include <logdepthbuf_pars_vertex>',
    NOISE_GLSL,
    HEIGHT_GLSL,
    'uniform vec3 uPadD[3]; uniform vec3 uPadP[3];',
    'float hfunP(vec3 d) {',
    '    float h = hfun(d);',
    '    for (int i = 0; i < 3; i++) {',
    '        if (uPadP[i].y > 0.0) h = mix(h, uPadP[i].z, 1.0 - smoothstep(uPadP[i].x, uPadP[i].y, length(d - uPadD[i])));',
    '    }',
    '    return uBias + (h - uBias) * uRel;',
    '}',
    'void main() {',
    '    float rf = position.x; float an = position.y; float rho = rf * rf;',
    '    vec3 d = normalize(uHC + (uHT1 * cos(an) + uHT2 * sin(an)) * tan(rho * uHAng));',
    '    float h = hfunP(d);',
    '    float dist = rho * uHAng * uR / uL;',
    '    vPd = length(d - uPl) * uR / uL; vRad = rf; vWp = d * uR / (uL * 18.0);',
    '    vWet = 1.0 - smoothstep(0.3, 0.55, (h - uBias) / (uAmp * 0.1 + 1e-5));',
    '    vec4 wp = modelMatrix * vec4(d * (uR * (1.0 + h) + (0.5 + 0.03 * dist) * uL), 1.0);',
    '    gl_Position = projectionMatrix * viewMatrix * wp;',
    '    #include <logdepthbuf_vertex>',
    '}'
].join('\n');
var HAZE_FRAG = [
    'uniform vec3 uFogCol; uniform float uHazeA; uniform float uClear; uniform float uTime;',
    'varying vec3 vWp; varying float vRad; varying float vPd; varying float vWet;',
    '#include <common>',
    '#include <logdepthbuf_pars_fragment>',
    NOISE_GLSL,
    'void main() {',
    '    #include <logdepthbuf_fragment>',
    '    float n = vnoise(vWp + vec3(uTime * 0.02, 0.0, uTime * 0.013)) * 0.6 + vnoise(vWp * 2.7 - vec3(0.0, uTime * 0.03, 0.0)) * 0.4;',
    '    float dd = vPd + (n - 0.5) * 9.0;',
    '    float cl = smoothstep(uClear * 0.3, uClear, dd);',                       // 0 in the hole the exhaust blows
    '    float bank = exp(-pow((dd - uClear) / (0.25 * uClear + 1.0), 2.0));',     // dust piled up at the edge of the hole
    '    float a = uHazeA * (0.2 + 0.8 * n) * (1.0 - smoothstep(0.55, 1.0, vRad)) * cl * (1.0 + 0.7 * bank) * (1.0 - 0.55 * vWet);',
    '    if (a < 0.004) discard;',
    '    gl_FragColor = vec4(uFogCol * 1.25 + vec3(0.05), min(a, 0.34));',
    '    #include <tonemapping_fragment>',
    '    #include <colorspace_fragment>',
    '}'
].join('\n');

// ─── JS port of the GLSL noise (float32-emulated hash, since the hash multiplies values
// up to ~250 000 where a double and a float disagree on the fract) ────────────────────
var f32 = Math.fround;
function fract(x) { return x - Math.floor(x); }
var C_INV_PI = f32(0.3183099), C_TENTH = f32(0.1), C_207 = f32(2.07);
function nhash(px, py, pz) {
    // GPUs contract p*0.3183099+0.1 into one fma (single rounding); emulate that, the
    // hash amplifies a 1-ulp difference to ~0.1, so plain double math would disagree.
    px = fract(f32(px * C_INV_PI + C_TENTH));
    py = fract(f32(py * C_INV_PI + C_TENTH));
    pz = fract(f32(pz * C_INV_PI + C_TENTH));
    px = f32(px * 17); py = f32(py * 17); pz = f32(pz * 17);
    return fract(f32(f32(f32(px * py) * pz) * f32(f32(px + py) + pz)));
}
function mix(a, b, t) { return a * (1 - t) + b * t; }
function vnoise(x, y, z) {
    var ix = Math.floor(x), iy = Math.floor(y), iz = Math.floor(z);
    var fx = x - ix, fy = y - iy, fz = z - iz;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
    return mix(
        mix(mix(nhash(ix, iy, iz), nhash(ix + 1, iy, iz), fx), mix(nhash(ix, iy + 1, iz), nhash(ix + 1, iy + 1, iz), fx), fy),
        mix(mix(nhash(ix, iy, iz + 1), nhash(ix + 1, iy, iz + 1), fx), mix(nhash(ix, iy + 1, iz + 1), nhash(ix + 1, iy + 1, iz + 1), fx), fy),
        fz);
}
function fbm(x, y, z) {
    var v = 0, a = 0.5;
    for (var i = 0; i < 4; i++) {
        v += a * vnoise(x, y, z);
        x = f32(x * C_207 + f32(11.3)); y = f32(y * C_207 + f32(7.9)); z = f32(z * C_207 + f32(5.1)); a *= 0.5;
    }
    return v;
}
function smoothstep(a, b, x) { var t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); }

// terra n for unit direction d with uniform set u = {seed:{x,y,z}, freq, warp}
function terraN(dx, dy, dz, seed, freq, warp) {
    var fq = f32(freq), px = f32(dx * fq + seed.x), py = f32(dy * fq + seed.y), pz = f32(dz * fq + seed.z);
    var qx = fbm(px + 0.0, py + 3.1, pz + 1.7);
    var qy = fbm(px + 5.2, py + 1.3, pz + 2.8);
    var qz = fbm(px + 1.7, py + 9.2, pz + 4.6);
    var n = fbm(px + (qx - 0.5) * warp, py + (qy - 0.5) * warp, pz + (qz - 0.5) * warp);
    n += (fbm(px * 3.7 + qx * 2, py * 3.7 + qy * 2, pz * 3.7 + qz * 2) - 0.5) * 0.22;
    return n;
}
// height as a fraction of R (matches hfun in HEIGHT_GLSL)
export function sampleHeight(dx, dy, dz, seed, freq, warp, sea, amp, bias, seaH) {
    var n = terraN(dx, dy, dz, seed, freq, warp);
    var land = smoothstep(sea - 0.035, sea + 0.035, n);
    var rel = Math.max(n - (seaH === undefined ? sea : seaH), 0);
    _lastN = n;
    var k = f32(f32(freq) * f32(5.3));
    var sx = f32(f32(seed.x * f32(1.7)) + f32(2.3)), sy = f32(f32(seed.y * f32(1.7)) + f32(2.3)), sz = f32(f32(seed.z * f32(1.7)) + f32(2.3));
    var rid = 1 - Math.abs(2 * vnoise(f32(dx * k + sx), f32(dy * k + sy), f32(dz * k + sz)) - 1);
    return bias + amp * (0.10 * land + rel * (2.2 + 1.6 * rid * smoothstep(0.02, 0.14, rel)));
}
var _lastN = 0;
export var _js = { nhash: nhash, vnoise: vnoise, fbm: fbm, terraN: terraN };

// ─── geometry helpers (non-indexed, flat normals) ───────────────────────────────
function GeoBuilder() { this.pos = []; this.var_ = []; this.part = []; this.w = []; this.ph = []; this.pc = []; }
GeoBuilder.prototype.tri = function (a, b, c, v, part, w3, ph3, pc) {
    var P = [a, b, c];
    for (var i = 0; i < 3; i++) {
        this.pos.push(P[i][0], P[i][1], P[i][2]);
        this.var_.push(v); this.part.push(part);
        this.w.push(w3 ? w3[i] : 0); this.ph.push(ph3 || 0); this.pc.push(pc || 0);
    }
};
// cone / frustum ring fan: rings of `sides` points
GeoBuilder.prototype.cone = function (cx, y0, cz, r0, y1, r1, sides, v, part, w3, ph, pc) {
    for (var i = 0; i < sides; i++) {
        var a0 = i / sides * Math.PI * 2, a1 = (i + 1) / sides * Math.PI * 2;
        var b0 = [cx + Math.cos(a0) * r0, y0, cz + Math.sin(a0) * r0];
        var b1 = [cx + Math.cos(a1) * r0, y0, cz + Math.sin(a1) * r0];
        var t0 = [cx + Math.cos(a0) * r1, y1, cz + Math.sin(a0) * r1];
        var t1 = [cx + Math.cos(a1) * r1, y1, cz + Math.sin(a1) * r1];
        var wb = w3 ? w3[0] : 0, wt = w3 ? w3[1] : 0;
        if (r1 < 1e-5) this.tri(b0, t0, b1, v, part, [wb, wt, wb], ph, pc);
        else {
            this.tri(b0, t0, t1, v, part, [wb, wt, wt], ph, pc);
            this.tri(b0, t1, b1, v, part, [wb, wt, wb], ph, pc);
        }
    }
};
// axis-aligned box; `part` carries the shade (>= 1.5 = lit)
GeoBuilder.prototype.box = function (x0, y0, z0, x1, y1, z1, shade) {
    var A = [x0, y0, z0], B = [x1, y0, z0], C = [x1, y1, z0], D = [x0, y1, z0], E = [x0, y0, z1], F = [x1, y0, z1], G = [x1, y1, z1], H = [x0, y1, z1], q = this;
    function quad(a, b, c, d) { q.tri(a, b, c, 0, shade); q.tri(a, c, d, 0, shade); }
    quad(A, D, C, B); quad(E, F, G, H); quad(A, E, H, D); quad(B, C, G, F); quad(D, H, G, C); quad(A, B, F, E);
};
// flat hexagon fan at height y between radii r0 (inner, 0 = filled) and r1
GeoBuilder.prototype.hex = function (r0, r1, y, shade) {
    for (var i = 0; i < 6; i++) {
        var a0 = i / 6 * Math.PI * 2, a1 = (i + 1) / 6 * Math.PI * 2;
        var o0 = [Math.cos(a0) * r1, y, Math.sin(a0) * r1], o1 = [Math.cos(a1) * r1, y, Math.sin(a1) * r1];
        if (r0 <= 0) this.tri([0, y, 0], o1, o0, 0, shade);
        else {
            var i0 = [Math.cos(a0) * r0, y, Math.sin(a0) * r0], i1 = [Math.cos(a1) * r0, y, Math.sin(a1) * r0];
            this.tri(i0, o1, o0, 0, shade); this.tri(i0, i1, o1, 0, shade);
        }
    }
};
// low UV sphere (ellipsoid), lat0..lat1 in [0,pi]
GeoBuilder.prototype.sphere = function (cx, cy, cz, rx, ry, rz, seg, rings, lat0, lat1, v, part, w, ph, pc) {
    function P(i, j) {
        var th = lat0 + (lat1 - lat0) * j / rings, a = i / seg * Math.PI * 2;
        return [cx + rx * Math.sin(th) * Math.cos(a), cy + ry * Math.cos(th), cz + rz * Math.sin(th) * Math.sin(a)];
    }
    for (var j = 0; j < rings; j++) for (var i = 0; i < seg; i++) {
        var a = P(i, j), b = P(i + 1, j), c = P(i + 1, j + 1), d = P(i, j + 1);
        if (j > 0 || lat0 > 1e-4) this.tri(a, d, b, v, part, [w, w, w], ph, pc);
        if (j < rings - 1 || lat1 < Math.PI - 1e-4) this.tri(b, d, c, v, part, [w, w, w], ph, pc);
    }
};
function finishGeo(THREE, g, names) {
    var geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute(g.pos, 3));
    var vcount = g.pos.length / 3;
    var nrm = new Float32Array(g.pos.length);
    for (var i = 0; i < vcount; i += 3) {
        var ax = g.pos[i * 3], ay = g.pos[i * 3 + 1], az = g.pos[i * 3 + 2];
        var ux = g.pos[i * 3 + 3] - ax, uy = g.pos[i * 3 + 4] - ay, uz = g.pos[i * 3 + 5] - az;
        var vx = g.pos[i * 3 + 6] - ax, vy = g.pos[i * 3 + 7] - ay, vz = g.pos[i * 3 + 8] - az;
        var nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        var l = Math.hypot(nx, ny, nz) || 1; nx /= l; ny /= l; nz /= l;
        for (var k = 0; k < 3; k++) { nrm[(i + k) * 3] = nx; nrm[(i + k) * 3 + 1] = ny; nrm[(i + k) * 3 + 2] = nz; }
    }
    geo.setAttribute('normal', new THREE.BufferAttribute(nrm, 3));
    geo.setAttribute('aVar', new THREE.Float32BufferAttribute(g.var_, 1));
    if (names.part) geo.setAttribute('aPart', new THREE.Float32BufferAttribute(g.part, 1));
    if (names.anim) {
        geo.setAttribute('aW', new THREE.Float32BufferAttribute(g.w, 1));
        geo.setAttribute('aPh', new THREE.Float32BufferAttribute(g.ph, 1));
        geo.setAttribute('aPartC', new THREE.Float32BufferAttribute(g.pc, 1));
    }
    return geo;
}

function hash01(i, j, k, s) {
    var h = (Math.imul(i | 0, 374761393) + Math.imul(j | 0, 668265263) + Math.imul(k | 0, 2147483647) + Math.imul(s | 0, 1274126177)) | 0;
    h = Math.imul(h ^ (h >>> 13), 1274126177); h ^= h >>> 16;
    return (h >>> 0) / 4294967296;
}
function nodeSeed(node) { var s = 0, id = String(node.id); for (var i = 0; i < id.length; i++) s = (Math.imul(s, 31) + id.charCodeAt(i)) | 0; return s; }
function col3(c) { return c && c.r !== undefined ? [c.r, c.g, c.b] : [c.x, c.y, c.z]; }
function col3o(c, o) { if (c && c.r !== undefined) { o[0] = c.r; o[1] = c.g; o[2] = c.b; } else { o[0] = c.x; o[1] = c.y; o[2] = c.z; } return o; }
var Q_GRID = [64, 96, 128, 128], Q_FLORA = [150, 300, 500, 700], Q_CREAT = [10, 20, 40, 40], Q_ROCK = [150, 250, 400, 400];

// palette -> surface look: 0 rocky (reds / oranges / browns), 1 lush (greens / teals), 2 icy (blues / violets / pale)
export function lookFromPalette(lo, hi) {
    var r = (lo[0] + hi[0]) / 2, g = (lo[1] + hi[1]) / 2, b = (lo[2] + hi[2]) / 2, mx = Math.max(r, g, b), mn = Math.min(r, g, b), d = mx - mn;
    var sat = mx > 0 ? d / mx : 0, hue = 0;
    if (d > 1e-5) { if (mx === r) hue = ((g - b) / d + 6) % 6; else if (mx === g) hue = (b - r) / d + 2; else hue = (r - g) / d + 4; hue *= 60; }
    if (sat < 0.28 && mx > 0.55) return 2;
    if (hue >= 65 && hue < 185) return 1;
    if (hue >= 185 && hue < 300) return 2;
    return 0;
}

// ─── rev 27: landmark geometry (units = L, Y up, ground at y = 0; every kind reaches 5 L below ground so slopes never show a gap) ──────────────────
function LmB() { this.p = []; this.v = []; this.s = []; this.k = 0; }
LmB.prototype.tri = function (a, b, c, sh) { this.p.push(a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2]); this.v.push(this.k, this.k, this.k); this.s.push(sh, sh, sh); };
LmB.prototype.quad = function (a, b, c, d, sh) { this.tri(a, b, c, sh); this.tri(a, c, d, sh); };
function lmAxes(hx, hy, hz, rx, ry, rz) {           // columns of Rz*Ry*Rx scaled by the half extents
    var cx = Math.cos(rx), sx = Math.sin(rx), cy = Math.cos(ry), sy = Math.sin(ry), cz = Math.cos(rz), sz = Math.sin(rz);
    return [[hx * cz * cy, hx * sz * cy, -hx * sy],
            [hy * (cz * sy * sx - sz * cx), hy * (sz * sy * sx + cz * cx), hy * cy * sx],
            [hz * (cz * sy * cx + sz * sx), hz * (sz * sy * cx - cz * sx), hz * cy * cx]];
}
LmB.prototype.obb = function (cx, cy, cz, hx, hy, hz, rx, ry, rz, sh) {
    var A = lmAxes(hx, hy, hz, rx || 0, ry || 0, rz || 0), q = this;
    function P(i, j, k) { return [cx + A[0][0] * i + A[1][0] * j + A[2][0] * k, cy + A[0][1] * i + A[1][1] * j + A[2][1] * k, cz + A[0][2] * i + A[1][2] * j + A[2][2] * k]; }
    q.quad(P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), sh); q.quad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), sh);
    q.quad(P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), sh); q.quad(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), sh);
    q.quad(P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), sh); q.quad(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), sh);
};
// tapered prism from a (radius r0) to b (radius r1; 0 = pointed); caps closed
LmB.prototype.frus = function (a, b, r0, r1, sides, sh) {
    var ax = [b[0] - a[0], b[1] - a[1], b[2] - a[2]], al = Math.hypot(ax[0], ax[1], ax[2]) || 1; ax = [ax[0] / al, ax[1] / al, ax[2] / al];
    var rf = Math.abs(ax[1]) < 0.9 ? [0, 1, 0] : [1, 0, 0];
    var u = [ax[1] * rf[2] - ax[2] * rf[1], ax[2] * rf[0] - ax[0] * rf[2], ax[0] * rf[1] - ax[1] * rf[0]], ul = Math.hypot(u[0], u[1], u[2]); u = [u[0] / ul, u[1] / ul, u[2] / ul];
    var w = [ax[1] * u[2] - ax[2] * u[1], ax[2] * u[0] - ax[0] * u[2], ax[0] * u[1] - ax[1] * u[0]];
    function R(c, r, i) { var an = i / sides * Math.PI * 2, co = Math.cos(an) * r, si = Math.sin(an) * r; return [c[0] + u[0] * co + w[0] * si, c[1] + u[1] * co + w[1] * si, c[2] + u[2] * co + w[2] * si]; }
    for (var i = 0; i < sides; i++) {
        var a0 = R(a, r0, i), a1 = R(a, r0, i + 1), b0 = R(b, r1, i), b1 = R(b, r1, i + 1);
        if (r1 < 1e-4) this.tri(a0, b, a1, sh); else { this.tri(a0, b0, b1, sh); this.tri(a0, b1, a1, sh); this.tri(b, b1, b0, sh); }
        this.tri(a, a1, a0, sh);
    }
};
function lmGeometry(THREE) {
    var B = new LmB(), i, an, x, z;
    B.k = 0;                                                             // 0 crystal spire (3x a tree)
    B.frus([0, -5, 0], [0, 1.6, 0], 9.5, 6.5, 7, 0.75);
    B.frus([0, 1, 0], [0.8, 27, 0.4], 3.7, 0, 6, 2.0);
    for (i = 0; i < 5; i++) { an = i * 1.256 + 0.4; x = Math.cos(an); z = Math.sin(an); B.frus([x * 3.6, 0.5, z * 3.6], [x * 6.6, 8.5 + i * 2.3, z * 6.6], 1.5, 0, 5, i % 2 ? 1.75 : 2.0); }
    B.k = 1;                                                             // 1 stone arch: two leaning pillars + beam
    B.obb(-9.5, 6, 0, 2.4, 11, 2.7, 0, 0, -0.16, 0.85); B.obb(9.5, 6, 0, 2.4, 11, 2.7, 0, 0, 0.16, 0.8);
    B.obb(0, 16.6, 0, 11.4, 1.9, 2.8, 0, 0, 0.02, 0.95);
    B.obb(0, 14.5, 2.85, 1.6, 0.22, 0.06, 0, 0, 0, 1.8);
    B.obb(13, 0.6, 6, 2.2, 1.4, 1.6, 0, 0.5, 0.1, 0.7); B.obb(-14, 0.2, -5, 1.6, 1.0, 2.2, 0.1, 0.9, 0, 0.65);
    B.k = 2;                                                             // 2 floating rock islands (bob in the vertex shader)
    var Y = 34;
    B.frus([0, Y, 0], [0, Y + 1.6, 0], 9.4, 8.6, 8, 1.15); B.frus([0, Y, 0], [0.6, Y - 12, 0.4], 9.4, 0, 8, 0.55);
    B.frus([0.5, Y + 1.6, 0], [0.9, Y + 8.5, 0.2], 1.3, 0, 5, 1.9); B.obb(-3, Y + 2.4, 2, 1.4, 0.8, 1.1, 0.1, 0.4, 0.1, 0.9);
    B.frus([17, Y + 8, 3], [17, Y + 9.2, 3], 4.8, 4.2, 7, 1.1); B.frus([17, Y + 8, 3], [17.3, Y + 1.5, 3.2], 4.8, 0, 7, 0.55);
    B.frus([-14, Y + 13, 7], [-14, Y + 13.9, 7], 3.2, 2.8, 6, 1.1); B.frus([-14, Y + 13, 7], [-14.2, Y + 8.5, 7.1], 3.2, 0, 6, 0.55);
    B.k = 3;                                                             // 3 geyser field: 5 vents (steam from the Points draw)
    for (i = 0; i < LM_VENTS.length; i++) { x = LM_VENTS[i][0]; z = LM_VENTS[i][1]; B.frus([x, -2, z], [x, 1.4, z], 4.2, 1.7, 7, 0.7); B.frus([x, 1.4, z], [x, 1.6, z], 1.7, 1.0, 7, 1.7); }
    B.frus([0, -1, 0], [0, 0.2, 0], 17, 15, 9, 0.5);
    B.k = 4;                                                             // 4 crashed hull fragment (dark parts kit)
    B.frus([0, -1, 0], [0, 0.3, 0], 17, 14, 9, 0.28);
    B.obb(0, 4, 0, 10, 3, 4.3, 0.05, 0.25, 0.22, 0.55); B.frus([9, 3.6, -1], [17, 1.8, 1], 3.6, 0, 5, 0.5);
    B.obb(-3, 6.5, -8, 5.2, 0.35, 6.2, 0.4, 0.2, 0.5, 0.45);
    for (i = 0; i < 4; i++) B.obb(-9.5 + i * 0.6, 4.5 + (i % 2) * 1.2, -2.5 + i * 1.7, 0.35, 5 - i * 0.6, 0.35, 0.2 * i, 0, 0.5 + i * 0.1, 0.42);
    B.frus([-10, 2.6, 2], [-17, 4.4, 1], 3.4, 2.6, 7, 0.4); B.frus([-17, 4.4, 1], [-17.4, 4.5, 1], 2.5, 2.2, 7, 1.6);
    B.obb(7, 0.6, 8, 1.8, 0.6, 1.2, 0.2, 0.7, 0, 0.4); B.obb(-6, 0.5, -10, 1.2, 0.7, 1.7, 0, 0.3, 0.3, 0.38);
    B.k = 5;                                                             // 5 monolith ring: 7 obelisks around a small altar
    for (i = 0; i < 7; i++) {
        an = i / 7 * Math.PI * 2; x = Math.cos(an) * 11; z = Math.sin(an) * 11;
        B.obb(x, 4, z, 0.9, 9, 0.9, 0.04 * ((i % 3) - 1), -an, 0, 0.8); B.frus([x, 13, z], [x, 15.2, z], 1.2, 0, 4, 0.85);
        B.obb(x, 6.5, z, 0.97, 0.24, 0.97, 0, -an, 0, 2.0);
    }
    B.frus([0, -0.5, 0], [0, 0.5, 0], 3.2, 2.6, 7, 0.7); B.frus([0, 0.5, 0], [0, 3.4, 0], 1.1, 0, 5, 1.8);
    var geo = new THREE.BufferGeometry(), n = B.p.length / 3, nrm = new Float32Array(B.p.length), j;
    for (j = 0; j < n; j += 3) {
        var P = B.p, ux = P[j * 3 + 3] - P[j * 3], uy = P[j * 3 + 4] - P[j * 3 + 1], uz = P[j * 3 + 5] - P[j * 3 + 2], vx = P[j * 3 + 6] - P[j * 3], vy = P[j * 3 + 7] - P[j * 3 + 1], vz = P[j * 3 + 8] - P[j * 3 + 2];
        var nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx, nl = Math.hypot(nx, ny, nz) || 1;
        for (var t3 = 0; t3 < 3; t3++) { nrm[(j + t3) * 3] = nx / nl; nrm[(j + t3) * 3 + 1] = ny / nl; nrm[(j + t3) * 3 + 2] = nz / nl; }
    }
    geo.setAttribute('position', new THREE.Float32BufferAttribute(B.p, 3)); geo.setAttribute('normal', new THREE.BufferAttribute(nrm, 3));
    geo.setAttribute('aVar', new THREE.Float32BufferAttribute(B.v, 1)); geo.setAttribute('aPart', new THREE.Float32BufferAttribute(B.s, 1));
    return geo;
}
var LM_VENTS = [[0, 0], [8, 3], [-7, 5], [4, -8], [-3, -9]];
var LM_KINDS = ['spire', 'arch', 'islands', 'geyser', 'hull', 'monolith'], LM_NOUN = ['Spire', 'Arch', 'Reach', 'Geysers', 'Wreck', 'Circle'];
var SYL_A = ['V', 'K', 'Z', 'Th', 'Br', 'M', 'S', 'T', 'N', 'Qu', 'Dr', 'Sh'], SYL_B = ['o', 'a', 'e', 'i', 'u', 'ai', 'ou'], SYL_C = ['hr', 'rn', 'l', 'th', 'x', 'n', 'r', 'sk', 'm', 'sh'];
var FF_MAX = 400, STEAM_MAX = 120, Q_FF = [120, 200, 300, 400];
function lmName(sd, kind) {
    var n = 1 + (hash01(sd, 3, 1, 1) > 0.55 ? 1 : 0), w = '';
    for (var i = 0; i < n; i++) {
        var on = SYL_A[Math.floor(hash01(sd, 4 + i, 2, 1) * SYL_A.length)]; w += i ? on.toLowerCase() : on;
        w += SYL_B[Math.floor(hash01(sd, 6 + i, 3, 1) * SYL_B.length)];
        if (i === n - 1 || hash01(sd, 8 + i, 4, 1) > 0.5) w += SYL_C[Math.floor(hash01(sd, 10 + i, 5, 1) * SYL_C.length)];
    }
    return 'The ' + w + ' ' + LM_NOUN[kind];
}
function hueOf(lo, hi) {
    var r = (lo[0] + hi[0]) / 2, g = (lo[1] + hi[1]) / 2, b = (lo[2] + hi[2]) / 2, mx = Math.max(r, g, b), d = mx - Math.min(r, g, b), h = 0;
    if (d > 1e-5) { if (mx === r) h = ((g - b) / d + 6) % 6; else if (mx === g) h = (b - r) / d + 2; else h = (r - g) / d + 4; h *= 60; }
    return h;
}

// ═════════════════════════════════════════════════════════════════════════════════
export function createPlanetSurface(engine, L) {
    var THREE = engine.THREE, scene = engine.scene, camera = engine.camera;
    var ps = { active: null, depth: 0, visible: true };

    // shared uniforms (the same objects feed patch, flora, creatures)
    var SH = {
        uFogCol: { value: new THREE.Vector3(0.5, 0.5, 0.7) },
        uFogK: { value: 0 },
        uLightDir: { value: new THREE.Vector3(-0.62, 0.52, 0.4).normalize() },
        uTime: { value: 0 },
        uNight: { value: 0 },
        uBio: { value: 0 }          // rev 27: night-scaled bioluminescence (flora / rocks / fireflies)
    };
    var alloc = null;            // built on first near-mode entry
    var qTier = (typeof globalThis !== 'undefined' && typeof globalThis.EMGOR_PERF_TIER === 'number') ? Math.max(0, Math.min(3, globalThis.EMGOR_PERF_TIER | 0)) : 3;
    var gridN = Q_GRID[qTier], floraLim = Q_FLORA[qTier], creatLim = Q_CREAT[qTier];
    var _cA = [0, 0, 0], _cH = [0, 0, 0];
    var t = 0;
    var node = null, R = 1;
    var haloH = null;            // hooks on the active node's atmo shell + glow sprite
    var haloT = 0, haloAlphaK = 1; // altitude ramp 0..1 (2.2 R -> 1.4 R), inside dim factor
    var rimI = 0, prevPos = new THREE.Vector3(), prevOk = false, spd = 0, flow = new THREE.Vector3(0, 1, 0);
    ps.shake = 0;
    var _mW = new THREE.Matrix4();
    var qInv = new THREE.Quaternion(), tmpV = new THREE.Vector3(), tmpA = new THREE.Vector3(), tmpB = new THREE.Vector3();
    var dirObj = new THREE.Vector3(), wLocal = new THREE.Vector3();
    var sea = 0.5, seaH = 0.5, freq = 3, warp = 1, seedV = null, amp = AMP, bias = BIAS, isGas = false;

    // patch state
    var C = new THREE.Vector3(0, 1, 0), T1 = new THREE.Vector3(1, 0, 0), T2 = new THREE.Vector3(0, 0, 1);
    var halfAng = 0.3, patchSet = false, sporeC = new THREE.Vector3(), sporeSet = false;
    // flora / creature seed centres (object-space unit directions)
    var floraC = new THREE.Vector3(), floraSet = false, creatC = new THREE.Vector3(), creatSet = false, rockC = new THREE.Vector3(), rockSet = false, rockLim = Q_ROCK[3], look = 0;

    // outpost pads (local dir, flat chord, blend chord, height frac) -- mirrors hfunP in the patch shader
    var pads = [], outR = 0, lastLand = false, rideKey = [NaN, 0, 0, 0, 0, 0, 0];
    function hFrac(dx, dy, dz) {
        var h = sampleHeight(dx, dy, dz, seedV, freq, warp, sea, amp, bias, seaH);
        for (var i = 0; i < pads.length; i++) {
            var pd = pads[i], ch = Math.sqrt((dx - pd.d.x) * (dx - pd.d.x) + (dy - pd.d.y) * (dy - pd.d.y) + (dz - pd.d.z) * (dz - pd.d.z));
            if (ch < pd.blend) { var w = 1 - smoothstep(pd.flat, pd.blend, ch); h = h * (1 - w) + pd.h * w; }
        }
        return h;
    }
    function nearPad(dx, dy, dz, extraL) {
        for (var i = 0; i < pads.length; i++) {
            var pd = pads[i];
            if (Math.sqrt((dx - pd.d.x) * (dx - pd.d.x) + (dy - pd.d.y) * (dy - pd.d.y) + (dz - pd.d.z) * (dz - pd.d.z)) < pd.blend + extraL * L / R) return true;
        }
        return false;
    }
    ps.outposts = [];

    function makePatchGeo(G) {
        var n1 = G + 1, pos = new Float32Array(n1 * n1 * 3), idx = new Uint16Array(G * G * 6), k = 0;
        for (var j = 0; j < n1; j++) for (var i = 0; i < n1; i++) {
            pos[(j * n1 + i) * 3] = i / G * 2 - 1; pos[(j * n1 + i) * 3 + 1] = j / G * 2 - 1; pos[(j * n1 + i) * 3 + 2] = 0;
        }
        for (var jj = 0; jj < G; jj++) for (var ii = 0; ii < G; ii++) {
            var v00 = jj * n1 + ii, v10 = v00 + 1, v01 = v00 + n1, v11 = v01 + 1;
            idx[k++] = v00; idx[k++] = v01; idx[k++] = v10; idx[k++] = v10; idx[k++] = v01; idx[k++] = v11;
        }
        var pg = new THREE.BufferGeometry();
        pg.setAttribute('position', new THREE.BufferAttribute(pos, 3));
        pg.setIndex(new THREE.BufferAttribute(idx, 1));
        return pg;
    }

    ps.footMode = false;      // rev 20b: ship.js sets this while the human is out (smaller, denser patch)
    ps.quality = qTier;
    ps.setQuality = function (t) {
        t = t < 0 ? 0 : t > 3 ? 3 : Math.round(t);
        if (t === qTier && alloc) return;
        qTier = t; ps.quality = t;
        floraLim = Q_FLORA[t]; creatLim = Q_CREAT[t]; rockLim = Q_ROCK[t];
        if (Q_GRID[t] !== gridN) {
            gridN = Q_GRID[t];
            if (alloc) { var old = alloc.patch.geometry; alloc.patch.geometry = makePatchGeo(gridN); old.dispose(); }
        }
        floraSet = false; creatSet = false; rockSet = false;       // re-seed against the new caps on the next update
    };

    function build() {
        var a = {};
        // patch
        var pg = makePatchGeo(gridN);
        a.patchU = {
            uSeed: { value: new THREE.Vector3() }, uColHigh: { value: new THREE.Color() }, uColLow: { value: new THREE.Color() },
            uColSea: { value: new THREE.Color() }, uAtmo: { value: new THREE.Color() }, uAccent: { value: new THREE.Color() },
            uBiome: { value: 0 }, uLook: { value: 0 }, uFreq: { value: 3 }, uWarp: { value: 1 }, uBandFreq: { value: 8 }, uSeaLevel: { value: 0.5 },
            uSpeckle: { value: 0 }, uLightDir: SH.uLightDir, uFogCol: SH.uFogCol, uFogK: SH.uFogK,
            uSeaH: { value: 0.5 }, uAmp: { value: AMP }, uBias: { value: BIAS }, uEps: { value: 0.0012 }, uTime: SH.uTime, uR: { value: 1 }, uDetD: { value: 140 * L }, uDetK: { value: 3000 },
            uC: { value: C }, uT1: { value: T1 }, uT2: { value: T2 }, uTan: { value: 0.3 }, uFade: { value: 1 }, uSurf: { value: 0 }, uRel: { value: 0 },
            uPadD: { value: [new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3()] }, uPadP: { value: [new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3()] }
        };
        a.patchMat = new THREE.ShaderMaterial({
            uniforms: a.patchU, vertexShader: PATCH_VERT, fragmentShader: PATCH_FRAG,
            transparent: true, depthWrite: true, toneMapped: false, side: THREE.DoubleSide,      // rev 19: double-sided (a camera that dips under a ridge sees the ground's underside, not space)
            polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2
        });
        a.patch = new THREE.Mesh(pg, a.patchMat);
        a.patch.frustumCulled = false;
        a.patch.renderOrder = -1;     // rev 24: drawn BEFORE the additive glow sprite (renderOrder 0), so the orbital glow still lays over the patch
        a.patch.matrixAutoUpdate = true;
        a.patch.onBeforeRender = syncRide;
        a.ride = new THREE.Group(); a.ride.matrixAutoUpdate = false; scene.add(a.ride);   // spins with the globe, see syncRide
        a.ride.add(a.patch);

        // flora: 3 variants merged, selected per instance
        var g = new GeoBuilder();
        // 0 conifer
        g.cone(0, 0, 0, 0.07, 0.3, 0.05, 5, 0, 0);
        g.cone(0, 0.2, 0, 0.42, 0.65, 0.2, 6, 0, 1);
        g.cone(0, 0.5, 0, 0.3, 1.0, 0.0, 6, 0, 1);
        // 1 round tree
        g.cone(0, 0, 0, 0.08, 0.5, 0.06, 5, 1, 0);
        g.sphere(0, 0.72, 0, 0.42, 0.34, 0.42, 7, 4, 0, Math.PI, 1, 1, 0, 0, 0);
        // 2 palm / mushroom cap
        g.cone(0, 0, 0, 0.06, 0.8, 0.04, 4, 2, 0);
        g.cone(0, 0.78, 0, 0.62, 0.9, 0.0, 7, 2, 1);
        g.cone(0, 0.74, 0, 0.0, 0.78, 0.62, 7, 2, 1);
        // 3 ice spire (icy planets)
        g.cone(0, 0, 0, 0.16, 1.0, 0.0, 5, 3, 1);
        g.cone(0.22, 0, 0.05, 0.1, 0.55, 0.0, 5, 3, 1);
        var fg = finishGeo(THREE, g, { part: true });
        var fa = new Float32Array(FLORA_MAX * 3), fs = new Float32Array(FLORA_MAX);
        a.floraCol = new THREE.InstancedBufferAttribute(fa, 3);
        a.floraSel = new THREE.InstancedBufferAttribute(fs, 1);
        fg.setAttribute('aCol', a.floraCol); fg.setAttribute('aSel', a.floraSel);
        a.floraBio = new THREE.InstancedBufferAttribute(new Float32Array(FLORA_MAX * 3), 3); fg.setAttribute('aBio', a.floraBio);
        a.floraU = { uTime: SH.uTime, uSway: { value: 0.1 }, uFadeA: { value: 130 * L }, uFadeB: { value: 190 * L }, uNearA: { value: 0 }, uNearB: { value: 1e-4 }, uGlow: SH.uNight, uBioN: SH.uBio, uLightDir: SH.uLightDir, uFogCol: SH.uFogCol, uFogK: SH.uFogK };
        a.flora = new THREE.InstancedMesh(fg, new THREE.ShaderMaterial({
            uniforms: a.floraU, vertexShader: FLORA_VERT_SW, fragmentShader: FLORA_FRAG, side: THREE.DoubleSide, toneMapped: false
        }), FLORA_MAX);
        a.flora.count = 0; a.flora.frustumCulled = false; a.flora.renderOrder = 2;
        a.flora.onBeforeRender = syncRide; a.ride.add(a.flora);

        // creatures: walker (0) + jelly (1)
        var c = new GeoBuilder(), s;
        c.sphere(0, 0.55, 0, 0.5, 0.3, 0.38, 7, 4, 0, Math.PI, 0, 0, 0, 0, 0);
        c.sphere(0.46, 0.62, 0.14, 0.07, 0.07, 0.07, 4, 2, 0, Math.PI, 0, 0, 0, 0, 1);
        c.sphere(0.46, 0.62, -0.14, 0.07, 0.07, 0.07, 4, 2, 0, Math.PI, 0, 0, 0, 0, 1);
        for (s = 0; s < 6; s++) {
            var side = s < 3 ? 1 : -1, lx = ((s % 3) - 1) * 0.3, ph = s * 2.1;
            // leg: tapered 3-sided prism from body to ground
            c.cone(lx, 0.5, side * 0.3, 0.07, 0.0, 0.025, 3, 0, 0, [0, 1], ph, 0);
        }
        // jelly bell + 5 tentacles
        c.sphere(0, 1.0, 0, 0.5, 0.4, 0.5, 8, 4, 0, Math.PI * 0.5, 1, 0, 0, 0, 0);
        c.sphere(0, 1.0, 0, 0.18, 0.12, 0.18, 5, 2, 0, Math.PI * 0.5, 1, 0, 0, 0, 1);
        for (s = 0; s < 5; s++) {
            var an = s / 5 * Math.PI * 2;
            c.cone(Math.cos(an) * 0.28, 1.0, Math.sin(an) * 0.28, 0.05, 0.0, 0.012, 3, 1, 0, [0, 1], s * 1.3, 0);
        }
        // ox (variant 2): stocky 4-legged grazer with horns
        c.sphere(0, 0.82, 0, 0.72, 0.44, 0.46, 7, 4, 0, Math.PI, 2, 0, 0, 0, 0);
        c.sphere(0.78, 0.9, 0, 0.26, 0.24, 0.22, 5, 3, 0, Math.PI, 2, 0, 0, 0, 0);
        c.sphere(0.98, 0.96, 0.12, 0.05, 0.05, 0.05, 4, 2, 0, Math.PI, 2, 0, 0, 0, 1);
        c.sphere(0.98, 0.96, -0.12, 0.05, 0.05, 0.05, 4, 2, 0, Math.PI, 2, 0, 0, 0, 1);
        c.cone(0.7, 1.05, 0.2, 0.05, 1.4, 0.0, 3, 2, 0, null, 0, 1);
        c.cone(0.7, 1.05, -0.2, 0.05, 1.4, 0.0, 3, 2, 0, null, 0, 1);
        for (s = 0; s < 4; s++) c.cone((s < 2 ? 0.45 : -0.45), 0.8, (s % 2 ? 0.3 : -0.3), 0.1, 0.0, 0.04, 4, 2, 0, [0, 1], (s === 0 || s === 3) ? 0 : 3.14, 0);
        // strider (variant 3): tall, long thin legs, long neck
        c.sphere(0, 1.9, 0, 0.4, 0.22, 0.24, 6, 3, 0, Math.PI, 3, 0, 0, 0, 0);
        c.cone(0.3, 1.95, 0, 0.08, 2.65, 0.045, 4, 3, 0, null, 0, 0);
        c.sphere(0.4, 2.7, 0, 0.17, 0.12, 0.12, 5, 3, 0, Math.PI, 3, 0, 0, 0, 0);
        c.sphere(0.52, 2.74, 0.07, 0.04, 0.04, 0.04, 4, 2, 0, Math.PI, 3, 0, 0, 0, 1);
        c.sphere(0.52, 2.74, -0.07, 0.04, 0.04, 0.04, 4, 2, 0, Math.PI, 3, 0, 0, 0, 1);
        for (s = 0; s < 4; s++) c.cone((s < 2 ? 0.22 : -0.22), 1.85, (s % 2 ? 0.17 : -0.17), 0.05, 0.0, 0.015, 3, 3, 0, [0, 1], (s === 0 || s === 3) ? 0 : 3.14, 0);
        var cg = finishGeo(THREE, c, { anim: true });
        a.creatCol = new THREE.InstancedBufferAttribute(new Float32Array(CREAT_MAX * 3), 3);
        a.creatSel = new THREE.InstancedBufferAttribute(new Float32Array(CREAT_MAX), 1);
        a.creatPh = new THREE.InstancedBufferAttribute(new Float32Array(CREAT_MAX), 1);
        a.creatGlow = new THREE.InstancedBufferAttribute(new Float32Array(CREAT_MAX), 1); a.creatMov = new THREE.InstancedBufferAttribute(new Float32Array(CREAT_MAX), 1);
        a.creatGlow.setUsage(THREE.DynamicDrawUsage); a.creatMov.setUsage(THREE.DynamicDrawUsage);
        cg.setAttribute('aCol', a.creatCol); cg.setAttribute('aSel', a.creatSel); cg.setAttribute('aInstPh', a.creatPh); cg.setAttribute('aGlow', a.creatGlow); cg.setAttribute('aMov', a.creatMov);
        a.creatU = { uTime: SH.uTime, uFadeA: { value: 110 * L }, uFadeB: { value: 160 * L }, uLightDir: SH.uLightDir, uFogCol: SH.uFogCol, uFogK: SH.uFogK };
        a.creat = new THREE.InstancedMesh(cg, new THREE.ShaderMaterial({
            uniforms: a.creatU, vertexShader: CREAT_VERT, fragmentShader: INST_FRAG, side: THREE.DoubleSide, toneMapped: false
        }), CREAT_MAX);
        a.creat.count = 0; a.creat.frustumCulled = false; a.creat.renderOrder = 2;
        a.creat.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
        a.creat.onBeforeRender = syncRide; a.ride.add(a.creat);
        a.creatData = []; a.creatAlt = [];
        function mkCd() { return { id: '', key: '', kind: '', beh: 0, v: 0, p: new THREE.Vector3(), h: new THREE.Vector3(1, 0, 0), pp: new THREE.Vector3(), want: new THREE.Vector3(), folP: new THREE.Vector3(), folPrev: new THREE.Vector3(), col: [0, 0, 0],
            r: 1, hover: 0, sz: 1, spd: 0, vmax: 1, flee: 0, fleeT: 0, tm: 0, paused: false, leader: -1, ox: 0, oz: 0, px: 0, glowT: 0, mov: 0, tamed: false, folT: -9 }; }
        for (var q = 0; q < CREAT_MAX; q++) { a.creatData.push(mkCd()); a.creatAlt.push(mkCd()); }
        a.creatRR = 0;

        // rocks (rev 18): 300 instanced lumpy boulders scattered on the land near the ship (same shader as the flora, one variant)
        var rk = new GeoBuilder();
        rk.sphere(0, 0.28, 0, 0.72, 0.5, 0.62, 5, 3, 0, Math.PI, 0, 1, 0, 0, 0);
        rk.sphere(0.46, 0.18, 0.22, 0.38, 0.3, 0.32, 4, 2, 0, Math.PI, 0, 1, 0, 0, 0);
        rk.sphere(-0.4, 0.14, -0.2, 0.3, 0.24, 0.28, 4, 2, 0, Math.PI, 0, 1, 0, 0, 0);
        var rg = finishGeo(THREE, rk, { part: true });
        a.rockCol = new THREE.InstancedBufferAttribute(new Float32Array(ROCK_MAX * 3), 3);
        a.rockSel = new THREE.InstancedBufferAttribute(new Float32Array(ROCK_MAX), 1);
        rg.setAttribute('aCol', a.rockCol); rg.setAttribute('aSel', a.rockSel);
        a.rockBio = new THREE.InstancedBufferAttribute(new Float32Array(ROCK_MAX * 3), 3); rg.setAttribute('aBio', a.rockBio);
        a.rockU = { uFadeA: { value: 90 * L }, uFadeB: { value: 150 * L }, uNearA: { value: 1.6 }, uNearB: { value: 3.0 }, uGlow: { value: 0 }, uBioN: SH.uBio, uLightDir: SH.uLightDir, uFogCol: SH.uFogCol, uFogK: SH.uFogK };
        a.rocks = new THREE.InstancedMesh(rg, new THREE.ShaderMaterial({
            uniforms: a.rockU, vertexShader: FLORA_VERT, fragmentShader: FLORA_FRAG, side: THREE.DoubleSide, toneMapped: false
        }), ROCK_MAX);
        a.rocks.count = 0; a.rocks.frustumCulled = false; a.rocks.renderOrder = 2;
        a.rocks.onBeforeRender = syncRide; a.ride.add(a.rocks);

        // spores (rev 20, gas giants): glowing drifting spheres, 60 instances, bob/drift in the vertex shader
        var spg = new GeoBuilder();
        spg.sphere(0, 0, 0, 1, 1, 1, 12, 8, 0, Math.PI, 0, 0, 0, 0, 0);
        var spgeo = finishGeo(THREE, spg, {});
        a.sporeCol = new THREE.InstancedBufferAttribute(new Float32Array(SPORE_MAX * 3), 3);
        a.sporeSp = new THREE.InstancedBufferAttribute(new Float32Array(SPORE_MAX * 3), 3);
        spgeo.setAttribute('aCol', a.sporeCol); spgeo.setAttribute('aSp', a.sporeSp);
        a.sporeU = { uTime: SH.uTime, uFadeA: { value: 55 * L }, uFadeB: { value: 90 * L }, uFogCol: SH.uFogCol, uFogK: SH.uFogK };
        a.spores = new THREE.InstancedMesh(spgeo, new THREE.ShaderMaterial({
            uniforms: a.sporeU, vertexShader: SPORE_VERT, fragmentShader: SPORE_FRAG, side: THREE.FrontSide, toneMapped: false,
            transparent: true, depthWrite: false, blending: THREE.AdditiveBlending
        }), SPORE_MAX);
        a.spores.count = 0; a.spores.frustumCulled = false; a.spores.renderOrder = 4;
        a.spores.onBeforeRender = syncRide; a.ride.add(a.spores);

        // atmosphere dome (camera-attached inverted sphere)
        a.domeU = { uFogCol: SH.uFogCol, uAtmo: { value: new THREE.Color() }, uUp: { value: new THREE.Vector3(0, 1, 0) }, uAlpha: { value: 0 }, uSun: { value: 1 }, uHor: { value: -0.3 }, uSunDir: SH.uLightDir, uStar: { value: 0 }, uStorm: { value: 0 },
            uPc: { value: new THREE.Vector3() }, uRingN: { value: new THREE.Vector3(0, 1, 0) }, uRingC: { value: new THREE.Color(1, 1, 1) }, uRingR: { value: new THREE.Vector2(1.4, 1.9) }, uRingA: { value: 0 },
            uMoon: { value: [new THREE.Vector4(), new THREE.Vector4(), new THREE.Vector4(), new THREE.Vector4()] }, uMoonC: { value: [new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3()] } };
        a.dome = new THREE.Mesh(new THREE.SphereGeometry(1, 24, 16), new THREE.ShaderMaterial({
            uniforms: a.domeU, vertexShader: DOME_VERT, fragmentShader: DOME_FRAG,
            side: THREE.BackSide, transparent: true, depthWrite: false, depthTest: true, toneMapped: false
        }));
        // drawn AFTER every far body / star / black hole (renderOrder 8 > their 0), BEFORE ship fx (15+). Depth-tested at
        // DOME_L ship lengths: the ground, trees and ship (nearer) stay crisp, everything beyond is washed by the sky.
        a.dome.scale.setScalar(Math.min(DOME_L * L, camera.far * 0.4));
        a.dome.frustumCulled = false; a.dome.renderOrder = 8;
        a.dome.onBeforeRender = function () { a.dome.position.set(camera.position.x - scene.position.x, camera.position.y - scene.position.y, camera.position.z - scene.position.z); a.dome.updateMatrixWorld(); };   // rev 20: floating origin: world = local + scene.position
        scene.add(a.dome);

        // wind streaks: ONE mesh, NSTREAK camera-facing quads (positions are camera-relative, mesh follows the camera)
        var sg = new THREE.BufferGeometry(), nv = NSTREAK * 4;
        a.stPos = new THREE.BufferAttribute(new Float32Array(nv * 3), 3); a.stOth = new THREE.BufferAttribute(new Float32Array(nv * 3), 3);
        a.stPos.setUsage(THREE.DynamicDrawUsage); a.stOth.setUsage(THREE.DynamicDrawUsage);
        var sEnd = new Float32Array(nv), sSide = new Float32Array(nv), sIdx = new Uint16Array(NSTREAK * 6);
        for (var q = 0; q < NSTREAK; q++) {
            var v0 = q * 4; sEnd[v0] = 0; sEnd[v0 + 1] = 0; sEnd[v0 + 2] = 1; sEnd[v0 + 3] = 1;
            sSide[v0] = -1; sSide[v0 + 1] = 1; sSide[v0 + 2] = -1; sSide[v0 + 3] = 1;
            sIdx.set([v0, v0 + 1, v0 + 2, v0 + 2, v0 + 1, v0 + 3], q * 6);
        }
        sg.setAttribute('position', a.stPos); sg.setAttribute('aOth', a.stOth);
        sg.setAttribute('aEnd', new THREE.BufferAttribute(sEnd, 1)); sg.setAttribute('aSide', new THREE.BufferAttribute(sSide, 1));
        sg.setIndex(new THREE.BufferAttribute(sIdx, 1));
        a.stU = { uCol: { value: new THREE.Color(1, 1, 1) }, uAlpha: { value: 0 }, uW: { value: 0.05 * L } };
        a.streaks = new THREE.Mesh(sg, new THREE.ShaderMaterial({
            uniforms: a.stU, vertexShader: STREAK_VERT, fragmentShader: STREAK_FRAG,
            transparent: true, depthWrite: false, depthTest: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide, toneMapped: false
        }));
        a.streaks.frustumCulled = false; a.streaks.renderOrder = 16; a.streaks.visible = false;
        a.streaks.onBeforeRender = function () { a.streaks.position.set(camera.position.x - scene.position.x, camera.position.y - scene.position.y, camera.position.z - scene.position.z); a.streaks.updateMatrixWorld(); };
        scene.add(a.streaks);
        a.st = [];
        for (var z = 0; z < NSTREAK; z++) a.st.push({ x: 0, y: 0, z: 0, len: 1, sp: 1, w: 1, init: false });

        // rev 26 weather: rain (LineSegments, <= RAIN_MAX segments) + aurora ribbon. Both follow the camera; +2 draw calls while visible.
        var RN = Q_RAIN[qTier], rg2 = new THREE.BufferGeometry(), rs = new Float32Array(RN * 2 * 3), re = new Float32Array(RN * 2);
        for (var rq = 0; rq < RN; rq++) {
            var r0 = Math.random(), r1 = Math.random(), r2 = Math.random();
            for (var rv = 0; rv < 2; rv++) { rs[(rq * 2 + rv) * 3] = r0; rs[(rq * 2 + rv) * 3 + 1] = r1; rs[(rq * 2 + rv) * 3 + 2] = r2; re[rq * 2 + rv] = rv; }
        }
        rg2.setAttribute('position', new THREE.BufferAttribute(new Float32Array(RN * 2 * 3), 3)); rg2.setAttribute('aS', new THREE.BufferAttribute(rs, 3)); rg2.setAttribute('aEnd', new THREE.BufferAttribute(re, 1));
        rg2.setDrawRange(0, 0);
        a.rainU = { uTime: SH.uTime, uUp: { value: new THREE.Vector3(0, 1, 0) }, uRt: { value: new THREE.Vector3(1, 0, 0) }, uFw: { value: new THREE.Vector3(0, 0, 1) }, uWind: { value: new THREE.Vector3() },
            uW: { value: 70 * L }, uH: { value: 45 * L }, uFall: { value: 55 * L }, uLen: { value: 0.09 }, uCol: { value: new THREE.Color(0.62, 0.76, 1.0) }, uAlpha: { value: 0 } };
        a.rain = new THREE.LineSegments(rg2, new THREE.ShaderMaterial({ uniforms: a.rainU, vertexShader: RAIN_VERT, fragmentShader: RAIN_FRAG,
            transparent: true, depthWrite: false, depthTest: false, blending: THREE.AdditiveBlending, toneMapped: false }));
        a.rain.frustumCulled = false; a.rain.renderOrder = 17; a.rain.visible = false; a.rainN = RN;
        a.rain.onBeforeRender = function () { a.rain.position.set(camera.position.x - scene.position.x, camera.position.y - scene.position.y, camera.position.z - scene.position.z); a.rain.updateMatrixWorld(); };
        scene.add(a.rain);
        a.aurU = { uUp: a.domeU.uUp, uRt: { value: new THREE.Vector3(1, 0, 0) }, uFw: { value: new THREE.Vector3(0, 0, 1) }, uTime: SH.uTime, uAz: { value: 0 }, uR: { value: 1 }, uI: { value: 0 },
            uColA: { value: new THREE.Color(0.12, 1.0, 0.6) }, uColB: { value: new THREE.Color(0.55, 0.3, 1.0) } };
        a.aurora = new THREE.Mesh(new THREE.PlaneGeometry(1, 1, 96, 10), new THREE.ShaderMaterial({ uniforms: a.aurU, vertexShader: AUR_VERT, fragmentShader: AUR_FRAG,
            side: THREE.DoubleSide, transparent: true, depthWrite: false, depthTest: true, blending: THREE.AdditiveBlending, toneMapped: false }));
        a.aurU.uR.value = Math.min(DOME_L * L, camera.far * 0.4) * 0.72;
        a.aurora.frustumCulled = false; a.aurora.renderOrder = 9; a.aurora.visible = false;
        a.aurora.onBeforeRender = function () { a.aurora.position.set(camera.position.x - scene.position.x, camera.position.y - scene.position.y, camera.position.z - scene.position.z); a.aurora.updateMatrixWorld(); };
        scene.add(a.aurora);

        // scars: NSCAR pooled ribbons (planet-frame group so they ride the spin)
        a.scarG = new THREE.Group(); a.ride.add(a.scarG);
        a.scars = [];
        for (var c2 = 0; c2 < NSCAR; c2++) {
            var cg2 = new THREE.BufferGeometry();
            cg2.setAttribute('position', new THREE.BufferAttribute(new Float32Array(SCAR_PTS * 2 * 3), 3));
            var ci = new Uint16Array((SCAR_PTS - 1) * 6);
            for (var w = 0; w < SCAR_PTS - 1; w++) ci.set([w * 2, w * 2 + 1, w * 2 + 2, w * 2 + 2, w * 2 + 1, w * 2 + 3], w * 6);
            cg2.setIndex(new THREE.BufferAttribute(ci, 1));
            var cm = new THREE.Mesh(cg2, new THREE.MeshBasicMaterial({
                color: 0xff7a2a, transparent: true, opacity: 0, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide, toneMapped: false
            }));
            cm.frustumCulled = false; cm.renderOrder = 3; cm.visible = false; cm.onBeforeRender = syncRide;
            a.scarG.add(cm);
            a.scars.push({ mesh: cm, ttl: 0, max: 1, born: 0 });
        }
        // outposts: pads, buildings, beacons (3 instanced draws, up to NPAD instances each)
        var pg2 = new GeoBuilder();
        pg2.hex(0, 1.0, 0.1, 0.5);                       // slab
        pg2.hex(0.84, 1.0, 0.12, 0.62);                  // rim
        pg2.hex(0, 0.84, 0.14, 0.95);                    // plate
        pg2.hex(0.30, 0.42, 0.16, 1.7);                  // lit ring
        for (var hc = 0; hc < 6; hc++) {
            var ha = hc / 6 * Math.PI * 2, hx = Math.cos(ha) * 0.92, hz = Math.sin(ha) * 0.92;
            pg2.box(hx - 0.035, 0.1, hz - 0.035, hx + 0.035, 0.34, hz + 0.035, 2.3);
        }
        var bg = new GeoBuilder();
        bg.box(-3, -0.8, -2.5, 3, 3.2, 2.5, 1.0);        // main block
        bg.box(-3.2, 3.2, -2.7, 3.2, 3.45, 2.7, 0.55);   // roof
        bg.box(-1.8, 3.45, -1.6, 1.8, 5.2, 1.7, 0.88);   // upper floor
        bg.box(-2.0, 5.2, -1.8, 2.0, 5.4, 1.9, 0.55);
        bg.box(3.0, -0.8, -1.2, 5.2, 1.8, 1.2, 0.78);    // annex
        bg.box(-0.1, 5.4, -0.1, 0.1, 10.0, 0.1, 0.5);    // antenna mast
        bg.box(-0.25, 10.0, -0.25, 0.25, 10.5, 0.25, 2.4);   // antenna lamp
        bg.box(-3.06, 1.5, -1.9, -3.0, 2.4, 1.9, 1.9);   // lit window strip, faces the pad
        bg.box(-1.2, 3.9, 1.7, 1.2, 4.7, 1.76, 1.9);
        function outMesh(gb, name) {
            var geo = finishGeo(THREE, gb, { part: true });
            var col = new THREE.InstancedBufferAttribute(new Float32Array(NPAD * 3), 3);
            geo.setAttribute('aCol', col);
            var m = new THREE.InstancedMesh(geo, new THREE.ShaderMaterial({
                uniforms: { uLightDir: SH.uLightDir, uFogCol: SH.uFogCol, uFogK: SH.uFogK },
                vertexShader: OUT_VERT, fragmentShader: OUT_FRAG, side: THREE.DoubleSide, toneMapped: false,
                polygonOffset: true, polygonOffsetFactor: -3, polygonOffsetUnits: -3
            }), NPAD);
            m.count = 0; m.frustumCulled = false; m.renderOrder = 2; m.onBeforeRender = syncRide; m.name = name;
            a.ride.add(m);
            return { mesh: m, col: col };
        }
        var po = outMesh(pg2, 'ps-pads'), bo = outMesh(bg, 'ps-bld');
        a.pads = po.mesh; a.padCol = po.col; a.blds = bo.mesh; a.bldCol = bo.col;
        var qg = new THREE.BufferGeometry();
        qg.setAttribute('position', new THREE.BufferAttribute(new Float32Array([-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0]), 3));
        qg.setIndex(new THREE.BufferAttribute(new Uint16Array([0, 1, 2, 0, 2, 3]), 1));
        var bph = new THREE.InstancedBufferAttribute(new Float32Array(NPAD), 1);
        qg.setAttribute('aPh', bph);
        a.beacU = { uTime: SH.uTime, uR: { value: 1 }, uMin: { value: 1.2 * L }, uCol: { value: new THREE.Color(1, 0.5, 0.9) } };
        a.beac = new THREE.InstancedMesh(qg, new THREE.ShaderMaterial({
            uniforms: a.beacU, vertexShader: BEAC_VERT, fragmentShader: BEAC_FRAG, transparent: true, depthWrite: false,
            blending: THREE.AdditiveBlending, side: THREE.DoubleSide, toneMapped: false
        }), NPAD);
        a.beac.count = 0; a.beac.frustumCulled = false; a.beac.renderOrder = 17; a.beac.onBeforeRender = syncRide;
        a.beacPh = bph; a.ride.add(a.beac);
        // rev 27: landmarks (one instanced draw, kind merged by aSel), night points (fireflies + geyser steam), dust haze
        a.lmSel = new THREE.InstancedBufferAttribute(new Float32Array(4), 1); a.lmCol = new THREE.InstancedBufferAttribute(new Float32Array(12), 3);
        a.lmAcc = new THREE.InstancedBufferAttribute(new Float32Array(12), 3); a.lmPh = new THREE.InstancedBufferAttribute(new Float32Array(4), 1);
        var lgeo = lmGeometry(THREE);
        lgeo.setAttribute('aSel', a.lmSel); lgeo.setAttribute('aCol', a.lmCol); lgeo.setAttribute('aAcc', a.lmAcc); lgeo.setAttribute('aPh', a.lmPh);
        a.lmU = { uTime: SH.uTime, uLightDir: SH.uLightDir, uFogCol: SH.uFogCol, uFogK: SH.uFogK, uBioN: SH.uBio };
        a.lm = new THREE.InstancedMesh(lgeo, new THREE.ShaderMaterial({ uniforms: a.lmU, vertexShader: LM_VERT, fragmentShader: LM_FRAG, side: THREE.DoubleSide, toneMapped: false }), 4);
        a.lm.count = 0; a.lm.frustumCulled = false; a.lm.renderOrder = 2; a.lm.onBeforeRender = syncRide; a.ride.add(a.lm);
        var PM = FF_MAX + STEAM_MAX, pgeo = new THREE.BufferGeometry();
        a.ptsPos = new THREE.BufferAttribute(new Float32Array(PM * 3), 3); a.ptsP = new THREE.BufferAttribute(new Float32Array(PM * 4), 4); a.ptsC = new THREE.BufferAttribute(new Float32Array(PM * 3), 3);
        pgeo.setAttribute('position', a.ptsPos); pgeo.setAttribute('aP', a.ptsP); pgeo.setAttribute('aC', a.ptsC);
        a.ptsU = { uTime: SH.uTime, uL: { value: L }, uScale: { value: 1000 }, uBio: SH.uBio, uSteam: { value: new THREE.Vector3(0.8, 0.85, 0.9) } };
        a.pts = new THREE.Points(pgeo, new THREE.ShaderMaterial({ uniforms: a.ptsU, vertexShader: PTS_VERT, fragmentShader: PTS_FRAG, transparent: true, depthWrite: false, toneMapped: false }));
        a.pts.frustumCulled = false; a.pts.renderOrder = 5; a.pts.visible = false; a.pts.onBeforeRender = syncRide; a.ride.add(a.pts);
        a.ffN = 0; a.steamN = 0;
        var HR = 22, HS = 56, hv = [], hi2 = [];
        for (var hj = 0; hj <= HR; hj++) for (var hk = 0; hk <= HS; hk++) hv.push(hj / HR, hk / HS * Math.PI * 2, 0);
        for (var hj2 = 0; hj2 < HR; hj2++) for (var hk2 = 0; hk2 < HS; hk2++) { var q0 = hj2 * (HS + 1) + hk2, q1 = q0 + 1, q2 = q0 + HS + 1, q3 = q2 + 1; hi2.push(q0, q2, q1, q1, q2, q3); }
        var hg = new THREE.BufferGeometry(); hg.setAttribute('position', new THREE.Float32BufferAttribute(hv, 3)); hg.setIndex(hi2);
        Object.assign(a.patchU, { uL: { value: L }, uHC: { value: new THREE.Vector3(0, 1, 0) }, uHT1: { value: new THREE.Vector3(1, 0, 0) }, uHT2: { value: new THREE.Vector3(0, 0, 1) }, uHAng: { value: 0.05 }, uPl: { value: new THREE.Vector3(0, 1, 0) }, uClear: { value: 0 }, uHazeA: { value: 0 } });
        a.hazeMat = new THREE.ShaderMaterial({ uniforms: a.patchU, vertexShader: HAZE_VERT, fragmentShader: HAZE_FRAG, transparent: true, depthWrite: false, side: THREE.DoubleSide, toneMapped: false });
        a.haze = new THREE.Mesh(hg, a.hazeMat); a.haze.frustumCulled = false; a.haze.renderOrder = 3; a.haze.visible = false; a.haze.onBeforeRender = syncRide; a.ride.add(a.haze);
        return a;
    }

    // ─── ride sync: copy the globe's CURRENT anchor position + spin onto the 'ride' group. Called from onBeforeRender
    // of every ground object, i.e. after updateBodies() has advanced mesh.rotation this frame (update() runs before
    // it, which is what made the terrain lag the painted sphere by a frame = w*R*dt of slide).
    function syncRide() {
        if (!node || !alloc) return;
        var q = node.mesh.quaternion, p = node.anchor.position, k = rideKey;
        if (k[0] === q.x && k[1] === q.y && k[2] === q.z && k[3] === q.w && k[4] === p.x && k[5] === p.y && k[6] === p.z) return;
        k[0] = q.x; k[1] = q.y; k[2] = q.z; k[3] = q.w; k[4] = p.x; k[5] = p.y; k[6] = p.z;
        var r = alloc.ride;
        r.position.copy(p); r.quaternion.copy(q); r.updateMatrix(); r.updateMatrixWorld(true);
        // rev 20: outposts come from the LOCAL matrices (ride.matrix x pad.matrix = true world), never matrixWorld: during a floating-origin
        // render the scene is translated by -camera, which would shift matrixWorld (and every position derived from it) by the camera position
        for (var i = 0; i < ps.outposts.length; i++) {
            var o = ps.outposts[i], me = _mW.multiplyMatrices(r.matrix, o.pad.matrix).elements;
            o.pos.set(me[12], me[13], me[14]);
            o.n.set(me[4], me[5], me[6]).normalize();
            o.axis.set(me[0], me[1], me[2]).normalize();
            o.npcSpot.pos.copy(o.pos).addScaledVector(o.axis, NPC_OFF * L);
            o.npcSpot.facing.copy(o.axis).multiplyScalar(-1);
        }
    }

    // ─── outposts (seeded per planet id) ──────────────────────────────────────────
    var OUT_A = ['Kel', 'Vor', 'Ash', 'Nim', 'Tor', 'Sev', 'Lum', 'Ori', 'Dra', 'Pel', 'Mar', 'Zan'];
    var OUT_B = ['vara', 'dun', 'thos', 'mere', 'lis', 'quor', 'bay', 'nox', 'rell', 'ita', 'gate', 'wick'];
    var OUT_T = ['Outpost', 'Landing', 'Port', 'Station', 'Haven', 'Depot'];
    function seedOutposts() {
        var a = alloc, sd = nodeSeed(node) + 31337, found = [], k, i;
        pads = []; ps.outposts = [];
        a.pads.count = a.blds.count = a.beac.count = 0;
        for (i = 0; i < NPAD; i++) { a.patchU.uPadP.value[i].set(0, 0, 0); }
        if (isGas || !seedV) return;
        var want = 1 + Math.floor(hash01(sd, 1, 2, 3) * 3), flatA = PAD_FLAT * L / R, blendA = PAD_BLEND * L / R;
        var ringN = 6;
        var cands = [];
        var usedPass = 0;
        for (var pass = 0; pass < 3 && !cands.length; pass++)      // rev 21: pass 0 = lowland, 1 = any land (more samples), 2 = anything above the waterline: every rocky planet gets >= 1 outpost
        for (k = 0, usedPass = pass; k < (pass ? 2500 : 500); k++) {                      // sample the sphere, score each lowland site by how flat its surroundings are
            var z = hash01(k, 7, sd, 1) * 2 - 1, ph = hash01(k, 8, sd, 2) * 6.2831853, rr = Math.sqrt(1 - z * z);
            var dx = rr * Math.cos(ph), dy = z, dz = rr * Math.sin(ph);
            var hc = hFrac(dx, dy, dz);
            if (pass === 0 ? !(_lastN > sea + 0.05 && _lastN < sea + 0.115) : pass === 1 ? !(_lastN > sea + 0.02) : !(_lastN > sea + 0.002)) continue;           // lowland, not beach / mountain
            var ref0 = Math.abs(dy) < 0.9, e1x, e1y, e1z;
            if (ref0) { e1x = dz; e1y = 0; e1z = -dx; } else { e1x = 0; e1y = -dz; e1z = dy; }
            var el = Math.hypot(e1x, e1y, e1z); e1x /= el; e1y /= el; e1z /= el;
            var e2x = e1y * dz - e1z * dy, e2y = e1z * dx - e1x * dz, e2z = e1x * dy - e1y * dx;   // e1 x d (right-handed X=e1, Y=d, Z=e2)
            var maxd = 0, minN = 9;
            for (var rg = 0; rg < ringN; rg++) {
                var ang = rg / ringN * 6.2832, cx = Math.cos(ang) * blendA * 1.4, cy = Math.sin(ang) * blendA * 1.4;
                var px = dx + e1x * cx + e2x * cy, py = dy + e1y * cx + e2y * cy, pz = dz + e1z * cx + e2z * cy, pl = Math.hypot(px, py, pz);
                var hh = hFrac(px / pl, py / pl, pz / pl);
                minN = Math.min(minN, _lastN); maxd = Math.max(maxd, Math.abs(hh - hc));
            }
            if (pass < 2 && minN < sea + 0.012) continue;            // whole ring on land
            cands.push({ d: new THREE.Vector3(dx, dy, dz), h: hc, e1: [e1x, e1y, e1z], e2: [e2x, e2y, e2z], k: k, score: maxd / (blendA * 1.4) });
        }
        cands.sort(function (p, q) { return p.score - q.score; });
        for (var ci = 0; ci < cands.length && found.length < want; ci++) {
            var cd = cands[ci], okSep = true;
            for (i = 0; i < found.length; i++) if (cd.d.dot(found[i].d) > Math.cos(OUT_SEP)) { okSep = false; break; }
            if (okSep && (found.length === 0 || (usedPass === 0 && cd.score < 0.35))) found.push(cd);
        }
        var pm = a.pads.instanceMatrix.array, bm = a.blds.instanceMatrix.array, bc = a.beac.instanceMatrix.array;
        var u = node.mesh.material.uniforms, lo = col3(u.uColLow.value), hi = col3(u.uColHigh.value), ac = col3(u.uAccent.value);
        for (i = 0; i < found.length; i++) {
            var f = found[i], d = f.d, id = node.id + '-op' + i;
            pads.push({ d: d, flat: flatA, blend: blendA, h: f.h });
            var pu = a.patchU;
            pu.uPadD.value[i].copy(d); pu.uPadP.value[i].set(flatA, blendA, f.h);
            var yaw = hash01(f.k, 11, sd, 5) * 6.2831853, cy2 = Math.cos(yaw), sy2 = Math.sin(yaw);
            var ux = f.e1[0] * cy2 + f.e2[0] * sy2, uy = f.e1[1] * cy2 + f.e2[1] * sy2, uz = f.e1[2] * cy2 + f.e2[2] * sy2;   // pad X (toward building)
            var vx = -f.e1[0] * sy2 + f.e2[0] * cy2, vy = -f.e1[1] * sy2 + f.e2[1] * cy2, vz = -f.e1[2] * sy2 + f.e2[2] * cy2;
            var rad = R * (1 + f.h), sp = PAD_L * L, o = i * 16, m4 = new THREE.Matrix4();
            // pad instance: X/Z = hex radius, Y = 1 L, lifted a hair above the flat ground
            var lift = 0.1 * L;
            pm[o] = ux * sp; pm[o + 1] = uy * sp; pm[o + 2] = uz * sp; pm[o + 3] = 0;
            pm[o + 4] = d.x * L; pm[o + 5] = d.y * L; pm[o + 6] = d.z * L; pm[o + 7] = 0;
            pm[o + 8] = vx * sp; pm[o + 9] = vy * sp; pm[o + 10] = vz * sp; pm[o + 11] = 0;
            pm[o + 12] = d.x * (rad + lift); pm[o + 13] = d.y * (rad + lift); pm[o + 14] = d.z * (rad + lift); pm[o + 15] = 1;
            var bs = BLD_S * L, off = BLD_OFF * L;
            bm[o] = ux * bs; bm[o + 1] = uy * bs; bm[o + 2] = uz * bs; bm[o + 3] = 0;
            bm[o + 4] = d.x * bs; bm[o + 5] = d.y * bs; bm[o + 6] = d.z * bs; bm[o + 7] = 0;
            bm[o + 8] = vx * bs; bm[o + 9] = vy * bs; bm[o + 10] = vz * bs; bm[o + 11] = 0;
            bm[o + 12] = d.x * rad + ux * off; bm[o + 13] = d.y * rad + uy * off; bm[o + 14] = d.z * rad + uz * off; bm[o + 15] = 1;
            var bi = i * 16, top = rad + 7 * BLD_S * L;                                     // beacon floats above the mast
            var bpx = d.x * (rad + 11 * BLD_S * L) + ux * off, bpy = d.y * (rad + 11 * BLD_S * L) + uy * off, bpz = d.z * (rad + 11 * BLD_S * L) + uz * off;
            bc[bi] = 1; bc[bi + 1] = 0; bc[bi + 2] = 0; bc[bi + 3] = 0; bc[bi + 4] = 0; bc[bi + 5] = 1; bc[bi + 6] = 0; bc[bi + 7] = 0;
            bc[bi + 8] = 0; bc[bi + 9] = 0; bc[bi + 10] = 1; bc[bi + 11] = 0; bc[bi + 12] = bpx; bc[bi + 13] = bpy; bc[bi + 14] = bpz; bc[bi + 15] = 1;
            a.beacPh.array[i] = hash01(f.k, 13, sd, 6) * 6.28;
            var tt = 0.3 + 0.5 * hash01(f.k, 14, sd, 7);
            for (var qq = 0; qq < 3; qq++) {
                a.padCol.array[i * 3 + qq] = hi[qq] * 0.3 + lo[qq] * 0.2 + 0.08;
                a.bldCol.array[i * 3 + qq] = (lo[qq] * (1 - tt) + hi[qq] * tt) * 0.72 + 0.1;
            }
            // pad Object3D (riding the globe): X = toward building, Y = up; children expose the NPC spot via its world offset
            var padO = new THREE.Object3D();
            m4.makeBasis(new THREE.Vector3(ux, uy, uz), new THREE.Vector3(d.x, d.y, d.z), new THREE.Vector3(vx, vy, vz));
            padO.quaternion.setFromRotationMatrix(m4);
            padO.position.set(d.x * rad, d.y * rad, d.z * rad);
            padO.updateMatrix(); a.ride.add(padO);
            ps.outposts.push({
                id: id, name: OUT_T[Math.floor(hash01(f.k, 15, sd, 8) * OUT_T.length)] + ' ' + OUT_A[Math.floor(hash01(f.k, 16, sd, 9) * OUT_A.length)] + OUT_B[Math.floor(hash01(f.k, 17, sd, 10) * OUT_B.length)],
                pad: padO, dir: d.clone(), radius: PAD_L * L, pos: new THREE.Vector3(), n: new THREE.Vector3(0, 1, 0), axis: new THREE.Vector3(1, 0, 0),
                vel: new THREE.Vector3(), prev: new THREE.Vector3(), prevOk: false,
                npcSpot: { pos: new THREE.Vector3(), facing: new THREE.Vector3(-1, 0, 0) }
            });
        }
        a.pads.count = a.blds.count = a.beac.count = found.length;
        a.pads.instanceMatrix.needsUpdate = a.blds.instanceMatrix.needsUpdate = a.beac.instanceMatrix.needsUpdate = true;
        a.padCol.needsUpdate = a.bldCol.needsUpdate = a.beacPh.needsUpdate = true;
        a.beacU.uR.value = R; outR = R;
        a.beacU.uCol.value.setRGB(ac[0], ac[1], ac[2]);
        rideKey[0] = NaN; syncRide();
    }
    // ─── rev 27: landmarks. Layout (direction space, seeded per planet id) is cached; matrices rebuild on every (re)seed so a pilot rescale re-lays them at the new radius.
    var lmCache = {}, lmList = [], bioC = [0.6, 1, 0.8], ffC = new THREE.Vector3(), ffSet = false, ffLook = -1, hazeClear = 0;
    var hC = new THREE.Vector3(0, 1, 0), hT1 = new THREE.Vector3(1, 0, 0), hT2 = new THREE.Vector3(0, 0, 1), ringQ = new THREE.Quaternion();
    function bioColor() {
        var ac = col3(node.mesh.material.uniforms.uAccent.value), m = Math.max(ac[0], ac[1], ac[2], 1e-3);
        bioC[0] = ac[0] / m; bioC[1] = ac[1] / m; bioC[2] = ac[2] / m; return bioC;
    }
    function lmLayout() {
        var c = lmCache[node.id]; if (c) return c;
        var sd = nodeSeed(node) + 90210, nL = 2 + Math.floor(hash01(sd, 1, 2, 3) * 3), res = [], i, k, g;
        var u = node.mesh.material.uniforms, lo = col3(u.uColLow.value), hi = col3(u.uColHigh.value), hue = hueOf(lo, hi);
        var exotic = look === 2 || (hue >= 255 && hue <= 350);
        var pool = [0, 1, 3, 4, 5]; if (exotic) pool.push(2);
        for (i = pool.length - 1; i > 0; i--) { var j = Math.floor(hash01(sd, i, 7, 7) * (i + 1)), tq = pool[i]; pool[i] = pool[j]; pool[j] = tq; }
        if (exotic) { pool.splice(pool.indexOf(2), 1); pool.unshift(2); }
        var minSep = Math.max(0.06, 320 * L / R), rr = 14 * L / R;
        for (i = 0; i < nL; i++) {
            var best = null, bs = 1e9;
            for (k = 0; k < 160; k++) {
                var z = hash01(k, 21 + i, sd, 1) * 2 - 1, ph = hash01(k, 31 + i, sd, 2) * 6.2831853, rq = Math.sqrt(1 - z * z);
                var dx = rq * Math.cos(ph), dy = z, dz = rq * Math.sin(ph), h = hFrac(dx, dy, dz), nn = _lastN;
                if (!(nn > sea + 0.04 && nn < sea + 0.2)) continue;
                if (pads.length && nearPad(dx, dy, dz, 260)) continue;
                var near = false;
                for (g = 0; g < res.length; g++) if (Math.hypot(dx - res[g].d.x, dy - res[g].d.y, dz - res[g].d.z) < minSep) { near = true; break; }
                if (near) continue;
                var ref0 = Math.abs(dy) < 0.9, e1x, e1y, e1z;
                if (ref0) { e1x = dz; e1y = 0; e1z = -dx; } else { e1x = 0; e1y = -dz; e1z = dy; }
                var el = Math.hypot(e1x, e1y, e1z); e1x /= el; e1y /= el; e1z /= el;
                var e2x = e1y * dz - e1z * dy, e2y = e1z * dx - e1x * dz, e2z = e1x * dy - e1y * dx;
                var maxd = 0, hmin = h, wet = false;
                for (g = 0; g < 5; g++) {
                    var an = g / 5 * 6.2832, px = dx + (e1x * Math.cos(an) + e2x * Math.sin(an)) * rr, py = dy + (e1y * Math.cos(an) + e2y * Math.sin(an)) * rr, pz = dz + (e1z * Math.cos(an) + e2z * Math.sin(an)) * rr, pl = Math.hypot(px, py, pz);
                    var hh = hFrac(px / pl, py / pl, pz / pl); if (_lastN < sea + 0.012) { wet = true; break; }
                    maxd = Math.max(maxd, Math.abs(hh - h)); hmin = Math.min(hmin, hh);
                }
                if (wet || maxd / rr >= bs) continue;
                bs = maxd / rr; best = { d: new THREE.Vector3(dx, dy, dz), h: h, hmin: hmin, e1: [e1x, e1y, e1z], e2: [e2x, e2y, e2z], k: k };
            }
            if (!best) continue;
            var kind = pool[i % pool.length];
            best.kind = kind; best.yaw = hash01(best.k, 41, sd, 3) * 6.2831853; best.sc = 0.9 + 0.35 * hash01(best.k, 42, sd, 4); best.name = lmName(sd + i * 977, kind); best.ph = hash01(best.k, 43, sd, 5) * 6.28;
            res.push(best);
        }
        lmCache[node.id] = res; return res;
    }
    function seedLandmarks() {
        var a = alloc; lmList = []; a.lm.count = 0; a.steamN = 0;
        a.ptsP.array.fill(0, FF_MAX * 4); a.ptsP.needsUpdate = true;
        if (!node || isGas || !seedV) return;
        var lay = lmLayout(), mat = a.lm.instanceMatrix.array, ca = a.lmCol.array, aa = a.lmAcc.array, u = node.mesh.material.uniforms;
        var lo = col3(u.uColLow.value), hi = col3(u.uColHigh.value), bc = bioColor(), pp = a.ptsPos.array, pa = a.ptsP.array, pc = a.ptsC.array, si = FF_MAX;
        for (var i = 0; i < lay.length; i++) {
            var c = lay[i], d = c.d, sc = L * c.sc, rad = R * (1 + Math.min(c.h, c.hmin)) - 0.4 * L, cy = Math.cos(c.yaw), sy = Math.sin(c.yaw);
            var ux = c.e1[0] * cy + c.e2[0] * sy, uy = c.e1[1] * cy + c.e2[1] * sy, uz = c.e1[2] * cy + c.e2[2] * sy;
            var vx = -c.e1[0] * sy + c.e2[0] * cy, vy = -c.e1[1] * sy + c.e2[1] * cy, vz = -c.e1[2] * sy + c.e2[2] * cy, o = i * 16;
            mat[o] = ux * sc; mat[o + 1] = uy * sc; mat[o + 2] = uz * sc; mat[o + 3] = 0;
            mat[o + 4] = d.x * sc; mat[o + 5] = d.y * sc; mat[o + 6] = d.z * sc; mat[o + 7] = 0;
            mat[o + 8] = vx * sc; mat[o + 9] = vy * sc; mat[o + 10] = vz * sc; mat[o + 11] = 0;
            mat[o + 12] = d.x * rad; mat[o + 13] = d.y * rad; mat[o + 14] = d.z * rad; mat[o + 15] = 1;
            a.lmSel.array[i] = c.kind; a.lmPh.array[i] = c.ph;
            for (var q = 0; q < 3; q++) {
                ca[i * 3 + q] = c.kind === 4 ? [0.34, 0.36, 0.4][q] : (c.kind === 2 ? (lo[q] * 0.4 + hi[q] * 0.6) * 0.8 + 0.12 : (lo[q] * 0.55 + hi[q] * 0.45) * 0.62 + 0.1);
                aa[i * 3 + q] = c.kind === 4 ? [1.0, 0.25, 0.15][q] : (c.kind === 3 ? Math.min(1, bc[q] * 0.9 + [0.3, 0.1, 0][q]) : Math.min(1, bc[q] * 0.8 + 0.2));
            }
            lmList.push({ id: node.id + '-lm' + i, kind: LM_KINDS[c.kind], name: c.name, lp: new THREE.Vector3(d.x * (R * (1 + c.h) + 7 * sc), d.y * (R * (1 + c.h) + 7 * sc), d.z * (R * (1 + c.h) + 7 * sc)) });
            if (c.kind === 3) {
                for (var vi = 0; vi < LM_VENTS.length; vi++) {
                    var vxl = LM_VENTS[vi][0] * sc, vzl = LM_VENTS[vi][1] * sc, base = rad + 1.7 * sc;
                    for (var pj = 0; pj < 6 && si < FF_MAX + STEAM_MAX; pj++, si++) {
                        pp[si * 3] = d.x * base + ux * vxl + vx * vzl; pp[si * 3 + 1] = d.y * base + uy * vxl + vy * vzl; pp[si * 3 + 2] = d.z * base + uz * vxl + vz * vzl;
                        pa[si * 4] = 1; pa[si * 4 + 1] = (pj / 6 + vi * 0.137 + c.ph * 0.01) % 1; pa[si * 4 + 2] = 1; pa[si * 4 + 3] = 2.6;
                        pc[si * 3] = pc[si * 3 + 1] = pc[si * 3 + 2] = 1;
                    }
                }
            }
        }
        a.steamN = si - FF_MAX;
        a.lm.count = lay.length;
        a.lm.instanceMatrix.needsUpdate = a.lmSel.needsUpdate = a.lmCol.needsUpdate = a.lmAcc.needsUpdate = a.lmPh.needsUpdate = true;
        a.ptsPos.needsUpdate = a.ptsP.needsUpdate = a.ptsC.needsUpdate = true;
        a.pts.geometry.setDrawRange(0, FF_MAX + STEAM_MAX);
    }
    ps.landmarks = function () {
        var out = [];
        if (!node || !alloc) return out;
        rideKey[0] = NaN; syncRide();
        for (var i = 0; i < lmList.length; i++) out.push({ id: lmList[i].id, kind: lmList[i].kind, pos: lmList[i].lp.clone().applyMatrix4(alloc.ride.matrix), name: lmList[i].name });
        return out;
    };
    // night fireflies: lattice-hashed on the shell around the player (stable per place), land only, <= 400 (Q_FF by tier), drifting in the vertex shader
    function seedFireflies() {
        var a = alloc, cell = 6 * L, E = 90 * L, m = Math.ceil(E / cell), cx = ffC.x * R, cy = ffC.y * R, cz = ffC.z * R;
        var ci = Math.floor(cx / cell), cj = Math.floor(cy / cell), ck = Math.floor(cz / cell), seed = nodeSeed(node) + 5150, cands = [], i, j, k;
        for (i = ci - m; i <= ci + m; i++) for (j = cj - m; j <= cj + m; j++) for (k = ck - m; k <= ck + m; k++) {
            var px = (i + hash01(i, j, k, seed + 1)) * cell, py = (j + hash01(i, j, k, seed + 2)) * cell, pz = (k + hash01(i, j, k, seed + 3)) * cell, rl = Math.hypot(px, py, pz);
            if (Math.abs(rl - R) > cell * 0.5) continue;
            cands.push([(px - cx) * (px - cx) + (py - cy) * (py - cy) + (pz - cz) * (pz - cz), px / rl, py / rl, pz / rl, i, j, k]);
        }
        cands.sort(function (p, q) { return p[0] - q[0]; });
        var cap = Math.round(Q_FF[qTier] * (look === 0 ? 0.6 : 1)), n = 0, pp = a.ptsPos.array, pa = a.ptsP.array, pc = a.ptsC.array, bc = bioColor();
        var tint = look === 1 ? [0.7, 1.0, 0.45] : (look === 2 ? [0.55, 0.85, 1.0] : [1.0, 0.6, 0.25]);
        for (var c = 0; c < cands.length && n < cap; c++) {
            var cd = cands[c], h = hFrac(cd[1], cd[2], cd[3]);
            if (_lastN < sea + 0.02) continue;
            var r1 = hash01(cd[4], cd[5], cd[6], seed + 4), r2 = hash01(cd[4], cd[5], cd[6], seed + 5), r3 = hash01(cd[4], cd[5], cd[6], seed + 6), rad = R * (1 + h) + (0.5 + 3 * r1) * L;
            pp[n * 3] = cd[1] * rad; pp[n * 3 + 1] = cd[2] * rad; pp[n * 3 + 2] = cd[3] * rad;
            pa[n * 4] = 0; pa[n * 4 + 1] = r2; pa[n * 4 + 2] = 0.6 + r3; pa[n * 4 + 3] = 0.11 + 0.07 * r3;
            for (var q = 0; q < 3; q++) pc[n * 3 + q] = (bc[q] * 0.5 + tint[q] * 0.5) * (0.8 + 0.4 * r1);
            n++;
        }
        for (var z = n; z < FF_MAX; z++) pa[z * 4 + 3] = 0;
        a.ffN = n; a.ptsPos.needsUpdate = a.ptsP.needsUpdate = a.ptsC.needsUpdate = true; ffSet = true;
    }
    // rev 21: ship-world.js (stores, shards, NPCs) rides on the active planet: ps.attachWorld(world) -> world.attach(node) on near-mode entry, world.detach() on leave
    ps.world = null;
    var worldOn = false;
    function worldAttach() {
        var w = ps.world; if (!w || worldOn || !node || !alloc) return;
        try { w.attach(node); if (w.group) alloc.ride.add(w.group); worldOn = true; } catch (e) { console.info('[ship-planet] world attach failed', e); }
    }
    function worldDetach() {
        var w = ps.world; if (!w || !worldOn) return;
        worldOn = false;
        try { if (w.group && w.group.parent) w.group.parent.remove(w.group); w.detach(); } catch (e) { console.info('[ship-planet] world detach failed', e); }
    }
    ps.attachWorld = function (world) {
        if (ps.world && ps.world !== world) worldDetach();
        ps.world = world || null;
        if (world && node) worldAttach();
    };
    function clearOutposts() {
        for (var i = 0; i < ps.outposts.length; i++) if (ps.outposts[i].pad.parent) ps.outposts[i].pad.parent.remove(ps.outposts[i].pad);
        ps.outposts = []; pads = [];
        if (alloc) { alloc.pads.count = alloc.blds.count = alloc.beac.count = 0; for (var q = 0; q < NPAD; q++) alloc.patchU.uPadP.value[q].set(0, 0, 0); }
    }

    function setNodeUniforms() {
        var u = node.mesh.material.uniforms, pu = alloc.patchU;
        // reference the planet's own uniform values so the look can never drift
        pu.uSeed = u.uSeed; pu.uColHigh = u.uColHigh; pu.uColLow = u.uColLow; pu.uColSea = u.uColSea; pu.uAtmo = u.uAtmo;
        pu.uAccent = u.uAccent; pu.uBiome = u.uBiome; pu.uFreq = u.uFreq; pu.uWarp = u.uWarp; pu.uBandFreq = u.uBandFreq;
        pu.uSeaLevel = u.uSeaLevel; pu.uSpeckle = u.uSpeckle; pu.uLightDir = u.uLightDir;
        alloc.patchMat.uniforms = pu; alloc.hazeMat.uniforms = pu; alloc.lmU.uLightDir = u.uLightDir;
        SH.uLightDir.value = u.uLightDir.value;
        alloc.floraU.uLightDir = u.uLightDir; alloc.creatU.uLightDir = u.uLightDir; alloc.rockU.uLightDir = u.uLightDir;
        alloc.patchMat.uniformsNeedUpdate = true; alloc.hazeMat.uniformsNeedUpdate = true;
        seedV = u.uSeed.value; freq = u.uFreq.value; warp = u.uWarp.value; sea = u.uSeaLevel.value; seaH = sea; pu.uSeaH.value = sea;
        isGas = u.uBiome.value > 0.5;
        amp = isGas ? AMP_GAS : AMP; bias = isGas ? GAS_DECK : BIAS;
        pu.uAmp.value = amp; pu.uBias.value = isGas ? 0 : bias; pu.uDetK.value = R / L;
        look = lookFromPalette(col3(u.uColLow.value), col3(u.uColHigh.value)); pu.uLook.value = look;
        var A = col3(u.uAtmo.value);
        alloc.domeU.uAtmo.value.setRGB(A[0], A[1], A[2]);
        floraSet = false; rockSet = false; sporeSet = false; ffSet = false;
    }

    // halo: updateBodies() rewrites the atmo uAlpha / glow opacity+scale every frame AFTER the pilot, so the
    // altitude boost is applied from onBeforeRender hooks (engine value captured once per rewrite, no compounding).
    function hookHalo(n) {
        var atmo = null, i, ch = n.mesh.children;
        for (i = 0; i < ch.length; i++) if (ch[i].material === n.atmoMat) atmo = ch[i];
        var gs = n.glowSprite, h = { n: n, atmo: atmo, gs: gs, oa: atmo && atmo.onBeforeRender, og: gs && gs.onBeforeRender,
            baseA: 0, lastA: -1, baseO: 0, lastO: -1, baseS: 0, lastS: -1 };
        if (atmo) atmo.onBeforeRender = function () {
            var u = n.atmoMat.uniforms.uAlpha;
            if (u.value !== h.lastA) h.baseA = u.value;
            h.lastA = u.value = Math.min(1.6, (h.baseA * (1 + 0.7 * haloT) + 0.1 * haloT) * haloAlphaK);
        };
        if (gs) gs.onBeforeRender = function () {
            var m = gs.material, sx = gs.scale.x;
            if (m.opacity !== h.lastO) h.baseO = m.opacity;
            if (sx !== h.lastS) h.baseS = sx;
            h.lastO = m.opacity = Math.min(1, (h.baseO + 0.2 * haloT) * haloAlphaK);
            h.lastS = sx = h.baseS * (1 + 0.45 * haloT);
            gs.scale.set(sx, sx, 1); gs.updateMatrixWorld(true);
        };
        haloH = h;
    }
    function unhookHalo() {
        if (!haloH) return;
        var h = haloH;
        if (h.atmo) { h.atmo.onBeforeRender = h.oa; h.atmo.scale.setScalar(1.07); }
        if (h.gs) h.gs.onBeforeRender = h.og;
        haloH = null; haloT = 0; haloAlphaK = 1; rimI = 0; ps.shake = 0;
    }

    function showObjs(on) {
        if (!alloc) return;
        alloc.patch.visible = on; alloc.dome.visible = on; alloc.streaks.visible = on && (rimI > 0.002 || wxStreakA > 0.02);
        alloc.rain.visible = on && rainN > 0; alloc.aurora.visible = on && wxAurA > 0.01;
        for (var si = 0; si < NSCAR; si++) alloc.scars[si].mesh.visible = on && alloc.scars[si].ttl > 0;
        alloc.flora.visible = on && alloc.flora.count > 0; alloc.creat.visible = on && alloc.creat.count > 0; alloc.rocks.visible = on && alloc.rocks.count > 0; alloc.spores.visible = on && alloc.spores.count > 0;
        alloc.pads.visible = alloc.blds.visible = alloc.beac.visible = on && alloc.pads.count > 0;
        alloc.lm.visible = on && alloc.lm.count > 0; alloc.pts.visible = on && ptsVis; alloc.haze.visible = on && hazeVis;
    }
    var ptsVis = false, hazeVis = false;

    // rev 24: land/water balance. Per planet land fraction target 0.45-0.8 (seeded); the sea level is solved against a height histogram of
    // ~2000 sphere directions (cached per planet id) and written BACK to the painted globe's uSeaLevel so orbit and surface agree.
    // Pilot radius < 600 L or a single outpost => all land (sea below the lowest sampled height).
    var seaCache = {};
    ps.solveSea = function (n) {
        var u = n.mesh.material.uniforms;
        if (u.uBiome.value > 0.5) return null;
        var c = seaCache[n.id];
        if (!c) {
            var sd = nodeSeed(n), Rn = n.mesh.scale.x, frac = 0.45 + 0.35 * hash01(sd, 41, 43, 47);
            var want = 1 + Math.floor(hash01(sd + 31337, 1, 2, 3) * 3);
            var allLand = (Rn / L < 600) || want === 1, N = 2000, v = new Float32Array(N), sv = u.uSeed.value, fq = u.uFreq.value, wp = u.uWarp.value;
            for (var i = 0; i < N; i++) {
                var y = 1 - 2 * (i + 0.5) / N, rr = Math.sqrt(1 - y * y), ph = i * 2.399963229728653;
                v[i] = terraN(rr * Math.cos(ph), y, rr * Math.sin(ph), sv, fq, wp);
            }
            v.sort();
            var sl;
            if (allLand) sl = v[0] - 0.09;
            else sl = v[Math.min(N - 1, Math.max(0, Math.floor((1 - frac) * N)))];
            var landN = 0; for (var j = 0; j < N; j++) if (v[j] > sl) landN++;
            c = seaCache[n.id] = { sea: sl, target: allLand ? 1 : frac, frac: landN / N, allLand: allLand, old: u.uSeaLevel.value };
        }
        u.uSeaLevel.value = c.sea;
        return c;
    };
    ps.seaInfo = function (n) { return seaCache[(n || node || {}).id] || null; };

    function activate(n) {
        if (node && node !== n) { worldDetach(); unhookHalo(); killScars(); clearOutposts(); }
        node = n;
        if (!alloc) alloc = build();
        hookHalo(n);
        setNodeUniforms();
        patchSet = false; floraSet = false; creatSet = false; sporeSet = false;
        ps.active = n; R = n.mesh.scale.x;
        if (!isGas) { var sc = ps.solveSea(n); if (sc) { sea = sc.sea; seaH = Math.max(sc.old, sc.sea); alloc.patchU.uSeaH.value = seaH; } }
        seedOutposts();
        seedLandmarks();
        worldAttach();
        wxSetup();
    }
    function deactivate() {
        worldDetach();
        unhookHalo(); killScars(); clearOutposts();
        node = null; ps.active = null; ps.depth = 0;
        if (alloc) { showObjs(false); alloc.domeU.uAlpha.value = 0; alloc.flora.count = 0; alloc.creat.count = 0; alloc.rocks.count = 0; alloc.spores.count = 0; rockSet = false; sporeSet = false; alloc.lm.count = 0; alloc.ffN = 0; alloc.steamN = 0; lmList = []; ffSet = false; }
    }

    function tangentFrame(c, t1, t2) {
        var ref = Math.abs(c.y) < 0.9 ? tmpA.set(0, 1, 0) : tmpA.set(1, 0, 0);
        t1.crossVectors(c, ref).normalize(); t2.crossVectors(c, t1).normalize();
    }

    // object-space (planet frame) unit direction of the world point
    function toObjDir(pos, out) {
        out.set(pos.x - node.anchor.position.x, pos.y - node.anchor.position.y, pos.z - node.anchor.position.z);
        qInv.copy(node.mesh.quaternion).invert();
        out.applyQuaternion(qInv);
        return out.normalize();
    }

    // ─── flora seeding (stable: lattice cells hashed on the sphere shell) ─────────
    var CELL_F = 9 * L;
    function seedFlora() {
        var a = alloc, Rr = R, cell = CELL_F, E = 200 * L, m = Math.ceil(E / cell);
        var cx = floraC.x * Rr, cy = floraC.y * Rr, cz = floraC.z * Rr;
        var ci = Math.floor(cx / cell), cj = Math.floor(cy / cell), ck = Math.floor(cz / cell);
        var seed = nodeSeed(node), cands = [];
        for (var i = ci - m; i <= ci + m; i++) for (var j = cj - m; j <= cj + m; j++) for (var k = ck - m; k <= ck + m; k++) {
            var px = (i + hash01(i, j, k, seed + 1)) * cell, py = (j + hash01(i, j, k, seed + 2)) * cell, pz = (k + hash01(i, j, k, seed + 3)) * cell;
            var rl = Math.hypot(px, py, pz);
            if (Math.abs(rl - Rr) > cell * 0.5) continue;           // one layer of cells, the shell
            var d2 = (px - cx) * (px - cx) + (py - cy) * (py - cy) + (pz - cz) * (pz - cz);
            cands.push([d2, px / rl, py / rl, pz / rl, i, j, k]);
        }
        cands.sort(function (p, q) { return p[0] - q[0]; });
        var cnt = 0, mat = a.flora.instanceMatrix.array, ca = a.floraCol.array, sa = a.floraSel.array;
        var lo = col3(node.mesh.material.uniforms.uColLow.value), hi = col3(node.mesh.material.uniforms.uColHigh.value), ac = col3(node.mesh.material.uniforms.uAccent.value);
        bioColor();
        var fCap = Math.round((look === 1 ? 700 : (look === 0 ? 150 : 60)) * floraLim / 700);
        for (var c = 0; c < cands.length && cnt < fCap; c++) {
            var cd = cands[c], dx = cd[1], dy = cd[2], dz = cd[3];
            var h = hFrac(dx, dy, dz);
            if (isGas || _lastN < sea + 0.03) continue;           // land only
            if (pads.length && nearPad(dx, dy, dz, 3)) continue;     // keep outposts clear
            if (_lastN > sea + 0.19) { if (hash01(cd[4], cd[5], cd[6], seed + 9) < 0.7) continue; }
            var rr = hash01(cd[4], cd[5], cd[6], seed + 4), r2 = hash01(cd[4], cd[5], cd[6], seed + 5), r3 = hash01(cd[4], cd[5], cd[6], seed + 6);
            var variant = look === 2 ? 3 : Math.floor(rr * 3) % 3, sh = (4 + r2 * 4) * L, sw = sh * (0.38 + r3 * 0.22) * (look === 2 ? 0.7 : 1);
            var rad = Rr * (1 + h) - 0.25 * L;
            // basis: right = d x ref, fwd = right x d, yaw by r3
            var ref0 = Math.abs(dy) < 0.9 ? 0 : 1;
            var rx, ry, rz;
            if (ref0 === 0) { rx = dz; ry = 0; rz = -dx; } else { rx = 0; ry = -dz; rz = dy; }   // d x ref
            var rl2 = Math.hypot(rx, ry, rz); rx /= rl2; ry /= rl2; rz /= rl2;
            var fx = ry * dz - rz * dy, fy = rz * dx - rx * dz, fz = rx * dy - ry * dx;                // right x d
            var yaw = r3 * 6.2832, cyw = Math.cos(yaw), syw = Math.sin(yaw);
            var ux = rx * cyw + fx * syw, uy = ry * cyw + fy * syw, uz = rz * cyw + fz * syw;
            var vx = -rx * syw + fx * cyw, vy = -ry * syw + fy * cyw, vz = -rz * syw + fz * cyw;
            var o = cnt * 16;
            mat[o] = ux * sw; mat[o + 1] = uy * sw; mat[o + 2] = uz * sw; mat[o + 3] = 0;
            mat[o + 4] = dx * sh; mat[o + 5] = dy * sh; mat[o + 6] = dz * sh; mat[o + 7] = 0;
            mat[o + 8] = vx * sw; mat[o + 9] = vy * sw; mat[o + 10] = vz * sw; mat[o + 11] = 0;
            mat[o + 12] = dx * rad; mat[o + 13] = dy * rad; mat[o + 14] = dz * rad; mat[o + 15] = 1;
            // palette tint: lerp lo/hi, with an occasional accent-coloured tree
            var tt = 0.25 + 0.75 * r2, base = hash01(cd[4], cd[5], cd[6], seed + 7) < 0.12 ? ac : null;
            for (var q = 0; q < 3; q++) ca[cnt * 3 + q] = look === 2 ? [0.62, 0.8, 0.98][q] * (0.8 + 0.3 * r2) : ((base ? base[q] * 0.5 : (lo[q] * (1 - tt) + hi[q] * tt)) * 0.82 + 0.02);
            sa[cnt] = variant;
            var bio = hash01(cd[4], cd[5], cd[6], seed + 11) < (look === 1 ? 0.38 : (look === 2 ? 0.3 : 0.14)) ? 0.6 + 0.4 * r3 : 0, bb = a.floraBio.array;
            bb[cnt * 3] = bioC[0] * bio; bb[cnt * 3 + 1] = bioC[1] * bio; bb[cnt * 3 + 2] = bioC[2] * bio;
            cnt++;
        }
        a.flora.count = cnt;
        a.flora.instanceMatrix.needsUpdate = true; a.floraCol.needsUpdate = true; a.floraSel.needsUpdate = true; a.floraBio.needsUpdate = true;
        floraSet = true;
    }

    // ─── rocks: lattice-hashed on the shell like the flora (stable per place), land only, smaller cells, size 0.6-2.2 L with the odd boulder
    function seedRocks() {
        var a = alloc, Rr = R, cell = 9 * L, E = 130 * L, m = Math.ceil(E / cell);
        var cx = rockC.x * Rr, cy = rockC.y * Rr, cz = rockC.z * Rr;
        var ci = Math.floor(cx / cell), cj = Math.floor(cy / cell), ck = Math.floor(cz / cell);
        var seed = nodeSeed(node) + 4242, cands = [];
        for (var i = ci - m; i <= ci + m; i++) for (var j = cj - m; j <= cj + m; j++) for (var k = ck - m; k <= ck + m; k++) {
            var px = (i + hash01(i, j, k, seed + 1)) * cell, py = (j + hash01(i, j, k, seed + 2)) * cell, pz = (k + hash01(i, j, k, seed + 3)) * cell;
            var rl = Math.hypot(px, py, pz);
            if (Math.abs(rl - Rr) > cell * 0.5) continue;
            var d2 = (px - cx) * (px - cx) + (py - cy) * (py - cy) + (pz - cz) * (pz - cz);
            cands.push([d2, px / rl, py / rl, pz / rl, i, j, k]);
        }
        cands.sort(function (p, q) { return p[0] - q[0]; });
        var cnt = 0, mat = a.rocks.instanceMatrix.array, ca = a.rockCol.array;
        var u = node.mesh.material.uniforms, lo = col3(u.uColLow.value), hi = col3(u.uColHigh.value); bioColor();
        for (var c = 0; c < cands.length && cnt < rockLim; c++) {
            var cd = cands[c], dx = cd[1], dy = cd[2], dz = cd[3];
            var h = hFrac(dx, dy, dz);
            if (isGas || _lastN < sea + 0.004) continue;                      // dry land only
            if (pads.length && nearPad(dx, dy, dz, 3)) continue;
            var r1 = hash01(cd[4], cd[5], cd[6], seed + 4), r2 = hash01(cd[4], cd[5], cd[6], seed + 5), r3 = hash01(cd[4], cd[5], cd[6], seed + 6);
            var big = r1 > 0.93 ? 1.4 : 1, sw = (0.3 + r2 * 1.2) * L * big, sh = sw * (0.55 + r3 * 0.5);
            var rad = Rr * (1 + h) - 0.2 * sh;
            var ref0 = Math.abs(dy) < 0.9 ? 0 : 1, rx, ry, rz;
            if (ref0 === 0) { rx = dz; ry = 0; rz = -dx; } else { rx = 0; ry = -dz; rz = dy; }
            var rl2 = Math.hypot(rx, ry, rz); rx /= rl2; ry /= rl2; rz /= rl2;
            var fx = ry * dz - rz * dy, fy = rz * dx - rx * dz, fz = rx * dy - ry * dx;
            var yaw = r3 * 6.2832, cyw = Math.cos(yaw), syw = Math.sin(yaw);
            var ux = rx * cyw + fx * syw, uy = ry * cyw + fy * syw, uz = rz * cyw + fz * syw;
            var vx = -rx * syw + fx * cyw, vy = -ry * syw + fy * cyw, vz = -rz * syw + fz * cyw;
            var o = cnt * 16;
            mat[o] = ux * sw; mat[o + 1] = uy * sw; mat[o + 2] = uz * sw; mat[o + 3] = 0;
            mat[o + 4] = dx * sh; mat[o + 5] = dy * sh; mat[o + 6] = dz * sh; mat[o + 7] = 0;
            mat[o + 8] = vx * sw * (0.7 + 0.5 * r1); mat[o + 9] = vy * sw * (0.7 + 0.5 * r1); mat[o + 10] = vz * sw * (0.7 + 0.5 * r1); mat[o + 11] = 0;
            mat[o + 12] = dx * rad; mat[o + 13] = dy * rad; mat[o + 14] = dz * rad; mat[o + 15] = 1;
            var tt = 0.7 + 0.6 * r2, hi2 = hFrac(dx, dy, dz) > 0.07;
            for (var q = 0; q < 3; q++) {
                var base = look === 2 ? [0.62, 0.7, 0.8][q] : (look === 1 ? [0.36, 0.35, 0.34][q] : (lo[q] * 0.35 + hi[q] * 0.25 + 0.1));
                ca[cnt * 3 + q] = Math.min(1, (base * 0.72 + 0.04) * tt * (hi2 ? 1.15 : 1));
            }
            var rb = hash01(cd[4], cd[5], cd[6], seed + 11) < (look === 1 ? 0.25 : (look === 2 ? 0.2 : 0.12)) ? 0.5 + 0.5 * r3 : 0, rbb = a.rockBio.array;
            rbb[cnt * 3] = bioC[0] * rb; rbb[cnt * 3 + 1] = bioC[1] * rb; rbb[cnt * 3 + 2] = bioC[2] * rb;
            cnt++;
        }
        a.rocks.count = cnt;
        a.rocks.instanceMatrix.needsUpdate = true; a.rockCol.needsUpdate = true; a.rockBio.needsUpdate = true;
        rockSet = true;
    }

    // ─── creature seeding + animation ──────────────────────────────────────────────
    var tamedMap = {}, BEH = ['grazer', 'skitterer', 'floater', 'strider'], KIND = ['walker', 'jelly', 'ox', 'strider'];
    function copyCd(d, s2) { for (var k in s2) { var v = s2[k]; if (v && v.isVector3) d[k].copy(v); else if (k === 'col') { d.col[0] = v[0]; d.col[1] = v[1]; d.col[2] = v[2]; } else d[k] = v; } }
    function seedCreatures() {
        var a = alloc, Rr = R, cell = 40 * L, E = 150 * L, m = Math.ceil(E / cell);
        var cx = creatC.x * Rr, cy = creatC.y * Rr, cz = creatC.z * Rr;
        var ci = Math.floor(cx / cell), cj = Math.floor(cy / cell), ck = Math.floor(cz / cell);
        var seed = nodeSeed(node) + 777, cands = [];
        for (var i = ci - m; i <= ci + m; i++) for (var j = cj - m; j <= cj + m; j++) for (var k = ck - m; k <= ck + m; k++) {
            var px = (i + hash01(i, j, k, seed + 1)) * cell, py = (j + hash01(i, j, k, seed + 2)) * cell, pz = (k + hash01(i, j, k, seed + 3)) * cell;
            var rl = Math.hypot(px, py, pz);
            if (Math.abs(rl - Rr) > cell * 0.5) continue;
            var d2 = (px - cx) * (px - cx) + (py - cy) * (py - cy) + (pz - cz) * (pz - cz);
            cands.push([d2, px / rl, py / rl, pz / rl, i, j, k]);
        }
        cands.sort(function (p, q) { return p[0] - q[0]; });
        var u = node.mesh.material.uniforms;
        var lo = col3(u.uColLow.value), hi = col3(u.uColHigh.value), ac = col3(u.uAccent.value), cnt = 0, q;
        var old = a.creatData, oldN = a.creat.count, nw = a.creatAlt, byId = {}, used = {}, i2;
        for (i2 = 0; i2 < oldN; i2++) byId[old[i2].id] = old[i2];
        for (i2 = 0; i2 < oldN && cnt < creatLim; i2++) if (old[i2].tamed) { copyCd(nw[cnt], old[i2]); nw[cnt].leader = -1; used[old[i2].key] = 1; cnt++; }     // tamed ones stay in the pool (they follow the player)
        for (var c = 0; c < cands.length && cnt < creatLim; c++) {
            var cd = cands[c], key = cd[4] + '_' + cd[5] + '_' + cd[6];
            if (used[key]) continue;
            var A = cd[4], B = cd[5], C2 = cd[6];
            var vr = hash01(A, B, C2, seed + 4);
            hFrac(cd[1], cd[2], cd[3]);
            var land = !isGas && _lastN > sea + 0.03, beh;
            if (!land || (pads.length && nearPad(cd[1], cd[2], cd[3], 20))) beh = 2; else if (vr < 0.5) beh = 0; else if (vr < 0.8) beh = 1; else if (vr < 0.9) beh = 3; else beh = 2;
            var members = 1;
            if (beh === 0) { members = 3 + Math.floor(hash01(A, B, C2, seed + 20) * 4); if (cnt + members > creatLim) { if (creatLim - cnt >= 3) members = creatLim - cnt; else { beh = 1; members = 1; } } }
            var lead = cnt, bodyV = beh === 0 ? (hash01(A, B, C2, seed + 21) > 0.45 ? 2 : 0) : beh === 1 ? 0 : beh === 2 ? 1 : 3, tt0 = hash01(A, B, C2, seed + 13);
            var head = hash01(A, B, C2, seed + 11) * 6.283;
            for (var mi = 0; mi < members; mi++) {
                var cr = nw[cnt], hm = hash01(A, B, C2, seed + 30 + mi);
                cr.id = mi ? key + '#' + mi : key; cr.key = key; cr.beh = beh; cr.v = bodyV; cr.kind = KIND[bodyV];
                cr.tamed = !!tamedMap[node.id + '|' + cr.id]; cr.leader = mi ? lead : -1; cr.glowT = 0; cr.spd = 0; cr.mov = 0; cr.fleeT = 0; cr.folT = -9; cr.want.set(0, 0, 0);
                cr.p.set(cd[1], cd[2], cd[3]); tangentFrame(cr.p, tmpA, tmpB);
                cr.h.copy(tmpA).multiplyScalar(Math.cos(head)).addScaledVector(tmpB, Math.sin(head));
                if (mi) {          // herd member: fixed offset in the leader's frame
                    var L0 = nw[lead], ang = mi / members * 6.283 + hash01(A, B, C2, seed + 22), dist = (5 + hm * 4) * L;
                    cr.ox = Math.cos(ang) * dist; cr.oz = Math.sin(ang) * dist;
                    _gv.crossVectors(L0.p, L0.h);
                    cr.p.copy(L0.p).multiplyScalar(Rr).addScaledVector(L0.h, cr.ox).addScaledVector(_gv, cr.oz).normalize();
                    cr.h.copy(L0.h); cr.h.addScaledVector(cr.p, -cr.h.dot(cr.p)).normalize();
                }
                cr.pp.copy(cr.p);
                cr.r = Rr * (1 + hFrac(cr.p.x, cr.p.y, cr.p.z));
                cr.hover = bodyV === 1 ? (5 + hash01(A, B, C2, seed + 5) * 7) * L : 0;
                var szk = 0.8 + hm * 0.7;
                cr.sz = (bodyV === 1 ? 4.5 : bodyV === 2 ? 3.4 : bodyV === 3 ? 3.6 : beh === 1 ? 2.6 : 3.5) * L * szk;
                cr.vmax = (beh === 0 ? 2.0 : beh === 1 ? 14 : beh === 2 ? 1.4 : 2.4) * L;
                cr.flee = (beh === 0 ? 10 : beh === 1 ? 6 : beh === 3 ? 8 : 0) * L;
                cr.px = hash01(A, B, C2, seed + 11 + mi) * 6.28; cr.tm = hash01(A, B, C2, seed + 24 + mi) * 4; cr.paused = hash01(A, B, C2, seed + 25) > 0.5;
                var tt = beh === 0 ? Math.min(1, Math.max(0, tt0 + (hm - 0.5) * 0.15)) : hm;
                for (q = 0; q < 3; q++) cr.col[q] = bodyV === 1 ? (ac[q] * 0.7 + hi[q] * 0.3) * 1.1 + 0.1 : (lo[q] * (1 - tt) + hi[q] * tt) * 0.9 + 0.12 + (bodyV === 3 ? ac[q] * 0.15 : 0);
                var o = byId[cr.id];
                if (o) { cr.p.copy(o.p); cr.h.copy(o.h); cr.pp.copy(o.pp); cr.spd = o.spd; cr.tm = o.tm; cr.paused = o.paused; cr.fleeT = o.fleeT; cr.glowT = o.glowT; cr.r = o.r; }
                cnt++;
            }
        }
        a.creatAlt = old; a.creatData = nw;
        for (i2 = 0; i2 < cnt; i2++) {
            var z = nw[i2];
            a.creatCol.array[i2 * 3] = z.col[0]; a.creatCol.array[i2 * 3 + 1] = z.col[1]; a.creatCol.array[i2 * 3 + 2] = z.col[2];
            a.creatSel.array[i2] = z.v; a.creatPh.array[i2] = (z.px * 7.31) % 6.28;
        }
        a.creat.count = cnt;
        a.creatCol.needsUpdate = true; a.creatSel.needsUpdate = true; a.creatPh.needsUpdate = true;
        creatSet = true;
    }

    // ─── spores (gas giants): 60 nearest lattice cells around the ship, hovering 3-17 L above the cloud tops, 1-3 L across
    function seedSpores() {
        var a = alloc, Rr = R, cell = 14 * L, E = 100 * L, m = Math.ceil(E / cell);
        var cx = sporeC.x * Rr, cy = sporeC.y * Rr, cz = sporeC.z * Rr;
        var ci = Math.floor(cx / cell), cj = Math.floor(cy / cell), ck = Math.floor(cz / cell);
        var seed = nodeSeed(node) + 9191, cands = [];
        for (var i = ci - m; i <= ci + m; i++) for (var j = cj - m; j <= cj + m; j++) for (var k = ck - m; k <= ck + m; k++) {
            var px = (i + hash01(i, j, k, seed + 1)) * cell, py = (j + hash01(i, j, k, seed + 2)) * cell, pz = (k + hash01(i, j, k, seed + 3)) * cell;
            var rl = Math.hypot(px, py, pz);
            if (Math.abs(rl - Rr) > cell * 0.5) continue;
            var d2 = (px - cx) * (px - cx) + (py - cy) * (py - cy) + (pz - cz) * (pz - cz);
            cands.push([d2, px / rl, py / rl, pz / rl, i, j, k]);
        }
        cands.sort(function (p, q) { return p[0] - q[0]; });
        var u = node.mesh.material.uniforms, hi = col3(u.uColHigh.value), ac = col3(u.uAccent.value), cnt = 0;
        var mat = a.spores.instanceMatrix.array;
        for (var c = 0; c < cands.length && cnt < SPORE_MAX; c++) {
            var cd = cands[c], h = hFrac(cd[1], cd[2], cd[3]);
            var r1 = hash01(cd[4], cd[5], cd[6], seed + 4), r2 = hash01(cd[4], cd[5], cd[6], seed + 5), r3 = hash01(cd[4], cd[5], cd[6], seed + 6);
            var sz = (1 + r1 * 2) * L, rad = Rr * (1 + h) + (3 + r2 * 14) * L, o = cnt * 16;
            mat[o] = sz; mat[o + 1] = 0; mat[o + 2] = 0; mat[o + 3] = 0; mat[o + 4] = 0; mat[o + 5] = sz; mat[o + 6] = 0; mat[o + 7] = 0;
            mat[o + 8] = 0; mat[o + 9] = 0; mat[o + 10] = sz; mat[o + 11] = 0;
            mat[o + 12] = cd[1] * rad; mat[o + 13] = cd[2] * rad; mat[o + 14] = cd[3] * rad; mat[o + 15] = 1;
            for (var q = 0; q < 3; q++) a.sporeCol.array[cnt * 3 + q] = Math.min(1.0, (ac[q] * (0.35 + 0.45 * r3) + hi[q] * (0.3 - 0.15 * r3)) * 0.9 + 0.04);
            a.sporeSp.array[cnt * 3] = r3 * 6.283; a.sporeSp.array[cnt * 3 + 1] = (1.2 + r1 * 3) * L; a.sporeSp.array[cnt * 3 + 2] = 0.5 + r2 * 0.9;
            cnt++;
        }
        a.spores.count = cnt;
        a.spores.instanceMatrix.needsUpdate = true; a.sporeCol.needsUpdate = true; a.sporeSp.needsUpdate = true;
        sporeSet = true;
    }
    var _pv = new THREE.Vector3(), _hv = new THREE.Vector3(), _gv = new THREE.Vector3(), _tv = new THREE.Vector3(), plObj = new THREE.Vector3(), plDir = new THREE.Vector3(), plOk = false;
    function rotH(cd, ang) { _gv.crossVectors(cd.p, cd.h); cd.h.multiplyScalar(Math.cos(ang)).addScaledVector(_gv, Math.sin(ang)); }
    function steer(cd, hd, rate, dt) {
        _gv.crossVectors(cd.p, cd.h);
        var ang = Math.atan2(_gv.dot(hd), cd.h.dot(hd)), mx = rate * dt;
        rotH(cd, Math.max(-mx, Math.min(mx, ang)));
    }
    function tangentTo(cd, target, out) {      // unit tangent at cd.p toward the unit direction `target`; returns ground distance
        out.copy(target).addScaledVector(cd.p, -cd.p.dot(target)); var l = out.length();
        if (l < 1e-9) { out.copy(cd.h); return 0; }
        out.multiplyScalar(1 / l); return l * R;
    }
    function animCreatures(dt) {
        var a = alloc, N = a.creat.count; if (!N) return;
        dt = Math.min(dt, 0.1);
        var mat = a.creat.instanceMatrix.array, Rr = R, pool = a.creatData, cd, i;
        // refresh the ground radius of a few creatures per frame (round robin); walkers turn back from water
        for (var rr = 0; rr < 8 && N; rr++) {
            a.creatRR = (a.creatRR + 1) % N; cd = pool[a.creatRR];
            cd.r = Rr * (1 + hFrac(cd.p.x, cd.p.y, cd.p.z));
            if (cd.beh !== 2 && !cd.tamed && !isGas && _lastN < sea + 0.008) { cd.h.multiplyScalar(-1); cd.p.copy(cd.pp); }
        }
        for (i = 0; i < N; i++) {
            cd = pool[i];
            var fol = cd.tamed && (t - cd.folT) < 0.35, rad, s = cd.sz;
            if (cd.tamed && !fol) { s = 0; rad = cd.r; }
            else if (fol) {
                _pv.copy(cd.folP); var fl = _pv.length() || 1; _pv.multiplyScalar(1 / fl);
                _tv.copy(cd.folP).sub(cd.folPrev); var mv = _tv.length(); cd.folPrev.copy(cd.folP);
                _tv.addScaledVector(_pv, -_tv.dot(_pv));
                if (_tv.length() > 0.02 * L) cd.h.lerp(_tv.normalize(), Math.min(1, dt * 8)); 
                cd.p.copy(_pv); cd.h.addScaledVector(cd.p, -cd.h.dot(cd.p)); if (cd.h.lengthSq() < 1e-9) cd.h.set(1, 0, 0).addScaledVector(cd.p, -cd.p.x); cd.h.normalize();
                cd.spd = mv / Math.max(dt, 1e-3); cd.mov = Math.min(1, cd.spd / (3 * L)); rad = fl;
            } else {
                var D = 1e9, sp = 0, hd = cd.h, rate = 1.5, gd;
                if (plOk) { D = _tv.copy(cd.p).multiplyScalar(cd.r + cd.hover).sub(plObj).length(); }
                var beh = cd.beh, ld = cd.leader >= 0 ? pool[cd.leader] : null;
                if (ld && ld.tamed) { ld = null; cd.leader = -1; }
                if (beh === 1) {                                                   // skitterer: darts, flees fast
                    if (D < cd.flee) { if (cd.fleeT <= 0) { cd.fleeT = 1.2; } else cd.fleeT = Math.max(cd.fleeT, 0.5); }
                    if (cd.fleeT > 0) { cd.fleeT -= dt; sp = cd.vmax * 1.15; tangentTo(cd, plDir, _hv); hd = _hv.multiplyScalar(-1); rate = 9; }
                    else {
                        cd.tm -= dt;
                        if (cd.tm <= 0) { cd.paused = !cd.paused; if (cd.paused) cd.tm = 0.8 + Math.random() * 2.2; else { cd.tm = 0.35 + Math.random() * 0.55; rotH(cd, (Math.random() - 0.5) * 4); } }
                        sp = cd.paused ? 0 : cd.vmax;
                    }
                } else if (beh === 2) {                                            // floater: slow drift, gathers toward low ground (water), curious about the player
                    cd.tm -= dt;
                    if (cd.tm <= 0) {
                        cd.tm = 2 + Math.random() * 2; _gv.crossVectors(cd.p, cd.h); var bestH = 1e9;
                        for (var q = 0; q < 4; q++) {
                            var an = Math.random() * 6.283, dd = 14 * L / Rr;
                            _pv.copy(cd.p).addScaledVector(cd.h, Math.cos(an) * dd).addScaledVector(_gv, Math.sin(an) * dd).normalize();
                            var hf = hFrac(_pv.x, _pv.y, _pv.z);
                            if (hf < bestH) { bestH = hf; cd.want.copy(cd.h).multiplyScalar(Math.cos(an)).addScaledVector(_gv, Math.sin(an)); }
                        }
                    }
                    rotH(cd, Math.sin(t * 0.3 + cd.px) * 0.2 * dt);
                    if (cd.want.lengthSq() > 0) { cd.want.addScaledVector(cd.p, -cd.want.dot(cd.p)); if (cd.want.lengthSq() > 1e-9) hd = cd.want.normalize(); }
                    rate = 0.4; sp = cd.vmax;
                    if (plOk && D < 30 * L) { tangentTo(cd, plDir, _hv); if (D > 6 * L) { hd = _hv; rate = 0.6; } else { hd = _hv.multiplyScalar(-1); rate = 1.2; } }
                } else if (ld) {                                                   // herd member: hold the formation slot
                    if (D < cd.flee && cd.fleeT <= 0) cd.fleeT = 3.5;
                    if (cd.fleeT > 0) { cd.fleeT -= dt; sp = cd.vmax * 2.4; tangentTo(cd, plDir, _hv); hd = _hv.multiplyScalar(-1); rate = 3; }
                    else {
                        _gv.crossVectors(ld.p, ld.h);
                        _pv.copy(ld.p).multiplyScalar(Rr).addScaledVector(ld.h, cd.ox).addScaledVector(_gv, cd.oz).normalize();
                        gd = tangentTo(cd, _pv, _hv); rate = 2.5;
                        if (gd > 1.5 * L) { hd = _hv; sp = Math.min(cd.vmax * 3, ld.spd + gd * 0.6); }
                        else { hd = ld.h; sp = ld.spd; }
                    }
                } else {                                                           // grazer leader / lone strider: walk-pause cycle, wander
                    if (cd.flee && D < cd.flee) cd.fleeT = Math.max(cd.fleeT, 3.5);
                    if (cd.fleeT > 0) { cd.fleeT -= dt; sp = cd.vmax * (beh === 3 ? 2.5 : 2.4); tangentTo(cd, plDir, _hv); hd = _hv.multiplyScalar(-1); rate = 3; cd.paused = false; }
                    else {
                        cd.tm -= dt;
                        if (cd.tm <= 0) { cd.paused = !cd.paused; cd.tm = cd.paused ? 4 + Math.random() * 5 : 6 + Math.random() * 6; if (!cd.paused) rotH(cd, (Math.random() - 0.5) * 1.6); }
                        sp = cd.paused ? 0 : cd.vmax; rotH(cd, Math.sin(t * 0.2 + cd.px) * 0.25 * dt);
                    }
                }
                cd.spd += (sp - cd.spd) * Math.min(1, dt * (beh === 1 ? 9 : 3));
                steer(cd, hd, rate, dt);
                var phi = cd.spd * dt / Rr;
                if (phi > 1e-9) {
                    cd.pp.copy(cd.p); var cc = Math.cos(phi), sn = Math.sin(phi);
                    _pv.copy(cd.p).multiplyScalar(cc).addScaledVector(cd.h, sn); _hv.copy(cd.h).multiplyScalar(cc).addScaledVector(cd.p, -sn);
                    cd.p.copy(_pv).normalize(); cd.h.copy(_hv).addScaledVector(cd.p, -_hv.dot(cd.p)).normalize();
                }
                cd.mov = Math.min(1, cd.spd / ((beh === 1 ? 6 : 2) * L)); rad = cd.r + cd.hover + (cd.v === 1 ? Math.sin(t * 1.3 + cd.px) * 1.6 * L : 0);
            }
            if (cd.glowT > 0) cd.glowT -= dt;
            a.creatGlow.array[i] = Math.min(1, Math.max(0, cd.glowT)); a.creatMov.array[i] = cd.mov;
            var dx = cd.p.x, dy = cd.p.y, dz = cd.p.z, fx = cd.h.x, fy = cd.h.y, fz = cd.h.z;
            if (fol) rad = rad;
            else if (!cd.tamed && cd.v !== 1) rad = cd.r;
            // right-handed basis: X = fwd (creature faces +x), Y = up, Z = X x Y
            var zx = fy * dz - fz * dy, zy = fz * dx - fx * dz, zz = fx * dy - fy * dx, o = i * 16;
            mat[o] = fx * s; mat[o + 1] = fy * s; mat[o + 2] = fz * s; mat[o + 3] = 0;
            mat[o + 4] = dx * s; mat[o + 5] = dy * s; mat[o + 6] = dz * s; mat[o + 7] = 0;
            mat[o + 8] = zx * s; mat[o + 9] = zy * s; mat[o + 10] = zz * s; mat[o + 11] = 0;
            mat[o + 12] = dx * rad; mat[o + 13] = dy * rad; mat[o + 14] = dz * rad; mat[o + 15] = 1;
        }
        a.creat.instanceMatrix.needsUpdate = true; a.creatGlow.needsUpdate = true; a.creatMov.needsUpdate = true;
    }

    // rev 26 public creature API
    function findCd(id) { if (!alloc) return null; for (var i = 0; i < alloc.creat.count; i++) if (alloc.creatData[i].id === id) return alloc.creatData[i]; return null; }
    ps.creatures = function () {
        var out = []; if (!alloc || !node) return out; syncRide();
        var r = alloc.ride;
        for (var i = 0; i < alloc.creat.count; i++) {
            var cd = alloc.creatData[i]; if (cd.tamed) continue;
            var v = new THREE.Vector3().copy(cd.p).multiplyScalar(cd.r + cd.hover + cd.sz * 0.4).applyQuaternion(r.quaternion).add(r.position);
            out.push({ id: cd.id, kind: cd.kind, beh: BEH[cd.beh], pos: v, tamed: false, planet: node.id });
        }
        return out;
    };
    ps.setCreatureTamed = function (id, on, planetId) {
        var pid = planetId || (node && node.id); if (!pid) return false;
        if (on) tamedMap[pid + '|' + id] = true; else delete tamedMap[pid + '|' + id];
        var cd = node && node.id === pid ? findCd(id) : null; if (cd) { cd.tamed = !!on; cd.folT = -9; }
        return true;
    };
    ps.setFollower = function (id, wp) {
        var cd = findCd(id); if (!cd || !cd.tamed || !alloc) return false;
        syncRide(); var r = alloc.ride;
        _tv.set(wp.x - r.position.x, wp.y - r.position.y, wp.z - r.position.z).applyQuaternion(qInv.copy(r.quaternion).invert());
        if (t - cd.folT > 0.35) cd.folPrev.copy(_tv);
        cd.folP.copy(_tv); cd.folT = t; return true;
    };
    ps.glowCreature = function (id, secs) { var cd = findCd(id); if (cd) cd.glowT = secs || 1.6; return !!cd; };
    ps.glowNear = function (wp, radius, secs) {
        if (!alloc || !node) return 0; syncRide(); var r = alloc.ride, n = 0;
        qInv.copy(r.quaternion).invert(); _tv.set(wp.x - r.position.x, wp.y - r.position.y, wp.z - r.position.z).applyQuaternion(qInv);
        for (var i = 0; i < alloc.creat.count; i++) { var cd = alloc.creatData[i]; _pv.copy(cd.p).multiplyScalar(cd.r + cd.hover + cd.sz * 0.4); if (_pv.distanceTo(_tv) < radius) { cd.glowT = secs || 1.6; n++; } }
        return n;
    };

    // ─── weather (rev 26) ──────────────────────────────────────────────────────────
    var wxForce = null, wxForceT = 0, wxSnap = false, wxCold = false, wxSeed = 0, wxPeriod = 180, wxStreakA = 0, wxAurA = 0, rainN = 0;
    var wxI = { wind: 0, storm: 0, aur: 0 }, wflow = new THREE.Vector3(), wxAxis = new THREE.Vector3(), wxRt = new THREE.Vector3(), wxFw = new THREE.Vector3();
    ps.weather = { state: 'clear', t: 0, wind: 0, storm: 0, aurora: 0 };
    ps.wind = new THREE.Vector3();
    ps.setWeather = function (st) { wxForce = (st === 'clear' || st === 'wind' || st === 'storm' || st === 'aurora') ? st : null; wxForceT = 0; wxSnap = true; };
    function wxSetup() {
        var A = col3(node.mesh.material.uniforms.uAtmo.value);
        wxCold = A[2] > A[0] * 1.05; wxSeed = nodeSeed(node);
        wxPeriod = 120 + hash01(wxSeed, 11, 3, 77) * 180;
    }
    function updateWeather(dt, sunTerm) {
        var a = alloc, st, tIn, now = Date.now() / 1000;
        if (wxForce) { st = wxForce; wxForceT += dt; tIn = wxForceT; }
        else if (isGas) { st = 'clear'; tIn = t; }
        else {
            var slot = Math.floor(now / wxPeriod), hh = hash01(slot, wxSeed & 1023, 7, 4242);
            st = hh < 0.36 ? 'clear' : hh < 0.62 ? 'wind' : hh < 0.82 ? 'storm' : (wxCold ? 'aurora' : 'clear'); tIn = now - slot * wxPeriod;
        }
        var k = wxSnap ? 1 : Math.min(1, dt / 5); wxSnap = false;
        wxI.wind += ((st === 'wind' ? 1 : st === 'storm' ? 1.25 : 0) - wxI.wind) * k;
        wxI.storm += ((st === 'storm' ? 1 : 0) - wxI.storm) * k;
        wxI.aur += ((st === 'aurora' ? 1 : 0) - wxI.aur) * k;
        var eff = smoothstep(0.15, 0.5, ps.depth), W = ps.weather;
        W.state = st; W.t = tIn; W.wind = wxI.wind * eff; W.storm = wxI.storm * eff; W.aurora = wxI.aur * eff;
        // up / pole basis (world)
        wxAxis.set(0, 1, 0).applyQuaternion(node.mesh.quaternion);
        wxRt.crossVectors(wxAxis, tmpV); if (wxRt.lengthSq() < 1e-6) wxRt.set(1, 0, 0).addScaledVector(tmpV, -tmpV.x); wxRt.normalize(); wxFw.crossVectors(tmpV, wxRt).normalize();
        // wind: a seeded horizontal direction in the patch tangent frame (rotated to world), gusting; zero outside the atmosphere
        var ang = hash01(wxSeed, 5, 9, 31) * 6.283 + Math.sin(t * 0.05) * 0.5, gust = 0.75 + 0.25 * Math.sin(t * 0.7 + 1.3);
        wflow.copy(T1).multiplyScalar(Math.cos(ang)).addScaledVector(T2, Math.sin(ang)).applyQuaternion(node.mesh.quaternion).normalize();
        ps.wind.copy(wflow).multiplyScalar((wxI.wind * 4 + wxI.storm * 3) * L * gust * eff);
        a.floraU.uSway.value = 0.1 + wxI.wind * 0.9 * eff + wxI.storm * 0.4 * eff;
        wxStreakA = Math.min(0.45, wxI.wind * 0.26 + wxI.storm * 0.1) * eff;
        // storm: darker dome + denser darker fog + rain
        a.domeU.uStorm.value = wxI.storm * eff;
        SH.uFogCol.value.multiplyScalar(1 - 0.5 * wxI.storm * eff); SH.uFogK.value *= 1 + 1.3 * wxI.storm * eff;
        rainN = Math.floor(a.rainN * Math.min(1, wxI.storm * eff * 1.05));
        a.rain.geometry.setDrawRange(0, rainN * 2);
        if (rainN > 0) {
            var ru = a.rainU; ru.uUp.value.copy(tmpV); ru.uRt.value.copy(wxRt); ru.uFw.value.copy(wxFw); ru.uWind.value.copy(ps.wind); ru.uAlpha.value = 0.55 * Math.min(1, wxI.storm * eff);
        }
        // aurora: a curtain over the pole, night side only (a forced aurora shows regardless of the sun)
        var nightK = wxForce === 'aurora' ? 1 : smoothstep(0.35, 0.85, 1 - sunTerm);
        wxAurA = wxI.aur * eff * nightK;
        if (wxAurA > 0.01) {
            var au = a.aurU; au.uI.value = wxAurA; au.uRt.value.copy(wxRt); au.uFw.value.copy(wxFw);
            au.uAz.value = (hash01(wxSeed, 8, 2, 55) - 0.5) * 0.8 + Math.sin(t * 0.02) * 0.3;
        }
    }

    // ─── wind streaks ──────────────────────────────────────────────────────────────
    var sR = new THREE.Vector3(), sU = new THREE.Vector3();
    function updateStreaks(dt, shipPos) {
        var a = alloc;
        // flow direction = against the ship's velocity; hovering falls back to "up" (streaks rise past a descending camera)
        var v = 0;
        if (prevOk && dt > 1e-5) {
            tmpB.set(shipPos.x - prevPos.x, shipPos.y - prevPos.y, shipPos.z - prevPos.z);
            v = tmpB.length() / dt;
            if (v > 1500 * L) v = 0;                                  // teleport (test / respawn), not motion
            if (v > 1e-4) { tmpB.multiplyScalar(-1 / (v * dt)); flow.lerp(tmpB, Math.min(1, dt * 6)); }
        }
        prevPos.set(shipPos.x, shipPos.y, shipPos.z); prevOk = true;
        spd += (v - spd) * Math.min(1, dt * 4);
        if (spd < 2 * L) flow.lerp(tmpV.set(shipPos.x - node.anchor.position.x, shipPos.y - node.anchor.position.y, shipPos.z - node.anchor.position.z).normalize(), Math.min(1, dt * 3));
        flow.normalize();
        var wA = wxStreakA, F = flow, alp = rimI * 0.9, hk = rimI, spdU = spd;
        if (rimI < 0.002) {            // rev 26: no rim pass -> the weather's wind haze reuses the same pool (streaks drift along the wind)
            if (wA < 0.02 || ps.depth < 0.25) { a.streaks.visible = false; return; }
            F = wflow; alp = wA; hk = 0.3; spdU = ps.wind.length() * 3;
        }
        a.streaks.visible = ps.visible;
        a.stU.uAlpha.value = alp;
        var Ac = col3(node.mesh.material.uniforms.uAtmo.value);
        a.stU.uCol.value.setRGB(0.55 + 0.45 * Ac[0], 0.55 + 0.45 * Ac[1], 0.55 + 0.45 * Ac[2]);
        // plane basis perpendicular to F
        var ref = Math.abs(F.y) < 0.9 ? sU.set(0, 1, 0) : sU.set(1, 0, 0);
        sR.crossVectors(F, ref).normalize(); sU.crossVectors(F, sR).normalize();
        var P = a.stPos.array, O = a.stOth.array, speed = 260 * L + spdU * 1.6, span = 45 * L;
        for (var i = 0; i < NSTREAK; i++) {
            var st = a.st[i];
            st.along = (st.along === undefined ? 0 : st.along);
            if (!st.init || st.along > span) {
                var ang = Math.random() * 6.2832, rad = (2 + Math.sqrt(Math.random()) * 16) * L;
                st.cx = Math.cos(ang) * rad; st.cy = Math.sin(ang) * rad;
                st.along = st.init ? -span * (0.4 + Math.random() * 0.6) : (Math.random() * 2 - 1) * span;
                st.len = (8 + Math.random() * 22) * L; st.sp = 0.7 + Math.random() * 0.6; st.init = true;
            }
            st.along += speed * st.sp * dt;
            var cx = sR.x * st.cx + sU.x * st.cy, cy = sR.y * st.cx + sU.y * st.cy, cz = sR.z * st.cx + sU.z * st.cy;
            var hl = st.len * (0.5 + hk), hx = cx + F.x * st.along, hy = cy + F.y * st.along, hz = cz + F.z * st.along;
            var tx = hx - F.x * hl, ty = hy - F.y * hl, tz = hz - F.z * hl, o = i * 12;
            // verts: tail,tail,head,head ; aOth = the other end
            P[o] = tx; P[o + 1] = ty; P[o + 2] = tz; P[o + 3] = tx; P[o + 4] = ty; P[o + 5] = tz;
            P[o + 6] = hx; P[o + 7] = hy; P[o + 8] = hz; P[o + 9] = hx; P[o + 10] = hy; P[o + 11] = hz;
            O[o] = hx; O[o + 1] = hy; O[o + 2] = hz; O[o + 3] = hx; O[o + 4] = hy; O[o + 5] = hz;
            O[o + 6] = tx; O[o + 7] = ty; O[o + 8] = tz; O[o + 9] = tx; O[o + 10] = ty; O[o + 11] = tz;
        }
        a.stPos.needsUpdate = true; a.stOth.needsUpdate = true;
    }

    // ─── scars (titan beam decals) ─────────────────────────────────────────────────
    function killScars() { if (!alloc) return; for (var i = 0; i < NSCAR; i++) { alloc.scars[i].ttl = 0; alloc.scars[i].mesh.visible = false; } }
    ps.scar = function (pos, dir, len, ttl) {
        if (!node || !alloc) return false;
        var a = alloc, sc = a.scars[0], i;
        for (i = 0; i < NSCAR; i++) { var c = a.scars[i]; if (c.ttl <= 0) { sc = c; break; } if (c.ttl < sc.ttl) sc = c; }
        toObjDir(pos, tmpA);
        var ox = tmpA.x, oy = tmpA.y, oz = tmpA.z;
        tmpB.set(dir.x, dir.y, dir.z).applyQuaternion(qInv);                 // qInv = inverse planet rotation (set by toObjDir)
        var dp = tmpB.x * ox + tmpB.y * oy + tmpB.z * oz;
        var tx = tmpB.x - ox * dp, ty = tmpB.y - oy * dp, tz = tmpB.z - oz * dp, tl = Math.hypot(tx, ty, tz);
        if (tl < 1e-6) return false;
        tx /= tl; ty /= tl; tz /= tl;
        var bx = oy * tz - oz * ty, by = oz * tx - ox * tz, bz = ox * ty - oy * tx;      // across-track
        var P = sc.mesh.geometry.attributes.position.array, wdt = Math.max(0.6 * L, len / 70), step = len / (SCAR_PTS - 1);
        for (i = 0; i < SCAR_PTS; i++) {
            var s = i * step - len * 0.5, ang = s / R, ca = Math.cos(ang), sa = Math.sin(ang);
            var px = ox * ca + tx * sa, py = oy * ca + ty * sa, pz = oz * ca + tz * sa;       // great-circle arc
            var h = R * (1 + hFrac(px, py, pz)) + 0.5 * L;
            var wtp = wdt * (0.35 + 0.65 * Math.sin(Math.PI * i / (SCAR_PTS - 1)));
            var o = i * 6;
            P[o] = px * h - bx * wtp; P[o + 1] = py * h - by * wtp; P[o + 2] = pz * h - bz * wtp;
            P[o + 3] = px * h + bx * wtp; P[o + 4] = py * h + by * wtp; P[o + 5] = pz * h + bz * wtp;
        }
        sc.mesh.geometry.attributes.position.needsUpdate = true;
        sc.ttl = sc.max = Math.max(0.05, ttl || 3); sc.mesh.visible = ps.visible;
        return true;
    };
    function updateScars(dt) {
        var a = alloc;
        for (var i = 0; i < NSCAR; i++) {
            var c = a.scars[i];
            if (c.ttl <= 0) { c.mesh.visible = false; continue; }
            c.ttl -= dt;
            if (c.ttl <= 0) { c.mesh.visible = false; continue; }
            var life = c.ttl / c.max, flick = 0.85 + 0.15 * Math.sin(t * 40 + i * 2);
            c.mesh.material.opacity = Math.min(1, life * 4) * Math.min(1, (1 - life) * 12 + 0.25) * flick;
            c.mesh.material.color.setRGB(1, 0.35 + 0.4 * life, 0.1 + 0.2 * life);
        }
    }

    // ─── per-frame ─────────────────────────────────────────────────────────────────
    ps.moonMin = 60 * L;      // rev 20: only a body under 60 L rendered radius is a moonlet (no surface)
    ps.update = function (dt, shipPos) {
        t += dt; SH.uTime.value = t;
        // pick the active body: nearest real body inside 3 R (3.25 R to leave)
        var best = null, bd = Infinity, list = engine.drawOrder;
        for (var i = 0; i < list.length; i++) {
            var n = list[i];
            if (!n.mesh || !n.anchor || !n.anchor.visible || !n.mesh.material || !n.mesh.material.uniforms || !n.mesh.material.uniforms.uSeed) continue;
            var Rn = n.mesh.scale.x; if (!(Rn > 0) || Rn < ps.moonMin) continue;      // moonlets (< 60 L) never get a surface
            var dx = shipPos.x - n.anchor.position.x, dy = shipPos.y - n.anchor.position.y, dz = shipPos.z - n.anchor.position.z;
            var ratio = Math.sqrt(dx * dx + dy * dy + dz * dz) / Rn;
            if (ratio < (n === node ? NEAR_OFF : NEAR_R) && ratio < bd) { bd = ratio; best = n; }
        }
        if (!best) { if (node) deactivate(); return; }
        if (best !== node) activate(best);
        var a = alloc;
        R = node.mesh.scale.x;
        if (outR && Math.abs(R / outR - 1) > 0.004) { clearOutposts(); seedOutposts(); seedLandmarks(); }   // planet rescaled (pilot blend): re-lay at the new radius
        var ratioD = bd;
        var deckF = isGas ? GAS_DECK * smoothstep(1.75, 1.45, ratioD) : 0;      // rev 19: effective radius of a gas giant's cloud deck (x R above the globe)
        a.patchU.uBias.value = isGas ? deckF : BIAS;
        var ratioE = ratioD / (1 + deckF);

        // keep the patch in the planet's (spinning) frame
        a.patchU.uR.value = R; a.beacU.uR.value = R;
        rideKey[0] = NaN; syncRide();    // also re-synced at render time (after updateBodies) by every ride object
        for (var oi = 0; oi < ps.outposts.length; oi++) {
            var op = ps.outposts[oi];
            if (op.prevOk && dt > 1e-5) op.vel.copy(op.pos).sub(op.prev).multiplyScalar(1 / dt); else op.vel.set(0, 0, 0);
            op.prev.copy(op.pos); op.prevOk = true;
        }

        if (ps.world && worldOn) {      // rev 21: player's planet-local position (undo the spin) for the world's streaming / interaction
            toObjDir(shipPos, dirObj); wLocal.copy(dirObj).multiplyScalar(Math.hypot(shipPos.x - node.anchor.position.x, shipPos.y - node.anchor.position.y, shipPos.z - node.anchor.position.z));
            try { ps.world.update(t, dt, wLocal); } catch (e) { if (!ps._wErr) { ps._wErr = 1; console.info('[ship-planet] world.update', e); } }
        }
        toObjDir(shipPos, dirObj);
        var horizon = Math.acos(Math.min(1, 1 / Math.max(ratioE, 1.0001)));
        var wantHalf = Math.min(1.4, Math.max(Math.atan((ps.footMode ? PATCH_FOOT_L : PATCH_MIN_L) * L / R), horizon * 1.5 + 0.05));    // rev 20: >= 2600 L reach, so the patch is past the horizon (138 L at 0.8 L) and into the fog
        a.patchU.uDetK.value = R / L; a.patchU.uEps.value = Math.min(0.0012, Math.max(2e-5, 1.5 * L / R));                            // normal-probe step ~1.5 L
        if (!patchSet || C.angleTo(dirObj) * R > Math.min(40 * L, halfAng * 0.125 * R) || Math.abs(wantHalf / halfAng - 1) > 0.25) {
            C.copy(dirObj); tangentFrame(C, T1, T2);
            halfAng = wantHalf; a.patchU.uTan.value = Math.tan(halfAng); patchSet = true;
        }
        // altitude look: the patch cross-fades in over the (never hidden) orbital sphere
        var fadeIn = 1 - smoothstep(FADE_IN_B, FADE_IN_A, ratioD);
        a.patchU.uFade.value = fadeIn;
        a.patchU.uRel.value = isGas ? 1 : Math.pow(smoothstep(REL_A, REL_B, ratioD), 2.2);
        a.patchMat.depthWrite = fadeIn > 0.98;      // rev 24: while the patch is still fading it must not hide the orbital glow sprite behind its depth
        a.patchU.uSurf.value = smoothstep(SURF_A, SURF_B, ratioD);

        // entry transition: halo brightens/expands 2.2 R -> 1.4 R, then dims once you are inside it (dome takes over)
        haloT = smoothstep(HALO_A, HALO_B, ratioD);
        haloAlphaK = 1 - smoothstep(1.45, 1.1, ratioD);           // rev 18: the orbital halo fades out completely by 1.1 R, so the horizon is the terrain
        if (haloH && haloH.atmo) haloH.atmo.scale.setScalar(1.07 + 0.05 * haloT);
        // rim pass envelope (ramps in 1.65 -> 1.55 R, out 1.45 -> 1.35 R)
        rimI = smoothstep(RIM_A, RIM_A - 0.1, ratioD) * (1 - smoothstep(RIM_B + 0.1, RIM_B, ratioD));
        ps.shake = rimI;
        if (!ps.visible) { haloT = 0; haloAlphaK = 1; rimI = 0; ps.shake = 0; }
        updateStreaks(dt, shipPos);

        // atmosphere: depth 0 at 1.4 R -> 1 at the floor
        var floorR = R * (1 + (isGas ? GAS_DECK : BIAS));
        var distC = bd * R;
        ps.depth = Math.max(0, Math.min(1, (ATMO_R * R - distC) / (ATMO_R * R - floorR)));
        var A = col3o(node.mesh.material.uniforms.uAtmo.value, _cA), Hh = col3o(node.mesh.material.uniforms.uColHigh.value, _cH);
        // up (world) from planet centre to camera -> sun term
        tmpV.set(shipPos.x - node.anchor.position.x, shipPos.y - node.anchor.position.y, shipPos.z - node.anchor.position.z).normalize();
        var sunDot = tmpV.dot(SH.uLightDir.value), sunTerm = smoothstep(-0.25, 0.55, sunDot);
        var fb = 0.28 + 0.72 * sunTerm;
        SH.uFogCol.value.set((A[0] * 0.5 + Hh[0] * 0.5) * fb * 0.6 + 0.02, (A[1] * 0.5 + Hh[1] * 0.5) * fb * 0.6 + 0.02, (A[2] * 0.5 + Hh[2] * 0.5) * fb * 0.6 + 0.02);   // horizon tint = pal.hi, zenith = pal.atmo
        // rev 19: ground visibility >= 400 L (f(400 L) = 0.38, f(150 L) = 0.05); only the haze layer near the top of the atmosphere densifies, and space (depth 0) is clear
        // rev 24: no top-of-atmosphere haze layer; fog density follows the altitude above the ground (clear from the air, f(400 L) = 0.10 on the surface)
        var altF = Math.max(0, ratioD * R - R * (1 + hFrac(dirObj.x, dirObj.y, dirObj.z))) / L;
        SH.uFogK.value = (0.78 / (400 * L)) * 0.42 * Math.exp(-altF / 450) * smoothstep(0.0, 0.04, ps.depth);
        if (isGas) SH.uFogK.value *= 2.4;          // rev 20: a thicker haze layer over the cloud tops
        SH.uNight.value = 1 - sunTerm;
        updateWeather(dt, sunTerm);
        a.domeU.uUp.value.copy(tmpV); a.domeU.uSun.value = sunTerm;
        a.domeU.uAlpha.value = Math.pow(ps.depth, 0.8) * 0.45; a.domeU.uStar.value = Math.pow(ps.depth, 0.8);   // rev 24: dome alpha <= 0.45
        a.domeU.uHor.value = -Math.sqrt(Math.max(0, 1 - 1 / (ratioE * ratioE)));   // sin(elevation) of the geometric horizon
        a.dome.position.copy(camera.position); a.dome.updateMatrixWorld();
        updateScars(dt);

        // rocks: within ~260 L of the ground
        var altG = ratioD * R - R * (1 + hFrac(dirObj.x, dirObj.y, dirObj.z));      // rev 20: altitude above the ACTUAL ground (mountains are 0.09 R = 1000 L high at this scale)
        if (altG < 280 * L && !isGas) {
            if (!rockSet || dirObj.angleTo(rockC) * R > 40 * L) { rockC.copy(dirObj); seedRocks(); }
        } else if (a.rocks.count) { a.rocks.count = 0; rockSet = false; }
        // flora (< 1.3 R) and creatures (< 1.2 R), pooled + seeded by lattice hash
        if (ratioD < FLORA_R && !isGas) {
            if (!floraSet || dirObj.angleTo(floraC) * R > 40 * L) { floraC.copy(dirObj); seedFlora(); }
        } else if (a.flora.count) { a.flora.count = 0; floraSet = false; }
        if (ratioD < CREAT_R && !isGas) {
            if (!creatSet || dirObj.angleTo(creatC) * R > 60 * L) { creatC.copy(dirObj); seedCreatures(); }
            plObj.copy(dirObj).multiplyScalar(distC); plDir.copy(dirObj); plOk = true;
            animCreatures(dt);
        } else if (a.creat.count) { a.creat.count = 0; creatSet = false; }
        if (ratioD < FLORA_R && isGas) {
            if (!sporeSet || dirObj.angleTo(sporeC) * R > 12 * L) { sporeC.copy(dirObj); seedSpores(); }
        } else if (a.spores.count) { a.spores.count = 0; sporeSet = false; }
        // rev 27: night life, ring + moons in the sky, dust haze
        SH.uBio.value = isGas ? 0 : smoothstep(0.3, 0.85, SH.uNight.value);
        var du = a.domeU, dl2 = Math.hypot(shipPos.x - node.anchor.position.x, shipPos.y - node.anchor.position.y, shipPos.z - node.anchor.position.z);
        du.uPc.value.set((shipPos.x - node.anchor.position.x) / R, (shipPos.y - node.anchor.position.y) / R, (shipPos.z - node.anchor.position.z) / R);
        var rm = node.ringMesh;
        if (rm && rm.material) {
            rm.getWorldQuaternion(ringQ); du.uRingN.value.set(0, 0, 1).applyQuaternion(ringQ);
            var rc = rm.material.color; du.uRingC.value.setRGB(rc.r, rc.g, rc.b); du.uRingA.value = 0.72 * ps.depth;
        } else du.uRingA.value = 0;
        var mi = 0, dl = engine.drawOrder;
        for (var li = 0; li < dl.length && mi < 4; li++) {
            var mn = dl[li];
            if (mn.parentNode !== node || !mn.anchor || !mn.mesh || !mn.mesh.material || !mn.mesh.material.uniforms || !mn.mesh.material.uniforms.uColLow) continue;
            var mx = mn.anchor.position.x - shipPos.x, my = mn.anchor.position.y - shipPos.y, mz = mn.anchor.position.z - shipPos.z, md = Math.hypot(mx, my, mz), mR = mn.mesh.scale.x;
            if (!(md > mR * 1.001)) continue;
            var tan = Math.min(0.12, Math.max(0.012, mR / Math.sqrt(md * md - mR * mR))), mu = mn.mesh.material.uniforms, c1 = mu.uColLow.value, c2 = mu.uColHigh.value;
            du.uMoon.value[mi].set(mx / md, my / md, mz / md, tan);
            du.uMoonC.value[mi].set((c1.r + c2.r) * 0.5 * 0.9 + 0.1, (c1.g + c2.g) * 0.5 * 0.9 + 0.1, (c1.b + c2.b) * 0.5 * 0.9 + 0.1); mi++;
        }
        for (; mi < 4; mi++) du.uMoon.value[mi].w = 0;
        var night = SH.uBio.value, pu2 = a.patchU;
        a.ptsU.uScale.value = ((engine.renderer && engine.renderer.domElement ? engine.renderer.domElement.height : window.innerHeight) || 800) / (2 * Math.tan(camera.fov * Math.PI / 360));
        a.ptsU.uSteam.value.set(Math.min(1, SH.uFogCol.value.x * 1.6 + 0.12), Math.min(1, SH.uFogCol.value.y * 1.6 + 0.12), Math.min(1, SH.uFogCol.value.z * 1.6 + 0.12));
        if (!isGas && altG < 160 * L && night > 0.05) {
            if (!ffSet || dirObj.angleTo(ffC) * R > 20 * L) { ffC.copy(dirObj); seedFireflies(); }
        } else if (a.ffN && !(night > 0.05)) { for (var fz = 0; fz < FF_MAX; fz++) a.ptsP.array[fz * 4 + 3] = 0; a.ptsP.needsUpdate = true; a.ffN = 0; ffSet = false; }
        ptsVis = a.steamN > 0 || a.ffN > 0;
        if (!isGas && altG < 110 * L) {                    // haze: a thin layer 0.5 L over the ground; the hole blown by the exhaust shrinks with altitude and closes slowly
            hC.copy(dirObj); tangentFrame(hC, hT1, hT2);
            pu2.uHC.value.copy(hC); pu2.uHT1.value.copy(hT1); pu2.uHT2.value.copy(hT2); pu2.uPl.value.copy(dirObj); pu2.uHAng.value = Math.min(1.2, 170 * L / R);
            var tgt = 34 * (1 - smoothstep(6 * L, 70 * L, altG)); hazeClear += (tgt - hazeClear) * Math.min(1, dt * (tgt > hazeClear ? 3 : 0.8));
            pu2.uClear.value = hazeClear; pu2.uHazeA.value = 0.3 * (1 - smoothstep(25 * L, 105 * L, altG)) * (1 - 0.5 * ps.weather.storm);
            hazeVis = pu2.uHazeA.value > 0.01;
        } else hazeVis = false;
        showObjs(ps.visible);
    };

    // rev 24: guidance points of interest for the active planet, world positions in the CURRENT frame (the ride group's spin applied)
    ps.pois = function () {
        var out = [];
        if (!node || !alloc) return out;
        rideKey[0] = NaN; syncRide();
        var M = alloc.ride.matrix, w = ps.world, i;
        if (w && worldOn) {
            for (i = 0; w.stores && i < w.stores.length; i++) { var st = w.stores[i]; out.push({ kind: '7/11', name: st.name || '7/11', pos: st.pos.clone().applyMatrix4(M), id: st.id }); }
            if (w.burgerHouse) out.push({ kind: 'burger', name: w.burgerHouse.name || 'Burger House', pos: w.burgerHouse.pos.clone().applyMatrix4(M), id: w.burgerHouse.id || 'burger' });
        }
        for (i = 0; i < lmList.length; i++) out.push({ kind: 'landmark', sub: lmList[i].kind, name: lmList[i].name, pos: lmList[i].lp.clone().applyMatrix4(M), id: lmList[i].id });
        for (i = 0; i < ps.outposts.length; i++) out.push({ kind: 'pad', name: ps.outposts[i].name, pos: ps.outposts[i].pos.clone(), id: ps.outposts[i].id });
        return out;
    };

    ps.setVisible = function (v) {
        ps.visible = !!v;
        showObjs(ps.visible);
    };

    ps.floorAt = function (pos, out) {
        if (!node || !alloc) return null;
        out = out || { r: 0, n: new THREE.Vector3() };
        if (!out.n) out.n = new THREE.Vector3();
        toObjDir(pos, tmpA);
        var dx = tmpA.x, dy = tmpA.y, dz = tmpA.z;
        var h = hFrac(dx, dy, dz);
        lastLand = isGas || _lastN > sea;
        // normal by finite differences along the tangent plane (object space), then rotate to world
        var ref0 = Math.abs(dy) < 0.9;
        var e1x, e1y, e1z;
        if (ref0) { e1x = dz; e1y = 0; e1z = -dx; } else { e1x = 0; e1y = -dz; e1z = dy; }
        var e1l = Math.hypot(e1x, e1y, e1z); e1x /= e1l; e1y /= e1l; e1z /= e1l;
        var e2x = dy * e1z - dz * e1y, e2y = dz * e1x - dx * e1z, e2z = dx * e1y - dy * e1x;
        var eps = 3 * L / R;
        function pt(ax, ay, az, o) {
            var l = Math.hypot(ax, ay, az); ax /= l; ay /= l; az /= l;
            var hh = hFrac(ax, ay, az), r = 1 + hh;
            o[0] = ax * r; o[1] = ay * r; o[2] = az * r;
        }
        var pb = [0, 0, 0], pc = [0, 0, 0];
        pt(dx + e1x * eps, dy + e1y * eps, dz + e1z * eps, pb);
        pt(dx + e2x * eps, dy + e2y * eps, dz + e2z * eps, pc);
        var ax = dx * (1 + h), ay = dy * (1 + h), az = dz * (1 + h);
        var ux = pb[0] - ax, uy = pb[1] - ay, uz = pb[2] - az, vx = pc[0] - ax, vy = pc[1] - ay, vz = pc[2] - az;
        var nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        if (nx * dx + ny * dy + nz * dz < 0) { nx = -nx; ny = -ny; nz = -nz; }
        out.n.set(nx, ny, nz).normalize().applyQuaternion(node.mesh.quaternion);
        out.r = R * (1 + h);
        return out;
    };

    // rev 18: LOCAL-frame queries. (x,y,z) is a position in the planet's own object space (the frame the mesh spins in), so the answer is a
    // pure function of the local direction: no anchor, no spin, no patch state. floorLocal -> surface radius (world units) under that point,
    // heightLocal -> height as a fraction of R for a unit direction, landLocal -> true over land (sea / gas = false).
    ps.heightLocal = function (dx, dy, dz) {
        if (!node || !alloc) return 0;
        var l = Math.sqrt(dx * dx + dy * dy + dz * dz) || 1;
        var h = hFrac(dx / l, dy / l, dz / l);
        lastLand = isGas || _lastN > sea;
        return h;
    };
    ps.floorLocal = function (x, y, z) {
        if (!node || !alloc) return 0;
        var l = Math.sqrt(x * x + y * y + z * z) || 1;
        return R * (1 + hFrac(x / l, y / l, z / l));
    };
    // rev 20b: the surface as RENDERED. The patch is the height function sampled at a warped grid and drawn as linear triangles (quad diagonal v01-v10), so
    // between vertices the visible ground differs from floorLocal by the interpolation error (more than a human's height on rough ground). This mirrors the
    // patch exactly: invert the grid warp, take the cell's three vertices (height at each vertex direction), intersect the ray from the planet centre with that
    // triangle. Answer = radius (world units) of the visible ground along the local direction (x,y,z); falls back to floorLocal outside the dense part of the patch.
    var _mfV = [new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3()];
    function unwarp(u) {         // solve s * (0.04 + 0.96 |s|^3) = u for s in [-1, 1]
        var lo = -1, hi = 1, m;
        for (var i = 0; i < 28; i++) { m = (lo + hi) * 0.5; if (m * (0.04 + 0.96 * Math.abs(m * m * m)) < u) lo = m; else hi = m; }
        return (lo + hi) * 0.5;
    }
    function gridVertex(i, j, G, tn, out) {
        var sx = i / G * 2 - 1, sy = j / G * 2 - 1, ux = sx * (0.04 + 0.96 * Math.abs(sx * sx * sx)), uy = sy * (0.04 + 0.96 * Math.abs(sy * sy * sy));
        var dx = C.x + (T1.x * ux + T2.x * uy) * tn, dy = C.y + (T1.y * ux + T2.y * uy) * tn, dz = C.z + (T1.z * ux + T2.z * uy) * tn, l = Math.sqrt(dx * dx + dy * dy + dz * dz);
        dx /= l; dy /= l; dz /= l;
        var h = hFrac(dx, dy, dz), r = 1 + h;
        return out.set(dx * r, dy * r, dz * r);
    }
    ps.meshFloorLocal = function (x, y, z) {
        if (!node || !alloc || !patchSet) return ps.floorLocal(x, y, z);
        var l = Math.sqrt(x * x + y * y + z * z) || 1, dx = x / l, dy = y / l, dz = z / l;
        var dc = dx * C.x + dy * C.y + dz * C.z;
        if (dc < 0.2) return R * (1 + hFrac(dx, dy, dz));
        var tn = a_tan(), vx = dx / dc - C.x, vy = dy / dc - C.y, vz = dz / dc - C.z;
        var ux = (vx * T1.x + vy * T1.y + vz * T1.z) / tn, uy = (vx * T2.x + vy * T2.y + vz * T2.z) / tn;
        var G = gridN;
        if (Math.abs(ux) > 0.35 || Math.abs(uy) > 0.35) return R * (1 + hFrac(dx, dy, dz));      // outside the full-relief part of the patch (the rim tapers to the sphere)
        var sx = unwarp(ux), sy = unwarp(uy), fi = (sx + 1) * 0.5 * G, fj = (sy + 1) * 0.5 * G;
        var i = Math.min(G - 1, Math.max(0, Math.floor(fi))), j = Math.min(G - 1, Math.max(0, Math.floor(fj))), fx = fi - i, fy = fj - j;
        var A, B, Cc;
        if (fx + fy <= 1) { A = gridVertex(i, j, G, tn, _mfV[0]); B = gridVertex(i, j + 1, G, tn, _mfV[1]); Cc = gridVertex(i + 1, j, G, tn, _mfV[2]); }
        else { A = gridVertex(i + 1, j, G, tn, _mfV[0]); B = gridVertex(i, j + 1, G, tn, _mfV[1]); Cc = gridVertex(i + 1, j + 1, G, tn, _mfV[2]); }
        // plane through A, B, Cc; ray p = t * d
        var e1x = B.x - A.x, e1y = B.y - A.y, e1z = B.z - A.z, e2x = Cc.x - A.x, e2y = Cc.y - A.y, e2z = Cc.z - A.z;
        var nx = e1y * e2z - e1z * e2y, ny = e1z * e2x - e1x * e2z, nz = e1x * e2y - e1y * e2x;
        var den = nx * dx + ny * dy + nz * dz;
        if (Math.abs(den) < 1e-12) return R * (1 + hFrac(dx, dy, dz));
        return R * (nx * A.x + ny * A.y + nz * A.z) / den;
    };
    function a_tan() { return alloc ? alloc.patchU.uTan.value : 0.3; }

    ps.landLocal = function (x, y, z) {
        if (!node || !alloc) return false;
        var l = Math.sqrt(x * x + y * y + z * z) || 1;
        hFrac(x / l, y / l, z / l);
        return isGas || _lastN > sea;
    };
    Object.defineProperty(ps, 'radius', { get: function () { return R; } });
    Object.defineProperty(ps, 'isGas', { get: function () { return isGas; } });

    // landing support: ok = over land, gentle slope (1 - n.up < 0.25), within LAND_ALT ship lengths of the ground
    ps.landable = function (pos) {
        var res = { ok: false, r: 0, n: new THREE.Vector3(0, 1, 0), slope: 1, onPad: false };
        if (!node || !alloc) return res;
        var fo = ps.floorAt(pos, { r: 0, n: new THREE.Vector3() });
        var land = lastLand;
        res.r = fo.r; res.n.copy(fo.n);
        var cx = node.anchor.position.x, cy = node.anchor.position.y, cz = node.anchor.position.z;
        var ux = pos.x - cx, uy = pos.y - cy, uz = pos.z - cz, ul = Math.hypot(ux, uy, uz) || 1;
        res.slope = 1 - (fo.n.x * ux + fo.n.y * uy + fo.n.z * uz) / ul;
        var alt = ul - fo.r;
        toObjDir(pos, tmpA);
        for (var i = 0; i < pads.length; i++) {
            var pd = pads[i], ch = Math.hypot(tmpA.x - pd.d.x, tmpA.y - pd.d.y, tmpA.z - pd.d.z);
            if (ch * R < PAD_L * L) { res.onPad = true; land = true; }
        }
        res.ok = land && res.slope < LAND_SLOPE && alt < LAND_ALT * L;
        res.why = res.ok ? '' : (!land ? 'water' : (res.slope >= LAND_SLOPE ? 'slope' : 'alt'));      // rev 19: why not (HUD message)
        return res;
    };

    ps.info = function () { return { amp: amp, look: look, rocks: alloc ? alloc.rocks.count : 0, flora: alloc ? alloc.flora.count : 0, creatures: alloc ? alloc.creat.count : 0, grid: gridN, active: node ? node.id : null }; };
    ps.lookOf = function (n) { var u = n.mesh.material.uniforms; return lookFromPalette(col3(u.uColLow.value), col3(u.uColHigh.value)) === 0 ? 'rocky' : (lookFromPalette(col3(u.uColLow.value), col3(u.uColHigh.value)) === 1 ? 'lush' : 'icy'); };

    // rev 19: id pass for the "never see through the ground" test. Renders ONLY the active planet's ground (patch, flora, creatures, rocks, pads, buildings,
    // and the orbital sphere itself) as flat white into a small render target and returns its RGBA bytes (R = 1 where terrain / an object on it was drawn,
    // 0 = sky / space). Additive overlays (beacons, scars, streaks) and the dome are left out. Everything else in the scene is hidden for the pass and restored.
    ps.idPass = function (renderer, cam, W, H, withSphere) {
        if (!node || !alloc) return null;
        var rt = new THREE.WebGLRenderTarget(W, H), saved = [], kids = scene.children, i, restore = [];
        var idFrag = '#include <common>\n#include <logdepthbuf_pars_fragment>\nvoid main() {\n#include <logdepthbuf_fragment>\n gl_FragColor = vec4(1.0);\n}';
        var ids = [];
        for (i = 0; i < kids.length; i++) { saved.push(kids[i].visible); kids[i].visible = (kids[i] === alloc.ride || kids[i] === node.anchor); }
        alloc.ride.traverse(function (o) {
            if (!o.isMesh) return;
            var m = o.material;
            if (m.blending === THREE.AdditiveBlending || o === alloc.dome) { restore.push([o, 'vis', o.visible]); o.visible = false; return; }
            var im = new THREE.ShaderMaterial({ uniforms: m.uniforms, vertexShader: m.vertexShader, fragmentShader: idFrag, side: THREE.DoubleSide, depthWrite: true, transparent: false });
            restore.push([o, 'mat', m]); o.material = im; ids.push(im);
        });
        var nm = node.mesh, oldMat = nm.material, kidVis = nm.children.map(function (c) { return c.visible; });
        nm.children.forEach(function (c) { c.visible = false; });
        var bm = new THREE.MeshBasicMaterial({ color: 0xffffff, toneMapped: false }); nm.material = bm; var nmVis = nm.visible; if (!withSphere) nm.visible = false;      // strict mode: the orbital sphere does not count as ground
        var oldBg = scene.background; scene.background = null;
        var oldRT = renderer.getRenderTarget(), oldCol = renderer.getClearColor(new THREE.Color()), oldA = renderer.getClearAlpha();
        renderer.setRenderTarget(rt); renderer.setClearColor(0x000000, 0); renderer.clear();
        var buf = new Uint8Array(W * H * 4);
        try { renderer.render(scene, cam); renderer.readRenderTargetPixels(rt, 0, 0, W, H, buf); }
        finally {
            renderer.setRenderTarget(oldRT); renderer.setClearColor(oldCol, oldA);
            scene.background = oldBg; nm.visible = nmVis; nm.material = oldMat; nm.children.forEach(function (c, k) { c.visible = kidVis[k]; });
            for (i = 0; i < restore.length; i++) { var r = restore[i]; if (r[1] === 'mat') r[0].material = r[2]; else r[0].visible = r[2]; }
            for (i = 0; i < kids.length; i++) kids[i].visible = saved[i];
            ids.forEach(function (m) { m.dispose(); }); bm.dispose(); rt.dispose();
        }
        return buf;
    };

    ps.dispose = function () {
        worldDetach(); ps.world = null;
        deactivate();
        if (alloc) {
            [alloc.patch, alloc.flora, alloc.creat, alloc.rocks, alloc.pads, alloc.blds, alloc.beac, alloc.dome, alloc.streaks, alloc.rain, alloc.aurora, alloc.lm, alloc.pts, alloc.haze, alloc.scarG].forEach(function (m) {
                scene.remove(m); if (m.parent) m.parent.remove(m); if (m.geometry) m.geometry.dispose(); if (m.material) m.material.dispose();
                if (m.dispose) m.dispose();
                if (m === alloc.scarG) m.children.forEach(function (c) { c.geometry.dispose(); c.material.dispose(); });
            });
            scene.remove(alloc.ride);
            alloc = null;
        }
    };

    return ps;
}

export default createPlanetSurface;

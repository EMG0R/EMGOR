// GORMAPPER renderer — WebGL2, shared by the editor preview and the output
// window so both show pixel-identical results.
//
// Content kinds:
//   pattern — hot-reloadable GLSL from /patterns/<id>.glsl (see patterns.js)
//   media   — uploaded image/video, cover-fit into the polygon bbox
//   screen  — the shared getDisplayMedia capture; 4-point polygons get a true
//             homography warp, other polygons get cover-fit like media
//   mask    — solid black (params.opacity)

import {
  ZONE_VS, QUAD_VS, TEX_FS, FLAT_FS, TESTCARD_FS,
  RING_FS, RING_REACT_FS, RING_AURORA_FS, RING_PARTICLES_FS, wrapWallProgram,
} from './shaders.js';
import { bbox, earcut, homographyFromCorners, hexToRgb } from './scene.js';
import { createPatternManager, linkProgram } from './patterns.js';

export function createRenderer(canvas, opts) {
  const o = opts || {};
  // Overlay mode (Electron transparent window over the real desktop):
  // transparent framebuffer, and "screen" objects become holes — the actual
  // desktop shows through them instead of a capture texture.
  const overlay = !!o.overlay;
  const gl = canvas.getContext('webgl2', overlay
    ? { antialias: true, alpha: true, premultipliedAlpha: false }
    : { antialias: true, alpha: false });
  if (!gl) throw new Error('WebGL2 not available');

  const mediaP = linkProgram(gl, ZONE_VS, TEX_FS);       // media zones (+ cover-fit screen)
  const flatP = linkProgram(gl, ZONE_VS, FLAT_FS);       // masks
  const ringP = linkProgram(gl, ZONE_VS, RING_FS);       // LED-ring bleed style
  const ringReactP = linkProgram(gl, ZONE_VS, RING_REACT_FS); // LED-ring reactive style (default)
  const ringAuroraP = linkProgram(gl, ZONE_VS, RING_AURORA_FS);     // aurora curtains
  const ringParticlesP = linkProgram(gl, ZONE_VS, RING_PARTICLES_FS); // ember/firefly field

  // Pi-authored wall program (projector.glsl): compiled hot, last-good kept
  // on failure, result reported to /api/wall/status so the scene generator
  // can self-correct. One program at a time — it comes from the scene.
  const wall = { src: null, prog: null, error: null };

  function compileWallProgram(src) {
    if (src === wall.src) return;
    wall.src = src;
    try {
      const p = linkProgram(gl, ZONE_VS, wrapWallProgram(src));
      if (wall.prog) gl.deleteProgram(wall.prog.prog);
      wall.prog = p;
      wall.error = null;
    } catch (e) {
      wall.error = String(e.message || e);   // keep last-good wall.prog
    }
    fetch('/api/wall/status', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        ok: !wall.error,
        error: wall.error,
        using: wall.error ? (wall.prog ? 'last-good glsl' : 'style fallback') : 'glsl',
      }),
    }).catch(() => {});
  }
  const quadP = linkProgram(gl, QUAD_VS, TEX_FS);        // 4-point screen (homography)
  const cardP = linkProgram(gl, QUAD_VS, TESTCARD_FS);   // screen placeholder (homography)
  const cardZoneP = linkProgram(gl, ZONE_VS, TESTCARD_FS); // screen placeholder (polygon)

  // hot-reloadable pattern programs
  const patterns = createPatternManager(gl, ZONE_VS, {
    onError: o.onPatternError,
    onListChange: o.onPatternList,
    onUpdate: o.onPatternUpdate,
  });

  gl.enable(gl.BLEND);
  // separate alpha blend keeps destination alpha correct for the transparent
  // (non-premultiplied) overlay framebuffer; identical result when opaque
  gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);

  // ---- geometry -------------------------------------------------------------

  // unit-square strip for homography quads
  const quadVAO = gl.createVertexArray();
  gl.bindVertexArray(quadVAO);
  const quadBuf = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, quadBuf);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([0, 0, 1, 0, 0, 1, 1, 1]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
  gl.bindVertexArray(null);

  // per-layer polygon VAO cache
  const polyCache = new Map();

  function polygonVAO(layer) {
    const key = JSON.stringify(layer.points);
    let entry = polyCache.get(layer.id);
    if (entry && entry.key === key) return entry;
    if (entry) { gl.deleteVertexArray(entry.vao); gl.deleteBuffer(entry.vbo); gl.deleteBuffer(entry.ibo); }

    const pts = layer.points;
    const tris = earcut(pts);
    const bb = bbox(pts);
    const verts = new Float32Array(pts.length * 4);
    for (let i = 0; i < pts.length; i++) {
      verts[i * 4 + 0] = pts[i][0];
      verts[i * 4 + 1] = pts[i][1];
      verts[i * 4 + 2] = (pts[i][0] - bb.x) / bb.w;   // bbox uv
      verts[i * 4 + 3] = (pts[i][1] - bb.y) / bb.h;
    }
    const vao = gl.createVertexArray();
    gl.bindVertexArray(vao);
    const vbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
    gl.bufferData(gl.ARRAY_BUFFER, verts, gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 16, 0);   // aPos
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 16, 8);   // aUV
    const ibo = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(tris), gl.STATIC_DRAW);
    gl.bindVertexArray(null);

    entry = { key, vao, vbo, ibo, count: tris.length, bb };
    polyCache.set(layer.id, entry);
    return entry;
  }

  function drawPoly(geo) {
    gl.bindVertexArray(geo.vao);
    gl.drawElements(gl.TRIANGLES, geo.count, gl.UNSIGNED_SHORT, 0);
    gl.bindVertexArray(null);
  }

  // ---- textures -------------------------------------------------------------

  function makeTexture() {
    const tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
      new Uint8Array([0, 0, 0, 255]));   // defined sampling before load
    return tex;
  }

  // shared screen-capture video (set from output.html after getDisplayMedia);
  // uploaded once per frame no matter how many objects use it
  let screenVideo = null;
  const screenTex = makeTexture();
  let screenFrameUploaded = false;

  function setScreenVideo(videoEl) { screenVideo = videoEl; }

  // ---- LED-ring feed (smartGOR lightserver canvas tap via /api/ring) --------
  // Polled only while some layer actually uses it; the server returns 204
  // when the tap is silent (GORMAPPER closed on the Pi's side, or Pi off),
  // and ring zones then render nothing — no wasted work anywhere.
  const RING_PIXELS = 1920;
  const ringTex = makeTexture();
  gl.bindTexture(gl.TEXTURE_2D, ringTex);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);   // seam-crossing windows
  let ringData = null;          // latest Uint8Array(1920*3), null until live
  let ringSeq = 0;
  let ringWanted = false;       // any ring layer in the last rendered scene
  let ringTimer = null;
  let ringMeta = null;          // latest semantic packet (tracks/stats/projector)
  let sceneProjector = null;    // projector block from the local scene JSON
  const ringZoneState = new Map();   // layer.id -> peak tracking + bursts

  // smoothed room mood — from semantic stats when the Pi sends them,
  // derived from the pixels otherwise
  const mood = { energy: 0, palA: [0.5, 0.5, 0.9], palB: [0.1, 0.1, 0.3] };

  function lerp3(a, b, k) { for (let i = 0; i < 3; i++) a[i] += (b[i] - a[i]) * k; }

  function hexTo01(hex) {
    const n = parseInt(String(hex).replace('#', ''), 16);
    return [((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255];
  }

  function updateMood() {
    if (ringMeta && ringMeta.stats) {
      const s = ringMeta.stats;
      mood.energy += ((s.energy ?? 0) * 6 > 1 ? 1 : (s.energy ?? 0) * 6) * 0.15 - mood.energy * 0.15;
      const dom = s.dominant || [];
      if (dom[0]) lerp3(mood.palA, hexTo01(dom[0]), 0.1);
      if (dom[1]) lerp3(mood.palB, hexTo01(dom[1]), 0.1);
      return;
    }
    if (!ringData) return;
    // pixel fallback: subsampled mean luminance + brightest pixel as palette
    let sum = 0, best = 0, bi = 0;
    for (let i = 0; i < RING_PIXELS; i += 8) {
      const j = i * 3;
      const lum = ringData[j] * 0.30 + ringData[j + 1] * 0.59 + ringData[j + 2] * 0.11;
      sum += lum;
      if (lum > best) { best = lum; bi = j; }
    }
    const energy = Math.min(1, (sum / (RING_PIXELS / 8) / 255) * 6);
    mood.energy += (energy - mood.energy) * 0.15;
    if (best > 40) {
      lerp3(mood.palA, [ringData[bi] / 255, ringData[bi + 1] / 255, ringData[bi + 2] / 255], 0.08);
      lerp3(mood.palB, [mood.palA[0] * 0.35, mood.palA[1] * 0.25, mood.palA[2] * 0.6], 0.05);
    }
  }

  async function pollRing() {
    if (!ringWanted) { ringTimer = null; return; }
    try {
      const [r, m] = await Promise.all([
        fetch('/api/ring', { cache: 'no-store' }),
        fetch('/api/ring/meta', { cache: 'no-store' }),
      ]);
      ringMeta = m.status === 200 ? await m.json() : null;
      if (r.status === 200) {
        const seq = +r.headers.get('X-Ring-Seq') || 0;
        if (seq !== ringSeq) {
          ringSeq = seq;
          ringData = new Uint8Array(await r.arrayBuffer());
          gl.bindTexture(gl.TEXTURE_2D, ringTex);
          gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
          gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGB, RING_PIXELS, 1, 0,
            gl.RGB, gl.UNSIGNED_BYTE, ringData);
          gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
        }
        updateMood();
      } else {
        ringData = null;        // 204: tap silent — zones go dark
      }
    } catch (e) { ringData = null; ringMeta = null; }
    ringTimer = setTimeout(pollRing, 66);   // ~15fps, matching the tap rate
  }

  // Peak tracking per zone: a bright blob moving through the calibrated
  // window is "an object crossing above the projector" — spawn a reflection
  // burst at its position that the shader shoots down/off the shell.
  function trackRingZone(layer, win) {
    let st = ringZoneState.get(layer.id);
    if (!st) { st = { seq: -1, peakU: -1, bursts: [], lastBurstMs: 0 }; ringZoneState.set(layer.id, st); }
    const now = performance.now();
    st.bursts = st.bursts.filter((b) => now - b.t0 < 1500);
    if (!ringData || st.seq === ringSeq) return st;
    st.seq = ringSeq;
    const start = Math.round(win.offset), n = Math.max(1, Math.round(win.count));
    let best = 0, bestI = -1;
    for (let i = 0; i < n; i++) {
      const j = ((start + i) % RING_PIXELS) * 3;
      const lum = ringData[j] * 0.30 + ringData[j + 1] * 0.59 + ringData[j + 2] * 0.11;
      if (lum > best) { best = lum; bestI = i; }
    }
    if (best > 110 && bestI >= 0) {
      const u = bestI / n;
      const moved = st.peakU >= 0 ? u - st.peakU : 0;
      if (Math.abs(moved) > 0.004 && now - st.lastBurstMs > 350 && st.bursts.length < 8) {
        st.bursts.push({ u, dir: Math.sign(moved) || 1, t0: now, strength: Math.min(1, best / 255) });
        st.lastBurstMs = now;
      }
      st.peakU = u;
    } else {
      st.peakU = -1;
    }
    return st;
  }

  // Ground-truth comets: semantic tracks inside (or approaching) the window.
  // Falls back to pixel peak-tracking bursts when the Pi sends pixels only.
  function ringComets(st, win, now) {
    const out = [];
    const tracks = ringMeta && Array.isArray(ringMeta.tracks) ? ringMeta.tracks : null;
    if (tracks) {
      // perfect sync: push each track forward by the TRUE age of the data at
      // this rendered frame (packet age varies 0-170ms across the 10Hz packet
      // + 15Hz poll chain — using the live age kills that jitter and makes
      // motion smooth) plus network latency plus the user's by-eye trim,
      // which now covers only the constant render/projector-glass delay
      const sync = ringMeta._sync || {};
      // source-aligned mode: when the lightserver delays its PHYSICAL outputs
      // (output_delay_ms > 0) the timeline is already synchronized at the
      // source — render positions directly, no forward extrapolation at all
      const sourceAligned = (ringMeta.output_delay_ms || 0) > 0;
      const ageMs = sync.received_at ? Math.min(500, Math.max(0, Date.now() - sync.received_at)) : 0;
      const aheadS = sourceAligned ? 0
        : (ageMs + (sync.latency_ms || 0) + (sync.trim_ms || 0)) / 1000;
      for (const tr of tracks) {
        if (out.length >= 8) break;
        const pos = tr.pos + (tr.vel || 0) * aheadS;
        // window-relative u, wrap-aware (win.offset+count may cross the seam)
        let rel = ((pos - win.offset) % RING_PIXELS + RING_PIXELS) % RING_PIXELS;
        const u = rel / win.count;
        if (u < -0.15 || u > 1.15) continue;
        out.push({
          u,
          age: 0.05,                            // live head, riding the top edge
          vel: (tr.vel || 0) / win.count,       // ring px/s -> window units/s
          strength: Math.min(1.5, (tr.brightness ?? 1) * (1 + (tr.size ?? 4) / 16)),
        });
      }
      return out;
    }
    for (const b of st.bursts) {
      out.push({ u: b.u, age: (now - b.t0) / 1000, vel: b.dir, strength: b.strength });
    }
    return out;
  }

  function drawRing(layer, W, H, t) {
    const geo = polygonVAO(layer);
    if (!geo.count) return;
    const c = layer.content || {};
    const prm = layer.params || {};
    const proj = (ringMeta && ringMeta.projector) || sceneProjector || {};   // the Pi conducts
    // calibrated mode: the window comes live from the room calibration
    // (projL/projR named points, seam-wrap aware); manual fields are the
    // fallback when calibration is absent or the zone opts out
    let win = null;
    const cal = ringMeta && ringMeta._calib;
    if (c.calibrated !== false && cal && cal.projL != null && cal.projR != null) {
      const L = Math.round(cal.projL);
      const count = ((Math.round(cal.projR) - L) % RING_PIXELS + RING_PIXELS) % RING_PIXELS || RING_PIXELS;
      win = { offset: L, count };
    } else {
      win = {
        offset: Math.min(Math.max(c.offset ?? 800, 0), RING_PIXELS - 1),
        count: Math.min(Math.max(c.count ?? 480, 1), RING_PIXELS),
      };
    }
    const st = trackRingZone(layer, win);
    const now = performance.now();
    const style = proj.style || c.style || 'react';
    // program pick: Pi-authored glsl wins; then the declarative style; a
    // broken glsl keeps last-good, or falls to the style if none ever worked
    if (proj.glsl) compileWallProgram(proj.glsl);
    const styleP = { bleed: ringP, react: ringReactP, flowfield: ringReactP,
      aurora: ringAuroraP, wash: ringAuroraP, particles: ringParticlesP }[style] || ringReactP;
    const P = (proj.glsl && wall.prog) ? wall.prog : styleP;
    const newFamily = P !== ringP && P !== ringReactP;   // uses uComets/uPrm interface
    const prmIn = proj.params || {};
    const palA = proj.palette && proj.palette[0] ? hexTo01(proj.palette[0]) : mood.palA;
    const palB = proj.palette && proj.palette[1] ? hexTo01(proj.palette[1]) : mood.palB;

    gl.useProgram(P.prog);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, ringTex);
    gl.uniform1i(P.u.uRing, 0);
    gl.uniform2f(P.u.uRes, W, H);
    gl.uniform2f(P.u.uWin, win.offset / RING_PIXELS, win.count / RING_PIXELS);
    gl.uniform1f(P.u.uFlip, c.flip ? 1 : 0);
    gl.uniform1f(P.u.uBrightness, (prm.brightness ?? 1) * (proj.brightness ?? 1));
    gl.uniform1f(P.u.uOpacity, prm.opacity ?? 1);
    gl.uniform1f(P.u.uLive, ringData ? 1 : 0);
    if (P !== ringP) {   // every reactive-family program shares the mood set
      gl.uniform1f(P.u.uTime, t);
      gl.uniform1f(P.u.uEnergy, Math.min(1.5, mood.energy * (proj.energy ?? 1)));
      gl.uniform3f(P.u.uPalA, palA[0], palA[1], palA[2]);
      gl.uniform3f(P.u.uPalB, palB[0], palB[1], palB[2]);
      // sync-debug readback: the effective extrapolation actually used this frame
      if (window.__syncDebug) {
        const sy = (ringMeta && ringMeta._sync) || {};
        const age = sy.received_at ? Date.now() - sy.received_at : 0;
        console.log('[sync] effective extrapolation ms:',
          Math.round(age + (sy.latency_ms || 0) + (sy.trim_ms || 0)));
      }
    }
    if (P === ringReactP) {
      gl.uniform1f(P.u.uTrackLive,
        ringMeta && Array.isArray(ringMeta.tracks) && ringMeta.tracks.length ? 1 : 0);
    }
    if (newFamily) {
      gl.uniform4f(P.u.uPrm, prmIn.density ?? 1, prmIn.speed ?? 1,
        prmIn.turbulence ?? 1, prmIn.hue_drift ?? 0);
      gl.uniform1f(P.u.uFade, style === 'wash' ? (prmIn.fade ?? 1.5) : (prmIn.fade ?? 1));
    }
    const comets = ringComets(st, win, now);
    const bu = new Float32Array(8 * 4);
    for (let i = 0; i < comets.length; i++) {
      const b = comets[i];
      bu.set([b.u, b.age, b.vel, b.strength], i * 4);
    }
    gl.uniform4fv(P.u[newFamily ? 'uComets[0]' : 'uBursts[0]'], bu);
    gl.uniform1i(P.u[newFamily ? 'uCometCount' : 'uBurstCount'], comets.length);
    drawPoly(geo);
  }

  function screenReady() { return screenVideo && screenVideo.readyState >= 2; }

  function uploadScreenFrame() {
    if (screenFrameUploaded) return;
    gl.bindTexture(gl.TEXTURE_2D, screenTex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, screenVideo);
    screenFrameUploaded = true;
  }

  // media cache by URL: { tex, el, kind, ready, w, h }
  const mediaCache = new Map();

  function mediaEntry(url) {
    let entry = mediaCache.get(url);
    if (entry) return entry;
    entry = { tex: makeTexture(), el: null, kind: null, ready: false, w: 1, h: 1 };
    mediaCache.set(url, entry);
    const isVideo = /\.(mp4|webm|mov|m4v)(\?|$)/i.test(url);
    if (isVideo) {
      const v = document.createElement('video');
      v.src = url;
      v.muted = true;
      v.loop = true;
      v.playsInline = true;
      v.autoplay = true;
      v.addEventListener('canplay', () => {
        entry.ready = true;
        entry.w = v.videoWidth || 1;
        entry.h = v.videoHeight || 1;
        v.play().catch(() => {});
      });
      v.play().catch(() => {});
      entry.el = v;
      entry.kind = 'video';
    } else {
      const img = new Image();
      img.onload = () => {
        entry.ready = true;
        entry.w = img.naturalWidth || 1;
        entry.h = img.naturalHeight || 1;
        gl.bindTexture(gl.TEXTURE_2D, entry.tex);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
      };
      img.src = url;
      entry.el = img;
      entry.kind = 'image';
    }
    return entry;
  }

  // cover-fit: scale bbox uv so the media fills the bbox, cropping overflow
  function coverFit(bbW, bbH, mediaW, mediaH) {
    const bbAspect = bbW / bbH;
    const mAspect = mediaW / mediaH;
    if (mAspect > bbAspect) {
      const sx = bbAspect / mAspect;
      return { scale: [sx, 1], offset: [(1 - sx) / 2, 0] };
    }
    const sy = mAspect / bbAspect;
    return { scale: [1, sy], offset: [0, (1 - sy) / 2] };
  }

  function bindTexProgram(P, tex, opacity, brightness, fit) {
    gl.useProgram(P.prog);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.uniform1i(P.u.uTex, 0);
    gl.uniform1f(P.u.uOpacity, opacity);
    gl.uniform1f(P.u.uBrightness, brightness);
    gl.uniform2f(P.u.uUVScale, fit ? fit.scale[0] : 1, fit ? fit.scale[1] : 1);
    gl.uniform2f(P.u.uUVOffset, fit ? fit.offset[0] : 0, fit ? fit.offset[1] : 0);
  }

  // ---- render ---------------------------------------------------------------

  function render(scene, timeSec) {
    const W = scene.width, H = scene.height;
    if (canvas.width !== W || canvas.height !== H) {
      canvas.width = W;
      canvas.height = H;
    }
    gl.viewport(0, 0, W, H);
    gl.clearColor(0, 0, 0, overlay ? 0 : 1);   // overlay: fully transparent ground
    gl.clear(gl.COLOR_BUFFER_BIT);
    screenFrameUploaded = false;

    let sawRing = false;
    sceneProjector = scene.projector || null;   // local fallback; Pi packet wins
    for (const layer of scene.layers) {
      if (!layer.visible || !layer.points || layer.points.length < 3) continue;
      const kind = (layer.content || {}).kind || 'pattern';
      if (kind === 'screen') {
        // overlay mode: screen objects are holes — draw nothing at all and the
        // real desktop underneath the transparent window shows through
        if (!overlay) drawScreen(layer, W, H, timeSec);
      }
      else if (kind === 'media') drawMedia(layer, W, H);
      else if (kind === 'mask') drawMask(layer, W, H);
      else if (kind === 'ring') { sawRing = true; drawRing(layer, W, H, timeSec); }
      else drawPattern(layer, W, H, timeSec);
    }
    // start/stop the ring feed based on actual use
    ringWanted = sawRing;
    if (ringWanted && !ringTimer) ringTimer = setTimeout(pollRing, 0);
  }

  function drawScreen(layer, W, H, t) {
    const prm = layer.params || {};
    const live = screenReady();
    if (live) uploadScreenFrame();

    if (layer.points.length === 4) {
      // true perspective warp — the mapped-monitor look
      const Hm = homographyFromCorners(layer.points);
      const P = live ? quadP : cardP;
      if (live) {
        bindTexProgram(P, screenTex, prm.opacity ?? 1, prm.brightness ?? 1, null);
      } else {
        gl.useProgram(P.prog);
        gl.uniform1f(P.u.uTime, t);
        gl.uniform1f(P.u.uOpacity, prm.opacity ?? 1);
      }
      gl.uniformMatrix3fv(P.u.uH, false, Hm);
      gl.uniform2f(P.u.uRes, W, H);
      gl.bindVertexArray(quadVAO);
      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      gl.bindVertexArray(null);
      return;
    }

    // non-quad polygon: cover-fit the capture into the bbox, clipped
    const geo = polygonVAO(layer);
    if (!geo.count) return;
    if (live) {
      const fit = coverFit(geo.bb.w, geo.bb.h, screenVideo.videoWidth || 16, screenVideo.videoHeight || 9);
      bindTexProgram(mediaP, screenTex, prm.opacity ?? 1, prm.brightness ?? 1, fit);
      gl.uniform2f(mediaP.u.uRes, W, H);
    } else {
      gl.useProgram(cardZoneP.prog);
      gl.uniform2f(cardZoneP.u.uRes, W, H);
      gl.uniform1f(cardZoneP.u.uTime, t);
      gl.uniform1f(cardZoneP.u.uOpacity, prm.opacity ?? 1);
    }
    drawPoly(geo);
  }

  function drawMedia(layer, W, H) {
    const geo = polygonVAO(layer);
    if (!geo.count || !layer.content.url) return;
    const prm = layer.params || {};
    const media = mediaEntry(layer.content.url);
    if (media.kind === 'video' && media.ready) {
      gl.bindTexture(gl.TEXTURE_2D, media.tex);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, media.el);
    }
    const fit = coverFit(geo.bb.w, geo.bb.h, media.w, media.h);
    bindTexProgram(mediaP, media.tex, prm.opacity ?? 1, prm.brightness ?? 1, fit);
    gl.uniform2f(mediaP.u.uRes, W, H);
    drawPoly(geo);
  }

  function drawPattern(layer, W, H, t) {
    const geo = polygonVAO(layer);
    if (!geo.count) return;
    const P = patterns.get(layer.content.pattern);
    if (!P) return;                            // pattern still loading/missing
    const prm = layer.params || {};
    gl.useProgram(P.prog);
    gl.uniform2f(P.u.uRes, W, H);
    gl.uniform1f(P.u.uTime, t);
    gl.uniform1f(P.u.uSpeed, prm.speed ?? 1);
    gl.uniform1f(P.u.uBrightness, prm.brightness ?? 1);
    gl.uniform1f(P.u.uOpacity, prm.opacity ?? 1);
    const c1 = hexToRgb(prm.color1), c2 = hexToRgb(prm.color2);
    gl.uniform3f(P.u.uColor1, c1[0], c1[1], c1[2]);
    gl.uniform3f(P.u.uColor2, c2[0], c2[1], c2[2]);
    const asp = geo.bb.w / geo.bb.h;           // keep noise square in any bbox
    gl.uniform2f(P.u.uAspect, asp >= 1 ? asp : 1, asp >= 1 ? 1 : 1 / asp);
    drawPoly(geo);
  }

  function drawMask(layer, W, H) {
    const geo = polygonVAO(layer);
    if (!geo.count) return;
    gl.useProgram(flatP.prog);
    gl.uniform2f(flatP.u.uRes, W, H);
    gl.uniform4f(flatP.u.uColor, 0, 0, 0, (layer.params || {}).opacity ?? 1);
    drawPoly(geo);
  }

  return { gl, render, setScreenVideo, patterns, overlay };
}

// GORMAPPER scene model + geometry helpers (ES module).
//
// Everything is an object: a polygon with a content assignment.
// {
//   width, height,                       // projector resolution
//   layers: [                            // rendered in array order
//     { id, name, visible,
//       points: [[x,y], ...],            // projector pixels
//       content: {
//         kind: "pattern" | "media" | "screen" | "mask",
//         pattern: "aurora",             // for kind=pattern → patterns/<id>.glsl
//         url: "/media/x.mp4"            // for kind=media
//       },
//       params: { speed, color1, color2, brightness, opacity } }
//   ]
// }
// kind=screen: the shared getDisplayMedia capture. A 4-point polygon gets a
// true homography warp (acts like a monitor); any other point count gets the
// capture cover-fit into the bbox, clipped to the polygon.
// kind=mask: solid black, uses only params.opacity.

export function newSceneId(prefix) {
  return (prefix || 'z') + Math.random().toString(36).slice(2, 8);
}

export function defaultParams() {
  return { speed: 1, color1: '#2ee6c8', color2: '#3d4bff', brightness: 1, opacity: 1 };
}

export function makeZone(points, pattern) {
  return {
    id: newSceneId('z'),
    name: 'Zone',
    visible: true,
    points,
    content: { kind: 'pattern', pattern: pattern || 'aurora', url: null },
    params: defaultParams(),
  };
}

export function makeMask(points) {
  const l = makeZone(points);
  l.id = newSceneId('m');
  l.name = 'Mask';
  l.content = { kind: 'mask', pattern: null, url: null };
  return l;
}

export function makeScreenObject(w, h) {
  const mx = w * 0.25, my = h * 0.25;
  const l = makeZone([[mx, my], [w - mx, my], [w - mx, h - my], [mx, h - my]]);
  l.id = newSceneId('s');
  l.name = 'Screen';
  l.content = { kind: 'screen', pattern: null, url: null };
  return l;
}

export function makeDefaultScene() {
  return { width: 1920, height: 1080, layers: [makeScreenObject(1920, 1080)] };
}

// Fill in anything missing from a loaded scene so hand-edited JSON (and the
// pre-"everything is an object" format) still works.
export function normalizeScene(scene) {
  let s = scene && typeof scene === 'object' ? scene : {};
  // tolerate a draft-shaped object ({ts, scene:{...}}) saved as a scene file
  if (s.scene && !Array.isArray(s.layers) && Array.isArray(s.scene.layers)) s = s.scene;
  s.width = s.width > 0 ? s.width : 1920;
  s.height = s.height > 0 ? s.height : 1080;
  s.layers = Array.isArray(s.layers) ? s.layers : [];
  for (const l of s.layers) {
    if (!l.id) l.id = newSceneId('l');
    if (l.visible === undefined) l.visible = true;
    // ---- migrate old typed layers -----------------------------------------
    if (l.type === 'screen' && Array.isArray(l.corners)) {
      l.points = l.corners;
      l.content = { kind: 'screen', pattern: null, url: null };
      l.params = Object.assign(defaultParams(), {
        opacity: l.opacity ?? 1, brightness: l.brightness ?? 1,
      });
      delete l.corners; delete l.type; delete l.opacity; delete l.brightness;
    } else if (l.type === 'mask') {
      l.content = { kind: 'mask', pattern: null, url: null };
      l.params = Object.assign(defaultParams(), { opacity: l.opacity ?? 1 });
      delete l.type; delete l.opacity;
    } else if (l.type === 'zone') {
      delete l.type;
    }
    // ---- current format ----------------------------------------------------
    l.points = Array.isArray(l.points) ? l.points : [];
    l.content = l.content || { kind: 'pattern', pattern: 'aurora', url: null };
    if (l.content.kind === 'shader') {          // older naming
      l.content.kind = 'pattern';
      l.content.pattern = l.content.pattern || l.content.preset || 'aurora';
      delete l.content.preset;
    }
    l.params = Object.assign(defaultParams(), l.params || {});
  }
  return s;
}

// ---- geometry ---------------------------------------------------------------

export function bbox(points) {
  let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
  for (const [x, y] of points) {
    if (x < x0) x0 = x; if (y < y0) y0 = y;
    if (x > x1) x1 = x; if (y > y1) y1 = y;
  }
  return { x: x0, y: y0, w: Math.max(x1 - x0, 1e-6), h: Math.max(y1 - y0, 1e-6) };
}

function signedArea(pts) {
  let a = 0;
  for (let i = 0, n = pts.length; i < n; i++) {
    const [x0, y0] = pts[i], [x1, y1] = pts[(i + 1) % n];
    a += x0 * y1 - x1 * y0;
  }
  return a / 2;
}

function cross(ax, ay, bx, by, cx, cy) {
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

function pointInTri(px, py, ax, ay, bx, by, cx, cy) {
  const d1 = cross(ax, ay, bx, by, px, py);
  const d2 = cross(bx, by, cx, cy, px, py);
  const d3 = cross(cx, cy, ax, ay, px, py);
  const hasNeg = d1 < 0 || d2 < 0 || d3 < 0;
  const hasPos = d1 > 0 || d2 > 0 || d3 > 0;
  return !(hasNeg && hasPos);
}

// Ear-clipping triangulation for a simple polygon (concave OK, no holes).
// Returns flat index array into `points`. Handles triangles, collinear points,
// and falls back gracefully if clipping stalls (degenerate input).
export function earcut(points) {
  const n = points.length;
  if (n < 3) return [];
  if (n === 3) return [0, 1, 2];

  const idx = [];
  for (let i = 0; i < n; i++) idx.push(i);
  if (signedArea(points) < 0) idx.reverse();   // consistent winding for ear test

  const tris = [];
  let guard = 0;
  const maxGuard = n * n * 2;
  while (idx.length > 3 && guard++ < maxGuard) {
    let clipped = false;
    for (let i = 0; i < idx.length; i++) {
      const i0 = idx[(i + idx.length - 1) % idx.length];
      const i1 = idx[i];
      const i2 = idx[(i + 1) % idx.length];
      const [ax, ay] = points[i0], [bx, by] = points[i1], [cx, cy] = points[i2];
      const area2 = cross(ax, ay, bx, by, cx, cy);
      if (area2 <= 1e-9) continue;             // reflex or collinear — not an ear
      let inside = false;
      for (const j of idx) {
        if (j === i0 || j === i1 || j === i2) continue;
        const [px, py] = points[j];
        if (pointInTri(px, py, ax, ay, bx, by, cx, cy)) { inside = true; break; }
      }
      if (inside) continue;
      tris.push(i0, i1, i2);
      idx.splice(i, 1);
      clipped = true;
      break;
    }
    if (!clipped) {
      // degenerate (self-intersecting / all-collinear): drop the flattest
      // vertex and keep going rather than looping forever
      let best = 0, bestAbs = Infinity;
      for (let i = 0; i < idx.length; i++) {
        const i0 = idx[(i + idx.length - 1) % idx.length];
        const i1 = idx[i];
        const i2 = idx[(i + 1) % idx.length];
        const a = Math.abs(cross(points[i0][0], points[i0][1], points[i1][0], points[i1][1], points[i2][0], points[i2][1]));
        if (a < bestAbs) { bestAbs = a; best = i; }
      }
      idx.splice(best, 1);
    }
  }
  if (idx.length === 3) tris.push(idx[0], idx[1], idx[2]);
  return tris;
}

// Homography mapping the unit square (0,0)(1,0)(1,1)(0,1) onto quad corners
// c[0..3] (projector pixels, in click/JSON order). Returns column-major mat3
// for WebGL, i.e. H * (u, v, 1)^T = (x*w, y*w, w)^T. Closed-form solution of
// the standard 8-DOF DLT system for a unit-square source (Heckbert's
// projective mapping).
export function homographyFromCorners(c) {
  const [x0, y0] = c[0], [x1, y1] = c[1], [x2, y2] = c[2], [x3, y3] = c[3];
  const dx1 = x1 - x2, dy1 = y1 - y2;
  const dx2 = x3 - x2, dy2 = y3 - y2;
  const sx = x0 - x1 + x2 - x3;
  const sy = y0 - y1 + y2 - y3;
  let a, b, cc, d, e, f, g, h;
  if (Math.abs(sx) < 1e-9 && Math.abs(sy) < 1e-9) {
    // parallelogram → affine
    a = x1 - x0; b = x2 - x1; cc = x0;
    d = y1 - y0; e = y2 - y1; f = y0;
    g = 0; h = 0;
  } else {
    const den = dx1 * dy2 - dy1 * dx2;
    g = (sx * dy2 - sy * dx2) / den;
    h = (dx1 * sy - dy1 * sx) / den;
    a = x1 - x0 + g * x1; b = x3 - x0 + h * x3; cc = x0;
    d = y1 - y0 + g * y1; e = y3 - y0 + h * y3; f = y0;
  }
  // row-major H = [a b cc; d e f; g h 1] → column-major for uniformMatrix3fv
  return new Float32Array([a, d, g, b, e, h, cc, f, 1]);
}

// point-in-polygon (even-odd) — editor hit testing
export function pointInPolygon(px, py, pts) {
  let inside = false;
  for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) {
    const [xi, yi] = pts[i], [xj, yj] = pts[j];
    if ((yi > py) !== (yj > py) && px < ((xj - xi) * (py - yi)) / (yj - yi) + xi) {
      inside = !inside;
    }
  }
  return inside;
}

// distance from point to segment + closest point — edge insertion
export function distToSegment(px, py, ax, ay, bx, by) {
  const dx = bx - ax, dy = by - ay;
  const len2 = dx * dx + dy * dy;
  let t = len2 ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0;
  t = Math.max(0, Math.min(1, t));
  const cx = ax + t * dx, cy = ay + t * dy;
  return { dist: Math.hypot(px - cx, py - cy), t, x: cx, y: cy };
}

export function hexToRgb(hex) {
  const m = /^#?([0-9a-f]{6})$/i.exec(hex || '');
  if (!m) return [1, 1, 1];
  const v = parseInt(m[1], 16);
  return [((v >> 16) & 255) / 255, ((v >> 8) & 255) / 255, (v & 255) / 255];
}

/* ship-photo.js — photo mode maths (Q37). Pure: no DOM, no THREE. ship.js owns the camera override, the HUD hide and the PNG save.
   Share link: ?p=<planetId>&d=<x,y,z>&t=<time>&m=<f|l>
     p  node id (string, as pose.id)        d  planet-local position / node scale (unit-ish: 1 = the surface)
     t  time of day 0..1 (0 = the planet's own sun, fraction of a turn about world Y)        m  f = flying, l = landed / on foot */

export var WATERMARK = 'emgor.online/nomansgor';
export var FOV_MIN = 15, FOV_MAX = 100, T_STEP = 1 / 48;
export var PITCH_MIN = 0.12;            // rad from the up axis (never straight over / under the target)

export function clampFov(f) { return f < FOV_MIN ? FOV_MIN : (f > FOV_MAX ? FOV_MAX : (f === f ? f : 60)); }
/* wheel deltaY (px or lines) -> new fov; scrolling up (negative) zooms in. */
export function fovFromWheel(fov, deltaY) { return clampFov(fov * Math.exp((deltaY > 0 ? 1 : (deltaY < 0 ? -1 : 0)) * 0.08)); }
export function wrapT(t) { t = +t; if (!isFinite(t)) return 0; t = t - Math.floor(t); return t >= 1 ? 0 : t; }
export function stepT(t, dir) { return wrapT(wrapT(t) + (dir > 0 ? T_STEP : -T_STEP)); }

/* sun: rotate unit base (x,y,z) about world Y by t turns; writes into out ({x,y,z}) and returns it. t = 0 returns base untouched. */
export function sunAt(bx, by, bz, t, out) {
  var a = wrapT(t) * Math.PI * 2, c = Math.cos(a), s = Math.sin(a);
  out.x = bx * c + bz * s; out.y = by; out.z = -bx * s + bz * c;
  return out;
}

/* orbitStep: rotate offset off (x,y,z array-like) about unit up by dyaw, then tilt toward / away from up by dpitch,
   keeping the angle to up in [PITCH_MIN, PI - PITCH_MIN]. Length preserved. Writes into out. */
export function orbitStep(off, up, dyaw, dpitch, out) {
  var ox = off[0], oy = off[1], oz = off[2], ux = up[0], uy = up[1], uz = up[2];
  var len = Math.sqrt(ox * ox + oy * oy + oz * oz), c, s, d, cx, cy, cz, rx, ry, rz, ang, na, ax, ay, az, al, k;
  if (!(len > 0)) { out[0] = 0; out[1] = 0; out[2] = 0; return out; }
  c = Math.cos(dyaw); s = Math.sin(dyaw);                       // Rodrigues about up
  d = ux * ox + uy * oy + uz * oz;
  cx = uy * oz - uz * oy; cy = uz * ox - ux * oz; cz = ux * oy - uy * ox;
  rx = ox * c + cx * s + ux * d * (1 - c); ry = oy * c + cy * s + uy * d * (1 - c); rz = oz * c + cz * s + uz * d * (1 - c);
  ang = Math.acos(Math.max(-1, Math.min(1, (ux * rx + uy * ry + uz * rz) / len)));
  na = Math.max(PITCH_MIN, Math.min(Math.PI - PITCH_MIN, ang + dpitch));
  if (na !== ang) {
    ax = uy * rz - uz * ry; ay = uz * rx - ux * rz; az = ux * ry - uy * rx;          // axis = up x r (tilts r away from up for +angle)
    al = Math.sqrt(ax * ax + ay * ay + az * az);
    if (al < 1e-9) { ax = -uz; ay = 0; az = ux; al = Math.sqrt(ax * ax + az * az) || 1; }      // r parallel to up: any perpendicular
    ax /= al; ay /= al; az /= al;
    c = Math.cos(na - ang); s = Math.sin(na - ang);
    d = ax * rx + ay * ry + az * rz;
    cx = ay * rz - az * ry; cy = az * rx - ax * rz; cz = ax * ry - ay * rx;
    k = 1 - c;
    ox = rx * c + cx * s + ax * d * k; oy = ry * c + cy * s + ay * d * k; oz = rz * c + cz * s + az * d * k;
  } else { ox = rx; oy = ry; oz = rz; }
  k = len / (Math.sqrt(ox * ox + oy * oy + oz * oz) || 1);
  out[0] = ox * k; out[1] = oy * k; out[2] = oz * k;
  return out;
}

function r3(v) { return (Math.round(v * 1000) / 1000).toString(); }
/* shareQuery: -> '?p=..&d=..&t=..&m=..' or '' when the inputs are unusable. */
export function shareQuery(id, d, t, mode) {
  if (id === undefined || id === null || id === '' || !d || d.length < 3) return '';
  for (var i = 0; i < 3; i++) if (!isFinite(d[i])) return '';
  return '?p=' + encodeURIComponent(String(id)) + '&d=' + r3(d[0]) + ',' + r3(d[1]) + ',' + r3(d[2]) + '&t=' + r3(wrapT(t)) + '&m=' + (mode === 'l' ? 'l' : 'f');
}
/* parseShare: location.search -> { id, d:[x,y,z], t, mode } or null. Rejects non-finite, zero-length and absurd (> 50 radii) positions. */
export function parseShare(search) {
  if (typeof search !== 'string' || search.length > 400) return null;
  var o = {}, parts = search.replace(/^\?/, '').split('&'), i, kv, v;
  for (i = 0; i < parts.length; i++) { kv = parts[i].split('='); if (kv.length === 2) { try { o[kv[0]] = decodeURIComponent(kv[1]); } catch (e) { return null; } } }
  if (!o.p || !o.d || o.p.length > 64) return null;
  v = o.d.split(',').map(Number);
  if (v.length !== 3 || !v.every(isFinite)) return null;
  var len = Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (!(len > 0.01) || len > 50) return null;
  return { id: o.p, d: v, t: o.t === undefined ? 0 : wrapT(o.t), mode: o.m === 'l' ? 'l' : 'f' };
}

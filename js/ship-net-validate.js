/* ship-net-validate.js — pure receive-boundary checks for untrusted relay peers (no THREE, no DOM).
   A peer can send anything the JSON grammar allows; every number that reaches the frame loop goes through here. */
export const POS_LIM = 1e9;      // world units; far above any galaxy coordinate, far below float trouble
export function num(v, lim, d) { return (typeof v === 'number' && isFinite(v) && Math.abs(v) <= lim) ? v : d; }
// pose sample or null. Quaternion must be finite and non-degenerate (renormalised), position bounded.
export function cleanPose(m) {
    if (!m || typeof m !== 'object') return null;
    var x = num(m.x, POS_LIM, NaN), y = num(m.y, POS_LIM, NaN), z = num(m.z, POS_LIM, NaN);
    var qx = num(m.qx, 2, NaN), qy = num(m.qy, 2, NaN), qz = num(m.qz, 2, NaN), qw = num(m.qw, 2, NaN);
    if (x !== x || y !== y || z !== z || qx !== qx || qy !== qy || qz !== qz || qw !== qw) return null;
    var l = Math.sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    if (l < 1e-3) return null;
    return { x: x, y: y, z: z, qx: qx / l, qy: qy / l, qz: qz / l, qw: qw / l, v: num(m.v, 1e6, 0), st: num(m.st, 255, 0) | 0, hp: num(m.hp, 255, 100) };
}
export function cleanName(n, d) { return (typeof n === 'string' && n.trim()) ? n.replace(/[\x00-\x1f\x7f]/g, '').slice(0, 24) : (d || 'PILOT'); }
export function cleanColor(c) { return (typeof c === 'number' && isFinite(c) && c >= 0 && c <= 0xffffff) ? (c | 0) : null; }
// fire: ox,oy,oz,dx,dy,dz all finite + bounded, else null
export function cleanFire(m) {
    if (!m || typeof m !== 'object') return null;
    var a = [num(m.x, POS_LIM, NaN), num(m.y, POS_LIM, NaN), num(m.z, POS_LIM, NaN), num(m.dx, 1e3, NaN), num(m.dy, 1e3, NaN), num(m.dz, 1e3, NaN)];
    for (var i = 0; i < 6; i++) if (a[i] !== a[i]) return null;
    return { w: num(m.w, 64, 0) | 0, x: a[0], y: a[1], z: a[2], dx: a[3], dy: a[4], dz: a[5] };
}

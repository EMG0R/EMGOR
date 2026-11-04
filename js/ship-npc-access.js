// Q29: pure NPC accessibility rules (no THREE). Village metres, x/z plane.
//   inCone(px,pz,fx,fz,nx,nz,r,deg,near) -> true when the NPC is within r AND within deg of the facing (or within `near` regardless)
//   segHitsBox(ax,az,bx,bz,box)          -> straight walk blocked by a wall box {min:{x,z},max:{x,z}}
//   placementReport(npcs, walls, centre, opts) -> { n, outside, outsideFrac, inside[], unreachable[], badSpawn[], pass }
export function inCone(px, pz, fx, fz, nx, nz, r, deg, near) {
  const dx = nx - px, dz = nz - pz, d = Math.hypot(dx, dz);
  if (d > r) return false;
  if (near != null && d <= near) return true;
  const fl = Math.hypot(fx, fz); if (fl < 1e-6 || d < 1e-6) return true;
  return (dx * fx + dz * fz) / (d * fl) >= Math.cos(deg * Math.PI / 180);
}
export function segHitsBox(ax, az, bx, bz, box) {
  let t0 = 0, t1 = 1; const dx = bx - ax, dz = bz - az;
  const ax_ = [[ax, dx, box.min.x, box.max.x], [az, dz, box.min.z, box.max.z]];
  for (const [o, d, lo, hi] of ax_) {
    if (Math.abs(d) < 1e-9) { if (o < lo || o > hi) return false; continue; }
    let a = (lo - o) / d, b = (hi - o) / d; if (a > b) { const t = a; a = b; b = t; }
    t0 = Math.max(t0, a); t1 = Math.min(t1, b); if (t0 > t1) return false;
  }
  return true;
}
const inBox = (x, z, b, m) => x > b.min.x - m && x < b.max.x + m && z > b.min.z - m && z < b.max.z + m;
// npcs: [{ x, z, y?, inside? }]; walls: boxes with min/max {x,y,z}. opts: minOutside .7, maxRadius 40*H(m), maxSlope .35 (slopeAt(x,z) optional), water(x,z) optional, clear .25
export function placementReport(npcs, walls, centre, opts) {
  opts = opts || {}; const minOut = opts.minOutside != null ? opts.minOutside : 0.7, maxR = opts.maxRadius != null ? opts.maxRadius : 70, clr = opts.clear != null ? opts.clear : 0.25;
  const cx = centre ? centre[0] : 0, cz = centre ? centre[1] : 0;
  const rep = { n: npcs.length, outside: 0, outsideFrac: 1, inside: [], unreachable: [], badSpawn: [], pass: true };
  for (let i = 0; i < npcs.length; i++) {
    const v = npcs[i]; if (v.inside) continue; rep.outside++;
    if (walls.some((b) => (v.y == null || (v.y <= b.max.y && v.y + 2 >= b.min.y)) && inBox(v.x, v.z, b, clr * 0.5))) rep.inside.push(i);
    else if (walls.some((b) => (v.y == null || (v.y <= b.max.y && v.y + 2 >= b.min.y)) && segHitsBox(cx, cz, v.x, v.z, b))) rep.unreachable.push(i);
    if (Math.hypot(v.x - cx, v.z - cz) > maxR || (opts.slopeAt && opts.slopeAt(v.x, v.z) > (opts.maxSlope != null ? opts.maxSlope : 0.35)) || (opts.water && opts.water(v.x, v.z))) rep.badSpawn.push(i);
  }
  rep.outsideFrac = npcs.length ? rep.outside / npcs.length : 1;
  rep.pass = rep.outsideFrac >= minOut && !rep.inside.length && !rep.unreachable.length && !rep.badSpawn.length;
  return rep;
}

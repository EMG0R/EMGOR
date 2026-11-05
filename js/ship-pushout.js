// ship-pushout.js — pure sphere-vs-AABB-list push-out (Q47). ship.js opBoxes() delegates here so walking and jetpack flight resolve against the same solids.
//   pushBoxes(p{x,y,z}, boxes[{min,max}], rs, passes = 3) -> true if p moved; p is pushed out IN PLACE, no allocations.
// One pass can leave p inside a neighbouring box (corner of two walls, a shelf against a wall), so it repeats until a pass moves nothing.
export function pushBoxesOnce(p, arr, rs) {
    var j, q, moved = false, cx, cy, cz, dx, dy, dz, d2, d, k, l, r, t, bb, u, dd, m;
    if (!arr) return false;
    for (j = 0; j < arr.length; j++) {
        q = arr[j];
        if (p.x < q.min.x - rs || p.x > q.max.x + rs || p.y < q.min.y - rs || p.y > q.max.y + rs || p.z < q.min.z - rs || p.z > q.max.z + rs) continue;
        cx = p.x < q.min.x ? q.min.x : p.x > q.max.x ? q.max.x : p.x; cy = p.y < q.min.y ? q.min.y : p.y > q.max.y ? q.max.y : p.y; cz = p.z < q.min.z ? q.min.z : p.z > q.max.z ? q.max.z : p.z;
        dx = p.x - cx; dy = p.y - cy; dz = p.z - cz; d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= rs * rs) continue;
        moved = true;
        if (d2 > 1e-14) { d = Math.sqrt(d2); k = (rs - d) / d; p.x += dx * k; p.y += dy * k; p.z += dz * k; }
        else {      // centre inside the box: leave by the shallowest face
            l = p.x - q.min.x; r = q.max.x - p.x; t = p.z - q.min.z; bb = q.max.z - p.z; u = p.y - q.min.y; dd = q.max.y - p.y;
            m = Math.min(l, r, t, bb, u, dd);
            if (m === l) p.x = q.min.x - rs; else if (m === r) p.x = q.max.x + rs; else if (m === t) p.z = q.min.z - rs; else if (m === bb) p.z = q.max.z + rs; else if (m === u) p.y = q.min.y - rs; else p.y = q.max.y + rs;
        }
    }
    return moved;
}
export function pushBoxes(p, arr, rs, passes) {
    var n = passes || 3, any = false;
    while (n-- > 0 && pushBoxesOnce(p, arr, rs)) any = true;
    return any;
}

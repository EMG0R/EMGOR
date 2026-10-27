/* ship-keys.js — small pure helpers shared by ship.js / ship-net.js (node-testable, no DOM).
   roomKey: relay planet-room key, <= 24 chars (server/nmg-relay planetKey regex {1,24}). Ids that fit pass through
   unchanged (old clients stay compatible); longer ids become 15 chars + '_' + 8-hex FNV-1a of the FULL id, so
   siblings that share a long prefix no longer collapse into one room. Same function on every client. */
export function hash8(s) {
  var h = 0x811c9dc5 >>> 0, i;
  for (i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 0x01000193) >>> 0; }
  return ('00000000' + h.toString(16)).slice(-8);
}
export function roomKey(id) {
  var s = String(id).replace(/[^A-Za-z0-9_.:-]/g, '_');
  return s.length <= 24 ? s : s.slice(0, 15) + '_' + hash8(s);
}
/* putRecent: store map[key] = val as the MOST recently used entry (re-inserting moves it to the end of key order),
   then evict least-recently-written keys down to `max`. Never evicts `key`. */
export function putRecent(map, key, val, max) {
  delete map[key]; map[key] = val;
  var ks = Object.keys(map), i;
  for (i = 0; ks.length - i > max && i < ks.length; i++) if (ks[i] !== key) delete map[ks[i]];
  return map;
}
/* cooldownMax: a new cooldown never shortens an existing one. */
export function cooldownMax(cur, until) { return Math.max(cur || 0, until); }

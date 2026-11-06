// Refcounted geometry cache (pure: no THREE). ship-human.js uses it for the villager LOD so every villager of one style shares one merged BufferGeometry set
// (colours are baked per vertex, so the key carries the theme colour). acquire(key, build) -> value (build runs on a miss only); release(key, free) frees at refs 0.
export const VILLAGER_TRI_BUDGET = 800;
export function createGeoCache() {
  const m = new Map(); let hits = 0, misses = 0;
  return {
    acquire(key, build) {
      let e = m.get(key);
      if (e) { e.refs++; hits++; return e.val; }
      misses++; e = { val: build(), refs: 1 }; m.set(key, e); return e.val;
    },
    release(key, free) {
      const e = m.get(key); if (!e) return;
      if (--e.refs <= 0) { m.delete(key); if (free) free(e.val); }
    },
    stats() { return { hits, misses, size: m.size, rate: hits / Math.max(1, hits + misses) }; },
    refs(key) { const e = m.get(key); return e ? e.refs : 0; },
    clear() { m.clear(); hits = misses = 0; },
  };
}
export const VILLAGER_GEO = createGeoCache();

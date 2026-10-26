// ship-depth.js — pure fractal-depth logic (docs/next-stage-plan.md B1). No DOM, no THREE, no Math.random.
//   microSeed(parentId, rockIndex)      -> stable id string 'm<hash>' used as the micro system's node id / seed
//   microSpec(parentId, rockIndex, d)   -> { id, seed, moons, depth, micro:true, g, day } for engine.makeBody (caller adds sysR / parentNode)
//   scale(depth)                        -> { g, day, loot, villageF } content scaling by depth (g x 0.85^d, loot tier = depth)
//   createDive() state machine          -> start(t, rock) / cancel() / tick(dt) / phase: 'idle' | 'hold' | 'dive' | 'in' | 'climb'
//   shouldClimb(distL, R)               -> true once the player is >= 3 R from the micro planet centre
//   push / pop / toSave / fromSave      -> profile.depthStack = [{ planetId, rockIndex }]
export const HOLD_T = 0.6, DIVE_T = 2, CLIMB_R = 3, MAX_DEPTH = 1;     // B1 = one level; B2 raises MAX_DEPTH

export function hash32(s) { s = String(s); let h = 2166136261 >>> 0; for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619) >>> 0; } return h >>> 0; }
export function microSeed(parentId, rockIndex) { return 'm' + hash32(String(parentId) + '#' + (rockIndex | 0)).toString(36); }
export function scale(depth) { const d = Math.max(0, depth | 0); return { g: Math.pow(0.85, d), day: Math.pow(0.7, d), loot: 1 + d, villageF: Math.pow(0.8, d) }; }
export function microSpec(parentId, rockIndex, depth) {
  const id = microSeed(parentId, rockIndex), h = hash32(id), d = Math.max(1, depth | 0), s = scale(d);
  return { id, seed: h, moons: h % 4, depth: d, micro: true, g: s.g, day: s.day, parentId: String(parentId), rockIndex: rockIndex | 0 };
}
export const shouldClimb = (distL, R) => distL >= CLIMB_R * R;
// dive progress 0..1 eased (camera dive + fov pull + patch -> dome crossfade all read this)
export const diveEase = (t) => { t = t < 0 ? 0 : t > 1 ? 1 : t; return t * t * (3 - 2 * t); };

// eligible = on foot + crouched + a rock under the crosshair; hold Z HOLD_T, then DIVE_T of transition, then 'in' until climb
export function createDive() {
  const st = { phase: 'idle', t: 0, rock: -1, k: 0 };
  return {
    st,
    start(rock) { if (st.phase === 'idle') { st.phase = 'hold'; st.t = 0; st.rock = rock | 0; st.k = 0; } return st.phase; },
    cancel() { if (st.phase === 'hold' || st.phase === 'dive') { st.phase = 'idle'; st.t = 0; st.k = 0; st.rock = -1; } return st.phase; },
    // returns the phase; 'dive' entered exactly when the hold completes; 'in' when the dive completes (caller swaps bodies on that edge)
    tick(dt) {
      if (st.phase === 'hold') { st.t += dt; if (st.t >= HOLD_T) { st.phase = 'dive'; st.t = 0; } }
      else if (st.phase === 'dive') { st.t += dt; st.k = diveEase(st.t / DIVE_T); if (st.t >= DIVE_T) { st.phase = 'in'; st.k = 1; st.t = 0; } }
      return st.phase;
    },
    climb() { if (st.phase === 'in') { st.phase = 'climb'; st.t = 0; st.k = 1; } return st.phase; },
    // climb finished (reverse ease), back to idle; returns the rock to re-place under the player
    land(dt) { if (st.phase !== 'climb') return st.phase; st.t += dt; st.k = 1 - diveEase(st.t / DIVE_T); if (st.t >= DIVE_T) { st.phase = 'idle'; st.k = 0; } return st.phase; }
  };
}

export function push(stack, planetId, rockIndex) {
  if (stack.length >= MAX_DEPTH) return false;
  stack.push({ planetId: String(planetId), rockIndex: rockIndex | 0 }); return true;
}
export function pop(stack) { return stack.pop() || null; }
export function toSave(stack) { return stack.map(e => ({ planetId: e.planetId, rockIndex: e.rockIndex })); }
export function fromSave(a) {
  const out = [];
  if (!Array.isArray(a)) return out;
  for (const e of a) { if (out.length >= MAX_DEPTH) break; if (e && typeof e.planetId === 'string' && Number.isFinite(e.rockIndex) && e.rockIndex >= 0) out.push({ planetId: e.planetId, rockIndex: e.rockIndex | 0 }); }
  return out;
}

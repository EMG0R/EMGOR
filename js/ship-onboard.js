// ship-onboard.js — Q26 first-five-minutes chain for NO MANS GOR. Pure data + reducers, no three.js, node-testable.
//   A new profile walks a 7-step chain; each step is one quest-board (tracker) entry with an optional HUD marker kind.
//   Steps are predicates over a snapshot of the game, so the chain can never get stuck on a missed event and skips ahead
//   when the profile already did the thing. State lives on profile.onb.c = { i: step index, won: bool, card: bool }.
//   snap = { planet, landed, beacon, ball, caught, won, station }  (all booleans, cumulative where the game keeps it)
export const STEPS = [
  { id: 'planet', title: 'REACH A PLANET', text: 'HOLD W + MOUSE · FLY TOWARD A PLANET, HOLD SPACE TO PULSE', mark: '7/11', when: 'fly' },
  { id: 'land', title: 'LAND', text: 'FLY UNDER 3 L OF THE SURFACE (THE SHIP SLOWS ITSELF), PRESS Z', mark: '', when: 'fly' },
  { id: 'beacon', title: 'WALK TO THE BEACON', text: 'Z · STEP OUT · WALK TO THE 7/11 BEACON', mark: '7/11', when: 'any' },
  { id: 'ball', title: 'BUY A GOR BALL', text: 'F AT THE 7/11 COUNTER · BUY A GOR BALL', mark: '7/11', when: 'any' },
  { id: 'catch', title: 'CATCH ONE', text: 'HURT A CREATURE, THEN THROW THE BALL (HOTBAR)', mark: '', when: 'any' },
  { id: 'fight', title: 'WIN A FIGHT', text: 'WALK UP TO A VILLAGER · F TWICE · FIGHT', mark: 'village', when: 'any' },
  { id: 'station', title: 'FIND THE STATION', text: 'FLY TO THE INTERGALACTIC 7/11 STATION (HUD MARKER) · Z TO DOCK', mark: 'station', when: 'fly' },
];
export const FAR_TEXT = 'THE 7/11 IS FAR (~1 L/S ON FOOT) · Z BOARD · FLY LOW TOWARD THE 7/11 MARKER · LAND BESIDE IT · Z STEP OUT';
export function textOf(s, far) { return s ? (far && s.id === 'beacon' ? FAR_TEXT : s.text) : ''; }
export const START_CARD = ['FLY · HOLD W, STEER WITH THE MOUSE', 'LAND · FLY LOW OVER A PLANET, PRESS Z', 'F = FIGHT · T = TALK'];

export function fresh(done) { return { i: done ? STEPS.length : 0, won: !!done, card: !!done }; }
export function norm(c, done) {
  if (!c || typeof c !== 'object' || Array.isArray(c)) return fresh(done);
  c.i = Math.max(0, Math.min(STEPS.length, c.i | 0)); c.won = !!c.won; c.card = !!c.card; return c;
}
export function finished(c) { return !c || c.i >= STEPS.length; }
function stepDone(id, s) {
  if (id === 'planet') return !!(s.planet || s.landed);
  if (id === 'land') return !!s.landed;
  if (id === 'beacon') return !!(s.beacon || s.ball || s.caught);
  if (id === 'ball') return !!(s.ball || s.caught);
  if (id === 'catch') return !!s.caught;
  if (id === 'fight') return !!s.won;
  if (id === 'station') return !!s.station;
  return false;
}
// advance as far as the snapshot allows -> number of steps gained (0 = nothing changed)
export function advance(c, snap) {
  let n = 0;
  while (c.i < STEPS.length && stepDone(STEPS[c.i].id, snap || {})) { c.i++; n++; }
  return n;
}
export function current(c) { return c && c.i < STEPS.length ? STEPS[c.i] : null; }
export function markKind(c) { const s = current(c); return s ? s.mark : ''; }
// tracker entry: [title line, objective line]
export function entry(c, far) {
  const s = current(c); if (!s) return null; const t = textOf(s, far);
  return ['FIRST STEPS ' + (c.i + 1) + '/' + STEPS.length + ' · ' + s.title, t.charAt(0) + t.slice(1).toLowerCase()];
}

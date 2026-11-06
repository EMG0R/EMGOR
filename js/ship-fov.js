/* ship-fov.js - the one place the game's vertical FOV is decided. Pure: mode in, degrees out.
   galaxy / station / landed / docking = base (42) · foot (incl. fight: the fight camera is positioned, never zoomed) = 55 · fly = base + 8 + adds, capped at 62. */
export var FOV_FOOT = 55, FOV_FLY_ADD = 8, FOV_FLY_CAP = 62;
export function targetFov(p) {
  var base = p.base, f;
  if (p.mode === 'foot') return FOV_FOOT;
  if (p.mode !== 'fly') return base;
  f = base + FOV_FLY_ADD + (p.boost || 0) + (p.pulse || 0) + (p.focus || 0) + (p.kick || 0) + (p.atm || 0) + (p.planet || 0) + ((p.sty || 1) - 1) * base;
  return f < base ? base : (f > FOV_FLY_CAP ? FOV_FLY_CAP : (f === f ? f : base));
}

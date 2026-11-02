// ship-scale.js: the one definition of how tall a human is next to the ship (Q21).
// H_RATIO = human height as a fraction of ship length L. 0.2 -> the ship is ~5 humans long (was 0.09 = 11).
// ship.js hOf(L), the world GM (world units per metre) and the planet store / village / hub scale all derive from it.
// Fixed on purpose at the old 0.09 (separate "bigger on the inside" spaces modelled at that scale): ship interior (ship.js interiorStep),
// station deck (ship.js SK_H, ship-station.js HUMAN_H).
export const H_RATIO = 0.2;
export const HUMAN_M = 1.8;                         // a human is 1.8 m tall
export const GM_PER_L = H_RATIO / HUMAN_M;          // world units per metre, divided by L (ship.js multiplies by L)
export function hOf(len) { return H_RATIO * len; }

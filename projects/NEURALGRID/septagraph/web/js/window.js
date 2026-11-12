/**
 * Window projection: device space -> coordinate space (spec §C /
 * WINDOW_MATH_v1.md). Byte-for-byte-of-logic port of
 * `septagraph/py/septagraph/window.py`.
 *
 * A window is axis-aligned: `center[8]`, `extent[8]` (half-widths, >= 0),
 * and an *assignment* mapping controller channels to coordinate indices.
 * Windows live only in the controller; they never travel on the wire.
 */

const K = 9;   // v2: + el (elevation)

function clamp01(x) {
  x = Number(x);
  if (Number.isNaN(x)) return 0.0;
  return x < 0.0 ? 0.0 : x > 1.0 ? 1.0 : x;
}

/**
 * A controller input channel.
 * `{name, dims, kind: "discrete"|"continuous", size: number[]|null}`
 */
export function makeChannel(name, dims, kind, size = null) {
  return { name, dims, kind, size };
}

/**
 * `InputManifest.load` equivalent: parse a JSON object matching
 * `{"controller_id": ..., "channels": [{"name","dims","kind","size"}, ...]}`.
 * Accepts either a parsed object or a JSON string.
 */
export function loadInputManifest(data) {
  const obj = typeof data === "string" ? JSON.parse(data) : data;
  const channels = obj.channels.map((c) => ({
    name: c.name,
    dims: c.dims,
    kind: c.kind,
    size: c.size != null ? Array.from(c.size) : null,
  }));
  return { controllerId: obj.controller_id, channels };
}

/**
 * A window: `{center: number[8], extent: number[8], assign: {[channelName]: number[]}}`
 * `assign[channelName][dim]` is the coordinate index that device dim maps
 * to, or -1 (or a nullish value) for "unassigned".
 */

/**
 * project(window, channel, d, velocity) -> number[8]
 *
 * Projects a device coordinate `d` (length == channel.dims) into K-space.
 * Unassigned indices take `mu[a] = center[a]`. Velocity maps to `loud`
 * (index 1) unless the window assigns `loud` elsewhere.
 */
export function project(win, channel, d, velocity = null) {
  const mu = win.center.slice();
  const idxs = (win.assign && win.assign[channel.name]) || new Array(channel.dims).fill(-1);

  for (let dim = 0; dim < idxs.length; dim++) {
    const a = idxs[dim];
    if (a === null || a === undefined || a < 0) continue;
    mu[a] = clamp01(win.center[a] + (2.0 * d[dim] - 1.0) * win.extent[a]);
  }

  const loudAssigned = idxs.some((a) => a === 1);
  if (velocity !== null && velocity !== undefined && !loudAssigned) {
    mu[1] = clamp01(win.center[1] - win.extent[1] + 2.0 * win.extent[1] * velocity);
  }

  return mu.map(clamp01);
}

/**
 * modulate(mu, idx, delta, window) -> number[8]
 *
 * Applies a controller-side bipolar modulator delta (softpot slide, lidar
 * height) to axis `idx`: `mu[a] += delta * extent[a]`, then clamps.
 */
export function modulate(mu, idx, delta, win) {
  delta = Math.max(-1.0, Math.min(1.0, Number(delta)));
  const out = mu.slice();
  out[idx] = clamp01(out[idx] + delta * win.extent[idx]);
  return out;
}

function infDist(a, b) {
  let m = 0;
  for (let i = 0; i < a.length; i++) m = Math.max(m, Math.abs(a[i] - b[i]));
  return m;
}

function* iterCells(dimsLeft, prefix) {
  if (dimsLeft.length === 0) {
    yield prefix.slice();
    return;
  }
  const [head, ...rest] = dimsLeft;
  for (let i = 0; i < head; i++) {
    prefix.push(i);
    yield* iterCells(rest, prefix);
    prefix.pop();
  }
}

/**
 * cellTable(window, channel, catalog = [], snapRadius = 0.06)
 *   -> Map<string "i,j,...", {cell: number[], mu: number[8], point: CatalogPoint|null}>
 *
 * Precomputes, for a discrete channel, the (mu, snapped-point-or-null) per
 * cell. Each cell's projected `mu` is replaced-for-lookup by the nearest
 * catalog point when `distInf(mu, point.mu) <= snapRadius`, so `/hit`
 * becomes a lookup rather than a search. Mirrors
 * `septagraph.window.cell_table`; the dict key (a Python tuple) becomes a
 * comma-joined string key here since JS Maps compare object/array keys by
 * identity, not value — use `cell.join(",")` to look up a specific cell.
 */
export function cellTable(win, channel, catalog = [], snapRadius = 0.06) {
  if (channel.kind !== "discrete" || !channel.size) {
    throw new Error("cellTable requires a discrete channel with a size");
  }

  const table = new Map();
  const size = channel.size;

  for (const cell of iterCells(size, [])) {
    const d = cell.map((i, dim) => (i + 0.5) / size[dim]);
    const mu = project(win, channel, d);

    let best = null;
    let bestDist = null;
    for (const p of catalog) {
      const dist = infDist(mu, p.mu);
      if (dist <= snapRadius && (bestDist === null || dist < bestDist)) {
        best = p;
        bestDist = dist;
      }
    }

    table.set(cell.join(","), { cell, mu, point: best });
  }

  return table;
}

export const _K = K;

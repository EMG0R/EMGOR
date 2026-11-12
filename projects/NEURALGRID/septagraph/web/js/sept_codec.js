/**
 * OSC encode/decode for the septagraph wire grammar (spec §B / WIRE_FORMAT_v1.md).
 *
 * Byte-for-byte port of `septagraph/py/septagraph/codec.py`. Every address
 * begins `/sept/v1/`. Type tags: `s` string, `i` int32, `f` float32, arrays
 * of 8 floats as 8 consecutive `f`.
 *
 * Uses `osc.js` (https://github.com/colinbdclark/osc.js), loaded as a UMD
 * global via:
 *
 *   <script src="https://cdn.jsdelivr.net/npm/osc@2.4.5/dist/osc-browser.min.js"></script>
 *
 * ...before this module. `osc.writePacket`/`osc.readPacket` are used with
 * explicit `{type, value}` tags so the wire bytes match `python-osc`
 * exactly (verified against the golden vectors in `septagraph/golden/`).
 */

const oscLib =
  (typeof globalThis !== "undefined" && globalThis.osc) ||
  (typeof window !== "undefined" && window.osc) ||
  null;

if (!oscLib) {
  throw new Error(
    "sept_codec.js: global `osc` (osc.js UMD) not found — load osc-browser(.min).js before this module"
  );
}

const K = 9;   // v2: + el (elevation)

function clamp01(x) {
  x = Number(x);
  if (Number.isNaN(x)) return 0.0;
  return x < 0.0 ? 0.0 : x > 1.0 ? 1.0 : x;
}

function fArgs(vec) {
  const out = new Array(K);
  for (let i = 0; i < K; i++) out[i] = { type: "f", value: Number(vec[i]) };
  return out;
}

export const ADDR_HIT = "/sept/v1/hit";
export const ADDR_MOVE = "/sept/v1/move";
export const ADDR_SHAPE = "/sept/v1/shape";
export const ADDR_MANIFEST_BEGIN = "/sept/v1/manifest/begin";
export const ADDR_MANIFEST_BOUNDS = "/sept/v1/manifest/bounds";
export const ADDR_MANIFEST_POINT = "/sept/v1/manifest/point";
export const ADDR_MANIFEST_END = "/sept/v1/manifest/end";
export const ADDR_MANIFEST_REQUEST = "/sept/v1/manifest/request";

// --- encode -----------------------------------------------------------

/** encodeHit({voiceId, layer, onset, velocity, mu, sigma}) -> Uint8Array */
export function encodeHit({ voiceId, layer, onset, velocity, mu, sigma }) {
  const args = [
    { type: "s", value: String(voiceId) },
    { type: "i", value: layer | 0 },
    { type: "i", value: onset ? 1 : 0 },
    { type: "f", value: Number(velocity) },
    ...fArgs(mu),
    ...fArgs(sigma),
  ];
  return oscLib.writePacket({ address: ADDR_HIT, args });
}

/** encodeMove({voiceId, mu}) -> Uint8Array */
export function encodeMove({ voiceId, mu }) {
  const args = [{ type: "s", value: String(voiceId) }, ...fArgs(mu)];
  return oscLib.writePacket({ address: ADDR_MOVE, args });
}

/** encodeShape({shapeId, layer, durationS, knots}) -> Uint8Array
 *  knots: Array<{t, mu, sigma}>
 */
export function encodeShape({ shapeId, layer, durationS, knots }) {
  const args = [
    { type: "s", value: String(shapeId) },
    { type: "i", value: layer | 0 },
    { type: "f", value: Number(durationS) },
    { type: "i", value: knots.length | 0 },
  ];
  for (const knot of knots) {
    args.push({ type: "f", value: Number(knot.t) });
    args.push(...fArgs(knot.mu));
    args.push(...fArgs(knot.sigma));
  }
  return oscLib.writePacket({ address: ADDR_SHAPE, args });
}

/** encodeManifestRequest() -> Uint8Array */
export function encodeManifestRequest() {
  return oscLib.writePacket({ address: ADDR_MANIFEST_REQUEST, args: [] });
}

/**
 * encodeManifest({rendererId, kind, name, lo, hi, points}) -> Uint8Array
 *
 * One OSC bundle: begin, bounds|points..., end (spec §B). Not part of the
 * required minimal export list for Task 2.2, but needed to reproduce the
 * `manifest_continuous.osc` / `manifest_catalog_2pts.osc` golden vectors,
 * so it is exported alongside the required functions.
 */
export function encodeManifest({ rendererId, kind, name, lo, hi, points }) {
  const pts = points || [];
  const nPoints = pts.length;

  const begin = {
    address: ADDR_MANIFEST_BEGIN,
    args: [
      { type: "s", value: String(rendererId) },
      { type: "s", value: String(kind) },
      { type: "i", value: nPoints | 0 },
      { type: "s", value: String(name) },
    ],
  };

  const packets = [begin];

  if (kind === "continuous") {
    const loV = lo != null ? lo : new Array(K).fill(0.0);
    const hiV = hi != null ? hi : new Array(K).fill(1.0);
    packets.push({
      address: ADDR_MANIFEST_BOUNDS,
      args: [{ type: "s", value: String(rendererId) }, ...fArgs(loV), ...fArgs(hiV)],
    });
  } else {
    for (const p of pts) {
      packets.push({
        address: ADDR_MANIFEST_POINT,
        args: [
          { type: "s", value: String(rendererId) },
          { type: "i", value: p.pointId | 0 },
          ...fArgs(p.mu),
          ...fArgs(p.sigma),
          { type: "s", value: String(p.instrument) },
          { type: "f", value: Number(p.maxRateHz) },
          { type: "f", value: Number(p.velMin) },
          { type: "f", value: Number(p.velMax) },
        ],
      });
    }
  }

  packets.push({
    address: ADDR_MANIFEST_END,
    args: [{ type: "s", value: String(rendererId) }],
  });

  // IMMEDIATELY timetag: raw bytes 00 00 00 00 00 00 00 01 (matches
  // pythonosc.osc_bundle_builder.IMMEDIATELY), NOT osc.timeTag(0) (which
  // encodes "now" as an NTP timestamp and would break byte equality).
  return oscLib.writePacket({ timeTag: { raw: [0, 1] }, packets });
}

// --- decode -------------------------------------------------------------

function needLen(args, n) {
  if (!args || args.length < n) {
    throw new Error(`short message: need ${n} args, got ${args ? args.length : 0}`);
  }
}

function readVec8(args, i) {
  needLen(args, i + K);
  const v = new Array(K);
  for (let k = 0; k < K; k++) v[k] = clamp01(Number(args[i + k]));
  return [v, i + K];
}

/** Flattens a parsed osc.js packet (bare message or nested bundle) into a
 * flat list of {address, args} messages, in order. */
function flattenMessages(parsed) {
  const out = [];
  const walk = (p) => {
    if (!p) return;
    if (Array.isArray(p.packets)) {
      for (const sub of p.packets) walk(sub);
    } else if (typeof p.address === "string") {
      out.push(p);
    }
  };
  walk(parsed);
  return out;
}

/**
 * Stateful decoder: accumulates manifest begin/point/end across packets,
 * mirroring `septagraph.codec.Decoder` in the Python core. A new `begin`
 * for a `rendererId` discards any in-progress partial manifest for that id.
 */
export class Decoder {
  constructor() {
    this._pending = new Map();
  }

  _decodeMessage(msg) {
    const addr = msg.address;
    const args = msg.args || [];
    const out = [];

    try {
      if (addr === ADDR_HIT) {
        needLen(args, 4 + 2 * K);
        const voiceId = String(args[0]);
        const layer = Number(args[1]) | 0;
        const onset = Boolean(args[2]);
        const velocity = clamp01(Number(args[3]));
        let i = 4;
        let mu, sigma;
        [mu, i] = readVec8(args, i);
        [sigma, i] = readVec8(args, i);
        out.push({ type: "hit", voiceId, layer, onset, velocity, mu, sigma });
      } else if (addr === ADDR_MOVE) {
        needLen(args, 1 + K);
        const voiceId = String(args[0]);
        const [mu] = readVec8(args, 1);
        out.push({ type: "move", voiceId, mu });
      } else if (addr === ADDR_SHAPE) {
        needLen(args, 4);
        const shapeId = String(args[0]);
        const layer = Number(args[1]) | 0;
        const durationS = Number(args[2]);
        const nKnots = Number(args[3]) | 0;
        const knots = [];
        let i = 4;
        for (let k = 0; k < nKnots; k++) {
          needLen(args, i + 1);
          const t = clamp01(Number(args[i]));
          i += 1;
          let mu, sigma;
          [mu, i] = readVec8(args, i);
          [sigma, i] = readVec8(args, i);
          knots.push({ t, mu, sigma });
        }
        out.push({ type: "shape", shapeId, layer, durationS, knots });
      } else if (addr === ADDR_MANIFEST_REQUEST) {
        out.push({ type: "manifestRequest" });
      } else if (addr === ADDR_MANIFEST_BEGIN) {
        needLen(args, 4);
        const rendererId = String(args[0]);
        this._pending.set(rendererId, {
          kind: String(args[1]),
          nPoints: Number(args[2]) | 0,
          name: String(args[3]),
          lo: null,
          hi: null,
          points: [],
        });
      } else if (addr === ADDR_MANIFEST_BOUNDS) {
        needLen(args, 1 + 2 * K);
        const rendererId = String(args[0]);
        const st = this._pending.get(rendererId);
        if (st) {
          let i = 1;
          let lo, hi;
          [lo, i] = readVec8(args, i);
          [hi, i] = readVec8(args, i);
          st.lo = lo;
          st.hi = hi;
        }
      } else if (addr === ADDR_MANIFEST_POINT) {
        needLen(args, 2 + 2 * K + 4);
        const rendererId = String(args[0]);
        const st = this._pending.get(rendererId);
        if (st) {
          const pointId = Number(args[1]) | 0;
          let i = 2;
          let mu, sigma;
          [mu, i] = readVec8(args, i);
          [sigma, i] = readVec8(args, i);
          const instrument = String(args[i]);
          i += 1;
          const maxRateHz = Number(args[i]);
          i += 1;
          const velMin = Number(args[i]);
          i += 1;
          const velMax = Number(args[i]);
          i += 1;
          st.points.push({ pointId, mu, sigma, instrument, maxRateHz, velMin, velMax });
        }
      } else if (addr === ADDR_MANIFEST_END) {
        needLen(args, 1);
        const rendererId = String(args[0]);
        const st = this._pending.get(rendererId);
        this._pending.delete(rendererId);
        if (st) {
          out.push({
            type: "manifest",
            rendererId,
            kind: st.kind,
            name: st.name,
            lo: st.lo,
            hi: st.hi,
            points: st.points,
          });
        }
      }
      // else: unknown /sept/v1/* address -> ignore silently (conformance C1)
    } catch (e) {
      // Malformed message for a known address: ignore rather than throw.
      return [];
    }

    return out;
  }

  /** feed(bytes: Uint8Array) -> Array<decoded message objects> */
  feed(bytes) {
    let parsed;
    try {
      parsed = oscLib.readPacket(bytes, {});
    } catch (e) {
      return [];
    }
    const msgs = flattenMessages(parsed);
    const out = [];
    for (const m of msgs) {
      if (typeof m.address !== "string" || !m.address.startsWith("/sept/v1/")) continue;
      out.push(...this._decodeMessage(m));
    }
    return out;
  }
}

/**
 * Stateless decode of a single packet (bundle or message). A manifest
 * bundle (begin+bounds/points+end all in one packet, as produced by
 * `encodeManifest`) resolves in one call. A manifest split across separate
 * packets requires a persistent `Decoder` (use `new Decoder().feed(...)`
 * across packets).
 */
export function decode(bytes) {
  return new Decoder().feed(bytes);
}

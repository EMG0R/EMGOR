# smartGOR Ring Topology — Canonical Doc

**Created:** 2026-08-09
**Status:** canonical reference for the 3-node LED ring. If firmware/lightserver
behavior ever disagrees with this doc, fix one of them and note it here.

The three ESP32 FCOB nodes now form a **closed physical ring around the room**:
one continuous 1920-pixel loop of light. Effects are authored against the ring,
never against an individual strip.

---

## 1. Physical layout (which end touches which)

Each strip has a *beginning* (pixel 0, where data enters from its ESP32) and an
*end* (its last pixel). The joints, as physically installed:

- **beginning of board1** touches **end of board3**
- **beginning of board3** touches **end of board2**
- **beginning of board2** touches **end of board1**

Equivalently, walking the ring in data/index direction:

```
... → strip1[0 → 799] → strip2[0 → 479] → strip3[0 → 639] → strip1[0] → ...
```

All three strips point the *same way* around the loop — no direction reversal
is needed anywhere. The joints are physical adjacency only (no data or power
crosses a joint; each ESP32 drives and powers its own strip independently).

## 2. Ring index map

One global ring coordinate `ring[0..1919]`, total **1920 px**:

| Ring range        | Node        | Local range   | Pixels |
|-------------------|-------------|---------------|--------|
| ring[0 .. 799]    | fcob-strip  | [0 .. 799]    | 800    |
| ring[800 .. 1279] | fcob-strip2 | [0 .. 479]    | 480    |
| ring[1280 .. 1919]| fcob-strip3 | [0 .. 639]    | 640    |

Conversion: `local = ring - offset`, with offsets 0 / 800 / 1280. Local index
increases in the same direction as ring index on every node.

## 3. Per-board hardware table

| Node        | Board MAC           | IP at time of writing | Pixels | Strip length | Notes |
|-------------|---------------------|-----------------------|--------|--------------|-------|
| fcob-strip  | (not recorded)      | 192.168.8.168         | 800    | 5 m          | original v1 board |
| fcob-strip2 | 20:50:0d:19:e2:88   | 192.168.8.138         | 480    | 3 m          | |
| fcob-strip3 | 68:09:47:4c:f1:b4   | 192.168.8.169         | 640    | 4 m          | |

- **IPs float.** The GL.iNet DHCP leases can and do change; the IPs above are a
  snapshot only. The **mDNS names are canonical**: `fcob-strip.local`,
  `fcob-strip2.local`, `fcob-strip3.local`. All tooling and configs should
  address nodes by mDNS name, with IP only as a manual fallback.
- **Retired board:** an earlier ESP32 with MAC beginning `e4:0c` was retired
  from the fleet. If a board with that MAC shows up on the network, it is not
  part of the ring.
- Flash rule: **disconnect the strip's wires from the board during USB flash**
  (see the master-plan addendum of 2026-08-09).

## 4. The ring abstraction

The lightserver on the Pi (`gorcave.local:8800`, repo copy at
`smartGOR/lightserver/`) treats the fleet as **one 1920-pixel render canvas**:

- Every scene/effect renders once onto the full 1920 px ring buffer.
- The buffer is then **sliced per node** using the index map in §2 and streamed
  to each ESP32.
- **Ring-wide control only:** `/prompt` (natural-language vibes) and `/scene`
  apply to the whole ring. There is deliberately no public per-strip control
  surface — clients (dashboard, the website chat agent, humans) only ever say
  what the *room* should look like.
- **Per-node endpoints are debug-only.** Each ESP32 still exposes its own local
  HTTP API; that exists for bring-up and debugging a single board, and using it
  while the lightserver is streaming will fight the stream. Don't build
  anything on top of per-node endpoints.
- Default scene: **ring-orbit** — dark-purple↔violet breathing background with
  a 10-LED soft-edged orb orbiting the full ring every 15 s.

Config files (lightserver repo root, next to each other):

- **`nodes.json`** — the fleet registry: one entry per node with `name`,
  `host` (mDNS, canonical), `ip` (informational snapshot, may be `null`),
  `pixel_count`, and `enabled`.
- **`ring.json`** — the ring geometry: an ordered `segments` array, one entry
  per node with `node` (must match a `nodes.json` name), `ring_offset`,
  `length`, and `reversed`. Segment order = order around the ring.
  `ring_offset` is optional (derived from the running order) but validated if
  present; `length` must match the node's `pixel_count`; total ring length is
  derived. `reversed: true` means a strip is physically mounted backwards and
  its slice is flipped before send — currently `false` for all three strips.
  The lightserver can be run with ring.json ignored (per-node engines) via a
  CLI flag in `lightserver/__main__.py`; that mode is debug-only.

## 5. Adding a future board to the ring

Two halves: physical splice + registry append.

1. **Physical:** break one joint and insert the new strip so its *beginning*
   touches the *end* of the strip before it and its *end* touches the
   *beginning* of the strip after it (same direction as the rest of the ring —
   never reversed). Give the new ESP32 its own PSU and data line as usual.
2. **Firmware:** flash the standard fcob-stream firmware with a new unique
   hostname (`fcob-stripN`), strip wires disconnected during USB flash.
3. **Registry:** add the node to `lightserver/nodes.json` (its `name`, `host`
   = `fcob-stripN.local`, `pixel_count`, `"ip": null`, `"enabled": true`),
   then append a matching segment to `lightserver/ring.json` at the position
   in the `segments` array where the strip sits in the loop:
   `{"node": "fcob-stripN", "length": <px>, "reversed": false}` — set
   `reversed: true` only if the strip had to be mounted pointing against the
   ring direction. `ring_offset` can be omitted (it is derived); total ring
   length and all offsets recompute from the files — no code change needed.
4. **Docs:** update the tables in §2 and §3 of this file (additive: correct
   the tables, and note the change with a date).
5. Restart/reload the lightserver and confirm the default scene walks the
   whole ring across the splice.

# MD + memory organization (COD aquarium)

How a Demiurge node holds its MD files and loads the *right* context — once, not
re-read every prompt — with a clean public/private wall. Bucket: sync (the
pattern ships); the personal instance (identities, personas, registry, memory
content) stays private on the owner's hub, never in this repo.

## MD layers — inheritance, most-specific wins
1. **Parent / system** — `MD/DEMIURGE.md`. The distro. PUBLIC. Every node reads it.
2. **Node / child** — `demiurge/local/device.md`. Identity + role. LOCAL bucket.
   Overrides the parent for this box. (See `MD/device.template.md`.)
3. **Persona** — the node agent's personality, pointed to by device.md `codling:`.
   Optional. May be PUBLIC (a generic COD persona) or PRIVATE (an owner's own).
4. **Owner identity** — the owner's personal top-level MD (e.g. an `_OWNER.md`).
   PRIVATE. Loaded only on the owner's own nodes. Never ships.

An agent on a node reads 1 → 2 → (3) → (4), in that order; the more specific
layer wins. The three agent backends point at the same files via
`CLAUDE.md` / `GEMINI.md` / `AGENTS.md` so they cannot drift.

## The public / private wall
- **PUBLIC** (ships in this repo, or may): `DEMIURGE.md`, `device.template.md`,
  a generic `COD.md`, a `COD_babysitter.md`, the comms mechanism
  (`setup-script/cod/` hook + the public EMG0R/cod repo). No identity, no keys, no node registry, no memory.
- **PRIVATE** (owner's hub repo + local bucket, NEVER here): the owner identity MD,
  the owner's personal personas, the aquarium registry (`COD_aquarium.md`),
  all memory content, credentials, keys, the bus token.
- Enforcement is by **where the file lives** (this public repo vs the private hub
  repo a node also clones) plus the `codling:` pointer. A node can only load a
  private persona if it has the private repo AND device.md points at it. Nodes do
  not all see every persona.

## Memory — central, read-on-demand, per-system
- **Hosted centrally** in the owner's hub git repo (origin on the hub, mirror on
  a public host); every node clones it. One source of truth.
- **Two-tier index**: only the small `MEMORY.md` index loads at session start. An
  individual memory file is opened **only when its index line is relevant** to the
  task. This is the mechanism for "don't re-read all memory every prompt."
- **Per-system scope**: a node AUTO-loads only the memory slice its role needs; it
  can ACCESS all of it, but reads the rest only when told / when relevant.
  - instrument node → the distro + its instrument's project memory
  - server / hub node → its services + the aquarium registry (can read all, rarely needs to)
  - COD → the aquarium registry + COD persona (rest on demand)
  - laptop / phone → full access, read on demand
- **Logging bar stays high**: one durable fact per file, add one index line, sync.
  Log only what is durable, not-derivable, behaviour-changing, and general.

## Why this shape
Access is universal; auto-loading is scoped. That keeps every node cheap to run
(small startup context) while nothing is walled off from a node that has a real
reason to reach it — except the private persona/identity wall, which is a true
boundary, not just a default.

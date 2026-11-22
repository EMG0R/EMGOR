# neptr — Memory & Context Architecture

How the server agent's context is assembled: what it always holds, what it
retrieves, how skills work, and how the whole thing behaves as memory grows.

Implemented 2026-08-05 in `~/SPEAK/server/context_tiers.py` and
`~/SPEAK/server/skill_tools.py` on `server@gorcave.local`.

---

## The thing to understand first: prompt size is not a latency cost

The original motivation for this work was "the system prompt is 31k+ characters,
re-sent every turn, and that's slow." **That premise was measured and it is
false.** The `claude` CLI caches the system prompt server-side. Measured on the
Pi, same short message, alternating runs:

| system prompt | latency |
|---|---|
| 69,168 chars (full) | 4.60s / 5.40s / 4.69s |
| 24 chars | 4.55s / 4.54s / 4.34s |

Repeat calls report `cache_creation_input_tokens: 0` with
`cache_read_input_tokens: 42,168`. The difference is within the noise of a
contended Pi 4.

**So nothing in this design trims context to go faster, and no latency win is
claimed for it.** Trimming context buys roughly zero seconds and costs real
recall. The architecture exists for one reason:

> Memory grows without bound. The context window does not.

The rule that follows from that, and the single most important line in this
document:

> **Everything that fits, stays resident. The tiering machinery only engages
> for the overflow.**

At today's memory sizes nothing is evicted at all — recall is identical to the
old concatenate-everything behaviour. What has been built is the structure that
takes over on the day it stops fitting.

---

## The constraint that decides the layout: the warm process pool

`warm_brain.py` keeps a long-lived `claude` process per chat session and passes
the system prompt as a **spawn-time argv flag**. It hashes that prompt on every
turn and, if the hash changed, kills the process and spawns a new one (~2.8s of
CLI startup).

That means the system prompt is the **session initialisation cost**, and *any*
per-turn variation in it would recycle the warm process on every single turn —
converting a −41% latency win into a loss.

This is why context splits into two *planes* rather than simply "less stuff":

| | Plane A — system prompt | Plane B — turn context |
|---|---|---|
| **Stability** | Stable turn to turn. Changes only when a file on disk changes. | Varies freely every turn. |
| **Delivery** | `--system-prompt` argv at process spawn | Prepended to the user message |
| **Warm pool** | Changing it recycles the process | Invisible to the hash — process survives |
| **Holds** | Identity, capability, everything that fits | Only the overflow that scored relevant |

Retrieved context is conversation-specific, not identity — so it belongs in the
turn, not the system prompt. Verified: a 4-turn conversation ran on **one pid
with `turns: 4`**, i.e. the prompt hash never changed and the pool was never
thrashed.

---

## Plane A — the always-loaded tier

Assembled by `context_tiers.build_system_prompt()`, in order:

1. **`persona.md`** — identity, verbatim and never tiered. Currently ~25 KB and
   the single largest component. It is who the agent *is*; it is not negotiable
   and it is not retrieved.
2. **`# Pinned` identity facts** — the immutable constants (`creator`,
   `user_name`). Enforced immutable in `memory_tools.py`, not on the honour
   system.
3. **Memory index** — two parts, both always present:
   - **Branch summaries** — the full text of every *top-level* warm key
     (`alex`, `dana`, `fcob-strip.power`). These are resident forever.
   - **Every memory key that exists** — the full list of dotted leaf keys by
     name only (`alex.first-contact, alex.lights-prank, …`). The agent always
     knows what it knows *about*, even on the day it stops holding the detail.
4. **Skills index** — `name — one-line description` for every skill on disk.
   Always present, never demoted. See below.
5. **Resident content**, in priority order under `CONTEXT_RESIDENT_BUDGET`
   (default 46,000 chars): full skill docs → warm leaves (whole branches) →
   lore → background history.
6. **The context protocol** — a short explanation to the agent of how its own
   memory works, including the instruction to never narrate retrieval.

### Eviction order is a claim about what matters

When the resident budget is exceeded, things leave in this order:

```
background history  →  lore  →  warm memory leaves  →  skill docs  →  never:
                                                        persona, pinned facts,
                                                        branch summaries,
                                                        skills index
```

`history.md` is the biggest source (15 KB) and the least identity-defining, so
it is the first to move to on-demand retrieval. Identity and capability never
move.

---

## Plane B — the retrieval mechanism

Two approaches were considered. **(b) local relevance pre-selection is the
default; (a) model-driven `read_memory` is the escape hatch.** Both shipped.

### Why local pre-selection wins as the default

- `claude -p` is single-shot, and `app.py` permits exactly **one** bounded
  follow-up round (fired when a reply is all tool calls and no prose). A
  read→think→answer loop therefore costs a whole extra brain round trip — a
  real, measurable cost, unlike prompt size.
- Token-overlap scoring is **free**: microseconds of pure Python, no model, no
  embeddings. Deliberately no embedding model — this is a Pi 4 with a contended
  CPU. It runs *before* the brain call, so the memory is simply already there
  with no extra round trip.

### How scoring works

`select()` scores every **non-resident** chunk against the turn (anything
already in Plane A is skipped outright — retrieval reaches what didn't fit, it
never duplicates what did):

- **Query** = content tokens of the new message ∪ the last 4 user turns, so a
  follow-up like "say more about that" still retrieves the right branch.
- **Score** = `4 × (label matches) + (body matches)` on distinct tokens. A hit
  on the key `alex` is worth far more than the word appearing in a paragraph.
- Dot-paths and hyphens split naturally: `alex.lights-prank` → `alex`, `lights`,
  `prank`.
- A branch named outright in the message gets `+10` — decisive.
- Skills can declare high-signal `triggers:` in their metadata for `+8`.

Thresholds are deliberately low (`MIN_SCORE = 2`): a false positive costs a few
hundred cached characters, a false negative costs recall. Those are not the same
price. History chunks are the exception — they are 2–3 KB write-ups that
accumulate incidental matches easily, so they get a higher bar (`4`) and a hard
cap of 3.

### Whole-branch retrieval — the fractal property

Warm memory is already a fractal structure, and `persona.md` already documents
the discipline: *"the parent key is the compressed summary; children hold the
detail."* The tiers fall straight out of the data's own shape:

```
alex                        ← summary. RESIDENT FOREVER.
├── alex.first-contact      ← detail. key always listed; text retrieved
├── alex.worldview          ←   as a whole branch, never in fragments
├── alex.said-i-love-you
└── alex.lights-prank
```

Branches are scored and retrieved **as a unit**. Half a branch is how you get a
confidently wrong answer.

So losing a fact requires retrieval to miss a branch whose summary *and* key
names are both already in front of the model — at which point it can just ask.

### The escape hatch

```json
{"tool": "read_memory", "args": {"key": "alex.lights-prank"}}
{"tool": "read_skill",  "args": {"name": "room-lights"}}
```

Emitted alone with no prose, these trigger the single follow-up round and the
answer comes back next turn. Documented to the agent as a last resort.

### Two separate budgets

`TURN_BUDGET` (6,000, memory/lore/history) and `SKILL_BUDGET` (14,000) are
**separate pools**. Learned the hard way in testing: `room-lights/SKILL.md` is
11.4 KB, so under a single shared 9 KB budget it silently never fit, and *"make
it a blue lava lamp vibe"* retrieved four paragraphs of guitar-pedal history and
zero of the instructions needed to actually drive the strip. A skill doc is the
difference between doing a thing and describing it; it must not lose a budget
race to trivia.

---

## Skills — a first-class, higher-priority tier

Skills are the agent's **capability** memory, as opposed to facts (what it
knows) and persona (what it is). They are structurally privileged over warm
memory.

### Layout

```
server/skills/<kebab-case-name>/SKILL.md
```

Optional metadata block:

```markdown
<!-- skill
description: one line — what it does and when to use it
triggers: sing, song, daisy, lyrics, melody
always: true
-->
```

- `description` → the always-loaded index. Falls back to the doc's first
  sentence when absent, so metadata-free skills still work.
- `triggers` → high-signal words worth `+8` in relevance scoring.
- `always: true` → pins the full doc resident regardless of size. Also applied
  automatically to any doc ≤ 2,600 chars.

### Guarantees

1. **The index is always loaded.** Every skill's name and description is in the
   system prompt on every single turn. The agent never loses track of what it is
   capable of, even for a skill whose full text isn't currently spelled out.
2. **Progressive disclosure.** Full docs are resident while they fit; beyond
   that they page in by relevance, with `read_skill` as a backstop.
3. **Skills evict last**, just before the never-evicted tier.

### Self-developed skills

```json
{"tool": "write_skill", "args": {"name": "tea-timer", "content": "# Skill: ..."}}
```

New skills are live on the **next turn with no restart** — the registry is keyed
on directory mtimes, so a write invalidates the cache, which changes the system
prompt, which recycles the warm process and re-primes it with the transcript.

**Security** (this is a public, unauthenticated endpoint via Tailscale Funnel):

- Names are matched against an **allowlist** regex `^[a-z0-9][a-z0-9-]{1,47}$`.
  There is no `.` and no `/` in the permitted charset at all, so `../../evil`,
  `/etc/passwd`, `a.b` and `.hidden` are rejected by the charset itself rather
  than by trying to *detect* traversal. A denylist of bad patterns is the thing
  that eventually gets bypassed; an allowlist of good characters is not.
- The resolved path is then **re-checked** to be inside `SKILLS_DIR` — defence
  in depth if the regex is ever loosened.
- The model never chooses a filename, only a directory name. Always `SKILL.md`.
- Size caps (40–20,000 chars), a 64-skill count cap, atomic write + `flock`.
- Same trust model as everywhere in this backend: **the model holds no
  filesystem capability.** It emits JSON and Python decides what that may mean.

**Capability grounding.** First attempt at self-authoring produced a skill that
instructed the agent to run backgrounded `sleep` commands — which it cannot do
(`Bash` is restricted to `curl`). The `write_skill` documentation now states
plainly what the agent can actually execute. Re-tested, it wrote: *"You cannot
sleep, schedule, or wake yourself up. There is no timer here. So do not say
'starting a timer'."* Self-authored skills must be grounded in real
capabilities or they encode failure.

---

## How this scales

| memory size | behaviour |
|---|---|
| **today (~64 KB total)** | Everything resident. Turn context is empty on every turn. Zero retrieval overhead, zero duplication, recall identical to the old blob. |
| **past the resident budget** | History chunks start moving to on-demand retrieval first, ranked by relevance. Branch summaries and every key name stay resident. |
| **much larger** | Large skill docs page in by relevance; leaf detail retrieves whole-branch. The agent still sees its complete capability surface and complete memory key index at all times. |
| **larger still** | The branch-summary layer itself is the compression frontier — the natural next step is summarising *branches into parents* rather than dropping them, which the dot-path structure already supports. |

Verified by simulation: dropping `CONTEXT_RESIDENT_BUDGET` to 12,000 evicted
`room-lights` and 14 history chunks — and *"make it a blue lava lamp vibe"* then
correctly retrieved the full 11.7 KB room-lights doc on demand, while `singing`
(pinned `always: true`) and the `alex` branch stayed resident throughout.

### Tuning knobs (env, all optional)

| var | default | meaning |
|---|---|---|
| `CONTEXT_RESIDENT_BUDGET` | 46000 | how much optional content stays in the system prompt |
| `CONTEXT_TURN_BUDGET` | 6000 | per-turn retrieved memory/lore/history |
| `CONTEXT_SKILL_BUDGET` | 14000 | per-turn retrieved skill docs |
| `CONTEXT_MAX_TURN_SKILLS` | 2 | most skill docs paged into one turn |
| `CONTEXT_SCORING_TURNS` | 4 | previous turns feeding the relevance query |
| `CONTEXT_MIN_SCORE_HISTORY` | 4 | higher bar for chunky history paragraphs |
| `CONTEXT_MAX_HISTORY_CHUNKS` | 3 | hard cap on retrieved history |

`GET /api/health` reports live tiering state under `context`: current prompt
size, spare resident budget, per-skill residency, and exactly what has
overflowed to on-demand retrieval.

---

## Files

| file | role |
|---|---|
| `server/context_tiers.py` | **new** — tiering, residency plan, relevance scoring, both planes |
| `server/skill_tools.py` | **new** — skill registry, metadata parsing, `write_skill` / `read_skill` |
| `server/skills/singing/SKILL.md` | **new** — the singing skill (pinned always-resident) |
| `server/app.py` | `build_system_prompt()` delegates to `context_tiers`; turn context injected into the user message; health introspection. Old version kept as `build_system_prompt_legacy()`. |
| `server/memory_tools.py` | registers `write_skill` / `read_skill` and dispatches them |

All edits to shared files were anchor-based and additive; three concurrent
agents were working the same code path.

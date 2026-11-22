# Local LLM brain on the Pi — research, benchmarks, integration, and where it stands

**Status: integration COMPLETE and installed; verification PARTIAL. Paused mid-Phase-4.**
**The service is running normally on the CLOUD brain. Nothing routes to local by default.**

Date: 2026-08-05. Author: local-brain agent.
Target: `server@gorcave.local` — **Raspberry Pi 4 Model B Rev 1.4, 8GB**, Debian 13 (trixie),
`6.18.34+rpt-rpi-v8` aarch64, 4× Cortex-A72, no GPU/NPU.

---

## TL;DR — the honest verdict

**The local brain works, and it is a novelty fallback, not a second-string brain.**

It genuinely runs, genuinely answers, genuinely falls back to cloud when it fails, and would
genuinely keep working with the internet unplugged. But on this hardware it is roughly
**50-100× slower than the cloud path** (2-4 minutes vs 2-6 seconds) and noticeably dumber —
it got `12 × 8` wrong (said 60), and it does not hold the neptr persona. Sustained use pins
the SoC at **83-85.7 °C with the soft thermal limit actively engaged** and the ARM clock
capped at ~1531 MHz against 1800 MHz nominal.

The single most important number in this document:

> **Prefill on this board runs at ~7 tokens/sec.**

Everything else follows from that. It is why the local system prompt had to be rewritten from
scratch, why KV-cache persistence is mandatory rather than an optimization, and why "just point
it at the existing 50k-char prompt" was never going to work.

**Do not melt the Pi** was the right instinct. It is already running hot.

---

## PHASE 1 — Research

### The finding that reshapes everything: this CPU has no dot-product instructions

```
$ lscpu | grep Flags
Flags:  fp asimd evtstrm crc32 cpuid
```

That is **ARMv8.0-A**. There is no `asimddp` (SDOT/UDOT, an optional ARMv8.2 extension) and no
`i8mm` (ARMv8.6). The Pi 5's Cortex-A76 *does* have dotprod; the Pi 4's A72 does not.

This matters because **every ARM repacking fast-path in llama.cpp is gated behind those
extensions**:

| Format | Requires | Available on A72? |
|---|---|---|
| `Q4_0_4_4` | dotprod | No |
| `Q4_0_4_8` | i8mm | No |
| `Q4_0_8_8` | SVE | No |
| Q4_K online repack (PR #17494, Nov 2025) | dotprod | No |

**Consequence: the entire "Q4_0 with ARM repacking beats Q4_K_M on ARM" discourse is a Pi 5 /
Snapdragon / Graviton story and does not apply to this board.** On a Pi 4 you get plain NEON
kernels regardless of which quant you pick.

**Therefore: Q4_K_M.** It has the best quality-per-byte, and on a non-dotprod core it costs
nothing in speed relative to Q4_0. I-quants (IQ4_XS etc.) are consistently *slower* than
K-quants on CPU and were not worth testing.

### Model selection

Researched against 2026-current releases, prioritising recent small models and specifically
instruction-following + structured JSON output at 1-4B scale.

**Chosen: `LiquidAI/LFM2.5-1.2B-Instruct-GGUF` @ Q4_K_M** (released 2026-01-05).

Reasoning:
- IFEval 86.23 — exceptional for 1.2B (per Liquid AI's release post).
- Tied #1 of 21 models on an independent CPU-only tool-calling benchmark, with far lower
  latency than models 3× its size.
- Hybrid state-space architecture → materially faster decode than a dense transformer of
  similar size, which matters enormously on a bandwidth-starved board.
- 731 MB at Q4_K_M. Small active weight footprint is worth more than parameter count here.
- **Caveat: LFM2.5 natively emits bracket-notation tool calls** (`[get_weather(city="X")]`),
  not JSON. This is a known and documented behaviour, and it is a direct risk to the
  `{"tool": ...}` contract. Untested in practice — see open questions.
- **License is LFM1.0, not Apache.** Check it if this ever ships commercially.

**Rejected after research:**
- **Qwen3-1.7B** — benchmarked it anyway (see below) and it lost on both axes. Research also
  flagged a non-monotonic capability valley where Qwen3-0.6B outperforms 1.7B on tool calling.
- **Llama 3.2 1B / 3B** — dead last on tool-calling benchmarks. 3B is also too slow here.
- **Gemma 4 E2B / any ≥3B model** — 2.9 GB, measured at ~2.5 tok/s decode on a Pi 4 elsewhere.
- **Gemma 3 1B** — poor tool-calling restraint.

**Not tried, worth trying next:** `unsloth/Qwen3.5-2B-MTP-GGUF` (Qwen3.5 small series, shipped
2026-03-02). Sparse MoE means fewer weights read per token, which is exactly the right shape for
a memory-bandwidth-bound board, and it emits *native JSON* tool calls — which would directly
address the biggest open risk. Also `Granite-4.0-1B` (Apache 2.0, strong BFCL v3).

### Runtime: `llama-server`, not ollama, not llama-cpp-python

- **llama-cpp-python** would put ~1 GB of model memory inside the FastAPI process and block the
  asyncio event loop during inference. It also adds measurable overhead.
- **ollama** wraps the same engine with less control, and this integration specifically needs
  `--slot-save-path` (KV persistence) and precise thread pinning, both first-class in llama-server.
- **llama-server** runs as a separate process whose memory is fully reclaimed on unload, exposes
  an OpenAI-compatible endpoint, and can be killed/restarted independently of the web service.

**No compilation was needed** — this saved ~25 minutes. The official prebuilt
`llama-b10276-bin-ubuntu-arm64.tar.gz` ships **runtime-dispatched CPU backends** including
`libggml-cpu-armv8.0_1.so`, so it runs correctly on a non-dotprod A72. It works on Debian 13
despite being built on Ubuntu.

---

## PHASE 2 — Measured benchmarks (all real, all on this Pi)

### Thread scaling — `llama-bench`, LFM2.5-1.2B-Instruct Q4_K_M

| Threads | Prefill `pp512` | Decode `tg64` |
|--------:|----------------:|--------------:|
| 2 | 5.00 tok/s | 3.47 tok/s |
| **3** | **7.04 tok/s** | **4.44 tok/s** |
| 4 | 6.79 tok/s | 3.29 tok/s |

**The 4th thread makes BOTH phases slower.** This is not a "leave a core for the OS" judgement
call — it is measured. `-t 3 -tb 3` is the correct setting and needs no further justification.
(The board is thermally saturated, so the 4th core adds heat and contention without adding
throughput.)

### Model comparison

| Model | Quant | Size | pp512 @ t=2 | tg64 @ t=2 |
|---|---|---:|---:|---:|
| **LFM2.5-1.2B-Instruct** | Q4_K_M | 695 MiB | **5.00 tok/s** | **3.47 tok/s** |
| Qwen3-1.7B | Q4_K_M | 1.03 GiB | 3.75 tok/s | 2.40 tok/s |

LFM2.5 wins decisively — ~33% faster prefill and ~45% faster decode at 60% the size. The
hybrid-SSM architecture claim holds up. (Both t=2 runs were under some background load from
concurrent work on the box, so treat these as slightly pessimistic *but fairly compared*.)

### Thermals — this is the part to take seriously

Sampled every 10s during sustained inference:

```
03:20:13 temp=83.2'C throttled=0xe0006 clk=600MHz  load=4.38
03:20:23 temp=85.2'C throttled=0xe0008 clk=1531MHz load=4.61
03:20:33 temp=84.7'C throttled=0xe0008 clk=1531MHz load=4.97
03:20:43 temp=83.7'C throttled=0xe0008 clk=1580MHz load=5.61
03:20:53 temp=84.7'C throttled=0xe0008 clk=1531MHz load=6.62
03:21:03 temp=84.2'C throttled=0xe0006 clk=600MHz  load=6.58
03:21:13 temp=84.7'C throttled=0xe0008 clk=1629MHz load=6.11
```

- **Peak observed: 85.7 °C.** Idle: ~57 °C. Post-run cooldown: ~65-74 °C.
- `throttled=0xe0008` → bit 3 = **soft temperature limit active RIGHT NOW**; bits 17/18/19 =
  capped/throttled/soft-limit have all occurred.
- ARM clock held at **1531-1629 MHz against 1800 MHz nominal** — a sustained ~15% clock loss.
- `0x...6` samples show bit 1 (freq capped now) + bit 2 (throttled now) with the clock dropped
  all the way to **600 MHz**.
- No under-voltage bit was ever set, so this is genuinely heat, not a bad PSU.

**Conclusion: this Pi has inadequate cooling for sustained inference.** It does not crash and it
does not damage itself (the throttle is doing its job), but it never runs at full clock during
inference, and every benchmark above is a *throttled* number. **A heatsink and a fan would
plausibly buy 15-20% throughput for a few dollars, and would be the highest-value hardware change
available.**

### Real end-to-end latency through `/api/chat`

| Scenario | Time | Notes |
|---|---:|---|
| Cold (model load + full 1597-token prefill) | **4m 10s** | first ever local turn |
| Cold with KV slot restored from disk | **~4m 38s → then ~3-4 min** | see caveat below |
| Warm (KV prefix cache hot, same session) | **1m 10s** | |
| Cloud brain, warm pool, for comparison | **2.6 - 6.4s** | |

Model *weights* load in **1.67 s** — that was never the bottleneck. Prefill is.

The local system prompt tokenizes to **1597 tokens**. At 7.04 tok/s that is ~227 s of pure
prefill, which matches the observed cold latency almost exactly.

**KV cache persistence works.** After a restart, llama-server reported:

```
slot get_availabl: selected slot by LCP similarity, f_sim_best = 0.994, f_keep = 0.987
```

98.7% of the prefilled cache was reused from the on-disk slot file. The saved slot is 26 MB.

---

## PHASE 2 — Honest quality assessment

Real replies actually produced by the local model:

| Prompt | Local reply | Verdict |
|---|---|---|
| "hey neptr, you awake?" | "yes, I'm here. What would you like to talk about?" | Coherent English. **Completely off-persona** — generic assistant voice, capitalised, offers help. neptr would say something like "Yeah." |
| "what is 12 times 8?" (warm, 2nd turn) | "yes, i'm here. what would you like to talk about?" | **Echoed its own previous reply.** Fixed — see below. |
| "what is 12 times 8?" | "60" | **Wrong.** (96) |

For contrast, the cloud brain answered the same prompts with "Yeah." and "96." — in persona,
correct, in ~3 seconds.

**Assessment: coherent, but not smart, and not neptr.** It forms grammatical, on-topic English
sentences. It does not follow the persona's voice rules (lowercase name, 3-15 words, no offers of
help, no "what would you like to talk about"), and it fails basic arithmetic.

### The echo bug and its fix

The cloud path flattens conversation into one text blob (`User: …\nneptr: …`) because
`claude -p` is single-shot. Handed the same blob, a 1.2B model does not reliably grasp that the
last line is the turn to answer — it just reproduced its previous reply verbatim.

Fixed by `_transcript_to_messages()` in `local_brain.py`, which parses that blob back into
proper `user`/`assistant` roles so llama-server can apply the model's own chat template, plus
`repeat_penalty: 1.1` over a 256-token window. This is also *better* for the KV cache, since the
rendered conversation then grows append-only. **The parser is unit-tested and correct, but the
end-to-end quality improvement from it was never measured** — see open questions.

### Tool JSON — **UNVERIFIED, and this is the biggest open risk**

**I never got a confirmed `{"tool": ...}` emission out of the local model.** I ran out of time
before the memory/lights/switching probes.

The concern is concrete and specific, not vague: **LFM2.5 natively emits bracket notation**
(`[write_memory(key="alex", value="drummer")]`) **rather than JSON**. Published benchmarks show
it ranking mid-pack on tool calling until a custom bracket parser is added, at which point it
jumps to joint-first. `memory_tools._parse_tool_line()` only accepts a line that parses as a JSON
object, so bracket-notation calls would be **silently passed through into the user-visible reply**
— visible garbage, and no memory write.

**Assume memory writes do NOT work on the local path until proven otherwise.** Mitigations, in
order of preference, are in "Next steps" below.

---

## PHASE 3 — What was built

### Files ADDED (all new, nothing overwritten)

| Path | Purpose |
|---|---|
| `~/SPEAK/server/local_brain.py` | llama-server lifecycle, brain state, inference, thermal guard, KV slot persistence, transcript→messages parser |
| `~/SPEAK/server/device_tools.py` | Backend-executed `set_lights` / `get_lights` |
| `~/SPEAK/server/skills/brain-switching/SKILL.md` | The switching skill doc |
| `~/SPEAK/persona_local.md` | Compact persona for the local brain (3,119 chars) |
| `~/SPEAK/memory/brain.json` | Persisted brain choice. **Currently `{"brain": "cloud"}`** |

### Files MODIFIED (surgically, additively)

**`~/SPEAK/server/memory_tools.py`**
- Added `BRAIN_TOOLS = {"set_brain"}` and `DEVICE_TOOLS = {"set_lights", "get_lights"}` to
  `KNOWN_TOOLS`.
- Added `get_lights` to `READ_TOOLS`.
- Added three dispatch branches in `_execute()`, using lazy imports to avoid circularity.
- Backup: `memory_tools.py.bak-localbrain-*`

**`~/SPEAK/server/app.py`**
- `from . import local_brain, memory_tools`
- `PERSONA_LOCAL_PATH`, `BRAIN_SKILL_PATH` constants.
- `build_local_system_prompt()` — the reduced local prompt.
- Local routing **inside `brain_call()`**, before the warm pool is consulted.
- `meta["brain"]` + `ChatResponse.brain` so every reply says which brain answered.
- `/api/health` gains `brain` and `local_brain`; new `GET`/`POST /api/brain`.
- Backups: `app.py.bak-localbrain2-*`

**`~/SPEAK/server/.env`** — appended `LOCAL_LLM_TIMEOUT=900`.

The patch script is idempotent and assert-guarded, saved at
`/tmp/patch_app.py` on the Pi (and in the agent scratchpad). It verifies every anchor and
syntax-checks the result *before* writing, so a concurrent edit causes a clean failure rather
than a clobber. **It failed safely twice** when other agents moved anchors, which is exactly
what it was designed to do.

### Design decisions worth knowing

**Integration point.** Other agents shipped `warm_brain.py` (a warm `claude` process pool) and a
`brain_call()` wrapper while this work was in flight. I hooked **inside `brain_call()`** rather
than at its call sites. This means (a) the warm pool is never touched or hash-thrashed on the
local path, because local returns before `_warm()` is consulted; (b) any *future* call site picks
up local routing for free. This survived two rounds of concurrent edits by other agents.

**The reduced local prompt.** Cloud sees ~67,874 chars. Local sees **7,665 chars / 1,597 tokens**.
What local does NOT see:
- `persona.md` (21.7k) → replaced by hand-condensed `persona_local.md` (3.1k)
- `memory/history.md` (15.4k) → **dropped entirely**
- `skills/room-lights/SKILL.md` (11.4k) → **dropped**; replaced by ~10 lines driving
  `set_lights`, because the local model has no shell and cannot run curl at all
- the per-turn retrieved context block

It DOES see `facts.md` and `lore.md` in full, plus the brain-switching skill, and it emits into
the same `memory_tools.process_reply()`. **The memory system is not forked.**

**Prompt ordering is deliberate:** static content (persona, skill) first, volatile content
(facts, lore) last — because llama-server's prefix cache only reuses an *exact leading prefix*, so
a memory write must invalidate only the tail.

**Lights on the local path.** The cloud brain drives the strip with its own scoped `curl` Bash
tool. The local model has no shell whatsoever, so `device_tools.py` lets it emit
`{"tool": "set_lights", ...}` for the *backend* to execute — same trust model as the memory tools
(the model never holds the capability; Python validates and performs against a hard-coded host).
The cloud path is completely untouched and still uses curl.

**Lazy load / idle unload.** llama-server is not started at import. It spawns on first local use,
is killed after 900 s idle, and is killed immediately when the brain switches to cloud. With
`brain=cloud` the model is never resident. Verified: `pgrep llama-server` returns 0 right now.

---

## PHASE 4 — Verification status

| Check | Status |
|---|---|
| Cloud brain answers `/api/chat` end to end | **PASS** — `{"reply":"Here.","brain":"cloud"}` |
| Local brain answers `/api/chat` end to end | **PASS** — `{"reply":"yes, I'm here...","brain":"local"}` |
| `brain` field reports which brain answered | **PASS** |
| `/api/health` reflects current brain + load state | **PASS** |
| `POST /api/brain` switches out of band | **PASS** — `{"brain":"local","result":"ok: brain set to local"}` |
| Graceful fallback local→cloud on failure | **PASS** (accidentally but convincingly — see below) |
| KV slot save + restore across restarts | **PASS** — `f_keep = 0.987` |
| `/api/voice/speak` | **PASS** — 200, 22,506-byte WAV |
| Room-lights skill causes real hardware change | **PASS** — strip went to `mode:solid, color:#ff0000`, then restored |
| **Conversational switching** ("switch to the local model") | **NOT TESTED** |
| **Memory tool JSON on the local path** | **NOT TESTED** — and at risk, see above |
| **`set_lights` on the local path** | **NOT TESTED** |
| **`/api/voice/chat`** | **NOT RE-TESTED** this session |

### The fallback proof (an accident that turned into good evidence)

The final local test returned `brain: "cloud"` for both turns despite `brain.json` saying
`local`. The log explains it:

```
[local-brain] stopping llama-server (switched to cloud)
[local-brain] request failed after 222.9s: Remote end closed connection without response
[brain] local brain unavailable this turn — falling back to cloud
[latency] brain path=warm took=6.37s
```

Something called `set_brain("cloud")` **while a local inference was in flight**, and `stop()`
killed llama-server underneath it. The user still got real, correct answers ("Yeah." and "96.")
from the cloud brain. **Graceful degradation works exactly as designed.**

But it also exposes a **real bug: `stop()` must not tear down a server that has an in-flight
request.** See next steps.

---

## Current state of the system — READ THIS FIRST ON RESUME

- `server-agent.service` is **active and healthy**.
- `/api/health` → `status ok | brain cloud | local loaded False | claude_authenticated True | stt local`
- `/api/chat` on cloud → verified working.
- `memory/brain.json` = `{"brain": "cloud"}`. **Cloud is the active brain.**
- **No llama-server process is running** and nothing starts one at boot. The model is loaded
  strictly lazily, only when the brain is set to local.
- FCOB strip restored to its original `generative / nebula / #0080ff / brightness 40`.

### Disk

```
/dev/mmcblk0p2   15G   11G  3.3G  76% /
```

**3.3 GB free — this got tight.** Installed:

| Path | Size |
|---|---:|
| `~/llama/llama-b10276/` | ~13 MB |
| `~/llama/models/LFM2.5-1.2B-Instruct-Q4_K_M.gguf` | 731 MB |
| `~/llama/models/LFM2.5-1.2B-Instruct-Q5_K_M.gguf` | 328 MB (downloaded, **never tested**) |
| `~/llama/models/Qwen3-1.7B-Q4_K_M.gguf` | 1.1 GB (benchmarked, **rejected — safe to delete**) |
| `~/llama/slots/neptr-prefix.bin` | 26 MB |

**Deleting `Qwen3-1.7B-Q4_K_M.gguf` reclaims 1.1 GB and loses nothing.** The external drive was
never mounted; everything is on the SD card.

---

## Next steps, in order

1. **Fix `stop()` racing an in-flight request.** `set_brain("cloud")` currently kills
   llama-server mid-inference. It should refuse to stop while a request is active (or defer to
   the reaper). This caused the only local failure observed. Also worth finding *who* called it —
   possibly the cloud brain emitting `set_brain` unprompted now that the skill is in its prompt,
   which would be its own problem.

2. **Settle the tool-JSON question — the highest-value unknown.** Send the local brain
   "remember that alex is a drummer" and inspect the raw reply. If it emits bracket notation, pick
   one of:
   - **(best) GBNF grammar / `json_schema` response format in llama-server**, which makes
     malformed tool calls structurally impossible at the sampler level. This is the right answer
     for any model at this scale and is worth doing regardless.
   - a bracket-notation parser alongside the JSON one in `memory_tools`.
   - switch to a model with native JSON tool calling (Qwen3.5-2B).

3. **Measure whether `_transcript_to_messages()` actually fixed the echo bug.** It is deployed
   and unit-tested but the end-to-end improvement was never observed.

4. **Test conversational switching** — "switch to the local model" → check `/api/health` →
   send a message → confirm `brain: "local"` → "use fable" → confirm back.

5. **Consider trimming the local prompt further.** 1597 tokens ≈ 227 s of cold prefill. Dropping
   the brain-switching skill from the *local* prompt (the tool is already listed in
   `persona_local.md`) would save ~600 tokens ≈ 85 s.

6. **Try `Qwen3.5-2B` (sparse MoE, native JSON tool calls).** Would address both the quality gap
   and the tool-JSON risk. Needs ~1.5 GB — delete the Qwen3-1.7B file first.

7. **Buy a heatsink and fan.** Cheapest available throughput win, and it directly addresses the
   "don't melt the Pi" concern.

---

## Things that cost me time — read these to avoid repeating them

- **`pkill -f 'llama-server'` over SSH kills its own shell**, because the remote `bash -c`
  command line contains the pattern. Exit 255, and — worse — **anything chained after it never
  runs**. This silently invalidated a whole test cycle: I thought I had restarted the service with
  new code and had not. Use `pkill -f 'llama[-]serv'` and *never* chain a restart after it.
- **`local_brain` is imported by `app.py` before `load_dotenv()` runs**, so every
  `os.environ.get()` at module level silently got its default. `LOCAL_LLM_TIMEOUT=900` in `.env`
  was being read as 300 and cold turns were timing out at exactly 300 s. Fixed by calling
  `load_dotenv()` in `local_brain.py` too. **Any new module doing module-level env reads has this
  same bug.**
- **The service does not log to journald.** `StandardOutput=append:/home/server/SPEAK/server/service.log`.
  `journalctl -u server-agent` shows only systemd lines. Read `~/SPEAK/server/service.log`.
  llama-server's own log is at `~/llama/llama-server.log` (I route it there deliberately — its
  per-request lines are the only way to see KV cache hit rates).
- **Do not send `llama-bench` and a model download at the same time.** My first Qwen3 numbers were
  measured under a concurrent 1.1 GB download and were meaningfully pessimistic.
- **The prompt grew under me.** The brief said ~31k chars; it was 50,533 when I measured it and
  67,874 after other agents' changes. Always re-measure `len(build_system_prompt())`.
- **`llama-server /slots` blocks while the single slot is busy**, so it is useless for polling
  progress mid-request. Use `ps -o etimes,pcpu` instead.
- **No compilation needed** — I nearly started a 25-minute build before checking that the
  prebuilt arm64 release ships runtime-dispatched `armv8.0` kernels. Check first.

---

## Configuration reference

All overridable in `~/SPEAK/server/.env`:

| Variable | Default | Notes |
|---|---|---|
| `BRAIN` | `cloud` | fallback default when `brain.json` is absent |
| `LOCAL_MODEL_PATH` | `~/llama/models/LFM2.5-1.2B-Instruct-Q4_K_M.gguf` | |
| `LLAMA_SERVER_BIN` | `~/llama/llama-b10276/llama-server` | |
| `LOCAL_LLM_PORT` | `8788` | 127.0.0.1 only |
| `LOCAL_LLM_THREADS` | `3` | measured optimum |
| `LOCAL_LLM_THREADS_BATCH` | `3` | measured optimum |
| `LOCAL_LLM_CTX` | `4096` | |
| `LOCAL_LLM_MAX_TOKENS` | `200` | |
| `LOCAL_LLM_TIMEOUT` | `900` (set in .env) | module default is 300 |
| `LOCAL_LLM_IDLE_UNLOAD` | `900` | seconds; 0 disables |
| `LOCAL_LLM_TEMP_ABORT` | `85.0` | °C; above this, refuse local and fall back to cloud |
| `LOCAL_LLM_SLOT_SAVE` | `true` | KV persistence |

Switching, three ways:
```bash
# API
curl -X POST localhost:8787/api/brain -H 'Content-Type: application/json' -d '{"brain":"local"}'
curl -s localhost:8787/api/brain

# Conversationally (UNTESTED)
"switch to the local model" / "go local" / "use fable" / "back to claude"

# State file
cat ~/SPEAK/memory/brain.json
```
Accepted synonyms — cloud: `cloud`, `claude`, `fable`, `online`, `remote`, `big`, `api`.
Local: `local`, `offline`, `pi`, `onboard`, `on-device`, `small`, `llama`, `lfm`.

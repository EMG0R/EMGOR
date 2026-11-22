/* demiurge-web — VIDEO. The fractal engine, under your thumb.
 *
 * This file is an EXTENSION, not a fork. It loads after app.js, registers its
 * own `/video/*` routes into the same router, and draws with the same
 * helpers, the same `.row/.fader/.seg/.lbl` shapes and the same CSS custom
 * properties as every other screen. Nothing here restyles anything: the
 * instrument has one look, and a second palette invented for the video mode
 * would read as a second program sharing a phone.
 *
 * On the colour, since it is the one thing easy to get wrong: the engine's
 * own theme (`NEPTR_VIDEO/src/ui/theme.h`) is a SYSTEM, not a set — every
 * shade is hue 296° at some saturation and value, with ALERT the single
 * deliberate escape from the purple, which is exactly what makes it read as
 * an alarm. style.css is the same system expressed as tokens (--accent,
 * --f0..--f3, --rim, --muted, --alert). So this file names tokens and never
 * literals: that is what keeps the phone, the panel and the projector
 * agreeing, and what keeps a token edit from having to be chased through
 * three files.
 *
 * Separate section, same instrument: /video/* is its own URL prefix with its
 * own screens, exactly as /neptr/* is, and there is no second nav bar, no tab
 * strip and no mode switch anywhere. You get to it from one row on HOME.
 *
 * ── how a control gets from here to the picture ─────────────────────────
 *
 *   fader  ─POST /api/video/param {id,value}─→  demiurge_web  ─UDP OSC─→
 *   /nv/param <id:s> <norm:f>  → neptr-video:8004 → ParamBank.setNorm(id)
 *
 * A browser cannot send UDP, so the server is the transport and nothing
 * more. Three things about that path are deliberate:
 *
 * THE ID IS A STRING, NEVER AN INDEX. The bank's tail is rebuilt whenever the
 * generator or the effect chain changes, so an index addresses a different
 * knob after any structural edit. An id addresses the same knob forever, and
 * an id the engine does not have is ignored rather than fatal — which is what
 * lets this page name knobs that only exist in some patches.
 *
 * VALUES ARE NORMALIZED POSITIONS, 0..1 — never real units. The engine owns
 * the curve: ZOOM is exponential across four decades, GRAVITY is bipolar
 * around a detent. A phone has no business knowing that, and a value in real
 * units would silently change meaning the day a range is widened. So a fader
 * here is a POSITION, and it is honestly labelled as a percentage of travel
 * rather than dressed up as a number with units it cannot know.
 *
 * IT IS NOT A `/cc`. The 7-bit CC vocabulary is csound's, and 128 steps is
 * not the resolution this layer works at. Nothing on this page is routed
 * through it.
 *
 * ── what this page can see ──────────────────────────────────────────────
 *
 * The engine broadcasts its whole parameter bank back at ~10 Hz to whichever
 * endpoint last sent it traffic — `/nv/sel <mode> <kernel>` then one
 * `/nv/state <id:s> <norm:f>` per parameter. That lands on the SERVER's UDP
 * socket, which is why `GET /api/video/state` exists: the server keeps the
 * last snapshot and reading it is also what keeps the server subscribed.
 * So the faders below are lit by the engine, not by memory of what this page
 * sent — a preset load, a hand on the device's own panel or the governor
 * thinning the swarm all show up here.
 *
 * The engine sends its `target()`, deliberately, not the live modulated
 * value: a fader has to sit where it was put, and streaming the modulated
 * value back at 10 Hz would make every fader on the phone twitch in time with
 * the LFOs. So a knob with an LFO on it reads STILL here while the picture
 * moves. That is correct, and it is the reason the MOD page shows depth
 * rather than the excursion.
 */
"use strict";

(function () {

/* ── the state feed ───────────────────────────────────────────────────────
 *
 * One poller for the whole section, not one per screen: the payload is the
 * entire bank and two screens asking for it separately would double the
 * traffic to say the same thing twice. It starts when a video screen is
 * built and stops on its own when none is mounted — checked by asking the
 * DOM whether the screen it was started for is still connected, because
 * app.js has no teardown hook and inventing one would mean editing app.js.
 */

const RATE = 250;          // ms between state reads — the engine sends at 10 Hz

let vstate = { live: false, params: {}, mode: null, kernel: null };
let watchers = [];         // { root, sync(state) }
let timer = null;
let inflight = false;

/* ── the engine's self-description ────────────────────────────────────────
 *
 * `CAT.param` is id -> { group, label, min, max, def }, straight from the
 * engine's own ParamBank, and `CAT.lists` holds the selectable banks
 * (kernel, palette, colour, trap, mode, op).
 *
 * Everything below that used to GUESS reads this first. The label tables, the
 * group regexes and the bipolar heuristics further down all exist because
 * `/nv/state` carried only `<id> <value>` and the rest stayed on the device.
 * It does not any more: `/nv/list` answers a `param` bank, the server caches
 * it at /api/video/catalog, and a knob the engine gains tomorrow arrives here
 * already named, already grouped and already the right polarity.
 *
 * The guesses are KEPT, as a fallback for a page talking to an engine too old
 * to answer, and because a catalogue can be a moment late — the state feed
 * comes up first. They are no longer the source of truth, which is what
 * stopped JRNY from reaching this page at all.
 */
let CAT = { param: {}, lists: {}, complete: false };
let catAt = 0;

function pollCatalog() {
  // Slow and self-cancelling: nearly constant data, asked for until it is
  // complete and then only rarely, in case the engine restarted with a
  // different bank.
  const now = Date.now();
  const gap = CAT.complete ? 15000 : 1200;
  if (now - catAt < gap) return;
  catAt = now;
  fetch("/api/video/catalog", { cache: "no-store" })
    .then(r => r.ok ? r.json() : null)
    .then(c => {
      if (!c || !c.lists) return;
      const byId = {};
      for (const p of (c.lists.param || [])) {
        // name packs group|label|min|max|def -- see sendLists() in main.cpp.
        const bits = String(p.name).split("|");
        if (bits.length < 5) continue;
        byId[p.id] = {
          group: bits[0], label: bits[1],
          min: parseFloat(bits[2]), max: parseFloat(bits[3]),
          def: parseFloat(bits[4]),
        };
      }
      CAT = { param: byId, lists: c.lists, complete: !!c.complete };
    })
    .catch(() => { });
}

function poll() {
  watchers = watchers.filter(w => w.root && w.root.isConnected);
  if (!watchers.length) { clearInterval(timer); timer = null; return; }

  /* NEVER MORE THAN ONE READ IN FLIGHT. A browser allows six connections per
     origin and this app already holds three of them open forever as
     EventSources. A poll that fires on a timer regardless of whether the last
     one came back will, the first time the Pi hesitates, queue requests until
     the budget is gone — and then EVERY request on the page blocks, including
     the ones that send parameters. Measured: with the cap reached, `fetch`
     from the console never returned at all and the whole surface went to
     dashes while the server was answering curl in 8 ms.

     Skipping a beat is free here: the next snapshot is the whole bank, not a
     delta, so a dropped read costs nothing but 250 ms of freshness. */
  if (inflight) return;
  inflight = true;
  // Rides the same timer but on its own much slower clock, so the catalogue
  // costs a request a quarter of an hour rather than four a second.
  pollCatalog();

  // no-store, because this is a live reading and a conditional GET or a
  // heuristically cached copy would freeze the surface on whatever the engine
  // happened to be doing when the page opened.
  fetch("/api/video/state", { cache: "no-store" })
    .then(r => r.ok ? r.json() : null)
    .then(s => {
      if (!s) return;
      vstate = s;
      for (const w of watchers) {
        // One watcher throwing must not stop the others, and must not stop
        // the poll: a dead feed is far worse than a wrong fader.
        try { w.sync(s); } catch (e) { /* keep the feed alive */ }
      }
    })
    .catch(() => { })
    .then(() => { inflight = false; });
}

function watch(root, sync) {
  watchers.push({ root, sync });
  if (timer === null) { timer = setInterval(poll, RATE); poll(); }
}

/* ── the controls, in the shape the instrument already has ────────────────
 *
 * Not a wall of faders. The engine's own panel is PAGED — MAIN / COLOUR /
 * SHAPE / SWARM / DRAW / MOD / FX — because forty-odd sliders in one list is
 * a wall, not an instrument. This mirrors that, and inside each page the
 * order is what a hand actually reaches for first rather than the order the
 * parameters happen to be registered in.
 *
 * `bip` marks a bipolar knob: its travel is symmetric about a centre detent,
 * so it is drawn with a centre tick and read out signed. Getting that wrong
 * would make "no rotation" a value you can only find by luck. The flags here
 * are the engine's own `Curve::Bipolar` entries, not a guess.
 */

const PAGES = [
  {
    slug: "main", name: "Main",
    blurb: "Time, space and how hard the machine is working.",
    params: [
      ["g.speed",         "Speed"],
      // Bipolar: negative flies back OUT of the dive. This page's param list
      // is hand-written rather than enumerated from the engine, so a new
      // global is invisible here until it is added -- which is exactly how
      // JRNY went missing from the web while showing up on the panel.
      ["g.journey",       "Journey", 1],
      ["g.zoom",          "Zoom"],
      ["g.detail",        "Detail"],
      ["g.center_x",      "Pan X", 1],
      ["g.center_y",      "Pan Y", 1],
      ["g.rotation",      "Rotate", 1],
      ["g.morph",         "Morph"],
      ["g.fractal_level", "Fractal level"],
      ["g.render_scale",  "Resolution"],
      ["g.glide",         "Glide"],
    ],
  },
  {
    slug: "colour", name: "Colour",
    blurb: "Colour comes out of the iteration itself — these shape how it is "
         + "read, not a filter laid over the picture.",
    params: [
      ["g.palette_mix",    "Palette mix"],
      ["g.palette_shift",  "Palette rotate"],
      ["g.palette_sat",    "Saturation"],
      ["g.palette_spread", "Spread"],
      ["g.stripe_freq",    "Stripe"],
      ["g.prism",          "Prism"],
    ],
  },
  {
    /* The generator's OWN knobs. There is no fixed list to write down: they
       are `k.*` for the generator and `b.k.*` for the morph target, they are
       different knobs for every one of the thirty generators, and the bank is
       rebuilt whenever you change it. So this page is built from whatever the
       engine says it has — which is the only version of this page that cannot
       go stale. */
    slug: "shape", name: "Shape", dynamic: "k",
    blurb: "The generator's own knobs, and the morph target's. This page is "
         + "whatever the engine says it has — change the generator and it "
         + "changes with it.",
  },
  {
    slug: "swarm", name: "Swarm",
    blurb: "Cohesion is the one that changes what you are looking at: at full "
         + "travel the particles tile the screen and rebuild the image, at "
         + "zero the same swarm is dust.",
    params: [
      ["p.cohesion",    "Cohesion"],
      ["p.gravity",     "Gravity", 1],
      ["p.swirl",       "Swirl", 1],
      ["p.motion",      "Motion"],
      ["p.speed",       "Speed"],
      // Density, not a crop: the alive pool tiles the whole frame at any
      // count, so turning this down thins the swarm rather than shrinking
      // the picture it makes.
      ["p.count",       "Count · density"],
      ["p.size",        "Size"],
      ["p.bright",      "Bright"],
      ["p.damping",     "Damping"],
      ["p.carry",       "Carry"],
      ["p.field_color", "Tint"],
      ["p.deposit",     "Deposit"],
      ["p.life",        "Life"],
      ["p.die_interior", "Die: sink"],
      ["p.die_edge",     "Die: edge"],
      ["p.die_exhaust",  "Die: burn"],
    ],
  },
  {
    slug: "draw", name: "Draw",
    blurb: "The brush. Strokes composite before the effect chain, so feedback "
         + "smears whatever you draw into structure.",
    params: [
      ["d.size",        "Brush size"],
      ["d.flow",        "Flow"],
      ["d.soft",        "Softness"],
      ["d.fade",        "Fade"],
      ["d.field_color", "Tint"],
    ],
  },
  {
    slug: "mod", name: "Modulation",
    /* An LFO is a SOURCE in the mapping table now — `lfo:0` beside `macro:0`
       — rather than a thing with one target field. So one LFO can fan out to
       several parameters, each with its own range and polarity, and `lfo:*`
       rows ADD a bipolar offset where every other source SETS. None of that
       is readable or writable over OSC, so this page carries the LFO's own
       knobs and says where the rest lives instead of guessing. */
    blurb: "Four LFOs and four macros. Depth is bipolar and the centre is off "
         + "— negative is the same shape inverted. What each one DRIVES is a "
         + "mapping row on the device: an LFO is a source now, so it can fan "
         + "out to several parameters at once, adding to each rather than "
         + "overwriting it. A knob with an LFO on it still reads here where "
         + "you put it, not where the LFO has taken it.",
    params: [
      ["lfo0.rate", "LFO 1 rate"], ["lfo0.depth", "LFO 1 depth", 1],
      ["lfo0.shape", "LFO 1 shape"], ["lfo0.phase", "LFO 1 phase"],
      ["lfo1.rate", "LFO 2 rate"], ["lfo1.depth", "LFO 2 depth", 1],
      ["lfo1.shape", "LFO 2 shape"], ["lfo1.phase", "LFO 2 phase"],
      ["lfo2.rate", "LFO 3 rate"], ["lfo2.depth", "LFO 3 depth", 1],
      ["lfo2.shape", "LFO 3 shape"], ["lfo2.phase", "LFO 3 phase"],
      ["lfo3.rate", "LFO 4 rate"], ["lfo3.depth", "LFO 4 depth", 1],
      ["lfo3.shape", "LFO 4 shape"], ["lfo3.phase", "LFO 4 phase"],
      ["macro0", "Macro 1"], ["macro1", "Macro 2"],
      ["macro2", "Macro 3"], ["macro3", "Macro 4"],
    ],
  },
  {
    /* Effect knobs are SLOT-scoped — `fx<slot>.<knob>` — so that two
       instances of one effect stay independently controllable. The cost is
       that these ids describe a CHAIN, and the chain is editable on the
       device. So this page is dynamic too: whatever `fx*` ids the engine
       currently reports, in the order it reports them, which is chain order.
       A written-down list here would aim at last week's chain. */
    slug: "fx", name: "Effects", dynamic: "fx",
    blurb: "The chain the engine is running right now, slot by slot. Edit the "
         + "chain on the device and this page follows it.",
  },
];

/* The four you reach for without looking. They live on the index screen so
   the common case is one tap deep, not two. */
const QUICK = [
  ["g.speed", "Speed"], ["g.zoom", "Zoom"],
  ["g.detail", "Detail"], ["g.fractal_level", "Fractal level"],
];

/* 0, 1, 2 are the protocol's own numbering for /nv/mode — they are part of
   the contract, not a local convention, so naming them here is safe in a way
   that naming the thirty generators would not be. */
const MODES = ["Synth", "Particles", "Draw"];

/* Every id this file writes down, so the dynamic pages can be defined as
   "everything else" and a knob added to the engine tomorrow still turns up
   somewhere instead of being invisible. */
const NAMED = new Set();
for (const p of PAGES) for (const q of (p.params || [])) NAMED.add(q[0]);

/* A handful of ids whose derived name would be actively misleading rather
   than merely terse. This is a READABILITY table, never a gate: an id that is
   not in it still gets a derived name and still gets drawn, which is the rule
   that stops the full-bank screen from ever hiding part of the instrument.

   `p.count` earns its entry — it used to crop the swarm and now sets its
   density (the alive pool tiles the whole frame at any count), so "Count"
   alone now describes the old behaviour. */
const NICE = {
  "p.count":        "Count · density",
  "p.die_interior": "Die: sink (interior)",
  "p.die_edge":     "Die: edge",
  "p.die_exhaust":  "Die: burn (exhaust)",
  "p.deposit":      "Deposit into ink",
  "p.field_color":  "Tint from palette",
  "d.field_color":  "Tint from palette",
  "g.fractal_level": "Fractal level",
  "g.render_scale": "Resolution",
  "g.glide":        "Glide",
};

/* An id we were not told a label for. `fx0.rotate` -> "Rotate", `k.wobble` ->
   "Wobble", `b.k.scale` -> "B · Scale". Derived rather than tabulated,
   because the whole point of a dynamic page is that there is no table.

   `ctx` is the group the id is already sitting under. Repeating the context
   in every row of a group headed "Effect 2" is noise, so when the caller can
   say where the fader is, the prefix comes off. */
function labelFor(id, ctx) {
  // The engine's own label, when it has told us one. Everything below this
  // line is the fallback for an engine that cannot answer /nv/list.
  const c = CAT.param[id];
  if (c && c.label) {
    // Effect and morph knobs still get their context stripped when the group
    // heading already carries it -- the engine labels a knob, not a position
    // in this page's layout.
    if (ctx === "fx" || ctx === "morph") return c.label;
    const slot = /^fx(\d+)\./.exec(id);
    if (slot) return slot[1] + " · " + c.label;
    if (id.indexOf("b.") === 0) return "B · " + c.label;
    return c.label;
  }
  if (NICE[id]) return NICE[id];

  const lfo = /^lfo(\d+)\.(.+)$/.exec(id);
  if (lfo) {
    const knob = cap(lfo[2]);
    return ctx === "lfo" ? "LFO " + (+lfo[1] + 1) + " " + knob.toLowerCase()
                         : "LFO " + (+lfo[1] + 1) + " " + knob.toLowerCase();
  }
  const macro = /^macro(\d+)$/.exec(id);
  if (macro) return "Macro " + (+macro[1] + 1);

  const morph = id.indexOf("b.") === 0;
  const tail = cap(id.slice(id.lastIndexOf(".") + 1));
  if (morph) return ctx === "morph" ? tail : "B · " + tail;
  const slot = /^fx(\d+)\./.exec(id);
  if (slot) return ctx === "fx" ? tail : slot[1] + " · " + tail;
  return tail;
}

function cap(s) {
  s = s.replace(/_/g, " ");
  return s.charAt(0).toUpperCase() + s.slice(1);
}

/* ── which knobs are bipolar ──────────────────────────────────────────────
 *
 * There is no polarity flag on the wire — `/nv/state` is `<id> <norm>` and
 * the curve stays on the device — so this has to be known here. Getting it
 * wrong is not dangerous (the value written is the same either way) but it is
 * misleading in both directions: a bipolar knob drawn 0..100% hides the
 * detent that means "off", and a unipolar knob drawn ±100 invents a centre
 * that is not there. Both were visible on the full-bank screen at once, which
 * is what a single loose regex over sixty-seven ids buys you: SPEED read
 * "+38" and PAN X read "50%", and each was the other's mistake.
 *
 * So it is split by how well the namespace is actually known:
 *
 *   EXACT for the stable bank. `g.*`, `p.*`, `d.*`, `lfo*`, `macro*` are
 *   fixed in the engine's own `state.cpp`/`mod.cpp`, and these are its
 *   `Curve::Bipolar` entries, listed rather than pattern-matched.
 *
 *   EXACT for effects, by knob name. The effect bank is fixed too, and every
 *   `Curve::Bipolar` knob in it is one of these seven names — no unipolar
 *   knob anywhere in the bank shares one, so the name decides it whatever
 *   slot the effect lands in.
 *
 *   A GUESS for `k.*` / `b.k.*` only. Those are the current generator's own
 *   knobs, thirty different sets of them, and a table would be the one thing
 *   this page has been careful not to keep: something that goes stale when a
 *   generator is added. The cost of a wrong guess here is a readout sign on
 *   one knob of one generator.
 */
const BIP_EXACT = new Set([
  "g.rotation", "g.center_x", "g.center_y",   // state.cpp
  "p.gravity", "p.swirl",                     // bipolar so either can reverse
  "lfo0.depth", "lfo1.depth", "lfo2.depth", "lfo3.depth",  // mod.cpp
]);
const BIP_FX_KNOB = /^(bright|angle|cycle|spin|speed|zoom|rotate)$/;
const BIP_KERNEL_GUESS = /\.(cre|cim|drift|wobble|skew|bias|offset|angle|rotate|spin)$/;

function bipolarFor(id) {
  // Known, not guessed: the engine sends the real range, and a knob is
  // bipolar exactly when its travel crosses zero. This retires the three
  // heuristics below for every engine that answers /nv/list -- including the
  // `k.*` guess, which was explicitly a guess over thirty generators.
  const c = CAT.param[id];
  if (c && isFinite(c.min) && isFinite(c.max)) return c.min < 0;
  if (BIP_EXACT.has(id)) return true;
  const fx = /^fx\d+\.(.+)$/.exec(id);
  if (fx) return BIP_FX_KNOB.test(fx[1]);
  if (/^(b\.)?k\./.test(id)) return BIP_KERNEL_GUESS.test(id);
  return false;
}

/* Which ids belong to a dynamic page, in the order the engine listed them. */
function dynamicIds(kind, state) {
  const ids = Object.keys(state.params || {});
  if (kind === "fx") return ids.filter(id => /^fx\d+\./.test(id));
  return ids.filter(id => /^(b\.)?k\./.test(id));
}

/* ── grouping the whole bank ──────────────────────────────────────────────
 *
 * The VISUALS screen is every parameter the engine has, and the grouping is
 * derived from the **id namespace** rather than written down.
 *
 * That is not a shortcut around the bank's own `group` field — it is the only
 * thing available. `/nv/state` carries exactly `<id:s> <norm:f>`; the group,
 * the label, the range and the curve all stay on the device. Deriving from
 * the id is what makes this page structurally identical to the bank at all
 * times: `fx<slot>.` IS the slot, `k.` IS the current generator, `b.k.` IS
 * the morph target, and those prefixes are the same fact the engine used to
 * build the group.
 *
 * The last rule is the one that matters most: **anything that matches nothing
 * still gets a group.** A parameter the engine gains tomorrow in a namespace
 * nobody here has heard of turns up under OTHER, which is the difference
 * between a page that ages and a page that silently stops showing part of the
 * instrument. A control surface hiding a control is the worst failure
 * available to it.
 */
const GROUPS = [
  { key: "global", name: "Global", tag: "GLOBAL",
    test: id => /^g\./.test(id) && !/^g\.(palette|stripe|prism)/.test(id),
    blurb: "Time, space, detail and the layer mix." },
  { key: "colour", name: "Colour", tag: "COLOUR",
    test: id => /^g\.(palette|stripe|prism)/.test(id),
    blurb: "How the iteration's own quantities are read as colour." },
  { key: "gen", name: "Generator", tag: "GEN",
    test: id => /^k\./.test(id),
    blurb: "This generator's own knobs. They change with the generator." },
  { key: "morph", name: "Morph target", tag: "MORPH",
    test: id => /^b\.k\./.test(id),
    blurb: "The B side's knobs, so you can move A and B independently." },
  { key: "swarm", name: "Swarm", tag: "SWARM",
    test: id => /^p\./.test(id),
    blurb: "The particles: forces, lifetime, and what they leave behind." },
  { key: "ink", name: "Ink", tag: "INK",
    test: id => /^d\./.test(id),
    blurb: "The brush. Strokes composite before the effect chain." },
  { key: "lfo", name: "LFOs", tag: "LFO",
    test: id => /^lfo\d+\./.test(id),
    blurb: "Four free-running shapes. Depth is bipolar and its centre is off."
         + " What each one drives is a MAP row on the device — an LFO is a"
         + " source now, so one can fan out to several parameters." },
  { key: "macro", name: "Macros", tag: "MACRO",
    test: id => /^macro\d+$/.test(id),
    blurb: "Knobs that are sources: what they drive is a MAP row on the"
         + " device, several parameters at a time." },
  // Effects come last and are split PER SLOT below, because `fx<slot>.` is
  // slot-scoped precisely so two instances of one effect stay independent.
  { key: "fx", name: "Effects", tag: "FX", perSlot: true,
    test: id => /^fx\d+\./.test(id),
    blurb: "The chain the engine is running, slot by slot." },
  { key: "other", name: "Other", tag: "OTHER",
    test: () => true,
    blurb: "Parameters this page has no name for. They are still yours to"
         + " move — the engine takes them by id like any other." },
];

/* The whole bank, bucketed, in the order above. Effect slots become one
   group each so a twelve-effect chain does not arrive as one heap. */
/* The engine's OWN grouping, when it will tell us -- which is the whole bank
   laid out the way the device lays it out, in the order the bank is built.

   Better than the namespace rules below in three ways that show immediately:
   effects group under their EFFECT NAME (FEEDBACK, KALEIDO) rather than
   "Effect 0", so two instances of one effect stay legible; `operator` -- the
   jit.op controls -- gets a section instead of falling into OTHER; and a
   group the engine adds later appears here without this file being edited.

   Sections come out in first-appearance order, which IS bank order, so the
   page reads top-to-bottom in the same sequence as the panel. */
function groupByCatalog(state) {
  const ids = Object.keys(state.params || {});
  const seen = [];
  const bucket = new Map();
  // Walk the catalogue, not the state dict: the catalogue is ordered and a
  // JSON object's key order is not something to lean on.
  const order = Object.keys(CAT.param);
  const known = new Set(order);
  for (const id of order) {
    if (!(id in (state.params || {}))) continue;   // not in this patch
    const g = CAT.param[id].group || "other";
    if (!bucket.has(g)) { bucket.set(g, []); seen.push(g); }
    bucket.get(g).push(id);
  }
  // Anything live that the catalogue has never heard of still gets drawn.
  // A control surface hiding a control is the worst failure available to it,
  // and a catalogue arriving a beat late must not blank the page.
  const orphans = ids.filter(id => !known.has(id));
  const out = [];
  for (const g of seen) {
    const pretty = /[a-z]/.test(g)
      ? g.charAt(0).toUpperCase() + g.slice(1)   // "global" -> "Global"
      : g;                                       // "FEEDBACK" is a name
    out.push({
      g: { key: g, blurb: "" }, name: pretty, tag: g.toUpperCase(),
      ids: bucket.get(g),
    });
  }
  if (orphans.length) {
    out.push({ g: { key: "other", blurb: "Parameters the engine has not "
      + "described. They are still yours to move -- it takes them by id." },
      name: "Other", tag: "OTHER", ids: orphans });
  }
  return out;
}

function groupBank(state) {
  // Only once the catalogue is actually populated -- otherwise the namespace
  // rules below carry the page, exactly as they did before.
  if (Object.keys(CAT.param).length) {
    const g = groupByCatalog(state);
    if (g.length) return g;
  }
  const ids = Object.keys(state.params || {});
  const out = [];
  const taken = new Set();
  for (const g of GROUPS) {
    const mine = ids.filter(id => !taken.has(id) && g.test(id));
    mine.forEach(id => taken.add(id));
    if (!mine.length) continue;
    if (!g.perSlot) { out.push({ g, name: g.name, tag: g.tag, ids: mine }); continue; }
    const slots = [];
    for (const id of mine) {
      const n = /^fx(\d+)\./.exec(id)[1];
      let s = slots.find(x => x.n === n);
      if (!s) slots.push(s = { n: n, ids: [] });
      s.ids.push(id);
    }
    slots.sort((a, b) => a.n - b.n);
    for (const s of slots) {
      out.push({ g, name: "Effect " + s.n, tag: "FX" + s.n, ids: s.ids });
    }
  }
  return out;
}

/* ── the write path ───────────────────────────────────────────────────────
   One endpoint, one shape. Posts are quiet because a fader drag is a stream
   and a toast per frame would be unusable — but the FIRST failure is spoken,
   once, because a page of controls that silently does nothing is the worst
   outcome available. */
let spoken = false;
function sendParam(id, value) {
  return post("/api/video/param", { id: id, value: value }, true).then(r => {
    if (r && r.ok) { spoken = false; return; }
    if (spoken) return;
    spoken = true;
    toast(r && r.status === 404
      ? "This server has no /api/video/param route yet — the video engine "
        + "cannot be reached from here."
      : "The video engine did not take that. Is neptr-video running?");
  });
}

function sendSelect(what, index) {
  return post("/api/video/select", { what: what, index: index });
}

/* ── a fader ──────────────────────────────────────────────────────────────
 *
 * The same `.fader` shape, the same `bindDrag`, the same throttle as every
 * other fader in the app. The difference from the NEPTR faders is that this
 * one is an ABSOLUTE write, not a control loop over relative detents — so it
 * lands where you put it and the echo confirms it rather than chasing it.
 *
 * The echo is still what lights the fill. `hold` is the one subtlety: for a
 * moment after a write, the snapshot in flight is older than the finger, so
 * letting it repaint would drag the thumb backwards. Holding for a beat is
 * the whole fix, and it is bounded — let go, and the engine is authoritative
 * again within half a second.
 */
function buildFader(id, label, bipolar, i, showId) {
  const wrap = el("div", "fader");
  wrap.style.setProperty("--f", "var(--f" + (i % 4) + ")");

  const head = el("div", "head");
  const lbl = el("span", "lbl", label);
  // On the full-bank screen the id is shown under the label. That screen
  // exists to answer "which knob is that exactly", and the id is both the
  // answer and the thing `/nv/param` is addressed by — so it is the caption
  // that is actually load-bearing, not decoration.
  if (showId) {
    lbl.appendChild(el("br"));
    lbl.appendChild(el("span", "pid", id));
  }
  head.appendChild(lbl);
  const val = el("span", "val", DASH);
  head.appendChild(val);
  wrap.appendChild(head);

  const track = el("div", "track");
  const fill = el("div", "fill");
  const edge = el("div", "edge");
  track.appendChild(fill);
  track.appendChild(edge);

  /* A bipolar knob's centre is a real place — "no rotation", "no gravity" —
     and the engine puts a detent there. Marking it means you can find it
     with a thumb instead of by luck. */
  if (bipolar) {
    const mid = el("div", "target");
    mid.style.left = "calc(50% - 1px)";
    mid.style.background = "var(--rim)";
    track.appendChild(mid);
  }
  wrap.appendChild(track);

  const readout = n => bipolar
    ? (n * 2 - 1 >= 0 ? "+" : "−")
      + Math.abs(Math.round((n * 2 - 1) * 100))
    : Math.round(n * 100) + "%";

  const paint = n => {
    n = Math.max(0, Math.min(1, n));
    fill.style.width = (n * 100) + "%";
    edge.style.left = (n * 100) + "%";
    val.textContent = readout(n);
  };

  let last = null, hold = 0;
  bindDrag(track, wrap, frac => {
    paint(frac);
    hold = Date.now() + 500;
    // Nothing to send if the thumb has not moved a whole tenth of a percent:
    // at 90 ms between posts a sub-pixel jitter is still a datagram.
    const q = Math.round(frac * 1000) / 1000;
    if (q === last) return null;
    last = q;
    return { id: id, value: q };
  }, body => { hold = Date.now() + 500; sendParam(body.id, body.value); });

  return {
    node: wrap,
    id: id,
    // Lower-cased once at build time: the filter runs over every fader on
    // every keystroke, and sixty-odd toLowerCase() calls per character is
    // work nobody asked for.
    hay: (label + " " + id).toLowerCase(),
    show(on) { wrap.classList.toggle("hidden", !on); },
    sync(state) {
      const v = state.params ? state.params[id] : undefined;
      if (v == null) {
        // Not in this patch. The engine ignores writes to it, so the fader
        // says so rather than pretending to be a control.
        track.dataset.unavailable = "1";
        val.textContent = DASH;
        fill.style.width = "0";
        edge.style.left = "0";
        return;
      }
      delete track.dataset.unavailable;
      if (Date.now() < hold || wrap.classList.contains("grabbing")) return;
      last = null;                  // the engine moved it; re-arm the filter
      paint(v);
    },
  };
}

/* Build a run of faders and register them with the poller as ONE watcher.
   One watcher per group rather than per fader keeps the filter in `poll()`
   cheap and makes teardown a single check. Returns a plain div, not a
   <section>, so a caller that is already inside one does not get the
   section's top margin twice. */
function buildFaders(defs, showId) {
  const node = el("div");
  const faders = defs.map(([id, label, bip], i) => {
    const f = buildFader(id, label, !!bip, i, showId);
    node.appendChild(f.node);
    return f;
  });
  return { node, faders, sync(s) { for (const f of faders) f.sync(s); } };
}

function faderGroup(defs, box) {
  const g = buildFaders(defs);
  watch(box, g.sync);
  return g.node;
}

/* ── the strip that says whether any of this is real ──────────────────────
   The same job `linkStrip()` does inside NEPTR, and worth repeating rather
   than sharing, because what it has to say is different — and because the
   one thing a control surface must never do is look live when it is not. */
function videoLink(box) {
  const d = el("div", "link");
  const dot = el("i", "dot");
  const txt = document.createTextNode("");
  d.appendChild(dot);
  d.appendChild(txt);
  const set = s => {
    d.classList.toggle("bad", !s.live);
    dot.className = "dot " + (s.live ? "live" : "bad");
    txt.nodeValue = s.live
      ? "Live — every fader below is lit by the engine itself."
      : "No answer from the video engine on UDP " + (s.port || 8004)
        + ". Controls still send; nothing is confirming them.";
  };
  set(vstate);
  watch(box, set);
  return d;
}

/* ── VISUALS: every parameter, directly ──────────────────────────────────
 *
 * The curated pages are a route through the instrument. This one is the
 * instrument: every id the engine reports, nothing chosen, nothing left out.
 *
 * Sixty-seven sliders in one list IS a wall — that was true when the paged
 * screens were built and it is still true. What makes this screen playable is
 * that the wall is navigable, and all three of the things that do it are
 * navigation rather than a smaller list:
 *
 *   FILTER   type "die" and the three death hazards are the whole screen.
 *            It matches the id as well as the label, so `fx2` finds a slot
 *            and `palette` finds the colour knobs, and it is the fastest way
 *            to reach one known knob out of sixty-seven on a phone.
 *   JUMP     a sticky row of chips, one per group, scrolling sideways rather
 *            than wrapping to three lines and pushing the faders off screen.
 *   FOLD     the group header is the fold control and is the full width of
 *            the screen, so it is a thumb target rather than a chevron.
 *            Which groups are shut is remembered per device.
 *
 * Everything here is built from the broadcast. There is no list of parameters
 * in this file for this screen — change the generator or edit the chain on
 * the device and the groups appear, disappear and renumber themselves,
 * because the id namespace IS the structure.
 *
 * The one thing the screen deliberately cannot do is show or edit the MAP
 * rows that decide what an LFO or a macro drives. No OSC verb reads or writes
 * them, and inventing a wire format from this side is how two mechanisms for
 * one job get started. It says so rather than hiding it.
 */

const FOLD_KEY = "nv.visuals.shut";

function readShut() {
  try { return new Set(JSON.parse(sessionStorage.getItem(FOLD_KEY) || "[]")); }
  catch (e) { return new Set(); }
}
function writeShut(set) {
  try { sessionStorage.setItem(FOLD_KEY, JSON.stringify([...set])); }
  catch (e) { /* private mode: folding just stops being remembered */ }
}

route(["video", "visuals"], "VISUALS", () => ({
  build(box) {
    box.appendChild(videoLink(box));

    const bar = el("div", "vbar");
    const find = el("input");
    find.type = "search";
    find.placeholder = "Filter — name or id";
    find.autocapitalize = "off";
    find.autocomplete = "off";
    find.spellcheck = false;
    bar.appendChild(find);
    const jump = el("div", "vjump");
    bar.appendChild(jump);
    const count = el("div", "vcount", "");
    bar.appendChild(count);
    box.appendChild(bar);

    const host = el("div", "dense");
    box.appendChild(host);

    const shut = readShut();
    let shape = "";           // the id set the DOM was built for
    let groups = [];          // [{ tag, ids, faders, node, headCount }]
    let all = [];             // every fader, for the filter

    const applyFilter = () => {
      const q = find.value.trim().toLowerCase();
      let shown = 0;
      for (const f of all) {
        const on = !q || f.hay.indexOf(q) >= 0;
        f.show(on);
        if (on) ++shown;
      }
      for (const g of groups) {
        const n = g.faders.filter(f => !q || f.hay.indexOf(q) >= 0).length;
        // A group with nothing in it disappears while filtering, rather than
        // leaving a row of empty headers to scroll past.
        g.node.classList.toggle("hidden", n === 0);
        g.headCount.textContent = q ? n + "/" + g.faders.length
                                    : String(g.faders.length);
        // Filtering opens what it finds: a hit hidden inside a folded group
        // would read as "no results".
        g.node.classList.toggle("shut", q ? false : shut.has(g.tag));
      }
      count.textContent = q
        ? shown + " of " + all.length + " parameters"
        : all.length + " parameters, live from the engine";
    };

    const rebuildBank = s => {
      const spec = groupBank(s);
      const key = spec.map(g => g.tag + ":" + g.ids.join(",")).join("|");
      if (key === shape) return;
      shape = key;

      // Only a STRUCTURAL change gets here — a new generator, an edited
      // chain — never a value change, or every fader would be replaced ten
      // times a second and a drag would be impossible.
      const keepScroll = window.scrollY;
      host.textContent = "";
      jump.textContent = "";
      groups = [];
      all = [];

      if (!spec.length) {
        host.appendChild(el("div", "note", s.live
          ? "The engine reports no parameters."
          : "Waiting for the engine…"));
        count.textContent = "";
        return;
      }

      spec.forEach((grp, gi) => {
        const node = el("div", "vgroup");
        node.id = "vg-" + grp.tag;

        const head = el("button");
        head.type = "button";
        head.appendChild(el("span", "fold", "▾"));
        head.appendChild(document.createTextNode(grp.name));
        const n = el("span", "n", String(grp.ids.length));
        head.appendChild(n);
        node.appendChild(head);

        const body = el("div");
        if (grp.g.blurb) {
          const b = el("div", "note", grp.g.blurb);
          b.style.paddingTop = "2px";
          body.appendChild(b);
        }
        const built = buildFaders(
          grp.ids.map((id, i) => [id, labelFor(id, grp.g.key),
                                  bipolarFor(id) ? 1 : 0]),
          /*showId=*/true);
        body.appendChild(built.node);
        node.appendChild(body);
        host.appendChild(node);

        const g = { tag: grp.tag, node: node, faders: built.faders,
                    headCount: n, sync: built.sync };
        groups.push(g);
        all = all.concat(built.faders);

        head.onclick = () => {
          const nowShut = !node.classList.contains("shut");
          node.classList.toggle("shut", nowShut);
          head.querySelector(".fold").textContent = nowShut ? "▸" : "▾";
          if (nowShut) shut.add(grp.tag); else shut.delete(grp.tag);
          writeShut(shut);
        };
        if (shut.has(grp.tag)) {
          node.classList.add("shut");
          head.querySelector(".fold").textContent = "▸";
        }

        const chip = el("button", null, grp.tag);
        chip.type = "button";
        chip.onclick = () => {
          // Jumping into a folded group would land on a header with nothing
          // under it, which reads as a broken link.
          if (node.classList.contains("shut")) head.click();
          node.scrollIntoView({ behavior: "smooth", block: "start" });
        };
        jump.appendChild(chip);
      });

      applyFilter();
      window.scrollTo(0, keepScroll);
    };

    find.oninput = applyFilter;

    watch(box, s => {
      rebuildBank(s);
      for (const g of groups) g.sync(s);
    });

    box.appendChild(el("div", "note",
      "Every parameter the engine reports, grouped by its id — so this screen "
      + "follows the generator and the chain instead of describing the ones "
      + "that were current when it was written. Values are positions in the "
      + "travel, 0 to 100%, because the engine owns the curve and the units."));
    box.appendChild(el("div", "note",
      "What an LFO or a macro DRIVES is a mapping row, and lives on the "
      + "device's MAP page — there is no OSC verb that reads or writes one, so "
      + "this page does not pretend to. The knobs below set an LFO's shape, "
      + "rate, phase and depth; where that depth lands is set on the "
      + "instrument."));

    // The port is the SERVER's, not a number typed here: it is configurable
    // (DEMIURGE_VIDEO_OSC_PORT) and a chip claiming 8004 while the traffic
    // goes to 8024 is a debugging aid that lies.
    const chips = el("div", "chips");
    chips.appendChild(el("span", "chip", "/nv/param <id> <0..1>"));
    const portChip = el("span", "chip", "udp …");
    chips.appendChild(portChip);
    watch(box, s => { portChip.textContent = "udp " + (s.port || "?"); });
    box.appendChild(chips);
  },
}));

/* ── screens ─────────────────────────────────────────────────────────────
   Section separation is by URL PREFIX and nothing else, exactly as /neptr/*
   sits today: no second nav, no tab bar, no mode switch. /video is a screen
   in the same stack, so the phone's own back gesture works and a deep link
   into #/video/swarm reloads straight into it. */

route(["video"], "VIDEO", () => ({
  build(box) {
    box.appendChild(videoLink(box));

    /* The full bank comes FIRST, above the selections and above the curated
       pages. The paged screens are a route through the instrument for when
       you know what you want to reach; this is the instrument, and it is what
       "direct control" means. Putting it at the bottom under the curated
       pages would make the complete thing read as the appendix. */
    const direct = el("section");
    const dr = el("div");
    dr.appendChild(row("Visuals — every parameter", {
      sub: "The whole bank, grouped, filterable",
      chev: true,
      onclick: () => go("/video/visuals"),
    }));
    direct.appendChild(dr);
    box.appendChild(direct);

    /* MODE — three fixed values, and the numbering is the protocol's. */
    const sel = el("section");
    sel.appendChild(el("div", "lbl", "Mode"));
    const modeRow = el("div", "pager");
    const modeBtns = MODES.map((name, n) => {
      const b = el("button", null, name.toUpperCase());
      b.onclick = () => sendSelect("mode", n);
      modeRow.appendChild(b);
      return b;
    });
    sel.appendChild(modeRow);

    /* GENERATOR — thirty of them, and their names live in the engine's
       kernel bank. Mirroring that list into this file would be a second copy
       of it that goes stale the first time one is added, and there is no
       verb on the wire that asks for names — so this steps and reports the
       index, which is what /nv/kernel actually speaks. The engine wraps. */
    sel.appendChild(el("div", "lbl", "Generator"));
    const genRow = el("div", "pager");
    const prev = el("button", null, "‹ PREV");
    const genLbl = el("button", null, DASH);
    const next = el("button", null, "NEXT ›");
    genLbl.disabled = true;
    genLbl.style.cursor = "default";
    /* Stepping has to accumulate from where the LAST TAP aimed, not from the
       last thing the engine said. Two taps 700 ms apart are inside one round
       trip of a 4 Hz poll, so stepping from the echo makes the second tap
       repeat the first and the generator moves one place for two presses —
       measured, not hypothetical.

       The READOUT stays the engine's, though: the intent is where we asked to
       go, and showing it would be a number that is right about the future and
       wrong about the picture. The engine wraps in both directions, so -1 is
       the last generator and there is nothing to clamp. */
    let kernel = 0;            // what the engine says it is
    let intent = null, until = 0;
    const step = d => {
      const base = (intent !== null && Date.now() < until) ? intent : kernel;
      intent = base + d;
      // Held until the ECHO agrees, not for a fixed guess at the round trip.
      // A phone puts a backgrounded tab's timers on a one-second leash, which
      // is longer than any round-trip figure worth writing down here. The
      // expiry below is only so a step the engine never took — because it
      // wrapped past our count, or because someone turned the generator on
      // the device — cannot leave this button aiming at a stale number.
      until = Date.now() + 4000;
      sendSelect("kernel", intent);
    };
    prev.onclick = () => step(-1);
    next.onclick = () => step(1);
    genRow.appendChild(prev); genRow.appendChild(genLbl); genRow.appendChild(next);
    sel.appendChild(genRow);

    /* And the named grid, once the engine has told us the bank.
     *
     * The note that used to sit here said the wire had no verb that asks for
     * names, so this page would not invent a copy of them. That was right,
     * and the answer was not to invent a copy but to add the verb: `/nv/list`
     * now answers a `kernel` bank, so these are the ENGINE's names, read at
     * runtime, and a generator added tomorrow appears here on its own.
     *
     * The stepper above stays. It is the fallback when the catalogue has not
     * arrived, and it is still the only thing that works against an engine
     * too old to answer -- and stepping by one is genuinely the better
     * gesture when you are hunting rather than choosing.
     */
    const pick = el("div", "vpick");
    sel.appendChild(pick);
    const note = el("div", "note", "");
    sel.appendChild(note);
    box.appendChild(sel);

    let picked = [];           // the bank the grid was built for
    const buildPick = () => {
      const bank = (CAT.lists && CAT.lists.kernel) || [];
      if (!bank.length) {
        note.textContent = "Generator names arrive with the engine's"
          + " catalogue. Until then, step with PREV / NEXT.";
        return;
      }
      // Rebuilt only when the bank itself changes, so the grid does not
      // thrash under a 4 Hz poll and a click is never rebuilt out from under
      // the pointer.
      const shape = bank.map(k => k.id).join(",");
      if (shape === picked.shape) return;
      picked = bank.map((k, i) => {
        const b = el("button", null, (k.name || k.id).toUpperCase());
        b.onclick = () => {
          intent = i;
          until = Date.now() + 4000;
          sendSelect("kernel", i);
          markPick();
        };
        pick.appendChild(b);
        return b;
      });
      picked.shape = shape;
      note.textContent = bank.length + " generators, named by the engine.";
    };
    const markPick = () => {
      // Lit by the ENGINE's index, except while a click is still in flight --
      // same rule as the stepper's readout, for the same reason.
      const at = (intent !== null && Date.now() < until) ? intent : kernel;
      const n = picked.length;
      picked.forEach((b, i) => b.classList.toggle("on",
        n > 0 && ((at % n) + n) % n === i));
    };

    watch(box, s => {
      modeBtns.forEach((b, n) => b.classList.toggle("on", s.mode === n));
      buildPick();
      if (s.kernel != null) {
        kernel = s.kernel;
        // The readout is the ENGINE's, always — the intent above is where the
        // taps aimed, and showing that would be a number that is right about
        // the future and wrong about the picture.
        const bank = (CAT.lists && CAT.lists.kernel) || [];
        const me = bank[s.kernel];
        genLbl.textContent = me ? (me.name || me.id).toUpperCase()
                                : "#" + s.kernel;
        if (intent !== null && s.kernel === intent) intent = null;  // landed
      }
      markPick();
    });

    /* PRESETS — eight slots, the engine's own CTRL/SHIFT 1-8. Loading only:
       saving from a phone would write a file on the device from a surface
       that cannot see what it is about to overwrite. */
    const pre = el("section");
    pre.appendChild(el("div", "lbl", "Load preset"));
    const preRow = el("div", "pager");
    preRow.style.flexWrap = "wrap";
    for (let n = 1; n <= 8; ++n) {
      const b = el("button", null, String(n));
      b.style.flex = "1 1 20%";
      b.onclick = () => sendSelect("preset", n);
      preRow.appendChild(b);
    }
    pre.appendChild(preRow);
    pre.appendChild(el("div", "note",
      "Load only. A slot with no file on the device is ignored."));
    box.appendChild(pre);

    const quick = el("section");
    quick.appendChild(el("div", "lbl", "At hand"));
    quick.appendChild(faderGroup(QUICK, box));
    box.appendChild(quick);

    const sec = el("section");
    sec.appendChild(el("div", "lbl", "Shortcuts"));
    const wrap = el("div");
    wrap.style.marginTop = "10px";
    for (const p of PAGES) {
      wrap.appendChild(row(p.name, {
        sub: p.dynamic
          ? "follows the engine"
          : p.params.length + " control"
            + (p.params.length === 1 ? "" : "s"),
        chev: true,
        onclick: () => go("/video/" + p.slug),
      }));
    }
    sec.appendChild(wrap);
    box.appendChild(sec);

    box.appendChild(el("div", "note",
      "The engine has to be showing VIDEO for any of this to be visible — it "
      + "boots into its own HOME selector, and a parameter set while that "
      + "menu is up lands in the bank and shows the moment you enter the "
      + "mode."));
  },
}));

for (const page of PAGES) {
  route(["video", page.slug], page.name, () => ({
    build(box) {
      box.appendChild(videoLink(box));
      if (page.blurb) {
        const b = el("div", "note", page.blurb);
        b.style.paddingTop = "0";
        b.style.paddingBottom = "6px";
        box.appendChild(b);
      }

      if (page.dynamic) {
        /* A dynamic page cannot be drawn before the engine has said what is
           on it. Rather than draw an empty page and leave it empty, it is
           drawn once the first snapshot lands and rebuilt whenever the set of
           ids CHANGES — not whenever a value changes, or every fader would be
           replaced ten times a second and a drag would be impossible. */
        const host = el("div");
        box.appendChild(host);
        let shape = "", group = null;
        // ONE watcher for the page, which owns whatever group is currently
        // drawn. Registering a fresh watcher per rebuild would leave the old
        // one in the list until the whole screen went away, syncing faders
        // that are no longer on the page.
        watch(box, s => {
          const ids = dynamicIds(page.dynamic, s);
          const key = ids.join(",");
          if (key !== shape) {
            shape = key;
            group = null;
            host.textContent = "";
            if (!ids.length) {
              host.appendChild(el("div", "note", s.live
                ? "This generator has no knobs of its own."
                : "Waiting for the engine…"));
            } else {
              group = buildFaders(
                ids.map(id => [id, labelFor(id), bipolarFor(id) ? 1 : 0]));
              host.appendChild(group.node);
            }
          }
          if (group) group.sync(s);
        });
      } else {
        const sec = el("section");
        sec.appendChild(faderGroup(page.params, box));
        box.appendChild(sec);
      }

      const chips = el("div", "chips");
      chips.appendChild(el("span", "chip", "/nv/param"));
      const pc = el("span", "chip", "udp …");
      chips.appendChild(pc);
      watch(box, s => { pc.textContent = "udp " + (s.port || "?"); });
      box.appendChild(chips);
    },
  }));
}

/* ── the index row on HOME ────────────────────────────────────────────────
 *
 * VIDEO has to be reachable from the top level or it is a URL you have to
 * know, which is not a section of an app. The home screen is built inside
 * app.js and this file does not edit app.js — so it wraps that screen's own
 * make() and appends one row after it has drawn, rather than reaching into
 * anything private. If the owning agent later adds the row natively, delete
 * this block and nothing else changes.
 *
 * Deliberately additive and deliberately defensive: if the home screen's
 * shape ever changes under us, the worst case is that the row is not drawn.
 * A crash here would take the whole home screen down with it, and the home
 * screen is the one that says whether the rig is making sound.
 */
function mountHomeRow() {
  const home = ROUTES.find(r => r.pattern.length === 0);
  if (!home || home._videoWrapped) return;
  home._videoWrapped = true;

  const make = home.make;
  home.make = function () {
    const s = make.apply(this, arguments) || {};
    let box = null;
    const build = s.build, update = s.update;
    s.build = function (node) { box = node; if (build) build.call(this, node); };
    s.update = function () {
      if (update) update.call(this);
      try {
        // The home screen empties and rebuilds its menu list on every update,
        // so the row is appended on every update rather than once — appending
        // it in build() would put it above a list that had not been drawn
        // yet, and then have it wiped by the first refresh.
        const list = box && box.lastChild;
        if (!list || !list.appendChild) return;
        list.appendChild(row("Video", {
          sub: "Fractals, feedback and the swarm",
          chev: true,
          onclick: () => go("/video"),
        }));
      } catch (e) { /* never take the home screen down over a menu row */ }
    };
    return s;
  };
}
mountHomeRow();

/* ── cold-load deep links ─────────────────────────────────────────────────
 *
 * app.js runs its first navigate() at the END of its own file — before this
 * file has registered anything. So a phone opening a bookmarked
 * #/video/swarm finds no route yet, and app.js does the correct thing with
 * an unknown hash: it bounces to home, rewriting location.hash on the way.
 * By the time we get here the hash we were asked for is gone.
 *
 * It is recoverable, because the Navigation Timing entry keeps the URL the
 * document was actually navigated to, fragment and all, no matter what the
 * page has done to location since. So: if we were bounced off a /video URL,
 * put it back. Guarded three ways — only on a hash we own, only when the
 * current hash is the home we were bounced to, and only if the API is there
 * — so the ordinary case cannot be disturbed by it.
 *
 * The alternative was asking for the <script> tag to go BEFORE app.js, which
 * trades one line of documentation for a load-order rule someone has to know
 * and keep. This keeps the mounting instruction "one line, anywhere after
 * app.js".
 */
let restored = false;
(function restoreDeepLink() {
  const at = location.hash.replace(/^#\/?/, "");
  if (at !== "" && at !== "/") return;   // not a bounce; nothing to undo
  let entry = null;
  try { entry = performance.getEntriesByType("navigation")[0]; } catch (e) { }
  if (!entry || !entry.name) return;
  const hash = entry.name.indexOf("#");
  if (hash < 0) return;
  const want = entry.name.slice(hash + 1);
  if (!/^\/?video(\/|$)/.test(want)) return;
  location.hash = want;                  // fires hashchange -> navigateReset
  restored = true;
})();

/* ── the wide layout, marked on <body> ────────────────────────────────────
 *
 * style.css lifts `main`'s 620px cap and columnises the bank for
 * `body.video-wide` past 1000px. The class is set from here rather than from
 * a screen's build(), because a route has no teardown hook in app.js — a
 * class added on the way in would still be there on every screen after it,
 * and the phone pages would silently inherit a layout written for a desk.
 *
 * Driven off the hash instead, which is the one thing that is always true of
 * where you are, so entering and LEAVING the section are the same event. */
function markWide() {
  const at = location.hash.replace(/^#\/?/, "");
  document.body.classList.toggle("video-wide", /^video(\/|$)/.test(at));
}
window.addEventListener("hashchange", markWide);
markWide();

/* And for every other hash, re-enter the router so the routes registered
   above are visible to the screen that is already on screen — in particular
   HOME, which app.js has already built without the Video row. Skipped after a
   deep-link restore, because the hashchange that fires from it does the same
   job and doing both would build the screen twice. */
if (!restored && typeof rebuild === "function") rebuild();

})();

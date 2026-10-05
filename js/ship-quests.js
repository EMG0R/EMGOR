// ship-quests.js — seeded quests for NO MANS GOR (docs/ship-mode.md Tier 1 item 4). Pure data + reducers, no three.js, node-testable.
//   generateQuest(seed, ctx, kind?) -> quest { id, kind, title, giverLine, steps:[{type,planetId?,item?:{stackKey,n,name},bossName?,count?,target?,seconds?,done,have}],
//                                    reward:{gor,items:[{stackKey,n,item}],words}, expiresMin, difficulty 1..3, giverKind, planetId, state:'open' }
//     ctx = { planetIds:[...], currentPlanetId, giverKind:'npc'|'board'|'dealer'|'clerk', wave, bossNameFn?(wave) }
//     bossNameFn: pass ship-enemies' bossName (it pulls three.js, so it is injected, not imported); a local fallback is used otherwise.
//     kinds: deliver | bounty | scan | retrieve | escort | harvest. Same (seed, ctx) -> identical quest (JSON-equal).
//   offerSet(seed, ctx, n=3) -> n quests of DISTINCT kinds, difficulty scaled to ctx.wave.
//   progress(quest, event) -> { quest: newQuest (input untouched), completed, stepDone, message }
//     events: {type:'kill',bossName} {type:'harvest',stackKey,n} {type:'deliver',planetId,stackKey,n}
//             {type:'scan',planetId,target} {type:'retrieve',questId} {type:'escort',seconds}
//     Steps run in order; only the first undone step reacts. Overflow n is clamped. Wrong events are no-ops (message '').
//   describe(quest) -> 2-line HUD text "TITLE\nobjective (progress)".
//   spawnSpecFor(quest) -> retrieve: { type:'crate', questId, where:'belt'|'planet', planetId, seed, pos:[x,y,z] unit-ish (belt: angle/radius/height; planet: unit vector)}
//                          escort: { type:'freighter', questId, seconds, route:[[x,y,z]...], speed } | null for the rest.
//   Reward items are stacks {stackKey,n,item}; ship.js adds them to the inventory, gor to the wallet, words to profile.known via lingo.teach.
import { resourceItem, generateItem, RESOURCE_BASE } from './ship-items.js';
import { fmt, stackKeyOf } from './ship-craft.js';
import { name as lingoName, line as lingoLine } from './ship-lingo.js';

export const KINDS = ['deliver', 'bounty', 'scan', 'retrieve', 'escort', 'harvest'];
const RES = Object.keys(RESOURCE_BASE);
const SCAN_TARGETS = ['creature', 'resource', 'creature', 'plant'];

function hashStr(s) { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
function mulberry(a) { return function () { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }
const pick = (r, a) => a[Math.floor(r() * a.length)];
const ri = (r, lo, hi) => lo + Math.floor(r() * (hi - lo + 1));
const clone = (o) => JSON.parse(JSON.stringify(o));
const num = (s) => (typeof s === 'number' ? s >>> 0 : hashStr(s));

function fallbackBoss(wave) {
  const S = ['XAL', 'MOR', 'ZETH', 'VOR', 'KRUN', 'GOR', 'NYX', 'DRAK'], r = mulberry((wave | 0) * 2654435761 + 12345);
  return S[Math.floor(r() * S.length)] + S[Math.floor(r() * S.length)] + ' THE HOLLOW';
}

function difficultyFor(r, wave) {
  const base = wave >= 10 ? 3 : wave >= 4 ? 2 : 1;
  const d = base + (r() < 0.25 ? 1 : r() < 0.15 ? -1 : 0);
  return Math.max(1, Math.min(3, d));
}

const PITCH = {
  deliver: (q) => `Take ${q.n} ${q.res} to the 7/11 on ${q.dest}. Counter guy is waiting.`,
  bounty: (q) => `${q.boss} has to go. Bring it down.`,
  scan: (q) => `Scan ${q.n} ${q.target}s on ${q.dest} with V. Do not pet them.`,
  retrieve: (q) => `A crate fell off a hauler ${q.where}. Find it.`,
  escort: () => `Fly beside my freighter for 60 seconds. It is nervous.`,
  harvest: (q) => `Bring me ${q.n} ${q.res}. Any planet. I am not picky.`,
};
const GREET = { npc: 'greeting', board: 'greeting', dealer: 'shopper', clerk: 'shopper' };

export function generateQuest(seed, ctx, kind) {
  ctx = ctx || {};
  const sd = num(seed), r = mulberry(sd ^ 0x51ED270B);
  const planets = (ctx.planetIds && ctx.planetIds.length) ? ctx.planetIds : ['Home'];
  const cur = ctx.currentPlanetId != null ? ctx.currentPlanetId : planets[0];
  const others = planets.filter((p) => p !== cur), pool = others.length ? others : planets;
  const wave = Math.max(1, ctx.wave | 0 || 1), giverKind = ctx.giverKind || 'board';
  const k = KINDS.indexOf(kind) >= 0 ? kind : pick(r, KINDS);
  const diff = difficultyFor(r, wave);
  const dest = pick(r, pool), resKind = pick(r, RES);
  const q = { dest, res: null, n: 0, target: null, boss: null, where: null };
  const steps = []; let gor = 0, title = '', expires = 20 + diff * 10, spawnMeta = null;

  if (k === 'deliver') {
    const it = resourceItem(resKind, cur); q.n = ri(r, 2, 3) * diff + 1; q.res = it.name;
    steps.push({ type: 'deliver', planetId: dest, item: { stackKey: it.stackKey, n: q.n, name: it.name } });
    gor = Math.round(q.n * it.price * 1.6 + 40 * diff); title = `Deliver ${q.n} ${it.name} to ${dest}`;
  } else if (k === 'bounty') {
    const bw = wave + diff - 1; q.boss = (ctx.bossNameFn || fallbackBoss)(bw);
    steps.push({ type: 'kill', bossName: q.boss }); gor = 250 * diff + wave * 20; title = `Bounty: ${q.boss}`; expires = 45 + diff * 15;
  } else if (k === 'scan') {
    q.n = 2 + diff; q.target = pick(r, SCAN_TARGETS);
    steps.push({ type: 'scan', planetId: dest, target: q.target, count: q.n });
    gor = 60 * q.n + 30 * diff; title = `Scan ${q.n} ${q.target}s on ${dest}`;
  } else if (k === 'retrieve') {
    const belt = r() < 0.5; q.where = belt ? 'in the asteroid belt' : `on ${dest}`;
    const pos = belt ? [r() * Math.PI * 2, 0.8 + r() * 0.4, (r() - 0.5) * 0.3] : (() => { const z = r() * 2 - 1, a = r() * Math.PI * 2, s = Math.sqrt(1 - z * z); return [s * Math.cos(a), z, s * Math.sin(a)]; })();
    const id = 'q' + sd.toString(36);
    steps.push({ type: 'retrieve', planetId: belt ? null : dest, questId: id });
    spawnMeta = { where: belt ? 'belt' : 'planet', planetId: belt ? null : dest, seed: sd, pos };
    gor = 120 * diff + 50; title = belt ? 'Retrieve a lost crate (belt)' : `Retrieve a lost crate on ${dest}`;
  } else if (k === 'escort') {
    steps.push({ type: 'escort', seconds: 60, have: 0 }); gor = 140 * diff + 40; title = 'Escort a freighter (60 s)';
    const route = []; for (let i = 0; i < 4; i++) route.push([Math.round((r() - 0.5) * 4000), Math.round((r() - 0.5) * 600), Math.round((r() - 0.5) * 4000)]);
    spawnMeta = { seconds: 60, route, speed: 40 + diff * 8 };
  } else {
    const it = resourceItem(resKind, pick(r, planets)); q.n = ri(r, 3, 5) * diff; q.res = RES.length && resKind;
    steps.push({ type: 'harvest', item: { stackKey: 'res:' + resKind, n: q.n, name: resKind }, kindOnly: true });
    gor = Math.round(q.n * it.price * 1.4 + 30 * diff); title = `Harvest ${q.n} ${resKind}`;
  }
  steps.forEach((s) => { s.done = false; s.have = s.have || 0; });
  gor = Math.round(gor / 5) * 5;
  const items = [];
  if (diff >= 2 || r() < 0.4) { const it = generateItem(sd ^ 0xBEEF, {}); items.push({ stackKey: stackKeyOf(it), n: 1, item: it }); }
  if (diff >= 3) { const rr = resourceItem(pick(r, RES), pick(r, planets)); items.push({ stackKey: rr.stackKey, n: ri(r, 2, 4), item: rr }); }
  const words = ri(r, 1, diff + 1);
  const npc = { seed: sd, role: giverKind === 'dealer' ? 'dealer' : giverKind === 'clerk' ? 'clerk' : 'drifter', name: lingoName(sd) };
  let hello = ''; try { hello = lingoLine(npc, { ctx: GREET[giverKind] || 'greeting' }); } catch (e) { hello = ''; }
  const giverLine = (hello ? hello + ' ' : '') + PITCH[k](q) + ` Pay: ${fmt(gor)}.`;
  const quest = { id: 'q-' + k + '-' + sd.toString(36), kind: k, title, giverLine, steps, reward: { gor, items, words }, expiresMin: expires, difficulty: diff, giverKind, planetId: cur, state: 'open', giver: npc.name };
  if (spawnMeta) quest.spawn = spawnMeta;
  if (k === 'retrieve') quest.steps[0].questId = quest.id;
  return quest;
}

export function offerSet(seed, ctx, n = 3) {
  const r = mulberry(num(seed) ^ 0xA11CE), kinds = KINDS.slice();
  for (let i = kinds.length - 1; i > 0; i--) { const j = Math.floor(r() * (i + 1)); [kinds[i], kinds[j]] = [kinds[j], kinds[i]]; }
  const out = [];
  for (let i = 0; i < Math.min(n, kinds.length); i++) out.push(generateQuest((num(seed) + Math.imul(i + 1, 0x9E3779B1)) >>> 0, ctx, kinds[i]));
  return out;
}

const NOOP = (quest) => ({ quest, completed: false, stepDone: false, message: '' });
export function progress(quest, ev) {
  if (!quest || quest.state === 'complete' || !ev) return NOOP(quest);
  const q = clone(quest), s = q.steps.find((x) => !x.done);
  if (!s) return NOOP(quest);
  let hit = false, msg = '';
  if (ev.type === 'kill' && s.type === 'kill' && ev.bossName === s.bossName) { s.have = 1; s.done = true; hit = true; msg = `${s.bossName} is down.`; }
  else if (ev.type === 'harvest' && s.type === 'harvest' && (ev.stackKey === s.item.stackKey || (s.kindOnly && String(ev.stackKey).indexOf('res-' + s.item.name + '-') === 0) || (s.kindOnly && ev.kind === s.item.name))) {
    s.have = Math.min(s.item.n, s.have + (ev.n | 0)); hit = true; s.done = s.have >= s.item.n; msg = `${s.item.name} ${s.have}/${s.item.n}`;
  } else if (ev.type === 'deliver' && s.type === 'deliver' && ev.planetId === s.planetId && ev.stackKey === s.item.stackKey) {
    s.have = Math.min(s.item.n, s.have + (ev.n | 0)); hit = true; s.done = s.have >= s.item.n; msg = `Delivered ${s.have}/${s.item.n}`;
  } else if (ev.type === 'scan' && s.type === 'scan' && ev.planetId === s.planetId && (ev.target === s.target || s.target === 'creature' && ev.target === 'plant' && false)) {
    s.have = Math.min(s.count, s.have + 1); hit = true; s.done = s.have >= s.count; msg = `Scanned ${s.have}/${s.count}`;
  } else if (ev.type === 'retrieve' && s.type === 'retrieve' && ev.questId === s.questId) { s.have = 1; s.done = true; hit = true; msg = 'Crate secured.'; }
  else if (ev.type === 'escort' && s.type === 'escort') { s.have = Math.min(s.seconds, s.have + Math.max(0, +ev.seconds || 0)); hit = true; s.done = s.have >= s.seconds; msg = `Escort ${Math.floor(s.have)}/${s.seconds}s`; }
  if (!hit) return NOOP(quest);
  const completed = q.steps.every((x) => x.done);
  if (completed) { q.state = 'complete'; msg = `${q.title} complete. +${fmt(q.reward.gor)}`; }
  return { quest: q, completed, stepDone: !!s.done, message: msg };
}

function stepText(s) {
  if (s.type === 'deliver') return `Deliver ${s.item.name} to ${s.planetId} 7/11 (${s.have}/${s.item.n})`;
  if (s.type === 'kill') return `Kill ${s.bossName}`;
  if (s.type === 'scan') return `Scan ${s.target}s on ${s.planetId} (${s.have}/${s.count})`;
  if (s.type === 'retrieve') return s.planetId ? `Find the crate on ${s.planetId}` : 'Find the crate in the belt';
  if (s.type === 'escort') return `Stay beside the freighter (${Math.floor(s.have)}/${s.seconds}s)`;
  return `Bring ${s.item.name} (${s.have}/${s.item.n})`;
}
export function describe(quest) {
  const s = quest.steps.find((x) => !x.done) || quest.steps[quest.steps.length - 1];
  return quest.title.toUpperCase() + '\n' + (quest.state === 'complete' ? 'Done. Collect ' + fmt(quest.reward.gor) : stepText(s));
}

export function spawnSpecFor(quest) {
  if (!quest || quest.state === 'complete' || !quest.spawn) return null;
  const sp = quest.spawn;
  if (quest.kind === 'retrieve') return { type: 'crate', questId: quest.id, where: sp.where, planetId: sp.planetId, seed: sp.seed, pos: sp.pos.slice() };
  if (quest.kind === 'escort') return { type: 'freighter', questId: quest.id, seconds: sp.seconds, route: sp.route.map((p) => p.slice()), speed: sp.speed };
  return null;
}

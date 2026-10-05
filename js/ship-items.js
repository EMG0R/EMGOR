// ship-items.js — comedic consumables for NO MANS GOR (docs/ship-mode.md rev 23 B).
// Every item is a seeded recombination: [modifier] + [infusion]-infused + [7/11 food]. Names only, all comedy.
//   generateItem(seed)         -> item (deterministic: same seed -> same item)
//   storeMenu(storeId, n=24)   -> [item] sorted by price (the 7/11 shelf list)
//   dealerMenu(dealerId, n=6)  -> [item] street stock: stronger, longer, pricier (item.dealer = true)
//   describe(item)             -> multi-line string (name, blurb, effect list, duration, price)
//   FRIES                      -> Burger House fries: glow light blue for 45 REAL minutes, no vision effect
//   ITEM_KINDS                 -> ['food','drink','snack','fries']
//   FOODS / INFUSIONS / MODIFIERS / BLURBS exported for counts + tests.
// item = { id, seed, name, kind, base, infusion, modifier, price, blurb, color (hex), absurdity,
//          effect: { duration (s of play time), params {blur, chroma, hue, wobble, double, contrast, invert, tint, fov, timeScale} (seeded subset),
//                    tintColor (hex, when tint is set), extras { speed, jump, chatWobble } } }
// Vision params feed engine.vision.set(): all 0..1 except hue (turns) and timeScale (0.85..1.15).
export const ITEM_KINDS = ['food', 'drink', 'snack', 'fries'];

function hashStr(s) { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
function mulberry(a) { a |= 0; return function () { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
const seedOf = (s) => (typeof s === 'number' ? (s >>> 0) : hashStr(s));

// [name, kind, colour]
export const FOODS = [
  ['Gardetto\'s Snack Mix', 'snack', 0xE8B04A], ['Big Gulp', 'drink', 0xFF5C7A], ['Taquito', 'food', 0xD9A05B], ['Slurpee', 'drink', 0x5CE8FF],
  ['Hot Dog', 'food', 0xE0603A], ['Pizza Slice', 'food', 0xF2B33D], ['Glazed Donut', 'snack', 0xFF9ACD], ['Energy Drink', 'drink', 0x7CFF3A],
  ['Nachos', 'food', 0xFFC837], ['Beef Jerky', 'snack', 0x8B4A2B], ['Corn Dog', 'food', 0xE5A24A], ['Monster Can', 'drink', 0x6CFF5C],
  ['Flaming Chips', 'snack', 0xFF4A2A], ['Egg Salad Sandwich', 'food', 0xF4E9A8], ['Gas Station Sushi', 'food', 0xFF8A7A], ['Roller Grill Burrito', 'food', 0xC98B4A],
  ['Cheese Puffs', 'snack', 0xFF9A1F], ['Pickle on a Stick', 'snack', 0x6BBF3A], ['Banana (Single)', 'snack', 0xFFE24A], ['Protein Bar', 'snack', 0xA8754A],
  ['Cold Brew', 'drink', 0x5A3A2A], ['Iced Tea Jug', 'drink', 0xD98A3A], ['Blue Raspberry Icee', 'drink', 0x3A8AFF], ['Chili Cheese Fries', 'food', 0xE0A030],
  ['Powdered Donut', 'snack', 0xF4F0F8], ['Gummy Worms', 'snack', 0xFF5CE1], ['Sour Straws', 'snack', 0xFF4A8A], ['Trail Mix', 'snack', 0x9A6A3A],
  ['Hard Boiled Egg', 'snack', 0xF8F4E8], ['Microwave Mac Bowl', 'food', 0xFFC84A], ['Quesadilla', 'food', 0xE8B86A], ['Breakfast Croissant', 'food', 0xE0A850],
  ['Fruit Cup', 'snack', 0xFF8A5C], ['Orange Soda', 'drink', 0xFF8A1F], ['Grape Soda', 'drink', 0x8A4AFF], ['Root Beer Float', 'drink', 0x6A3A2A],
  ['Cotton Candy', 'snack', 0xFFB0E8], ['Jalapeno Poppers', 'food', 0x7CBF3A], ['Cinnamon Roll', 'food', 0xD98A5A], ['Pretzel Bites', 'snack', 0xC07A3A],
  ['Coconut Water', 'drink', 0xEAF4E0], ['Frozen Burrito', 'food', 0xB8A07A], ['Lemon Ice', 'snack', 0xF8F05C], ['Popcorn Tub', 'snack', 0xFFF0B0],
  ['Chicken Wings (6)', 'food', 0xD9602A], ['Soft Serve Cone', 'snack', 0xFFF4F8], ['Sparkling Water', 'drink', 0xBEEFFF], ['Mystery Casserole Cup', 'food', 0xA89A6A],
];
// [name, family, tier 1..5].  families: psy (colour/warp), stim (speed/jitter), down (slow/blur), dis (double/wobble), weird (anything), cozy (warm/tint)
export const INFUSIONS = [
  ['crack', 'stim', 4], ['fent', 'down', 5], ['ketamine', 'dis', 4], ['shroom', 'psy', 3], ['DMT', 'psy', 5], ['adderall', 'stim', 2], ['lean', 'down', 3],
  ['molly', 'psy', 3], ['acid', 'psy', 4], ['speed', 'stim', 3], ['ayahuasca', 'psy', 4], ['kratom', 'down', 2], ['kava', 'cozy', 1], ['nitrous', 'dis', 2],
  ['salvia', 'dis', 4], ['codeine', 'down', 3], ['caffeine', 'stim', 1], ['nicotine', 'stim', 2], ['melatonin', 'cozy', 1], ['catnip', 'cozy', 1],
  ['mescaline', 'psy', 4], ['peyote', 'psy', 3], ['ritalin', 'stim', 2], ['xanax', 'down', 3], ['valium', 'down', 2], ['ambien', 'down', 2],
  ['LSD', 'psy', 4], ['PCP', 'dis', 5], ['bath salt', 'weird', 5], ['sizzurp', 'down', 3], ['hash', 'cozy', 2], ['edible', 'cozy', 2],
  ['delta-8', 'cozy', 2], ['absinthe', 'psy', 3], ['mead', 'cozy', 1], ['moonshine', 'dis', 3], ['nutmeg', 'weird', 1], ['sugar rush', 'stim', 1],
  ['taurine', 'stim', 1], ['guarana', 'stim', 1], ['ginseng', 'cozy', 1], ['ephedra', 'stim', 3], ['modafinil', 'stim', 2], ['gabapentin', 'down', 2],
  ['Zorbitol', 'weird', 2], ['Glorpamine', 'weird', 3], ['Nebulex', 'psy', 2], ['Quasarine', 'stim', 3], ['Voidgrass', 'dis', 3], ['Plasmadrine', 'stim', 4],
  ['Blorphine', 'down', 4], ['Lunarium', 'cozy', 2], ['Cometamine', 'psy', 3], ['Fluxatrol', 'weird', 3], ['Grav-Dust', 'dis', 4], ['Photonic Pep', 'stim', 2],
  ['Starlean', 'down', 3], ['Dream Goo', 'psy', 3], ['Hyperdrive Honey', 'stim', 3], ['Mirror Mushroom', 'dis', 3], ['Wormhole Wine', 'psy', 4], ['Pulsar Pollen', 'cozy', 2],
  ['Ganymede Gas', 'dis', 3], ['Antimatter Tea', 'weird', 5], ['Cosmic Cough Syrup', 'down', 3], ['Time Salts', 'weird', 4],
];
// [name, ampMul, durMul, tier]
export const MODIFIERS = [
  ['Double', 1.4, 1.2, 1], ['Ultra', 1.6, 1.0, 2], ['Limited Edition', 1.1, 1.0, 1], ['Expired', 0.8, 0.7, 2], ['Blessed', 1.0, 1.3, 2], ['Cursed', 1.3, 1.0, 3],
  ['Gas Station', 1.0, 1.0, 0], ['Artisanal', 0.9, 1.1, 1], ['Extra Hot', 1.2, 0.9, 1], ['Diet', 0.55, 0.8, 0], ['Mega', 1.5, 1.4, 2], ['Family Size', 1.0, 2.0, 1],
  ['Haunted', 1.2, 1.1, 3], ['Organic', 0.8, 1.2, 0], ['Gluten-Free', 0.9, 1.0, 0], ['Overnight', 1.0, 1.7, 1], ['Discount', 0.7, 0.8, 0], ['Premium', 1.2, 1.2, 1],
  ['Imported', 1.1, 1.0, 1], ['Deluxe', 1.3, 1.3, 2], ['Forbidden', 1.7, 1.2, 3], ['Legally Distinct', 1.0, 1.0, 0], ['Heroic', 1.3, 1.0, 2], ['Unlicensed', 1.25, 0.9, 2],
  ['Rotating', 1.0, 0.9, 1], ['Whispered', 1.1, 1.1, 2],
];
const FEEL = ['like a warm blanket made of static', 'like a Tuesday that forgot to end', 'like regret, but crunchy', 'like the inside of a lava lamp', 'like homework you finished early',
  'like a hug from a vending machine', 'faintly of pennies', 'like a rumor about a comet', 'like the colour purple, somehow', 'like grandma\'s attic on fire (nicely)', 'like the last slice at a party', 'like a sneeze in zero gravity'];
const QUIRK = ['The wrapper is warm and hums.', 'It has been looking at you since you walked in.', 'The expiry date is a mood.', 'Three out of four clerks refuse to touch it.', 'Comes with a free opinion.',
  'The label is written in crayon.', 'Something inside is still moving, politely.', 'It glows when nobody is watching.', 'Previous owner unknown, previous owner\'s ghost known.', 'Approved by a committee of pigeons.',
  'The barcode scans as a haiku.', 'Shaking it makes a sound like applause.'];
const WARN = ['Do not operate a ship. Or a spoon.', 'Side effects include sudden opinions about jazz.', 'May cause the floor to feel optional.', 'Consult a clerk, a priest, or a raccoon.',
  'Not evaluated by anyone qualified.', 'Eat in a calm room. There are no calm rooms.', 'Results may vary; so may colours.', 'Keep away from open flames and honest mirrors.',
  'Best consumed while sitting down, already.', 'Sold as is. Whatever "is" means today.'];
export const BLURBS = [
  '{mod} {food}. {inf}-infused. Tastes {feel}.', 'A {food} with a secret: {inf}. {quirk}', '{quirk} Tastes {feel}. {warn}', 'They said {inf} was optional. They lied about the {food} too.',
  'One bite and the {food} starts having feelings. {warn}', '{inf} in a {food}. Somebody had to. Tastes {feel}.', 'Fresh-ish {food}, {inf} to taste. {quirk}', 'Tastes {feel}. {quirk}',
  'The {food} that asks questions. Powered by {inf}. {warn}', '{mod} edition. The {inf} is load-bearing.', 'Contains {inf}, a {food}, and a promise. {warn}', 'Hot-held {food} laced with {inf}. {quirk}',
  'You do not eat this {food}. It eats the afternoon. {inf} inside.', 'A {food} for people who got bored of Tuesdays. {inf}-forward.', '{inf} makes it fizz. The {food} makes it legal. Mostly.',
  'Chef\'s note: more {inf}, less questions. Tastes {feel}.', 'Lovingly microwaved with {inf}. {warn}', 'This {food} has been through things. Now with {inf}.', 'Clerk\'s favourite. Clerk is not well. {inf}.',
  '{food}, but make it {inf}. {quirk}', 'Marketing says "zesty". Medicine says "no". {inf} {food}.', 'An honest {food} with a dishonest amount of {inf}. {warn}', 'Cosmic {food}. Cosmic {inf}. Cosmic regret, sold separately.',
  'Tastes {feel}. Leaves a glow. {warn}', '{quirk} Also {inf}. Also {food}.', 'The {inf} is mostly vibes. The {food} is mostly legal.', 'Eat it fast, before it learns your name. {inf} {food}.',
  'Seven out of eleven stars. {inf}-infused {food}. {quirk}', 'Limited stock, unlimited {inf}. {warn}', 'It is a {food}. It is also a {inf} situation.', 'Drink-eat-snack hybrid, {inf} grade. Tastes {feel}.',
  'Bought by pilots, eaten by pilots, regretted by wingmen. {inf} {food}.',
];
// which vision keys each infusion family prefers (extra keys are added at random for high absurdity)
const FAMILY_KEYS = {
  psy: ['hue', 'chroma', 'wobble', 'tint', 'contrast'], stim: ['timeScale', 'fov', 'chroma', 'contrast', 'double'], down: ['blur', 'double', 'contrast', 'timeScale', 'tint'],
  dis: ['double', 'wobble', 'blur', 'chroma', 'fov'], weird: ['invert', 'hue', 'wobble', 'fov', 'double', 'timeScale'], cozy: ['tint', 'blur', 'hue', 'contrast'],
};
const ALL_KEYS = ['blur', 'chroma', 'hue', 'wobble', 'double', 'contrast', 'invert', 'tint', 'fov', 'timeScale'];
const TINTS = [0xFF7A3A, 0x7A3AFF, 0x3AFFB0, 0xFF3AA0, 0x3AA0FF, 0xFFE03A];

function pickOf(r, a) { return a[Math.floor(r() * a.length)]; }
function fill(tpl, v) { return tpl.replace(/\{(\w+)\}/g, (m, k) => (v[k] != null ? v[k] : m)); }
function shade(hex, f) { const r = Math.min(255, ((hex >> 16) & 255) * f), g = Math.min(255, ((hex >> 8) & 255) * f), b = Math.min(255, (hex & 255) * f); return (Math.round(r) << 16) | (Math.round(g) << 8) | Math.round(b); }

export function generateItem(seed, opts) {
  opts = opts || {};
  const s = seedOf(seed), r = mulberry(s ^ 0x5bd1e995);
  const food = pickOf(r, FOODS), inf = pickOf(r, INFUSIONS);
  const hasMod = r() < 0.72, mod = hasMod ? pickOf(r, MODIFIERS) : null;
  const dealer = !!opts.dealer;
  const ampMul = (mod ? mod[1] : 1) * (dealer ? 1.25 : 1), durMul = (mod ? mod[2] : 1) * (dealer ? 1.4 : 1);
  // effect params: a seeded subset, biased by the infusion family
  const nKeys = 2 + Math.floor(r() * 2) + (inf[2] >= 4 ? 1 : 0) + (dealer ? 1 : 0);
  const pool = FAMILY_KEYS[inf[1]].slice(), keys = [];
  while (keys.length < nKeys) {
    const src = (pool.length && r() < 0.8) ? pool : ALL_KEYS;
    const k = src[Math.floor(r() * src.length)];
    if (keys.indexOf(k) < 0) keys.push(k);
    if (keys.length >= ALL_KEYS.length) break;
  }
  const params = {};
  const A = () => Math.min(1, (0.25 + r() * 0.55) * ampMul);
  for (const k of keys) {
    if (k === 'hue') params.hue = Math.round((0.12 + r() * 0.5) * (r() < 0.5 ? -1 : 1) * Math.min(1.6, ampMul) * 100) / 100;
    else if (k === 'timeScale') params.timeScale = Math.round((inf[1] === 'down' || (inf[1] !== 'stim' && r() < 0.5) ? 0.85 + r() * 0.08 : 1.07 + r() * 0.08) * 100) / 100;
    else if (k === 'invert') params.invert = Math.round(Math.min(0.6, (0.15 + r() * 0.35) * ampMul) * 100) / 100;
    else params[k] = Math.round(A() * 100) / 100;
  }
  const effect = { duration: 0, params, extras: {} };
  if (params.tint != null) effect.tintColor = pickOf(r, TINTS);
  effect.duration = Math.round(Math.max(15, Math.min(240, (30 + r() * 140) * durMul)));
  const ex = effect.extras;
  const fam = inf[1];
  ex.speed = Math.round((fam === 'stim' ? 1.15 + r() * 0.5 : fam === 'down' ? 0.65 + r() * 0.25 : 0.85 + r() * 0.4) * 100) / 100;
  ex.jump = Math.round((fam === 'stim' ? 1.2 + r() * 1.0 : fam === 'down' ? 0.6 + r() * 0.3 : fam === 'weird' ? 0.5 + r() * 2.0 : 0.85 + r() * 0.5) * 100) / 100;
  ex.chatWobble = Math.round(Math.min(1, (fam === 'psy' || fam === 'dis' ? 0.3 + r() * 0.6 : r() * 0.45) * Math.min(1.3, ampMul)) * 100) / 100;
  const absurdity = inf[2] + (mod ? mod[3] : 0) + keys.length * 0.7 + (dealer ? 1.5 : 0);
  let price = Math.round(6 + absurdity * 9 + effect.duration / 12 + r() * 10);
  if (dealer) price = Math.round(price * 2.2);
  price = Math.max(8, Math.min(dealer ? 900 : 480, price));
  const nm = (mod ? mod[0] + ' ' : '') + inf[0].charAt(0).toUpperCase() + inf[0].slice(1) + '-Infused ' + food[0];
  const blurb = fill(pickOf(r, BLURBS), { mod: mod ? mod[0] : 'Plain', food: food[0], inf: inf[0], feel: pickOf(r, FEEL), quirk: pickOf(r, QUIRK), warn: pickOf(r, WARN) });
  const famTint = { psy: 0xFF5CE1, stim: 0xFFE24A, down: 0x5C7AFF, dis: 0x5CE8FF, weird: 0x7CFF8A, cozy: 0xFFA05C }[fam];
  const color = shade(food[2], 0.75) | 0; const mixed = ((((food[2] >> 16) & 255) + ((famTint >> 16) & 255)) >> 1 << 16) | ((((food[2] >> 8) & 255) + ((famTint >> 8) & 255)) >> 1 << 8) | (((food[2] & 255) + (famTint & 255)) >> 1);
  return {
    id: 'it' + s.toString(16).padStart(8, '0'), seed: s, name: nm, kind: food[1], base: food[0], infusion: inf[0], family: fam, modifier: mod ? mod[0] : null,
    price, blurb, color: mixed || color, absurdity: Math.round(absurdity * 10) / 10, dealer, effect,
  };
}

export function storeMenu(storeId, n) {
  n = n || 24; const base = hashStr('store:' + storeId), out = [], names = new Set();
  for (let i = 0, guard = 0; out.length < n && guard < n * 8; i++, guard++) {
    const it = generateItem((base + Math.imul(i + 1, 0x9E3779B1)) >>> 0);
    if (names.has(it.name)) continue; names.add(it.name); out.push(it);
  }
  return out.sort((a, b) => a.price - b.price);
}
export function dealerMenu(dealerId, n) {
  n = n || 6; const base = hashStr('dealer:' + dealerId), out = [], names = new Set();
  for (let i = 0, guard = 0; out.length < n && guard < n * 12; i++, guard++) {
    const it = generateItem((base + Math.imul(i + 1, 0x85EBCA6B)) >>> 0, { dealer: true });
    if (names.has(it.name) || it.absurdity < 7) continue; names.add(it.name); out.push(it);
  }
  return out.sort((a, b) => a.price - b.price);
}

const SEASON = [
  'Dusted with a seasoning blend older than the station. It has a secret. The secret is salt, and also love.',
  'Crisp edges, soft middle, and that famous red-gold seasoning. Eat them hot, tell no one.',
  'Salted by hand, seasoned by a recipe the clerk will not discuss. Glows. You will too.',
];
export const FRIES = {
  id: 'fries', seed: 1951, name: 'Burger House Fries', kind: 'fries', base: 'Seasoned Fries', infusion: null, family: 'glow', modifier: null,
  price: 12, blurb: SEASON[0], color: 0x7FD8FF, absurdity: 0, dealer: false, blurbs: SEASON,
  // duration is REAL wall-clock seconds: ship.js stores an expiry timestamp on the profile and glows the human + hull light blue until then
  effect: { duration: 45 * 60, realtime: true, params: {}, extras: {}, glow: { color: 0x7FD8FF, minutes: 45 } },
};

const KEY_ORDER = ALL_KEYS;
export function describe(item) {
  if (!item) return '';
  const e = item.effect || { params: {}, extras: {} }, p = e.params || {};
  const mins = e.duration >= 120 ? (Math.round(e.duration / 6) / 10) + ' min' : e.duration + ' s';
  let fx;
  if (item.kind === 'fries') fx = 'Glow: light blue, 45 real minutes';
  else {
    const parts = KEY_ORDER.filter((k) => p[k] != null).map((k) => k + ' ' + p[k]);
    const x = e.extras || {};
    if (x.speed != null && Math.abs(x.speed - 1) > 0.05) parts.push('speed x' + x.speed);
    if (x.jump != null && Math.abs(x.jump - 1) > 0.05) parts.push('jump x' + x.jump);
    if (x.chatWobble > 0.05) parts.push('chat wobble ' + x.chatWobble);
    fx = 'Effects: ' + (parts.join(', ') || 'none') + ' (' + mins + ')';
  }
  return item.name + '\n' + item.blurb + '\n' + fx + '\nPrice: ' + item.price + ' units';
}

// ── machine parts (rev 25 B): sold at 7/11s only (dealers never stock parts).  Crafting (ship-craft.js) reads item.tags. ──
//   partsMenu(storeId, n=6) -> [{ id, kind:'part', base, name, price (gorCoin), color, blurb, tags:['part', base] }]  seeded per store
import { PARTS as CRAFT_PARTS } from './ship-craft.js';
export const PARTS = CRAFT_PARTS;      // ONE parts source: ship-craft.js (price field, base = id)
export function partsMenu(storeId, n) {
  n = Math.max(1, Math.min(PARTS.length, n || 6));
  const r = mulberry(hashStr('parts:' + storeId) ^ 0x70a7), pool = PARTS.slice(), out = [];
  while (out.length < n && pool.length) {
    const p = pool.splice(Math.floor(r() * pool.length), 1)[0];
    out.push({ id: p.id, kind: 'part', base: p.id, name: p.name, price: Math.round(p.price * (0.8 + r() * 0.5)), color: p.color, blurb: p.blurb, tags: ['part', p.id] });
  }
  return out.sort((a, b) => a.price - b.price);
}

// ── resources (rev 25): harvested on planets, crafting tags 'res' + 'res:<kind>' (ship-craft.js tokens).  One stack per planet+kind via stackKey.
//   resourceItem(kind, planetSeed) -> { id, kind:'resource', base: kind, name ("Papers Crystal"), color, price, blurb, tags:['res','res:'+kind], stackKey }
//   planetSeed = planet name/id string (flavours the name) or a number (picks a flavour word).
export const RESOURCE_BASE = {
  crystal: { noun: 'Crystal', color: 0x5CE8FF, price: 40, blurb: 'Grown slowly, priced quickly.' },
  plant: { noun: 'Pod', color: 0x9CFF7A, price: 18, blurb: 'Squishy. Mostly friendly.' },
  ore: { noun: 'Ore', color: 0xFFB030, price: 28, blurb: 'Rock that knows what it is worth.' },
  ice: { noun: 'Ice', color: 0xBEEFFF, price: 22, blurb: 'Cold on purpose.' },
  spore: { noun: 'Spore', color: 0xE080FF, price: 35, blurb: 'Drifts. Do not inhale. Or do.' },
};
const RES_FLAVOR = ['Nebula', 'Comet', 'Void', 'Quasar', 'Plasma', 'Meteor', 'Orbit', 'Lunar'];
export function resourceItem(kind, planetSeed) {
  const b = RESOURCE_BASE[kind] || RESOURCE_BASE.crystal, k = RESOURCE_BASE[kind] ? kind : 'crystal';
  let flavor, h;
  if (typeof planetSeed === 'number') { h = planetSeed >>> 0; flavor = RES_FLAVOR[h % RES_FLAVOR.length]; }
  else {
    const w = String(planetSeed || 'planet').trim().split(/[\s_\-/]+/)[0].replace(/[^A-Za-z0-9]/g, '') || 'Planet';
    flavor = w.charAt(0).toUpperCase() + w.slice(1); h = hashStr(w.toLowerCase());
  }
  const id = 'res-' + k + '-' + flavor.toLowerCase();
  return { id, kind: 'resource', base: k, name: flavor + ' ' + b.noun, color: b.color, price: Math.round(b.price * (0.85 + (h % 31) / 100)), blurb: b.blurb, tags: ['res', 'res:' + k], stackKey: id };
}

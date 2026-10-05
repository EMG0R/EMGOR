// ship-craft.js — gorCoin + 3x3 crafting for NO MANS GOR (docs/ship-mode.md rev 25 A, B). Pure data/logic, no DOM, node-safe.
//   CURRENCY {name:'gorCoin', sym:'ɢ'}   fmt(1240) -> "ɢ 1,240"
//   PARTS            10 parts {id,name,price,blurb,tags:['part',id],color,kind:'part',base:id}
//   PART_ICON_HINTS  part id -> existing ship-icons ICON_TEMPLATES name (iconFor keys by base/infusion/modifier/color; crafted items carry iconHint too)
//   RECIPES          hand-authored list {id,name,shapeless,pattern,out,count,output(ingredientItems)}; tokens: part id ('coil'/'part:coil'), 'part:any',
//                    'food:any' (edible), 'infusion:any', 'infusion:<family>', kind shorthands ('food','drink','snack','fries'), 'base:<slug>', 'out:<recipeId>', 'crafted:any'
//   match(grid)      grid = 9 stacks|null (row-major) -> { recipe, output, count, consumes:[cellIdx...] } (1 of each consumed cell; recipe=null for a
//                    combination result) or null (empty / <2 stacks and no recipe). Shaped patterns match anywhere on the grid and mirrored.
//   combine(stacks)  deterministic, order-independent: tags merge -> category 'weapon-mod'|'ship-upgrade'|'food'|'junk', tier=max+1, blended effect,
//                    generated name ("Flux-Capacitor Fent Burger Mk II"), price, icon fields. Output crafts again forever.
//   applyMod(weapon, mod)        -> { weapon, line }  (3 slots: scope/coil/chamber on weapon.mods; stats rebuilt from weapon.baseStats; pure)
//   applyUpgrade(profile, upg)   -> { profile, line } (profile.upgrades[slot] += level; fuel/pens/pods also counters; pure)
//   sellPrice(item)              -> gorCoin int;  stackKeyOf(item); itemTags(item)
// stack = { id, item, n }.  crafted item = { id, name, kind, base, infusion, modifier, color, price, blurb, tier, category, tags, stackKey, comp, frags,
//   iconHint, mod?:{slot,delta:{dmg,rate,range,speed,spread (fractions), count (add)},special}, upgrade?:{slot,level}, effect? (foods) }
export const CURRENCY = { name: 'gorCoin', sym: 'ɢ' };
export function fmt(n) { return CURRENCY.sym + ' ' + Math.round(n || 0).toLocaleString('en-US'); }

function hashStr(s) { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
const slug = (s) => String(s || '').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
const cap = (s) => s.charAt(0).toUpperCase() + s.slice(1);
const ROMAN = ['', 'I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X'];
const roman = (n) => ROMAN[Math.min(10, Math.max(1, n))] || String(n);
const r2 = (x) => Math.round(x * 100) / 100;

export const PARTS = [
  { id: 'capacitor', name: 'Flux Capacitor', price: 90, color: 0x5CE8FF, blurb: 'Stores a feeling for later. Hums at 88 Hz.' },
  { id: 'servo', name: 'Servo Unit', price: 45, color: 0xFFB05C, blurb: 'Turns small decisions into small motions.' },
  { id: 'coil', name: 'Plasma Coil', price: 70, color: 0xFF5CE1, blurb: 'Copper wound by someone who really cared.' },
  { id: 'plate', name: 'Hull Plate', price: 35, color: 0xA8A0C8, blurb: 'Flat, grey, and brave about it.' },
  { id: 'lens', name: 'Focus Lens', price: 55, color: 0xBEEFFF, blurb: 'Makes far things rude and close.' },
  { id: 'battery', name: 'Cell Battery', price: 40, color: 0x7CFF3A, blurb: 'Charged, mostly. Do not lick.' },
  { id: 'gyro', name: 'Gyro Stabilizer', price: 65, color: 0xFFE24A, blurb: 'Insists on knowing which way is up.' },
  { id: 'antenna', name: 'Signal Antenna', price: 30, color: 0x8A6CFF, blurb: 'Picks up stations, rumors, and the occasional ghost.' },
  { id: 'fin', name: 'Cooling Fin', price: 25, color: 0x3AA0FF, blurb: 'Keeps your bad ideas at a safe temperature.' },
  { id: 'chip', name: 'Logic Chip', price: 80, color: 0x3AFFB0, blurb: 'Smarter than the clerk. Cheaper too.' },
].map((p) => Object.assign(p, { tags: ['part', p.id], kind: 'part', base: p.id }));
export const PART_ICON_HINTS = { capacitor: 'can', servo: 'donut', coil: 'stick', plate: 'tray', lens: 'donut', battery: 'energy', gyro: 'donut', antenna: 'stick', fin: 'bar', chip: 'candy' };
const PART_BY_ID = Object.fromEntries(PARTS.map((p) => [p.id, p]));
const WEAPON_PARTS = ['lens', 'coil', 'capacitor', 'antenna', 'chip'], SHIP_PARTS = ['plate', 'battery', 'servo', 'gyro', 'fin'];

// ── tags / comp ──
export function stackKeyOf(item) { return item.stackKey || item.id || slug(item.name); }
export function itemTags(item) {
  const t = new Set(item.tags || []);
  if (item.kind === 'part' || (item.tags && item.tags.indexOf('part') >= 0)) { t.add('part'); if (item.base && PART_BY_ID[item.base]) t.add('part:' + item.base); }
  if (item.kind === 'food' || item.kind === 'drink' || item.kind === 'snack' || item.kind === 'fries' || item.category === 'food') { t.add('edible'); }
  if (item.kind) t.add('kind:' + item.kind);
  if (item.infusion) { t.add('infusion'); if (item.family) t.add('infusion:' + item.family); }
  if (item.base && !PART_BY_ID[item.base]) t.add('base:' + slug(item.base));
  if (item.category) { t.add('crafted'); t.add('cat:' + item.category); }
  if (item.recipe) t.add('out:' + item.recipe);
  if (item.tier) t.add('tier:' + item.tier);
  return t;
}
function tokMatch(tok, tags) {
  if (tok === 'food:any') return tags.has('edible');
  if (tok === 'infusion:any') return tags.has('infusion');
  if (tok === 'part:any') return tags.has('part');
  if (tok === 'crafted:any') return tags.has('crafted');
  return tags.has(tok) || tags.has('part:' + tok) || tags.has('kind:' + tok);
}
function compOf(item) {
  if (item.comp) return item.comp;
  const c = { parts: {}, edible: 0, infusion: 0, junk: 0 }, t = itemTags(item);
  if (t.has('part') && item.base && PART_BY_ID[item.base]) c.parts[item.base] = 1;
  if (t.has('edible')) c.edible = 1;
  if (t.has('infusion')) c.infusion = 1;
  if (item.category === 'junk') c.junk = 1;
  return c;
}
function mergeComp(items) {
  const c = { parts: {}, edible: 0, infusion: 0, junk: 0 };
  for (const it of items) { const x = compOf(it); for (const k in x.parts) c.parts[k] = (c.parts[k] || 0) + x.parts[k]; c.edible += x.edible; c.infusion += x.infusion; c.junk += x.junk; }
  return c;
}
const tierOf = (it) => (it.tier || 1);

// ── recipes ──
const E = (duration, params, extras) => ({ duration, params: params || {}, extras: extras || {} });
const HINT = { 'weapon-mod': 'pill', 'ship-upgrade': 'tray', food: 'can', junk: 'bar', material: 'tray' };
let RID = 0;
function R(id, name, pattern, out) {
  const shapeless = pattern.charAt(0) === '~';
  const rows = shapeless ? null : pattern.split('|').map((r) => r.trim().split(/\s+/).map((t) => (t === '.' ? null : t)));
  const list = shapeless ? pattern.slice(1).trim().split(/\s+/) : null;
  RID++;
  return { id, name, shapeless, pattern, rows, list, out, count: out.count || 1, tier: out.tier || 2 };
}
const MOD = (slot, delta, o) => Object.assign({ cat: 'weapon-mod', slot, delta, color: 0xFF8AE8, price: 160 }, o || {});
const UPG = (slot, level, o) => Object.assign({ cat: 'ship-upgrade', slot, level, color: 0x9AA8FF, price: 180 }, o || {});
const MAT = (base, color, price, o) => Object.assign({ cat: 'material', base, color, price }, o || {});
const FOOD = (base, color, price, eff, o) => Object.assign({ cat: 'food', base, color, price, effect: eff, kind: 'food' }, o || {});

export const RECIPES = [
  // materials
  R('seasoning-salt', 'Seasoning Salt', '~fries fries', MAT('Seasoning Salt', 0xF2EEE6, 18, { count: 3, hint: 'bag', blurb: 'The red-gold dust itself. Do not tell the clerk.' })),
  R('hull-plate', 'Reinforced Hull Plate', 'plate plate|plate plate', MAT('Reinforced Hull Plate', 0xC8C0E8, 150, { blurb: 'Four plates agreeing with each other.' })),
  R('bolt-core', 'Bolt Core', 'coil capacitor coil', MAT('Bolt Core', 0xFF8AE8, 260, { blurb: 'A tiny lightning argument in a can.' })),
  R('infusion-dust', 'Infusion Dust', '~infusion:any infusion:any', MAT('Infusion Dust', 0xC8A0FF, 70, { hint: 'candy', blurb: 'Two infusions, pressed flat. Smells like Tuesday.' })),
  R('fuel-cell', 'Fuel Cell', 'battery|capacitor|battery', UPG('fuel', 1, { base: 'Fuel Cell', color: 0x7CFF3A, price: 210, hint: 'energy', blurb: 'Tops up the tank for a long haul.' })),
  R('fuel-cell-big', 'Deep-Space Fuel Cell', '~out:fuel-cell out:fuel-cell out:fuel-cell', UPG('fuel', 3, { base: 'Deep Fuel Cell', color: 0xB8FF3A, price: 700, tier: 3, hint: 'energy' })),
  R('dream-cell', 'Dream Cell', '~infusion:any battery', UPG('fuel', 2, { base: 'Dream Cell', color: 0xFF5CE1, price: 320, hint: 'energy', blurb: 'Runs on vibes and one battery.' })),
  // weapon mods: scope
  R('scope-mk', 'Scope', 'lens|antenna', MOD('scope', { range: 0.25, spread: -0.15 }, { base: 'Scope' })),
  R('scope-sniper', 'Sniper Scope', 'lens lens plate', MOD('scope', { range: 0.5, speed: 0.2 }, { base: 'Sniper Scope', price: 280 })),
  R('scope-gyro', 'Gyro Scope', 'lens|gyro|lens', MOD('scope', { spread: -0.4, range: 0.15 }, { base: 'Gyro Scope', price: 260 })),
  R('scope-seeker', 'Seeker Scope', '~lens antenna chip', MOD('scope', { range: 0.2 }, { special: 'homingLite', base: 'Seeker Scope', price: 330 })),
  R('scope-bolt', 'Bolt Scope', '~lens bolt-core', MOD('scope', { range: 0.3, dmg: 0.2 }, { base: 'Bolt Scope', price: 400 })),
  R('scope-fin', 'Cool Scope', 'lens fin', MOD('scope', { range: 0.15, rate: 0.1 }, { base: 'Cool Scope', price: 140 })),
  // coil mods
  R('coil-mod', 'Coil Mod', 'coil|capacitor', MOD('coil', { dmg: 0.2 }, { base: 'Coil Mod' })),
  R('coil-twin', 'Twin Coil', 'coil coil|capacitor capacitor', MOD('coil', { dmg: 0.4, rate: 0.1 }, { base: 'Twin Coil', price: 360 })),
  R('coil-burn', 'Ember Coil', '~coil coil infusion:any', MOD('coil', { dmg: 0.1 }, { special: 'burn', base: 'Ember Coil', price: 300 })),
  R('coil-chain', 'Arc Coil', '~coil antenna capacitor', MOD('coil', { dmg: 0.1 }, { special: 'chain', base: 'Arc Coil', price: 310 })),
  R('coil-servo', 'Whip Coil', 'servo|coil', MOD('coil', { speed: 0.25, rate: 0.1 }, { base: 'Whip Coil', price: 200 })),
  R('coil-bolt', 'Bolt Coil', 'bolt-core battery', MOD('coil', { dmg: 0.5 }, { base: 'Bolt Coil', price: 480, tier: 3 })),
  // chamber mods
  R('chamber-mod', 'Chamber Mod', 'plate chip|plate battery', MOD('chamber', { rate: 0.2 }, { base: 'Chamber Mod' })),
  R('chamber-pierce', 'Piercing Chamber', '~chip plate plate coil', MOD('chamber', { dmg: 0.1 }, { special: 'pierce', base: 'Piercing Chamber', price: 320 })),
  R('chamber-split', 'Splitter Chamber', '~chip chip lens', MOD('chamber', { spread: 0.1 }, { special: 'splitOnHit', base: 'Splitter Chamber', price: 330 })),
  R('chamber-over', 'Overcharge Chamber', 'battery capacitor battery', MOD('chamber', { dmg: 0.15 }, { special: 'overcharge', base: 'Overcharge Chamber', price: 340 })),
  R('chamber-count', 'Dual Chamber', 'chip servo chip', MOD('chamber', { count: 1 }, { base: 'Dual Chamber', price: 380 })),
  R('chamber-cool', 'Cooled Chamber', 'fin chip fin', MOD('chamber', { rate: 0.3 }, { base: 'Cooled Chamber', price: 260 })),
  // ship upgrades
  R('engine-kit', 'Engine Kit', 'servo servo|coil fin', UPG('engine', 1, { base: 'Engine Kit', color: 0xFFB05C })),
  R('engine-turbo', 'Turbo Engine Kit', 'servo coil servo|fin fin fin', UPG('engine', 2, { base: 'Turbo Engine Kit', color: 0xFF8A3A, price: 420, tier: 3 })),
  R('engine-gyro', 'Trim Engine Kit', '~servo gyro fin', UPG('engine', 1, { base: 'Trim Engine Kit', color: 0xFFD08A, price: 220 })),
  R('shield-cell', 'Shield Cell', 'battery|plate|battery', UPG('shield', 1, { base: 'Shield Cell', color: 0x5CE8FF })),
  R('shield-bubble', 'Bubble Shield', 'capacitor capacitor|plate plate', UPG('shield', 2, { base: 'Bubble Shield', color: 0x8AF0FF, price: 440, tier: 3 })),
  R('shield-hullplate', 'Plated Shield', '~out:hull-plate battery capacitor', UPG('shield', 2, { base: 'Plated Shield', color: 0xBEEFFF, price: 460, tier: 3 })),
  R('cargo-kit', 'Cargo Kit', 'plate . plate|plate plate plate', UPG('cargo', 1, { base: 'Cargo Kit', color: 0xD9A05B })),
  R('cargo-big', 'Freight Cargo Kit', 'plate plate plate|plate servo plate|plate plate plate', UPG('cargo', 2, { base: 'Freight Cargo Kit', color: 0xE8B040, price: 480, tier: 3 })),
  R('cargo-chip', 'Smart Cargo Kit', '~plate plate chip', UPG('cargo', 1, { base: 'Smart Cargo Kit', color: 0x3AFFB0, price: 240 })),
  R('jetpack-booster', 'Jetpack Booster', 'gyro|battery|fin', UPG('jetpack', 1, { base: 'Jetpack Booster', color: 0xFFE24A })),
  R('jetpack-mk2', 'Twin Jetpack Booster', 'gyro gyro|battery battery|fin fin', UPG('jetpack', 2, { base: 'Twin Jetpack Booster', color: 0xFFF08A, price: 440, tier: 3 })),
  R('pen-kit', 'Creature Pen Kit', 'plate lens plate|plate . plate|plate plate plate', UPG('pens', 1, { base: 'Pen Kit', color: 0xBEEFFF, price: 300, blurb: 'A glass pod with a tiny door. Adds one pen.' })),
  R('crew-pod-kit', 'Crew Pod Kit', 'plate plate|battery fin|plate plate', UPG('pods', 1, { base: 'Crew Pod Kit', color: 0xA8A0C8, price: 300, blurb: 'A bunk with opinions. Adds one crew pod.' })),
  // creature treats + snack combos
  R('creature-treat', 'Creature Treat', '~fries infusion:any', FOOD('Creature Treat', 0xFFB06C, 40, E(60, {}, { treat: 1 }), { count: 2, blurb: 'Creatures will follow you home for this.' })),
  R('creature-treat-salt', 'Salted Creature Treat', '~seasoning-salt jerky', FOOD('Salted Creature Treat', 0xE8C8A8, 55, E(60, {}, { treat: 2 }), { count: 2, blurb: 'Jerky, salted twice. Very good boy.' })),
  R('salted-fries', 'Salted Fries', '~fries out:seasoning-salt', FOOD('Salted Fries', 0xF4C84A, 30, E(120, { chroma: 0.2 }, { speed: 1.1 }), { kind: 'fries', blurb: 'Fries with the salt put back on purpose.' })),
  R('combo-meal', 'Combo Meal', '~food drink snack', FOOD('Combo Meal', 0xFFB84A, 60, E(150, { contrast: 0.2 }, { speed: 1.1, jump: 1.1 }), { blurb: 'The number three, no substitutions.' })),
  R('snack-trio', 'Snack Trio', 'snack snack snack|drink drink drink', FOOD('Party Pack', 0xFF9ACD, 110, E(200, { chroma: 0.3, double: 0.15 }, { chatWobble: 0.3 }), { blurb: 'Six things in a bag. Share. Or do not.' })),
  R('stack-meal', 'Stacked Meal', 'food|food', FOOD('Stacked Meal', 0xD9A05B, 45, E(120, { wobble: 0.2 }, { jump: 1.15 }))),
  R('spicy-mix', 'Spicy Mix', '~infusion:any infusion:any out:seasoning-salt', FOOD('Spicy Mix', 0xFF4A2A, 75, E(140, { hue: 0.2, wobble: 0.3 }, { speed: 1.25 }))),
  R('charged-drink', 'Charged Drink', '~drink battery', FOOD('Charged Drink', 0x7CFF3A, 85, E(100, { blur: 0.1 }, { speed: 1.4, jump: 1.3 }), { blurb: 'Tastes like licking a nine-volt. Do it anyway.' })),
  R('chilled-drink', 'Chilled Drink', '~drink fin', FOOD('Chilled Drink', 0x3AA0FF, 50, E(160, { tint: 0.3, blur: 0.15 }, { speed: 0.9 }), { tintColor: 0x3AA0FF, blurb: 'Cold enough to slow your thoughts. In a good way.' })),
  R('brain-snack', 'Brain Snack', '~snack chip', FOOD('Brain Snack', 0x3AFFB0, 95, E(170, { contrast: 0.35 }, { jump: 1.2 }), { blurb: 'Crunchy and slightly smarter than you.' })),
  R('hot-nacho-dog', 'Nacho Dog', '~base:hot-dog base:nachos', FOOD('Nacho Dog', 0xFFC837, 65, E(130, { chroma: 0.25 }, { speed: 1.2 }))),
  R('signal-snack', 'Signal Snack', '~snack antenna', FOOD('Signal Snack', 0x8A6CFF, 60, E(120, { double: 0.2 }, { chatWobble: 0.4 }), { blurb: 'Picks up the chatter. Crunchy.' })),
  R('scrap-slab', 'Fry-Plated Slab', '~plate fries', { cat: 'junk', base: 'Fry-Plated Slab', color: 0x9AA0AA, price: 40, blurb: 'Technically armor. Technically lunch.' }),
];

function isJunkOut(o) { return o.cat === 'junk'; }
export function buildRecipeOutput(recipe, items) {
  const o = recipe.out, comp = mergeComp(items), tier = Math.max(recipe.tier, items.reduce((m, it) => Math.max(m, tierOf(it)), 1));
  const sc = 1 + 0.25 * (tier - recipe.tier), key = 'cr:' + recipe.id + ':' + tier;
  const color = o.color | 0, cat = o.cat;
  const base = o.base, kind = cat === 'food' ? (o.kind || 'food') : cat === 'junk' ? 'junk' : cat === 'material' ? 'material' : 'part';
  const it = {
    id: key, recipe: recipe.id, name: base + (tier > 2 ? ' Mk ' + roman(tier) : ''), kind, base, infusion: null, modifier: null, color,
    price: Math.round((o.price || 40) * (1 + 0.5 * (tier - recipe.tier))), blurb: o.blurb || (cat === 'weapon-mod' ? 'Bolts on. Slot: ' + o.slot + '.' : cat === 'ship-upgrade' ? 'Ship upgrade: ' + o.slot + '.' : 'Hand-crafted.'),
    tier, category: cat, tags: ['crafted', 'cat:' + cat, 'out:' + recipe.id], stackKey: key, comp, frags: [base.split(' ').slice(-2).join('-')],
    iconHint: o.hint || HINT[cat], absurdity: 0, dealer: false,
  };
  if (cat === 'weapon-mod') { const d = {}; for (const k in o.delta) d[k] = k === 'count' ? o.delta[k] : r2(o.delta[k] * sc); it.mod = { slot: o.slot, delta: d, special: o.special || null }; }
  if (cat === 'ship-upgrade') it.upgrade = { slot: o.slot, level: Math.max(1, Math.round(o.level * sc)) };
  if (cat === 'food') { it.effect = JSON.parse(JSON.stringify(o.effect)); if (o.tintColor) it.effect.tintColor = o.tintColor; it.family = 'cozy'; }
  return it;
}

// ── grid matching ──
function cellsOf(grid) { const out = []; for (let i = 0; i < 9; i++) { const s = grid[i]; if (s && s.item && s.n > 0) out.push({ i, s, tags: itemTags(s.item) }); } return out; }
function matchShaped(rows, cells) {
  const h = rows.length, w = Math.max.apply(null, rows.map((r) => r.length));
  const need = rows.reduce((n, r) => n + r.filter(Boolean).length, 0);
  if (need !== cells.length) return null;
  const at = (x, y) => { for (const c of cells) if (c.i === y * 3 + x) return c; return null; };
  for (const mirror of [false, true]) for (let oy = 0; oy + h <= 3; oy++) for (let ox = 0; ox + w <= 3; ox++) {
    let ok = true; const used = [];
    for (let y = 0; y < h && ok; y++) for (let x = 0; x < w; x++) {
      const tok = rows[y][mirror ? w - 1 - x : x] || null, c = at(ox + x, oy + y);
      if (!tok) { if (c) { ok = false; break; } } else if (!c || !tokMatch(tok, c.tags)) { ok = false; break; } else used.push(c.i);
    }
    if (ok && used.length === need) return used;
  }
  return null;
}
function matchShapeless(list, cells) {
  if (list.length !== cells.length) return null;
  const used = new Set();
  const go = (k) => { if (k === list.length) return true; for (const c of cells) { if (used.has(c.i) || !tokMatch(list[k], c.tags)) continue; used.add(c.i); if (go(k + 1)) return true; used.delete(c.i); } return false; };
  return go(0) ? Array.from(used).sort((a, b) => a - b) : null;
}
export function match(grid) {
  const cells = cellsOf(grid || []);
  if (!cells.length) return null;
  for (const r of RECIPES) {
    const used = r.shapeless ? matchShapeless(r.list, cells) : matchShaped(r.rows, cells);
    if (used) return { recipe: r, output: buildRecipeOutput(r, used.map((i) => grid[i].item)), count: r.count, consumes: used };
  }
  if (cells.length < 2) return null;
  return { recipe: null, output: combine(cells.map((c) => c.s)), count: 1, consumes: cells.map((c) => c.i) };
}

// ── combination engine ──
const PART_FX = { lens: { range: 0.2, spread: -0.1 }, coil: { dmg: 0.15 }, capacitor: { rate: 0.1, dmg: 0.05 }, antenna: { range: 0.1, speed: 0.1 }, chip: { speed: 0.12, rate: 0.05 }, gyro: { spread: -0.15 }, battery: { dmg: 0.08 }, plate: { spread: -0.03 }, fin: { rate: 0.08 }, servo: { speed: 0.1, rate: 0.05 } };
const MOD_SLOT = { lens: 'scope', antenna: 'scope', gyro: 'scope', coil: 'coil', capacitor: 'coil', battery: 'coil', chip: 'chamber', plate: 'chamber', fin: 'chamber', servo: 'chamber' };
const UP_SLOT = { servo: 'engine', fin: 'engine', coil: 'engine', battery: 'shield', plate: 'shield', capacitor: 'shield', gyro: 'jetpack', antenna: 'cargo', chip: 'cargo', lens: 'cargo' };
const SPECIAL_POOL = ['pierce', 'chain', 'burn', 'homingLite', 'splitOnHit', 'overcharge'];
const CAT_NOUN = { scope: 'Scope Mod', coil: 'Coil Mod', chamber: 'Chamber Mod', engine: 'Engine Kit', shield: 'Shield Kit', cargo: 'Cargo Kit', jetpack: 'Jetpack Kit' };
const RANK = { part: 0, inf: 1, food: 2, other: 3 };

function fragsOf(it) {
  if (it.frags && it.frags.length) return it.frags.map((f) => [f, it.kind === 'part' || it.category === 'weapon-mod' || it.category === 'ship-upgrade' ? 'part' : 'food']);
  if (it.kind === 'part' && PART_BY_ID[it.base]) return [[PART_BY_ID[it.base].name.replace(/\s+/g, '-'), 'part']];
  const out = [];
  if (it.infusion) out.push([cap(String(it.infusion).split(/[\s-]/)[0]), 'inf']);
  const words = String(it.base || it.name || 'Thing').replace(/\(.*?\)/g, '').trim().split(/\s+/);
  out.push([words[words.length - 1].replace(/[^A-Za-z0-9']/g, '') || 'Thing', 'food']);
  return out;
}
function blendEffect(items, tier) {
  const fx = items.filter((i) => i.effect && i.effect.params), sums = {}, cnt = {}, ex = {}, ecnt = {};
  let dur = 0, tint = null;
  for (const it of fx) {
    const e = it.effect; dur += e.duration || 0; if (e.tintColor != null && tint == null) tint = e.tintColor;
    for (const k in e.params) { sums[k] = (sums[k] || 0) + e.params[k]; cnt[k] = (cnt[k] || 0) + 1; }
    for (const k in (e.extras || {})) if (typeof e.extras[k] === 'number') { ex[k] = (ex[k] || 0) + e.extras[k]; ecnt[k] = (ecnt[k] || 0) + 1; }
  }
  const boost = 1 + 0.08 * (tier - 1), params = {}, extras = {};
  for (const k in sums) { const v = sums[k] / cnt[k]; params[k] = r2(k === 'hue' || k === 'timeScale' ? v : Math.min(1, v * boost)); }
  for (const k in ex) extras[k] = r2(ex[k] / ecnt[k]);
  const e = { duration: fx.length ? Math.round(dur / fx.length * (1 + 0.1 * (tier - 1))) : 60, params, extras };
  if (tint != null) e.tintColor = tint;
  return e;
}
function blendColor(items) {
  let r = 0, g = 0, b = 0; for (const it of items) { const c = it.color | 0; r += (c >> 16) & 255; g += (c >> 8) & 255; b += c & 255; }
  const n = Math.max(1, items.length); return (Math.round(r / n) << 16) | (Math.round(g / n) << 8) | Math.round(b / n);
}

export function combine(stacks) {
  const st = (stacks || []).filter((s) => s && s.item).slice().sort((a, b) => (stackKeyOf(a.item) < stackKeyOf(b.item) ? -1 : stackKeyOf(a.item) > stackKeyOf(b.item) ? 1 : 0));
  const items = st.map((s) => s.item), seedStr = st.map((s) => stackKeyOf(s.item)).join('|'), seed = hashStr(seedStr);
  const comp = mergeComp(items), tier = items.reduce((m, it) => Math.max(m, tierOf(it)), 1) + 1;
  const pw = WEAPON_PARTS.reduce((n, p) => n + (comp.parts[p] || 0), 0), sh = SHIP_PARTS.reduce((n, p) => n + (comp.parts[p] || 0), 0);
  const fd = comp.edible + comp.infusion * 0.5;
  let cat = 'junk';
  if (comp.junk < items.length && pw + sh + fd > 0) cat = fd > pw + sh ? 'food' : pw + sh === 0 ? 'junk' : pw > sh ? 'weapon-mod' : 'ship-upgrade';
  // dominant part (deterministic: highest count, tie by id)
  const domOf = (map, ids) => ids.slice().sort((a, b) => ((comp.parts[b] || 0) - (comp.parts[a] || 0)) || (a < b ? -1 : 1))[0];
  let slot = null, mod = null, upgrade = null, effect = null;
  const scale = 1 + 0.2 * (tier - 2), clampD = (v) => Math.max(-1.5, Math.min(1.5, v));
  if (cat === 'weapon-mod') {
    const dom = domOf(MOD_SLOT, WEAPON_PARTS.concat(SHIP_PARTS).filter((p) => comp.parts[p]));
    slot = MOD_SLOT[dom];
    const d = { dmg: 0, rate: 0, range: 0, speed: 0, spread: 0 };
    for (const p in comp.parts) { const fx = PART_FX[p]; for (const k in fx) d[k] += fx[k] * comp.parts[p] * scale; }
    for (const k in d) d[k] = r2(clampD(d[k]));
    if (tier >= 4 && comp.parts.servo) d.count = 1;
    const sp = (comp.infusion > 0 || tier >= 4) ? SPECIAL_POOL[seed % SPECIAL_POOL.length] : null;
    mod = { slot, delta: d, special: sp };
  } else if (cat === 'ship-upgrade') {
    const dom = domOf(UP_SLOT, SHIP_PARTS.concat(WEAPON_PARTS).filter((p) => comp.parts[p]));
    slot = UP_SLOT[dom];
    upgrade = { slot, level: 1 + Math.floor((tier - 2) / 2) + (pw + sh >= 4 ? 1 : 0) };
  } else if (cat === 'food') effect = blendEffect(items, tier);
  // name
  const all = [].concat.apply([], items.map(fragsOf)), seen = new Set(), uniq = [];
  for (const f of all) if (!seen.has(f[0])) { seen.add(f[0]); uniq.push(f); }
  uniq.sort((a, b) => hashStr(a[0] + seed) - hashStr(b[0] + seed));
  const pick = uniq.slice(0, 3).sort((a, b) => ((RANK[a[1]] - RANK[b[1]]) || (a[0] < b[0] ? -1 : 1)));
  const frags = pick.map((f) => f[0]);
  const noun = cat === 'junk' ? 'Scrap Heap' : cat === 'food' ? '' : (CAT_NOUN[slot] || 'Gizmo');
  const core = (frags.join(' ') + (noun ? ' ' + noun : '')).trim();
  const price = Math.max(5, Math.round(items.reduce((n, it) => n + (it.price || 10), 0) * (cat === 'junk' ? 0.4 : 0.85) + 14 * tier));
  const color = blendColor(items), base = noun || frags[frags.length - 1] || 'Gizmo';
  const key = 'cx:' + hashStr(seedStr).toString(36);
  const it = {
    id: key, recipe: null, name: core + ' Mk ' + roman(tier), kind: cat === 'food' ? 'food' : cat === 'junk' ? 'junk' : 'part', base, infusion: null, modifier: null,
    color, price, tier, category: cat, tags: ['crafted', 'cat:' + cat, 'combined'], stackKey: key, comp, frags, iconHint: HINT[cat], absurdity: 0, dealer: false,
    blurb: cat === 'junk' ? 'Nobody asked for this. Somebody will pay for it.' : cat === 'food' ? 'A combination nobody planned. Tastes like ' + (frags[frags.length - 1] || 'regret') + '.' : 'Improvised from ' + items.length + ' things. It mostly holds.',
  };
  if (mod) it.mod = mod; if (upgrade) it.upgrade = upgrade; if (effect) { it.effect = effect; it.family = 'cozy'; }
  return it;
}

// ── applying ──
const MOD_SLOTS = ['scope', 'coil', 'chamber'];
function rebuildStats(base, mods) {
  const s = Object.assign({}, base), m = { dmg: 0, rate: 0, range: 0, speed: 0, spread: 0, count: 0 };
  for (const slot of MOD_SLOTS) { const md = mods[slot]; if (!md) continue; for (const k in md.delta) m[k] = (m[k] || 0) + md.delta[k]; if (md.special) s.special = md.special; }
  s.dmg = r2(base.dmg * (1 + m.dmg)); s.rate = r2(base.rate * (1 + m.rate)); s.range = Math.round(base.range * (1 + m.range)); s.speed = Math.round(base.speed * (1 + m.speed));
  s.spread = r2(Math.max(0, base.spread * (1 + m.spread))); s.count = base.count + (m.count || 0);
  return s;
}
export function applyMod(weapon, mod) {
  const md = mod.mod || mod, slot = md.slot;
  if (MOD_SLOTS.indexOf(slot) < 0) return { weapon, line: 'No slot for ' + (mod.name || 'mod') };
  const baseStats = weapon.baseStats || Object.assign({}, weapon.stats), mods = Object.assign({}, weapon.mods || {});
  const before = weapon.stats; mods[slot] = { name: mod.name, delta: md.delta, special: md.special || null };
  const stats = rebuildStats(baseStats, mods);
  const w = Object.assign({}, weapon, { baseStats, mods, stats });
  const dps = (s) => s.dmg * s.rate * s.count, pct = Math.round((dps(stats) / Math.max(0.01, dps(before)) - 1) * 100);
  return { weapon: w, line: slot.toUpperCase() + ' slot: ' + (mod.name || 'mod') + ' | dps ' + (pct >= 0 ? '+' : '') + pct + '%' + (stats.special && stats.special !== before.special ? ', ' + stats.special : '') };
}
export function applyUpgrade(profile, upgrade) {
  const u = upgrade.upgrade || upgrade, slot = u.slot, lv = u.level || 1;
  const p = Object.assign({}, profile), ups = Object.assign({}, p.upgrades || {});
  ups[slot] = (ups[slot] || 0) + lv; p.upgrades = ups;
  if (slot === 'pens') p.pens = (p.pens || 0) + lv; else if (slot === 'pods') p.pods = (p.pods || 0) + lv; else if (slot === 'fuel') p.fuelMax = (p.fuelMax || 100) + 25 * lv;
  return { profile: p, line: slot.toUpperCase() + ' +' + lv + ' (now ' + ups[slot] + ')' };
}
export function sellPrice(item) {
  if (!item) return 0;
  const f = item.category === 'junk' ? 0.35 : item.kind === 'fries' ? 0.5 : 0.5;
  return Math.max(1, Math.floor((item.price || 0) * f));
}

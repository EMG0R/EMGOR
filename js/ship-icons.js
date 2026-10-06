// Procedural 16x16 pixel-art icons for NO MANS GOR items + weapons (Minecraft item style).
// One hand-made template per base category; infusion hue-shifts the fill, adds sparkles;
// modifiers add a 3x3 corner badge. Deterministic, cached, no external assets.
//
// Template chars: . clear | O outline | d b l h = main fill (dark, mid, light, highlight)
// 1 2 3 w g k s = fixed per-template accents (see pal) | everything else clear.

function hashStr(s) { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
function mulberry(a) { a |= 0; return function () { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }

const DEF_PAL = { 1: 0xD9A05B, 2: 0xD8402E, 3: 0xF2EEE6, w: 0xF4F0E8, g: 0x9AA0AA, k: 0x2E2838, s: 0xE8D060 };

// [pal overrides, rows]
export const ICON_TEMPLATES = {
  cup: [{ 2: 0xE8E0E8, 3: 0xEFEAF2 }, [
    '..........OO....', '.........O22O...', '........O22O....', '..OOOOOOOOOOOO..',
    '..O3333333333O..', '..OOOOOOOOOOOO..', '...OhlbbbbbbdO..', '...OhlbbbbbbdO..',
    '...OhlbwwwbbdO..', '...OhlbwwwbbdO..', '...OhlbbbbbbdO..', '....OlbbbbbdO...',
    '....OlbbbbbdO...', '....OlbbbbddO...', '....OdbbbbddO...', '.....OOOOOOO....']],
  bag: [{ 2: 0xE04A3A, w: 0xF6F2E8 }, [
    '................', '..OOOOOOOOOOOO..', '..OwOwOwOwOwOO..', '..OOOOOOOOOOOO..',
    '..OhlbbbbbbbdO..', '..OhlbbbbbbbdO..', '..OhlbwwwwwbdO..', '..Ohlbw222wbdO..',
    '..OhlbwwwwwbdO..', '..OhlbbbbbbbdO..', '..OhlbbbbbbbdO..', '..OhlbbbbbbbdO..',
    '..OdbbbbbbbddO..', '..OOOOOOOOOOOO..', '..OwOwOwOwOwOO..', '..OOOOOOOOOOOO..']],
  hotdog: [{ 1: 0xE0B060, 2: 0xE8C020 }, [
    '................', '................', '...OOOOOOOOOO...', '..O1hh11111111O.',
    '.O111111111111O.', '.O1OOOOOOOOO1O..', '.OOhlbbbbbbbOO..', '.OhlbbbbbbbbdO..',
    '.O1dbbbbbbbddO1.', '.O11OdddddOO11O.', '..O1111111111O..', '...OOOOOOOOOO...',
    '................', '................', '................', '................']],
  pizza: [{ 1: 0xD9A05B, 2: 0xB8301E }, [
    '................', '..OOOOOOOOOOOO..', '.O1hh1111111dO..', '.O11111111111dO.',
    '.OOOOOOOOOOOOO..', '..OhlbbbbbbbdO..', '..OlbbObbbbbdO..', '...OlbbbbbOdbO..',
    '...ObbObbbbbO...', '....ObbbbbbdO...', '....OblbbObO....', '.....ObbbbdO....',
    '.....ObbObO.....', '......ObbdO.....', '......ObdO......', '.......OO.......']],
  donut: [{ 1: 0xD9A05B, 3: 0xF6F2E8, 2: 0x5CE8FF }, [
    '................', '.....OOOOOO.....', '...OOlhbbbbOO...', '..OlhbbwbbbbwO..',
    '.OlhbbOOOObbbdO.', '.OlbbOkkkkObbdO.', '.ObwbOkkkkObbdO.', '.ObbbbOOOO2bddO.',
    '.O1bbbbbbbbbddO.', '..O11bbwbbbddO..', '...OO111111OO...', '.....OOOOOO.....',
    '................', '................', '................', '................']],
  can: [{ 3: 0xB8BEC8, w: 0xF4F0E8, 2: 0xD8402E }, [
    '................', '....OOOOOOOO....', '...O33333333O...', '..OOOOOOOOOOOO..',
    '..OhlbbbbbbbdO..', '..OhlbbbbbbbdO..', '..OhlbbbbbbbdO..', '..OhlwwwwwwwdO..',
    '..Ohlw222222wdO.', '..OhlwwwwwwwdO..', '..OhlbbbbbbbdO..', '..OhlbbbbbbbdO..',
    '..OdbbbbbbbddO..', '...OOOOOOOOOO...', '................', '................']],
  energy: [{ 3: 0xB8BEC8, 2: 0xFFE24A, k: 0x25202E }, [
    '....OOOOOOOO....', '....O333333O....', '...OOOOOOOOOO...', '...OkkkkkkkkO...',
    '...OkkkkkkkkO...', '...OhlbbbbbdO...', '...Ohlbb22bdO...', '...Ohlb22bbdO...',
    '...Ohlbb22bbdO..', '...Ohlbb2bbdO...', '...Ohlb2bbbdO...', '...OhlbbbbbdO...',
    '...OdbbbbbddO...', '...OkkkkkkkkO...', '....OOOOOOOO....', '................']],
  bottle: [{ 3: 0xEFEAF2, w: 0xF4F0E8, 2: 0xD8402E }, [
    '................', '......OOOO......', '......O33O......', '......O33O......',
    '......OOOO......', '......OhbO......', '.....OhlbdO.....', '....OhlbbbdO....',
    '...OhlbbbbbdO...', '...OhwwwwwwdO...', '...Ohw2222wdO...', '...OhwwwwwwdO...',
    '...OhlbbbbbdO...', '...OdbbbbbddO...', '....OOOOOOOO....', '................']],
  tray: [{ 1: 0xE8B040, 3: 0xF2EEE6, 2: 0xD8402E }, [
    '................', '....OOO..OOO....', '...O1h1OO1h1O...', '..O1hbbbbbb1dO..',
    '..OhlbbbbbbbdO..', '..OlbbbbbbbbdO..', '.OOOOOOOOOOOOOO.', '.O333333333333O'.replace('',''),
    '..O3222222223O..', '..O3333333333O..', '...OO33333OO....', '....OOOOOOOO....',
    '................', '................', '................', '................']],
  jerky: [{ w: 0xE8C8A8 }, [
    '..........OOOO..', '........OOhlbbO.', '......OOhlbbbbdO', '....OOhlbbwbbdO.',
    '...OhlbbbbbbdOO.', '..OhlbbwbbbdOO..', '.OhlbbbbbbdOO...', '.ObbbbwbbdOO....',
    '.ObbbbbbdOO.....', '..OdbbbbdO......', '...OOddOO.......', '................',
    '................', '................', '................', '................']],
  stick: [{ 1: 0xC89A5A, 2: 0xD8402E }, [
    '......OOOO......', '.....OhlbdO.....', '....OhlbbbdO....', '....Ohl2bbdO....',
    '....Ohlb2bdO....', '....Ohl2bbdO....', '....Ohlb2bdO....', '....OhlbbbdO....',
    '....OhlbbbdO....', '....OdbbbddO....', '.....OOddOO.....', '......O11O......',
    '......O11O......', '......O11O......', '......O1dO......', '.......OO.......']],
  burrito: [{ w: 0xE4E8EE, g: 0xA4AAB4, s: 0xC03A2A }, [
    '................', '................', '...OOOOOOOOOO...', '..OhlbbbbbbbbO..',
    '.OhlbbsbbbbbbdO.', '.OlbbbbbbsbbbdO.', '.OwwwwwwwwwwwwO.', '.OwgwwgwwgwwgwO.',
    '..OgwwgwwgwwgO..', '...OOOOOOOOOO...', '................', '................',
    '................', '................', '................', '................']],
  candy: [{ 3: 0xF6F2E8 }, [
    '................', '................', '................', '................',
    '..O3O.OOOO.O3O..', '.O33OOhlbbOO33O.', 'O333OhlbbbbO333O', 'O333ObbbbbdO333O',
    '.O33OObbbdOO33O.', '..O3O.OOOO.O3O..', '................', '................',
    '................', '................', '................', '................']],
  fries: [{ 1: 0xF4C84A, w: 0xF6F2E8 }, [
    '....O..O.O......', '...O1OO1O1O.O...', '..O1h1O1h1O1O...', '..O1111111111O..',
    '..OOOOOOOOOOOO..', '..OhlbbbbbbbdO..', '...OhlbwwbbdO...', '...OhlwbbwbdO...',
    '...OhlbwwbbdO...', '...OhlbbbbbdO...', '...OhlbbbbbdO...', '....OlbbbbdO....',
    '....OdbbbbdO....', '.....OOOOOO.....', '................', '................']],
  pill: [{ 3: 0xF2EEE6, w: 0xF6F2E8, 2: 0xD8402E }, [
    '................', '...OOOOOOOOOO...', '..O3h33333333O..', '..O3333333333O..',
    '..OOOOOOOOOOOO..', '...OhlbbbbbdO...', '...OhlbbbbbdO...', '...OwwwwwwwwO...',
    '...Ow22ww33wO...', '...Ow2222ww3O...', '...OwwwwwwwwO...', '...OhlbbbbbdO...',
    '...OdbbbbbddO...', '....OOOOOOOO....', '................', '................']],
  bar: [{ w: 0xF2EEE6, 2: 0xD8402E }, [
    '................', '................', '................', '.O.OOOOOOOOOO.O.',
    '..OOhlbbbbbdOO..', '.O2OlbwwwwbdO2O.', '.O2OlbwwwwbdO2O.', '..OOdbbbbbddOO..',
    '.O.OOOOOOOOOO.O.', '................', '................', '................',
    '................', '................', '................', '................']],
};

// weapon silhouettes, drawn pointing right. b/d/l/h tinted by weapon colour
export const WEAPON_TEMPLATES = {
  bolt: [
    '................', '................', '................', '...OOOOOOOOOOO..',
    '..OgggggOllllhO.', '..OgkkkgObbbbwO.', '..OgggggOddddhO.', '..OOOOOOOOOOOO..',
    '...OgkO.........', '...OgkO.........', '...OggO.........', '....OOO.........',
    '................', '................', '................', '................'],
  needle: [
    '................', '................', '................', '................',
    '...OOOOOOOOOOOOO', '..OgggllllllhwO.', '..OgkkbbbbbbbddO', '...OOOOOOOOOOOOO',
    '...OgO..........', '...OgO..........', '....O...........', '................',
    '................', '................', '................', '................'],
  orb: [
    '................', '................', '.........OOOO...', '.......OOhhllO..',
    '..OOOOOOhlbbbdO.', '.OgggggOOlbbbdO.', '.OgkkkgOOlbbbdO.', '.OgggggOOObbdO..',
    '..OOOOOO..OOOO..', '...OgO..........', '...OgO..........', '....O...........',
    '................', '................', '................', '................'],
  shard: [
    '................', '..........OOO...', '.....OOOOOhlbO..', '..OOOgggOOOOOO..',
    '.OggkkggOhlbbdO.', '.OgkkkggOOOOOO..', '..OOOgggOhlbdO..', '.......OOOOO....',
    '...OgO..........', '...OgO..........', '....O...........', '................',
    '................', '................', '................', '................'],
};

const GLYPH = {
  C: ['###', '#..', '#..', '#..', '###'], B: ['##.', '#.#', '##.', '#.#', '##.'],
  A: ['.#.', '#.#', '###', '#.#', '#.#'], S: ['###', '#..', '###', '..#', '###'],
};
const BADGES = {
  x2: ['#.#', '#.#', '#.#'], plus: ['.#.', '###', '.#.'], skull: ['###', '#.#', '.#.'],
  flame: ['.#.', '##.', '###'], leaf: ['..#', '.##', '#..'], dash: ['...', '###', '...'],
  tag: ['###', '#.#', '###'],
};
const BADGE_OF = {
  Double: ['x2', 0xFFE24A], Mega: ['x2', 0xFF8A3A], Ultra: ['x2', 0xFF4FA8], 'Family Size': ['x2', 0x7CD0FF],
  Blessed: ['plus', 0xFFF4A0], 'Limited Edition': ['plus', 0xFFD24A], Premium: ['plus', 0xFFD24A], Deluxe: ['plus', 0xC8A0FF], Heroic: ['plus', 0x7CD0FF], Imported: ['plus', 0xFF9ACD],
  Cursed: ['skull', 0xC060FF], Expired: ['skull', 0x9AB860], Haunted: ['skull', 0xE8E8F8], Forbidden: ['skull', 0xFF4A4A], Whispered: ['skull', 0xA0B8FF],
  'Extra Hot': ['flame', 0xFF5A2A], Organic: ['leaf', 0x6CD84A], Diet: ['leaf', 0xBEEFFF], 'Gluten-Free': ['leaf', 0xE8D060],
  Discount: ['dash', 0xFF6A6A], 'Gas Station': ['tag', 0xB8BEC8], 'Legally Distinct': ['tag', 0xE8E8E8], Unlicensed: ['tag', 0xFF9A3A], Rotating: ['tag', 0x7CD0FF], Artisanal: ['tag', 0xD9A05B], Overnight: ['tag', 0x7A8AFF],
};

// base name -> template
const BASE_RULES = [
  [/gulp|slurpee|icee|fruit cup|float|lemon ice|soft serve|casserole/i, 'cup'],
  [/gardetto|chips|puffs|trail mix|pretzel/i, 'bag'], [/hot dog/i, 'hotdog'], [/pizza|quesadilla/i, 'pizza'],
  [/donut|cinnamon/i, 'donut'], [/soda/i, 'can'], [/energy|monster/i, 'energy'],
  [/brew|jug|coconut|sparkling|water/i, 'bottle'], [/nachos|sushi|mac bowl|popper|wings|popcorn/i, 'tray'],
  [/jerky/i, 'jerky'], [/taquito|corn dog|pickle/i, 'stick'], [/burrito|croissant/i, 'burrito'],
  [/gummy|straws|cotton|egg\b|hard boiled/i, 'candy'], [/fries/i, 'fries'], [/bar|banana|sandwich/i, 'bar'],
];
const KIND_TPL = { drink: 'bottle', snack: 'bag', food: 'tray', fries: 'fries' };
function templateFor(item) {
  if (item.family === 'weird' && item.kind !== 'fries') return 'pill';
  const base = String(item.base || item.name || '');
  for (const [re, t] of BASE_RULES) if (re.test(base)) return t;
  return KIND_TPL[item.kind] || 'bar';
}

// ---- colour helpers ----
const hex2 = (n) => [(n >> 16) & 255, (n >> 8) & 255, n & 255];
function rgb2hsl(r, g, b) {
  r /= 255; g /= 255; b /= 255; const mx = Math.max(r, g, b), mn = Math.min(r, g, b), l = (mx + mn) / 2; let h = 0, s = 0;
  if (mx !== mn) { const d = mx - mn; s = l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
    h = mx === r ? (g - b) / d + (g < b ? 6 : 0) : mx === g ? (b - r) / d + 2 : (r - g) / d + 4; h /= 6; }
  return [h, s, l];
}
function hsl2rgb(h, s, l) {
  h = ((h % 1) + 1) % 1; l = Math.max(0, Math.min(1, l)); s = Math.max(0, Math.min(1, s));
  const f = (p, q, t) => { t = ((t % 1) + 1) % 1; return t < 1 / 6 ? p + (q - p) * 6 * t : t < 1 / 2 ? q : t < 2 / 3 ? p + (q - p) * (2 / 3 - t) * 6 : p; };
  if (s === 0) { const v = Math.round(l * 255); return [v, v, v]; }
  const q = l < 0.5 ? l * (1 + s) : l + s - l * s, p = 2 * l - q;
  return [Math.round(f(p, q, h + 1 / 3) * 255), Math.round(f(p, q, h) * 255), Math.round(f(p, q, h - 1 / 3) * 255)];
}
const css = (c) => 'rgb(' + c[0] + ',' + c[1] + ',' + c[2] + ')';

function mainPalette(color, infusion) {
  const [r, g, b] = hex2((color == null ? 0x888888 : color) >>> 0);
  let [h, s, l] = rgb2hsl(r, g, b);
  if (infusion) { h += ((hashStr(infusion) % 1000) / 1000 - 0.5) * 0.36; s = Math.min(1, s * 1.1 + 0.08); }
  l = Math.max(0.3, Math.min(0.7, l));
  return {
    d: hsl2rgb(h + 0.02, s, l * 0.58), b: hsl2rgb(h, s, l), l: hsl2rgb(h - 0.01, s, l + 0.14),
    h: hsl2rgb(h - 0.02, s * 0.55, l + 0.32), O: hsl2rgb(h, Math.min(0.5, s), 0.09), spark: hsl2rgb(h, 0.9, 0.82),
  };
}

function put(px, x, y, c) { if (x >= 0 && y >= 0 && x < 16 && y < 16) px[y * 16 + x] = c; }
function drawGlyph(px, rows, x0, y0, col, back) {
  const w = rows[0].length, h = rows.length;
  for (let y = -1; y <= h; y++) for (let x = -1; x <= w; x++) {
    if (back && (x < 0 || y < 0 || x >= w || y >= h || true)) put(px, x0 + x, y0 + y, back);
  }
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) if (rows[y][x] === '#') put(px, x0 + x, y0 + y, col);
}

function parse(rows) { // -> array of 256 chars
  const out = [];
  for (let y = 0; y < 16; y++) { const r = (rows[y] || '').padEnd(16, '.'); for (let x = 0; x < 16; x++) out.push(r[x]); }
  return out;
}

function toCanvas(px) {
  const c = document.createElement('canvas'); c.width = 16; c.height = 16;
  const ctx = c.getContext('2d'), img = ctx.createImageData(16, 16);
  for (let i = 0; i < 256; i++) { const p = px[i]; if (p) { img.data[i * 4] = p[0]; img.data[i * 4 + 1] = p[1]; img.data[i * 4 + 2] = p[2]; img.data[i * 4 + 3] = 255; } }
  ctx.putImageData(img, 0, 0);
  return c;
}

function dither(ch, seedBit) {
  // b pixels on a checker beside a d pixel become d (soft 2-tone transition)
  for (let y = 0; y < 16; y++) for (let x = 0; x < 15; x++) {
    const i = y * 16 + x;
    if (ch[i] === 'b' && ch[i + 1] === 'd' && ((x + y + seedBit) & 1) === 0) ch[i] = 'd';
    if (ch[i] === 'b' && x > 0 && ch[i - 1] === 'l' && ((x + y + seedBit) & 1) === 1) ch[i] = 'l';
  }
}

const cache = new Map();

// ---- v2 icons: resources, crafting parts, mods, junk/scrap/shard, pets, crew, quests ----
// [default colour, pal overrides, rows]. Same char scheme as above; main fill tinted by colour.
export const NEW_TEMPLATES = {
  crystal: [0x9A6CFF, { w: 0xF6F0FF }, [
    '................', '.......OO.......', '......OhhO......', '.....OhlbbO.....', '.....OhlbbdO....', '....OhlbbbbdO...', '....OhlbwbbdO...', '....OlbbbbbdO...',
    '.OO.OlbbbbbdO...', 'OhlOOlbbbbbdO...', 'OlbbOlbbbbddO...', 'OdbbOdbbbbddO...', '.OOOOOdbbddOO...', '.....OOOOOO.....', '................', '................']],
  plant: [0x5CD060, { 1: 0xA8743A }, [
    '................', '................', '....OO....OO....', '...OhlO..OlhO...', '..OhlbbOOblbdO..', '..OlbbbdObbddO..', '...OdbbdObddO...', '....OOdbdOOO....',
    '......OdbO......', '......O1dO......', '......O1dO......', '....OOO11OOO....', '...O1dd11dd1O...', '...OdddddddddO..', '....OOOOOOOOO...', '................']],
  ore: [0x8A8E9C, { w: 0xF2D060 }, [
    '................', '................', '.....OOOOOO.....', '...OOhhlbbbOO...', '..OhlbbbbwbbdO..', '.OhlbbwbbbbbbdO.', '.OlbbbbbbbwbbdO.', '.OlbbwbbbbbbbdO.',
    '.OlbbbbbbbbbddO.', '.OdbbbbwbbbddO..', '..OdbbbbbbbddO..', '..OOdddbbdddOO..', '...OOOOOOOOOO...', '................', '................', '................']],
  ice: [0x7CD8FF, { w: 0xFFFFFF }, [
    '................', '......OOOO......', '....OOhhwlOO....', '...OhhlbbbbdO...', '..OhlbbwbbbbdO..', '..OlbbwwbbbbdO..', '..OlbbbwbbbbdO..', '..OlbbbbbbbddO..',
    '..OdbbbbbbbddO..', '...OdbbbbbbdO...', '....OOddbdOO....', '......OOOO......', '................', '................', '................', '................']],
  spore: [0xD060C8, { 3: 0xE8E0C8, w: 0xFFF0A0, g: 0xB8AE90 }, [
    '................', '................', '....OOOOOOOO....', '..OOhhlbbbbbOO..', '.OhlwwbbbwbbbdO.', '.OlwwwbbbbwbbdO.', '.OlbwbbbbbbbddO.', '.OdbbbbwbbbddO..',
    '..OOddddddddOO..', '....OO3333OO....', '.....O3w3gO.....', '.....O3w3gO.....', '.....O3w3gO.....', '......OOOO......', '................', '................']],
  capacitor: [0x4A7CD8, { 2: 0xE8E8F0, g: 0xB8BEC8 }, [
    '................', '....OgO..OgO....', '....OgO..OgO....', '..OOOOOOOOOOOO..', '..OhlbbbbbbbdO..', '..OhlbbbbbbbdO..', '..Ohl2222bbbdO..', '..Ohl2222bbbdO..',
    '..OhlbbbbbbbdO..', '..OhlbbbbbbbdO..', '..OdbbbbbbbddO..', '..OOOOOOOOOOOO..', '....OgO..OgO....', '....OgO..OgO....', '.....O....O.....', '................']],
  servo: [0x3A3E4E, { w: 0xE8E8F0, g: 0xB8BEC8, s: 0xE8D060, k: 0x1A1620 }, [
    '................', '................', '......OOOO......', '.....OwwwwO.....', '......OwwO......', '..OOOOOOOOOOOO..', '..OhlbbbbbbbdO..', 'OOOhlbbkkkbbdOOO',
    'OgOhlbbkkkbbdOgO', 'OOOhlbbbbbbbdOOO', '..OdbbbbbbbddO..', '..OOOOOOOOOOOO..', '.....OsOgOkO....', '................', '................', '................']],
  coil: [0xD8803A, { k: 0x3A2A22 }, [
    '................', '................', '....OOOOOOOOO...', '...OhlbbbbbbdO..', '...OOOOOOOOOO...', '....OdbbbbdO....', '...OhlbbbbbdO...', '...OOOOOOOOOO...',
    '....OdbbbbdO....', '...OhlbbbbbdO...', '...OOOOOOOOOO...', '....OdbbbbdO....', '...OhlbbbbbdO...', '...OOOOOOOOOO...', '................', '................']],
  plate: [0x8C94A8, { w: 0xF4F0E8 }, [
    '................', '................', '..OOOOOOOOOOOO..', '.OhhlbbbbbbbbdO.', '.OhwlbbbbbbbwdO.', '.OlbbbbbbbbbbdO.', '.OlbbbddddbbbdO.', '.OlbbdbbbbdbbdO.',
    '.OlbbdbbbbdbbdO.', '.OlbbbddddbbbdO.', '.OlbbbbbbbbbbdO.', '.OlbwbbbbbbwbdO.', '.OdbbbbbbbbbddO.', '..OOOOOOOOOOOO..', '................', '................']],
  lens: [0x7FE8FF, { g: 0x8E94A2, w: 0xFFFFFF }, [
    '................', '................', '....OOOOOOOO....', '...OOggggggOO...', '..OgOhhlbbbbOgO.', '.OgOhwwlbbbbdOgO', '.OgOhwlbbbbbdOgO', '.OgOlbbbbbbbdOgO',
    '.OgOlbbbbbbddOgO', '..OgOdbbbbbddOgO', '..OggOOddddOOggO', '...OOggggggggOO.', '.....OOOOOOO....', '................', '................', '................']],
  battery: [0x58D868, { 2: 0xFFE24A, g: 0xB8BEC8, k: 0x25202E }, [
    '................', '......OOOO......', '.....OgggkO.....', '...OOOOOOOOOO...', '...OhlbbbbbdO...', '...Ohlbb22bdO...', '...Ohlb22bbdO...', '...Ohlbb22bdO...',
    '...Ohlb2bbbdO...', '...OhlbbbbbdO...', '...OhlbbbbbdO...', '...OdbbbbbddO...', '...OOOOOOOOOO...', '................', '................', '................']],
  gyro: [0xC060D8, { g: 0x9AA0AA, k: 0x2E2838 }, [
    '................', '................', '....OOOOOOOO....', '..OOggggggggOO..', '.OggOOOOOOOOggO.', '.OgOOhlbbbbdOgO.', '.OgOhlbkkkbbdOgO', '.OgOlbbkkkbbdOgO',
    '.OgOdbbbbbbddOgO', '.OgOOdbbbbddOgO.', '.OggOOOOOOOOggO.', '..OOggggggggOO..', '....OOOOOOOO....', '................', '................', '................']],
  antenna: [0x58A8D8, { 2: 0xFF5A4A, g: 0xB8BEC8, k: 0x2E2838 }, [
    '................', '.......OO.......', '......O22O......', '......O22O......', '.......OO.......', '..O...OgkO...O..', '..OO..OgkO..OO..', '...OO.OgkO.OO...',
    '....OOOgkOOO....', '.....OOgkOO.....', '......OgkO......', '......OgkO......', '....OOOOOOOO....', '...OhlbbbbbdO...', '...OdbbbbbddO...', '...OOOOOOOOOO...']],
  fin: [0xE05A7A, {}, [
    '................', '.............OO.', '...........OOhO.', '.........OOhlbO.', '.......OOhlbbbdO', '.....OOhlbbbbdO.', '...OOhlbbbbbbdO.', '..OhlbbbbbbbddO.',
    '..OlbbbbbbbddO..', '..OhlbbbbbddO...', '..OdbbbbddOO....', '..OOddddOO......', '...OOOOOO.......', '................', '................', '................']],
  chip: [0x2E8A5A, { g: 0xC8CCD4, w: 0xE8E8F0 }, [
    '................', '................', '................', '..OOOOOOOOOOOO..', '.OgOhlbbbbbbdOgO', '..OOhlbbbbbbdOO.', '.OgOhlbwbbbbdOgO', '..OOhlbbbbbbdOO.',
    '.OgOhlbbbkbbdOgO', '..OOhlbbbbbbdOO.', '.OgOdbbbbbbddOgO', '..OOOOOOOOOOOO..', '................', '................', '................', '................']],
  scope: [0x4A5060, { w: 0x7FE8FF, g: 0x9AA0AA, k: 0x2E2838 }, [
    '................', '................', '................', '................', '....OOOOOOOO....', '..OOhlbbbbbdOO..', '.OwwOhlbbbbdOwwO', '.OwwOlbbbbbdOwwO',
    '.OwwOdbbbbddOwwO', '..OOOdbbbbdOOO..', '......OddO......', '....OOOOOOOO....', '....OgggggkgO...', '....OOOOOOOO....', '................', '................']],
  chamber: [0xFF8A3A, { w: 0xFFF4C0, s: 0xFFE24A, g: 0x9AA0AA, k: 0x2E2838 }, [
    '................', '................', '....OOOOOOOO....', '...OgggggggkO...', '..OOOOOOOOOOOO..', '..OhlbbbbbbbdO..', '..OhlOOOOOObdO..', '..OhlOwwwwObdO..',
    '..OhlOwsswObdO..', '..OhlOwwwwObdO..', '..OhlOOOOOObdO..', '..OdbbbbbbbddO..', '..OOOOOOOOOOOO..', '...OgggggggkO...', '....OOOOOOOO....', '................']],
  kit: [0xB8884A, { 2: 0xE04A3A, w: 0xF4F0E8 }, [
    '................', '................', '................', '.OOOOOOOOOOOOOO.', '.OhhlbbbbbbbbdO.', '.OOOOOOOOOOOOOO.', '.Olbbbbw22wbbdO.', '.Olbbbw2222wbdO.',
    '.Olbbbbw22wbbdO.', '.OlbbbbbbbbbbdO.', '.OdbbbbbbbbbddO.', '.OOOOOOOOOOOOOO.', '................', '................', '................', '................']],
  junk: [0x7A7E8A, { k: 0x2E2838 }, [
    '................', '......OOOO......', '...OO.OhlO.OO...', '..OhlOOhlbOOdO..', '...OhlbbbbbdO...', '..OOhlbOObbdOO..', '.OhlbbOkkObbbdO.', '.OlbbbOkkObbbdO.',
    '..OOdbbOObbddOO.', '...OdbbbbbbddO..', '..OOdOObbOOdOO..', '..OhdO.OO.OdhO..', '...OO......OO...', '................', '................', '................']],
  scrap: [0x9AA0AE, { w: 0xF4F0E8 }, [
    '................', '................', '................', '..OOOO....OO....', '.OhllOOOOOllO...', '.OhlbbbbbbbbdO..', '..OlbbbwbbbbdO..', '..OOdbbbbbbddO..',
    '....OOddbdOOO...', '.......OOO......', '................', '................', '................', '................', '................', '................']],
  shard: [0x5CE8FF, { w: 0xFFFFFF, s: 0xD8F8FF }, [
    '................', '.......OO.......', '..s...OwhO......', '.....OhlbbO.....', '....OhlbbbbO....', '...OhlbbbbbdO...', '..OhlbwwbbbbdO..', '..OlbbwwbbbbdO..',
    '...OlbbbbbbdO...', '....OdbbbbdO....', '.....OdbbdO..s..', '......OddO......', '.......OO.......', '................', '................', '................']],
  walker: [0xD8A05A, { k: 0x2E2838 }, [
    '................', '................', '................', '................', '..........OOOO..', '..OOOOOO.OhlbbO.', '.OhlbbbbbbbwbbdO', '.OlbbbbbbbbbbdO.',
    '.OlbbbbbbbbbddO.', '.OdbbbbbbbbddOO.', '..OOOOOOOOOOOO..', '..OdOOdO.OdOOdO.', '..OOOOOO.OOOOOO.', '................', '................', '................']],
  jelly: [0xFF7ACD, { k: 0x2E2838 }, [
    '................', '.....OOOOOO.....', '...OOhhlbbbOO...', '..OhwlbbbbbbdO..', '.OhwlbbbbbbbbdO.', '.OlbbbkbbkbbbdO.', '.OlbbbbbbbbbbdO.', '.OdbbbbbbbbbddO.',
    '..OOOOOOOOOOOO..', '..OlO.OdO.OlO...', '...OlO.OdO.OdO..', '..OdO.OlO.OdO...', '...OO..OO..OO...', '................', '................', '................']],
  strider: [0x7CD860, { k: 0x2E2838 }, [
    '................', '....OOOOOO......', '...OhlbbbbOO....', '..OhlbwbbbbbO...', '..OlbbbbbbbbdO..', '...OdbbbbbbdO...', '....OOOddOOO....', '....OlO..OdO....',
    '...OlO....OdO...', '...OlO....OdO...', '..OlO......OdO..', '..OlO......OdO..', '.OllO......OddO.', '.OOOO......OOOO.', '................', '................']],
  critter: [0xB07AFF, { 2: 0xFF8AB0, k: 0x2E2838 }, [
    '................', '..OO........OO..', '.OhbO......ObdO.', '.OhlbOOOOOObbdO.', '..OhlbbbbbbbdO..', '.OhlbkbbbbkbbdO.', '.Olbbbbb22bbbdO.', '.OlbbbbbbbbbbdO.',
    '.OdbbbbbbbbbddO.', '..OdbbbbbbbddO..', '...OOddddddOO...', '....OOOOOOOO....', '................', '................', '................', '................']],
};
const HELMET = [
  '................', '.....OOOOOO.....', '...OOhhlbbbOO...', '..OhlbbbbbbbdO..', '.OhlbbbbbbbbbdO.', '.OlbOOOOOOOObdO.', '.OlOkkkkkkkkOdO.', '.OlOkkkkkkkkOdO.',
  '.OlOkkkkkkkkOdO.', '.OlOkkkkkkkkOdO.', '.OlOkkkkkkkkOdO.', '.OdOOOOOOOOOOdO.', '..OdbbbbbbbddO..', '...OOOOOOOOOO...', '................', '................'];
const ROLE_GLYPH = {
  pilot: ['..#..', '.###.', '#####', '..#..', '.#.#.'], miner: ['.###.', '#..#.', '...#.', '...#.', '...#.'],
  chef: ['.#.#.', '#####', '#####', '.###.', '.###.'], scout: ['..#..', '.#.#.', '#.#.#', '.#.#.', '..#..'],
};
const ROLE_COL = { pilot: 0x5AA8FF, miner: 0xE0A040, chef: 0xFF6A5A, scout: 0x6CD870 };
const COIN = [
  '................', '.....OOOOOO.....', '...OOhhllbbOO...', '..OhllbbbbbbdO..', '.OhlbbbbbbbbbdO.', '.OlbbbbbbbbbbdO.', '.OlbbbbbbbbbbdO.', '.OlbbbbbbbbbbdO.',
  '.OlbbbbbbbbbbdO.', '.OlbbbbbbbbbbdO.', '.OlbbbbbbbbbbdO.', '.OdbbbbbbbbbddO.', '..OdbbbbbbbddO..', '...OOddddddOO...', '.....OOOOOO.....', '................'];
const QUEST = {
  deliver: [0xE8B040, ['.####.', '#....#', '######', '#.##.#', '#.##.#', '######']], bounty: [0xE04A3A, ['..##..', '.#..#.', '#.##.#', '#.##.#', '.#..#.', '..##..']],
  scan: [0x4AD0FF, ['.###..', '#...#.', '#...#.', '.###..', '....#.', '.....#']], retrieve: [0xB070FF, ['..##..', '.####.', '######', '..##..', '..##..', '..##..']],
  escort: [0x5AD870, ['######', '#....#', '#....#', '.#..#.', '.#..#.', '..##..']], harvest: [0x7CE050, ['...###', '..####', '.####.', '####..', '##.#..', '#..#..']],
};
const PET_PLAN = [[/jelly|blob|float|drift|squid/i, 'jelly'], [/strider|stilt|long|tall/i, 'strider'], [/walker|walk|hound|quad|beast/i, 'walker']];
const PARTS = new Set(['capacitor', 'servo', 'coil', 'plate', 'lens', 'battery', 'gyro', 'antenna', 'fin', 'chip']);
const RES = new Set(['crystal', 'plant', 'ore', 'ice', 'spore']);
const HINT_ALIAS = { crate: 'kit', upgrade: 'kit', 'ship-upgrade': 'kit', 'weapon-mod': 'scope', modcoil: 'coil2', 'mod-coil': 'coil2', barrel: 'chamber', sight: 'scope' };

function buildNew(key, T, color, post) {
  const hit = cache.get(key); if (hit) return hit;
  const pal = Object.assign({}, DEF_PAL, T[1]), mp = mainPalette(color == null ? T[0] : color, null);
  const rows = T[2], ch = parse(rows), px = new Array(256).fill(null);
  dither(ch, hashStr(key) & 1);
  for (let i = 0; i < 256; i++) { const c = ch[i]; if (c === '.') continue; if (mp[c]) px[i] = mp[c]; else if (pal[c] != null) px[i] = hex2(pal[c]); }
  if (post) post(px, mp);
  const c = toCanvas(px); cache.set(key, c); return c;
}
export function questIcon(kind) {
  kind = QUEST[kind] ? kind : 'deliver'; const Q = QUEST[kind];
  return buildNew('Q|' + kind, [Q[0], {}, COIN], null, (px) => drawGlyph(px, Q[1], 5, 5, [30, 20, 40], null));
}
export function crewIcon(m) {
  m = m || {}; const role = ROLE_GLYPH[m.role] ? m.role : 'pilot', col = m.color == null ? ROLE_COL[role] : m.color;
  return buildNew('C|' + role + '|' + (col | 0), [col, { k: 0x1E1830 }, HELMET], col, (px) => drawGlyph(px, ROLE_GLYPH[role], 5, 6, [150, 240, 255], null));
}
function newTemplateFor(item) {
  const kind = String(item.kind || ''), base = String(item.base || '').toLowerCase(), hint = String(item.iconHint || '').toLowerCase(), cat = String(item.category || '');
  if (QUEST[kind]) return ['Q', kind];
  if (kind === 'pet' || item.plan) { const p = String(item.plan || item.name || ''); for (const [re, t] of PET_PLAN) if (re.test(p)) return ['T', t]; return ['T', 'critter']; }
  if (kind === 'crew' || item.role) return ['C'];
  if (kind === 'resource' && RES.has(base)) return ['T', base];
  if (kind === 'shard' || /signal shard/i.test(item.name || '') || base === 'signal shard') return ['T', 'shard'];
  if (kind === 'scrap' || base === 'scrap' || /^scrap$/i.test(item.name || '')) return ['T', 'scrap'];
  const h = HINT_ALIAS[hint] || hint;
  if (cat === 'junk') return ['T', NEW_TEMPLATES[h] ? h : 'junk'];
  if (cat === 'weapon-mod') return ['T', h === 'coil' || h === 'coil2' ? 'modcoil' : NEW_TEMPLATES[h] ? h : 'scope'];
  if (cat === 'ship-upgrade') return ['T', NEW_TEMPLATES[h] && h !== 'coil' ? h : 'kit'];
  if (item.family || /^(drink|snack|food|fries)$/.test(kind)) return null;
  if (NEW_TEMPLATES[h]) return ['T', h];
  if (PARTS.has(base) || RES.has(base)) return ['T', base];
  return null;
}
function newIcon(item) {
  const r = newTemplateFor(item); if (!r) return null;
  if (r[0] === 'Q') return questIcon(r[1]);
  if (r[0] === 'C') return crewIcon(item);
  const t = r[1], tint = (t === 'modcoil') ? 0x4AD0FF : null, T = t === 'modcoil' ? NEW_TEMPLATES.coil : NEW_TEMPLATES[t];
  const useColor = (item.kind === 'resource' || item.kind === 'pet' || item.plan) && item.color != null ? item.color : tint;
  return buildNew('N|' + t + '|' + (useColor == null ? '' : useColor | 0), T, useColor);
}


export function iconFor(item) {
  if (!item) item = {};
  if (item.shape && !item.base) return weaponIcon(item);
  { const n = newIcon(item); if (n) return n; }
  const key = 'I|' + (item.base || item.name) + '|' + (item.infusion || '') + '|' + (item.modifier || '') + '|' + (item.color | 0);
  const hit = cache.get(key); if (hit) return hit;
  const tname = templateFor(item), T = ICON_TEMPLATES[tname];
  const pal = Object.assign({}, DEF_PAL, T[0]);
  const mp = mainPalette(item.color, item.infusion);
  const ch = parse(T[1]);
  const sd = hashStr(key), r = mulberry(sd);
  dither(ch, sd & 1);
  const px = new Array(256).fill(null);
  for (let i = 0; i < 256; i++) {
    const c = ch[i]; if (c === '.') continue;
    if (mp[c]) px[i] = mp[c]; else if (pal[c] != null) px[i] = hex2(pal[c]);
  }
  // infusion sparkles on clear cells near the item
  if (item.infusion) {
    const cand = [];
    for (let y = 1; y < 15; y++) for (let x = 1; x < 15; x++) {
      if (px[y * 16 + x]) continue;
      let near = false;
      for (let dy = -2; dy <= 2 && !near; dy++) for (let dx = -2; dx <= 2; dx++) { const xx = x + dx, yy = y + dy; if (xx >= 0 && yy >= 0 && xx < 16 && yy < 16 && px[yy * 16 + xx] && (dx || dy)) { near = true; break; } }
      if (near && (x > 3 || y > 3)) cand.push([x, y]);
    }
    const n = Math.min(3, cand.length);
    for (let k = 0; k < n; k++) {
      const j = Math.floor(r() * cand.length), [x, y] = cand.splice(j, 1)[0];
      put(px, x, y, k === 0 ? [255, 255, 255] : mp.spark);
      if (k === 0) for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) if (!px[(y + dy) * 16 + x + dx] && x + dx >= 0 && x + dx < 16 && y + dy >= 0 && y + dy < 16) put(px, x + dx, y + dy, mp.spark);
    }
  }
  // modifier badge, top-left 3x3
  if (item.modifier) {
    const b = BADGE_OF[item.modifier] || [['tag', 'dash', 'plus'][hashStr(item.modifier) % 3], 0xD0C8E0 | 0];
    drawGlyph(px, BADGES[b[0]], 0, 0, hex2(b[1]), [20, 14, 28]);
  }
  const c = toCanvas(px); cache.set(key, c); return c;
}

export function weaponIcon(w) {
  w = w || {};
  const key = 'W|' + (w.shape || 'bolt') + '|' + (w.color | 0) + '|' + (w.cls || 'C');
  const hit = cache.get(key); if (hit) return hit;
  const T = WEAPON_TEMPLATES[w.shape] || WEAPON_TEMPLATES.bolt;
  const mp = mainPalette(w.color == null ? 0x7FE8FF : w.color, null);
  const pal = Object.assign({}, DEF_PAL, { g: 0x8E94A2, k: 0x2E2838, w: 0xFFFFFF });
  const ch = parse(T), px = new Array(256).fill(null);
  for (let i = 0; i < 256; i++) { const c = ch[i]; if (c === '.') continue; if (mp[c]) px[i] = mp[c]; else if (pal[c] != null) px[i] = hex2(pal[c]); }
  const g = GLYPH[String(w.cls || 'C').charAt(0)] || GLYPH.C;
  drawGlyph(px, g, 12, 10, mp.h, [20, 14, 28]);
  const c = toCanvas(px); cache.set(key, c); return c;
}

export function iconDataUrl(item) { return iconFor(item).toDataURL('image/png'); }

export function drawIcon(ctx, item, x, y, scale) {
  scale = scale || 1;
  const prev = ctx.imageSmoothingEnabled; ctx.imageSmoothingEnabled = false;
  ctx.drawImage(iconFor(item), x, y, 16 * scale, 16 * scale);
  ctx.imageSmoothingEnabled = prev;
}

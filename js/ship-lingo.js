// ship-lingo.js — per-seed alien lexicon + sentence recombination (docs/ship-mode.md rev 23 C). No model calls, tiny CPU.
//   name(seed)                    -> a pronounceable alien name (deterministic)
//   word(seed, concept)           -> the alien spelling of an English concept in that seed's lexicon (consistent forever)
//   translate(word, known, seed?) -> the English word if `known` (a Set of concepts) has it, else the alien word (seed default 0)
//   line(npc, ctx)                -> string. npc = { seed, role, known:Set, name? }. ctx = 'greeting'|'pitch'|'gossip'|'warning'|'lore'|'dealer'|'cashier'|'fries'|'shopper'
//                                    or { ctx, item, price, name } to fill {item}/{price}/{name}.  Alien words are wrapped in ALIEN_OPEN/ALIEN_CLOSE
//                                    (zero-width chars, invisible if printed raw).  Each call advances npc._n so lines vary but stay seeded.
//   render(line, seed?)           -> [{ text, alien:bool, concept? }] segments (concept only when `seed` is given, for teaching on click)
//   strip(line)                   -> the line without markers
//   teach(known, n=1)             -> [concept...] newly added to `known` (a Set), random among the still-unknown
//   CONCEPTS                      -> every translatable word (the "dictionary")
export const ALIEN_OPEN = '​', ALIEN_CLOSE = '‌';
export const CONCEPTS = ['hello', 'friend', 'buy', 'sell', 'cheap', 'rich', 'star', 'void', 'ship', 'fly', 'danger', 'fast', 'food', 'drink', 'tasty', 'strange', 'love', 'hate', 'big', 'small',
  'warm', 'cold', 'bright', 'dark', 'planet', 'moon', 'home', 'gold', 'sleep', 'fear', 'joy', 'trade', 'hungry', 'secret', 'ancient', 'quiet', 'loud', 'enemy', 'pirate', 'law', 'dust', 'song',
  'dance', 'dream', 'eat', 'fries', 'salt', 'grease', 'fresh', 'hot', 'trust', 'listen', 'whisper', 'deal', 'good', 'best', 'rare', 'free', 'price', 'lost', 'found', 'wise', 'fool', 'fun',
  'wallet', 'door', 'light', 'soon', 'never', 'always', 'again', 'thanks', 'sorry', 'welcome', 'night', 'day', 'water', 'fire', 'ice', 'smell', 'taste', 'sky', 'map', 'coffee', 'sugar',
  'glow', 'weird', 'legal', 'shiny', 'rumor', 'guard', 'toll', 'gorcoin', 'mission', 'crew', 'station', 'work'];

function hashStr(s) { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
function mulberry(a) { a |= 0; return function () { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
const seedOf = (s) => (typeof s === 'number' ? (s >>> 0) : hashStr(s));

const ONSETS = ['b', 'd', 'g', 'k', 'm', 'n', 'p', 'r', 's', 't', 'v', 'z', 'th', 'kr', 'zh', 'sk', 'gl', 'vr', 'ql', 'x', 'h', 'l', 'f', 'br', 'tr', 'sh', 'w', 'y'];
const VOWELS = ['a', 'e', 'i', 'o', 'u', 'ae', 'oo', 'ee', 'ou', 'ai', 'y', 'ia'];
const CODAS = ['', '', 'n', 'r', 'x', 'k', 'th', 'm', 's', 'l', 'g', 'zz', 'rk', 'nd'];
const BAD = /sex|ass|cum|tit|fag|gay|nig|fuk|fok|dik|cok|pis|rap|pus|kkk|wtf|nazi|anal|hoe|dam|hell/;
const lexCache = new Map();
function inventory(seed) {
  const r = mulberry(seed ^ 0x13579bdf), pick = (a, n) => { const o = []; while (o.length < n) { const v = a[Math.floor(r() * a.length)]; if (o.indexOf(v) < 0) o.push(v); } return o; };
  return { on: pick(ONSETS, 10), vo: pick(VOWELS, 5), co: pick(CODAS, 4), r };
}
function lexicon(seed) {
  seed = seed >>> 0;
  let lx = lexCache.get(seed); if (lx) return lx;
  const inv = inventory(seed), used = new Set(); lx = { map: new Map(), rev: new Map() };
  for (const c of CONCEPTS) {
    const r = mulberry(seed ^ hashStr(c)); let w = '', guard = 0;
    do {
      const n = 1 + Math.floor(r() * 2.4); w = '';
      for (let i = 0; i < n; i++) w += inv.on[Math.floor(r() * inv.on.length)] + inv.vo[Math.floor(r() * inv.vo.length)] + (i === n - 1 || r() < 0.2 ? inv.co[Math.floor(r() * inv.co.length)] : '');
      guard++;
    } while ((used.has(w) || w.length < 2 || BAD.test(w)) && guard < 30);
    used.add(w); lx.map.set(c, w); lx.rev.set(w, c);
  }
  if (lexCache.size > 64) lexCache.clear();
  lexCache.set(seed, lx);
  return lx;
}
export function word(seed, concept) { const lx = lexicon(seedOf(seed)); return lx.map.get(String(concept).toLowerCase()) || String(concept); }
export function name(seed) {
  const s = seedOf(seed), inv = inventory(s ^ 0x2468ace), r = mulberry(s ^ 0xabcdef);
  const n = 2 + Math.floor(r() * 2); let w = '';
  for (let i = 0; i < n; i++) w += inv.on[Math.floor(r() * inv.on.length)] + inv.vo[Math.floor(r() * inv.vo.length)] + (i === n - 1 ? inv.co[Math.floor(r() * inv.co.length)] : '');
  if (BAD.test(w)) w = 'zor' + w.slice(-2);
  return w.charAt(0).toUpperCase() + w.slice(1);
}
export function translate(w, known, seed) {
  const c = String(w).toLowerCase();
  if (known && known.has(c)) return w;
  const a = word(seed || 0, c);
  return a === c ? w : a;
}

// ── sentence material.  [concept] = translatable word ([Concept] capitalises);  {slot} = random pick from SLOTS;  {name} {item} {price} = from ctx ──
const SLOTS = {
  adj: ['[big]', '[small]', '[strange]', '[ancient]', '[shiny]', '[rare]', '[weird]', '[quiet]', '[bright]', 'suspiciously [cheap]'],
  thing: ['[fries]', '[coffee]', '[sugar]', '[map]', '[dust]', '[void]', '[moon]', '[sky]', '[ship]', '[gold]', '[salt]'],
  greet: ['[Hello]', '[Welcome]', 'Oh, [hello]', '[Hello], [friend]'],
  mood: ['[joy]', '[fear]', '[love]', '[hunger]'.replace('hunger', 'hungry')],
  bye: ['[Thanks]', 'Come back [soon]', 'Safe [sky]s', '[Thanks], [friend]'],
  when: ['[soon]', '[never]', '[always]', '[again]', 'by [night]'],
  who: ['the [pirate]s', 'the [guard]', 'a [fool]', 'a [wise] one', 'my cousin', 'a [rich] stranger', 'the [law]'],
  place: ['the [dark] side of the [moon]', 'behind the [door]', 'under the [map]', 'past the [toll]', 'in the [void]', 'near the [water]'],
  quip: ['Allegedly.', 'Do not quote me.', 'I read it on a wall.', 'The wall was very sure.', 'Anyway.', 'Hm.'],
};
const T = {
  greeting: [
    '{greet}! Mind the [door], it has opinions.', '{greet}. Everything here is {adj}, including me.', '{greet}, {name} here. Do not touch the [shiny] things.', '{greet}! [Fresh] air, [free]. Everything else costs.',
    '{greet}. You look [hungry]. Or lost. Both are legal here.', '{greet}, traveller. The aisles are long and [warm].', 'Oh. A visitor. {greet}.', '{greet}! I was just thinking about {thing}.',
    '{greet}. [Good] timing. Nothing exploded yet.', '{greet}, [friend]! Or customer. Same word, nearly.', '{greet}. Stay [warm], stay [fun], stay [hungry].', '{greet}! Welcome to the [best] shelf of the [void].',
    '{greet}. I {mood}ly acknowledge your presence.'.replace('{mood}ly', 'politely'), '{greet}! Take a basket. Fill it with {adj} choices.',
  ],
  pitch: [
    '{item}: {price} [gorcoin]. I would call it a steal, but I take [gorcoin].', 'Spend your [gorcoin] on the {item}. Save it for what? A rainy [planet]?', 'The {item}, {price} [gorcoin]. Worth every coin, allegedly.',
    'Try the {item}. It is [tasty] and only {price} [gorcoin].', '{item}, {price} [gorcoin]. Cheaper than a [rumor], [good]er too.', 'You want [best]? Take the {item}. {price} and it is yours.',
    'This {item} is {adj}. {price} [gorcoin]. I am not allowed to say more.', 'Hot today: {item}. [Fresh] enough. {price}.', 'A [rare] find! {item}, {price} [gorcoin]. {quip}',
    'Buy the {item}. Your [future] self will understand. {price} [gorcoin].'.replace('[future]', '[wise]'), 'The {item} is [tasty] and {adj}. {price}. No refunds on [dream]s.',
    '{item}! [Cheap] at {price}, [shiny] at any price.', 'For you, {price} [gorcoin] for the {item}. For others, also {price}. I am consistent.', 'Everyone who tries the {item} comes back. Some stay. {price}.',
    'Two for {price}, one for [never]: the {item}.', '[Trade] me {price} [gorcoin] and take the {item} away.', 'The {item}. [Good] for the [sky], [good] for the [sleep].',
  ],
  gossip: [
    'I heard {who} turned one [gorcoin] into a thousand. Then back into one.', 'The [gorcoin] is just a [dream] with a serial number. {quip}', '{who} pays in [gorcoin] only. I charge extra for that.', 'A rich [pirate] hides [gorcoin] {place}. Allegedly. {quip}',
    'I heard {who} found {thing} {place}. {quip}', 'They say {who} loves {thing}. They say a lot.', 'Between us: {who} owes me {thing}. {when} I will ask.', 'Did you hear? {who} stopped by {place}. Then [never] came back.',
    'My cousin says the [moon] is {adj} this season. {quip}', 'Word is {who} is selling [map]s of {place}. {quip}', 'Someone left {thing} {place}. [Glow] in the dark. I did not touch it.',
    'I hear {who} bought [coffee] for the whole [planet]. {quip}', '{who} told me a [secret]. It was about {thing}. I forgot.', 'They are saying the [price] of {thing} is {adj} again. {quip}',
    'Rumor: {thing} cures [fear]. Counter-rumor: it causes it.', 'Heard {who} dancing {place}. [Dance] like nobody is [guard]ing.', '{who} swears {thing} is {adj}. I swear nothing. Sweet life.',
  ],
  warning: [
    '[Danger] {place}. I mean it. A little.', 'Careful, {who} is around. Hide your [wallet].', 'Do not go {place}. I did. Look at me.', 'The [void] is {adj} today. Fly [slow]ly.'.replace('[slow]', '[quiet]'),
    '[Danger]! ...kidding. Only {adj} danger, the [good] kind.', 'Watch out for {who}. They take {thing}, and then they take more.', 'The [guard] says be [good]. The [pirate]s say the opposite.',
    'If you see {thing} {place}, leave it. [Fear] is wise.', 'Keep your [ship] locked. {who} is [hungry].', 'The [toll] ahead is {adj}. Pay it. Do not discuss it.', '[Danger] is [hot] tonight, [friend]. Go [home].',
    'Do not eat {thing} before you fly. [Trust] me, or not. Do not.', '{who} is [loud] tonight. That is the warning.',
  ],
  lore: [
    'Long ago, a [star] ate a [moon] and was never [hungry] again.', 'The [ancient] ones built this [door] from [dust] and [song].', 'They say the [void] hums {thing}. Listen, and it hums back.',
    'Before the first [ship], there was only [sky] and a very patient [dream].', 'This [planet] was a [gift] once. Now it is a gift shop.'.replace('[gift]', '[gold]'),
    'The oldest [map] shows a shop here. The shop is still here. The map is not.', 'A [wise] one once said: [trade] is just [love] with a receipt.', 'Every [star] was a [lost] wish. Every wish wanted {thing}.',
    'The first [coffee] was brewed by a [fool] and a [fire]. Both are legends.', 'We were [quiet] folk until someone invented [salt]. Then [loud].', 'The [moon] owes this place a [song]. It has not paid.',
    'In the beginning: [dark]. Then [light]. Then the snack aisle.', 'Our [ancient] law: always [welcome] the [lost], always overcharge the [rich].',
  ],
  dealer: [
    'No [gorcoin] on the receipt, no questions on the way out. {item}, {price}.', 'Only [gorcoin]. No [trade]s, no promises. {item}.', 'Pay in [gorcoin], leave with {item}. Simple [law].',
    'Psst. [Hello], [friend]. Got a {item}. {price} [gorcoin], no questions.', 'Hey. Hey. You. {item}. {price}. It is [legal]-ish.', 'Not from the store. Better. {item}, {price} [gorcoin], [secret] stash.',
    'I got what the clerk cannot sell: {item}. {price} [gorcoin].', '[Shh]. {item}. {price}. Tell no one, tell everyone.'.replace('[Shh]', '[Quiet]'), 'Looking for a [good] time? {item}, {price}. Warranty: [never].',
    'The [guard] does not see me. You do not see me. {item} for {price}.', 'Stuff you cannot [taste] in there: {item}. Only {price} [gorcoin].', 'This {item} is [rare], [weird], and [free]... of receipts. {price}.',
    'First one {price}. Second one is a [dream]. {item}.', 'Between you, me and the [moon]: {item}, {price}, [fresh] tonight.', '[Deal]? {item}, {price}. Say [yes] by blinking.'.replace('[yes]', '[good]'),
    'I know a [guy]. I am the [guy]. {item}, {price} [gorcoin].'.replace('[guy]', '[friend]').replace('[guy]', '[friend]'),
  ],
  cashier: [
    'We take [gorcoin]. Only [gorcoin]. Have you tried [gorcoin]?', 'Your [gorcoin] is [good] here. Your opinions are [free].', 'Parts aisle takes [gorcoin] too. Build something, [friend].', 'Missions pay [gorcoin]; I take it back at the register. Circle.',
    '{greet}. That will be {price} [gorcoin]. I do not make the rules.', 'Do you want a bag? It is [free]. It is also a bag.', 'Scan, pay, [thanks]. The sacred loop.', '{greet}! Find everything? Do not answer that.',
    '{price} [gorcoin]. Cash, card, or a [good] story.', 'Your total is {price}. Your change is a [dream].', 'Would you like to round up for [moon] charity? The [moon] is doing fine.',
    'Receipt? It says [thanks] in nine languages. Eight are fake.', 'Next! ...You are next. It was a joke. Nobody is here.', 'Sorry, the card machine is [sleep]ing. Cash, or [trade] me a [secret].',
    'That is {price} [gorcoin] for the {item}. Enjoy. Or do not. I am paid either way.', 'Hot dog on the roller has been there since the [ancient] days. Safe, I assume.', '{bye}. Mind the [door].',
    'Thank you for shopping the INTERGALACTIC. We [love] you. Legally required.',
  ],
  fries: [
    '[Fries]! [Hot], [salt]y, perfect. {price} [gorcoin].', 'The seasoning is a [secret]. It is [salt] and something [ancient].', 'One order of [fries]. Hot. [Fresh]. Does not glow, you do.',
    'We have only [fries]. We are [good] at one thing.', 'Seasoned [fries]. Light blue for forty-five minutes. Do not ask how.', '[Hello]! [Fries]? Of course [fries]. It is a window, not a menu.',
    'Fresh out of the oil. The window is [warm]. The [fries] are warmer.', '[Salt], [grease], [love]. That is the whole recipe. And one more [secret].', 'Same stand, same [fries], same window since the [ancient] days.',
    'They glow, [friend]. They actually glow. Do not drive.', '[Thanks]! Enjoy the [glow]. {bye}.', 'People cross the [void] for these. Do not [trust] the [map]; trust the smell.',
    '[Fries], [fresh] and [hot]. That is the pitch. That is the whole pitch.',
  ],
  mission: [
    'The board upstairs pays in [gorcoin]. Real [gorcoin]. Read it, [friend].', 'Missions? Station has them. {price} [gorcoin] if you do not die.', 'Work is [good] here: pick a job, send your [crew], collect [gorcoin].',
    'Mining job on the board. The [gorcoin] is [good]. The rocks are [rude].'.replace('[rude]', '[loud]'), 'Everyone on this [station] works for [gorcoin] or [fries]. Usually both.',
    'The mission board never sleeps. Neither do the [pirate]s. [Trade] accordingly.', 'Tell your [crew] to bring back [gorcoin], not feelings.', 'A bounty is just a [friend]ly [secret] with a price tag in [gorcoin].',
  ],
  station: [
    'Welcome aboard the [station]. Everything costs [gorcoin]. Even the air, in theory.', 'This [station] runs on [gorcoin], [coffee], and denial.', 'Dock fees are paid in [gorcoin]. Complaints are paid in [dust].',
    'The 7/11 takes [gorcoin]. The board gives [gorcoin]. Circle of [life].'.replace('[life]', '[love]'), 'Mind the [gorcoin] on the floor. It is probably someone\'s.', 'No [gorcoin], no [fries]. Station [law].',
  ],
  recruit: [
    'I work for [fries] and [gorcoin]. Mostly [fries].', 'Hire me. I am {adj}, I am [fast], I take only a small share of the [gorcoin].', 'Got a [ship]? Got [gorcoin]? Got room? I need all three.',
    'I am a {role}. Give me a bunk and one fry a day. The [gorcoin] is a bonus.', 'Take me with you. My last captain paid in exposure. And [fries].',
  ],
  shopper: [
    'Where do they keep the {thing}? I have looked {place}.', 'This aisle is {adj}. I [love] it.', 'I came for [coffee]. Now I own {thing}. Shopping is a river.', 'Do you think the {thing} is [fresh]? Asking for me.',
    'Excuse me, [friend]. Is the {thing} {adj} or just [weird]?', '[Hello]! I am only browsing. Browsing is my religion.', 'I have been in this aisle since [night]. Send help. Or {thing}.',
    'The prices here are {adj}. I said what I said.', 'Everything is [shiny] and I am [hungry]. Dangerous mix.', 'Every time I come here I forget what I wanted. Then I buy {thing}.', 'Have you tried the [sugar] aisle? [Joy].',
    'Shh. The clerk is watching. The clerk is always watching.', 'I only need one thing. It is {thing}. The aisle disagrees.',
  ],
};
const ROLE = {
  conspiracy: [
    'The [moon] is not a [moon]. Ask {who}. Ask no one.', 'They put [sugar] in the [water] and {thing} in the [sky]. Wake up.', 'Every shelf here is a [map]. Read them in order. I did. I regret it.',
    'The [guard] is a [fool]. The [fool] is a [guard]. Think about that.',
  ],
  pilot: [
    'I flew [ship]s before you were a [star]. Now I buy [food].', 'Retired. [Quiet] life. Loud knees.', 'Three tours past the [void]. The [coffee] here is still the [best] thing I found.',
    'Back in my day the [sky] had fewer lanes and more [fire].',
  ],
  kid: [
    'Look! A tiny [ship]! Fwoosh! It is [fast]. Mine.', 'My [ship] has lasers. Real ones. Mom says no. Mom is [wise].', 'Can I have {thing}? Please? Please? [Please].'.replace('[Please]', '[thanks]'),
    'I am [small] but my [ship] is [big]. In my head.',
  ],
  stationcop: [
    'Station [law]: no loitering, no flying indoors, no unlicensed [gorcoin].', 'Badge says [guard]. Pay says part-time. Move along, [friend].', 'Keep your [ship] on the pad and your hands on your [gorcoin].',
    'I have written eleven tickets today. Ten were for [fries].', 'Report suspicious [pirate]s to the board. It pays in [gorcoin].',
  ],
  tourist: [
    'Is this the real [station]? Where do I buy [gorcoin]? I only have [dust].', 'I got lost looking for the [door]. I think I am in a different [void].', 'Excuse me, [friend]: where is the [fries]? My guide book is [ancient].',
    'Everyone here is [strange]! I [love] it. How much is one [gorcoin]?', 'I took a wrong turn at the [moon]. Now I live here. It is fine.',
  ],
  cop: [
    'I see everything. I do nothing. It is a [good] arrangement.', 'Move along. Or do not. It is not in my job description.', '[Law] is a suggestion with a badge. I am the badge.',
    'Off-duty. Mostly. Just here for the [coffee] and the [sugar].',
  ],
};
const CTX_ALIAS = { greet: 'greeting', hello: 'greeting', npc: 'shopper' };

function expand(tpl, r, vars) {
  let guard = 0, s = tpl;
  while (/\{\w+\}/.test(s) && guard++ < 6) {
    s = s.replace(/\{(\w+)\}/g, (m, k) => {
      if (vars[k] != null) return vars[k];
      const a = SLOTS[k]; return a ? a[Math.floor(r() * a.length)] : m;
    });
  }
  return s;
}
function capital(s) { return s.charAt(0).toUpperCase() + s.slice(1); }

export function line(npc, ctx) {
  npc = npc || {}; const info = typeof ctx === 'object' && ctx ? ctx : { ctx };
  let c = String(info.ctx || 'greeting').toLowerCase(); c = CTX_ALIAS[c] || c;
  const pool = T[c] || T.greeting, seed = seedOf(npc.seed || 0);
  const n = npc._n = (npc._n | 0) + 1;
  const r = mulberry(seed ^ hashStr(c) ^ Math.imul(n, 0x9E3779B1) ^ hashStr(npc.name || npc.role || ''));
  let tpl = null;
  if (c === 'shopper' && npc.role && ROLE[npc.role] && r() < 0.65) tpl = ROLE[npc.role][Math.floor(r() * ROLE[npc.role].length)];
  if (!tpl) tpl = pool[Math.floor(r() * pool.length)];
  const vars = {
    role: info.role || npc.role || 'drifter', name: info.name || npc.name || name(seed),
    item: info.item ? (info.item.name || info.item) : 'mystery snack', price: info.price != null ? info.price : (info.item && info.item.price != null ? info.item.price : Math.round(5 + r() * 60)),
  };
  let s = expand(tpl, r, vars);
  const known = npc.known;
  s = s.replace(/\[([A-Za-z]+)\]/g, (m, w) => {
    const cap = w.charAt(0) !== w.charAt(0).toLowerCase(), key = w.toLowerCase();
    if (known && known.has(key)) return key === 'gorcoin' ? 'gorCoin' : (cap ? capital(key) : key);
    const a = word(seed, key);
    if (a === key && CONCEPTS.indexOf(key) < 0) return cap ? capital(key) : key;      // not a lexicon word: plain English
    return ALIEN_OPEN + (cap ? capital(a) : a) + ALIEN_CLOSE;
  });
  s = s.replace(/\s+/g, ' ').replace(/\s+([.,!?])/g, '$1').trim();
  return s.replace(/(^|[.!?]\s)(\u200B?)([a-z])/g, (m, a, b, ch) => a + b + ch.toUpperCase());
}

// the currency word for a speaker: 'gorCoin' when the listener knows it, else the seed's alien word (wrapped in markers). No `known` -> plain 'gorCoin'.
export function money(seed, known) {
  if (!known || known.has('gorcoin')) return 'gorCoin';
  return ALIEN_OPEN + word(seed || 0, 'gorcoin') + ALIEN_CLOSE;
}
// role flavours that exist: shopper|conspiracy|pilot|kid|cop|stationcop|tourist
export const ROLES = Object.keys(ROLE);
export function render(text, seed) {
  const out = []; text = String(text || '');
  const re = new RegExp(ALIEN_OPEN + '([^' + ALIEN_CLOSE + ']*)' + ALIEN_CLOSE, 'g');
  const lx = seed != null ? lexicon(seedOf(seed)) : null;
  let last = 0, m;
  while ((m = re.exec(text))) {
    if (m.index > last) out.push({ text: text.slice(last, m.index), alien: false });
    const seg = { text: m[1], alien: true };
    if (lx) { const cn = lx.rev.get(m[1].toLowerCase()); if (cn) seg.concept = cn; }
    out.push(seg); last = m.index + m[0].length;
  }
  if (last < text.length) out.push({ text: text.slice(last), alien: false });
  return out;
}
export function strip(text) { return String(text || '').split(ALIEN_OPEN).join('').split(ALIEN_CLOSE).join(''); }
export function teach(known, n, rng) {
  n = n == null ? 1 : n; const r = rng || Math.random, learned = [];
  if (!known) return learned;
  const left = CONCEPTS.filter((c) => !known.has(c));
  for (let i = 0; i < n && left.length; i++) { const k = Math.floor(r() * left.length), c = left.splice(k, 1)[0]; known.add(c); learned.push(c); }
  return learned;
}
export const TEMPLATE_COUNTS = Object.fromEntries(Object.keys(T).map((k) => [k, T[k].length]));

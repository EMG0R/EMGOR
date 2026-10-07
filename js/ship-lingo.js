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
  // rev 27 shoplifting: guard (copwarn | copchase | copbusted), clerk about the cart (cart), shoppers who saw something (shoplift)
  copwarn: [
    'Hey. [Friend]. I saw that. Put it back or [pay] for it.', 'Slow down, {name}. The shelf is not a [gift]. Eyes are on you.', 'That is not yours yet. I am watching your hands, [friend].',
    'One more and we have a [problem]. Station [law]. Keep your hands where I can see them.', 'I have a [good] view of aisle three. Think about that.',
  ],
  copchase: [
    'STOP right there! [Law]! Drop the {thing}!', 'Hey! HEY! You! Bring that back! I will [run]. I am slow but I [never] stop.', 'Halt, thief! This is a [law] zone!', 'Shoplifter! Somebody [stop] them! Not you, {name}. The other one.',
  ],
  copbusted: [
    'Got you. Hand it over. The fine is [gorcoin], the paperwork is forever.', 'That is the {adj}est shoplift I have ever stopped. Empty your pockets, [friend].', 'Busted. Items go back on the shelf. You pay the fine. I will [forget] the rest.',
    'Eleven tickets yesterday. You are twelve. [Thanks] for the [gift].',
  ],
  cart: [
    'You gonna pay for that? The [gorcoin] goes in the till, not the pocket.', 'Is that {thing} in your hands? That is a [buy], [friend]. Bring it to the counter.', 'I can see your cart from here. Pay up, then [fly].',
    'Lots of {thing} in your arms, {name}. Ring it up, or the guard rings you.', 'That is a full cart, [friend]. I accept [gorcoin]. I do not accept [fast] excuses.',
  ],
  shoplift: [
    'Did you see that? They took the {thing} and just... [walked]. Bold.', 'I did not see anything. I am a [quiet] shopper. I saw everything.', 'The guard is right there, [friend]. Maybe not that shelf.',
    '[Shh]. I am not judging. Okay, I am a little [impressed].'.replace('[Shh]', '[Quiet]'), 'Careful. The guard does a loop every minute. I timed it. I have no [life].',
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
  farmer: [
    'The [water] is late and the [sky] is early. Crops do not care.', 'Planted at [dawn]. Harvested never. Eaten by a [fool] with a [ship].', 'Good soil, [good] light, bad [night]s. The golum keeps the bad ones off.',
    'I talk to the plants. They are [quiet]. I respect that.', 'Another [day], another row. The [star] does half the work.',
  ],
  trader: [
    'Real [price]s. Village [price]s. Not station [price]s.', 'I [buy] crystals and [sell] oddities. Ask me about the oddities.', 'A [deal] is a handshake with [gorcoin] in it.', 'Everything on this stall was somebody\'s [secret]. Now it is [cheap].',
  ],
  elder: [
    'I have seen forty [night]s of this place. The golums were here first.', 'Sit. [Listen]. The [void] hums a different [song] over this village.', 'Young [friend]: [work] is the only [quiet] thing worth having.', 'My knees know the weather. My [dream]s know the rest.',
  ],
  guard: [
    'Golum says: [danger] stays outside the fence.', 'Do not hit the villagers. The golum has opinions.',
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

// ── seeded branching conversations (rev 26). No model calls; pure function of (npc.seed, state). ──
//   converse(npc, state) -> { node, text, segments, choices:[{id,label,next}], effects:{teach,gor,mood,item?,quest?,learn?}, done }
//   npc   = { seed, role, name?, known:Set, mood?:-2..2 }.   state = { next?: choice.next (omit to start), steps?, mood?, known?, item?, salt? }  (state is updated: node/steps/mood/lastAsk)
//   The caller applies effects (teach via lingo.teach / effects.learn, gor, mood, item offer, quest). npc._n is never touched, so a seed always replays the same.
import { RECIPES as CRAFT_RECIPES } from './ship-craft.js';
const CV_ALIAS = { cashier: 'clerk', store: 'clerk', cop: 'stationcop', retired: 'retired', 'retired pilot': 'retired', retiredpilot: 'retired', fries: 'fries', burger: 'fries' };
const CV_TOPICS = ['gossip', 'lore', 'trade', 'mission', 'personal'];
const CV_LABEL = {
  gossip: ['Heard anything lately?', 'Any gossip?', 'What is the word around here?', 'Got any rumors?'],
  lore: ['Tell me something old.', 'Any history around here?', 'What is the oldest story you know?', 'Got any lore?'],
  trade: ['What can I get?', 'Let us talk business.', 'Anything for sale?', 'What do you deal in?'],
  mission: ['Any work going?', 'Need a hand with something?', 'Got a job for me?', 'Anything need doing?'],
  personal: ['How are you, really?', 'What is your story?', 'What do you do out here?', 'Where are you from?'],
};
const CV_MORE = ['Go on.', 'Tell me more.', 'And then?', 'Keep going.'];
const CV_BYE = ['Thanks. Bye.', 'See you around.', 'I should go.', 'Safe skies.'];
const CV_RUDE = ['Whatever. Bye.', 'Boring. Leaving.', 'Not interested.'];
const CV = {
  greeting: ['{greet}! What brings you over?', '{greet}. You look like you have questions.', '{greet}, traveller. Talk or buy, both are fine.', '{greet}. I have a minute. Maybe two.', '{greet}! You again? New face, I mean. {name} here.',
    '{greet}, {name}. Mind the {adj} floor.', 'Oh! {greet}. I was just muttering about {thing}.', '{greet}. Pull up a [void] and sit.', '{greet}! Everyone here is {adj} today. Including you.', '{greet}. Careful, I talk a lot.', '{greet}, [friend]. Ask away.', '{greet}. The [day] is long, the chat is [free].'],
  gossip: ['They say {who} was seen {place}. {quip}', 'Word is the [toll] went up again. I blame {who}.', 'Somebody sold {thing} for double. {quip}', 'I heard {who} lost a [ship] in a card game.', 'Rumor: there is a [secret] door {place}. {quip}',
    'The [guard] was [fool]ed by {who}. Everyone saw.', 'Nobody talks about {place}. So everybody does.', 'Apparently {thing} is the new [gold]. {quip}', 'Between us, {who} owes me a [song]. {quip}', 'Heard the [law] is checking [gorcoin]s now. {quip}', 'Big [rumor] today: the [moon] is hollow. {quip}', 'My cousin says {who} sleeps {place}. {quip}'],
  lore: ['Long ago the [star]s were [quiet]. Then someone invented [fries].', 'The first [station] was built on a [dream] and a loan.', 'They say the [void] hums a [song] if you [listen] long enough.', 'An [ancient] [pirate] buried [gold] {place}. Or [salt]. Accounts differ.',
    'Before [gorcoin] there was [dust]. Before [dust], favors.', 'The [moon] was once a [planet]. It never forgave anyone.', 'Old [map]s show a [bright] star that is not there now.', 'The black hole is just the [void] with better PR.', 'Pilots say you can [taste] the [sky] before a storm.', 'Every [planet] hides one [rare] thing. Most hide [dust].', 'My grandmother [whisper]ed that [ice] remembers.', 'The shards are the [ancient] [song], cooled and cut.'],
  trade: ['Prices are {adj} today. {quip}', 'I can do you a [deal]. Not a [good] one. A [deal].', 'Everything is for [sell] except my [friend]s.', 'Supply is [small], demand is [big], my mood is both.', 'Ask me about {thing}. I mean it.',
    'For you, a [cheap] price. For them, the other price.', 'Rule one: never [buy] on an empty stomach.', 'Bring [gorcoin]. Bring more than you think.', 'The [best] [deal] is the one you walk away from. Then come back.', 'Stock is [fresh], conscience is not.', '[Trade] is just talking with receipts.', 'I do not haggle. I negotiate with feelings.'],
  mission: ['I need {thing} from {place}. Bring it back, and I will make it worth your while.', 'A small job: find the [lost] crate near {place}. Easy. Mostly.', 'Someone must [listen] to the [void] for me. It pays in [gorcoin].', 'Scout {place} and tell me if it is still there.',
    'Deliver this note to {who}. Do not read it. I will know.', 'There is a [pirate] problem. I have a [mission] and no [ship].', 'Bring me something [rare]. I will know it when I see it.', 'My [crew] quit. Be my crew. Just today.', 'I will give you a [map]. You give me a [good] story.', 'Fetch [water] from a [ice] moon. Do not ask.', 'It is a [work] job, honest, mostly.', 'The [station] board is full. I have the unlisted one.'],
  personal: ['Me? I came for a day. That was years ago.', 'I miss [home]. The [home] I picture, anyway.', 'I like [quiet] [night]s and loud [coffee].', 'Honestly? Some days I [fear] the [void]. Today I [love] it.',
    'I am saving for a [ship]. Prices keep [fly]ing away.', 'My [dream]? A [small] place with a [big] window.', 'I never learned to [dance]. Do not tell anyone.', 'I have been here since the [door] was new.', 'People ask where I am from. I say [far].'.replace('[far]', '[home]'), 'I talk to strangers because [quiet] gets loud.', 'I collect [song]s. Hum me one sometime.', 'Being kind costs nothing. Everything else here does.'],
  goodbye: ['{bye}. Come again.', '{bye}! Mind the [door].', 'Off you go, then. {bye}.', '{bye}, [friend]. Keep your [wallet] close.', 'Already? Fine. {bye}.', '{bye}. I was enjoying that.', 'Go. Explore. {bye}.', '{bye}! Tell them {name} sent you.', '{bye}. Stay [warm].', '{bye}. I will be right here. I am always right here.', 'See you [soon], or [never]. {quip}', '{bye}. Do not forget {thing}.'],
  rude: ['Hmph. {bye}, I suppose.', 'Rude. Fine. Go.', 'Door is that way. It has opinions about you.', 'Wow. Okay. [Goodbye]-ish.'.replace('[Goodbye]', '[Thanks]'), 'Noted. Mood: [hate].', 'Sure. Leave. See if I care. I care a bit.', 'The [fool] leaves. Of course.', 'Fine. Come back with manners.', 'You too, I guess.', 'Leaving mid-sentence. Bold.'],
  dealer: ['Menu today: {item}, {price} [gorcoin]. Look no further.', 'Hot off the [fire]: {item}. {price}. Do not ask where.', 'Psst. {item}. {price}. You did not see me.', 'Best in the [void]: {item}. {price} and it is yours.', '{item}, {price}. I would [buy] it myself, if I were you.',
    'Special for you, [friend]: {item} at {price}.', 'It is {item} or nothing. {price}, no refunds.', 'I got {item}. It is [rare]. It is {price}. Both facts are true.', 'Try {item}. {price}. Taste of [danger].', 'Shiny {item}, {price}. One careful owner.', 'Look: {item}. {price}. Blink and it is gone.', 'You want {item}? It wants you. {price}.'],
  craft: ['Pro tip: mix things and see what happens. Try making {recipe}.', 'Crafting hint: {recipe} is a good one. Check the 3x3 grid.', 'Ever made {recipe}? Lay the parts out on the grid and see.', 'Between us: the grid knows how to build {recipe}.', 'I once built {recipe} out of leftovers. Try it.',
    'Stack your spare bits. {recipe} is hiding in there somewhere.', 'Hint: {recipe}. Parts on the grid, shapes matter.', 'The [best] hobby: making {recipe} from trash.', 'Bring me nothing, make {recipe}. It works.', 'Not sure how, but {recipe} is craftable. Experiment.', 'The grid loves {recipe}. Feed it.', 'Someone showed me {recipe} once. I still think about it.'],
  wordask: ['"{w}" means "{c}". Now you owe me one.', '{w}? That is "{c}". Easy once you hear it.', 'Ha. "{w}" is "{c}", [friend].', 'It is just "{c}". Say it with an accent.', 'Oh, {w}. "{c}". I say it forty times a day.', '"{c}". That is all. {w} is "{c}".', '{w} means "{c}". Do not forget it.', 'Good ear. "{w}" = "{c}".', 'Hm, "{w}"? "{c}". Use it well.', 'Careful with {w}. It is "{c}" and it bites.'],
};
const CV_ROLE = {
  dealer: { personal: ['Dealing is honest work. Mostly.', 'My stock moves faster than my conscience.', 'Cheap is a feeling, not a price.'], trade: ['My menu changes by the hour. Keep up.'] },
  clerk: { personal: ['The register and I have an understanding.', 'Eight hours of [fries] smell. I miss my nose.', 'I know everyone by what they buy.'], trade: ['Scan, bag, smile. In that order.'] },
  fries: { personal: ['I have not seen daylight since the [salt] shipment.', 'The [grease] is old. The recipe is older.', 'Fries are not food. They are a promise.'], trade: ['One bag, two bags. Never zero.'] },
  stationcop: { personal: ['I wanted to be a pilot. I got a badge.', 'It is mostly paperwork and [coffee].', 'Do not run in the [station]. I will chase you, slowly.'], lore: ['Station [law] was written on a napkin. We still have the napkin.'] },
  tourist: { personal: ['I came for the [fries]. Staying for the [fries].', 'I am on a tour. The guide left. The tour continued.', 'Where I come from, [door]s do not talk.'], gossip: ['A local told me this is a pirate cafe. Is it?'] },
  pilot: { personal: ['I am looking for a [ship] that will take me.', 'Fresh off the academy. Nervous. Eager.', 'My last [ship] is still in orbit. Somewhere.'], mission: ['I need a co-pilot for one quiet run.'] },
  retired: { personal: ['Retired. [Quiet] life. Loud knees.', 'I flew when the [sky] had fewer lanes.', 'I miss the [void]. Not the paperwork.'], lore: ['Three tours past the rim, and the [coffee] here is still the [best] thing I found.'] },
  miner: { personal: ['Twenty years of [dust]. I love it. My lungs do not.', 'Rocks are honest. People are not.', 'Dig deep, [sell] high.'], mission: ['I need [ice] and [gold]. A haul pays well.'] },
  chef: { personal: ['I cook what [hungry] people deserve.', 'A pinch of [salt], a pinch of [danger].', 'The perfect [fries] is a rumor.'], trade: ['I trade recipes for [rare] spices.'] },
  scout: { personal: ['I map what is not on the [map].', 'First one in, last one to complain.', 'I sleep under [star]s. Mostly.'], lore: ['I found a [bright] ruin once. It was gone by morning.'] },
  conspiracy: { gossip: ['The shelves are [map]s. They always were.', 'Nobody can prove the [moon] exists.'], lore: ['Wake up. The [void] is a ceiling.'] },
  farmer: { personal: ['Dirt under my nails, [sky] over my head.', 'I am growing something [rare]. Do not step on it.', 'The [night] mobs hate the golum. So do I, a little. It never talks.'], trade: ['Ask the trader. I only grow things.'], gossip: ['The elder rings the bell when something is wrong.'] },
  trader: { personal: ['My stall is my whole [home].', 'Small village, honest [price]s. Mostly.', 'I trade [crystal]s for things from far away.'], trade: ['Six deals a day. All seeded. All fair-ish.', 'Bring me [crystal]s, [ore], [ice]. I pay above the station.'], lore: ['The golums were built by the first settlers. Nobody remembers how.'] },
  elder: { personal: ['I keep the [bell]. When it rings, hide.', 'I am old. The golum is older.'], mission: ['The village always needs a [good] pair of hands. Yours, maybe.', 'Work is posted. Take what you can carry.'], lore: ['The huts will outlive us all. They cannot be broken. Do not try.', 'The first golum stood up out of the [dust] and never sat down.'], gossip: ['Hit one of mine and the golum learns your name.'] },
  kid: { personal: ['I am [small] but I am going to be [big].', 'Mom says do not talk to strangers. You are fine.', 'I am building a [ship] from boxes.'], mission: ['Find my toy [ship]! It is [lost] under the aisle.'] },
  shopper: {},
};
const cvH = (s) => hashStr(s);
function cvRole(npc) { let r = String(npc.role || 'shopper').toLowerCase(); r = CV_ALIAS[r] || r; if (r === 'pilot' && npc.active) return 'pilot'; if (r === 'pilot') return 'retired'; return CV_ROLE[r] || r === 'retired' ? r : 'shopper'; }
function cvSpeak(npc, tpl, r, vars, pTrans) {
  const seed = seedOf(npc.seed || 0), known = npc.known;
  let s = expand(tpl, r, vars);
  s = s.replace(/\[([A-Za-z]+)\]/g, (m, w) => {
    const cap = w.charAt(0) !== w.charAt(0).toLowerCase(), key = w.toLowerCase();
    if ((known && known.has(key)) || (CONCEPTS.indexOf(key) >= 0 && r() < pTrans)) return key === 'gorcoin' ? 'gorCoin' : (cap ? capital(key) : key);
    const a = word(seed, key);
    if (a === key && CONCEPTS.indexOf(key) < 0) return cap ? capital(key) : key;
    return ALIEN_OPEN + (cap ? capital(a) : a) + ALIEN_CLOSE;
  });
  s = s.replace(/\s+/g, ' ').replace(/\s+([.,!?])/g, '$1').trim();
  return s.replace(/(^|[.!?]\s)(​?)([a-z])/g, (m, a, b, ch) => a + b + ch.toUpperCase());
}
export function converse(npc, state) {
  npc = npc || {}; state = state || {};
  const seed = seedOf(npc.seed || 0), role = cvRole(npc), known = state.known || npc.known || new Set();
  const mood = Math.max(-2, Math.min(2, Math.round(state.mood != null ? state.mood : (npc.mood != null ? npc.mood : 0))));
  const nxt = state.next || 'greeting', parts = String(nxt).split(':'), id = parts[0];
  const steps = state.steps = (state.steps | 0) + 1;
  const r = mulberry(seed ^ cvH('cv:' + nxt) ^ Math.imul(steps, 0x9E3779B1) ^ cvH(npc.name || role) ^ cvH(String(state.salt || '')));
  const pTrans = Math.min(0.55, (known.size / CONCEPTS.length) * 0.9 + (mood > 0 ? 0.08 * mood : 0));
  const stage2 = /2$/.test(id), topic = id.replace(/2$/, '');
  const nm = npc.name || name(seed);
  const vars = { name: nm, role, item: state.item ? (state.item.name || state.item) : 'mystery snack', price: state.item && state.item.price != null ? state.item.price : Math.round(5 + r() * 60) };
  const effects = { teach: 0, gor: 0, mood: 0 };
  let kind = topic, extra = '', done = false, recipe = null, wv = null;
  const pool = (k) => (CV[k] || CV.gossip).concat((CV_ROLE[role] && CV_ROLE[role][k]) || []);
  if (id === 'wordask') { kind = 'wordask'; const c = parts[1] || 'hello'; wv = { w: ALIEN_OPEN + word(seed, c) + ALIEN_CLOSE, c }; effects.teach = 1; effects.learn = [c]; effects.mood = 1; (state.asked || (state.asked = new Set())).add(c); }
  else if (id === 'goodbye') { kind = parts[1] === 'rude' ? 'rude' : 'goodbye'; done = true; if (kind === 'rude') effects.mood = -1; else if (mood >= 1) effects.gor = 2; }
  else if (id === 'trade' || id === 'trade2') {
    if (role === 'dealer' || role === 'fries') { if (stage2 || r() < 0.0) kind = 'trade'; else kind = 'dealer'; effects.item = 'offer'; if (stage2) kind = 'dealer'; }
    else if (role === 'clerk') { kind = stage2 ? 'craft' : 'trade'; }
  }
  if (kind === 'craft') { const rs = (CRAFT_RECIPES || []).filter((q) => q && q.name); recipe = rs.length ? rs[Math.floor(r() * rs.length)].name : 'something'; vars.recipe = recipe; effects.craft = recipe; }
  if (stage2 && topic === 'lore') { effects.teach = 1; }
  if (stage2 && topic === 'mission') { if (mood >= 0) { effects.quest = true; effects.gor = 5; } else { effects.mood = 0; } }
  if (stage2 && topic === 'personal') { effects.mood = 1; if (mood >= 1) effects.gor = 3; }
  if (stage2 && topic === 'gossip' && mood >= 1) effects.teach = 1;
  if (id === 'greeting') effects.mood = 0;
  const mainPool = pool(kind);
  const tpl = (kind === 'wordask') ? CV.wordask[Math.floor(r() * CV.wordask.length)] : mainPool[Math.floor(r() * mainPool.length)];
  if (wv) { vars.w = wv.w; vars.c = wv.c; }
  let text = cvSpeak(npc, tpl, r, vars, pTrans);
  if (mood <= -1 && id !== 'greeting' && !done && r() < 0.6) text = 'Hmph. ' + text;
  if (id === 'greeting' && mood <= -1) text = cvSpeak(npc, '{greet}. Make it quick.', r, vars, pTrans);
  const segments = render(text, seed);
  // choices
  const choices = [];
  const avail = CV_TOPICS.filter((t) => {
    if (mood <= -1 && (t === 'personal' || t === 'mission')) return false;
    if (t === 'lore' && known.size < 3 && mood < 1) return false;
    if (t === 'mission' && known.size < 2 && mood < 1) return false;
    if (t === 'personal' && mood < 0 && r() < 0.5) return false;
    return true;
  });
  const shuffled = avail.slice().sort(() => r() - 0.5);
  if ((role === 'dealer' || role === 'clerk' || role === 'fries') && id === 'greeting') { const i = shuffled.indexOf('trade'); if (i > 0) { shuffled.splice(i, 1); shuffled.unshift('trade'); } }
  const pickLabel = (arr) => arr[Math.floor(r() * arr.length)];
  const leave = () => ({ id: 'leave', label: pickLabel(mood <= -1 || r() < 0.2 ? CV_RUDE : CV_BYE), next: (mood <= -1 || r() < 0.2) ? 'goodbye:rude' : 'goodbye' });
  const lastLeave = { id: 'leave', label: pickLabel(CV_BYE), next: 'goodbye' };
  if (done) { /* terminal */ }
  else if (steps >= 5) choices.push(lastLeave);
  else {
    const asked = state.asked || (state.asked = new Set());
    const alienSeg = segments.filter((sg) => sg.alien && sg.concept && !known.has(sg.concept) && !asked.has(sg.concept));
    if (id === 'greeting') {
      for (const t of shuffled.slice(0, mood >= 1 ? 2 : 2)) choices.push({ id: t, label: pickLabel(CV_LABEL[t]), next: t });
      if (mood >= 2 && shuffled[2]) choices.push({ id: shuffled[2], label: pickLabel(CV_LABEL[shuffled[2]]), next: shuffled[2] });
      else choices.push(leave());
    } else {
      if (!stage2 && TOPIC_DEEP.has(topic) && id !== 'wordask') choices.push({ id: topic + '2', label: pickLabel(CV_MORE), next: topic + '2' });
      const other = shuffled.find((t) => t !== topic);
      if (alienSeg.length && r() < 0.7 && choices.length < 2) { const sg = alienSeg[Math.floor(r() * alienSeg.length)]; choices.push({ id: 'ask', label: 'What does "' + sg.text + '" mean?', next: 'wordask:' + sg.concept }); }
      if (other && choices.length < 2) choices.push({ id: other, label: pickLabel(CV_LABEL[other]), next: other });
      if (!choices.length && shuffled[0]) choices.push({ id: shuffled[0], label: pickLabel(CV_LABEL[shuffled[0]]), next: shuffled[0] });
      choices.push(leave());
    }
  }
  if (!done && !choices.length) choices.push(lastLeave);
  state.node = id; state.mood = mood + (effects.mood || 0);
  return { node: nxt, text, segments, choices: choices.slice(0, 3), effects, done };
}
const TOPIC_DEEP = new Set(['gossip', 'lore', 'trade', 'mission', 'personal']);
export const CONVERSE_ROLES = ['shopper', 'clerk', 'dealer', 'stationcop', 'tourist', 'pilot', 'miner', 'chef', 'scout', 'conspiracy', 'kid', 'retired', 'fries'];

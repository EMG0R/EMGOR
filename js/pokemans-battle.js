// pokemans-battle.js — the one catch path: a classic pixel turn-based battle (FIGHT / BALL / RUN) against a wild POKEMANS creature.
//   Pure core (node-testable, rng injected): battleStats, moveList, calcDamage, turnOrder, runChance, foeMove, levelUp, createBattleCore
//   DOM/canvas overlay (no Three.js): startBattle(opts) -> ctrl { core, act, fight, ball, run, close, done }
// Creature sprites: the real creature rig drawn by ps.creatSprite (ship-planet.js, one orthographic render, nearest-filtered target) and quantised to 6 violet-tinted
// shades here, cached per species+variant+view for the session. "YOU" keeps the GOR BRAWL humanoid (smash-sprites.js atlas).
import { statsAt, xpForLevel, MAX_LEVEL } from './pokemans-gen.js';
import { battleCatchChance, LURE_BONUS } from './pokemans-catch.js';
import { PLAN_BASE, TYPE_COL } from './pokemans-fighter.js';
import { buildAtlas } from './smash-sprites.js';

export const PLAN_V = { skimmer: 0, crawler: 2, floater: 1, strider: 3, burrower: 2, swarmer: 0 };      // species body plan -> creature rig variant
export const LEVEL_CAP = 30, CRIT_DEN = 16, ROLL_MIN = 0.85, FLEE_TURNS = 3, WIN_HP_K = 1.08, WIN_POW_K = 1.04;
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);

// ── pure ──────────────────────────────────────────────────────────────────────
// stats at the mon's level, plus the battle-level bonus (mon.bl wins: HP x1.08, atk/sp x1.04 each)
export function battleStats(mon) {
  const s = statsAt(mon.sp, mon.ind, mon.level), bl = mon.bl | 0, o = { hp: s.hp, atk: s.atk, def: s.def, spd: s.spd, sp: s.sp };
  if (bl > 0) { o.hp = Math.round(s.hp * Math.pow(WIN_HP_K, bl)); o.atk = Math.round(s.atk * Math.pow(WIN_POW_K, bl)); o.sp = Math.round(s.sp * Math.pow(WIN_POW_K, bl)); }
  return o;
}
export function moveList(mon) {
  const out = [];
  for (let i = 0; i < 4; i++) { const m = (mon.learned && mon.learned[i]) || mon.sp.moves[i]; if (m) out.push({ name: String(m.name), type: m.type, power: m.power | 0 || 40, accuracy: m.accuracy | 0 || 100 }); }
  return out;
}
export const YOU_MOVES = [{ name: 'JET', type: 'spark', power: 55, accuracy: 100 }, { name: 'PUNCH', type: 'stone', power: 45, accuracy: 100 }, { name: 'DIVE', type: 'void', power: 70, accuracy: 85 }];
export function youFighter() {
  return { name: 'YOU', level: 10, types: [], st: { hp: 70, atk: 46, def: 40, spd: 40, sp: 46 }, hp: 70, max: 70, moves: YOU_MOVES.map((m) => Object.assign({}, m)), mon: null, fid: 'pilot', hue: -1, key: 'you', v: -1 };
}
export function fighterOf(mon) {
  const st = battleStats(mon), hp = mon.dead || !(mon.hp > 0) ? st.hp : Math.min(mon.hp, st.hp);
  return { name: String(mon.sp.name).toUpperCase(), level: mon.level | 0 || 1, types: mon.sp.types.slice(), st, hp, max: st.hp, moves: moveList(mon), mon, key: mon.sp.id + ':' + mon.sp.tier, v: PLAN_V[mon.sp.bodyRecipe.plan] | 0, fid: PLAN_BASE[mon.sp.bodyRecipe.plan] || 'twin', hue: ((mon.sp.bodyRecipe.hue + (mon.ind && mon.ind.hueFlip ? 180 : 0)) % 360 + 360) % 360 };
}
// Gen-1 style: ((2L/5+2) * P * A/D)/50 + 2, x STAB 1.5, x type chart (2 / 0.5 / 1), x roll 0.85..1.0, x crit 2 at 1/16. roll01 / critU in [0,1).
export function calcDamage(att, mv, def, roll01, critU, chart) {
  const stab = att.types.indexOf(mv.type) >= 0, A = stab ? att.st.sp : att.st.atk, D = Math.max(5, def.st.def);
  let mult = 1; if (chart) for (let i = 0; i < def.types.length; i++) mult *= chart.mult(mv.type, def.types[i]);
  const crit = critU < 1 / CRIT_DEN, base = Math.floor(Math.floor((2 * att.level / 5 + 2) * mv.power * A / D) / 50) + 2;
  const dmg = Math.max(1, Math.floor(base * (stab ? 1.5 : 1) * mult * (ROLL_MIN + (1 - ROLL_MIN) * clamp(roll01, 0, 0.999999)) * (crit ? 2 : 1)));
  return { dmg, crit, mult, stab };
}
export const turnOrder = (spdMe, spdFoe, u) => (spdMe > spdFoe ? 'me' : spdMe < spdFoe ? 'foe' : (u < 0.5 ? 'me' : 'foe'));
export const runChance = (spdMe, spdFoe, tries) => (spdMe >= spdFoe ? 1 : clamp(0.8 * spdMe / Math.max(1, spdFoe) + 0.1 * (tries | 0), 0.1, 1));
export function foeMove(moves, u) {      // weighted by power x accuracy
  let tot = 0; for (let i = 0; i < moves.length; i++) tot += moves[i].power * moves[i].accuracy / 100;
  let x = clamp(u, 0, 0.999999) * tot; for (let i = 0; i < moves.length; i++) { x -= moves[i].power * moves[i].accuracy / 100; if (x < 0) return i; }
  return Math.max(0, moves.length - 1);
}
// a win levels the party fighter: +1 level (cap 30), one more battle-level (HP +8 %, power +4 %); returns true if it levelled
export function levelUp(mon) {
  if (!mon || (mon.level | 0) >= LEVEL_CAP) return false;
  mon.level = Math.min(MAX_LEVEL, (mon.level | 0) + 1); mon.xp = Math.max(mon.xp | 0, xpForLevel(mon.level)); mon.bl = (mon.bl | 0) + 1;
  mon.hp = battleStats(mon).hp; return true;
}

// opts: { mine: fighter, wild: fighter, chart, rng, catchU, balls: { tier(), use(t), n() }, bonus() }  events: msg / anim / hp / ball / end
export function createBattleCore(o) {
  const me = o.mine, foe = o.wild, rng = o.rng || Math.random, catchU = o.catchU || rng, balls = o.balls || { tier: () => 0, use() {}, n: () => 0 };
  const C = { me, foe, over: false, result: '', idle: 0, tries: 0, turn: 0, ev: null };
  const msg = (s) => C.ev.push({ k: 'msg', s });
  const end = (r) => { C.over = true; C.result = r; C.ev.push({ k: 'end', r }); };
  C.ballOdds = () => { const t = balls.tier(); return t ? { tier: t, pct: Math.round(battleCatchChance(t, foe.hp, foe.max, o.bonus ? o.bonus() : 0) * 100), n: balls.n() } : { tier: 0, pct: 0, n: 0 }; };
  function strike(who, mv) {
    const att = who === 'me' ? me : foe, def = who === 'me' ? foe : me, dside = who === 'me' ? 'foe' : 'me';
    msg((who === 'foe' ? 'Wild ' : '') + att.name + ' used ' + mv.name + '!'); C.ev.push({ k: 'anim', side: who, fx: 'atk' });
    if (rng() * 100 >= mv.accuracy) { msg('It missed!'); return false; }
    const d = calcDamage(att, mv, def, rng(), rng(), o.chart);
    def.hp = Math.max(0, def.hp - d.dmg); C.ev.push({ k: 'anim', side: dside, fx: 'hurt' }); C.ev.push({ k: 'hp', side: dside, hp: def.hp, max: def.max, dmg: d.dmg });
    if (d.crit) msg('A critical hit!'); if (d.mult > 1) msg("It's super effective!"); else if (d.mult < 1) msg("It's not very effective...");
    if (def.hp <= 0) {
      if (who === 'me') { msg('Wild ' + foe.name + ' fainted!'); if (me.mon && levelUp(me.mon)) { msg(me.name + ' grew to Lv ' + me.mon.level + '!'); me.level = me.mon.level; } end('win'); }
      else { msg(me.name + ' blacked out...'); end('lose'); }
    }
    return who === 'me';
  }
  C.act = function (a) {
    C.ev = []; if (C.over) return C.ev;
    let dealt = false; C.turn++;
    if (a.k === 'fight') {
      const mv = me.moves[a.i | 0]; if (!mv) return C.ev;
      const seq = turnOrder(me.st.spd, foe.st.spd, rng()) === 'me' ? ['me', 'foe'] : ['foe', 'me'];
      for (let i = 0; i < 2 && !C.over; i++) { if (seq[i] === 'me') dealt = strike('me', mv) || dealt; else strike('foe', foe.moves[foeMove(foe.moves, rng())]); }
    } else if (a.k === 'ball') {
      const t = balls.tier(); if (!t) { msg('No GOR BALLS!'); C.turn--; return C.ev; }
      balls.use(t); const pct = battleCatchChance(t, foe.hp, foe.max, o.bonus ? o.bonus() : 0), caught = catchU() < pct, shakes = caught ? 3 : 1 + Math.floor(rng() * 3);
      msg('You threw a GOR BALL!'); C.ev.push({ k: 'ball', tier: t, shakes, caught, pct });
      if (caught) { msg('Gotcha! ' + foe.name + ' was caught!'); end('caught'); } else { msg('It broke free!'); strike('foe', foe.moves[foeMove(foe.moves, rng())]); }
    } else if (a.k === 'run') {
      C.tries++;
      if (rng() < runChance(me.st.spd, foe.st.spd, C.tries - 1)) { msg('Got away safely!'); end('run'); }
      else { msg("Can't escape!"); strike('foe', foe.moves[foeMove(foe.moves, rng())]); }
    }
    if (!C.over) { C.idle = dealt ? 0 : C.idle + 1; if (C.idle >= FLEE_TURNS) { msg('Wild ' + foe.name + ' fled!'); end('fled'); } }
    return C.ev;
  };
  return C;
}

// ── sprites ───────────────────────────────────────────────────────────────────
const atlases = {};
function hsl(h, s, l) { const k = (n) => (n + h / 30) % 12, a = s * Math.min(l, 1 - l), f = (n) => l - a * Math.max(-1, Math.min(k(n) - 3, 9 - k(n), 1)); return [f(0) * 255, f(8) * 255, f(4) * 255]; }
// paint a 96x96 pixel sprite: the rig's idle frame at 2x nearest, recoloured to `hue` (>=0) in 4 flat tones mixed 12 % toward HUD violet
export function drawSprite(cv, fid, hue) {
  cv.width = 96; cv.height = 96;
  const at = atlases[fid] || (atlases[fid] = buildAtlas([fid])), f = at.frame(fid, 'idle', 0), ctx = cv.getContext('2d');
  ctx.imageSmoothingEnabled = false; ctx.clearRect(0, 0, 96, 96);
  ctx.drawImage(at.canvas, f[0], f[1], at.W, at.H, 16, 0, at.W * 2, at.H * 2);
  if (!(hue >= 0)) return cv;
  const im = ctx.getImageData(0, 0, 96, 96), d = im.data;
  for (let i = 0; i < d.length; i += 4) {
    if (!d[i + 3]) continue; d[i + 3] = 255;
    if (d[i] === 0x1a && d[i + 1] === 0x12 && d[i + 2] === 0x26) continue;
    const q = Math.round((0.3 * d[i] + 0.59 * d[i + 1] + 0.11 * d[i + 2]) / 255 * 4) / 4, c = hsl(hue, 0.55, 0.18 + q * 0.55);
    d[i] = c[0] * 0.88 + 168 * 0.12; d[i + 1] = c[1] * 0.88 + 85 * 0.12; d[i + 2] = c[2] * 0.88 + 247 * 0.12;
  }
  ctx.putImageData(im, 0, 0); return cv;
}

const sprCache = new Map();
// raw RGBA from the rig render -> 6 flat shades of the species hue mixed toward HUD violet + a 1px dark outline
export function quantise(raw, hue) {
  const px = raw.w, cv = document.createElement('canvas'); cv.width = px; cv.height = px;
  const ctx = cv.getContext('2d'), im = ctx.createImageData(px, px), d = im.data, src = raw.data; let mx = 1e-3;
  for (let i = 0; i < src.length; i += 4) if (src[i + 3] > 127) mx = Math.max(mx, (0.3 * src[i] + 0.59 * src[i + 1] + 0.11 * src[i + 2]) / 255);
  const ramp = []; for (let q = 0; q < 6; q++) { const c = hsl(hue, 0.5, 0.14 + q * 0.1); ramp.push([c[0] * 0.8 + 168 * 0.2, c[1] * 0.8 + 85 * 0.2, c[2] * 0.8 + 247 * 0.2]); }
  const solid = (x, y) => x >= 0 && y >= 0 && x < px && y < px && src[(y * px + x) * 4 + 3] > 127;
  for (let y = 0; y < px; y++) for (let x = 0; x < px; x++) {
    const o = (y * px + x) * 4;
    if (solid(x, y)) { const q = Math.min(5, Math.floor(clamp((0.3 * src[o] + 0.59 * src[o + 1] + 0.11 * src[o + 2]) / 255 / mx, 0, 1) * 5.999)), c = ramp[q]; d[o] = c[0]; d[o + 1] = c[1]; d[o + 2] = c[2]; d[o + 3] = 255; }
    else if (solid(x - 1, y) || solid(x + 1, y) || solid(x, y - 1) || solid(x, y + 1)) { d[o] = 0x1a; d[o + 1] = 0x12; d[o + 2] = 0x26; d[o + 3] = 255; }
  }
  ctx.putImageData(im, 0, 0); return cv;
}
export function clearSprites() { sprCache.clear(); }
if (typeof window !== 'undefined') window.addEventListener('pagehide', clearSprites);
// paint fighter f into canvas cv: creature rig via o.sprite({v, rgb, view, px}) (cached), else the humanoid fallback
function paintFighter(cv, f, view, o) {
  if (f.hue >= 0 && o.sprite) {
    const key = f.key + ':' + f.v + ':' + view + ':' + Math.round(f.hue);
    let c = sprCache.get(key);
    if (!c) { try { const raw = o.sprite({ v: f.v, rgb: hsl(f.hue, 0.55, 0.55).map((n) => n / 255), view, px: view === 'back' ? 128 : 96 }); if (raw) { c = quantise(raw, f.hue); sprCache.set(key, c); } } catch (e) { console.warn('[battle] sprite', e); } }
    if (c) { cv.width = c.width; cv.height = c.height; const x = cv.getContext('2d'); x.imageSmoothingEnabled = false; x.drawImage(c, 0, 0); return; }
  }
  drawSprite(cv, f.fid, f.hue);
}

// ── overlay ───────────────────────────────────────────────────────────────────
const BALL_COL = [0xFF5C7A, 0x5CE8FF, 0xFFD25C];
const hex = (n) => '#' + n.toString(16).padStart(6, '0');
export function startBattle(o) {
  const doc = document, core = createBattleCore(o), speed = o.instant ? 0 : 1, rng = o.rng || Math.random;
  const root = doc.createElement('div'); root.id = 'pkb'; root.className = 'pkb';
  root.innerHTML = '<div class="pkb-screen"><div class="pkb-bg"></div><div class="pkb-pad pkb-pad-foe"></div><div class="pkb-pad pkb-pad-me"></div>' +
    '<canvas class="pkb-spr pkb-foe"></canvas><canvas class="pkb-spr pkb-me"></canvas>' +
    '<div class="pkb-box pkb-box-foe gor-panel"><b class="pkb-n"></b><i class="pkb-l"></i><div class="pkb-hp"><u></u></div></div>' +
    '<div class="pkb-box pkb-box-me gor-panel"><b class="pkb-n"></b><i class="pkb-l"></i><div class="pkb-hp"><u></u></div><span class="pkb-hpn"></span></div>' +
    '<div class="pkb-ball"></div><div class="pkb-wipe"></div></div>' +
    '<div class="pkb-ui gor-panel"><div class="pkb-msg"></div><div class="pkb-menu"></div></div>';
  const $ = (s) => root.querySelector(s), screen = $('.pkb-screen'), msgEl = $('.pkb-msg'), menu = $('.pkb-menu'), ballEl = $('.pkb-ball');
  const sprFoe = $('.pkb-foe'), sprMe = $('.pkb-me'), bFoe = $('.pkb-box-foe'), bMe = $('.pkb-box-me');
  paintFighter(sprFoe, core.foe, 'side', o); paintFighter(sprMe, core.me, 'back', o);
  let mode = 'busy', sel = 0, closed = false, resolveDone, btns = [];
  const done = new Promise((r) => { resolveDone = r; });
  const wait = (ms) => new Promise((r) => (speed ? setTimeout(r, ms * speed) : r()));
  function setBox(box, f, hp) {
    box.querySelector('.pkb-n').textContent = f.name; box.querySelector('.pkb-l').textContent = 'Lv' + f.level;
    const u = box.querySelector('u'), fr = clamp(hp / f.max, 0, 1); u.style.width = (fr * 100) + '%'; u.className = fr < 0.2 ? 'is-low' : fr < 0.5 ? 'is-mid' : '';
    const n = box.querySelector('.pkb-hpn'); if (n) n.textContent = hp + ' / ' + f.max;
  }
  setBox(bFoe, core.foe, core.foe.hp); setBox(bMe, core.me, core.me.hp);
  function render() {
    menu.textContent = ''; btns = [];
    const items = mode === 'main' ? (() => { const b = core.ballOdds(); return [{ t: 'FIGHT', f: () => { mode = 'fight'; sel = 0; render(); } }, { t: b.tier ? 'BALL ' + b.pct + '% x' + b.n : 'BALL --', c: 'is-ball', f: () => run({ k: 'ball' }) }, { t: 'RUN', f: () => run({ k: 'run' }) }]; })()
      : mode === 'fight' ? core.me.moves.map((m, i) => ({ t: m.name.toUpperCase() + ' ' + m.type + ' ' + m.power, c: 't-' + m.type, f: () => run({ k: 'fight', i }) })).concat([{ t: 'BACK', f: () => { mode = 'main'; sel = 0; render(); } }]) : [];
    sel = clamp(sel, 0, Math.max(0, items.length - 1));
    items.forEach((it, i) => { const b = doc.createElement('button'); b.type = 'button'; b.textContent = it.t; b.className = 'pkb-b ' + (it.c || '') + (i === sel ? ' is-sel' : ''); b.onclick = it.f; b.onmouseenter = () => { sel = i; paint(); }; menu.appendChild(b); btns.push(b); b._f = it.f; });
    menu.className = 'pkb-menu m-' + mode;
    if (mode === 'main') msgEl.textContent = 'What will ' + core.me.name + ' do?';
  }
  function paint() { btns.forEach((b, i) => b.classList.toggle('is-sel', i === sel)); }
  function anim(el, cls, ms) { el.classList.remove(cls); void el.offsetWidth; el.classList.add(cls); return wait(ms); }
  async function throwBall(e) {
    const sr = screen.getBoundingClientRect(), a = sprMe.getBoundingClientRect(), z = sprFoe.getBoundingClientRect();
    const x0 = a.left + a.width * 0.7 - sr.left, y0 = a.top + a.height * 0.3 - sr.top, x1 = z.left + z.width * 0.5 - sr.left, y1 = z.top + z.height * 0.55 - sr.top;
    ballEl.style.setProperty('--bc', hex(BALL_COL[clamp(e.tier, 1, 3) - 1])); ballEl.style.display = 'block'; ballEl.style.opacity = '1';
    const at = (t) => 'translate(' + (x0 + (x1 - x0) * t - 10) + 'px,' + (y0 + (y1 - y0) * t - Math.sin(Math.PI * t) * sr.height * 0.28 - 10) + 'px) rotate(' + (t * 720) + 'deg)';
    if (speed && ballEl.animate) {
      const kf = []; for (let i = 0; i <= 10; i++) kf.push({ transform: at(i / 10) });
      await ballEl.animate(kf, { duration: 650, easing: 'linear', fill: 'forwards' }).finished.catch(() => 0);
    } else ballEl.style.transform = at(1);
    sprFoe.classList.add('is-in'); await wait(320);
    const gy = y1 + z.height * 0.28, rest = 'translate(' + (x1 - 10) + 'px,' + gy + 'px)';
    if (speed && ballEl.animate) await ballEl.animate([{ transform: 'translate(' + (x1 - 10) + 'px,' + y1 + 'px)' }, { transform: rest }], { duration: 260, easing: 'ease-in', fill: 'forwards' }).finished.catch(() => 0); else ballEl.style.transform = rest;
    for (let i = 0; i < e.shakes; i++) {
      if (speed && ballEl.animate) await ballEl.animate([{ transform: rest + ' rotate(0)' }, { transform: rest + ' rotate(-28deg)' }, { transform: rest + ' rotate(24deg)' }, { transform: rest + ' rotate(0)' }], { duration: 640, easing: 'steps(6)', fill: 'forwards' }).finished.catch(() => 0);
      msgEl.textContent = '. '.repeat(i + 1); await wait(180);
    }
    if (e.caught) { root.classList.add('is-click'); await wait(700); ballEl.style.opacity = '0.0'; root.classList.remove('is-click'); }
    else { ballEl.style.display = 'none'; sprFoe.classList.remove('is-in'); await wait(200); }
  }
  async function play(evs) {
    for (const e of evs) {
      if (e.k === 'msg') { msgEl.textContent = e.s; await wait(780); }
      else if (e.k === 'anim') { await anim(e.side === 'me' ? sprMe : sprFoe, e.fx === 'atk' ? (e.side === 'me' ? 'fx-atkme' : 'fx-atkfoe') : 'fx-hurt', 260); }
      else if (e.k === 'hp') { setBox(e.side === 'me' ? bMe : bFoe, e.side === 'me' ? core.me : core.foe, e.hp); await wait(460); }
      else if (e.k === 'ball') await throwBall(e);
      else if (e.k === 'end') { await wait(500); finish(e.r); }
    }
  }
  async function run(a) {
    if (mode === 'busy' || closed) return; const prev = mode; mode = 'busy'; menu.textContent = ''; btns = [];
    const ev = core.act(a); await play(ev);
    if (!closed && !core.over) { mode = 'main'; sel = 0; render(); } else if (!ev.length) mode = prev;
    return ev;
  }
  function finish(r) {
    if (closed) return; const m = core.me.mon;
    if (m) { m.hp = r === 'lose' ? battleStats(m).hp : Math.max(1, core.me.hp); if (r === 'win') m.hp = Math.min(battleStats(m).hp, m.hp); }
    closed = true; window.removeEventListener('keydown', onKey, true); root.classList.add('is-out');
    setTimeout(() => { root.remove(); }, speed ? 260 : 0);
    resolveDone({ result: r, core });
    if (o.onEnd) try { o.onEnd(r, core); } catch (e) { console.warn('[battle] onEnd', e); }
  }
  function onKey(e) {
    if (closed) return;
    const k = e.code; let used = true;
    if (mode === 'busy') used = /^(Arrow|Key[WASDEFXQ]|Enter|Space|Escape|Backspace|Tab)/.test(k);
    else if (k === 'ArrowDown' || k === 'ArrowRight' || k === 'KeyS' || k === 'KeyD') { sel = (sel + 1) % btns.length; paint(); }
    else if (k === 'ArrowUp' || k === 'ArrowLeft' || k === 'KeyW' || k === 'KeyA') { sel = (sel + btns.length - 1) % btns.length; paint(); }
    else if (k === 'Enter' || k === 'Space' || k === 'KeyE' || k === 'KeyF') { if (btns[sel]) btns[sel]._f(); }
    else if (k === 'Escape' || k === 'Backspace' || k === 'KeyX') { if (mode === 'fight') { mode = 'main'; sel = 0; render(); } }
    else used = /^(Key|Digit|Arrow|Tab|Shift)/.test(k);
    if (used) { e.preventDefault(); e.stopImmediatePropagation(); }
  }
  window.addEventListener('keydown', onKey, true);
  (o.host || doc.body).appendChild(root);
  const ctrl = {
    core, el: root, done, act: run, fight: (i) => run({ k: 'fight', i }), ball: () => run({ k: 'ball' }), run: () => run({ k: 'run' }),
    get mode() { return mode; }, get closed() { return closed; }, close: () => finish(core.over ? core.result : 'run'),
  };
  msgEl.textContent = 'A wild ' + core.foe.name + ' appeared!';
  wait(1100).then(() => { if (!closed) { mode = 'main'; render(); } });
  if (!speed) { mode = 'main'; render(); }
  return ctrl;
}

// GOR BRAWL UI: DOM over a 480x270 canvas. Character select, HUD (damage %, stocks, timer), countdown,
// K.O. text, pause, results. Styles are injected here (no separate css file). Original chrome only.
import { FIGHTERS, FIGHTER_IDS } from './smash-fighters.js';
import { PLAYER_COLORS } from './smash-engine.js';

const CSS = `
.sm-root{position:fixed;inset:0;background:#05020a;display:flex;align-items:center;justify-content:center;overflow:hidden;font-family:ui-monospace,Menlo,Consolas,monospace;color:#efe6ff;user-select:none;-webkit-user-select:none}
.sm-wrap{position:relative;width:calc(var(--u)*480px);height:calc(var(--u)*270px);background:#08030f;overflow:hidden}
.sm-wrap>canvas{position:absolute;inset:0;width:100%;height:100%;image-rendering:pixelated;image-rendering:crisp-edges}
.sm-layer{position:absolute;inset:0;pointer-events:none}
.sm-hud{display:none}.sm-hud.on{display:block}
.sm-timer{position:absolute;top:calc(var(--u)*6px);left:50%;transform:translateX(-50%);font-weight:900;font-size:calc(var(--u)*16px);letter-spacing:calc(var(--u)*1px);text-shadow:0 calc(var(--u)*1px) 0 #1a0b30,0 0 calc(var(--u)*6px) #8a5cd8}
.sm-cards{position:absolute;left:0;right:0;bottom:calc(var(--u)*6px);display:flex;justify-content:center;gap:calc(var(--u)*8px)}
.sm-card{min-width:calc(var(--u)*92px);padding:calc(var(--u)*3px) calc(var(--u)*6px);background:linear-gradient(#2a1450cc,#120828cc);border:calc(var(--u)*1px) solid #5a3a90;border-bottom-width:calc(var(--u)*3px);position:relative;border-radius:calc(var(--u)*3px)}
.sm-card .nm{font-size:calc(var(--u)*7px);letter-spacing:calc(var(--u)*1px);font-weight:700;display:flex;align-items:center;gap:calc(var(--u)*3px)}
.sm-card .sw{width:calc(var(--u)*7px);height:calc(var(--u)*7px);display:inline-block;border:calc(var(--u)*1px) solid #0008}
.sm-card .pc{font-size:calc(var(--u)*24px);font-weight:900;line-height:1;text-shadow:0 calc(var(--u)*2px) 0 #0009}
.sm-card .pc small{font-size:calc(var(--u)*9px)}
.sm-card .pips{display:flex;gap:calc(var(--u)*3px);margin-top:calc(var(--u)*1px);align-items:center;font-size:calc(var(--u)*6px)}
.sm-card .pip{width:calc(var(--u)*7px);height:calc(var(--u)*7px);border-radius:50%;border:calc(var(--u)*1px) solid #0008}
.sm-card .pip.x{opacity:.18}
.sm-card.hit{animation:smshake .22s}
.sm-card.out{opacity:.4}
@keyframes smshake{0%,100%{transform:translate(0,0)}20%{transform:translate(calc(var(--u)*-2px),calc(var(--u)*1px))}40%{transform:translate(calc(var(--u)*2px),calc(var(--u)*-1px))}60%{transform:translate(calc(var(--u)*-1px),0)}80%{transform:translate(calc(var(--u)*1px),0)}}
.sm-big{position:absolute;left:0;right:0;top:34%;text-align:center;font-weight:900;font-size:calc(var(--u)*46px);letter-spacing:calc(var(--u)*3px);text-shadow:0 calc(var(--u)*3px) 0 #1a0b30,0 0 calc(var(--u)*14px) currentColor;opacity:0;transform:scale(1.4);transition:opacity .08s,transform .12s}
.sm-big.on{opacity:1;transform:scale(1)}
.sm-screen{position:absolute;inset:0;display:none;background:radial-gradient(ellipse at 50% 30%,#2a1458 0%,#0b0418 70%);pointer-events:auto;overflow:hidden}
.sm-screen.on{display:block}
.sm-title{position:absolute;top:calc(var(--u)*8px);left:0;right:0;text-align:center;font-weight:900;font-size:calc(var(--u)*22px);letter-spacing:calc(var(--u)*4px);color:#e9dcff;text-shadow:0 calc(var(--u)*2px) 0 #4a2a90,0 0 calc(var(--u)*12px) #8a5cd8}
.sm-sub{position:absolute;top:calc(var(--u)*34px);left:0;right:0;text-align:center;font-size:calc(var(--u)*6.5px);color:#a890d8;letter-spacing:calc(var(--u)*1px)}
.sm-slots{position:absolute;top:calc(var(--u)*50px);left:calc(var(--u)*14px);right:calc(var(--u)*14px);display:grid;grid-template-columns:repeat(4,1fr);gap:calc(var(--u)*6px)}
.sm-slot{background:#1a0b3acc;border:calc(var(--u)*1px) solid #4a2a80;padding:calc(var(--u)*4px);text-align:center;cursor:pointer;border-radius:calc(var(--u)*3px);position:relative}
.sm-slot.sel{border-color:#c79cff;box-shadow:0 0 calc(var(--u)*8px) #8a5cd888}
.sm-slot.off{opacity:.45}
.sm-slot .pl{font-weight:900;font-size:calc(var(--u)*9px)}
.sm-slot .ct{margin:calc(var(--u)*3px) auto;font-size:calc(var(--u)*6.5px);padding:calc(var(--u)*2px) calc(var(--u)*3px);background:#3a1f70;border:calc(var(--u)*1px) solid #7a5ac8;display:inline-block;cursor:pointer}
.sm-slot canvas{display:block;margin:0 auto;width:calc(var(--u)*48px);height:calc(var(--u)*72px);image-rendering:pixelated}
.sm-slot .fn{font-weight:800;font-size:calc(var(--u)*8px);letter-spacing:calc(var(--u)*1px)}
.sm-cards2{position:absolute;bottom:calc(var(--u)*34px);left:calc(var(--u)*14px);right:calc(var(--u)*14px);display:flex;gap:calc(var(--u)*6px);justify-content:center}
.sm-pick{flex:1;background:#140a2ccc;border:calc(var(--u)*1px) solid #3a2470;padding:calc(var(--u)*3px);cursor:pointer;text-align:center;font-size:calc(var(--u)*6px);border-radius:calc(var(--u)*3px)}
.sm-pick:hover,.sm-pick.cur{border-color:#e9dcff;background:#2a1458}
.sm-pick b{display:block;font-size:calc(var(--u)*8px);letter-spacing:calc(var(--u)*1px)}
.sm-pick span{color:#a890d8;font-size:calc(var(--u)*5px)}
.sm-btn{position:absolute;bottom:calc(var(--u)*8px);left:50%;transform:translateX(-50%);font:inherit;font-weight:900;font-size:calc(var(--u)*12px);letter-spacing:calc(var(--u)*2px);padding:calc(var(--u)*4px) calc(var(--u)*18px);background:#7a3ae0;color:#fff;border:0;border-bottom:calc(var(--u)*3px) solid #3a1a80;border-radius:calc(var(--u)*3px);cursor:pointer}
.sm-btn:hover{background:#9a5af8}
.sm-btn.alt{background:#2a1458;border-bottom-color:#12082a}
.sm-keys{position:absolute;bottom:calc(var(--u)*2px);left:0;right:0;text-align:center;font-size:calc(var(--u)*4.6px);color:#7a68a8}
.sm-res{position:absolute;inset:0;display:none;background:#05020acc;pointer-events:auto}.sm-res.on{display:block}
.sm-rtitle{position:absolute;top:calc(var(--u)*16px);left:0;right:0;text-align:center;font-size:calc(var(--u)*24px);font-weight:900;letter-spacing:calc(var(--u)*3px)}
.sm-rrows{position:absolute;top:calc(var(--u)*56px);left:calc(var(--u)*50px);right:calc(var(--u)*50px);display:flex;flex-direction:column;gap:calc(var(--u)*5px)}
.sm-rrow{display:grid;grid-template-columns:calc(var(--u)*24px) 1fr repeat(4,calc(var(--u)*44px));align-items:center;background:#1a0b3acc;border:calc(var(--u)*1px) solid #4a2a80;padding:calc(var(--u)*3px) calc(var(--u)*6px);font-size:calc(var(--u)*7px);border-radius:calc(var(--u)*3px)}
.sm-rrow.w{border-color:#ffd84d;background:#3a2a10cc}
.sm-rrow b{font-size:calc(var(--u)*11px)}
.sm-rh{position:absolute;top:calc(var(--u)*46px);left:calc(var(--u)*50px);right:calc(var(--u)*50px);display:grid;grid-template-columns:calc(var(--u)*24px) 1fr repeat(4,calc(var(--u)*44px));font-size:calc(var(--u)*5px);color:#a890d8;padding:0 calc(var(--u)*6px)}
.sm-rbtn{position:absolute;bottom:calc(var(--u)*14px);left:0;right:0;display:flex;justify-content:center;gap:calc(var(--u)*10px)}
.sm-rbtn .sm-btn{position:static;transform:none}
.sm-pause{position:absolute;inset:0;display:none;align-items:center;justify-content:center;background:#05020a99;font-weight:900;font-size:calc(var(--u)*26px);letter-spacing:calc(var(--u)*4px);pointer-events:none}
.sm-pause.on{display:flex}
`;

const CTRL_OPTS = [
  { ctrl: 'p1', label: 'HUMAN  WASD' }, { ctrl: 'p2', label: 'HUMAN  ARROWS' },
  { ctrl: 'cpu', level: 1, label: 'CPU  LV1' }, { ctrl: 'cpu', level: 2, label: 'CPU  LV2' }, { ctrl: 'cpu', level: 3, label: 'CPU  LV3' }, { ctrl: 'off', label: 'OFF' },
];
const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };

export function createUI(root) {
  if (!document.getElementById('sm-css')) { const s = document.createElement('style'); s.id = 'sm-css'; s.textContent = CSS; document.head.appendChild(s); }
  const ui = { slots: [{ fid: 'pilot', opt: 0 }, { fid: 'swift', opt: 3 }, { fid: 'hopper', opt: 5 }, { fid: 'builder', opt: 5 }], sel: 0, game: null, atlas: null, onLaunch: null };
  const rootEl = el('div', 'sm-root'), wrap = el('div', 'sm-wrap'), canvas = el('canvas'); canvas.width = 480; canvas.height = 270;
  wrap.appendChild(canvas);
  const hud = el('div', 'sm-layer sm-hud'), timer = el('div', 'sm-timer', '4:00'), cards = el('div', 'sm-cards'), big = el('div', 'sm-big'), pause = el('div', 'sm-pause', 'PAUSED');
  hud.append(timer, cards, big); wrap.append(hud, pause);
  const sel = el('div', 'sm-screen on'), res = el('div', 'sm-res');
  wrap.append(sel, res); rootEl.appendChild(wrap); root.appendChild(rootEl); ui.canvas = canvas;

  function layout() {
    const u = Math.min(innerWidth / 480, innerHeight / 270), s = u >= 1 ? Math.floor(u) : u;
    rootEl.style.setProperty('--u', String(s));
  }
  layout(); addEventListener('resize', layout);

  // ---- select screen
  const previews = [];
  function drawPreview(cv, fid, frame) {
    if (!ui.atlas) return; const fr = ui.atlas.frame(fid, 'idle', frame), c = cv.getContext('2d'); c.imageSmoothingEnabled = false; c.clearRect(0, 0, cv.width, cv.height); c.drawImage(ui.atlas.canvas, fr[0], fr[1], 32, 48, 0, 0, cv.width, cv.height);
  }
  function buildSelect() {
    sel.innerHTML = ''; previews.length = 0;
    sel.append(el('div', 'sm-title', 'GOR BRAWL'), el('div', 'sm-sub', 'PLATEAU  -  3 STOCKS  -  4:00  -  CLICK A SLOT, THEN A FIGHTER'));
    const slots = el('div', 'sm-slots');
    ui.slots.forEach((s, i) => {
      const d = el('div', 'sm-slot' + (i === ui.sel ? ' sel' : '') + (CTRL_OPTS[s.opt].ctrl === 'off' ? ' off' : ''));
      const cv = el('canvas'); cv.width = 32; cv.height = 48; previews.push([cv, () => s.fid]);
      const ct = el('div', 'ct', CTRL_OPTS[s.opt].label);
      ct.onclick = (e) => { e.stopPropagation(); s.opt = (s.opt + 1) % CTRL_OPTS.length; buildSelect(); };
      d.append(el('div', 'pl', 'P' + (i + 1)), ct, cv, el('div', 'fn', FIGHTERS[s.fid].name));
      d.querySelector('.pl').style.color = PLAYER_COLORS[i];
      d.onclick = () => { ui.sel = i; buildSelect(); };
      slots.appendChild(d);
    });
    sel.appendChild(slots);
    const row = el('div', 'sm-cards2');
    FIGHTER_IDS.forEach((id) => {
      const f = FIGHTERS[id], p = el('div', 'sm-pick' + (ui.slots[ui.sel].fid === id ? ' cur' : ''), '<b style="color:' + f.color + '">' + f.name + '</b>' + f.blurb + '<br><span>WT ' + f.weight + '  RUN ' + f.run + '  FALL ' + f.fall + '</span>');
      p.onclick = () => { ui.slots[ui.sel].fid = id; buildSelect(); };
      row.appendChild(p);
    });
    sel.appendChild(row);
    const go = el('button', 'sm-btn', 'FIGHT'); go.onclick = () => ui.launch(); sel.append(go, el('div', 'sm-keys', 'P1  WASD + J ATTACK  K SPECIAL  L SHIELD  I GRAB  SPACE JUMP     P2  ARROWS + NUM1 / NUM2 / NUM3 / NUM0     PAD SUPPORTED     P PAUSE'));
    previews.forEach(([cv, fn]) => drawPreview(cv, fn(), 0));
  }
  let selTimer = 0;
  function animatePreviews() { clearInterval(selTimer); let n = 0; selTimer = setInterval(() => { n++; previews.forEach(([cv, fn]) => drawPreview(cv, fn(), (n >> 1) % 6)); }, 110); }
  ui.launch = () => {
    const players = ui.slots.map((s) => { const o = CTRL_OPTS[s.opt]; return { fid: s.fid, ctrl: o.ctrl, level: o.level || 2 }; }).filter((p) => p.ctrl !== 'off');
    if (players.length < 2) { const s = ui.slots.find((x) => CTRL_OPTS[x.opt].ctrl === 'off'); if (s) s.opt = 3; return ui.launch(); }
    ui.onLaunch && ui.onLaunch({ players, stage: 'plateau' });
  };
  ui.attach = (game) => { ui.game = game; ui.atlas = game.atlas; buildSelect(); animatePreviews(); };
  ui.showSelect = () => { res.classList.remove('on'); hud.classList.remove('on'); sel.classList.add('on'); buildSelect(); animatePreviews(); if (ui.game) ui.game.stop(); };

  // ---- HUD
  let cardEls = [], cache = [], bigT = 0;
  ui.onStart = (sim) => {
    clearInterval(selTimer); sel.classList.remove('on'); res.classList.remove('on'); hud.classList.add('on'); pause.classList.remove('on');
    cards.innerHTML = ''; cardEls = []; cache = [];
    sim.fighters.forEach((f) => {
      const c = el('div', 'sm-card'), d = FIGHTERS[f.fid];
      c.innerHTML = '<div class="nm"><i class="sw" style="background:' + PLAYER_COLORS[f.i] + '"></i>P' + (f.i + 1) + ' ' + d.name + (f.ctrl === 'cpu' ? ' <span style="color:#a890d8">CPU' + f.level + '</span>' : '') + '</div><div class="pc">0<small>%</small></div><div class="pips"></div>';
      cards.appendChild(c); cardEls.push({ c, pc: c.querySelector('.pc'), pips: c.querySelector('.pips') }); cache.push({});
    });
    say('', 0);
  };
  function say(txt, color, ms) {
    big.textContent = txt; big.style.color = color || '#fff'; big.classList.toggle('on', !!txt); clearTimeout(bigT);
    if (txt && ms) bigT = setTimeout(() => big.classList.remove('on'), ms);
  }
  ui.onEvent = (e, game) => {
    if (e.t === 'count') say(String(e.n), '#c79cff', 900);
    else if (e.t === 'go') say('GOR!', '#ffd84d', 700);
    else if (e.t === 'ko') { say('K.O.!', PLAYER_COLORS[e.who % 4], 900); const ce = cardEls[e.who]; if (ce) { ce.c.classList.remove('hit'); void ce.c.offsetWidth; ce.c.classList.add('hit'); } }
    else if (e.t === 'hit') { const ce = cardEls[e.who]; if (ce && e.dmg >= 3) { ce.c.classList.remove('hit'); void ce.c.offsetWidth; ce.c.classList.add('hit'); } }
    else if (e.t === 'end') say('GAME!', '#fff', 1500);
    else if (e.t === 'over') showResults(e.result, game);
  };
  ui.onPause = (p) => pause.classList.toggle('on', !!p);
  const pcColor = (p) => { if (p < 50) return '#fff'; if (p < 100) { const t = (p - 50) / 50; return 'rgb(255,' + Math.round(255 - t * 20) + ',' + Math.round(255 - t * 160) + ')'; } const t = Math.min(1, (p - 100) / 80); return 'rgb(255,' + Math.round(235 - t * 160) + ',' + Math.round(95 - t * 40) + ')'; };
  ui.update = (sim) => {
    const secs = Math.max(0, Math.ceil(sim.time / 60)), ts = (secs / 60 | 0) + ':' + String(secs % 60).padStart(2, '0');
    if (ts !== ui._ts) { timer.textContent = ts; ui._ts = ts; }
    sim.fighters.forEach((f, i) => {
      const k = cache[i], ce = cardEls[i]; if (!ce) return; const pct = Math.floor(f.pct), key = pct + ':' + f.stocks + ':' + f.res;
      if (k.key === key) return; k.key = key;
      ce.pc.innerHTML = pct + '<small>%</small>'; ce.pc.style.color = pcColor(pct); ce.c.classList.toggle('out', f.stocks <= 0);
      let h = ''; for (let s = 0; s < 3; s++) h += '<i class="pip' + (s < f.stocks ? '' : ' x') + '" style="background:' + PLAYER_COLORS[f.i] + '"></i>';
      if (FIGHTERS[f.fid].res0 != null) h += '<span style="margin-left:4px;color:#e8c040">BLK ' + f.res + '</span>';
      ce.pips.innerHTML = h;
    });
  };

  // ---- results
  function showResults(r, game) {
    const sim = game.sim; hud.classList.remove('on'); say('', 0); res.innerHTML = ''; res.classList.add('on');
    const w = r.winner >= 0 ? sim.fighters[r.winner] : null;
    const t = el('div', 'sm-rtitle', w ? 'P' + (w.i + 1) + ' ' + FIGHTERS[w.fid].name + ' WINS' : 'DRAW'); t.style.color = w ? PLAYER_COLORS[w.i] : '#fff'; res.appendChild(t);
    res.appendChild(el('div', 'sm-rh', '<span></span><span>FIGHTER</span><span>STOCKS</span><span>K.O.S</span><span>DEALT</span><span>TAKEN</span>'));
    const rows = el('div', 'sm-rrows');
    r.rows.forEach((row, k) => {
      const d = el('div', 'sm-rrow' + (k === 0 && w ? ' w' : ''), '<b style="color:' + PLAYER_COLORS[row.i] + '">' + (k + 1) + '</b><span>P' + (row.i + 1) + ' ' + FIGHTERS[row.fid].name + (row.ctrl === 'cpu' ? ' (CPU)' : '') + '</span><span>' + row.stocks + '</span><span>' + sim.fighters.filter((f) => f.i !== row.i).reduce((a, f) => a + f.kos, 0) + '</span><span>' + row.dealt + '%</span><span>' + row.taken + '%</span>');
      rows.appendChild(d);
    });
    res.appendChild(rows);
    const b = el('div', 'sm-rbtn'), a = el('button', 'sm-btn', 'REMATCH'), m = el('button', 'sm-btn alt', 'MENU');
    a.onclick = () => ui.onLaunch && ui.onLaunch(game.cfg); m.onclick = () => ui.showSelect(); b.append(a, m); res.appendChild(b);
  }
  addEventListener('keydown', (e) => { if (ui.game && ui.game.sim && ui.game.sim.phase === 'play' && (e.code === 'KeyP' || e.code === 'Escape')) ui.game.pause(); else if (e.code === 'Enter' && sel.classList.contains('on')) ui.launch(); });
  return ui;
}

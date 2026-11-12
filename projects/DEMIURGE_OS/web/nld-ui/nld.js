'use strict';
// Shared nonlinear-daylight UI. Vanilla, event-driven only: no timers while idle.
// Param contract: docs/parameters.md sec 7 -> POST /api/pool/set {path, value}.
var CONFIG = {
  setUrl: '/api/pool/set',          // pool drive endpoint (parameters.md sec 7)
  paramsUrl: '/api/pool/params',    // registry; used only for optional active-language hint
  activeUrl: '/api/pool/active',    // PLACEHOLDER: not defined by parameters.md; expects {"language":"csound"}
  prefix: 'nld/',
  minIntervalMs: 50                 // client coalesce, <= 20 Hz (pool cap is 100 Hz)
};
var defaults = {density: .35, tone: .45, length: .5};
var $ = function(s, r){ return (r || document).querySelector(s); };
var statusEl = $('#status'), langEl = $('#lang'), activeBtn = $('#active');
function say(t, err){ statusEl.textContent = t; statusEl.className = err ? 'err' : ''; }

function post(name, v){
  return fetch(CONFIG.setUrl, {method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({path: CONFIG.prefix + name, value: v})})
    .then(function(r){ if(!r.ok) throw new Error(r.status); say('sent ' + name + ' ' + v.toFixed(2)); })
    .catch(function(e){ say('set failed (' + (e.message || e) + '): pool API not live?', true); });
}

function Slider(el){
  var name = el.dataset.name, out = $('output', el), val = defaults[name];
  var last = 0, pending = null, tm = null;
  function show(v){
    val = v; el.style.setProperty('--v', v);
    out.textContent = v.toFixed(2); el.setAttribute('aria-valuenow', v.toFixed(2));
  }
  function send(v){
    var now = performance.now(), wait = CONFIG.minIntervalMs - (now - last);
    if(wait <= 0){ last = now; post(name, v); }
    else { pending = v; if(!tm) tm = setTimeout(function(){ tm = null; last = performance.now(); post(name, pending); }, wait); } // one-shot, only while dragging
  }
  function at(e){ var r = el.getBoundingClientRect(); return Math.min(1, Math.max(0, (e.clientX - r.left) / r.width)); }
  el.addEventListener('pointerdown', function(e){ el.setPointerCapture(e.pointerId); el.classList.add('drag'); var v = at(e); show(v); send(v); });
  el.addEventListener('pointermove', function(e){ if(el.hasPointerCapture(e.pointerId)){ var v = at(e); show(v); send(v); } });
  function up(e){ el.classList.remove('drag'); }
  el.addEventListener('pointerup', up); el.addEventListener('pointercancel', up);
  el.addEventListener('keydown', function(e){
    var d = e.key === 'ArrowRight' || e.key === 'ArrowUp' ? .02 : e.key === 'ArrowLeft' || e.key === 'ArrowDown' ? -.02 : 0;
    if(d){ e.preventDefault(); var v = Math.min(1, Math.max(0, val + d)); show(v); send(v); }
  });
  show(val);
}
document.querySelectorAll('.sl').forEach(Slider);

// Active example: read-only, fetched once on load, on tab focus, or on tap. No polling.
function refreshActive(){
  fetch(CONFIG.activeUrl).then(function(r){ if(!r.ok) throw 0; return r.json(); })
    .then(function(j){ langEl.textContent = 'hello_' + j.language; activeBtn.classList.add('live'); say('connected'); })
    .catch(function(){
      activeBtn.classList.remove('live');
      langEl.textContent = 'engine: unknown (mock)';
      say('no active-engine endpoint yet; sliders post to ' + CONFIG.setUrl);
    });
}
activeBtn.addEventListener('click', refreshActive);
document.addEventListener('visibilitychange', function(){ if(!document.hidden) refreshActive(); });
refreshActive();

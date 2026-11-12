"""The GORCAVE control panel — a single self-contained page served at
GET /control. Same-origin with every lightserver endpoint, mobile-first,
matches the gorcave cosmic brand. No external dependencies."""

CONTROL_HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#0a0512">
<title>GORCAVE — control</title>
<style>
  :root{
    --bg:#0a0512; --bg-glow:#150b26;
    --surface:rgba(168,146,214,0.05); --surface-2:rgba(168,146,214,0.09);
    --line:rgba(178,158,222,0.12); --line-strong:rgba(178,158,222,0.30);
    --text:rgba(236,231,247,0.90); --text-dim:rgba(214,205,235,0.55);
    --text-faint:rgba(198,188,226,0.28); --lavender:rgba(178,163,214,0.85);
    --ok:rgba(140,214,168,0.90); --bad:rgba(224,130,140,0.90);
  }
  *{box-sizing:border-box}
  html{height:100%;background:var(--bg)}
  body{margin:0;min-height:100%;color:var(--text);
    font-family:'Courier New',Courier,monospace;
    background:radial-gradient(120% 70% at 50% 0%,var(--bg-glow) 0%,var(--bg) 62%),var(--bg);
    padding:18px 16px 60px;max-width:640px;margin:0 auto;-webkit-font-smoothing:antialiased}
  header{display:flex;align-items:baseline;justify-content:space-between;gap:10px;margin-bottom:6px}
  h1{font-size:15px;letter-spacing:.32em;margin:0;color:var(--lavender);font-weight:400}
  .live{font-size:11px;color:var(--text-dim);display:flex;align-items:center;gap:6px}
  .dot{width:7px;height:7px;border-radius:50%;background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .now{font-size:12px;color:var(--text-dim);margin:0 0 18px;min-height:15px}
  .now b{color:var(--text)}
  section{background:var(--surface);border:1px solid var(--line);border-radius:14px;
    padding:14px;margin-bottom:14px}
  .label{font-size:10px;letter-spacing:.22em;color:var(--text-faint);text-transform:uppercase;margin-bottom:10px}
  textarea{width:100%;background:rgba(0,0,0,.28);border:1px solid var(--line-strong);
    border-radius:10px;color:var(--text);font-family:inherit;font-size:14px;padding:11px;
    resize:vertical;min-height:52px}
  textarea:focus{outline:none;border-color:var(--lavender)}
  .row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
  button{font-family:inherit;font-size:13px;color:var(--text);cursor:pointer;
    background:var(--surface-2);border:1px solid var(--line-strong);border-radius:10px;
    padding:10px 14px;transition:.14s;flex:0 0 auto}
  button:hover{background:rgba(178,163,214,.16);border-color:var(--lavender)}
  button:active{transform:translateY(1px)}
  button.go{background:linear-gradient(90deg,rgba(150,122,208,.65),rgba(178,163,214,.85));
    border-color:transparent;color:#0a0512;font-weight:bold;flex:1 1 auto}
  button.ghost{background:transparent}
  button:disabled{opacity:.4;cursor:default}
  .chips{display:flex;gap:7px;flex-wrap:wrap}
  .chip{font-size:12px;padding:7px 11px;border-radius:20px}
  .swatch{width:34px;height:34px;padding:0;border-radius:9px}
  input[type=color]{width:46px;height:38px;padding:0;border:1px solid var(--line-strong);
    border-radius:9px;background:none;cursor:pointer}
  input[type=range]{flex:1 1 120px;accent-color:var(--lavender)}
  .status{font-size:11px;color:var(--text-dim);margin-top:9px;min-height:14px}
  .status.err{color:var(--bad)} .status.ok{color:var(--ok)}
  .spin{display:inline-block;width:11px;height:11px;border:2px solid var(--line-strong);
    border-top-color:var(--lavender);border-radius:50%;animation:sp .7s linear infinite;vertical-align:-1px;margin-right:6px}
  @keyframes sp{to{transform:rotate(360deg)}}
  .muted{opacity:.5;pointer-events:none}
  .pole{display:flex;align-items:center;gap:10px;margin-bottom:9px}
  .pole span{font-size:12px;color:var(--text-dim);min-width:56px}
</style>
</head>
<body>
<header>
  <h1>G O R C A V E</h1>
  <div class="live"><span class="dot" id="livedot"></span><span id="fps">—</span></div>
</header>
<p class="now">now playing · <b id="scenename">…</b></p>

<section>
  <div class="label">say a vibe · neptr dreams it up</div>
  <textarea id="vibe" placeholder="cosmic underwater nebula…"></textarea>
  <div class="row" style="margin-top:10px">
    <button class="go" id="gen">generate</button>
    <button class="ghost" id="like" title="save this look to your taste">♥ <span id="tastect">0</span></button>
  </div>
  <div class="status" id="genstatus"></div>
</section>

<section>
  <div class="label">quick color · whole ring</div>
  <div class="row" style="margin-bottom:10px">
    <input type="color" id="picker" value="#8b2fc9">
    <button id="applycol" class="go">fill ring</button>
  </div>
  <div class="chips" id="swatches"></div>
  <div class="row" style="margin-top:12px">
    <span style="font-size:11px;color:var(--text-faint)">DIM</span>
    <input type="range" id="bright" min="5" max="100" value="80">
    <span style="font-size:11px;color:var(--text-faint)">BRIGHT</span>
  </div>
</section>

<section>
  <div class="label">saved looks · one tap</div>
  <div class="chips" id="scenes"><span class="status">loading…</span></div>
</section>

<section id="polesec" class="muted">
  <div class="label">the 2 poles <span id="polestat" style="color:var(--text-faint)">· connecting…</span></div>
  <div class="pole"><span>pole 1</span><input type="color" class="polecol" data-id="1" value="#5d2fc9"><button class="poleoff" data-id="1">off</button></div>
  <div class="pole"><span>pole 2</span><input type="color" class="polecol" data-id="2" value="#8b2fc9"><button class="poleoff" data-id="2">off</button></div>
</section>

<section>
  <div class="label">power</div>
  <div class="row">
    <button id="resume">▶ resume</button>
    <button id="stop">⏸ stop stream</button>
    <button id="black" class="ghost">⏻ blackout</button>
  </div>
  <div class="status" id="pwrstatus"></div>
</section>

<script>
const $=s=>document.querySelector(s), api=(p,o)=>fetch(p,o).then(r=>r.json());
function solid(hex,bright){return {name:"solid "+hex,master:{brightness:bright},
  layers:[{type:"solid",color:hex}]};}
async function post(path,body){
  const r=await fetch(path,{method:"POST",headers:{"Content-Type":"application/json"},
    body:body?JSON.stringify(body):null});
  if(!r.ok) throw new Error((await r.json().catch(()=>({}))).detail||r.status); return r.json();}

const SW=["#8b2fc9","#5d2fc9","#b026ff","#2ee6ff","#ff9040","#ff2e6e","#2effa0","#f5e6ff"];
$("#swatches").innerHTML=SW.map(c=>`<button class="swatch" style="background:${c}" data-c="${c}"></button>`).join("");
$("#swatches").addEventListener("click",e=>{const c=e.target.dataset.c; if(c){$("#picker").value=c; fill(c);}});

const brightVal=()=>Math.round($("#bright").value)/100*1.5;
async function fill(hex){try{await post("/scene",solid(hex,brightVal())); flash($("#pwrstatus"),"filled "+hex,"ok"); refresh();}catch(e){flash($("#pwrstatus"),e.message,"err");}}
$("#applycol").onclick=()=>fill($("#picker").value);
$("#bright").addEventListener("change",()=>fill($("#picker").value));

$("#gen").onclick=async()=>{
  const t=$("#vibe").value.trim(); if(!t)return;
  const s=$("#genstatus"); s.className="status"; s.innerHTML='<span class="spin"></span>neptr is dreaming… (10–30s)';
  $("#gen").disabled=true;
  try{const r=await post("/prompt",{text:t}); s.className="status ok";
    s.textContent="✓ "+(r.scene?.name||"done")+"  ("+r.source+")"; refresh();}
  catch(e){s.className="status err"; s.textContent="✗ "+e.message;}
  finally{$("#gen").disabled=false;}
};
$("#like").onclick=async()=>{try{const r=await post("/like"); $("#tastect").textContent=r.taste_count;
  flash($("#genstatus"),"♥ saved to your taste ("+r.taste_count+")","ok");}catch(e){flash($("#genstatus"),e.message,"err");}};

$("#resume").onclick=()=>post("/resume").then(()=>flash($("#pwrstatus"),"resumed","ok")).then(refresh);
$("#stop").onclick=()=>post("/stop").then(()=>flash($("#pwrstatus"),"stream stopped","ok"));
$("#black").onclick=()=>fill("#000000");

function flash(el,msg,cls){el.className="status "+(cls||""); el.textContent=msg; setTimeout(()=>{if(el.textContent===msg)el.textContent="";},4000);}

async function loadScenes(){try{const s=await api("/scenes");
  const html=Object.entries(s).map(([k,v])=>`<button class="chip" data-s="${k}">${v||k}</button>`).join("");
  $("#scenes").innerHTML=html||'<span class="status">no saved scenes</span>';}catch(e){$("#scenes").innerHTML='<span class="status err">'+e.message+'</span>';}}
$("#scenes").addEventListener("click",async e=>{const n=e.target.dataset.s; if(!n)return;
  try{await post("/scene/"+encodeURIComponent(n)); refresh();}catch(err){flash($("#pwrstatus"),err.message,"err");}});

async function refresh(){try{const st=await api("/state");
  $("#scenename").textContent=st.scene||"—"; $("#fps").textContent=Math.round(st.fps_actual||0)+" fps";
  $("#livedot").style.background=st.streaming?"var(--ok)":"var(--text-faint)";}catch(e){$("#livedot").style.background="var(--bad)";}}
async function loadTaste(){try{const t=await api("/taste"); $("#tastect").textContent=t.count;}catch(e){}}

// poles: enabled once the BLE bridge answers (POST /lamp/{id})
async function probePoles(){try{const r=await fetch("/lamp/health"); if(r.ok){const d=await r.json();
  if(d.ok){$("#polesec").classList.remove("muted"); $("#polestat").textContent="· live";}}}catch(e){}}
document.querySelectorAll(".polecol").forEach(inp=>inp.addEventListener("change",()=>
  post("/lamp/"+inp.dataset.id,{on:true,color:inp.value,brightness:80}).catch(()=>{})));
document.querySelectorAll(".poleoff").forEach(b=>b.addEventListener("click",()=>
  post("/lamp/"+b.dataset.id,{on:false}).catch(()=>{})));

loadScenes(); loadTaste(); refresh(); probePoles();
setInterval(refresh,3000);
</script>
</body>
</html>"""

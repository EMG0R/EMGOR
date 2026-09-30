// Shared star backdrop — same field as the homepage galaxy, 2D canvas, fixed behind the page.
(function () {
  function hash32(s){var h=2166136261;for(var i=0;i<s.length;i++){h^=s.charCodeAt(i);h=Math.imul(h,16777619);}return h>>>0;}
  function rng(seed){var a=seed>>>0;return function(){a|=0;a=a+0x6D2B79F5|0;var t=Math.imul(a^a>>>15,1|a);t=t+Math.imul(t^t>>>7,61|t)^t;return((t^t>>>14)>>>0)/4294967296;};}
  var DPR = Math.min(window.devicePixelRatio || 1, 2), TAU = Math.PI*2;
  var sc = document.getElementById('stars');
  if (!sc) { sc = document.createElement('canvas'); sc.id = 'stars'; document.body.insertBefore(sc, document.body.firstChild); }
  sc.style.cssText = 'position:fixed;inset:0;width:100vw;height:100vh;z-index:-1;pointer-events:none;display:block';
  var bg = sc.getContext('2d'), W = 0, H = 0;
  var STAR_COLORS = ['rgba(235,225,255,', 'rgba(192,132,252,', 'rgba(110,235,255,'];
  var nebula = [], starN=0, starX, starY, starR, starPh, starW, starTw, starLayer, starTint;
  (function makeNebula(){ var hues=[268,288,248,195]; for (var i=0;i<hues.length;i++){ var S=512,c=document.createElement('canvas'); c.width=c.height=S; var g=c.getContext('2d');
    var gr=g.createRadialGradient(S/2,S/2,0,S/2,S/2,S/2); gr.addColorStop(0,'hsla('+hues[i]+',70%,34%,'+(i===3?0.03:0.06)+')');
    gr.addColorStop(0.55,'hsla('+hues[i]+',65%,24%,'+(i===3?0.015:0.03)+')'); gr.addColorStop(1,'hsla('+hues[i]+',60%,20%,0)'); g.fillStyle=gr; g.fillRect(0,0,S,S); nebula.push(c);} })();
  function makeStars(){ W=innerWidth; H=innerHeight; sc.width=W*DPR; sc.height=H*DPR;
    starN=Math.max(140,Math.min(460,Math.floor((W*H)/5200)));
    starX=new Float32Array(starN); starY=new Float32Array(starN); starR=new Float32Array(starN); starPh=new Float32Array(starN);
    starW=new Float32Array(starN); starTw=new Uint8Array(starN); starLayer=new Uint8Array(starN); starTint=new Uint8Array(starN);
    var r=rng(hash32('emgor::stars'));
    for (var i=0;i<starN;i++){ starX[i]=r()*W; starY[i]=r()*H; var l=r(); starLayer[i]=l<0.5?0:(l<0.83?1:2); starR[i]=0.4+starLayer[i]*0.45+r()*0.7; starPh[i]=r()*TAU;
      starW[i]=TAU/(6+r()*14); starTw[i]=r()<0.3?1:0; var tt=r(); starTint[i]=tt<0.07?2:(tt<0.26?1:0); } }
  function drawBackdrop(time){ bg.setTransform(DPR,0,0,DPR,0,0); bg.clearRect(0,0,W,H); bg.globalCompositeOperation='lighter'; var d=Math.max(W,H);
    for (var i=0;i<nebula.length;i++){ var ph=time*0.012+i*2.1; var bx=W*(0.18+0.64*(0.5+0.5*Math.sin(ph+i*1.7))), by=H*(0.2+0.6*(0.5+0.5*Math.cos(ph*0.8+i*2.3))), s=d*(0.7+i*0.22);
      bg.drawImage(nebula[i], bx-s/2, by-s/2, s, s); }
    for (var j=0;j<starN;j++){ var sN=0.5+0.5*Math.sin(time*starW[j]+starPh[j]); var a=Math.pow(sN,1.6)*(0.55+starLayer[j]*0.15);
      if (starTw[j]) a*=0.82+0.18*Math.sin(time*2.7+starPh[j]*3.1); if (a<=0.015) continue;
      bg.fillStyle=STAR_COLORS[starTint[j]]+a.toFixed(3)+')'; var rr=starR[j]; bg.fillRect(starX[j]-rr,starY[j]-rr,rr*2,rr*2); }
    bg.globalCompositeOperation='source-over'; }
  addEventListener('resize', makeStars); makeStars();
  var reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;
  (function tick(t){ drawBackdrop((t||0)/1000); if(!reduced) requestAnimationFrame(tick); })(0);
})();

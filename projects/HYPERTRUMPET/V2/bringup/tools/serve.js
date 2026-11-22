// bringup web server (localhost only): serves web/index.html; POST /log mirrors telemetry to logs/tele.jsonl (last 2000 lines);
// POST /cmd queues serial commands for the open tab, GET /cmd drains them; GET/POST /params reads/writes params/params.json, the Mac-side
// backup of every effect param + master volume, so values survive reflashes and record-version changes on the Seed.
// POSTs need header x-ht: 1 (a browser page from another site cannot send it without a CORS preflight, which this server never answers).
const http=require('http'),fs=require('fs'),path=require('path');
const ROOT=path.join(__dirname,'..'), LOG=path.join(ROOT,'logs','tele.jsonl'), PRM=path.join(ROOT,'params','params.json');
let q=[], lines=[];
const body=(req,cb)=>{let b='';req.on('data',d=>{b+=d;if(b.length>1e6)req.destroy();});req.on('end',()=>cb(b));};
http.createServer((req,res)=>{
  const post=req.method==='POST', ok=req.headers['x-ht']==='1';
  if(req.url==='/log'&&post){body(req,b=>{lines.push(new Date().toISOString().slice(11,23)+' '+b.trim());if(lines.length>2000)lines=lines.slice(-1500);fs.writeFile(LOG,lines.join('\n')+'\n',()=>{});res.end('ok');});return;}
  if(req.url==='/cmd'){ if(post){ if(!ok){res.statusCode=403;return res.end();} body(req,b=>{q.push(...b.split('\n').filter(Boolean));res.end('queued');}); return; } res.end(q.join('\n'));q=[];return;}
  if(req.url==='/params'){
    if(post){ if(!ok){res.statusCode=403;return res.end();} body(req,b=>{try{JSON.parse(b);}catch(e){res.statusCode=400;return res.end('bad json');} fs.writeFile(PRM+'.tmp',b,e=>{if(e){res.statusCode=500;return res.end('write failed');} fs.rename(PRM+'.tmp',PRM,()=>res.end('saved'));});}); return; }
    res.setHeader('content-type','application/json'); fs.readFile(PRM,(e,d)=>res.end(e?'{}':d)); return;
  }
  res.setHeader('content-type','text/html');fs.createReadStream(path.join(ROOT,'web','index.html')).pipe(res);
}).listen(8731,'127.0.0.1');

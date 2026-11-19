// serve.mjs -- zero-dependency static server for sim/web on 127.0.0.1:8765
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const root = path.join(path.dirname(fileURLToPath(import.meta.url)), 'web');
const port = Number(process.env.PORT || 8765);
const mime = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8', '.wasm': 'application/wasm', '.json': 'application/json', '.png': 'image/png', '.svg': 'image/svg+xml', '.ico': 'image/x-icon' };
export function start(p = port) {
  const srv = http.createServer((req, res) => {
    let u = decodeURIComponent((req.url || '/').split('?')[0]);
    if (u.endsWith('/')) u += 'index.html';
    const f = path.normalize(path.join(root, u));
    if (!f.startsWith(root)) { res.writeHead(403); return res.end('forbidden'); }
    fs.readFile(f, (err, data) => {
      if (err) { res.writeHead(404, { 'content-type': 'text/plain' }); return res.end('not found'); }
      res.writeHead(200, { 'content-type': mime[path.extname(f)] || 'application/octet-stream', 'cache-control': 'no-store' });
      res.end(data);
    });
  });
  // 8765 is the default; if another app owns it (it does on Emory's Mac: TheWatchM), try the next ports.
  return new Promise((ok, bad) => {
    let port = p, tries = 0;
    const go = () => { srv.once('error', (e) => { if (e.code === 'EADDRINUSE' && ++tries < 20) { port++; go(); } else bad(e); }); srv.listen(port, '127.0.0.1', () => ok(srv)); };
    go();
  });
}
if (import.meta.url === 'file://' + process.argv[1]) {
  start().then((s) => console.log('HYPERTRUMPET V2 sim serving ' + root + '\nURL: http://127.0.0.1:' + s.address().port)).catch(e => { console.error(e.message); process.exit(1); });
}

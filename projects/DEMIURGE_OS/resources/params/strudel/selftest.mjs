// selftest: echo every change of /test/ping back as /pout. Ports from env.
import { init, add, get, out } from './dparams.mjs';
init({ stage: 'test' });
add('/test/ping', 0.1);
let last = 0.1;
out('/test/ping', last);
setInterval(() => { const v = get('/test/ping'); if (v !== last) { last = v; out('/test/ping', v); } }, 10);

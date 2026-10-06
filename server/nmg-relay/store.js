'use strict';
/* Tiny JSON file store for nmg-relay. Files load at boot, writes are debounced (2 s), atomic (tmp + rename),
   and refused above 2 MB per file. Dir: env NMG_DATA, default ~/nmg-relay/data. */
const fs = require('fs');
const os = require('os');
const path = require('path');

const DIR = process.env.NMG_DATA || path.join(os.homedir(), 'nmg-relay', 'data');
const MAX_BYTES = 2 * 1024 * 1024, DEBOUNCE_MS = 2000;
const FILES = { profiles: {}, discoveries: {}, events: {} };
const data = {}, timers = {};

try { fs.mkdirSync(DIR, { recursive: true }); } catch (e) { console.error('store: mkdir failed', e.message); }
Object.keys(FILES).forEach(function (k) {
  try { data[k] = JSON.parse(fs.readFileSync(path.join(DIR, k + '.json'), 'utf8')); }
  catch (e) { data[k] = JSON.parse(JSON.stringify(FILES[k])); }
});

function flush(k) {
  clearTimeout(timers[k]); timers[k] = null;
  const txt = JSON.stringify(data[k]);
  if (txt.length > MAX_BYTES) { console.error('store: ' + k + '.json over 2 MB, not written'); return false; }
  const f = path.join(DIR, k + '.json'), tmp = f + '.tmp';
  try { fs.writeFileSync(tmp, txt); fs.renameSync(tmp, f); return true; }
  catch (e) { console.error('store: write failed', k, e.message); return false; }
}
function get(k) { return data[k]; }
function size(k) { return JSON.stringify(data[k]).length; }
function save(k) { if (!timers[k]) { timers[k] = setTimeout(function () { flush(k); }, DEBOUNCE_MS); timers[k].unref(); } }
function flushAll() { Object.keys(FILES).forEach(function (k) { if (timers[k]) flush(k); }); }

module.exports = { get: get, save: save, size: size, flushAll: flushAll, MAX_BYTES: MAX_BYTES, DIR: DIR };

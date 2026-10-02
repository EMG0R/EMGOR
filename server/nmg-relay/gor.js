'use strict';
/* Nerfed gorcave: the lobby neptr. One plain Anthropic Messages API call, NO tools,
   system prompt = the persona file only. Nothing here can reach the real gorcave.
   Env: ANTHROPIC_API_KEY (from the systemd EnvironmentFile), NMG_GOR_PERSONA, NMG_GOR_MODEL. */
const fs = require('fs');
const os = require('os');
const path = require('path');

const MODEL = process.env.NMG_GOR_MODEL || 'claude-haiku-4-5-20251001';
const PERSONA_PATH = process.env.NMG_GOR_PERSONA || path.join(os.homedir(), 'EMGOR_SKILLS', '_NERFED_GORCAVE.md');
const MAX_IN = 400, MAX_TOKENS = 300, TIMEOUT_MS = 20000;
const PER_CLIENT_MS = 5000, GLOBAL_PER_HOUR = 60, HISTORY_PAIRS = 8;
const FAIL = '…neptr is thinking too hard. try again.';

let persona = null;                    // loaded once, on first use
const history = [];                    // [{role, content}], rolling, memory only
const stamps = [];                     // global request times, last hour

function clean(s) { return String(s == null ? '' : s).replace(/[^\x20-\x7e -￿]/g, ' ').replace(/[<>]/g, '').replace(/\s+/g, ' ').trim(); }
function loadPersona() {
  if (persona === null) persona = fs.readFileSync(PERSONA_PATH, 'utf8');
  return persona;
}

// client: per-connection state object (we stash gorAt / gorBusy on it)
// ctx: { name, reply(obj) -> asker only, broadcast(obj) -> whole room }
async function handleGor(client, text, ctx) {
  const now = Date.now();
  text = clean(text).slice(0, MAX_IN);
  if (!text) return;
  const fail = function () { ctx.reply({ t: 'gor', from: 'neptr', text: FAIL }); };
  if (client.gorBusy || now - (client.gorAt || 0) < PER_CLIENT_MS) return fail();
  while (stamps.length && now - stamps[0] > 3600000) stamps.shift();
  if (stamps.length >= GLOBAL_PER_HOUR) return fail();
  client.gorAt = now; client.gorBusy = true; stamps.push(now);
  const name = clean(ctx.name).slice(0, 16) || 'pilot';
  try {
    const key = process.env.ANTHROPIC_API_KEY;
    if (!key) throw new Error('no key');
    const sys = loadPersona();
    const messages = history.slice(-HISTORY_PAIRS * 2).concat([{ role: 'user', content: name + ': ' + text }]);
    const res = await fetch('https://api.anthropic.com/v1/messages', {
      method: 'POST',
      headers: { 'content-type': 'application/json', 'x-api-key': key, 'anthropic-version': '2023-06-01' },
      body: JSON.stringify({ model: MODEL, max_tokens: MAX_TOKENS, system: sys, messages: messages }),
      signal: AbortSignal.timeout(TIMEOUT_MS)
    });
    if (!res.ok) throw new Error('http ' + res.status);
    const j = await res.json();
    const out = clean((j.content || []).filter(function (b) { return b && b.type === 'text'; }).map(function (b) { return b.text; }).join(' ')).slice(0, 600);
    if (!out) throw new Error('empty');
    history.push({ role: 'user', content: name + ': ' + text }, { role: 'assistant', content: out });
    while (history.length > HISTORY_PAIRS * 2) history.shift();
    ctx.broadcast({ t: 'gor', from: 'neptr', text: out, re: name });
  } catch (e) {
    fail();
  } finally {
    client.gorBusy = false;
  }
}
module.exports = { handleGor };

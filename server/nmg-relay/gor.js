/* Nerfed gorcave: the lobby neptr. One non-interactive `claude -p` run (the Pi's own Claude Code
   login, i.e. the subscription, no API key). ALL tools off, no MCP, scratch cwd, clean env.
   Env: NMG_GOR_PERSONA, NMG_GOR_MODEL (default sonnet), CLAUDE_BIN, NMG_GOR_SANDBOX. */
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

const MODEL = process.env.NMG_GOR_MODEL || 'sonnet';
const CLAUDE_BIN = process.env.CLAUDE_BIN || 'claude';
const PERSONA_PATH = process.env.NMG_GOR_PERSONA || path.join(os.homedir(), 'EMGOR_SKILLS', '_NERFED_GORCAVE.md');
const SANDBOX = process.env.NMG_GOR_SANDBOX || path.join(os.homedir(), 'nmg-relay', 'sandbox');
const MAX_IN = 400, TIMEOUT_MS = 25000;
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


// Spawn claude -p: zero tools, no MCP, no slash commands, no session saved, closed stdin, hard kill at TIMEOUT_MS.
function runClaude(sys, prompt) {
  return new Promise(function (resolve, reject) {
    try { fs.mkdirSync(SANDBOX, { recursive: true }); } catch (e) {}
    const env = { PATH: process.env.PATH || '/usr/local/bin:/usr/bin:/bin', HOME: process.env.HOME || os.homedir(), LANG: 'C.UTF-8' };
    if (process.env.USER) env.USER = process.env.USER;
    const args = ['-p', '--model', MODEL, '--system-prompt', sys, '--tools', '', '--strict-mcp-config',
      '--mcp-config', '{"mcpServers":{}}', '--disable-slash-commands', '--no-session-persistence',
      '--output-format', 'text', '--', prompt];
    const p = spawn(CLAUDE_BIN, args, { cwd: SANDBOX, env: env, stdio: ['ignore', 'pipe', 'ignore'] });
    let buf = '', done = false;
    const t = setTimeout(function () { if (!done) { done = true; p.kill('SIGKILL'); reject(new Error('timeout')); } }, TIMEOUT_MS);
    p.stdout.on('data', function (d) { if (buf.length < 20000) buf += d; });
    p.on('error', function (e) { if (!done) { done = true; clearTimeout(t); reject(e); } });
    p.on('close', function (code) { if (done) return; done = true; clearTimeout(t); code === 0 ? resolve(buf) : reject(new Error('exit ' + code)); });
  });
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
    const sys = loadPersona();
    const convo = history.slice(-HISTORY_PAIRS * 2).map(function (m) { return (m.role === 'user' ? '' : 'neptr: ') + m.content; });
    const prompt = (convo.length ? 'Recent chat:\n' + convo.join('\n') + '\n\n' : '') + 'Reply to the latest message only.\nLatest message:\n' + name + ': ' + text;
    const raw = await runClaude(sys, prompt);
    const out = clean(raw).slice(0, 600);
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

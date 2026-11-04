'use strict';
/* Leaderboard counters for nmg-relay, derived only from frames the relay already sees (no new client frames):
   pos (distinct planet pids landed per session), kill (credit to the killer), sm.end (winner slot), prof.save (dex size, dive depth).
   Pure: takes the `board` store object, mutates it, returns it. Keyed by lowercase display name. */
const MAX_ROWS = 500;
const FIELDS = ['catches', 'wins', 'depth', 'planets', 'kills'];

function row(board, name) {
  const k = String(name || '').toLowerCase().slice(0, 16);
  if (!k || k === '__proto__') return null;
  if (!Object.prototype.hasOwnProperty.call(board, k)) {
    const keys = Object.keys(board);
    if (keys.length >= MAX_ROWS) {      // evict the least-recently-seen row
      let old = keys[0];
      for (let i = 1; i < keys.length; i++) if (board[keys[i]].at < board[old].at) old = keys[i];
      delete board[old];
    }
    board[k] = { name: String(name).slice(0, 16), catches: 0, wins: 0, depth: 0, planets: 0, kills: 0, at: 0 };
  }
  return board[k];
}
function bump(board, name, field, by, now) {
  const r = row(board, name);
  if (!r || FIELDS.indexOf(field) < 0) return false;
  r[field] += by | 0; r.at = now; return true;
}
function best(board, name, field, v, now) {      // monotone max (catches, depth)
  const r = row(board, name);
  if (!r || FIELDS.indexOf(field) < 0 || !(v > r[field])) return false;
  r[field] = Math.min(9999, v | 0); r.at = now; return true;
}
// profile blob -> catches (dex species seen), depth (fractal dive stack length)
function fromProfile(board, name, data, now) {
  if (!data || typeof data !== 'object') return false;
  let a = false, b = false;
  if (data.dex && data.dex.seen && typeof data.dex.seen === 'object' && !Array.isArray(data.dex.seen)) a = best(board, name, 'catches', Object.keys(data.dex.seen).length, now);
  if (Array.isArray(data.depthStack)) b = best(board, name, 'depth', data.depthStack.length, now);
  return a || b;
}
// sm.end result.winner is a fighter slot; slot i = i-th entry of the room's fighter list (assumption, see docs/GORCAVE-LOG.md Q38)
function winnerName(result, fighterNames) {
  if (!result || typeof result !== 'object' || !Number.isInteger(result.winner) || result.winner < 0) return null;
  return fighterNames[result.winner] || null;
}
function top(board, n) {
  return Object.keys(board).map((k) => board[k]).sort((a, b) => (b.wins + b.kills + b.catches + b.planets + b.depth) - (a.wins + a.kills + a.catches + a.planets + a.depth)).slice(0, n || 100);
}
module.exports = { row, bump, best, fromProfile, winnerName, top, FIELDS, MAX_ROWS };

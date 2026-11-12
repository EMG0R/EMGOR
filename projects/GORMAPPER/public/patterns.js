// GORMAPPER pattern manager — loads /patterns/*.glsl, compiles them into
// programs, and hot-swaps them when the server reports a file change (SSE).
// A compile error never kills anything: the zone keeps its last working
// program and the error text goes to onError (the editor shows it, the
// output ignores it).

import { wrapPattern, parsePatternMeta } from './shaders.js';

// ---- generic GL program helpers (also used by renderer.js) ------------------

export function compileShader(gl, type, src) {
  const sh = gl.createShader(type);
  gl.shaderSource(sh, src);
  gl.compileShader(sh);
  if (!gl.getShaderParameter(sh, gl.COMPILE_STATUS)) {
    const log = gl.getShaderInfoLog(sh);
    gl.deleteShader(sh);
    throw new Error(log);
  }
  return sh;
}

export function linkProgram(gl, vsSrc, fsSrc) {
  const vs = compileShader(gl, gl.VERTEX_SHADER, vsSrc);
  const fs = compileShader(gl, gl.FRAGMENT_SHADER, fsSrc);
  const prog = gl.createProgram();
  gl.attachShader(prog, vs);
  gl.attachShader(prog, fs);
  gl.linkProgram(prog);
  gl.deleteShader(vs);
  gl.deleteShader(fs);
  if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
    const log = gl.getProgramInfoLog(prog);
    gl.deleteProgram(prog);
    throw new Error(log);
  }
  const uniforms = {};
  const n = gl.getProgramParameter(prog, gl.ACTIVE_UNIFORMS);
  for (let i = 0; i < n; i++) {
    const info = gl.getActiveUniform(prog, i);
    uniforms[info.name] = gl.getUniformLocation(prog, info.name);
  }
  return { prog, u: uniforms };
}

// ---- pattern manager --------------------------------------------------------

export function createPatternManager(gl, zoneVS, handlers) {
  const h = handlers || {};
  // id → { id, name, params, program: {prog,u}|null, error: string|null }
  const entries = new Map();

  async function loadOne(id) {
    let entry = entries.get(id);
    if (!entry) {
      entry = { id, name: id, params: [], program: null, error: null };
      entries.set(id, entry);
    }
    try {
      const r = await fetch('/patterns/' + encodeURIComponent(id) + '.glsl?t=' + Date.now());
      if (!r.ok) throw new Error('HTTP ' + r.status);
      const src = await r.text();
      const meta = parsePatternMeta(src, id);
      entry.name = meta.name;
      entry.params = meta.params;
      const program = linkProgram(gl, zoneVS, wrapPattern(src));   // throws on GLSL error
      if (entry.program) gl.deleteProgram(entry.program.prog);
      entry.program = program;
      entry.error = null;
      if (h.onUpdate) h.onUpdate(id);
    } catch (e) {
      entry.error = String(e.message || e);
      if (h.onError) h.onError(id, entry.error);   // last working program kept
    }
  }

  async function refreshList() {
    try {
      const list = await (await fetch('/api/patterns')).json();
      const seen = new Set();
      for (const p of list) {
        seen.add(p.id);
        const known = entries.get(p.id);
        if (known) {          // keep program; refresh metadata from listing
          known.name = p.name || known.name;
          known.params = p.params || known.params;
        } else {
          await loadOne(p.id);
        }
      }
      for (const id of [...entries.keys()]) {
        if (!seen.has(id)) {  // pattern file deleted
          const e = entries.get(id);
          if (e.program) gl.deleteProgram(e.program.prog);
          entries.delete(id);
        }
      }
      if (h.onListChange) h.onListChange(list);
    } catch (e) { /* server briefly down */ }
  }

  return {
    init: refreshList,
    refreshList,
    reload: loadOne,
    get(id) {
      const e = entries.get(id);
      return e && e.program ? e.program : null;
    },
    list() {
      return [...entries.values()].map((e) => ({ id: e.id, name: e.name, params: e.params }));
    },
    errorOf(id) {
      const e = entries.get(id);
      return e ? e.error : null;
    },
  };
}

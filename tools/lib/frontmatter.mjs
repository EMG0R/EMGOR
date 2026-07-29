// Minimal YAML-frontmatter parser for the EMGOR universe contract.
// Supports exactly the subset in docs/universe-schema.md:
//   - scalars (strings, numbers, booleans, YYYY-MM-DD dates-as-strings)
//   - inline arrays:            tags: [max-for-live, synth]
//   - inline objects:           { label: "Live demo", url: "ciesen.html" }
//   - block lists (nested):     downloads:\n  - files/THE_PURP.amxd
//     including lists of inline objects.
// Zero dependencies. Not a general YAML parser on purpose.

/**
 * Split a string on commas that are not inside quotes, brackets or braces.
 */
function splitTop(s) {
  const parts = [];
  let cur = '';
  let depth = 0;
  let quote = null;
  for (const ch of s) {
    if (quote) {
      cur += ch;
      if (ch === quote) quote = null;
      continue;
    }
    if (ch === '"' || ch === "'") { quote = ch; cur += ch; continue; }
    if (ch === '[' || ch === '{') { depth++; cur += ch; continue; }
    if (ch === ']' || ch === '}') { depth--; cur += ch; continue; }
    if (ch === ',' && depth === 0) { parts.push(cur); cur = ''; continue; }
    cur += ch;
  }
  if (cur.trim() !== '') parts.push(cur);
  return parts.map((p) => p.trim());
}

function unquote(s) {
  s = s.trim();
  if (s.length >= 2 && ((s[0] === '"' && s.at(-1) === '"') || (s[0] === "'" && s.at(-1) === "'"))) {
    return s.slice(1, -1);
  }
  return s;
}

/** Parse a scalar value: booleans, null, numbers; everything else stays a string. */
function parseScalar(raw) {
  const s = raw.trim();
  const bare = unquote(s);
  if (bare !== s) return bare; // quoted => always string
  if (s === 'true') return true;
  if (s === 'false') return false;
  if (s === 'null' || s === '~' || s === '') return null;
  // Keep dates (YYYY-MM-DD) and anything non-numeric as strings.
  if (/^-?\d+(\.\d+)?$/.test(s)) return Number(s);
  return s;
}

/** Parse an inline value: {..} object, [..] array, or scalar. */
function parseInline(raw) {
  const s = raw.trim();
  if (s.startsWith('{') && s.endsWith('}')) {
    const obj = {};
    for (const part of splitTop(s.slice(1, -1))) {
      const i = part.indexOf(':');
      if (i === -1) throw new Error(`Bad inline object entry: "${part}" in "${s}"`);
      obj[unquote(part.slice(0, i))] = parseInline(part.slice(i + 1));
    }
    return obj;
  }
  if (s.startsWith('[') && s.endsWith(']')) {
    return splitTop(s.slice(1, -1)).map(parseInline);
  }
  return parseScalar(s);
}

/**
 * Parse frontmatter + body from a markdown file's full text.
 * Returns { data, body }. data is null if the file has no frontmatter block.
 */
export function parseFrontmatter(text) {
  const norm = text.replace(/^﻿/, '').replace(/\r\n/g, '\n');
  const m = norm.match(/^---\n([\s\S]*?)\n---(\n|$)/);
  if (!m) return { data: null, body: norm };
  const body = norm.slice(m[0].length);
  const lines = m[1].split('\n');
  const data = {};
  let i = 0;
  while (i < lines.length) {
    const line = lines[i];
    if (line.trim() === '' || line.trim().startsWith('#')) { i++; continue; }
    if (/^\s/.test(line)) {
      throw new Error(`Unexpected indented line outside a list (line ${i + 1} of frontmatter): "${line}"`);
    }
    const km = line.match(/^([A-Za-z0-9_-]+):\s*(.*)$/);
    if (!km) throw new Error(`Cannot parse frontmatter line: "${line}"`);
    const key = km[1];
    // Strip trailing comments only when unambiguous (value not quoted/inline).
    let rest = km[2];
    if (rest !== '' && !/^["'{[]/.test(rest)) {
      rest = rest.replace(/\s+#.*$/, '');
    }
    if (rest !== '') {
      data[key] = parseInline(rest);
      i++;
      continue;
    }
    // Empty value: expect an indented block list of "- item" lines.
    const items = [];
    i++;
    while (i < lines.length) {
      const l = lines[i];
      if (l.trim() === '' || l.trim().startsWith('#')) { i++; continue; }
      const im = l.match(/^\s+-\s+(.*)$/);
      if (!im) break;
      let item = im[1];
      if (!/^["'{[]/.test(item)) item = item.replace(/\s+#.*$/, '');
      items.push(parseInline(item));
      i++;
    }
    data[key] = items;
  }
  return { data, body };
}

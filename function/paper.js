// =============================================================================
// paper — public, versioned paper editor storage.
//
//   GET  /.netlify/functions/paper?id=X            -> {id, html, savedAt, version} | 404
//   GET  /.netlify/functions/paper?id=X&history=1  -> [{version, savedAt, bytes}] newest first
//   GET  /.netlify/functions/paper?id=X&version=N  -> {id, html, savedAt, version}
//   POST /.netlify/functions/paper  {id, html}     -> {ok, id, version, savedAt}
//
// Anyone can save; every save is a new immutable version. There is no delete
// endpoint and nothing is ever overwritten except the `current` pointer.
// Empty documents are allowed.
//
// Blobs (store "papers"):
//   <id>/v/<000001>  -> the html of that version (metadata: savedAt, bytes)
//   <id>/current     -> JSON {version, savedAt, html}
// =============================================================================

const { getStore } = require('@netlify/blobs');

const IDS = ['digital-luthier', 'neptr-performance-system', 'neuralgrid', 'demiurgeos',
    'thesis', 'nam-csound', 'bouba', 'omniplex', 'open-pedal'];
const MAX_BODY = 500 * 1024;

function json(statusCode, obj) {
    return {
        statusCode,
        headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' },
        body: JSON.stringify(obj),
    };
}

// ---- sanitizer: allowlist of tags, only href on <a> ------------------------
const ALLOWED = new Set(['p', 'div', 'br', 'b', 'strong', 'i', 'em', 'ul', 'ol', 'li', 'h1', 'h2', 'h3', 'a']);
const VOID = new Set(['br']);
const DROP_CONTENT = new Set(['script', 'style', 'iframe', 'object', 'embed', 'noscript', 'template', 'textarea', 'title', 'svg', 'math']);

function safeHref(v) {
    // decode the entities that matter, strip control chars/whitespace, then check scheme
    const d = v.replace(/&#x([0-9a-f]+);?/gi, (m, h) => String.fromCharCode(parseInt(h, 16)))
        .replace(/&#(\d+);?/g, (m, n) => String.fromCharCode(parseInt(n, 10)))
        .replace(/&colon;/gi, ':').replace(/&tab;|&newline;/gi, '')
        .replace(/[\u0000- \u007f-\u009f]/g, '');
    if (/^https?:/i.test(d)) return true;
    if (/^[a-z][a-z0-9+.-]*:/i.test(d)) return false; // any other scheme
    if (d.startsWith('//')) return false;
    return true; // relative, #anchor, ?query
}

function escText(s) {
    return s.replace(/&(?!(?:[a-z][a-z0-9]{1,31}|#\d{1,7}|#x[0-9a-f]{1,6});)/gi, '&amp;')
        .replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
function escAttr(s) {
    return s.replace(/&(?!(?:[a-z][a-z0-9]{1,31}|#\d{1,7}|#x[0-9a-f]{1,6});)/gi, '&amp;')
        .replace(/"/g, '&quot;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function sanitize(input) {
    const src = String(input == null ? '' : input).replace(/<!--[\s\S]*?(-->|$)/g, '');
    const tagRe = /<(\/?)([a-zA-Z][a-zA-Z0-9]*)((?:"[^"]*"|'[^']*'|[^>"'])*)>/g;
    let out = '', last = 0, m, skipUntil = null;
    const stack = [];
    while ((m = tagRe.exec(src))) {
        const text = src.slice(last, m.index);
        last = tagRe.lastIndex;
        const closing = m[1] === '/', name = m[2].toLowerCase();
        if (skipUntil) {
            if (closing && name === skipUntil) skipUntil = null;
            continue;
        }
        if (text) out += escText(text);
        if (!closing && DROP_CONTENT.has(name)) { skipUntil = name; continue; }
        if (!ALLOWED.has(name)) continue;
        if (closing) {
            const i = stack.lastIndexOf(name);
            if (i === -1) continue;
            while (stack.length > i) out += '</' + stack.pop() + '>';
            continue;
        }
        let attrs = '';
        if (name === 'a') {
            const hm = /(?:^|\s)href\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s"'>]+))/i.exec(m[3]);
            const href = hm ? (hm[1] != null ? hm[1] : hm[2] != null ? hm[2] : hm[3]) : null;
            if (href != null && safeHref(href)) attrs = ' href="' + escAttr(href) + '"';
        }
        if (VOID.has(name)) { out += '<' + name + '>'; continue; }
        stack.push(name);
        out += '<' + name + attrs + '>';
    }
    if (!skipUntil) {
        const rest = src.slice(last);
        // a trailing unterminated "<..." is plain text, escaped
        out += escText(rest);
    }
    while (stack.length) out += '</' + stack.pop() + '>';
    return out;
}

const pad = (n) => String(n).padStart(6, '0');
const vKey = (id, n) => id + '/v/' + pad(n);

async function listVersions(store, id) {
    const nums = [];
    const res = await store.list({ prefix: id + '/v/' });
    for (const b of (res && res.blobs) || []) {
        const n = parseInt(b.key.slice((id + '/v/').length), 10);
        if (!isNaN(n)) nums.push(n);
    }
    return nums.sort((a, b) => b - a);
}

exports.sanitize = sanitize;
exports.IDS = IDS;

exports.handler = async function (event) {
    try {
        const store = getStore('papers');

        if (event.httpMethod === 'GET') {
            const params = event.queryStringParameters || {};
            const id = params.id;
            if (!IDS.includes(id)) return json(400, { error: 'unknown paper id' });

            if (params.history) {
                const nums = await listVersions(store, id);
                const rows = await Promise.all(nums.map(async (n) => {
                    const r = await store.getMetadata(vKey(id, n));
                    const md = (r && r.metadata) || {};
                    return { version: n, savedAt: md.savedAt || null, bytes: Number(md.bytes) || 0 };
                }));
                return json(200, rows);
            }

            if (params.version) {
                const n = parseInt(params.version, 10);
                if (!(n > 0)) return json(400, { error: 'bad version' });
                const r = await store.getWithMetadata(vKey(id, n), { type: 'text' });
                if (!r || r.data == null) return json(404, { error: 'no such version' });
                const md = r.metadata || {};
                return json(200, { id, html: r.data, savedAt: md.savedAt || null, version: n });
            }

            const raw = await store.get(id + '/current', { type: 'text' });
            if (!raw) return json(404, { error: 'no saved version yet' });
            let cur;
            try { cur = JSON.parse(raw); } catch (e) { return json(500, { error: 'corrupt current' }); }
            return json(200, { id, html: cur.html, savedAt: cur.savedAt, version: cur.version });
        }

        if (event.httpMethod === 'POST') {
            const body = event.body || '';
            const size = event.isBase64Encoded ? Buffer.byteLength(body, 'base64') : Buffer.byteLength(body);
            if (size > MAX_BODY) return json(413, { error: 'body too large' });
            let payload;
            try {
                payload = JSON.parse(event.isBase64Encoded ? Buffer.from(body, 'base64').toString('utf8') : (body || '{}'));
            } catch (e) {
                return json(400, { error: 'malformed JSON body' });
            }
            const { id, html } = payload || {};
            if (!IDS.includes(id)) return json(400, { error: 'unknown paper id' });
            if (typeof html !== 'string') return json(400, { error: 'html (string) is required' });

            const clean = sanitize(html);
            const savedAt = new Date().toISOString();
            const bytes = Buffer.byteLength(clean);

            // next version number; onlyIfNew guarantees we never overwrite an existing version
            let n = ((await listVersions(store, id))[0] || 0) + 1;
            for (let tries = 0; ; tries++) {
                const r = await store.set(vKey(id, n), clean, { onlyIfNew: true, metadata: { savedAt, bytes } });
                if (!r || r.modified !== false) break;
                if (tries > 20) return json(409, { error: 'version conflict, retry' });
                n++;
            }
            await store.set(id + '/current', JSON.stringify({ version: n, savedAt, html: clean }));
            return json(200, { ok: true, id, version: n, savedAt });
        }

        return json(405, { error: 'method not allowed' });
    } catch (err) {
        console.error('paper error:', err);
        return json(500, { error: 'internal error' });
    }
};

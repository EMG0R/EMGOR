/* mdlite.js — tiny dependency-free markdown renderer for the EMGOR universe.
   Supports: YAML-ish frontmatter parsing, headings, bold/italic, inline code,
   fenced code blocks, links, images, ul/ol lists, blockquotes, hr.
   Exposes window.MDLITE = { parseFrontmatter, render }. */
(function () {
    'use strict';

    // ─── frontmatter ───────────────────────────────────────────
    // Parses a small YAML subset: scalars, inline arrays [a, b],
    // "- item" block lists, and inline objects { k: v, k2: v2 }.
    function parseFrontmatter(text) {
        var meta = {};
        if (!/^---\s*\n/.test(text)) return { meta: meta, body: text };
        var end = text.indexOf('\n---', 3);
        if (end === -1) return { meta: meta, body: text };
        var fm = text.slice(4, end);
        var body = text.slice(end + 4).replace(/^[^\n]*\n?/, ''); // drop rest of "---" line
        var lines = fm.split('\n');
        var curKey = null;
        for (var i = 0; i < lines.length; i++) {
            var line = lines[i];
            if (!line.trim()) continue;
            var listMatch = line.match(/^\s+-\s+(.*)$/);
            if (listMatch && curKey) {
                if (!Array.isArray(meta[curKey])) meta[curKey] = [];
                meta[curKey].push(parseScalar(listMatch[1]));
                continue;
            }
            var kv = line.match(/^([A-Za-z0-9_-]+):\s*(.*)$/);
            if (kv) {
                curKey = kv[1];
                var val = kv[2].trim();
                if (val === '') { meta[curKey] = []; continue; } // block list follows
                meta[curKey] = parseScalar(val);
            }
        }
        return { meta: meta, body: body };
    }

    function parseScalar(v) {
        v = v.trim();
        // strip trailing comment-free quotes
        if (/^".*"$/.test(v) || /^'.*'$/.test(v)) return v.slice(1, -1);
        if (v === 'true') return true;
        if (v === 'false') return false;
        // inline array
        if (/^\[.*\]$/.test(v)) {
            return v.slice(1, -1).split(',').map(function (s) { return parseScalar(s); })
                .filter(function (s) { return s !== ''; });
        }
        // inline object { label: "x", url: "y" }
        if (/^\{.*\}$/.test(v)) {
            var obj = {};
            v.slice(1, -1).split(',').forEach(function (pair) {
                var m = pair.match(/^\s*([A-Za-z0-9_-]+)\s*:\s*(.*)\s*$/);
                if (m) obj[m[1]] = parseScalar(m[2]);
            });
            return obj;
        }
        return v;
    }

    // ─── escaping ──────────────────────────────────────────────
    function esc(s) {
        return s.replace(/&/g, '&amp;').replace(/</g, '&lt;')
                .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
    }
    function escAttr(s) { return esc(String(s)); }

    // ─── inline formatting ─────────────────────────────────────
    function inline(src) {
        // protect code spans first
        var codes = [];
        var s = src.replace(/`([^`]+)`/g, function (_, c) {
            codes.push('<code>' + esc(c) + '</code>');
            return '\u0000' + (codes.length - 1) + '\u0000';
        });
        s = esc(s);
        // images ![alt](url)
        s = s.replace(/!\[([^\]]*)\]\(([^)\s]+)\)/g, function (_, alt, url) {
            return '<img src="' + escAttr(url) + '" alt="' + escAttr(alt) + '" loading="lazy">';
        });
        // links [text](url)
        s = s.replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, function (_, txt, url) {
            var ext = /^https?:\/\//.test(url);
            return '<a href="' + escAttr(url) + '"' +
                (ext ? ' target="_blank" rel="noopener"' : '') + '>' + txt + '</a>';
        });
        // bold then italic
        s = s.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
        s = s.replace(/(^|[^*])\*([^*\n]+)\*/g, '$1<em>$2</em>');
        s = s.replace(/(^|[\s(])_([^_\n]+)_(?=[\s).,;:!?]|$)/g, '$1<em>$2</em>');
        // restore code spans
        s = s.replace(/\u0000(\d+)\u0000/g, function (_, i) { return codes[+i]; });
        return s;
    }

    // ─── block-level rendering ─────────────────────────────────
    function render(md) {
        var lines = md.replace(/\r\n?/g, '\n').split('\n');
        var out = [];
        var i = 0, inCode = false, codeBuf = [], codeLang = '';
        var listType = null; // 'ul' | 'ol'
        var para = [];

        function flushPara() {
            if (para.length) {
                out.push('<p>' + inline(para.join(' ')) + '</p>');
                para = [];
            }
        }
        function closeList() {
            if (listType) { out.push('</' + listType + '>'); listType = null; }
        }

        for (i = 0; i < lines.length; i++) {
            var line = lines[i];

            // fenced code
            var fence = line.match(/^```\s*(\S*)\s*$/);
            if (fence) {
                if (!inCode) {
                    flushPara(); closeList();
                    inCode = true; codeLang = fence[1]; codeBuf = [];
                } else {
                    out.push('<pre class="md-code"' +
                        (codeLang ? ' data-lang="' + escAttr(codeLang) + '"' : '') +
                        '><code>' + esc(codeBuf.join('\n')) + '</code></pre>');
                    inCode = false;
                }
                continue;
            }
            if (inCode) { codeBuf.push(line); continue; }

            // blank
            if (!line.trim()) { flushPara(); closeList(); continue; }

            // heading
            var h = line.match(/^(#{1,6})\s+(.*)$/);
            if (h) {
                flushPara(); closeList();
                var lv = Math.min(h[1].length + 1, 6); // h1 in md -> h2 (panel owns h1)
                out.push('<h' + lv + '>' + inline(h[2]) + '</h' + lv + '>');
                continue;
            }

            // hr
            if (/^(\s*[-*_]){3,}\s*$/.test(line)) {
                flushPara(); closeList(); out.push('<hr>'); continue;
            }

            // blockquote
            var bq = line.match(/^>\s?(.*)$/);
            if (bq) {
                flushPara(); closeList();
                var bqLines = [bq[1]];
                while (i + 1 < lines.length && /^>\s?/.test(lines[i + 1])) {
                    bqLines.push(lines[++i].replace(/^>\s?/, ''));
                }
                out.push('<blockquote><p>' + inline(bqLines.join(' ')) + '</p></blockquote>');
                continue;
            }

            // lists
            var ul = line.match(/^\s*[-*+]\s+(.*)$/);
            var ol = line.match(/^\s*\d+[.)]\s+(.*)$/);
            if (ul || ol) {
                flushPara();
                var want = ul ? 'ul' : 'ol';
                if (listType !== want) { closeList(); out.push('<' + want + '>'); listType = want; }
                out.push('<li>' + inline((ul || ol)[1]) + '</li>');
                continue;
            }

            // paragraph text
            para.push(line.trim());
        }
        if (inCode) {
            out.push('<pre class="md-code"><code>' + esc(codeBuf.join('\n')) + '</code></pre>');
        }
        flushPara(); closeList();
        return out.join('\n');
    }

    window.MDLITE = { parseFrontmatter: parseFrontmatter, render: render };
})();

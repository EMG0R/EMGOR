/* Live papers: pulls the current saved version from the paper function and
   renders it through a client-side allowlist (the server sanitizes too). */
(function () {
  var API = '/.netlify/functions/paper?id=';
  var OK = { P:1, BR:1, B:1, STRONG:1, I:1, EM:1, UL:1, OL:1, LI:1, H1:1, H2:1, H3:1, A:1, DIV:1 };

  function clean(node, out) {
    for (var c = node.firstChild; c; c = c.nextSibling) {
      if (c.nodeType === 3) { out.appendChild(document.createTextNode(c.nodeValue)); continue; }
      if (c.nodeType !== 1) continue;
      if (!OK[c.tagName]) { if (!/^(SCRIPT|STYLE|IFRAME|OBJECT|EMBED|SVG|MATH|TEMPLATE|TEXTAREA|NOSCRIPT)$/i.test(c.tagName)) clean(c, out); continue; }
      var el = document.createElement(c.tagName.toLowerCase());
      if (c.tagName === 'A') {
        var h = c.getAttribute('href') || '';
        if (/^(https?:|\/(?!\/)|#|[^:]*$)/i.test(h)) { el.setAttribute('href', h); el.setAttribute('rel', 'noopener'); }
      }
      clean(c, el);
      out.appendChild(el);
    }
    return out;
  }
  function render(html) {
    var doc = new DOMParser().parseFromString('<body>' + html, 'text/html');
    return clean(doc.body, document.createDocumentFragment());
  }
  function fetchPaper(id) {
    return fetch(API + encodeURIComponent(id), { cache: 'no-store' })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) { return j && typeof j.html === 'string' && j.html.trim() ? j : null; })
      .catch(function () { return null; });
  }
  function text(n) { return (n.textContent || '').replace(/\s+/g, ' ').trim(); }

  // split live html into {title, abstract}
  function parts(html) {
    var frag = render(html), box = document.createElement('div');
    box.appendChild(frag);
    var blocks = Array.prototype.slice.call(box.children);
    var title = blocks.length ? text(blocks[0]) : '';
    var abs = '', i;
    for (i = 0; i < blocks.length; i++) {
      if (/^abstract:?$/i.test(text(blocks[i]))) {
        for (var k = i + 1; k < blocks.length; k++) {
          var t = text(blocks[k]);
          if (/^H[1-3]$/.test(blocks[k].tagName) || (blocks[k].children.length === 1 && /^(B|STRONG)$/.test(blocks[k].firstChild.tagName) && text(blocks[k].firstChild) === t)) break;
          abs += (abs ? ' ' : '') + t;
        }
        break;
      }
    }
    return { title: title, abstract: abs };
  }
  function cap(s) { return s.toLowerCase().replace(/(^|[\s(-])([a-z])/g, function (m, a, b) { return a + b.toUpperCase(); }).replace(/\b(Of|A|The|In|And|To|For)\b(?!^)/g, function (m) { return m.toLowerCase(); }).replace(/^./, function (m) { return m.toUpperCase(); }); }

  function indexPage() {
    var titles = document.querySelectorAll('[data-live-title]');
    Array.prototype.forEach.call(titles, function (h) {
      var id = h.getAttribute('data-live-title');
      var a = document.querySelector('[data-live-abstract="' + id + '"]');
      fetchPaper(id).then(function (j) {
        if (!j) return;
        var p = parts(j.html);
        if (p.title) h.textContent = (p.title === p.title.toUpperCase()) ? cap(p.title) : p.title;
        if (a && p.abstract) a.textContent = p.abstract;
      });
    });
  }

  function paperPage() {
    var body = document.querySelector('[data-live-paper]');
    if (!body) return;
    var id = body.getAttribute('data-live-paper'), last = null;
    var fallback = body.cloneNode(true).childNodes;
    function tick() {
      if (document.hidden) return;
      fetchPaper(id).then(function (j) {
        if (!j || j.version === last) return;
        last = j.version;
        var tb = document.querySelector('.titleblock'); if (tb) tb.hidden = true;
        body.textContent = '';
        var live = document.createElement('div'); live.className = 'live-paper';
        live.appendChild(render(j.html));
        body.appendChild(live);
      });
    }
    tick();
    setInterval(tick, 20000);
    document.addEventListener('visibilitychange', tick);
  }

  indexPage();
  paperPage();
})();

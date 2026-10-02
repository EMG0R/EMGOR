(function () {
  'use strict';
  var IDS = {
    'digital-luthier': 'Workflow of a Modern Digital Luthier',
    'neptr-performance-system': 'Performance System of a Modern Digital Luthier',
    'neuralgrid': 'neuralGrid',
    'demiurgeos': 'DemiurgeOS',
    'thesis': 'Digital Lutherie and Expression (thesis)',
    'nam-csound': 'Integrating Neural Amp Modeling Into Csound',
    'bouba': 'BOUBA',
    'omniplex': 'Omniplex',
    'open-pedal': 'Open-Pedal'
  };
  var API = '/.netlify/functions/paper';
  var id = new URLSearchParams(location.search).get('id');
  var $ = function (s) { return document.getElementById(s); };
  var doc = $('doc'), statusEl = $('status');
  var dirty = false, previewing = false, previewHtml = '', savedAt = null, current = '';

  if (!IDS.hasOwnProperty(id)) {
    $('title').textContent = 'Unknown paper';
    doc.contentEditable = 'false';
    doc.innerHTML = '<p>Pick one:</p><ul>' + Object.keys(IDS).map(function (k) {
      return '<li><a href="edit.html?id=' + k + '">' + IDS[k] + '</a></li>';
    }).join('') + '</ul>';
    $('save').disabled = true; $('hist').disabled = true;
    return;
  }
  $('title').textContent = IDS[id];
  document.title = 'Edit: ' + IDS[id];
  $('back').href = '../index.html#/papers/' + id;

  // ---- sanitize (same allowlist as the server) ----
  var ALLOWED = { P: 1, DIV: 1, BR: 1, B: 1, STRONG: 1, I: 1, EM: 1, UL: 1, OL: 1, LI: 1, H1: 1, H2: 1, H3: 1, A: 1 };
  function safeHref(v) {
    var d = v.replace(/[\u0000- \u007f-\u009f]/g, '');
    if (/^https?:/i.test(d)) return true;
    if (/^[a-z][a-z0-9+.-]*:/i.test(d) || d.indexOf('//') === 0) return false;
    return true;
  }
  function clean(node, outParent) {
    for (var c = node.firstChild; c; c = c.nextSibling) {
      if (c.nodeType === 3) { outParent.appendChild(document.createTextNode(c.nodeValue)); continue; }
      if (c.nodeType !== 1) continue;
      var tag = c.tagName.toUpperCase();
      if (/^(SCRIPT|STYLE|IFRAME|OBJECT|EMBED|NOSCRIPT|TEMPLATE|TEXTAREA|TITLE|SVG|MATH)$/.test(tag)) continue;
      if (!ALLOWED[tag]) { clean(c, outParent); continue; }
      var el = document.createElement(tag.toLowerCase());
      if (tag === 'A') {
        var h = c.getAttribute('href');
        if (h != null && safeHref(h)) el.setAttribute('href', h);
      }
      if (tag !== 'BR') clean(c, el);
      outParent.appendChild(el);
    }
  }
  function sanitize(html) {
    var src = new DOMParser().parseFromString('<body>' + html, 'text/html').body;
    var tmp = document.createElement('div');
    clean(src, tmp);
    return tmp.innerHTML;
  }

  // ---- status ----
  function fmt(iso) {
    if (!iso) return '';
    var d = new Date(iso);
    return d.toLocaleDateString([], { month: 'short', day: 'numeric' }) + ' ' +
      d.toLocaleTimeString([], { hour: 'numeric', minute: '2-digit' }).toLowerCase().replace(' ', '');
  }
  function setStatus(t) { statusEl.textContent = t; }
  function markDirty() { if (previewing) return; dirty = true; setStatus('unsaved changes'); }
  function markSaved() {
    dirty = false;
    setStatus('saved ' + new Date(savedAt).toLocaleTimeString([], { hour: 'numeric', minute: '2-digit' }).toLowerCase().replace(' ', ''));
  }

  function setDoc(html) { doc.innerHTML = sanitize(html); }

  // ---- load ----
  function load() {
    setStatus('loading');
    fetch(API + '?id=' + id, { cache: 'no-store' }).then(function (r) {
      if (r.status === 404) return null;
      if (!r.ok) throw new Error('load failed');
      return r.json();
    }).then(function (j) {
      if (j) { setDoc(j.html); savedAt = j.savedAt; markSaved(); return; }
      return fetch('seed/' + id + '.html').then(function (r) { return r.ok ? r.text() : ''; }).then(function (t) {
        setDoc(t); dirty = false; setStatus('starting text, not saved yet');
      });
    }).catch(function () { setStatus('could not load'); });
  }

  // ---- save ----
  function post(html) {
    setStatus('saving');
    return fetch(API, {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: id, html: sanitize(html) })
    }).then(function (r) {
      return r.json().then(function (j) { if (!r.ok) throw new Error(j.error || 'save failed'); return j; });
    });
  }
  function save() {
    if (previewing) return;
    post(doc.innerHTML).then(function (j) {
      savedAt = j.savedAt; markSaved();
      if (!$('histpanel').hidden) loadHistory();
    }).catch(function (e) { setStatus('save failed: ' + e.message); });
  }

  // ---- history ----
  function loadHistory() {
    var p = $('histpanel');
    p.textContent = 'loading';
    fetch(API + '?id=' + id + '&history=1', { cache: 'no-store' }).then(function (r) { return r.json(); }).then(function (rows) {
      p.textContent = '';
      if (!rows.length) { p.textContent = 'No saved versions yet.'; return; }
      rows.forEach(function (v) {
        var d = document.createElement('div');
        d.innerHTML = 'v' + v.version + ' &nbsp; ' + fmt(v.savedAt) + ' <span>' + (v.bytes / 1024).toFixed(1) + ' KB</span>';
        d.onclick = function () { openVersion(v); };
        p.appendChild(d);
      });
    }).catch(function () { p.textContent = 'could not load history'; });
  }
  function openVersion(v) {
    fetch(API + '?id=' + id + '&version=' + v.version, { cache: 'no-store' }).then(function (r) { return r.json(); }).then(function (j) {
      if (!previewing) current = doc.innerHTML;
      previewing = true; previewHtml = j.html;
      setDoc(j.html);
      doc.contentEditable = 'false';
      $('preview').hidden = false; $('histpanel').hidden = true;
      $('pvlabel').textContent = 'Viewing v' + v.version + ', ' + fmt(v.savedAt) + ' (read only)';
      $('save').disabled = true;
    });
  }
  function backToCurrent() {
    previewing = false;
    setDoc(current);
    doc.contentEditable = 'true';
    $('preview').hidden = true; $('save').disabled = false;
  }
  $('hist').onclick = function () {
    var p = $('histpanel');
    p.hidden = !p.hidden;
    if (!p.hidden) loadHistory();
  };
  $('current').onclick = backToCurrent;
  $('restore').onclick = function () {
    post(previewHtml).then(function (j) {
      savedAt = j.savedAt; current = previewHtml; backToCurrent(); markSaved();
    }).catch(function (e) { setStatus('restore failed: ' + e.message); });
  };
  $('save').onclick = save;

  // ---- editing behaviour ----
  try { document.execCommand('defaultParagraphSeparator', false, 'p'); } catch (e) {}
  doc.addEventListener('input', markDirty);

  function blockOf(node) {
    while (node && node !== doc) {
      if (node.nodeType === 1 && /^(P|DIV|LI|H1|H2|H3)$/.test(node.tagName)) return node;
      node = node.parentNode;
    }
    return null;
  }
  function inList() {
    var s = getSelection();
    if (!s.rangeCount) return false;
    for (var n = s.anchorNode; n && n !== doc; n = n.parentNode) if (n.nodeName === 'LI') return true;
    return false;
  }

  doc.addEventListener('keydown', function (e) {
    if (previewing) return;
    var mod = e.metaKey || e.ctrlKey, k = e.key.toLowerCase();
    if (mod && k === 's') { e.preventDefault(); save(); return; }
    if (mod && k === 'b') { e.preventDefault(); document.execCommand('bold'); markDirty(); return; }
    if (mod && k === 'i') { e.preventDefault(); document.execCommand('italic'); markDirty(); return; }
    if (e.key === 'Tab' && inList()) {
      e.preventDefault();
      document.execCommand(e.shiftKey ? 'outdent' : 'indent');
      markDirty();
      return;
    }
    if (e.key === ' ' && !mod) {
      var s = getSelection();
      if (!s.rangeCount || !s.isCollapsed) return;
      var b = blockOf(s.anchorNode);
      if (!b || b.tagName === 'LI') return;
      var t = b.textContent;
      var kind = (t === '-' || t === '*') ? 'ul' : (t === '1.' ? 'ol' : null);
      if (!kind) return;
      e.preventDefault();
      b.textContent = '';
      b.appendChild(document.createElement('br'));
      var r = document.createRange(); r.setStart(b, 0); r.collapse(true);
      s.removeAllRanges(); s.addRange(r);
      document.execCommand(kind === 'ul' ? 'insertUnorderedList' : 'insertOrderedList');
      markDirty();
    }
  });

  doc.addEventListener('paste', function (e) {
    if (previewing) return;
    e.preventDefault();
    var t = (e.clipboardData || window.clipboardData).getData('text/plain');
    document.execCommand('insertText', false, t);
    markDirty();
  });

  window.addEventListener('keydown', function (e) {
    if (!e.defaultPrevented && (e.metaKey || e.ctrlKey) && e.key.toLowerCase() === 's') { e.preventDefault(); save(); }
  });
  window.addEventListener('beforeunload', function (e) {
    if (dirty) { e.preventDefault(); e.returnValue = ''; }
  });

  load();
})();

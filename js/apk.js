// APK page: photo bento (published from apk/layout.json) + localhost editor that saves through tools/apk-dev.py
(function () {
  var grid = document.getElementById('grid');
  var photosSec = document.querySelector('.photos');
  var dropHint = document.getElementById('dropHint');
  var editbar = document.getElementById('editbar');
  var statusEl = document.getElementById('editStatus');
  var SHAPES = ['sq', 'wide', 'tall', 'big'];
  var state = { slots: [] };
  var editing = false;
  var devOK = false;

  var q = new URLSearchParams(location.search);
  var local = /^(localhost|127\.0\.0\.1|\[::1\])$/.test(location.hostname);
  var wantEdit = local || q.has('edit');

  // ── layout / unit sizing ──────────────────────────────────────────
  function sizeUnit() {
    var cs = getComputedStyle(grid);
    var cols = parseInt(cs.getPropertyValue('--cols')) || 4;
    var gap = parseFloat(cs.getPropertyValue('--gap')) || 10;
    var u = (grid.clientWidth - gap * (cols - 1)) / cols;
    grid.style.setProperty('--u', u + 'px');
  }
  addEventListener('resize', function () { sizeUnit(); clampAll(); });

  function uid() { return Math.random().toString(36).slice(2, 8); }

  // ── rendering ─────────────────────────────────────────────────────
  function applyTransform(slot, el) {
    var img = el.querySelector('img');
    if (!img) return;
    img.style.transform = 'translate(' + (slot.fx * 100).toFixed(3) + '%, ' + (slot.fy * 100).toFixed(3) + '%) scale(' + slot.s.toFixed(4) + ')';
  }

  function clampSlot(slot, el) {
    var img = el.querySelector('img');
    if (!img || !img.naturalWidth) return;
    var W = el.clientWidth, H = el.clientHeight;
    var k = Math.max(W / img.naturalWidth, H / img.naturalHeight);
    var rw = img.naturalWidth * k * slot.s, rh = img.naturalHeight * k * slot.s;
    var mx = Math.max(0, (rw - W) / 2) / W, my = Math.max(0, (rh - H) / 2) / H;
    slot.fx = Math.max(-mx, Math.min(mx, slot.fx));
    slot.fy = Math.max(-my, Math.min(my, slot.fy));
    applyTransform(slot, el);
  }
  function clampAll() {
    Array.prototype.forEach.call(grid.children, function (el) {
      var slot = state.slots.filter(function (s) { return s.id === el.dataset.id; })[0];
      if (slot) clampSlot(slot, el);
    });
  }

  function render() {
    grid.innerHTML = '';
    state.slots.forEach(function (slot) {
      var el = document.createElement('div');
      el.className = 'ph' + (slot.file ? '' : ' empty');
      el.dataset.id = slot.id;
      el.dataset.shape = slot.shape || 'sq';
      if (slot.file) {
        var img = document.createElement('img');
        img.src = slot.file;
        img.alt = 'EMGOR';
        img.loading = 'lazy';
        img.decoding = 'async';
        img.onload = function () { clampSlot(slot, el); };
        el.appendChild(img);
        applyTransform(slot, el);
      }
      if (editing) attachEditor(slot, el);
      grid.appendChild(el);
    });
    sizeUnit();
    renderDownloads();
  }

  // ── downloads panel ───────────────────────────────────────────────
  function renderDownloads() {
    var box = document.getElementById('dlPhotos');
    box.innerHTML = '';
    var n = 0;
    state.slots.forEach(function (s) {
      if (!s.file) return;
      n++;
      var a = document.createElement('a');
      a.className = 'dl'; a.href = s.file; a.download = 'emgor-' + (n < 10 ? '0' : '') + n + '.jpg';
      a.textContent = 'photo ' + n;
      box.appendChild(a);
    });
    var zip = document.getElementById('dlZip');
    zip.hidden = n === 0;
    if (n === 0) { var e = document.createElement('div'); e.className = 'dl'; e.textContent = 'no photos yet'; box.appendChild(e); }
  }
  (function songs() {
    var box = document.getElementById('dlSongs');
    var pl = document.getElementById('player');
    var tracks = [];
    try { tracks = JSON.parse(pl.getAttribute('data-tracks')); } catch (e) {}
    tracks.forEach(function (t) {
      var a = document.createElement('a');
      a.className = 'dl'; a.href = t.src; a.download = '';
      a.textContent = t.name;
      var s = document.createElement('small'); s.textContent = t.src.split('.').pop(); a.appendChild(s);
      box.appendChild(a);
    });
  })();
  document.getElementById('dlToggle').addEventListener('click', function () {
    var p = document.getElementById('dlPanel');
    p.hidden = !p.hidden;
    this.setAttribute('aria-expanded', String(!p.hidden));
  });

  // ── editor ────────────────────────────────────────────────────────
  var saveT = null;
  function setStatus(t) { statusEl.textContent = t; }
  function post(path, body) {
    return fetch(path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) })
      .then(function (r) { if (!r.ok) throw new Error(r.status); return r.json(); });
  }
  function saveDraft() {
    clearTimeout(saveT);
    saveT = setTimeout(function () {
      post('/__apk/draft', { layout: state }).then(function () { setStatus('draft saved'); })
        .catch(function () { setStatus('draft NOT saved — run tools/apk-dev.py'); });
    }, 300);
  }

  function readFile(file) {
    return new Promise(function (res, rej) {
      var fr = new FileReader();
      fr.onload = function () { res(fr.result); };
      fr.onerror = rej;
      fr.readAsDataURL(file);
    });
  }
  function upload(file) {
    setStatus('uploading ' + file.name + '…');
    return readFile(file).then(function (data) { return post('/__apk/upload', { name: file.name, data: data }); });
  }
  function imageFiles(dt) {
    return Array.prototype.filter.call(dt.files || [], function (f) { return /^image\//.test(f.type); });
  }

  function addSlot(file) {
    var slot = { id: uid(), file: file || null, fx: 0, fy: 0, s: 1, shape: 'sq' };
    state.slots.push(slot);
    return slot;
  }

  function attachEditor(slot, el) {
    // tools
    var tools = document.createElement('div');
    tools.className = 'ph-tools';
    var shapeBtn = document.createElement('button');
    shapeBtn.type = 'button'; shapeBtn.textContent = slot.shape || 'sq'; shapeBtn.title = 'cycle frame shape';
    shapeBtn.addEventListener('click', function (e) {
      e.stopPropagation();
      slot.shape = SHAPES[(SHAPES.indexOf(slot.shape || 'sq') + 1) % SHAPES.length];
      render(); saveDraft();
    });
    var left = document.createElement('button');
    left.type = 'button'; left.textContent = '←'; left.title = 'move earlier';
    left.addEventListener('click', function (e) { e.stopPropagation(); move(slot, -1); });
    var right = document.createElement('button');
    right.type = 'button'; right.textContent = '→'; right.title = 'move later';
    right.addEventListener('click', function (e) { e.stopPropagation(); move(slot, 1); });
    var del = document.createElement('button');
    del.type = 'button'; del.textContent = '×'; del.title = 'remove frame (file stays on disk)';
    del.addEventListener('click', function (e) {
      e.stopPropagation();
      state.slots = state.slots.filter(function (s) { return s !== slot; });
      render(); saveDraft();
    });
    tools.appendChild(shapeBtn); tools.appendChild(left); tools.appendChild(right); tools.appendChild(del);
    el.appendChild(tools);

    if (slot.file) {
      var zoom = document.createElement('input');
      zoom.type = 'range'; zoom.className = 'ph-zoom'; zoom.min = '1'; zoom.max = '4'; zoom.step = '0.01'; zoom.value = slot.s;
      zoom.addEventListener('pointerdown', function (e) { e.stopPropagation(); });
      zoom.addEventListener('input', function () { slot.s = parseFloat(zoom.value); clampSlot(slot, el); saveDraft(); });
      el.appendChild(zoom);
    }

    // drop a file onto this frame
    el.addEventListener('dragover', function (e) { e.preventDefault(); e.stopPropagation(); el.classList.add('over'); });
    el.addEventListener('dragleave', function () { el.classList.remove('over'); });
    el.addEventListener('drop', function (e) {
      e.preventDefault(); e.stopPropagation(); el.classList.remove('over');
      var fs = imageFiles(e.dataTransfer);
      if (!fs.length) return;
      upload(fs[0]).then(function (r) {
        slot.file = r.path; slot.fx = 0; slot.fy = 0; slot.s = 1;
        render(); saveDraft();
      }).catch(function () { setStatus('upload failed — is tools/apk-dev.py running?'); });
    });

    // pan (1 pointer) / pinch (2 pointers) / wheel zoom
    var ptrs = {}, start = null;
    el.addEventListener('pointerdown', function (e) {
      if (!slot.file) return;
      el.setPointerCapture(e.pointerId);
      ptrs[e.pointerId] = { x: e.clientX, y: e.clientY };
      var keys = Object.keys(ptrs);
      if (keys.length === 1) start = { x: e.clientX, y: e.clientY, fx: slot.fx, fy: slot.fy };
      if (keys.length === 2) {
        var a = ptrs[keys[0]], b = ptrs[keys[1]];
        start = { d: Math.hypot(a.x - b.x, a.y - b.y), s: slot.s, fx: slot.fx, fy: slot.fy };
      }
      el.classList.add('dragging');
    });
    el.addEventListener('pointermove', function (e) {
      if (!ptrs[e.pointerId] || !start) return;
      ptrs[e.pointerId] = { x: e.clientX, y: e.clientY };
      var keys = Object.keys(ptrs);
      if (keys.length === 1) {
        slot.fx = start.fx + (e.clientX - start.x) / el.clientWidth;
        slot.fy = start.fy + (e.clientY - start.y) / el.clientHeight;
      } else if (keys.length >= 2) {
        var a = ptrs[keys[0]], b = ptrs[keys[1]];
        slot.s = Math.max(1, Math.min(4, start.s * Math.hypot(a.x - b.x, a.y - b.y) / start.d));
        var z = el.querySelector('.ph-zoom'); if (z) z.value = slot.s;
      }
      clampSlot(slot, el);
    });
    function up(e) {
      delete ptrs[e.pointerId];
      if (!Object.keys(ptrs).length) { start = null; el.classList.remove('dragging'); saveDraft(); }
      else { var k = Object.keys(ptrs)[0]; start = { x: ptrs[k].x, y: ptrs[k].y, fx: slot.fx, fy: slot.fy }; }
    }
    el.addEventListener('pointerup', up);
    el.addEventListener('pointercancel', up);
    el.addEventListener('wheel', function (e) {
      if (!slot.file) return;
      e.preventDefault();
      slot.s = Math.max(1, Math.min(4, slot.s * Math.exp(-e.deltaY * 0.0015)));
      var z = el.querySelector('.ph-zoom'); if (z) z.value = slot.s;
      clampSlot(slot, el); saveDraft();
    }, { passive: false });
  }

  function move(slot, dir) {
    var i = state.slots.indexOf(slot), j = i + dir;
    if (j < 0 || j >= state.slots.length) return;
    state.slots.splice(i, 1); state.slots.splice(j, 0, slot);
    render(); saveDraft();
  }

  function enableEditor() {
    editing = true;
    document.documentElement.classList.add('editing');
    editbar.hidden = false;
    dropHint.hidden = false;
    document.getElementById('addSlot').addEventListener('click', function () { addSlot(null); render(); saveDraft(); });
    document.getElementById('publish').addEventListener('click', function () {
      setStatus('publishing…');
      post('/__apk/publish', { layout: state }).then(function () {
        setStatus('✓ written to apk/layout.json — commit + push to go live');
      }).catch(function () { setStatus('publish failed — run tools/apk-dev.py'); });
    });
    // drop anywhere on the photos section (outside a frame) → new frames
    ['dragenter', 'dragover'].forEach(function (ev) {
      document.addEventListener(ev, function (e) { e.preventDefault(); photosSec.classList.add('over'); });
    });
    document.addEventListener('dragleave', function (e) { if (!e.relatedTarget) photosSec.classList.remove('over'); });
    document.addEventListener('drop', function (e) {
      e.preventDefault(); photosSec.classList.remove('over');
      var fs = imageFiles(e.dataTransfer);
      if (!fs.length) return;
      // fill empty frames first, then append
      var chain = Promise.resolve();
      fs.forEach(function (f) {
        chain = chain.then(function () { return upload(f); }).then(function (r) {
          var empty = state.slots.filter(function (s) { return !s.file; })[0];
          if (empty) { empty.file = r.path; empty.fx = 0; empty.fy = 0; empty.s = 1; }
          else addSlot(r.path);
          render(); saveDraft();
        });
      });
      chain.catch(function () { setStatus('upload failed — is tools/apk-dev.py running?'); });
    });
    setStatus(devOK ? 'editing — drop photos anywhere' : 'editor needs tools/apk-dev.py (saves disabled)');
  }

  // ── boot ──────────────────────────────────────────────────────────
  fetch('apk/layout.json', { cache: 'no-store' }).then(function (r) { return r.json(); }).catch(function () { return { slots: [] }; })
    .then(function (pub) {
      state = pub && pub.slots ? pub : { slots: [] };
      if (!wantEdit) return;
      return fetch('/__apk/draft', { cache: 'no-store' }).then(function (r) {
        if (!r.ok) throw new Error();
        devOK = true; return r.json();
      }).then(function (d) { if (d && d.slots) state = d; }).catch(function () { devOK = false; });
    })
    .then(function () {
      state.slots.forEach(function (s) { s.id = s.id || uid(); s.fx = s.fx || 0; s.fy = s.fy || 0; s.s = s.s || 1; s.shape = s.shape || 'sq'; });
      if (wantEdit) enableEditor();
      render();
    });
})();

// EPK page: three fixed photo frames (hero, a, b) published from epk/layout.json,
// plus a localhost editor that saves through tools/epk-dev.py (drop or click a frame, drag to move, pinch/scroll/slider to zoom).
(function () {
  var editbar = document.getElementById('editbar');
  var statusEl = document.getElementById('editStatus');
  var picker = document.getElementById('picker');
  var frames = {};
  Array.prototype.forEach.call(document.querySelectorAll('.frame[data-frame]'), function (el) { frames[el.dataset.frame] = el; });
  var state = { frames: {} };
  var editing = false, devOK = false, pickTarget = null;

  var q = new URLSearchParams(location.search);
  var local = /^(localhost|127\.0\.0\.1|\[::1\])$/.test(location.hostname);
  var wantEdit = local || q.has('edit') || window.EPK_EDIT === true;

  function slotOf(key) { return state.frames[key] || (state.frames[key] = { file: null, fx: 0, fy: 0, s: 1 }); }
  function applyAspect() {} // frame sizes are fixed in CSS: hero 4:5, pair 1:1 — same on every device

  function applyTransform(slot, el) {
    var img = el.querySelector('img');
    if (img) img.style.transform = 'translate(' + (slot.fx * 100).toFixed(3) + '%, ' + (slot.fy * 100).toFixed(3) + '%) scale(' + slot.s.toFixed(4) + ')';
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
  addEventListener('resize', function () { Object.keys(frames).forEach(function (k) { clampSlot(slotOf(k), frames[k]); }); });

  function render() {
    Object.keys(frames).forEach(function (key) {
      var el = frames[key], slot = slotOf(key);
      // fresh element = no stacked listeners from earlier renders
      var clean = el.cloneNode(false); clean.innerHTML = '';
      el.parentNode.replaceChild(clean, el); el = frames[key] = clean;
      el.onclick = null;
      applyAspect(slot, el);
      el.classList.toggle('empty', !slot.file);
      if (slot.file) {
        var img = document.createElement('img');
        img.src = slot.file; img.alt = 'EMGOR'; img.decoding = 'async';
        img.onload = function () { applyAspect(slot, el); clampSlot(slot, el); requestAnimationFrame(function () { img.classList.add('in'); }); };
        el.appendChild(img);
        applyTransform(slot, el);
      }
      if (editing) attachEditor(key, slot, el);
    });
  }

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
      post('/__epk/draft', { layout: state }).then(function () { setStatus('draft saved'); })
        .catch(function () { setStatus('NOT saved — start: python3 tools/epk-dev.py'); });
    }, 300);
  }
  function readFile(file) {
    return new Promise(function (res, rej) { var fr = new FileReader(); fr.onload = function () { res(fr.result); }; fr.onerror = rej; fr.readAsDataURL(file); });
  }
  function putFile(key, file) {
    if (!file || !/^image\//.test(file.type)) { setStatus('that is not an image'); return; }
    setStatus('uploading ' + file.name + '…');
    readFile(file).then(function (data) { return post('/__epk/upload', { name: file.name, data: data }); })
      .then(function (r) {
        var slot = slotOf(key);
        slot.file = r.path; slot.fx = 0; slot.fy = 0; slot.s = 1;
        render(); saveDraft();
      })
      .catch(function () { setStatus('upload failed — start: python3 tools/epk-dev.py'); });
  }
  picker.addEventListener('change', function () { if (picker.files[0] && pickTarget) putFile(pickTarget, picker.files[0]); picker.value = ''; });

  function attachEditor(key, slot, el) {
    if (slot.file) {
      var tools = document.createElement('div');
      tools.className = 'fr-tools';
      var swap = document.createElement('button');
      swap.type = 'button'; swap.textContent = 'replace';
      swap.addEventListener('click', function (e) { e.stopPropagation(); pickTarget = key; picker.click(); });
      var del = document.createElement('button');
      del.type = 'button'; del.textContent = '×'; del.title = 'clear (file stays on disk)';
      del.addEventListener('click', function (e) { e.stopPropagation(); slot.file = null; render(); saveDraft(); });
      tools.appendChild(swap); tools.appendChild(del);
      el.appendChild(tools);

      var zoom = document.createElement('input');
      zoom.type = 'range'; zoom.className = 'fr-zoom'; zoom.min = '1'; zoom.max = '4'; zoom.step = '0.01'; zoom.value = slot.s;
      zoom.addEventListener('pointerdown', function (e) { e.stopPropagation(); });
      zoom.addEventListener('input', function () { slot.s = parseFloat(zoom.value); clampSlot(slot, el); saveDraft(); });
      el.appendChild(zoom);
    } else {
      el.onclick = function () { pickTarget = key; picker.click(); };
    }

    el.addEventListener('dragover', function (e) { e.preventDefault(); e.stopPropagation(); el.classList.add('over'); });
    el.addEventListener('dragleave', function () { el.classList.remove('over'); });
    el.addEventListener('drop', function (e) {
      e.preventDefault(); e.stopPropagation(); el.classList.remove('over');
      var f = e.dataTransfer.files && e.dataTransfer.files[0];
      if (f) putFile(key, f); else setStatus('drop a file from Finder (not from a browser or Photos)');
    });

    var ptrs = {}, start = null;
    el.addEventListener('pointerdown', function (e) {
      if (!slot.file) return;
      el.setPointerCapture(e.pointerId);
      ptrs[e.pointerId] = { x: e.clientX, y: e.clientY };
      var keys = Object.keys(ptrs);
      if (keys.length === 1) start = { x: e.clientX, y: e.clientY, fx: slot.fx, fy: slot.fy };
      if (keys.length === 2) { var a = ptrs[keys[0]], b = ptrs[keys[1]]; start = { d: Math.hypot(a.x - b.x, a.y - b.y), s: slot.s }; }
      el.classList.add('dragging');
    });
    el.addEventListener('pointermove', function (e) {
      if (!ptrs[e.pointerId] || !start) return;
      ptrs[e.pointerId] = { x: e.clientX, y: e.clientY };
      var keys = Object.keys(ptrs);
      if (keys.length === 1) {
        slot.fx = start.fx + (e.clientX - start.x) / el.clientWidth;
        slot.fy = start.fy + (e.clientY - start.y) / el.clientHeight;
      } else {
        var a = ptrs[keys[0]], b = ptrs[keys[1]];
        slot.s = Math.max(1, Math.min(4, start.s * Math.hypot(a.x - b.x, a.y - b.y) / start.d));
        var z = el.querySelector('.fr-zoom'); if (z) z.value = slot.s;
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
      var z = el.querySelector('.fr-zoom'); if (z) z.value = slot.s;
      clampSlot(slot, el); saveDraft();
    }, { passive: false });
  }

  function enableEditor() {
    editing = true;
    document.documentElement.classList.add('editing');
    editbar.hidden = false;
    document.getElementById('publish').addEventListener('click', function () {
      setStatus('publishing…');
      post('/__epk/publish', { layout: state, bios: document.querySelector('.bio').innerText.trim() })
        .then(function () { setStatus('✓ saved to epk/layout.json — tell claude to push'); })
        .catch(function () { setStatus('publish failed — start: python3 tools/epk-dev.py'); });
    });
    ['dragenter', 'dragover', 'drop'].forEach(function (ev) { document.addEventListener(ev, function (e) { e.preventDefault(); }); });
    setStatus(devOK ? 'editing — drop or click a frame' : 'editor offline — start: python3 tools/epk-dev.py');
  }

  fetch('epk/layout.json', { cache: 'no-store' }).then(function (r) { return r.json(); }).catch(function () { return {}; })
    .then(function (pub) {
      if (pub && pub.frames) state = pub;
      if (!wantEdit) return;
      return fetch('/__epk/draft', { cache: 'no-store' })
        .then(function (r) { if (!r.ok) throw new Error(); devOK = true; return r.json(); })
        .then(function (d) { if (d && d.frames) state = d; })
        .catch(function () { devOK = false; });
    })
    .then(function () {
      if (wantEdit) enableEditor();
      render();
    });
})();

// neuralGrid Pi app UI. Vanilla JS, no frameworks. SSE for live state,
// POST /api/<action> for every write. Mobile-friendly: big buttons, no
// hover-only affordances (everything works on tap).
(function () {
  "use strict";

  const $ = (sel) => document.querySelector(sel);
  const post = (action, payload) =>
    fetch("/api/" + action, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload || {}),
    }).catch(() => {});

  // -- 8x8 grid mirror (also the fake-grid tap surface) --------------------
  const gridEl = $("#grid");
  const pads = [];
  for (let y = 0; y < 8; y++) {
    for (let x = 0; x < 8; x++) {
      const pad = document.createElement("div");
      pad.className = "pad";
      pad.dataset.x = x;
      pad.dataset.y = y;
      pad.addEventListener("pointerdown", () => post("fake/key", { x, y, edge: 1 }));
      pad.addEventListener("pointerup", () => post("fake/key", { x, y, edge: 0 }));
      pad.addEventListener("pointerleave", () => post("fake/key", { x, y, edge: 0 }));
      gridEl.appendChild(pad);
      pads.push(pad);
    }
  }

  function renderGrid(rows) {
    // rows: 64 [r,g,b] entries, index = y*8+x -- LOGICAL order (see
    // App.get_state's "grid" field): the mirror always shows the same
    // frame the active mode believes it's drawing to, regardless of
    // orientation.rot. Physical rotation is shown only by the compass.
    if (!rows) return;
    for (let i = 0; i < 64; i++) {
      const [r, g, b] = rows[i] || [0, 0, 0];
      pads[i].style.background = `rgb(${r},${g},${b})`;
    }
  }

  // -- orientation compass -------------------------------------------------
  // top_edge is a HARDWARE edge name (N/S/E/W) -- which physical edge of
  // the grid is currently the logical top (data.orientation.top_edge, see
  // orientation.py). Arrow points at that physical direction.
  const orientArrowEl = $("#orientArrow");
  const orientLabelEl = $("#orientLabel");
  const ORIENT_ARROW_DEG = { N: 0, E: 90, S: 180, W: 270 };
  let lastTopEdge = null;
  function renderOrientation(orientation) {
    if (!orientation || !orientArrowEl || !orientLabelEl) return;
    const edge = orientation.top_edge;
    if (edge === lastTopEdge) return;
    lastTopEdge = edge;
    orientArrowEl.style.transform = `rotate(${ORIENT_ARROW_DEG[edge] ?? 0}deg)`;
    orientLabelEl.textContent = edge;
  }

  // -- mode buttons / panels -------------------------------------------
  const modeButtons = document.querySelectorAll(".mode-btn");
  const panels = {
    direct_lab: $("#panelDirect"),
    septagraph: $("#panelSeptagraph"),
    midi: $("#panelMidi"),
  };

  modeButtons.forEach((btn) => {
    btn.addEventListener("click", () => post("mode/switch", { mode: btn.dataset.mode }));
  });

  function setActiveMode(mode) {
    modeButtons.forEach((b) => b.classList.toggle("active", b.dataset.mode === mode));
    Object.entries(panels).forEach(([name, el]) => {
      el.hidden = name !== mode;
    });
  }

  // -- direct lab panel --------------------------------------------------
  // Layout toggle: FULL = all 64 pads play the rows/segments catalog;
  // SPLIT = rows 0-3 play, rows 4-7 = on-grid sequencer; MODULETTES = all
  // 64 pads play modulettes directly, pad (x,y) -> note y*8+x (the 4x8
  // sequencer runs UI-only in FULL and MODULETTES, same as before).
  document.querySelectorAll("#layoutToggle .layout-btn").forEach((btn) => {
    btn.addEventListener("click", () => post("direct/set_layout", { mode: btn.dataset.layout }));
  });
  function renderLayoutToggle(layoutMode) {
    document.querySelectorAll("#layoutToggle .layout-btn").forEach((btn) => {
      btn.classList.toggle("active", btn.dataset.layout === layoutMode);
    });
    const hint = {
      split: "rows 0-3 play · rows 4-7 sequencer",
      modulettes: "all 64 pads -> modulettes (note = y*8+x) · sequencer UI-only",
    }[layoutMode] || "all 64 pads play · sequencer UI-only";
    $("#layoutHint").textContent = hint;
    const modNote = $("#rowConfigModulettesNote");
    if (modNote) modNote.hidden = layoutMode !== "modulettes";
  }

  // searchable-enough dropdown builder: one <select>, options grouped by
  // instrument via <optgroup> (native browser search-as-you-type already
  // works inside a long <select> on both desktop and mobile).
  function buildPointSelect(points, selectedId, onChange) {
    const sel = document.createElement("select");
    const byInstrument = {};
    points.filter((p) => p.enabled).forEach((p) => {
      (byInstrument[p.instrument] = byInstrument[p.instrument] || []).push(p);
    });
    Object.keys(byInstrument).sort().forEach((instr) => {
      const group = document.createElement("optgroup");
      group.label = instr;
      byInstrument[instr].forEach((p) => {
        const opt = document.createElement("option");
        opt.value = p.point_id;
        opt.textContent = `${p.instrument} / ${p.element}`;
        if (selectedId === p.point_id) opt.selected = true;
        group.appendChild(opt);
      });
      sel.appendChild(group);
    });
    sel.addEventListener("change", () => onChange(parseInt(sel.value, 10)));
    return sel;
  }

  let lastPointsSignature = "";
  function renderTrackAssign(directState) {
    const points = directState.points || [];
    const sig = JSON.stringify(points.map((p) => p.point_id));
    if (sig === lastPointsSignature) return; // avoid rebuilding selects every tick
    lastPointsSignature = sig;

    const container = $("#trackAssign");
    container.innerHTML = "";
    const nTracks = (directState.sequencer && directState.sequencer.n_tracks) || 4;
    for (let t = 0; t < nTracks; t++) {
      const cur = directState.track_assignment ? directState.track_assignment[t] : undefined;
      const sel = buildPointSelect(points, cur, (pid) => post("direct/assign_track", { track: t, point_id: pid }));
      const label = document.createElement("label");
      label.textContent = "track " + t + " ";
      label.appendChild(sel);
      container.appendChild(label);
    }

    const fsrContainer = $("#fsrAssign");
    fsrContainer.innerHTML = "";
    for (let i = 0; i < 4; i++) {
      const cur = directState.fsr_assign ? directState.fsr_assign[i] : undefined;
      const sel = buildPointSelect(points, cur, (pid) => post("direct/assign_fsr", { index: i, point_id: pid }));
      const label = document.createElement("label");
      label.textContent = "FSR" + i + " ";
      label.appendChild(sel);
      fsrContainer.appendChild(label);
    }
  }

  // -- direct lab: per-row segment editor (layout v2) ---------------------
  // Each row 0-7 = a list of segments {instrument, page, width}; widths must
  // sum to 8. Instrument/width/+/- edits POST the whole row via
  // direct/set_row_segments (client validates the sum first; the server
  // rejects too). Page buttons are per segment (direct/segment_page).
  function swatch(colors, instr) {
    const dot = document.createElement("span");
    dot.className = "swatch";
    dot.style.background = (colors && colors[instr]) || "#333";
    dot.title = instr || "(none)";
    return dot;
  }

  let lastLegendSignature = "";
  function renderInstrumentLegend(directState) {
    const colors = directState.instrument_colors || {};
    const sig = JSON.stringify(colors);
    if (sig === lastLegendSignature) return;
    lastLegendSignature = sig;
    const el = $("#instrumentLegend");
    el.innerHTML = "";
    Object.keys(colors).forEach((instr) => {
      const item = document.createElement("span");
      item.className = "legend-item";
      item.append(swatch(colors, instr), document.createTextNode(instr));
      el.appendChild(item);
    });
  }

  // local draft edits (per row) not yet accepted by the server, so a row
  // whose widths don't currently sum to 8 stays editable instead of being
  // overwritten by the next SSE tick.
  const rowDrafts = {};
  let lastRowsSignature = "";

  function renderRowConfig(directState) {
    const rows = directState.rows || {};
    const instruments = directState.instruments || [];
    const colors = directState.instrument_colors || {};
    const split = !!directState.split;
    const layoutMode = directState.layout_mode || (split ? "split" : "full");
    // MODULETTES plays all 64 pads directly and never touches rows_config
    // (the FULL layout), so the row/segment editor below is read-only
    // while it's active -- editing it here would silently mutate the FULL
    // layout that's currently NOT what's on the grid.
    const isModulettes = layoutMode === "modulettes";
    const sig = JSON.stringify([rows, instruments, colors, split, layoutMode, rowDrafts]);
    if (sig === lastRowsSignature) return;
    lastRowsSignature = sig;

    const container = $("#rowConfig");
    container.innerHTML = "";
    container.classList.toggle("row-config-disabled", isModulettes);
    Object.keys(rows).map((r) => parseInt(r, 10)).sort((a, b) => a - b).forEach((row) => {
      const serverSegs = rows[row].segments || [];
      const draft = rowDrafts[row];
      const segs = draft ? draft : serverSegs.map((s) => ({ instrument: s.instrument, page: s.page, width: s.width }));
      const isSeq = (split && row >= 4) || isModulettes;

      const wrap = document.createElement("div");
      wrap.className = "row-config" + (isSeq ? " row-seq" : "") + (draft ? " row-draft" : "");

      const head = document.createElement("div");
      head.className = "row-head";
      const rowLabel = document.createElement("span");
      rowLabel.className = "row-label";
      rowLabel.textContent = "row " + row + (isSeq ? " · sequencer" : "");
      head.appendChild(rowLabel);

      const total = segs.reduce((n, s) => n + (parseInt(s.width, 10) || 0), 0);
      const valid = total === 8 && segs.length > 0;

      const submit = () => {
        if (!valid) return;
        delete rowDrafts[row];
        post("direct/set_row_segments", { row, segments: segs.map((s) => ({
          instrument: s.instrument || null, page: parseInt(s.page, 10) || 0, width: parseInt(s.width, 10),
        })) });
      };
      const setDraft = () => {
        rowDrafts[row] = segs;
        lastRowsSignature = "";
        renderRowConfig(directState);
      };

      const addBtn = document.createElement("button");
      addBtn.textContent = "+ segment";
      addBtn.disabled = isSeq || segs.length >= 8;
      addBtn.addEventListener("click", () => {
        // steal one column from the widest segment so the sum stays 8
        const widest = segs.reduce((best, s, i) => (s.width > segs[best].width ? i : best), 0);
        if (segs[widest].width < 2) return;
        segs[widest].width -= 1;
        segs.push({ instrument: segs[widest].instrument, page: 0, width: 1 });
        rowDrafts[row] = segs;
        const sum = segs.reduce((n, s) => n + s.width, 0);
        if (sum === 8) { delete rowDrafts[row]; post("direct/set_row_segments", { row, segments: segs }); }
        else setDraft();
      });
      const rmBtn = document.createElement("button");
      rmBtn.textContent = "− segment";
      rmBtn.disabled = isSeq || segs.length <= 1;
      rmBtn.addEventListener("click", () => {
        const last = segs.pop();
        segs[segs.length - 1].width += last.width;
        delete rowDrafts[row];
        post("direct/set_row_segments", { row, segments: segs });
      });
      head.append(addBtn, rmBtn);

      const msg = document.createElement("span");
      msg.className = "row-validation";
      if (!valid) msg.textContent = `widths sum to ${total}, need 8 (not sent)`;
      head.appendChild(msg);
      wrap.appendChild(head);

      const segsEl = document.createElement("div");
      segsEl.className = "segments";
      segs.forEach((seg, i) => {
        const segEl = document.createElement("div");
        segEl.className = "segment";
        segEl.style.flexGrow = Math.max(1, parseInt(seg.width, 10) || 1);

        const sel = document.createElement("select");
        sel.disabled = isSeq;
        const none = document.createElement("option");
        none.value = "";
        none.textContent = "(none)";
        if (!seg.instrument) none.selected = true;
        sel.appendChild(none);
        instruments.forEach((instr) => {
          const opt = document.createElement("option");
          opt.value = instr;
          opt.textContent = instr;
          if (seg.instrument === instr) opt.selected = true;
          sel.appendChild(opt);
        });
        sel.addEventListener("change", () => {
          seg.instrument = sel.value || null;
          seg.page = 0;
          if (valid) submit(); else setDraft();
        });

        const width = document.createElement("input");
        width.type = "number";
        width.min = 1;
        width.max = 8;
        width.value = seg.width;
        width.disabled = isSeq;
        width.title = "width (pads)";
        width.addEventListener("change", () => {
          seg.width = Math.max(1, Math.min(8, parseInt(width.value, 10) || 1));
          const sum = segs.reduce((n, s) => n + (parseInt(s.width, 10) || 0), 0);
          if (sum === 8) { delete rowDrafts[row]; post("direct/set_row_segments", { row, segments: segs }); }
          else setDraft();
        });

        const srv = serverSegs[i] || {};
        const nPages = srv.n_pages || 1;
        const prev = document.createElement("button");
        prev.textContent = "◀";
        prev.disabled = isSeq || !!draft;
        prev.addEventListener("click", () => post("direct/segment_page", { row, seg: i, delta: -1 }));
        const next = document.createElement("button");
        next.textContent = "▶";
        next.disabled = isSeq || !!draft;
        next.addEventListener("click", () => post("direct/segment_page", { row, seg: i, delta: 1 }));
        const pageLabel = document.createElement("span");
        pageLabel.className = "page-label";
        pageLabel.textContent = `page ${(srv.page || 0) + 1}/${nPages}`;

        const top = document.createElement("div");
        top.className = "segment-top";
        top.append(swatch(colors, seg.instrument), sel, width);
        const bottom = document.createElement("div");
        bottom.className = "segment-bottom";
        bottom.append(prev, pageLabel, next);
        segEl.append(top, bottom);
        segsEl.appendChild(segEl);
      });
      wrap.appendChild(segsEl);
      container.appendChild(wrap);
    });
  }

  // -- septagraph panel --------------------------------------------------
  document.querySelectorAll(".presets button[data-preset]").forEach((btn) => {
    btn.addEventListener("click", () => post("septagraph/preset", { preset: btn.dataset.preset }));
  });
  $("#manifestRequest").addEventListener("click", () => post("septagraph/manifest_request", {}));

  function renderSeptagraph(sState) {
    if (!sState) return;
    $("#septCatalogCount").textContent = sState.n_catalog_points;
    $("#septSnapRadius").textContent = sState.snap_radius;
    document.querySelectorAll(".presets button[data-preset]").forEach((btn) => {
      btn.classList.toggle("active", btn.dataset.preset === sState.preset);
    });
  }

  // -- midi panel --------------------------------------------------------
  $("#midiBaseNote").addEventListener("change", (e) =>
    post("midi/config", { base_note: parseInt(e.target.value, 10) })
  );
  $("#midiChannel").addEventListener("change", (e) =>
    post("midi/config", { channel: parseInt(e.target.value, 10) })
  );
  $("#midiFsrAftertouch").addEventListener("change", (e) =>
    post("midi/config", { fsr_aftertouch: e.target.checked })
  );

  $("#midiRefreshPorts").addEventListener("click", () =>
    fetch("/api/midi/list_ports", { method: "POST", headers: { "Content-Type": "application/json" }, body: "{}" })
      .then((r) => r.json())
      .then((data) => renderMidiPorts(data.ports || [], null))
      .catch(() => {})
  );

  let lastPortsSignature = "";
  function renderMidiPorts(ports, selected) {
    const sig = JSON.stringify([ports, selected]);
    if (sig === lastPortsSignature) return;
    lastPortsSignature = sig;
    const sel = $("#midiPortSelect");
    sel.innerHTML = "";
    ports.forEach((name) => {
      const opt = document.createElement("option");
      opt.value = name;
      opt.textContent = name;
      if (name === selected) opt.selected = true;
      sel.appendChild(opt);
    });
  }
  $("#midiPortSelect").addEventListener("change", (e) => post("midi/select_port", { port: e.target.value }));

  let lastFsrNotesSignature = "";
  function renderMidiFsrNotes(mState) {
    const notes = mState.fsr_notes || [36, 38, 42, 46];
    const sig = JSON.stringify(notes);
    if (sig === lastFsrNotesSignature) return;
    lastFsrNotesSignature = sig;
    const container = $("#midiFsrNotes");
    container.innerHTML = "";
    notes.forEach((note, i) => {
      const input = document.createElement("input");
      input.type = "number";
      input.min = 0;
      input.max = 127;
      input.value = note;
      input.addEventListener("change", () => {
        const next = notes.slice();
        next[i] = parseInt(input.value, 10);
        post("midi/config", { fsr_notes: next });
      });
      const label = document.createElement("label");
      label.textContent = "FSR" + i + " ";
      label.appendChild(input);
      container.appendChild(label);
    });
  }

  function renderMidi(mState) {
    if (!mState) return;
    $("#midiAvailable").textContent = mState.midi_available ? "connected" : "no port (silent)";
    renderMidiPorts(mState.ports || [], mState.port_name);
    renderMidiFsrNotes(mState);
    $("#midiFsrAftertouch").checked = !!mState.fsr_aftertouch;
  }

  // -- FSR meters + gamma/floor settings (task 4) --------------------------
  const fsrMetersEl = $("#fsrMeters");
  const fsrBars = [];
  for (let i = 0; i < 4; i++) {
    const wrap = document.createElement("div");
    wrap.className = "fsr-lane";
    const label = document.createElement("div");
    label.textContent = "FSR" + i;
    const bar = document.createElement("div");
    bar.className = "fsr-bar";
    const fill = document.createElement("div");
    fill.className = "fsr-fill";
    bar.appendChild(fill);
    wrap.append(label, bar);
    fsrMetersEl.appendChild(wrap);
    fsrBars.push(fill);
  }
  function renderFsrMeters(fsrState) {
    if (!fsrState) return;
    const pressure = fsrState.pressure || [0, 0, 0, 0];
    pressure.forEach((p, i) => {
      if (fsrBars[i]) fsrBars[i].style.width = Math.round(p * 100) + "%";
    });
    if (document.activeElement !== $("#fsrGamma")) $("#fsrGamma").value = fsrState.gamma;
    if (document.activeElement !== $("#fsrFloor")) $("#fsrFloor").value = fsrState.floor;
  }
  function postFsrConfig() {
    post("fsr/config", { gamma: parseFloat($("#fsrGamma").value), floor: parseFloat($("#fsrFloor").value) });
  }
  $("#fsrGamma").addEventListener("change", postFsrConfig);
  $("#fsrFloor").addEventListener("change", postFsrConfig);

  // -- sequencer (shared editor; targets whichever mode is active) ------
  const seqGridEl = $("#seqGrid");
  let currentMode = "direct_lab";
  let seqShape = { n_tracks: 0, n_steps: 0 };

  function ensureSeqGrid(nTracks, nSteps) {
    if (seqShape.n_tracks === nTracks && seqShape.n_steps === nSteps) return;
    seqShape = { n_tracks: nTracks, n_steps: nSteps };
    seqGridEl.innerHTML = "";
    for (let t = 0; t < nTracks; t++) {
      const row = document.createElement("div");
      row.className = "seq-row";
      for (let s = 0; s < nSteps; s++) {
        const cell = document.createElement("div");
        cell.className = "seq-step";
        cell.dataset.track = t;
        cell.dataset.step = s;
        cell.addEventListener("click", () =>
          post("sequencer/toggle", { mode: currentMode, track: t, step: s })
        );
        cell.addEventListener("contextmenu", (e) => {
          e.preventDefault();
          post("sequencer/cycle", { mode: currentMode, track: t, step: s });
        });
        row.appendChild(cell);
      }
      seqGridEl.appendChild(row);
    }
  }

  function renderSequencer(seq) {
    if (!seq) return;
    ensureSeqGrid(seq.n_tracks, seq.n_steps);
    const cells = seqGridEl.querySelectorAll(".seq-step");
    cells.forEach((cell) => {
      const t = parseInt(cell.dataset.track, 10);
      const s = parseInt(cell.dataset.step, 10);
      const level = (seq.pattern[t] || [])[s] || 0;
      cell.className = "seq-step" + (level ? " level-" + level : "");
      if (seq.playhead === s) cell.classList.add("playhead");
    });
    $("#tempo").value = seq.tempo_bpm;
    $("#tempoVal").textContent = Math.round(seq.tempo_bpm);
    $("#swing").value = seq.swing_pct;
    $("#swingVal").textContent = Math.round(seq.swing_pct);
    $("#stepsPerBeat").value = String(seq.steps_per_beat);
  }

  $("#playBtn").addEventListener("click", () => post("sequencer/play", { mode: currentMode }));
  $("#stopBtn").addEventListener("click", () => post("sequencer/stop", { mode: currentMode }));
  $("#tempo").addEventListener("input", (e) => post("sequencer/tempo", { mode: currentMode, bpm: parseFloat(e.target.value) }));
  $("#swing").addEventListener("input", (e) => post("sequencer/swing", { mode: currentMode, pct: parseFloat(e.target.value) }));
  $("#stepsPerBeat").addEventListener("change", (e) => post("sequencer/steps_per_beat", { mode: currentMode, n: parseInt(e.target.value, 10) }));

  const slotsEl = $("#patternSlots");
  for (let i = 0; i < 8; i++) {
    const btn = document.createElement("button");
    btn.textContent = String(i + 1);
    btn.addEventListener("click", () => post("sequencer/load_slot", { mode: currentMode, slot: i }));
    btn.addEventListener("dblclick", () => post("sequencer/save_slot", { mode: currentMode, slot: i }));
    slotsEl.appendChild(btn);
  }

  $("#panicBtn").addEventListener("click", () => post("panic", {}));

  // -- brightness -----------------------------------------------------------
  const brightnessSlider = $("#brightnessSlider");
  const brightnessVal = $("#brightnessVal");
  let brightnessFromServer = null;
  if (brightnessSlider) {
    const sendBrightness = () => {
      const level = Number(brightnessSlider.value);
      brightnessVal.textContent = String(level);
      brightnessFromServer = level;
      post("brightness", { level });
    };
    brightnessSlider.addEventListener("input", () => {
      brightnessVal.textContent = brightnessSlider.value;
    });
    brightnessSlider.addEventListener("change", sendBrightness);
  }

  // -- SENSORS panel: raw hardware readout (softpots, FSRs, lidars, link) --
  // Copies pi/tools/monitor.py's sensor-lane look (raw bar + value + state
  // badge) so hardware can be sanity-checked from this app's own UI without
  // switching to the monitor service. Open by default on desktop, collapsed
  // on phone (one-time check on load; doesn't fight a manual toggle after).
  const sensorsPanelEl = $("#sensorsPanel");
  if (sensorsPanelEl) {
    sensorsPanelEl.open = window.matchMedia("(min-width: 640px)").matches;
  }

  function buildSensorLane(container, name, opts) {
    opts = opts || {};
    const wrap = document.createElement("div");
    wrap.className = "sensor-lane";
    const label = document.createElement("div");
    label.className = "sensor-lane-name";
    label.textContent = name;
    const bar = document.createElement("div");
    bar.className = "sensor-bar";
    const fill = document.createElement("div");
    fill.className = "sensor-fill";
    bar.appendChild(fill);
    const val = document.createElement("div");
    val.className = "sensor-val";
    wrap.append(label, bar, val);
    const lane = { fill, val };
    if (opts.badge) {
      const badge = document.createElement("div");
      badge.className = "sensor-badge";
      wrap.appendChild(badge);
      lane.badge = badge;
    }
    if (opts.age) {
      const age = document.createElement("div");
      age.className = "sensor-age";
      wrap.appendChild(age);
      lane.age = age;
    }
    container.appendChild(wrap);
    return lane;
  }

  const softpotLanes = ["N", "S", "E", "W"].map((n) =>
    buildSensorLane($("#sensorSoftpots"), n)
  );
  const fsrLanes = ["FSR0", "FSR1", "FSR2", "FSR3"].map((n) =>
    buildSensorLane($("#sensorFsrs"), n, { badge: true })
  );
  const lidarLanes = ["N", "S", "E", "W"].map((n) =>
    buildSensorLane($("#sensorLidars"), n, { badge: true, age: true })
  );
  const sensorLinkEl = $("#sensorLink");

  function renderSensors(sensors) {
    if (!sensors) return;
    (sensors.softpots || []).forEach((s, i) => {
      const lane = softpotLanes[i];
      if (!lane) return;
      lane.fill.style.width = Math.round((s.raw / 4095) * 100) + "%";
      lane.val.textContent = s.raw;
    });
    (sensors.fsrs || []).forEach((f, i) => {
      const lane = fsrLanes[i];
      if (!lane) return;
      lane.fill.style.width = Math.round((f.value || 0) * 100) + "%";
      lane.val.textContent = f.raw;
      lane.badge.textContent = f.pressed ? "PRESSED" : "idle";
      lane.badge.className = "sensor-badge" + (f.pressed ? " on" : "");
    });
    (sensors.lidars || []).forEach((l, i) => {
      const lane = lidarLanes[i];
      if (!lane) return;
      const pct = l.mm != null ? Math.min(100, Math.round((l.mm / 2000) * 100)) : 0;
      lane.fill.style.width = pct + "%";
      lane.fill.classList.toggle("invalid", !l.ok);
      lane.val.textContent = l.mm != null ? l.mm : "—";
      lane.badge.textContent = l.ok ? "ok" : "INVALID (no return / out of range)";
      lane.badge.className = "sensor-badge" + (l.ok ? " on" : " off");
      lane.age.textContent = l.age_s != null ? l.age_s.toFixed(1) + "s ago" : "never";
    });
    if (sensorLinkEl) {
      const hello = sensors.hello;
      const helloTxt = hello
        ? `HELLO fw${hello.fw_ver} analog×${hello.n_analog} (${sensors.hello_age_s ?? "?"}s ago)`
        : "no HELLO yet";
      sensorLinkEl.textContent =
        `link ${sensors.link_connected ? "connected" : "down"} · ${helloTxt} · ` +
        `${sensors.key_events ?? 0} key events`;
    }
  }

  // -- SSE ----------------------------------------------------------------
  const statusEl = $("#status");
  const labPillEl = $("#labPill");

  function renderLabPill(data) {
    if (!labPillEl) return;
    if (!data.lab_connected) {
      labPillEl.textContent = "LAB";
      labPillEl.className = "pill";
      labPillEl.title = "no --lab target configured";
      return;
    }
    labPillEl.classList.remove("reachable", "unreachable");
    if (data.lab_reachable) {
      labPillEl.classList.add("reachable");
      const uptime = data.lab_uptime != null ? `${data.lab_uptime}s uptime` : "uptime unknown";
      labPillEl.textContent = "LAB";
      labPillEl.title = `gor_server reachable (${uptime}, last /pong ${data.last_pong_age_s ?? "?"}s ago)`;
    } else {
      labPillEl.classList.add("unreachable");
      labPillEl.textContent = "LAB";
      labPillEl.title = "no /pong from gor_server yet";
    }
  }

  // SSE already runs at webui.UI_FPS (15fps); the SENSORS panel only needs
  // <= 10Hz, so drop ticks that land inside the last 100ms.
  let lastSensorsRenderMs = 0;
  function maybeRenderSensors(sensors) {
    const now = Date.now();
    if (now - lastSensorsRenderMs < 100) return;
    lastSensorsRenderMs = now;
    renderSensors(sensors);
  }

  function connect() {
    const es = new EventSource("/events");
    es.onopen = () => {
      statusEl.textContent = "live";
      statusEl.classList.add("live");
    };
    es.onerror = () => {
      statusEl.textContent = "reconnecting…";
      statusEl.classList.remove("live");
    };
    es.onmessage = (ev) => {
      let data;
      try {
        data = JSON.parse(ev.data);
      } catch (e) {
        return;
      }
      currentMode = data.mode;
      setActiveMode(data.mode);
      renderGrid(data.grid);
      renderOrientation(data.orientation);
      renderLabPill(data);
      if (brightnessSlider && data.brightness != null && !brightnessSlider.matches(":active") && document.activeElement !== brightnessSlider) {
        brightnessSlider.value = data.brightness;
        brightnessVal.textContent = String(data.brightness);
      }
      const s = data.state || {};
      if (s.direct_lab) {
        renderLayoutToggle(s.direct_lab.layout_mode || (s.direct_lab.split ? "split" : "full"));
        renderInstrumentLegend(s.direct_lab);
        renderRowConfig(s.direct_lab);
        renderTrackAssign(s.direct_lab);
        $("#directSkips").textContent = s.direct_lab.skip_count;
        $("#directLastSent").textContent = s.direct_lab.last_sent || "—";
      }
      renderSeptagraph(s.septagraph);
      renderMidi(s.midi);
      renderFsrMeters(data.fsr);
      maybeRenderSensors(data.sensors);
      const active = s[data.mode];
      if (active && active.sequencer) renderSequencer(active.sequencer);
    };
  }

  connect();
})();

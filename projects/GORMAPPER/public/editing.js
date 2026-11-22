// GORMAPPER editing — pointer interaction + overlay drawing for the editor.
// Works on an overlay <canvas> stacked above the WebGL preview.
//
// Interactions:
//   click zone body            select (drag to move whole zone)
//   drag vertex handle         move vertex
//   click near an edge         insert vertex there
//   alt-click a vertex         delete vertex (min 3 kept)
//   drawing mode (N / M)       click to place points, Enter or double-click
//                              to close, Esc to cancel

import { pointInPolygon, distToSegment, bbox, makeZone, makeMask } from './scene.js';

const HANDLE_R = 13;     // screen px
const EDGE_TOL = 10;     // screen px

export function createEditing(opts) {
  const { canvas, getScene, getScale, onChange, onSelect } = opts;

  let selectedId = null;
  let drawing = null;          // { type: 'zone'|'mask', points: [[x,y],...] }
  let drag = null;             // { kind:'vertex'|'body', layer, index?, start, orig }
  let hoverPt = null;          // scene-space cursor for draw preview

  // ---- helpers --------------------------------------------------------------

  function layerPoints(layer) {
    return layer.points;
  }

  function layerColor(layer) {
    const kind = (layer.content || {}).kind;
    return kind === 'mask' ? '#f5427e' : kind === 'screen' ? '#42d7f5' : '#7ef542';
  }

  function toScene(ev) {
    const r = canvas.getBoundingClientRect();
    const s = getScale();
    return [(ev.clientX - r.left) / s, (ev.clientY - r.top) / s];
  }

  function editableLayers() {
    // topmost first for hit testing
    return [...getScene().layers].reverse();
  }

  function findLayer(id) {
    return getScene().layers.find((l) => l.id === id) || null;
  }

  // hit priority: selected layer's vertices → any layer's vertices →
  // selected layer's edges → bodies (topmost first)
  function hitTest(px, py) {
    const s = getScale();
    const hr = HANDLE_R / s, et = EDGE_TOL / s;
    const layers = editableLayers();
    const ordered = [
      ...layers.filter((l) => l.id === selectedId),
      ...layers.filter((l) => l.id !== selectedId),
    ];
    for (const layer of ordered) {
      const pts = layerPoints(layer);
      if (!pts) continue;
      for (let i = 0; i < pts.length; i++) {
        if (Math.hypot(px - pts[i][0], py - pts[i][1]) <= hr * 1.8) {
          return { kind: 'vertex', layer, index: i };
        }
      }
    }
    for (const layer of ordered) {
      const pts = layerPoints(layer);
      if (!pts || pts.length < 2) continue;
      for (let i = 0; i < pts.length; i++) {
        const j = (i + 1) % pts.length;
        const d = distToSegment(px, py, pts[i][0], pts[i][1], pts[j][0], pts[j][1]);
        if (d.dist <= et) return { kind: 'edge', layer, index: i, at: [d.x, d.y] };
      }
    }
    for (const layer of ordered) {
      const pts = layerPoints(layer);
      if (pts && pts.length >= 3 && pointInPolygon(px, py, pts)) {
        return { kind: 'body', layer };
      }
    }
    return null;
  }

  // ---- pointer events -------------------------------------------------------

  canvas.addEventListener('pointerdown', (ev) => {
    if (ev.button !== 0) return;
    const [px, py] = toScene(ev);

    if (drawing) {
      drawing.points.push([px, py]);
      onChange(false);
      return;
    }

    const hit = hitTest(px, py);
    if (!hit) {
      setSelected(null);
      onChange(false);
      return;
    }
    setSelected(hit.layer.id);

    if (hit.kind === 'vertex') {
      if (ev.altKey) {
        const pts = layerPoints(hit.layer);
        if (pts.length > 3) {
          pts.splice(hit.index, 1);
          onChange(true);
        }
        return;
      }
      drag = { kind: 'vertex', layer: hit.layer, index: hit.index };
    } else if (hit.kind === 'edge') {
      const pts = layerPoints(hit.layer);
      pts.splice(hit.index + 1, 0, [hit.at[0], hit.at[1]]);
      drag = { kind: 'vertex', layer: hit.layer, index: hit.index + 1 };
      onChange(false);
    } else {
      drag = {
        kind: 'body',
        layer: hit.layer,
        start: [px, py],
        orig: layerPoints(hit.layer).map((p) => [p[0], p[1]]),
      };
    }
    canvas.setPointerCapture(ev.pointerId);
  });

  canvas.addEventListener('pointermove', (ev) => {
    const [px, py] = toScene(ev);
    hoverPt = [px, py];
    if (drawing) { onChange(false); return; }
    if (!drag) {
      const hit = hitTest(px, py);
      canvas.style.cursor = !hit ? 'crosshair'
        : hit.kind === 'vertex' ? 'grab'
        : hit.kind === 'edge' ? 'copy' : 'move';
      return;
    }
    if (drag.kind === 'vertex') {
      const pts = layerPoints(drag.layer);
      pts[drag.index] = [px, py];
    } else {
      const dx = px - drag.start[0], dy = py - drag.start[1];
      const pts = layerPoints(drag.layer);
      for (let i = 0; i < pts.length; i++) {
        pts[i] = [drag.orig[i][0] + dx, drag.orig[i][1] + dy];
      }
    }
    onChange(false);
  });

  canvas.addEventListener('pointerup', (ev) => {
    if (drag) {
      drag = null;
      onChange(true);
    }
    if (canvas.hasPointerCapture(ev.pointerId)) canvas.releasePointerCapture(ev.pointerId);
  });

  canvas.addEventListener('dblclick', (ev) => {
    ev.preventDefault();
    if (drawing) {
      // the second click of the dblclick added a duplicate point — drop it,
      // keeping the double-clicked position as the final vertex
      drawing.points.pop();
      const pts = drawing.points;
      const n = pts.length;
      if (n >= 2 && Math.hypot(pts[n - 1][0] - pts[n - 2][0], pts[n - 1][1] - pts[n - 2][1]) < 3) {
        pts.pop();
      }
      finishDraw();
      return;
    }
    // double-click a vertex to delete it (same as alt-click)
    const [px, py] = toScene(ev);
    const hit = hitTest(px, py);
    if (hit && hit.kind === 'vertex') {
      const pts = layerPoints(hit.layer);
      if (pts.length > 3) {
        pts.splice(hit.index, 1);
        onChange(true);
      }
    }
  });

  // ---- drawing mode ---------------------------------------------------------

  function startDraw(type) {
    drawing = { type, points: [] };
    setSelected(null);
    onChange(false);
  }

  function finishDraw() {
    if (!drawing) return;
    if (drawing.points.length >= 3) {
      const scene = getScene();
      const layer = drawing.type === 'mask' ? makeMask(drawing.points) : makeZone(drawing.points);
      const kind = drawing.type === 'mask' ? 'mask' : 'pattern';
      layer.name = (drawing.type === 'mask' ? 'Mask ' : 'Zone ')
        + (scene.layers.filter((l) => (l.content || {}).kind === kind).length + 1);
      scene.layers.push(layer);
      setSelected(layer.id);
    }
    drawing = null;
    onChange(true);
  }

  function cancelDraw() {
    drawing = null;
    onChange(false);
  }

  function isDrawing() { return !!drawing; }

  // ---- selection / mutation helpers ----------------------------------------

  function setSelected(id) {
    if (selectedId === id) return;
    selectedId = id;
    onSelect(id);
  }

  function deleteSelected() {
    const scene = getScene();
    const i = scene.layers.findIndex((l) => l.id === selectedId);
    if (i < 0) return;
    scene.layers.splice(i, 1);
    setSelected(null);
    onChange(true);
  }

  // ---- overlay drawing ------------------------------------------------------

  function draw(ctx) {
    const s = getScale();
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.save();
    ctx.scale(s, s);
    ctx.lineWidth = 1.5 / s;

    for (const layer of getScene().layers) {
      const pts = layerPoints(layer);
      if (!pts || pts.length < 2) continue;
      const sel = layer.id === selectedId;
      const color = layerColor(layer);
      ctx.strokeStyle = sel ? '#ffffff' : color;
      ctx.globalAlpha = sel ? 1 : 0.75;
      ctx.beginPath();
      ctx.moveTo(pts[0][0], pts[0][1]);
      for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
      ctx.closePath();
      ctx.stroke();

      for (let i = 0; i < pts.length; i++) {
        const [x, y] = pts[i];
        ctx.fillStyle = sel ? '#ffffff' : color;
        ctx.beginPath();
        ctx.arc(x, y, (sel ? HANDLE_R : HANDLE_R * 0.7) / s, 0, Math.PI * 2);
        ctx.fill();
        if (sel) {
          ctx.fillStyle = '#000';
          ctx.font = `${10 / s}px ui-monospace, monospace`;
          ctx.textAlign = 'center';
          ctx.textBaseline = 'middle';
          ctx.fillText(String(i + 1), x, y);
        }
      }
      if (sel) {
        const bb = bbox(pts);
        ctx.fillStyle = '#ffffff';
        ctx.font = `${13 / s}px ui-monospace, monospace`;
        ctx.textAlign = 'left';
        ctx.textBaseline = 'bottom';
        ctx.fillText(layer.name || 'object', bb.x, bb.y - 6 / s);
      }
    }

    // in-progress polygon
    if (drawing && drawing.points.length) {
      const pts = drawing.points;
      ctx.globalAlpha = 1;
      ctx.strokeStyle = drawing.type === 'mask' ? '#f5427e' : '#f5e642';
      ctx.setLineDash([6 / s, 4 / s]);
      ctx.beginPath();
      ctx.moveTo(pts[0][0], pts[0][1]);
      for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
      if (hoverPt) ctx.lineTo(hoverPt[0], hoverPt[1]);
      ctx.stroke();
      ctx.setLineDash([]);
      for (const [x, y] of pts) {
        ctx.fillStyle = '#f5e642';
        ctx.beginPath();
        ctx.arc(x, y, 4 / s, 0, Math.PI * 2);
        ctx.fill();
      }
    }
    ctx.restore();
  }

  return {
    draw,
    startDraw,
    finishDraw,
    cancelDraw,
    isDrawing,
    deleteSelected,
    get selectedId() { return selectedId; },
    select: setSelected,
  };
}

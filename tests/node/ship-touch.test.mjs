import test from 'node:test';
import assert from 'node:assert/strict';
import { stickVec, stickKeys, padLayout, hitPad, pinchThrottle, STICK_DEAD } from '../../js/ship-touch.js';

test('stickVec: dead zone, clamp, y up', () => {
  assert.deepEqual(stickVec(0, 0, 80), { x: 0, y: 0 });
  assert.deepEqual(stickVec(5, 5, 80), { x: 0, y: 0 });
  const v = stickVec(0, -400, 80);                      // far up
  assert.ok(Math.abs(v.y - 1) < 1e-9 && v.x === 0);
  const w = stickVec(400, 0, 80);
  assert.ok(Math.abs(w.x - 1) < 1e-9);
  const m = stickVec(80 * (STICK_DEAD + 0.4), 0, 80);
  assert.ok(m.x > 0 && m.x < 1);
  assert.deepEqual(stickVec(10, 10, 0), { x: 0, y: 0 });
  for (const [dx, dy] of [[1e9, 1e9], [-3, 7], [NaN, 1]]) { const r = stickVec(dx, dy, 80); if (!Number.isNaN(dx)) assert.ok(Number.isFinite(r.x) && Number.isFinite(r.y) && Math.hypot(r.x, r.y) <= 1.0000001); }
});
test('stickKeys: thresholds and opposite keys never both', () => {
  assert.deepEqual(stickKeys({ x: 0, y: 0.2 }), { KeyW: false, KeyS: false, KeyA: false, KeyD: false });
  const k = stickKeys({ x: -0.9, y: 0.9 });
  assert.ok(k.KeyW && k.KeyA && !k.KeyS && !k.KeyD);
  for (let a = 0; a < 360; a += 15) { const r = stickKeys({ x: Math.cos(a), y: Math.sin(a) }); assert.ok(!(r.KeyW && r.KeyS) && !(r.KeyA && r.KeyD)); }
});
test('padLayout: all required pads, on screen, no overlap, across phone sizes', () => {
  for (const [W, H] of [[844, 390], [667, 375], [915, 412], [1180, 820]]) {
    const p = padLayout(W, H), codes = p.map(x => x.code);
    for (const c of ['KeyZ', 'KeyF', 'KeyT', 'KeyE', 'Space', 'ShiftLeft', 'FIRE', 'Escape']) assert.ok(codes.includes(c), c);
    for (const a of p) {
      assert.ok(a.x - a.r >= 0 && a.x + a.r <= W && a.y - a.r >= 0 && a.y + a.r <= H, `${a.code} on screen ${W}x${H}`);
      for (const b of p) if (a !== b) assert.ok(Math.hypot(a.x - b.x, a.y - b.y) >= a.r + b.r - 1e-6, `${a.code}/${b.code} overlap ${W}x${H}`);
    }
    assert.equal(p.find(x => x.code === 'Escape').hold, true);
    assert.equal(p.find(x => x.code === 'FIRE').mouse, 0);
  }
});
test('hitPad: hits centre, misses empty, deterministic', () => {
  const p = padLayout(844, 390), z = p.find(x => x.code === 'KeyZ');
  assert.equal(hitPad(p, z.x, z.y).code, 'KeyZ');
  assert.equal(hitPad(p, 5, 200), null);
});
test('pinchThrottle: sign, zero on degenerate', () => {
  assert.ok(pinchThrottle(100, 150) > 0);
  assert.ok(pinchThrottle(100, 60) < 0);
  assert.equal(pinchThrottle(0, 50), 0);
  assert.equal(pinchThrottle(100, 100), 0);
});

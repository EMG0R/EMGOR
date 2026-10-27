import test from 'node:test';
import assert from 'node:assert/strict';
import { cleanPose, cleanFire, cleanName, cleanColor } from '../../js/ship-net-validate.js';
const good = { x: 1, y: 2, z: 3, qx: 0, qy: 0, qz: 0, qw: 1, v: 5, st: 3, hp: 80 };
test('pose: good passes, bad rejected', () => {
    assert.deepEqual(cleanPose(good), good);
    for (const k of ['x', 'qw', 'z']) for (const bad of [NaN, Infinity, '5', null, 1e12]) assert.equal(cleanPose({ ...good, [k]: bad }), null, k + String(bad));
    assert.equal(cleanPose({ ...good, qw: 0 }), null);
    assert.equal(cleanPose(null), null);
});
test('pose: quaternion renormalised, junk side fields defaulted', () => {
    const p = cleanPose({ ...good, qw: 2, v: 'x', hp: NaN, st: 9999 });
    assert.ok(Math.abs(p.qw - 1) < 1e-9); assert.equal(p.v, 0); assert.equal(p.hp, 100); assert.equal(p.st, 0);
});
test('fire / name / color', () => {
    assert.ok(cleanFire({ x: 1, y: 1, z: 1, dx: 0, dy: 0, dz: 1, w: 2 }));
    assert.equal(cleanFire({ x: 1, y: 1, z: '1', dx: 0, dy: 0, dz: 1 }), null);
    assert.equal(cleanFire({ x: 1, y: 1, z: 1, dx: 1e9, dy: 0, dz: 1 }), null);
    assert.equal(cleanName({ a: 1 }), 'PILOT'); assert.equal(cleanName('a'.repeat(99)).length, 24); assert.equal(cleanName('  '), 'PILOT');
    assert.equal(cleanColor('red'), null); assert.equal(cleanColor(-1), null); assert.equal(cleanColor(0xff00ff), 0xff00ff);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import * as S from '../../js/ship-styles.js';

test('STEEV and SONIK capsules come from the rig recipe', () => {
  for (const id of ['STEEV', 'SONIK']) {
    const st = S.createStyle(id, {}), rig = S.skinFor(id).hitbox;
    assert.equal(st.hitbox.r, rig.r, id + ' radius');
    if (id === 'STEEV') assert.equal(st.hitbox.h, rig.h);
    else assert.equal(st.hitbox.hStand, rig.h);
  }
});
test('MARO live path uses the table constants', () => {
  assert.equal(S.MARO_K.PUNCH_DMG.join(), '6,6,10');
  const st = S.createStyle('MARO', {});
  assert.ok(st && typeof st.step === 'function');
});

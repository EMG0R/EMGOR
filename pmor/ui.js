// PMOR web port, UI layer. Navigation state machine, OLED / pad / encoder rendering,
// touch input, patches. No audio imports: the engine is passed in (see PROTOCOL.md).
// Firmware references are L<n> into POCKETGORATOR_CODE.cpp.

const OW = 128, OH = 64;

// libDaisy Font_6x8, glyphs 32..126, 8 rows each, 6 bits per row (MSB = left pixel).
const FONT_HEX = '00000000000000000808080808000800141414000000000014143e143e141400081e281c0a3c08003032040810260600102828102a241a000c0c08100000000004081010100804001008040404081000082a1c3e1c2a08000008083e08080000000000000c0c08000000003e0000000000000000000c0c0000020408102000001c22262a32221c000818080808081c001c22021c20203e003e02040c02221c00040c14243e0404003e203c0202221c000e10203c22221c003e020204081020001c22221c22221c001c22221e0204380000000800080000000000080008081000020408100804020000003e003e00000010080402040810001c22020c080008001c222a2e2c201e00081422223e2222003c22223c22223c001c22202020221c003c22222222223c003e20203c20203e003e20203c202020001e22202026221e002222223e222222001c08080808081c000e0404040424180022242830282422002020202020203e0022362a2a2a2222002222322a262222001c22222222221c003c22223c202020001c2222222a241a003c22223c282422001c22201c02221c003e2a0808080808002222222222221c0022222222221408002222222a2a2a1400222214081422220022221408080808003e02041c10203e001e10101010101e0000201008040200001e02020202021e0008142200000000000000000000003e001818080400000000000018041c241e0020202c3222322c0000001c2220221c0002021a2622261a0000001c223e201c00040a081c0808080000001c26261a020020202c32222222000800180808081c00040004040424180020202428302824001808080808081c000000342a2a2a2a0000002c322222220000001c2222221c0000002c32322c200000001a26261a020000002c322020200000001e201c023c0008083e08080a04000000222222261a000000222222140800000022222a2a14000000221408142200000022221e02220000003e0408103e00040808100808040008080800080808001008080408081000102a040000000000';
const FONT = new Uint8Array(95 * 8);
for (let i = 0; i < FONT.length; i++) FONT[i] = parseInt(FONT_HEX.substr(i * 2, 2), 16);

export const MENU = { SYNTH: 0, DRUM: 1, SMP: 2, FX: 3, SESS: 4, DASH: 5, GLOB: 6 };
export const MENU_COUNT = 7;
const MN = ['SYNTH', 'DRUMS', 'SAMPLER', 'FX', 'PATCHES', 'DASH', 'GLOBALS'];
const WAVE_NAMES = ['SAW', 'SQUARE', 'TRI', 'SINE', 'PULSE', 'THIEF', 'FM', 'WT', 'FOLD', 'PLUCK', 'SUPER', 'CZ', 'ARCADE'];
const MODE_NAMES = ['IONIAN', 'DORIAN', 'PHRYGIAN', 'LYDIAN', 'MIXOLYD', 'AEOLIAN', 'LOCRIAN', 'HARM-MIN', 'PENTA'];
const PADN = ['HCL', 'HMD', 'HOP', 'RIDE', 'SNP1', 'SNP2', 'SNRF', 'CHOP', 'TOMM', 'TOMH', 'BELL', 'BLIP', 'KCK1', 'KCK2', 'KCK3', 'K808'];
const PADN2 = ['EHAT1', 'EHAT2', 'EHAT3', 'ECYM', 'CLAP', 'ESNR', 'RIM', 'ZAP', 'LZTOM', 'FMBEL', 'MTL', 'COSMO', 'K909', 'KCLUB', 'KSUB', 'KHARD'];
const FXNB = [
  ['STUT 4', 'STUT 8', 'STUT 16', 'STUT 32', 'REVERSE', 'CHORUS', 'PAN', 'CRUSH', 'HALF', 'DOUBLE', 'BUZZ', 'MUTE', 'FILTER', 'WASH', 'DIST', 'STOP'],
  ['DUB', 'GVERB', 'TGATE', 'FLANG', 'RING', 'FOLD', 'COMB', 'VOWEL', 'ISO-LO', 'ISO-MD', 'ISO-HI', 'PHONE', 'GRAIN', 'WOW', 'DOWN', 'SCAT'],
  ['PLATE', 'SHIMR', 'PONG', 'SLAP', 'PHASE', 'WAH', 'TREM', 'HAAS', 'VIBRA', 'DIMEN', 'SPRNG', 'SIREN', 'WARM', 'SUB', 'AIR', 'PUMP'],
  ['OCT+', 'OCT-', 'DSTRY', 'REPT', 'RGRN', 'FREEZ', 'METAL', 'FRMNT', 'FSHFT', 'FUZZ', 'GSEQ', 'SRAMP', 'VSTOP', 'RADIO', 'NGATE', 'SHMOD'],
  ['RPT2', 'RPT6', 'SHUF', 'Q6/8', 'PUNCH', 'FILL', 'BUILD', 'BALL', 'LPF', 'HPF', 'SQUSH', 'SCRTC', 'SCRTF', 'GLCH', 'BLIND', 'S&H'],
  ['DELAY', 'PNGPG', 'FADE', 'FBACK', 'DRONE', 'FREEZ', 'UNI', 'UNILO', 'DETUN', 'PLFO', 'BND+', 'BND-', 'WARBL', '5TH', 'SUB', 'LOOP'],
  ['GATE4', 'GATE8', 'GAT16', 'TRNCE', 'TREM', 'PAN', 'PUMP', 'SWELL', 'NOISE', 'WIDE', 'MONO', 'BRITE', 'DARK', 'DRIVE', 'SGATE', 'KILL'],
  ['RAT2', 'RAT3', 'RAT4', 'RAT8', 'STRUM', 'TRNC2', 'STAB', 'RINGL', 'RINGH', 'VIBR', 'CHOP', 'HALF', 'DBL', 'GLCH', 'REVG', 'SIL'],
];
// L4238-4246
const FXC = [
  [[255, 215, 65], [70, 150, 255], [155, 65, 245], [185, 65, 220]],
  [[40, 230, 180], [80, 255, 80], [255, 100, 200], [255, 150, 40]],
  [[120, 200, 255], [60, 255, 160], [255, 80, 120], [200, 120, 255]],
  [[255, 90, 40], [255, 40, 120], [120, 255, 40], [255, 230, 40]],
  [[255, 170, 0], [255, 90, 160], [0, 200, 255], [160, 255, 90]],
  [[90, 160, 255], [160, 90, 255], [0, 255, 200], [255, 120, 90]],
  [[255, 120, 200], [120, 255, 120], [255, 200, 80], [80, 200, 255]],
  [[200, 80, 255], [255, 80, 80], [80, 255, 200], [255, 255, 80]],
];
const VCOL = [[255, 60, 40], [200, 60, 255], [255, 200, 40], [60, 220, 255]];
const LAVA = [[70, 150, 255], [75, 175, 240], [240, 235, 75], [255, 215, 65], [245, 240, 80], [80, 150, 255], [155, 65, 245], [185, 65, 220]];
const DRIFT = [1, 2, 1, 2, 2, 1, 2, 1, 1, 2, 2, 1, 2, 1, 1, 2];
// engine parameter name per cursor position on each engine page (SPEC 2.3; PRISE is on the SYNTH page)
const ENG_PARAMS = [
  ['wave', 'dens', 'poly', 'vsyn', 'wtscan', 'prise', 'bars', 'beats'],
  ['kit', 'cdrum', 'vdrum', 'bars', 'beats'],
  ['csmp', 'vsmp', 'bars', 'beats'],
];
const SYN_SEQ = 0;
const DB_NAME = 'pmor', DB_STORE = 'patches', LS_PREFIX = 'pmor.patches.v1.';
const LED_MS = 80, OLED_MS = 66;

const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const pop16 = (m) => { let n = 0; m &= 0xffff; while (m) { n += m & 1; m >>>= 1; } return n; };
const pad = (s, n) => { s = String(s); return s.length >= n ? s : s + ' '.repeat(n - s.length); };
const lp = (n, w) => String(n).padStart(w, ' ');
const z2 = (n) => String(n).padStart(2, '0');
const sgn = (n) => (n < 0 ? '-' : '+') + Math.abs(n);

// ---------------------------------------------------------------------------------------------
// 1-bit framebuffer primitives with libDaisy semantics (hid/disp/color_display.h)

export function makeFb() { return new Uint8Array(OW * OH); }

function px(fb, x, y, on) {
  if (x < 0 || y < 0 || x >= OW || y >= OH) return;
  fb[y * OW + x] = on ? 1 : 0;
}

export function line(fb, x1, y1, x2, y2, on) {
  const dx = Math.abs(x2 - x1), dy = Math.abs(y2 - y1);
  const sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
  let err = dx - dy;
  px(fb, x2, y2, on);
  while (x1 !== x2 || y1 !== y2) {
    px(fb, x1, y1, on);
    const e2 = err * 2;
    if (e2 > -dy) { err -= dy; x1 += sx; }
    if (e2 < dx) { err += dx; y1 += sy; }
  }
}

export function rect(fb, x1, y1, x2, y2, on, fill) {
  if (fill) {
    for (let x = x1; x <= x2; x++) for (let y = y1; y <= y2; y++) px(fb, x, y, on);
  } else {
    line(fb, x1, y1, x2, y1, on); line(fb, x2, y1, x2, y2, on);
    line(fb, x2, y2, x1, y2, on); line(fb, x1, y2, x1, y1, on);
  }
}

// WriteString: every cell is opaque (background pixels are written as !on); a char outside
// 32..126 or one that does not fit aborts the rest of the string (L410-463 of libDaisy).
export function text(fb, x, y, str, on) {
  for (let i = 0; i < str.length; i++) {
    const c = str.charCodeAt(i);
    if (c < 32 || c > 126) return x;
    if (OW < x + 6 || OH < y + 8) return x;
    const g = (c - 32) * 8;
    for (let r = 0; r < 8; r++) {
      const b = FONT[g + r];
      for (let j = 0; j < 6; j++) px(fb, x + j, y + r, ((b >> (5 - j)) & 1) ? on : !on);
    }
    x += 6;
  }
  return x;
}
const row = (fb, y, s) => text(fb, 0, y, s, true);

// ---------------------------------------------------------------------------------------------
// Lava palette and LED frame (SPEC 4, A.1, A.10). Returned colours are pre-dim (the *179>>8
// hardware dim is applied when painting).

export function lavaColor(phase) {
  const i = (phase >> 5) & 7, f = phase & 31, j = (i + 1) & 7;
  const a = LAVA[i], b = LAVA[j];
  return [a[0] + (((b[0] - a[0]) * f) >> 5), a[1] + (((b[1] - a[1]) * f) >> 5), a[2] + (((b[2] - a[2]) * f) >> 5)];
}
const dimLava = (ph) => { const c = lavaColor(ph); return [c[0] >> 5, c[1] >> 5, c[2] >> 5]; };
export const phaseAt = (i, step) => (16 * i + DRIFT[i] * step) & 255;

export function blankTick() {
  return {
    gstep: 0, songLen: 16, step_ms: 125, bpm: 120, out_amp: 0,
    playing: { syn: 0, held: 0 }, syn_seq_page: new Uint16Array(16), syn_arm: 0,
    dr_page: [0, 0, 0, 0], sm_page: new Array(16).fill(0), dr_counts: [0, 0, 0, 0], sm_counts: new Array(16).fill(0),
    dr_pad: [13, 10, 4, 0], sel_drum: 0, sel_smp: 0, smp_len: new Float32Array(16), rec_slot: -1,
    fx: { bank: 0, eff: new Array(8).fill(0), held: new Array(8).fill(0), fsr: 0, lid: 0, rec: false },
    params: { wave: 5, dens: 6, poly: 5, vsyn: 10, wtscan: 0, prise: 0, bars: 1, beats: 16, kit: 0, cdrum: 9, vdrum: 10, csmp: 9, vsmp: 10, key: 0, mode: 3, swing: 0, mvol_idx: 9, syn_mode: 0 },
    micState: 'none', sampleRate: 48000,
  };
}

export function ledFrame(v) {
  const T = v.T, out = new Array(16);
  const ph = v.phases;
  if (v.vol_mode) {
    for (let i = 0; i < 16; i++) {
      let r = 0, g = 0, b = 0;
      if (i <= v.vol_pidx) {
        if (i <= 9) { r = 30; g = 120; b = 20; } else if (i <= 12) { r = 120; g = 90; b = 10; } else { r = 160; g = 25; b = 10; }
        if (i === v.vol_pidx) { r += 60; g += 60; b += 30; }
      }
      out[i] = [r & 255, g & 255, b & 255];
    }
  } else if (v.menu === MENU.FX) {
    const fh = T.fx.eff[v.fx_bank & 7] | 0;
    for (let i = 0; i < 16; i++) {
      const c = FXC[v.fx_bank & 7][(i >> 2) & 3];
      out[i] = ((fh >> i) & 1) ? [c[0], c[1], c[2]] : [c[0] >> 4, c[1] >> 4, c[2] >> 4];
    }
  } else if (v.menu === MENU.SESS) {
    for (let i = 0; i < 16; i++) {
      if (i === v.sess.warn) out[i] = [255, 0, 0];
      else if (i === v.sess.cur) out[i] = [200, 200, 200];
      else if ((v.sess.exists >> i) & 1) out[i] = [120, 70, 10];
      else out[i] = [0, 0, 0];
    }
  } else if (v.eng_focus === MENU.SMP) {
    if (v.seq_view) {
      const page = T.gstep >> 4, sl = T.songLen, sh = v.seq_place ? 1 : 2, placed = T.sm_page[T.sel_smp & 15] | 0;
      for (let i = 0; i < 16; i++) {
        const st = page * 16 + i;
        if (st >= sl) out[i] = [0, 0, 0];
        else if (st === T.gstep) out[i] = [140, 140, 140];
        else if ((placed >> i) & 1) out[i] = [40 >> sh, 220 >> sh, 170 >> sh];
        else out[i] = dimLava(ph[i]);
      }
    } else {
      for (let i = 0; i < 16; i++) {
        if (T.rec_slot === i) out[i] = [220, 30, 20];
        else if (T.smp_len[i] > 0) out[i] = [10, 55, 42];
        else out[i] = dimLava(ph[i]);
      }
    }
  } else if (v.eng_focus === MENU.DRUM && v.seq_view) {
    const sd = T.sel_drum & 3, c = VCOL[sd], sh = v.seq_place ? 1 : 2, page = T.gstep >> 4, sl = T.songLen, placed = T.dr_page[sd] | 0;
    for (let i = 0; i < 16; i++) {
      const st = page * 16 + i;
      if (st >= sl) out[i] = [0, 0, 0];
      else if (st === T.gstep) out[i] = [140, 140, 140];
      else if ((placed >> i) & 1) out[i] = [c[0] >> sh, c[1] >> sh, c[2] >> sh];
      else out[i] = dimLava(ph[i]);
    }
  } else if (v.eng_focus === MENU.SYNTH && v.seq_view) {
    const atEdit = v.syn.cache[v.syn.seq_edit % 128] | 0, atPlay = T.playing.syn | 0;
    for (let i = 0; i < 16; i++) {
      if ((v.syn.arm >> i) & 1) out[i] = [200, 200, 255];
      else if ((atPlay >> i) & 1) out[i] = [120, 180, 255];
      else if ((atEdit >> i) & 1) out[i] = [20, 40, 120];
      else out[i] = dimLava(ph[i]);
    }
  } else {
    const fsr = v.fsr;
    for (let i = 0; i < 16; i++) {
      let [r, g, b] = lavaColor(ph[i]);
      if ((v.held_mask >> i) & 1) {
        if (fsr >= 2600) { /* full */ } else if (fsr <= 1200) { r >>= 2; g >>= 2; b >>= 2; } else {
          const t = fsr - 1200;
          r = ((r >> 2) + Math.floor(((r - (r >> 2)) * t) / 1400)) & 255;
          g = ((g >> 2) + Math.floor(((g - (g >> 2)) * t) / 1400)) & 255;
          b = ((b >> 2) + Math.floor(((b - (b >> 2)) * t) / 1400)) & 255;
        }
      } else { r >>= 5; g >>= 5; b >>= 5; }
      out[i] = [r, g, b];
    }
  }
  return out;
}

// pre-dim colour channel -> screen value: hardware dim (*179>>8), then a gamma lift so dim lava
// is visible on a phone but still clearly dim.
export const screenChannel = (c) => Math.round(255 * Math.pow(((c * 179) >> 8) / 178, 0.55));

// ---------------------------------------------------------------------------------------------
// OLED screens (SPEC 3, L4884-5249). `v` is the view model built by the UI.

export function renderOled(fb, v) {
  fb.fill(0);
  const T = v.T, P = T.params, menu = v.menu;
  const bpm = T.bpm | 0;
  const animLeft = v.animStep ? 6 - v.animStep : 0;

  if (v.splash) {
    text(fb, 52, 18, 'PMOR', true);
    text(fb, 64 - v.splash.length * 3, 34, v.splash, true);
    if (v.error) text(fb, 64 - v.error.length * 3, 46, v.error, true);
    return fb;
  }
  if (v.bootFrame >= 0) {
    const prog = v.bootFrame / 19;
    for (let k = 0; k < 5; k++) {
      let s = prog * 2 + k * 0.2; s -= Math.trunc(s); s *= s;
      const w = 3 + Math.trunc(s * 61), h = 2 + Math.trunc(s * 30);
      rect(fb, 64 - w, 32 - h, 63 + w, 31 + h, true, false);
    }
    if (v.bootFrame >= 4) text(fb, 25, 29, 'POCKETGORATOR', true);
    return fb;
  }

  if (v.vol_mode) {
    text(fb, 31, 6, 'MASTER VOL', true);
    const pct = Math.trunc((v.vol_pidx * 125 + 7) / 15);
    const ps = pct + '%';
    text(fb, 64 - ps.length * 3, 20, ps, true);
    const bw = Math.trunc((v.vol_pidx * 124) / 15);
    rect(fb, 2, 32, 126, 48, true, false);
    if (bw > 0) rect(fb, 2, 32, 2 + bw, 48, true, true);
    rect(fb, 101, 29, 101, 51, true, true);
    text(fb, 7, 56, pct > 100 ? 'TAN ZONE  BOTH=EXIT' : 'PADS SET  BOTH=EXIT', true);
  } else if (v.animStep > 0) {
    const prog = v.animStep / 6;
    for (let k = 0; k < 5; k++) {
      let s = prog * 0.99 + k * 0.2; s -= Math.trunc(s);
      if (v.animDir < 0) s = 1 - s;
      s *= s;
      const w = 3 + Math.trunc(s * 61), h = 2 + Math.trunc(s * 30);
      rect(fb, 64 - w, 32 - h, 63 + w, 31 + h, true, false);
    }
    text(fb, 64 - MN[menu].length * 3, 29, MN[menu], true);
  } else if (menu === MENU.DASH && !v.saver) {
    const s = v.sensors;
    const f1 = (x) => {
      const sg = x < 0 ? '-' : '+', a = x < 0 ? -x : x;
      let w = Math.trunc(a), f = Math.trunc((a - w) * 10 + 0.5);
      if (f >= 10) { w++; f = 0; }
      return sg + w + '.' + f;
    };
    row(fb, 0, 'A:' + f1(s.ax) + ' ' + f1(s.ay) + ' ' + f1(s.az));
    row(fb, 9, 'G:' + f1(s.gx) + ' ' + f1(s.gy) + ' ' + f1(s.gz));
    row(fb, 18, 'M:' + f1(s.mx) + ' ' + f1(s.my) + ' ' + f1(s.mz));
    row(fb, 27, 'D:' + lp(s.dist, 4) + 'mm S:' + s.status.toString(16).toUpperCase().padStart(2, '0') + ' O:+' + s.oct);
    row(fb, 36, 'E1:' + pad(v.e1s, 3) + ' E2:' + pad(v.e2s, 3));
    const f = Math.min(19, Math.trunc(T.out_amp * 19));
    row(fb, 45, '[' + '#'.repeat(f) + '.'.repeat(19 - f) + ']');
    row(fb, 54, 'F:' + lp(v.fsr, 4) + ' SD:' + pad(v.sdOk ? 'OK' : '--', 2) + ' K:' + z2(pop16(v.held_mask)) + ' :D');
  } else if (v.saver) {
    // L4944-4989
    const sv = v.sv, amp = T.out_amp, se = v.sensors;
    sv.pe += (amp - sv.pe) * 0.25;
    sv.sx += (se.ax - sv.sx) * 0.05; sv.sy += (se.ay - sv.sy) * 0.05; sv.sz += (se.az - sv.sz) * 0.05;
    const gm = Math.min(1, Math.sqrt(se.gx * se.gx + se.gy * se.gy + se.gz * se.gz) / 250);
    sv.gsp += (gm - sv.gsp) * 0.15;
    sv.ph += 0.012 + sv.gsp * 0.22;
    if (sv.ph > 62.831853) sv.ph -= 62.831853;
    const fa = 2 + sv.sx * 1.6, fb_ = 3 + sv.sy * 1.6, fc = 4 + sv.sz * 1.6, fd = 5 - sv.sx * 1.4;
    const R = Math.min(30, 8 + sv.pe * 40), N = 110, ph = sv.ph;
    for (let pass = 0; pass < 2; pass++) {
      const dir = pass ? -1 : 1, rr = pass ? R * 0.62 : R;
      let pxp = 0, pyp = 0, first = true;
      for (let i = 0; i <= N; i++) {
        const u = (i / N) * 6.2831853;
        const fx = 64 + rr * (Math.sin(fa * u + ph * dir) + 0.5 * Math.sin(fc * u - ph * 0.7));
        const fy = 32 + rr * 0.55 * (Math.sin(fb_ * u + ph * 1.1 * dir) + 0.5 * Math.sin(fd * u));
        const xi = clamp(Math.trunc(fx), 0, 127), yi = clamp(Math.trunc(fy), 0, 63);
        if (!first) line(fb, pxp, pyp, xi, yi, true);
        pxp = xi; pyp = yi; first = false;
      }
    }
  } else if (v.seq_view && menu === MENU.SMP) {
    text(fb, 0, 0, 'SAMPLE SEQ', true);
    const sec = T.smp_len[T.sel_smp & 15] || 0, whole = Math.floor(sec), tenth = Math.floor((sec - whole) * 10 + 1e-6);
    row(fb, 11, 'SLOT:' + z2((T.sel_smp & 15) + 1) + '  ' + whole + '.' + tenth + 's');
    row(fb, 21, (v.seq_place ? '>PLACING<' : 'PLAYING') + '  STEP ' + z2(T.gstep + 1));
    row(fb, 31, 'STEPS:' + z2(T.sm_counts[T.sel_smp & 15] | 0));
    row(fb, 41, 'BPM ' + lp(bpm, 3));
    row(fb, 54, 'L=BACK R=' + (v.seq_place ? 'PLAY' : 'PLACE') + ' RT=SLOT');
  } else if (v.seq_view && menu === MENU.DRUM) {
    const CHN = ['KICK', 'PERC', 'SNARE', 'HAT'], sd = T.sel_drum & 3;
    text(fb, 0, 0, 'DRUM MACHINE', true);
    row(fb, 11, 'SND: ' + CHN[sd] + ' ' + (P.kit ? PADN2 : PADN)[T.dr_pad[sd] & 15]);
    row(fb, 21, (v.seq_place ? '>PLACING<' : 'PLAYING') + '  STEP ' + z2(T.gstep + 1));
    row(fb, 31, 'K:' + z2(T.dr_counts[0]) + ' P:' + z2(T.dr_counts[1]) + ' S:' + z2(T.dr_counts[2]) + ' H:' + z2(T.dr_counts[3]));
    row(fb, 41, 'BPM ' + lp(bpm, 3));
    row(fb, 54, 'L=BACK R=' + (v.seq_place ? 'PLAY' : 'PLACE') + ' RT=SND');
  } else if (menu === MENU.SYNTH) {
    text(fb, 49, 0, 'SYNTH', true);
    const ML = ['SEQ', 'ARP', 'GEN'];
    for (let i = 0; i < 3; i++) {
      const x0 = 2 + i * 43, act = P.syn_mode === i;
      rect(fb, x0, 10, x0 + 39, 20, true, act);
      text(fb, x0 + 11, 11, ML[i], !act);
      if (v.mode_cur === i) rect(fb, x0 - 2, 8, x0 + 41, 22, true, false);
    }
    if (v.seq_view && P.syn_mode === SYN_SEQ) {
      // the firmware never reaches this screen (SPEC 0.8); shown here by design
      const sl = T.songLen, bpb = P.beats > 0 ? P.beats : 16, gs = T.gstep % sl;
      row(fb, 25, 'EDIT ' + z2(v.syn.seq_edit + 1) + '/' + z2(sl) + ' BAR' + (Math.trunc(v.syn.seq_edit / bpb) + 1));
      row(fb, 35, 'PLAY ' + z2(gs + 1) + '     BAR' + (Math.trunc(gs / bpb) + 1));
      row(fb, 45, 'ARM:' + pop16(v.syn.arm) + '  HOLD R=REC');
      row(fb, 55, 'RCLK=PLACE  L=BACK');
    } else {
      const LB = ['WAVE', 'DENS', 'POLY', 'VOL', 'SCAN', 'PRISE', 'BARS', 'BEATS', 'TAP'];
      const VL = [WAVE_NAMES[P.wave] || '?', P.dens, P.poly, P.vsyn * 10 + '%', P.wtscan, P.prise, P.bars, P.beats, bpm];
      const sel = v.mode_cur >= 3 ? v.mode_cur - 3 : -1;
      const top = clamp(sel - 1, 0, 5);
      for (let r = 0; r < 4; r++) listRow(fb, v, 25 + r * 10, LB[top + r], VL[top + r], top + r === sel);
    }
  } else if (menu === MENU.FX) {
    text(fb, 46, 0, '<FX' + ((v.fx_bank & 7) + 1) + '>', true);
    const nm = FXNB[v.fx_bank & 7], fe = T.fx.eff[v.fx_bank & 7] | 0;
    let ln = 0;
    for (let b = 0; b < 16 && ln < 3; b++) if ((fe >> b) & 1) { row(fb, 11 + ln * 10, nm[b]); ln++; }
    if (ln === 0) row(fb, 11, 'HOLD PADS  RT=BANK');
    const fw = Math.trunc(T.fx.fsr * 124);
    rect(fb, 2, 44, 126, 52, true, false);
    if (fw > 0) rect(fb, 2, 44, 2 + fw, 52, true, true);
    row(fb, 56, 'FSR' + lp(Math.trunc(T.fx.fsr * 100), 3) + '%   BPM:' + lp(bpm, 3));
    if (v.fx_bank === 0 && v.recBadge) { rect(fb, 0, 0, 23, 9, true, true); text(fb, 2, 1, 'REC', false); }
  } else if (menu === MENU.SMP) {
    text(fb, 43, 0, 'SAMPLER', true);
    rect(fb, 2, 10, 41, 20, true, v.seq_view);
    text(fb, 8, 11, 'SEQ EDIT', !v.seq_view);
    if (v.mode_cur === 0) rect(fb, 0, 8, 43, 22, true, false);
    let l;
    if (v.seq_view) l = 'BAR ' + (Math.trunc(T.gstep / (P.beats > 0 ? P.beats : 16)) + 1) + '/' + P.bars + '  SLOT' + z2((T.sel_smp & 15) + 1);
    else if (T.rec_slot >= 0) l = 'REC PAD ' + z2(T.rec_slot + 1) + '...';
    else if (T.micState === 'denied') l = 'MIC DENIED';
    else if (T.micState === 'asking') l = 'MIC: ALLOW ACCESS';
    else { let nf = 0; for (let s = 0; s < 16; s++) if (T.smp_len[s] > 0) nf++; l = 'SLOTS:' + z2(nf) + ' HOLD=REC'; }
    row(fb, 23, l);
    const LB = ['CHANCE', 'VOL', 'BARS', 'BEATS', 'TAP'], VL = [P.csmp, P.vsmp * 10 + '%', P.bars, P.beats, bpm];
    const sel = v.mode_cur >= 1 ? v.mode_cur - 1 : -1;
    for (let r = 0; r < 3; r++) {
      const idx = r + (sel >= 3 ? 2 : 0);
      if (idx >= 5) break;
      listRow(fb, v, 33 + r * 10, LB[idx], VL[idx], idx === sel);
    }
  } else if (menu === MENU.SESS) {
    text(fb, 31, 0, 'PATCHES', true);
    for (let i = 0; i < 16; i++) {
      const x0 = 2 + (i % 4) * 14, y0 = 12 + (i >> 2) * 10, ex = (v.sess.exists >> i) & 1;
      rect(fb, x0, y0, x0 + 11, y0 + 8, true, !!ex);
      if (i === v.sess.cur) rect(fb, x0 - 1, y0 - 1, x0 + 12, y0 + 9, true, false);
    }
    for (let p = 0; p < 4; p++) text(fb, 118, 12 + p * 10, 'ABCD'[p], true);
    if (v.sess.warn >= 0) row(fb, 56, 'RELEASE=KEEP P' + z2(v.sess.warn + 1) + '!');
    else row(fb, 56, 'PG' + 'ABCD'[v.sess.cur >> 2] + '.' + ((v.sess.cur % 4) + 1) + ' TAP:GO HOLD:NEW');
  } else if (menu === MENU.GLOB) {
    text(fb, 40, 0, 'GLOBALS', true);
    const gl = ['BPM   ' + lp(bpm, 3), 'KEY   ' + sgn(P.key), 'MODE  ' + (MODE_NAMES[P.mode] || '?'), 'SWING ' + P.swing];
    for (let p = 0; p < 4; p++) {
      const y = 16 + p * 11, s = v.mode_cur === p;
      if (s) rect(fb, 0, y - 1, 127, y + 8, true, true);
      text(fb, 0, y, ' ' + gl[p], !s);
      if (s && v.param_edit) editMark(fb, y, !s);
    }
  } else {
    text(fb, 49, 0, 'DRUMS', true);
    rect(fb, 2, 10, 41, 20, true, v.seq_view);
    text(fb, 8, 11, 'SEQ EDIT', !v.seq_view);
    if (v.mode_cur === 0) rect(fb, 0, 8, 43, 22, true, false);
    if (v.seq_view) row(fb, 23, 'BAR ' + (Math.trunc(T.gstep / (P.beats > 0 ? P.beats : 16)) + 1) + '/' + P.bars + '  ' + PADN[T.dr_pad[T.sel_drum & 3] & 15]);
    const LB = ['KIT', 'CHANCE', 'VOL', 'BARS', 'BEATS', 'TAP'];
    const VL = [P.kit ? 'ELEC' : 'CLASS', P.cdrum, P.vdrum * 10 + '%', P.bars, P.beats, bpm];
    const sel = v.mode_cur >= 1 ? v.mode_cur - 1 : -1;
    const top = sel <= 1 ? 0 : (sel >= 4 ? 3 : sel - 1);
    const y0 = v.seq_view ? 33 : 25, rows = v.seq_view ? 3 : 4;
    for (let r = 0; r < rows; r++) {
      const idx = top + r;
      if (idx >= 6) break;
      listRow(fb, v, y0 + r * 10, LB[idx], VL[idx], idx === sel);
    }
  }

  // audio-reactive border particles (L5198-5219)
  if (!v.vol_mode && animLeft === 0 && !v.saver && menu !== MENU.FX) {
    const np = Math.trunc(T.out_amp * 16);
    for (let p = 0; p < np; p++) {
      const rr = v.rand(), edge = (rr >>> 18) & 3;
      if (edge === 0) { const x = rr & 63, y = (rr >>> 8) & 2; px(fb, x, y, true); px(fb, 127 - x, y, true); }
      else if (edge === 1) { const x = rr & 63, y = 63 - ((rr >>> 8) & 2); px(fb, x, y, true); px(fb, 127 - x, y, true); }
      else if (edge === 2) { const y = rr & 31, x = (rr >>> 8) & 2; px(fb, x, y, true); px(fb, x, 63 - y, true); }
      else { const y = rr & 31, x = 127 - ((rr >>> 8) & 2); px(fb, x, y, true); px(fb, x, 63 - y, true); }
    }
  }
  // page pips (L5222)
  if (!v.saver && (menu !== MENU.DASH || animLeft > 0)) {
    for (let i = 0; i < MENU_COUNT; i++) rect(fb, 92 + i * 4, 0, 94 + i * 4, 2, true, i === menu);
  }
  // save icon (L5229)
  if (!v.saver && !v.vol_mode) {
    if (v.saveFail) {                      // a failed save must be visible: flashing '!' in the icon box
      const on = (v.spin & 8) !== 0;
      if (on) rect(fb, 119, 0, 127, 8, true, true);
      text(fb, 121, 0, '!', !on);
    } else if (v.saveBusy) {
      const sp = v.spin & 7;
      const dx = sp < 2 ? 2 : sp < 4 ? 0 : sp < 6 ? -2 : 0, dy = sp < 2 ? 0 : sp < 4 ? 2 : sp < 6 ? 0 : -2;
      px(fb, 124 + dx, 4 + dy, true); px(fb, 124, 4, true);
    } else {
      px(fb, 123, 1, true); px(fb, 123, 2, true);
      line(fb, 120, 7, 126, 3, true); line(fb, 126, 7, 120, 3, true); line(fb, 120, 3, 126, 3, true);
    }
  }
  return fb;
}

function listRow(fb, v, y, label, value, sel) {
  if (sel) rect(fb, 0, y - 1, 127, y + 8, true, true);
  text(fb, 0, y, ' ' + pad(label, 6) + ' ' + value, !sel);
  if (sel && v.param_edit) editMark(fb, y, !sel);
}
// the firmware writes "\x10" here, which libDaisy refuses to draw, so edit mode is invisible on the
// unit (SPEC 12.3). A small right-pointing triangle takes its place.
function editMark(fb, y, on) {
  px(fb, 120, y + 1, on); px(fb, 120, y + 2, on); px(fb, 120, y + 3, on); px(fb, 120, y + 4, on); px(fb, 120, y + 5, on);
  px(fb, 121, y + 2, on); px(fb, 121, y + 3, on); px(fb, 121, y + 4, on); px(fb, 122, y + 3, on);
}

// ---------------------------------------------------------------------------------------------
// Layout (device pixels) and painting

const C = {
  page: '#0b0714', body: '#120b20', edge: '#2b1f48', glass: '#04030a', lit: '#e6eeff',
  padOff: '#150f24', encBody: '#1c1432', encRing: '#4a3978', encIn: '#271c47', encInOn: '#4a3480', mark: '#c4a3ff',
  word: '#6b4fb0', track: '#2b1f48', thumb: '#b794ff', help: '#7a62b8',
};

export function computeLayout(cssW, cssH, dpr) {
  const W = Math.max(1, Math.round(cssW * dpr)), H = Math.max(1, Math.round(cssH * dpr));
  const ts = Math.max(1, Math.round(dpr));
  const m = Math.round(Math.min(W, H) * 0.035);
  // the column is as wide as the screen allows; the OLED inside it is an integer multiple of 128x64
  const colW = Math.min(W - 2 * m, Math.floor((H - 2 * m) * 0.6));
  const g = Math.round(colW * 0.03), stripH = Math.round(34 * dpr), encD = Math.round(colW * 0.36);
  let s = Math.max(1, Math.floor((colW - 2 * Math.max(4, Math.round(colW * 0.035))) / OW)), bez, oledBlockH, padsAvail;
  for (;;) {
    bez = Math.max(4, Math.round(OW * s * 0.035));
    oledBlockH = OH * s + 2 * bez;
    padsAvail = H - 2 * m - stripH - oledBlockH - encD - 3 * g;
    if (padsAvail >= colW * 0.55 || s === 1) break;
    s--;
  }
  const x0 = Math.round((W - colW) / 2);
  const padsSize = Math.max(Math.min(colW, padsAvail), 80);
  const extra = Math.max(0, padsAvail - padsSize), yTop = m + Math.floor(extra / 2);
  const strip = { x: x0, y: yTop, w: colW, h: stripH };
  const obW = OW * s + 2 * bez;
  const oledBlock = { x: x0 + Math.round((colW - obW) / 2), y: yTop + stripH, w: obW, h: oledBlockH };
  const glass = { x: oledBlock.x + bez, y: oledBlock.y + bez, w: OW * s, h: OH * s };
  const R = encD / 2, ecy = oledBlock.y + oledBlockH + g + R;
  const enc = [{ cx: x0 + colW * 0.25, cy: ecy, r: R }, { cx: x0 + colW * 0.75, cy: ecy, r: R }];
  const gp = Math.round(padsSize * 0.03), cell = Math.floor((padsSize - 3 * gp) / 4), gridW = 4 * cell + 3 * gp;
  const py0 = Math.round(ecy + R + g), px0 = x0 + Math.round((colW - gridW) / 2), pads = [];
  for (let k = 0; k < 16; k++) pads.push({ x: px0 + (k & 3) * (cell + gp), y: py0 + (k >> 2) * (cell + gp), w: cell, h: cell });
  const bp = Math.round(colW * 0.03);
  const body = { x: x0 - bp, y: yTop - bp, w: colW + 2 * bp, h: py0 + gridW - yTop + 2 * bp, r: Math.round(colW * 0.07) };
  // DASH sliders live in the strip
  const sl = [], cw = colW / 3, sy = strip.y + Math.round(stripH * 0.4), sh = stripH - Math.round(stripH * 0.4);
  for (let i = 0; i < 3; i++) sl.push({ x: strip.x + i * cw, y: sy, w: cw, h: sh, tx: strip.x + i * cw + 11 * ts, tw: cw - 17 * ts });
  return { W, H, ts, m, strip, oledBlock, glass, scale: s, enc, pads, cell, gp, body, sliders: sl, padsRect: { x: px0, y: py0, w: gridW, h: gridW }, gap: g };
}

function rr(ctx, x, y, w, h, r) {
  r = Math.min(r, w / 2, h / 2);
  ctx.beginPath();
  ctx.moveTo(x + r, y); ctx.lineTo(x + w - r, y); ctx.arcTo(x + w, y, x + w, y + r, r);
  ctx.lineTo(x + w, y + h - r); ctx.arcTo(x + w, y + h, x + w - r, y + h, r);
  ctx.lineTo(x + r, y + h); ctx.arcTo(x, y + h, x, y + h - r, r);
  ctx.lineTo(x, y + r); ctx.arcTo(x, y, x + r, y, r); ctx.closePath();
}

function pixText(ctx, str, x, y, sc, color) {
  ctx.fillStyle = color;
  for (let i = 0; i < str.length; i++) {
    const c = str.charCodeAt(i) - 32;
    if (c >= 0 && c < 95) {
      for (let r = 0; r < 8; r++) {
        const b = FONT[c * 8 + r];
        for (let j = 0; j < 6;) {
          if (!((b >> (5 - j)) & 1)) { j++; continue; }
          let k = j; while (k < 6 && ((b >> (5 - k)) & 1)) k++;
          ctx.fillRect(x + (i * 6 + j) * sc, y + r * sc, (k - j) * sc, sc);
          j = k;
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------------------------
// The UI

export function createPmorUI(opts) {
  const { canvas, engine, storage } = opts;
  const nowFn = opts.now || (() => Date.now());
  const timers = opts.timers || { set: (f, ms) => setTimeout(f, ms), clear: (id) => clearTimeout(id) };
  const ctx = canvas.getContext('2d');

  let T = blankTick();
  let phase = 'idle'; // idle -> starting -> boot -> run
  let startErr = '';
  let bootStart = 0;
  let hidden = false, needResume = false, lastTickAt = 0;
  const S = {
    menu: MENU.SYNTH, mode_cur: 0, param_edit: false, seq_view: false, seq_place: false, seq_edit: 0, view_page: 0,
    vol_mode: false, vol_pidx: 9, eng_focus: MENU.SYNTH, fx_bank: 0, last_enc: nowFn(),
  };
  let animStart = -1, animDir = 1;
  let fxBankHold = 0;
  const fxHeld = new Array(8).fill(0);
  let fxSentAt = -1e9;
  let synArm = 0;
  const synCache = new Uint16Array(128);
  const sv = { pe: 0, sx: 0, sy: 0, sz: 0, gsp: 0, ph: 0 };
  let rngSt = 0xbeef1234;
  const rand = () => { rngSt = (Math.imul(rngSt, 1664525) + 1013904223) >>> 0; return rngSt; };
  const t0 = nowFn();
  let ledStep = 0;

  // sensors (DASH sliders stand in for FSR lean, lidar and tilt)
  const slid = { lean: 0.5, lidar: 0, tilt: 0.5 };
  let fsrRaw = 0, fsrSentAt = -1e9, octShift = 0;
  const sensors = { ax: 0, ay: 0, az: 1, gx: 0, gy: 0, gz: 0, mx: 0, my: 0, mz: 0, dist: 0, status: 0, oct: 0 };
  const e1 = { s: '--', until: 0 }, e2 = { s: '--', until: 0 };

  // pads
  let heldMask = 0, noteHeld = 0, smpHeld = 0;
  const padPress = new Array(16).fill(null); // {real, v} per pad
  const sessPress = new Array(16).fill(-1);
  let sessWarn = -1;

  // encoders (A.11)
  const E = [mkEnc(), mkEnc()];
  function mkEnc() { return { touch: false, drag: false, pressAt: 0, delta: 0, clicked: false, counts: 0, pid: -1, sx: 0, sy: 0, lx: 0, ly: 0, circ: false, px: 0, ang: 0 }; }
  const heldE = (i) => E[i].touch && !E[i].drag;
  let c1Pend = -1, c2Pend = -1, bothLatch = false, c1Cleared = false, acc1 = 0, acc2 = 0;
  let recFxOn = false, liveWriteOn = false;
  const HOLD_FX_MS = 90;

  // patches
  let slots = new Array(16).fill(null);
  let exists = 1, cur = 0, planted = false;
  let saveAt = -1, saveBusy = false, saveFailed = false, saving = false, lastDirty = -1;
  const slotDirty = new Set();
  let opChain = Promise.resolve();
  let lastWarn = -1, stripWasDash = false;

  // rendering
  let L = null;
  let fbCur = makeFb(), fbNew = makeFb(), fbPainted = null;
  let ledPainted = null, ledDirty = true, encDirty = true, stripDirty = true, padTouch = 0, padTouchPainted = -1;
  let lastFrame = -1e9, frameDue = Infinity, timerId = null, timerAt = Infinity;
  const ptrs = new Map();

  // ------------------------------------------------------------------ helpers
  const now = () => nowFn();
  const markDirty = () => { saveAt = now() + 3000; saveBusy = true; };
  const setTick = (patch) => { Object.assign(T, patch); };

  function applyPage(p) {
    S.menu = p >= 0 && p < MENU_COUNT ? p : 0;
    if (S.menu <= MENU.SMP) S.eng_focus = S.menu;
  }

  function engSafe(fn) { try { return fn(); } catch (e) { return undefined; } }

  // ------------------------------------------------------------------ patches (SPEC 8)
  // storage is async: get(key) -> value|null, set(key, value), delete(key). Keys: 'meta' and 's0'..'s15'.
  async function loadStore() {
    try {
      const meta = await storage.get('meta');
      if (!meta || meta.v !== 1 || typeof meta.exists !== 'number') return null;
      const sl = new Array(16).fill(null);
      for (let i = 0; i < 16; i++) {
        if (!((meta.exists >> i) & 1)) continue;
        const o = await storage.get('s' + i);
        if (o && o.p) sl[i] = o;
      }
      if (!sl[0]) return null;
      return { meta, slots: sl };
    } catch (e) { return null; }
  }

  // writes only the slots that changed, then the meta record (so a half-written save never lists a missing slot)
  async function persist() {
    const todo = [...slotDirty];
    slotDirty.clear();
    let i = -1;
    try {
      for (i of todo) {
        if (slots[i]) await storage.set('s' + i, slots[i]); else await storage.delete('s' + i);
      }
      await storage.set('meta', { v: 1, cur, exists, planted });
      saveFailed = false;
    } catch (e) {
      for (const k of todo) slotDirty.add(k);
      saveFailed = true;
      throw e;
    }
  }

  async function stash() {
    const p = await engine.getPersist();
    slots[cur] = { p, page: S.menu };
    slotDirty.add(cur);
  }

  async function doSave() {
    saveAt = -1;
    if (saving) { saveAt = now() + 500; return; }
    saving = true;
    try { await stash(); await persist(); } catch (e) { saveFailed = true; saveAt = now() + 10000; }
    saving = false;
    if (saveAt < 0 && !saveFailed) saveBusy = false;
    requestFrame(false);
    schedule();
  }

  function resetEditState() {
    synArm = 0; synCache.fill(0); S.seq_edit = 0;
    engSafe(() => engine.synArm(0));
  }

  const chain = (fn) => { opChain = opChain.then(fn).catch(() => {}); return opChain; };

  function sessionLoad(idx) {
    return chain(async () => {
      await stash();
      cur = idx; exists |= 1 << idx;
      const s = slots[idx];
      if (!s) return;
      engine.setPersist(s.p);
      applyPage(s.page);
      resetEditState();
      markDirty();
      requestFrame(true);
    });
  }
  function sessionNew(idx) {
    return chain(async () => {
      await stash();
      cur = idx; exists |= 1 << idx;
      const d = JSON.parse(JSON.stringify(engine.defaults()));
      slots[idx] = { p: d, page: MENU.SYNTH }; slotDirty.add(idx);
      engine.setPersist(d);
      applyPage(MENU.SYNTH);
      resetEditState();
      markDirty();
      requestFrame(true);
    });
  }
  function sessionDelete(idx) {
    if (idx === 0) return opChain;
    exists &= ~(1 << idx); slots[idx] = null; slotDirty.add(idx);
    markDirty();
    if (cur === idx) return sessionLoad(0);
    return opChain;
  }

  // L3447-3479: kick 0x1111, voice 1 0x1010, voice 2 0x5555, menu DRUMS. Built through engine calls so
  // the opaque persist format is never touched; planted once (the firmware's re-plant is bug 0.17).
  async function plantExample() {
    engine.set('mvol_idx', 0); // the pattern must not be audible while it is being captured
    engine.setPersist(JSON.parse(JSON.stringify(engine.defaults())));
    for (const st of [0, 4, 8, 12]) engine.toggleStep(1, 0, st);
    for (const st of [4, 12]) engine.toggleStep(1, 1, st);
    for (let st = 0; st < 16; st += 2) engine.toggleStep(1, 2, st);
    const p = await engine.getPersist();
    slots[1] = { p, page: MENU.DRUM }; slotDirty.add(1);
    exists |= 2; planted = true;
  }

  async function boot() {
    try {
      const st = await loadStore();
      if (st) {
        const m = st.meta;
        slots = st.slots;
        exists = (m.exists | 1) & 0xffff; cur = m.cur >= 0 && m.cur < 16 && slots[m.cur] ? m.cur : 0; planted = !!m.planted;
        for (let i = 0; i < 16; i++) if (!slots[i]) exists &= ~(1 << i);
      } else {
        slots[0] = { p: JSON.parse(JSON.stringify(engine.defaults())), page: MENU.SYNTH };
        slotDirty.add(0);
        exists = 1; cur = 0;
      }
      if (!planted) {
        if (!((exists >> 1) & 1)) await plantExample();
        planted = true;
      }
      engine.setPersist(slots[cur].p);
      applyPage(slots[cur].page);
      engine.set('lid', 0);
    } finally {
      engine.set('mvol_idx', S.vol_pidx);   // always un-mute, even if the capture above threw
    }
    try { await persist(); } catch (e) { /* saveFailed is set: the icon flashes, the instrument still runs */ }
  }

  // ------------------------------------------------------------------ engine-facing actions
  function clearFxHeld() {
    for (let b = 0; b < 8; b++) if (fxHeld[b]) { fxHeld[b] = 0; engine.fxHeld(b, 0); fxSentAt = now(); }
  }
  // the firmware reads the tilt filter only while the FX page is up (L4779), so 'ax' is sent on entering FX
  // and on a tilt change made there; the DASH slider just stores the value
  function sendAx() { if (S.menu === MENU.FX) engine.set('ax', sensors.ax); }
  function setFxBank(b) {
    S.fx_bank = b; fxBankHold = now() + 250;
    engine.fxBank(b);
  }

  function optToggle(eid, voice, st) {
    if ((st >> 4) !== (T.gstep >> 4)) return;
    const bit = 1 << (st & 15);
    if (eid === 1) {
      const was = (T.dr_page[voice] & bit) !== 0;
      T.dr_page[voice] ^= bit; T.dr_counts[voice] += was ? -1 : 1;
    } else {
      const was = (T.sm_page[voice] & bit) !== 0;
      T.sm_page[voice] ^= bit; T.sm_counts[voice] += was ? -1 : 1;
    }
  }

  function padDown(k, info) {
    const n = now(), bit = 1 << k;
    S.last_enc = n;
    heldMask |= bit;
    if (S.vol_mode) {
      S.vol_pidx = k; engine.set('mvol_idx', k); T.params.mvol_idx = k; markDirty();
    } else if (S.menu === MENU.FX) {
      const b = S.fx_bank & 7;
      fxHeld[b] |= bit; engine.fxHeld(b, fxHeld[b]); fxSentAt = now();
    } else if (S.menu === MENU.SESS) {
      sessPress[k] = n;
    } else {
      const synEdit = S.eng_focus === MENU.SYNTH && T.params.syn_mode === SYN_SEQ && S.seq_view;
      if (S.seq_place && S.eng_focus !== MENU.SYNTH) {
        const st = S.view_page * 16 + k;
        if (st < T.songLen) {
          const eid = S.eng_focus === MENU.DRUM ? 1 : 2, voice = eid === 1 ? T.sel_drum & 3 : T.sel_smp & 15;
          engine.toggleStep(eid, voice, st); optToggle(eid, voice, st); markDirty();
        }
      } else if (S.eng_focus === MENU.SYNTH) {
        engine.noteOn(k); noteHeld |= bit;
        if (synEdit) { synArm = (synArm ^ bit) & 0xffff; engine.synArm(synArm); }
      } else if (S.eng_focus === MENU.SMP) {
        engine.samplerPress(k); smpHeld |= bit;
      } else {
        engine.drumHit(k);
        const v = 3 - (k >> 2);
        if (T.sel_drum !== v || T.dr_pad[v] !== k) markDirty();
        T.sel_drum = v; T.dr_pad[v] = k;
      }
    }
    sendFsr(true);
  }

  function padUp(k) {
    const n = now(), bit = 1 << k;
    heldMask &= ~bit; padPress[k] = null;
    for (let b = 0; b < 8; b++) if (fxHeld[b] & bit) { fxHeld[b] &= ~bit; engine.fxHeld(b, fxHeld[b]); fxSentAt = n; }
    if (noteHeld & bit) { noteHeld &= ~bit; engine.noteOff(k); }
    if (S.menu === MENU.SESS && sessPress[k] >= 0) {
      const held = n - sessPress[k];
      if (held < 800 && ((exists >> k) & 1)) sessionLoad(k);
      sessPress[k] = -1;
    }
    if (smpHeld & bit) { smpHeld &= ~bit; engine.samplerRelease(k); if (T.rec_slot === k) markDirty(); }
    sendFsr(true);
  }

  // FSR (SPEC 2.1): real pointer pressure or contact area if the device reports it, else the DASH lean slider
  function sendFsr(force) {
    const n = now();
    let raw = 0;
    if (heldMask) {
      let real = false, sum = 0;
      for (let k = 0; k < 16; k++) if (padPress[k] && padPress[k].real) { real = true; sum += padPress[k].v; }
      raw = 400 + Math.round((real ? Math.min(1, sum) : slid.lean) * 2200);
    }
    if (raw === fsrRaw) return;
    if (!force && (Math.abs(raw - fsrRaw) < 40 || n - fsrSentAt < 25)) return;
    fsrRaw = raw; fsrSentAt = n;
    engine.set('fsr', raw);
  }

  function updateSensors() {
    sensors.ax = Math.round((slid.tilt * 2 - 1) * 1000) / 1000;
    sensors.dist = Math.round(slid.lidar * 600);
    const d = sensors.dist;
    if (d === 0 || d > 750) octShift = 0;
    else {
      const zt = d < 150 ? 0 : d < 300 ? 1 : d < 450 ? 2 : 3, zlo = [0, 150, 300, 450], zhi = [150, 300, 450, 750];
      if (zt > octShift && d >= zlo[zt] + 20) octShift = zt;
      else if (zt < octShift && d <= zhi[zt] - 20) octShift = zt;
    }
    sensors.oct = octShift;
  }

  // ------------------------------------------------------------------ encoder logic (A.11, L4504-4708)
  function processEncoders(n) {
    let d1 = E[0].delta, d2 = E[1].delta; E[0].delta = 0; E[1].delta = 0;
    let c1 = E[0].clicked, c2 = E[1].clicked; E[0].clicked = false; E[1].clicked = false;
    const h1 = heldE(0), h2 = heldE(1);
    if (d1 || d2 || c1 || c2 || h1 || h2) S.last_enc = n;
    let doClear = false;
    if (c1) { c1Pend = n; c1 = false; }
    if (c2) { c2Pend = n; c2 = false; }
    if (h1 && h2 && !bothLatch) {
      bothLatch = true; S.vol_mode = !S.vol_mode; c1Pend = c2Pend = -1;
    }
    if (!h1 && !h2) bothLatch = false;
    if (bothLatch) c1Pend = c2Pend = -1;
    if (c1Pend >= 0 && c1Cleared && !h1) { c1Pend = -1; c1Cleared = false; }
    else if (c1Pend >= 0) {
      if (h1 && !c1Cleared && n - c1Pend >= 700) { doClear = true; c1Cleared = true; }
      else if (!h1) { c1 = true; c1Pend = -1; }
    }
    if (c2Pend >= 0 && n - c2Pend >= 130) { c2 = true; c2Pend = -1; }
    // modal overlay; a left-hold must not wipe data from behind it (firmware wart, deviation)
    if (S.vol_mode) { d1 = 0; d2 = 0; c1 = false; c2 = false; doClear = false; }

    if (d1 > 0) { e1.s = '>'; e1.until = n + 250; } else if (d1 < 0) { e1.s = '<'; e1.until = n + 250; }
    if (c1) { e1.s = 'CLK'; e1.until = n + 250; }
    if (d2 > 0) { e2.s = '>'; e2.until = n + 250; } else if (d2 < 0) { e2.s = '<'; e2.until = n + 250; }
    if (c2) { e2.s = 'CLK'; e2.until = n + 250; }

    acc1 += d1;
    if (acc1 >= 4 || acc1 <= -4) {
      animStart = n; animDir = acc1 > 0 ? 1 : -1;
      S.menu = (S.menu + (acc1 > 0 ? 1 : MENU_COUNT - 1)) % MENU_COUNT;
      acc1 = 0;
      sendAx();
      S.seq_view = false; S.seq_place = false; S.param_edit = false; S.mode_cur = 0;
      clearFxHeld();
      markDirty();
      if (S.menu <= MENU.SMP) S.eng_focus = S.menu;
    }

    if (doClear) {
      if (S.menu === MENU.FX) engine.fxClearAuto();
      else if (S.menu === MENU.SYNTH && T.params.syn_mode === SYN_SEQ) {
        engine.synClear(); synCache.fill(0); synArm = 0; engine.synArm(0);
      } else if (S.seq_view && S.eng_focus === MENU.DRUM) engine.clearPattern(1, T.sel_drum & 3);
      else if (S.seq_view && S.eng_focus === MENU.SMP) engine.clearPattern(2, T.sel_smp & 15);
      markDirty();
    } else if (c1) {
      if (S.param_edit) S.param_edit = false;
      else if (S.seq_view) { S.seq_view = false; S.seq_place = false; }
    }

    const m = S.menu, engPage = m <= MENU.SMP;
    if ((m === MENU.DRUM || m === MENU.SMP) && S.seq_view) {
      acc2 += d2;
      if (acc2 >= 4 || acc2 <= -4) {
        const dir = acc2 > 0 ? 1 : -1;
        if (m === MENU.DRUM) { T.sel_drum = (T.sel_drum + 4 + dir) % 4; engine.selectDrum(T.sel_drum); }
        else { T.sel_smp = (T.sel_smp + 16 + dir) & 15; engine.selectSlot(T.sel_smp); }
        markDirty(); acc2 = 0;
      }
      if (c2) S.seq_place = !S.seq_place;
    } else if (m === MENU.SYNTH && S.seq_view) {
      acc2 += d2;
      if (acc2 >= 4 || acc2 <= -4) {
        const dir = acc2 > 0 ? 1 : -1, sl = T.songLen;
        S.seq_edit = (S.seq_edit + sl + dir) % sl; acc2 = 0;
      }
      if (c2 && !h2) {
        const sl = T.songLen;
        engine.synArm(synArm); engine.synPlace(S.seq_edit);
        synCache[S.seq_edit % 128] = synArm;
        S.seq_edit = (S.seq_edit + 1) % sl; synArm = 0; engine.synArm(0); markDirty();
      }
    } else if (engPage) {
      const nmodes = m === MENU.SYNTH ? 3 : 1, nparam = ENG_PARAMS[m].length, nit = nmodes + nparam + 1, tapIdx = nit - 1;
      acc2 += d2;
      const n2 = acc2 >= 4 ? 1 : acc2 <= -4 ? -1 : 0;
      if (n2) {
        acc2 = 0;
        if (S.param_edit && S.mode_cur >= nmodes && S.mode_cur < tapIdx) { engine.adjust(ENG_PARAMS[m][S.mode_cur - nmodes], n2); markDirty(); }
        else S.mode_cur = (S.mode_cur + nit + n2) % nit;
      }
      if (c2 && S.mode_cur < nmodes) {
        if (m === MENU.SYNTH) {
          if (T.params.syn_mode !== S.mode_cur) { T.params.syn_mode = S.mode_cur; engine.synMode(S.mode_cur); markDirty(); }
          else if (S.mode_cur === SYN_SEQ) { S.seq_view = true; S.seq_place = false; }
        } else { S.seq_view = true; S.seq_place = false; }
      } else if (c2 && S.mode_cur >= nmodes && S.mode_cur < tapIdx) S.param_edit = !S.param_edit;
      else if (c2) { engine.tap(); markDirty(); }
    } else if (m === MENU.FX) {
      acc2 += d2;
      if (acc2 >= 4 || acc2 <= -4) {
        setFxBank((S.fx_bank + 8 + (acc2 > 0 ? 1 : -1)) % 8);
        clearFxHeld(); acc2 = 0;
      }
    } else if (m === MENU.GLOB) {
      acc2 += d2;
      const n2 = acc2 >= 4 ? 1 : acc2 <= -4 ? -1 : 0;
      if (n2) {
        acc2 = 0;
        if (S.param_edit) { globAdj(S.mode_cur, n2); markDirty(); }
        else S.mode_cur = (S.mode_cur + 4 + n2) % 4;
      }
      if (c2) S.param_edit = !S.param_edit;
    } else acc2 = 0;

    syncHoldFx(n);
  }

  // the engine owns every range and step (BPM 30..250 included); the OLED reads the result from the next tick
  function globAdj(idx, d) {
    engine.adjust(['bpm', 'key', 'mode', 'swing'][idx], d);
  }

  // right encoder held: FX automation record, synth live write. Debounced so a drag-to-rotate never
  // counts as a hold.
  function syncHoldFx(n) {
    const h1 = heldE(0), h2 = heldE(1), matured = h2 && n - E[1].pressAt >= HOLD_FX_MS;
    const rec = matured && !h1 && S.menu === MENU.FX && !S.vol_mode;
    const live = matured && S.menu === MENU.SYNTH && S.seq_view && T.params.syn_mode === SYN_SEQ && !S.vol_mode;
    if (rec !== recFxOn) { recFxOn = rec; engine.fxRecord(rec); if (!rec) markDirty(); }
    if (live !== liveWriteOn) { liveWriteOn = live; engine.synLiveWrite(live); if (!live) markDirty(); }
  }

  function watchPatches(n) {
    let warn = -1;
    if (S.menu === MENU.SESS) {
      for (let k = 0; k < 16; k++) {
        if (sessPress[k] < 0) continue;
        const held = n - sessPress[k], ex = (exists >> k) & 1;
        if (!ex && held > 3000) { sessPress[k] = -1; sessionNew(k); }
        else if (ex && k !== 0) {
          if (held > 5000 && held < 10000) warn = k;
          if (held >= 10000) { sessPress[k] = -1; sessionDelete(k); }
        }
      }
    } else for (let k = 0; k < 16; k++) sessPress[k] = -1;
    sessWarn = warn;
  }

  // ------------------------------------------------------------------ rendering
  const animStep = (n) => {
    if (animStart < 0) return 0;
    const s = Math.floor((n - animStart) / OLED_MS) + 1;
    return s <= 6 ? s : 0;
  };
  const saverOn = (n) => phase === 'run' && n - S.last_enc > 25000 && !S.vol_mode && animStep(n) === 0;

  function buildView(n) {
    const as = animStep(n);
    const vw = {
      T, menu: S.menu, mode_cur: S.mode_cur, param_edit: S.param_edit, seq_view: S.seq_view, seq_place: S.seq_place,
      vol_mode: S.vol_mode, vol_pidx: S.vol_pidx, eng_focus: S.eng_focus, fx_bank: S.fx_bank,
      animStep: as, animDir, saver: saverOn(n), sv, sensors, rand, spin: Math.floor(n / OLED_MS),
      saveBusy: saveBusy || saveFailed, saveFail: saveFailed, e1s: n < e1.until ? e1.s : '--', e2s: n < e2.until ? e2.s : '--',
      fsr: fsrRaw, sdOk: !saveFailed, held_mask: heldMask,
      recBadge: heldE(1) && !heldE(0) && S.menu === MENU.FX && !S.vol_mode,
      syn: { arm: synArm, cache: synCache, seq_edit: S.seq_edit },
      sess: { exists, cur, warn: sessWarn },
      phases: null, splash: null, error: startErr, bootFrame: -1,
    };
    if (phase === 'idle') vw.splash = 'TAP TO START';
    else if (phase === 'starting') vw.splash = 'STARTING...';
    else if (phase === 'boot') vw.bootFrame = Math.min(7, Math.floor((n - bootStart) / 30));
    const step = phase === 'run' || phase === 'boot' ? Math.floor((n - t0) / LED_MS) : 0;
    vw.phases = new Array(16);
    for (let i = 0; i < 16; i++) vw.phases[i] = phaseAt(i, step);
    ledStep = step;
    return vw;
  }

  function sameFb(a, b) { for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false; return true; }
  function sameLed(a, b) {
    for (let i = 0; i < 16; i++) { const x = a[i], y = b[i]; if (x[0] !== y[0] || x[1] !== y[1] || x[2] !== y[2]) return false; }
    return true;
  }

  function paintStatic() {
    if (!L) return;
    ctx.fillStyle = C.page; ctx.fillRect(0, 0, L.W, L.H);
    ctx.fillStyle = C.body; rr(ctx, L.body.x, L.body.y, L.body.w, L.body.h, L.body.r); ctx.fill();
    ctx.strokeStyle = C.edge; ctx.lineWidth = Math.max(1, L.ts); ctx.stroke();
    const o = L.oledBlock;
    ctx.fillStyle = C.glass; rr(ctx, o.x, o.y, o.w, o.h, Math.round(o.w * 0.025)); ctx.fill();
    ctx.strokeStyle = C.edge; ctx.stroke();
    fbPainted = null; ledPainted = null; encDirty = true; stripDirty = true; ledDirty = true; padTouchPainted = -1;
  }

  function paintOled(fb) {
    const g = L.glass, s = L.scale;
    ctx.fillStyle = C.glass; ctx.fillRect(g.x, g.y, g.w, g.h);
    ctx.fillStyle = C.lit;
    for (let y = 0; y < OH; y++) {
      const o = y * OW;
      for (let x = 0; x < OW;) {
        if (!fb[o + x]) { x++; continue; }
        let k = x; while (k < OW && fb[o + k]) k++;
        ctx.fillRect(g.x + x * s, g.y + y * s, (k - x) * s, s);
        x = k;
      }
    }
  }

  function paintPads(leds) {
    ctx.fillStyle = C.body; ctx.fillRect(L.padsRect.x - L.gp, L.padsRect.y - L.gp, L.padsRect.w + 2 * L.gp, L.padsRect.h + 2 * L.gp);
    for (let k = 0; k < 16; k++) {
      const p = L.pads[k], c = leds[k];
      const r = screenChannel(c[0]), g = screenChannel(c[1]), b = screenChannel(c[2]);
      ctx.fillStyle = (r | g | b) ? 'rgb(' + r + ',' + g + ',' + b + ')' : C.padOff;
      rr(ctx, p.x, p.y, p.w, p.h, p.w * 0.18); ctx.fill();
      if ((padTouch >> k) & 1) { ctx.strokeStyle = 'rgba(255,255,255,0.55)'; ctx.lineWidth = Math.max(2, L.ts * 2); rr(ctx, p.x + 1, p.y + 1, p.w - 2, p.h - 2, p.w * 0.18); ctx.stroke(); }
    }
  }

  function paintEnc() {
    for (let i = 0; i < 2; i++) {
      const e = L.enc[i], st = E[i];
      ctx.fillStyle = C.body; ctx.fillRect(e.cx - e.r - 3, e.cy - e.r - 3, 2 * e.r + 6, 2 * e.r + 6);
      ctx.fillStyle = C.encBody; ctx.beginPath(); ctx.arc(e.cx, e.cy, e.r, 0, 6.2832); ctx.fill();
      ctx.strokeStyle = C.encRing; ctx.lineWidth = Math.max(2, L.ts * 2); ctx.stroke();
      const a0 = (st.counts * 3.75 * Math.PI) / 180;
      // 24 detent ticks that turn with the knob
      ctx.strokeStyle = C.edge; ctx.lineWidth = Math.max(1, L.ts);
      ctx.beginPath();
      for (let t = 0; t < 24; t++) {
        const a = a0 + (t * Math.PI * 2) / 24, c = Math.cos(a), s = Math.sin(a);
        ctx.moveTo(e.cx + c * e.r * 0.8, e.cy + s * e.r * 0.8); ctx.lineTo(e.cx + c * e.r * 0.92, e.cy + s * e.r * 0.92);
      }
      ctx.stroke();
      ctx.fillStyle = st.touch && !st.drag ? C.encInOn : C.encIn; ctx.beginPath(); ctx.arc(e.cx, e.cy, e.r * 0.56, 0, 6.2832); ctx.fill();
      const ma = a0 - Math.PI / 2;
      ctx.strokeStyle = C.mark; ctx.lineWidth = Math.max(3, L.ts * 3); ctx.lineCap = 'round';
      ctx.beginPath(); ctx.moveTo(e.cx + Math.cos(ma) * e.r * 0.22, e.cy + Math.sin(ma) * e.r * 0.22); ctx.lineTo(e.cx + Math.cos(ma) * e.r * 0.74, e.cy + Math.sin(ma) * e.r * 0.74); ctx.stroke();
      ctx.lineCap = 'butt';
    }
  }

  function paintStrip() {
    const s = L.strip;
    ctx.fillStyle = C.body; ctx.fillRect(s.x, s.y, s.w, s.h);
    if (S.menu === MENU.DASH && phase === 'run') {
      const help = 'F=FSR LEAN  L=LIDAR  T=TILT  DRAG';
      pixText(ctx, help, s.x + Math.round((s.w - help.length * 6 * L.ts) / 2), s.y + L.ts * 3, L.ts, C.help);
      const names = ['F', 'L', 'T'], vals = [slid.lean, slid.lidar, slid.tilt];
      for (let i = 0; i < 3; i++) {
        const q = L.sliders[i], ty = q.y + q.h / 2, th = Math.max(3, L.ts * 3);
        pixText(ctx, names[i], q.x + L.ts * 2, ty - 4 * L.ts, L.ts, C.help);
        ctx.fillStyle = C.track; rr(ctx, q.tx, ty - th / 2, q.tw, th, th / 2); ctx.fill();
        ctx.fillStyle = C.thumb; ctx.beginPath(); ctx.arc(q.tx + q.tw * vals[i], ty, th * 2, 0, 6.2832); ctx.fill();
      }
    } else {
      const ws = L.ts * 2, w = 'PMOR', tw = w.length * 6 * ws - ws;
      pixText(ctx, w, s.x + Math.round((s.w - tw) / 2), s.y + Math.round((s.h - 8 * ws) / 2), ws, C.word);
    }
  }

  function renderFrame(n) {
    if (!L || hidden) return;
    lastFrame = n; frameDue = Infinity;
    const vw = buildView(n);
    const dashNow = S.menu === MENU.DASH && phase === 'run';
    if (dashNow !== stripWasDash) { stripWasDash = dashNow; stripDirty = true; }
    renderOled(fbNew, vw);
    if (!fbPainted || !sameFb(fbNew, fbPainted)) {
      paintOled(fbNew);
      const t = fbPainted || fbCur; fbPainted = fbNew; fbNew = t;
    }
    const leds = ledFrame(vw);
    if (ledDirty || !ledPainted || !sameLed(leds, ledPainted) || padTouch !== padTouchPainted) {
      paintPads(leds); ledPainted = leds; ledDirty = false; padTouchPainted = padTouch;
    }
    if (encDirty) { paintEnc(); encDirty = false; }
    if (stripDirty) { paintStrip(); stripDirty = false; }
  }

  function requestFrame(urgent) {
    if (hidden) return;
    const n = now();
    if (urgent || n - lastFrame >= OLED_MS - 6) renderFrame(n);
    else frameDue = Math.min(frameDue, lastFrame + OLED_MS);
  }

  // ------------------------------------------------------------------ scheduling (no timer unless something is pending)
  function needLoop(n) { return phase === 'boot' || (phase === 'run' && (animStep(n) > 0 || saverOn(n))); }

  function nextWake(n) {
    let w = Infinity;
    const upd = (t) => { if (t < w) w = t; };
    if (c1Pend >= 0 && heldE(0) && !c1Cleared) upd(c1Pend + 700);
    if (c2Pend >= 0) upd(c2Pend + 130);
    if (heldE(1) && n - E[1].pressAt < HOLD_FX_MS) upd(E[1].pressAt + HOLD_FX_MS);
    if (saveAt >= 0) upd(saveAt);
    if (S.menu === MENU.SESS) {
      for (let k = 0; k < 16; k++) {
        if (sessPress[k] < 0) continue;
        if (!((exists >> k) & 1)) upd(sessPress[k] + 3001);
        else if (k !== 0) { upd(sessPress[k] + 5001); upd(sessPress[k] + 10000); }
      }
    }
    if (needLoop(n)) upd(phase === 'boot' ? lastFrame + 30 : lastFrame + OLED_MS);
    if (frameDue !== Infinity) upd(frameDue);
    return w;
  }

  function schedule() {
    if (hidden) { if (timerId !== null) { timers.clear(timerId); timerId = null; timerAt = Infinity; } return; }
    const n = now(), w = nextWake(n);
    if (w === Infinity) { if (timerId !== null) { timers.clear(timerId); timerId = null; timerAt = Infinity; } return; }
    if (timerId !== null && Math.abs(timerAt - w) < 1) return;
    if (timerId !== null) timers.clear(timerId);
    timerAt = w;
    timerId = timers.set(onWake, Math.max(0, w - n));
  }

  function onWake() { timerId = null; timerAt = Infinity; update(); }

  function update() {
    const n = now();
    if (phase === 'boot' && n - bootStart >= 240) { phase = 'run'; S.last_enc = n; ledDirty = true; stripDirty = true; }
    if (phase === 'run') {
      processEncoders(n);
      watchPatches(n);
      if (saveAt >= 0 && n >= saveAt) doSave();
    }
    if (animStart >= 0 && animStep(n) === 0) { animStart = -1; renderFrame(n); }
    if ((frameDue !== Infinity && n >= frameDue) || (needLoop(n) && n - lastFrame >= (phase === 'boot' ? 29 : OLED_MS - 1))) renderFrame(n);
    else if (phase === 'run' && sessWarn !== lastWarn) renderFrame(n);
    lastWarn = sessWarn;
    schedule();
  }

  // ------------------------------------------------------------------ ticks
  function onTick(t) {
    const n = now();
    T = t; lastTickAt = n;
    if (n >= fxBankHold && t.fx && S.fx_bank !== (t.fx.bank & 7)) S.fx_bank = t.fx.bank & 7;
    if (S.seq_edit >= t.songLen) S.seq_edit = 0;
    // the engine drops a bank-0 hold after 20 s: take its word for bits the UI believes are held. Ignored for
    // 150 ms after our own fxHeld so a tick that was already in flight cannot undo a fresh press.
    if (t.fx && t.fx.held && n - fxSentAt > 150) {
      for (let b = 0; b < 8; b++) fxHeld[b] &= (t.fx.held[b] | 0);
    }
    // t.dirty counts autosave-worthy engine changes (tap tempo, FX automation, live write...)
    if (typeof t.dirty === 'number') {
      if (phase === 'run' && t.dirty !== lastDirty && saveAt < 0) { saveAt = n + 3000; saveBusy = true; }
      lastDirty = t.dirty;
    }
    if (t.syn_seq_page) { const base = (t.gstep >> 4) * 16; for (let i = 0; i < 16; i++) if (base + i < 128) synCache[base + i] = t.syn_seq_page[i]; }
    if (!S.vol_mode && S.menu !== MENU.FX && S.menu !== MENU.SESS && S.seq_view) S.view_page = t.gstep >> 4;
    if (phase !== 'run' && phase !== 'boot') return;
    if (phase === 'run') { processEncoders(n); watchPatches(n); }
    requestFrame(false);
    schedule();
  }

  // ------------------------------------------------------------------ pointers
  function inCircle(e, x, y, k) { const dx = x - e.cx, dy = y - e.cy; return dx * dx + dy * dy <= (e.r * k) * (e.r * k); }

  function hit(x, y) { // css px -> device px
    x *= L.dpr; y *= L.dpr;
    if (S.menu === MENU.DASH) for (let i = 0; i < 3; i++) { const q = L.sliders[i]; if (x >= q.x && x < q.x + q.w && y >= q.y - L.ts * 4 && y < q.y + q.h) return { kind: 'slider', i, x, y }; }
    for (let i = 0; i < 2; i++) if (inCircle(L.enc[i], x, y, 1.12)) return { kind: 'enc', i, x, y };
    const hp = L.gp / 2;
    for (let k = 0; k < 16; k++) { const p = L.pads[k]; if (x >= p.x - hp && x < p.x + p.w + hp && y >= p.y - hp && y < p.y + p.h + hp) return { kind: 'pad', k, x, y }; }
    return null;
  }

  function pressureOf(info) {
    if (info && info.pressure > 0 && info.pressure !== 0.5) return { real: true, v: clamp(info.pressure, 0, 1) };
    const sz = info ? Math.sqrt((info.width || 0) * (info.height || 0)) : 0;
    if (sz > 3) return { real: true, v: clamp((sz - 14) / 26, 0, 1) };
    return { real: false, v: 0 };
  }

  function setSlider(i, x) {
    const q = L.sliders[i], t = clamp((x - q.tx) / q.tw, 0, 1);
    if (i === 0) slid.lean = t;
    else if (i === 1) { slid.lidar = t; updateSensors(); engine.set('lid', sensors.dist); }
    else { slid.tilt = t; updateSensors(); sendAx(); }
    if (i === 0) sendFsr(false);
    stripDirty = true;
  }

  function pointerDown(id, x, y, info) {
    if (!L || phase !== 'run') return;
    if (needResume) { needResume = false; engSafe(() => engine.resume()); }
    const h = hit(x, y);
    S.last_enc = now();
    if (!h) return;
    const n = now();
    if (h.kind === 'pad') {
      if ((heldMask >> h.k) & 1) return;
      ptrs.set(id, { kind: 'pad', k: h.k });
      padPress[h.k] = pressureOf(info);
      padTouch |= 1 << h.k;
      padDown(h.k, info);
    } else if (h.kind === 'enc') {
      const st = E[h.i];
      if (st.touch) return;
      const e = L.enc[h.i];
      st.touch = true; st.drag = false; st.pressAt = n; st.pid = id; st.sx = st.lx = h.x; st.sy = st.ly = h.y; st.px = 0;
      st.circ = !inCircle(e, h.x, h.y, 0.56);
      st.ang = Math.atan2(h.y - e.cy, h.x - e.cx);
      st.clicked = true;
      ptrs.set(id, { kind: 'enc', i: h.i });
      encDirty = true;
    } else {
      ptrs.set(id, { kind: 'slider', i: h.i });
      setSlider(h.i, h.x);
    }
    processEncoders(n);
    renderFrame(n);
    schedule();
  }

  function pointerMove(id, x, y, info) {
    const p = ptrs.get(id);
    if (!p || !L) return;
    x *= L.dpr; y *= L.dpr;
    const n = now();
    if (p.kind === 'pad') {
      const r = L.pads[p.k], hp = L.gp / 2;
      if (x < r.x - hp || x >= r.x + r.w + hp || y < r.y - hp || y >= r.y + r.h + hp) {
        ptrs.delete(id); padTouch &= ~(1 << p.k); padUp(p.k);
        renderFrame(n); schedule();
      } else if (info) {
        const pr = pressureOf(info);
        if (pr.real) { padPress[p.k] = pr; sendFsr(false); }
      }
    } else if (p.kind === 'enc') {
      const st = E[p.i], e = L.enc[p.i];
      const css = 1 / L.dpr;
      if (!st.drag) {
        const dd = Math.hypot(x - st.sx, y - st.sy) * css;
        if (dd < 8) return;
        st.drag = true;
        if (p.i === 0) { c1Pend = -1; c1Cleared = false; } else c2Pend = -1;
        st.lx = x; st.ly = y; st.ang = Math.atan2(y - e.cy, x - e.cx); st.px = 0;
        encDirty = true;
      }
      let counts = 0;
      if (st.circ) {
        const a = Math.atan2(y - e.cy, x - e.cx);
        let da = a - st.ang;
        if (da > Math.PI) da -= 2 * Math.PI; else if (da < -Math.PI) da += 2 * Math.PI;
        st.ang = a;
        st.px += (da * 180) / Math.PI / 3.75;
        counts = Math.trunc(st.px); st.px -= counts;
      } else {
        st.px += ((st.ly - y) * css) / 4.5;
        counts = Math.trunc(st.px); st.px -= counts;
      }
      st.lx = x; st.ly = y;
      if (counts) {
        st.delta += counts; st.counts += counts; encDirty = true;
        if (typeof navigator !== 'undefined' && navigator.vibrate && Math.abs(counts) >= 1) engSafe(() => navigator.vibrate(1));
        processEncoders(n); renderFrame(n); schedule();
      }
    } else if (p.kind === 'slider') setSlider(p.i, x), renderFrame(n);
  }

  function pointerUp(id) {
    const p = ptrs.get(id);
    if (!p) return;
    ptrs.delete(id);
    const n = now();
    if (p.kind === 'pad') { padTouch &= ~(1 << p.k); padUp(p.k); }
    else if (p.kind === 'enc') { const st = E[p.i]; st.touch = false; st.drag = false; st.pid = -1; encDirty = true; processEncoders(n); }
    watchPatches(n);
    // ticks stop when iOS interrupts the context (call, alarm); a release is a gesture that may resume it
    if (phase === 'run' && lastTickAt && n - lastTickAt > 400) engSafe(() => engine.resume());
    renderFrame(n);
    schedule();
  }

  function releaseAll() {
    for (const id of [...ptrs.keys()]) pointerUp(id);
    clearFxHeld();
    if (noteHeld) for (let k = 0; k < 16; k++) if ((noteHeld >> k) & 1) { noteHeld &= ~(1 << k); engine.noteOff(k); }
  }

  // ------------------------------------------------------------------ lifecycle
  function resize(cssW, cssH, dpr) {
    L = computeLayout(cssW, cssH, dpr || 1);
    L.cssW = cssW; L.dpr = dpr || 1;
    paintStatic();
    if (hidden) return;
    renderFrame(now());
  }

  function tapStart() {
    if (phase !== 'idle') return Promise.resolve();
    phase = 'starting'; startErr = '';
    let p;
    try { p = engine.start(); } catch (e) { p = Promise.reject(e); } // must run inside the gesture (iOS)
    renderFrame(now());
    return Promise.resolve(p).then(() => boot()).then(() => {
      phase = 'boot'; bootStart = now(); ledDirty = true;
      renderFrame(bootStart); schedule();
    }).catch(() => {
      phase = 'idle'; startErr = 'AUDIO FAILED - TAP';
      renderFrame(now()); schedule();
    });
  }

  function setHidden(h) {
    if (h === hidden) return;
    hidden = h;
    if (h) {
      releaseAll();
      if (saveAt >= 0 && phase === 'run') doSave();
      engSafe(() => engine.suspend());
      needResume = true;
      schedule();
    } else {
      needResume = false;
      engSafe(() => engine.resume());
      if (L) { paintStatic(); renderFrame(now()); }
      schedule();
    }
  }

  engine.onTick = onTick;
  updateSensors();

  return {
    S, onTick, resize, tapStart, update, setHidden, pointerDown, pointerMove, pointerUp,
    pointerCancel: pointerUp, releaseAll,
    flush: () => opChain,
    phase: () => phase,
    layout: () => L,
    view: () => buildView(now()),
    leds: () => ledFrame(buildView(now())),
    state: () => ({ phase, exists, cur, slots, synArm, heldMask, fsrRaw, saveBusy, saveFailed, sessWarn, slid, vol_mode: S.vol_mode, animStart, recFxOn, liveWriteOn, timerPending: timerId !== null }),
    tick: () => T,
  };
}

// ---------------------------------------------------------------------------------------------
// Browser glue (only when a DOM exists)

export function attachDom(ui, canvas, win, doc) {
  const fit = () => {
    const dpr = win.devicePixelRatio || 1, r = canvas.getBoundingClientRect();
    canvas.width = Math.max(1, Math.round(r.width * dpr)); canvas.height = Math.max(1, Math.round(r.height * dpr));
    ui.resize(r.width, r.height, dpr);
  };
  const xy = (e) => { const r = canvas.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; };
  const info = (e) => ({ pressure: e.pressure, width: e.width, height: e.height, type: e.pointerType });
  canvas.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    try { canvas.setPointerCapture(e.pointerId); } catch (err) { /* synthetic pointer */ }
    const [x, y] = xy(e); ui.pointerDown(e.pointerId, x, y, info(e));
  }, { passive: false });
  canvas.addEventListener('pointermove', (e) => { e.preventDefault(); const [x, y] = xy(e); ui.pointerMove(e.pointerId, x, y, info(e)); }, { passive: false });
  // iOS only counts pointerup/touchend as an audio-unlocking gesture, so start on release
  canvas.addEventListener('pointerup', (e) => { e.preventDefault(); ui.pointerUp(e.pointerId); if (ui.phase() === 'idle') ui.tapStart(); }, { passive: false });
  canvas.addEventListener('pointercancel', (e) => ui.pointerCancel(e.pointerId));
  // iOS Safari: touchend and click are the long-established audio-unlocking gestures; pointerup alone is not relied on
  const kick = () => { if (ui.phase() === 'idle') ui.tapStart(); };
  canvas.addEventListener('touchend', kick, { passive: true });
  canvas.addEventListener('click', kick);
  for (const ev of ['contextmenu', 'dblclick', 'gesturestart', 'selectstart']) doc.addEventListener(ev, (e) => e.preventDefault());
  doc.addEventListener('touchmove', (e) => e.preventDefault(), { passive: false });
  doc.addEventListener('visibilitychange', () => ui.setHidden(doc.hidden));
  win.addEventListener('pagehide', () => ui.setHidden(true));
  win.addEventListener('pageshow', () => ui.setHidden(doc.hidden));
  win.addEventListener('resize', fit);
  win.addEventListener('orientationchange', fit);
  if (win.ResizeObserver) new win.ResizeObserver(fit).observe(canvas);
  fit();
}

// ---------------------------------------------------------------------------------------------
// Patch storage backends. Interface: async get(key) -> value|null, set(key, value), delete(key).

export function idbBackend(idb, name = DB_NAME) {
  return new Promise((resolve, reject) => {
    let req;
    try { req = idb.open(name, 1); } catch (e) { reject(e); return; }
    req.onupgradeneeded = () => { try { req.result.createObjectStore(DB_STORE); } catch (e) { /* exists */ } };
    req.onerror = () => reject(req.error || new Error('indexedDB open failed'));
    req.onblocked = () => reject(new Error('indexedDB blocked'));
    req.onsuccess = () => {
      const db = req.result;
      const run = (mode, fn) => new Promise((res, rej) => {
        let tx, out;
        try { tx = db.transaction(DB_STORE, mode); out = fn(tx.objectStore(DB_STORE)); } catch (e) { rej(e); return; }
        tx.oncomplete = () => res(out && out.result !== undefined ? out.result : null);
        tx.onerror = () => rej(tx.error || (out && out.error) || new Error('indexedDB transaction failed'));
        tx.onabort = () => rej(tx.error || new Error('indexedDB transaction aborted'));
      });
      resolve({
        get: (k) => run('readonly', (st) => st.get(k)),
        set: (k, v) => run('readwrite', (st) => st.put(v, k)).then(() => undefined),
        delete: (k) => run('readwrite', (st) => st.delete(k)).then(() => undefined),
      });
    };
  });
}

// Fallback when IndexedDB is unavailable: the small, non-audio part of each patch only.
export function lsBackend(ls) {
  const strip = (v) => (v && v.p && v.p.audio && Object.keys(v.p.audio).length ? Object.assign({}, v, { p: Object.assign({}, v.p, { audio: {} }) }) : v);
  return {
    get: async (k) => { const r = ls.getItem(LS_PREFIX + k); return r ? JSON.parse(r) : null; },
    set: async (k, v) => { ls.setItem(LS_PREFIX + k, JSON.stringify(strip(v))); },
    delete: async (k) => { ls.removeItem(LS_PREFIX + k); },
  };
}

// Resolves lazily so the UI can be built synchronously: IndexedDB first, localStorage if it will not open.
export function openStorage(idb, ls) {
  const ready = (idb ? idbBackend(idb) : Promise.reject(new Error('no indexedDB')))
    .catch(() => (ls ? lsBackend(ls) : { get: async () => null, set: async () => { throw new Error('no storage'); }, delete: async () => {} }));
  return { get: (k) => ready.then((b) => b.get(k)), set: (k, v) => ready.then((b) => b.set(k, v)), delete: (k) => ready.then((b) => b.delete(k)) };
}

if (typeof document !== 'undefined' && typeof window !== 'undefined') {
  const canvas = document.getElementById('pmor');
  let idb = null, ls = null;
  try { idb = window.indexedDB || null; } catch (e) { /* blocked */ }
  try { ls = window.localStorage; } catch (e) { /* blocked */ }
  const storage = openStorage(idb, ls);
  const stubEngine = { start: () => Promise.reject(new Error('engine')), set() {}, onTick: null };
  const boot = (engine) => {
    const ui = createPmorUI({ canvas, engine, storage, now: () => performance.now() });
    attachDom(ui, canvas, window, document);
  };
  if (canvas) import('./engine.js').then((m) => boot(new m.Engine()), () => boot(stubEngine));
}

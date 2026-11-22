"""Single-cycle wavetable editor state for the NEPTR Phase 4 UI WAVE screen.

Pure logic + DSP, no pygame — the UI file owns all rendering and touch
routing. Testable headless.

Model
-----
* `wave`      : N=2048 float samples in [-1, 1], one cycle, phase 0..1.
* `harmonics` : NH=64 partials (k = 1..64) as (a_k, b_k) cos/sin
                coefficients; magnitude = hypot(a, b), phase = atan2(-b, a)
                per the usual  a*cos(wt) + b*sin(wt) = m*cos(wt + ph)  form.

Both views stay consistent:
  - finger draws the WAVE  -> analyze() refits the 64 partials (DFT).
  - finger sculpts a BAR   -> the wave is resynthesized from the partials.
    The first bar edit after a draw makes the 64-partial resynthesis the
    truth (content above harmonic 64 is dropped at that moment — documented
    behavior, inaudible for hand-drawn cycles at this resolution). After
    that, single-bar edits update the wave INCREMENTALLY (one basis table
    add per change), so bar drags stay cheap on the Pi.

DFT choice
----------
numpy's rfft when importable (the Pi kivy-venv is probed at import time by
the UI); otherwise a small pure-python DFT over the 64 needed bins using one
cached cos table of length N and index arithmetic ((k*n) % N) — no per-sample
trig calls. 2048x64 ~ 131k multiply-adds per direction: tens of ms in CPython
on the Pi, and it only runs on stroke commit, never per frame.

OSC payload helpers live here too (blob = big-endian float32, the natural
OSC byte order), so the encoding is unit-testable without a socket.
"""

import math
import struct

try:
    import numpy as _np
except ImportError:          # pure-python fallback (fine at this size)
    _np = None

N = 2048          # samples per cycle in the published table
NH = 64           # partials kept/displayed/editable

TWO_PI = 2.0 * math.pi


def _clamp(v, lo=-1.0, hi=1.0):
    return lo if v < lo else (hi if v > hi else v)


class WaveState:
    def __init__(self):
        # start with a plain sine so the screen never opens on dead silence
        if _np is not None:
            self.wave = list(_np.sin(_np.arange(N) * (TWO_PI / N)))
        else:
            self.wave = [math.sin(TWO_PI * n / N) for n in range(N)]
        self._cos = None            # cached cos table for the pure-python DFT
        self._basis = {}            # k -> (cos_k list, sin_k list), lazy
        self._undo = None           # one-step undo snapshot (wave, a, b)
        self._spectral_truth = False  # True once a bar edit resynthesized
        self.a = [0.0] * (NH + 1)   # index 1..NH used
        self.b = [0.0] * (NH + 1)
        self.rev = 0                # bumped on every visible change
        self.analyze()
        self._undo = None           # initial analyze is not undoable

    # ------------------------------------------------------------- undo

    def _snapshot(self):
        self._undo = (list(self.wave), list(self.a), list(self.b),
                      self._spectral_truth)

    def undo(self):
        """One-step undo (swaps, so undo of undo redoes). True if applied."""
        if self._undo is None:
            return False
        cur = (list(self.wave), list(self.a), list(self.b),
               self._spectral_truth)
        self.wave, self.a, self.b, self._spectral_truth = \
            (list(self._undo[0]), list(self._undo[1]), list(self._undo[2]),
             self._undo[3])
        self._undo = cur
        self.rev += 1
        return True

    def clear(self):
        self._snapshot()
        self.wave = [0.0] * N
        self.a = [0.0] * (NH + 1)
        self.b = [0.0] * (NH + 1)
        self._spectral_truth = False
        self.rev += 1

    # ------------------------------------------------------- wave drawing

    def begin_stroke(self):
        self._snapshot()
        self._last_pt = None

    def add_point(self, phase, amp):
        """Feed one touch sample: phase 0..1 across the cycle, amp -1..1.
        Gaps from fast strokes are linearly interpolated from the previous
        sample of the same stroke."""
        idx = max(0, min(N - 1, int(phase * N)))
        amp = _clamp(amp)
        if self._last_pt is None:
            self.wave[idx] = amp
        else:
            i0, a0 = self._last_pt
            if idx == i0:
                self.wave[idx] = amp
            else:
                step = 1 if idx > i0 else -1
                span = idx - i0
                for i in range(i0 + step, idx + step, step):
                    t = (i - i0) / span
                    self.wave[i] = _clamp(a0 + (amp - a0) * t)
        self._last_pt = (idx, amp)
        self._spectral_truth = False
        self.rev += 1

    def end_stroke(self):
        """Commit a draw stroke: refit the partials to the drawn wave."""
        self._last_pt = None
        self.analyze()
        self.rev += 1

    # ----------------------------------------------------------- analysis

    def _cos_table(self):
        if self._cos is None:
            self._cos = [math.cos(TWO_PI * n / N) for n in range(N)]
        return self._cos

    def analyze(self):
        """wave -> (a_k, b_k) for k = 1..NH. DC is deliberately dropped
        (the editor is a waveshape, not an offset)."""
        if _np is not None:
            spec = _np.fft.rfft(_np.asarray(self.wave))
            for k in range(1, NH + 1):
                self.a[k] = 2.0 * spec[k].real / N
                self.b[k] = -2.0 * spec[k].imag / N
        else:
            cos = self._cos_table()
            w = self.wave
            for k in range(1, NH + 1):
                sa = 0.0
                sb = 0.0
                q = (3 * N) // 4          # cos(x - pi/2) = sin(x)
                for n in range(N):
                    i = (k * n) % N
                    sa += w[n] * cos[i]
                    sb += w[n] * cos[(i + q) % N]
                self.a[k] = 2.0 * sa / N
                self.b[k] = 2.0 * sb / N

    # -------------------------------------------------------- resynthesis

    def _basis_k(self, k):
        """(cos_k, sin_k) length-N tables for partial k, cached lazily."""
        t = self._basis.get(k)
        if t is None:
            if _np is not None:
                ph = _np.arange(N) * (TWO_PI * k / N)
                t = (_np.cos(ph), _np.sin(ph))
            else:
                cos = self._cos_table()
                q = (3 * N) // 4
                ck = [cos[(k * n) % N] for n in range(N)]
                sk = [cos[((k * n) + q) % N] for n in range(N)]
                t = (ck, sk)
            self._basis[k] = t
        return t

    def resynthesize(self):
        """wave = full sum of the NH partials (drops >NH residual)."""
        if _np is not None:
            acc = _np.zeros(N)
            for k in range(1, NH + 1):
                if self.a[k] or self.b[k]:
                    ck, sk = self._basis_k(k)
                    acc = acc + self.a[k] * ck + self.b[k] * sk
            self.wave = [_clamp(v) for v in acc]
        else:
            acc = [0.0] * N
            for k in range(1, NH + 1):
                ak = self.a[k]
                bk = self.b[k]
                if ak == 0.0 and bk == 0.0:
                    continue
                ck, sk = self._basis_k(k)
                for n in range(N):
                    acc[n] += ak * ck[n] + bk * sk[n]
            self.wave = [_clamp(v) for v in acc]
        self._spectral_truth = True
        self.rev += 1

    # ------------------------------------------------------ bar (spectrum) edits

    def begin_bar_stroke(self):
        self._snapshot()
        self._last_bar = None
        if not self._spectral_truth:
            # switch truth to the 64-partial model exactly once per stroke
            # sequence — from here on, single-bar changes are incremental.
            self.resynthesize()

    def set_magnitude(self, k, mag):
        """Set partial k's magnitude (phase kept; a zero partial becomes a
        pure sine term so it contributes from phase 0). Incremental wave
        update: one basis-table add."""
        if not (1 <= k <= NH):
            return
        mag = max(0.0, min(1.0, mag))
        old_a, old_b = self.a[k], self.b[k]
        m0 = math.hypot(old_a, old_b)
        if m0 < 1e-9:
            new_a, new_b = 0.0, mag          # fresh partial: sine phase
        else:
            s = mag / m0
            new_a, new_b = old_a * s, old_b * s
        da, db = new_a - old_a, new_b - old_b
        if da == 0.0 and db == 0.0:
            return
        self.a[k], self.b[k] = new_a, new_b
        ck, sk = self._basis_k(k)
        if _np is not None:
            w = _np.asarray(self.wave) + da * ck + db * sk
            self.wave = [_clamp(v) for v in w]
        else:
            w = self.wave
            for n in range(N):
                w[n] = _clamp(w[n] + da * ck[n] + db * sk[n])
        self.rev += 1

    def stroke_magnitude(self, k, mag):
        """Feed one touch sample of a bar stroke: like set_magnitude, but a
        fast sweep leaves no gaps — bars skipped since the previous sample of
        the same stroke are filled by linear interpolation (the bar-strip
        counterpart of add_point's wave interpolation)."""
        if not (1 <= k <= NH):
            return
        mag = max(0.0, min(1.0, mag))
        last = getattr(self, "_last_bar", None)
        if last is not None and abs(k - last[0]) > 1:
            k0, m0 = last
            step = 1 if k > k0 else -1
            span = k - k0
            for i in range(k0 + step, k, step):
                self.set_magnitude(i, m0 + (mag - m0) * ((i - k0) / span))
        self.set_magnitude(k, mag)
        self._last_bar = (k, mag)

    def end_bar_stroke(self):
        self._last_bar = None
        self.rev += 1

    # ---------------------------------------------------------- accessors

    def magnitude(self, k):
        return math.hypot(self.a[k], self.b[k])

    def phase(self, k):
        return math.atan2(-self.b[k], self.a[k])

    def magnitudes(self):
        return [self.magnitude(k) for k in range(1, NH + 1)]

    def phases(self):
        return [self.phase(k) for k in range(1, NH + 1)]

    # -------------------------------------------------------- OSC payloads

    def table_blob(self):
        """The 2048-sample table as an OSC blob: big-endian float32
        (network byte order, matching OSC's own float encoding)."""
        return struct.pack(">%df" % N, *(_clamp(v) for v in self.wave))

    def harmonics_args(self):
        """[NH, mag1..magNH, phase1..phaseNH] for /neptr/wavetable/harmonics."""
        return [NH] + self.magnitudes() + self.phases()

    def chunk_args(self, size=16):
        """The table as [offset, s0..s15] float lists for the Csound-native
        /neptr/wavetable/chunk stream (Csound 6's OSClisten has no blob
        type, and 17 'f' args is comfortably within its limits — verified
        against csound 6.18). 128 messages + /neptr/wavetable/end."""
        w = [_clamp(v) for v in self.wave]
        return [[float(off)] + w[off:off + size]
                for off in range(0, N, size)]


# ---------------------------------------------------------------- log layout
# Harmonic bars on a log-spaced x axis: lower harmonics get more horizontal
# room ("more sensitivity toward lower harmonics"). Bar k (1-based) spans
# [edge(k), edge(k+1)) of the 0..1 strip with edge(k) = ln(k)/ln(NH+1).

_LN = math.log(NH + 1)


def bar_edges(k):
    """(x0, x1) fractions of the bar strip for harmonic k (1..NH)."""
    return math.log(k) / _LN, math.log(k + 1) / _LN


def bar_at(frac):
    """Inverse: which harmonic (1..NH) owns x-fraction `frac` of the strip."""
    frac = max(0.0, min(0.999999, frac))
    return max(1, min(NH, int(math.exp(frac * _LN))))

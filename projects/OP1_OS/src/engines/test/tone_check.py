import sys, wave, numpy as np
def load(p):
    w = wave.open(p); n = w.getnframes()
    return np.frombuffer(w.readframes(n), dtype="<i2").reshape(-1, 2)[:, 0].astype(float) / 32768, w.getframerate()
def f0_report(name, f0):
    x, sr = load(f"/tmp/pu_tone_{name}.wav"); x = x[sr // 2:]
    N = len(x); X = np.abs(np.fft.rfft(x * np.hanning(N))) ** 2; fr = np.fft.rfftfreq(N, 1 / sr)
    tot = X.sum(); harm = np.zeros_like(X, dtype=bool)
    k = 1
    while k * f0 < sr / 2:
        harm |= np.abs(fr - k * f0) < 3 * sr / N * 2; k += 1
    print(f"{name:6s} f0={f0:7.1f} rms={x.std():.3f} peak={abs(x).max():.2f} non-harmonic energy {10*np.log10(max(X[~harm].sum(),1e-20)/tot):6.1f} dB rel total")
f0 = lambda n: 440 * 2 ** ((n - 69) / 12)
for nm, n in (("saw84", 84), ("sq84", 84), ("tri84", 84), ("saw96", 96)): f0_report(nm, f0(n))
for nm in ("res", "res2"):
    x, sr = load(f"/tmp/pu_tone_{nm}.wav"); x = x[sr // 2:]
    X = np.abs(np.fft.rfft(x * np.hanning(len(x)))); fr = np.fft.rfftfreq(len(x), 1 / sr)
    top = np.argsort(X)[-3:][::-1]
    print(nm, "peak", abs(x).max(), "rms", x.std(), "top bins Hz", [round(fr[i]) for i in top], "HF(>10k)/total", (X[fr > 10000] ** 2).sum() / (X ** 2).sum())

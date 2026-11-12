#!/home/gorpi7v5/kivy-venv/bin/python
"""PRIME audio test: latency, tone quality, underrun artifacts via the 7/8 loopback.

Cable: HAT OUT7 -> IN7, OUT8 -> IN8  (PipeWire playback_AUX6/7 -> capture_AUX6/7).
Uses only PipeWire tools (pw-play, pw-record, pw-link, pw-top). Never touches
config, services or the HAT directly. Everything it links is removed on exit.

Paths measured (one short run each, <=10 s of audio, -20 dBFS):
  hw     stimulus -> playback_AUX6 -> cable -> capture_AUX6          (bypasses csound)
  engine stimulus -> playback_AUX6 -> cable -> capture_AUX6 -> csound input7
         -> PMOR strip -> main out 1/2 -> alsa_output monitor_AUX0/1 (tap is
         before the DAC, so DAC/analog-out latency is not included)

Latency reference: the stimulus is also sent to a reference port wired straight
into the recorder in the same graph cycle, so lag = recorded sample offset vs
that reference (+-1 quantum uncertainty if the graph orders the two stream
nodes unexpectedly; the hw number is therefore the primary datum).

Usage: prime_audio_test.py [--paths hw,engine] [--tag TXT]
Report: results/<ts>.json and results/<ts>.txt
"""
import warnings; warnings.filterwarnings('ignore')
import argparse, json, os, re, signal, subprocess, sys, tempfile, threading, time, wave
import numpy as np

FS = 48000
OUT_NODE = "alsa_output.platform-soc_107c000000_sound.pro-output-0"
IN_NODE = "alsa_input.platform-soc_107c000000_sound.pro-input-0"
PLAY, REC = "primetest_play", "primetest_rec"
LEVEL = 0.1                      # -20 dBFS peak
LEAD, TAIL = 1.0, 1.5            # silence before / after (s)
N_CHIRP, CHIRP_S, CHIRP_GAP = 3, 0.25, 0.15
TONE_S = 8.0
F1, F2 = 1000.0, 1500.0          # AUX6 / AUX7 tones
NREC = 12                        # 8 capture + ref + (unused ref2) + monL + monR
CH_REF, CH_MONL, CH_MONR = 8, 10, 11
RESULTS = os.path.expanduser("~/PRIME_RECOVERY/audiotest/results")
ABORT_ERR_JUMP = 30              # abort a test if xruns climb this much during it
PRE = ["taskset", "-c", "0-2", "nice", "-n", "10"]


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


# ---------------------------------------------------------------- xruns
def parse_top(text):
    out = {}
    for ln in text.splitlines():
        t = ln.split()
        if len(t) >= 10 and t[0] in "RSIC" and t[1].isdigit():
            name = t[-1]
            try:
                out[name] = int(t[8])
            except ValueError:
                pass
    return out


def xrun_snapshot():
    r = sh(["pw-top", "-b", "-n", "2"], timeout=20)
    last = r.stdout.split("S   ID")[-1]
    d = parse_top("S   ID" + last)
    return {"hat_out": d.get(OUT_NODE), "hat_in": d.get(IN_NODE), "csound6": d.get("csound6")}


class XrunWatch(threading.Thread):
    """pw-top -b stream; one sample per ~1 s with wall time."""
    def __init__(self, n):
        super().__init__(daemon=True)
        self.n, self.samples, self.stop_flag = n, [], False
        self.p = None

    def run(self):
        self.p = subprocess.Popen(["nice", "-n", "10", "pw-top", "-b", "-n", str(self.n)],
                                  stdout=subprocess.PIPE, text=True)
        buf = []
        for ln in self.p.stdout:
            if ln.startswith("S   ID") and buf:
                self._flush(buf); buf = []
            buf.append(ln)
        if buf:
            self._flush(buf)

    def _flush(self, buf):
        d = parse_top("".join(buf))
        if OUT_NODE in d:
            self.samples.append((time.time(), d.get(OUT_NODE), d.get(IN_NODE), d.get("csound6")))

    def climbing(self):
        s = self.samples
        if len(s) < 2:
            return False
        base = s[1] if len(s) > 1 else s[0]       # first sample after pw-top warmup
        cur = s[-1]
        return any((cur[i] or 0) - (base[i] or 0) >= ABORT_ERR_JUMP for i in (1, 3))

    def kill(self):
        if self.p and self.p.poll() is None:
            self.p.terminate()


# ---------------------------------------------------------------- stimulus
def chirp(f0, f1, dur):
    n = int(dur * FS); t = np.arange(n) / FS
    k = (f1 / f0) ** (1 / dur)
    ph = 2 * np.pi * f0 * (k ** t - 1) / np.log(k)
    w = np.sin(np.linspace(0, np.pi, n)) ** 0.5
    return np.sin(ph) * w


def make_stimulus():
    segs, marks = [np.zeros(int(LEAD * FS))], {}
    marks["chirp_starts"] = []
    pos = len(segs[0])
    for _ in range(N_CHIRP):
        c = chirp(200, 16000, CHIRP_S)
        segs.append(c); marks["chirp_starts"].append(pos); pos += len(c)
        g = np.zeros(int(CHIRP_GAP * FS)); segs.append(g); pos += len(g)
    tone_n = int(TONE_S * FS)
    ramp = int(0.02 * FS)
    env = np.ones(tone_n); env[:ramp] = np.linspace(0, 1, ramp); env[-ramp:] = np.linspace(1, 0, ramp)
    n = np.arange(tone_n)
    t1 = np.sin(2 * np.pi * F1 * n / FS) * env
    marks["tone_start"], marks["tone_len"] = pos, tone_n
    ch6 = np.concatenate(segs + [t1, np.zeros(int(TAIL * FS))])
    return ch6, marks, np.concatenate(segs + [np.sin(2 * np.pi * F2 * n / FS) * env,
                                               np.zeros(int(TAIL * FS))]), len(np.concatenate(segs))


def write_wav(path, chans):
    x = np.stack(chans, axis=1) * LEVEL
    pcm = np.clip(np.round(x * 2147483647), -2147483648, 2147483647).astype("<i4")
    with wave.open(path, "wb") as w:
        w.setnchannels(x.shape[1]); w.setsampwidth(4); w.setframerate(FS)
        w.writeframes(pcm.tobytes())


def read_wav(path):
    with wave.open(path, "rb") as w:
        n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
        raw = w.readframes(n)
    assert sw == 4, sw
    return np.frombuffer(raw, dtype="<i4").reshape(-1, ch).astype(np.float64) / 2147483648.0


# ---------------------------------------------------------------- capture run
def wait_ports(prefix, count, timeout=4.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        ports = [l for l in sh(["pw-link", "-o" if "play" in prefix else "-i"]).stdout.splitlines()
                 if l.startswith(prefix + ":")]
        if len(ports) >= count:
            return ports
        time.sleep(0.05)
    raise RuntimeError(f"ports for {prefix} did not appear")


def link(a, b, made):
    r = sh(["pw-link", a, b])
    if r.returncode:
        raise RuntimeError(f"pw-link {a} {b}: {r.stderr.strip()}")
    made.append((a, b))


def unlink_all(made):
    for a, b in made:
        sh(["pw-link", "-d", a, b])
    made.clear()


def run_pass(name, route_engine, stim, tmp):
    """returns (rec array [n,12], info dict)"""
    ch6, marks, ch7, _ = stim
    wav_in = os.path.join(tmp, f"{name}_stim.wav")
    wav_out = os.path.join(tmp, f"{name}_rec.wav")
    write_wav(wav_in, [ch6, ch7, ch6])           # AUX6 stim, AUX7 stim, reference
    before = xrun_snapshot()
    dur = len(ch6) / FS
    watch = XrunWatch(int(dur + 7))
    made, rec, play = [], None, None
    info = {"name": name, "xrun_before": before, "aborted": False}
    try:
        watch.start()
        cm = ",".join(f"AUX{i}" for i in range(NREC))
        rec = subprocess.Popen(PRE + ["pw-record", "--target", "0", "--rate", str(FS), "--format", "s32",
                                      "--channels", str(NREC), "--channel-map", cm, "--latency", "256",
                                      "-P", f"node.name={REC} node.dont-reconnect=true node.passive=false",
                                      wav_out], stderr=subprocess.PIPE, text=True)
        wait_ports(REC, NREC)
        for i in range(8):
            link(f"{IN_NODE}:capture_AUX{i}", f"{REC}:input_AUX{i}", made)
        link(f"{OUT_NODE}:monitor_AUX0", f"{REC}:input_AUX{CH_MONL}", made)
        link(f"{OUT_NODE}:monitor_AUX1", f"{REC}:input_AUX{CH_MONR}", made)
        time.sleep(0.4)
        play = subprocess.Popen(PRE + ["pw-play", "--target", "0", "--channels", "3",
                                       "--latency", "256",
                                       "-P", f"node.name={PLAY} node.dont-reconnect=true", wav_in],
                                stderr=subprocess.PIPE, text=True)
        wait_ports(PLAY, 3)
        t_link = time.time()
        link(f"{PLAY}:output_LFE", f"{REC}:input_AUX{CH_REF}", made)
        link(f"{PLAY}:output_FL", f"{OUT_NODE}:playback_AUX6", made)
        link(f"{PLAY}:output_FR", f"{OUT_NODE}:playback_AUX7", made)
        info["t_link"] = t_link
        t_end = t_link + dur + 1.0
        while play.poll() is None and time.time() < t_end + 2:
            if watch.climbing():
                info["aborted"] = True
                play.terminate(); break
            time.sleep(0.1)
        time.sleep(0.5)
    finally:
        for p in (play,):
            if p and p.poll() is None:
                p.terminate()
        if rec and rec.poll() is None:
            rec.send_signal(signal.SIGINT)
            try:
                rec.wait(5)
            except subprocess.TimeoutExpired:
                rec.kill()
        unlink_all(made)
        watch.kill()
    info["xrun_after"] = xrun_snapshot()
    info["xrun_series"] = [(round(t - info.get("t_link", t), 2), a, b, c) for t, a, b, c in watch.samples]
    return read_wav(wav_out), info


# ---------------------------------------------------------------- analysis
def xcorr_lag(sig, ref, lo=-480, hi=24000):
    """lag (samples, fractional) at which ref best matches sig; plus coefficient."""
    L = len(ref)
    seg = sig[: L + hi]
    n = 1 << int(np.ceil(np.log2(len(seg) + L)))
    c = np.fft.irfft(np.fft.rfft(seg, n) * np.conj(np.fft.rfft(ref, n)), n)
    c = np.concatenate([c[-(-lo):], c[: hi]]) if lo < 0 else c[:hi]
    k = int(np.argmax(np.abs(c)))
    y0, y1, y2 = (abs(c[k - 1]), abs(c[k]), abs(c[k + 1])) if 0 < k < len(c) - 1 else (0, abs(c[k]), 0)
    d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2) if (y0 - 2 * y1 + y2) != 0 else 0.0
    norm = np.linalg.norm(ref) * np.linalg.norm(seg[max(0, k + lo):max(0, k + lo) + L]) + 1e-30
    return k + d + lo, float(c[k] / norm)


def analyze_latency(rec, sig_ch, marks, quantum_hint=None):
    ref = rec[:, CH_REF]
    onset = int(np.argmax(np.abs(ref) > 1e-4))
    lags = []
    for cs in marks["chirp_starts"]:
        a = onset + (cs - marks["chirp_starts"][0])
        R = ref[a:a + int(CHIRP_S * FS)]
        # window starts 48 samples early so small negative lags would still be seen
        win = rec[a - 48:a + len(R) + 24000, sig_ch]
        lag, coef = xcorr_lag(win, R, lo=-0, hi=24000 + 48)
        lag -= 48
        lags.append((lag, coef))
    l = np.array([x[0] for x in lags]); cf = np.array([x[1] for x in lags])
    return {"onset_sample": onset, "lag_samples": [round(float(x), 2) for x in l],
            "lag_ms_median": round(float(np.median(l)) / FS * 1000, 3),
            "lag_ms_min": round(float(l.min()) / FS * 1000, 3), "lag_ms_max": round(float(l.max()) / FS * 1000, 3),
            "jitter_samples": round(float(l.max() - l.min()), 2),
            "corr_coef": [round(float(x), 3) for x in cf]}


def bh_window(n):
    k = np.arange(n) * 2 * np.pi / (n - 1)
    return 0.35875 - 0.48829 * np.cos(k) + 0.14128 * np.cos(2 * k) - 0.01168 * np.cos(3 * k)


def analyze_tone(x, f0, noise_x=None):
    """x: steady tone segment. Returns level/THD+N/noise/spurs."""
    n = len(x); w = bh_window(n)
    X = np.fft.rfft(x * w)
    P = np.abs(X) ** 2 / (n * np.sum(w ** 2))
    P[1:-1] *= 2
    df = FS / n
    freqs = np.arange(len(P)) * df
    lobe = 6
    def band(f):
        k = int(round(f / df)); return slice(max(k - lobe, 0), k + lobe + 1)
    fund = P[band(f0)].sum()
    mask = (freqs >= 20) & (freqs <= 20000)
    tonal = np.zeros(len(P), bool)
    tonal[band(f0)] = True
    harm = 0.0
    for h in range(2, 11):
        if f0 * h < FS / 2 - 1000:
            harm += P[band(f0 * h)].sum(); tonal[band(f0 * h)] = True
    tot = P[mask].sum()
    nd = max(tot - fund, 1e-30)
    noise_mask = mask & ~tonal
    bins_n = noise_mask.sum()
    robust_noise = np.median(P[noise_mask]) * bins_n          # median-bin * count
    # spurs: bins >15 dB above a 200-bin running median floor
    from numpy.lib.stride_tricks import sliding_window_view
    Pn = np.where(noise_mask, P, np.nan)
    floor = np.nanmedian(sliding_window_view(np.pad(Pn, 100, constant_values=np.nan), 201), axis=1)
    cand = np.where(noise_mask & (P > floor * 10 ** 1.5) & (P > 1e-14))[0]
    spurs, last = [], -99
    for k in cand:
        if k - last > 3 or not spurs:
            spurs.append([k, P[k]])
        elif P[k] > spurs[-1][1]:
            spurs[-1] = [k, P[k]]
        last = k
    spurs = sorted(spurs, key=lambda s: -s[1])[:8]
    db = lambda v: round(10 * np.log10(max(v, 1e-30)), 2)
    return {
        "fund_peak_dbfs": db(2 * fund), "rms_dbfs": db(fund),
        "thd_n_db": db(nd / fund), "thd_n_pct": round(100 * float(np.sqrt(nd / fund)), 5),
        "thd_db": db(harm / fund), "thd_pct": round(100 * float(np.sqrt(harm / fund)), 5),
        "noise_total_dbfs_20_20k": db(P[noise_mask].sum()),
        "noise_robust_dbfs_20_20k": db(robust_noise),
        "snr_db": db(fund / max(P[noise_mask].sum(), 1e-30)),
        "spurs": [{"hz": round(float(k * df), 1), "dbc": db(p / fund), "dbfs_rms": db(p)} for k, p in spurs],
    }


def find_artifacts(x, f0, t0_index):
    """sample-accurate discontinuity detection on a steady sine.
    e[n] = x[n]-2cos(w)x[n-1]+x[n-2] is ~0 for a pure sine; clicks, drops, repeats and
    zero runs produce spikes at the exact sample. t0_index maps segment index -> stimulus time."""
    w = 2 * np.pi * f0 / FS
    e = x[2:] - 2 * np.cos(w) * x[1:-1] + x[:-2]
    sig = 1.4826 * np.median(np.abs(e - np.median(e))) + 1e-12
    thr = max(10 * sig, 2e-4)
    idx = np.where(np.abs(e) > thr)[0] + 2
    events = []
    for i in idx:
        if events and i - events[-1]["end"] <= 64:
            events[-1]["end"] = int(i); events[-1]["peak"] = max(events[-1]["peak"], float(abs(e[i - 2])))
        else:
            events.append({"start": int(i), "end": int(i), "peak": float(abs(e[i - 2]))})
    # phase slip estimate per event via complex demod
    n = np.arange(len(x))
    z = x * np.exp(-1j * w * n)
    c = np.concatenate([[0], np.cumsum(z)])
    per = int(round(FS / f0))
    y = (c[per:] - c[:-per]) / per                      # y[k] covers k..k+per-1
    out = []
    for ev in events:
        a, b = ev["start"], ev["end"]
        pre = y[max(a - 400, 0):max(a - 100, 1)]
        post = y[b + 100:b + 400]
        slip = None
        if len(pre) > 20 and len(post) > 20:
            dphi = np.angle(np.mean(post) / np.mean(pre))
            slip = round(float(dphi / w), 2)               # samples (mod one period), +: later
        zr = int(np.max(np.diff(np.flatnonzero(np.r_[True, np.abs(x[max(a - 8, 0):b + 8]) > 1e-9, True])), initial=1)) - 1
        out.append({"sample": a + t0_index, "t_s": round((a + t0_index) / FS, 4), "len": b - a + 1,
                    "peak_dbfs": round(20 * np.log10(ev["peak"] + 1e-12), 1),
                    "phase_slip_samples": slip, "zero_run": zr if zr >= 4 else 0})
    amp = 2 * np.abs(y)
    core = amp[200:-200]
    return {"e_sigma": float(sig), "threshold": float(thr), "events": out, "count": len(out),
            "amp_db_p2p": round(float(20 * np.log10(core.max() / max(core.min(), 1e-12))), 3),
            "amp_min_dbfs": round(float(20 * np.log10(max(core.min(), 1e-12))), 2)}


def channel_alignment(rec, a, b):
    """tone from AUX6 (F1) must land on capture 6, AUX7 (F2) on 7, nothing elsewhere."""
    seg = rec[a:b, :8]
    n = len(seg); win = bh_window(n)
    out = {}
    for ch in range(8):
        X = np.abs(np.fft.rfft(seg[:, ch] * win)) ** 2 / (n * np.sum(win ** 2))
        df = FS / n
        pw = lambda f: float(X[max(int(f / df) - 5, 0):int(f / df) + 6].sum() * 2)
        out[f"in{ch + 1}"] = {"f1_dbfs": round(10 * np.log10(pw(F1) + 1e-30), 1),
                                "f2_dbfs": round(10 * np.log10(pw(F2) + 1e-30), 1)}
    ok = (out["in7"]["f1_dbfs"] > -35 and out["in8"]["f2_dbfs"] > -35 and
          out["in7"]["f2_dbfs"] < out["in7"]["f1_dbfs"] - 40 and out["in8"]["f1_dbfs"] < out["in8"]["f2_dbfs"] - 40 and
          all(out[f"in{c}"]["f1_dbfs"] < -60 and out[f"in{c}"]["f2_dbfs"] < -60 for c in (1, 2, 3, 4, 5, 6)))
    return {"per_channel": out, "aligned": bool(ok)}


def analyze(rec, info, marks, path):
    sig_ch = 6 if path == "hw" else CH_MONL
    res = {"path": path, "rec_seconds": round(len(rec) / FS, 2)}
    lat = analyze_latency(rec, sig_ch, marks)
    res["latency"] = lat
    ref = rec[:, CH_REF]
    on = lat["onset_sample"]
    lagmed = int(round(np.median(lat["lag_samples"])))
    ts, tl = marks["tone_start"], marks["tone_len"]
    trim = int(0.15 * FS)
    a = on + (ts - marks["chirp_starts"][0]) + lagmed + trim
    b = on + (ts - marks["chirp_starts"][0]) + lagmed + tl - trim
    seg = rec[a:b, sig_ch]
    res["tone_window"] = [a, b]
    res["tone"] = analyze_tone(seg, F1)
    res["tone"]["gain_vs_ref_db"] = round(float(20 * np.log10(np.std(seg) / np.std(ref[on + (ts - marks["chirp_starts"][0]) + trim:on + (ts - marks["chirp_starts"][0]) + tl - trim]))), 2)
    # noise floor from the pre-roll silence, before the stimulus onset (hw path only meaningful)
    pre = rec[int(0.35 * FS):max(on - int(0.1 * FS), int(0.4 * FS)), sig_ch]
    if len(pre) > 4096:
        res["silence_rms_dbfs"] = round(float(20 * np.log10(np.sqrt(np.mean(pre ** 2)) + 1e-12)), 1)
    res["artifacts"] = find_artifacts(seg, F1, a)
    # THD+N again on the longest glitch-free stretch (events padded by 0.1 s), so a click does not
    # masquerade as distortion
    pad = int(0.1 * FS)
    cuts = [0] + [v for ev in res["artifacts"]["events"] for v in (ev["sample"] - a - pad, ev["sample"] - a + ev["len"] + pad)] + [len(seg)]
    pieces = [(cuts[i], cuts[i + 1]) for i in range(0, len(cuts), 2) if cuts[i + 1] - cuts[i] > 0]
    p0, p1 = max(pieces, key=lambda p: p[1] - p[0])
    if p1 - p0 > int(1.0 * FS):
        res["tone_clean"] = analyze_tone(seg[p0:p1], F1)
        res["tone_clean"]["window_s"] = round((p1 - p0) / FS, 2)
    for ev in res["artifacts"]["events"]:
        ev["t_stim_s"] = round((ev["sample"] - on) / FS, 3)       # time relative to stimulus onset
    if path == "hw":
        res["alignment"] = channel_alignment(rec, a, b)
    # xrun deltas
    xb, xa = info["xrun_before"], info["xrun_after"]
    res["xrun_delta"] = {k: (None if xb[k] is None or xa[k] is None else xa[k] - xb[k]) for k in xb}
    res["xrun_series_t_vs_link"] = info["xrun_series"]
    res["aborted"] = info["aborted"]
    return res


def text_report(rep):
    L = [f"PRIME audio test  {rep['when']}  tag={rep['tag']}",
         f"quantum/state: {rep['env']}", ""]
    for r in rep["runs"]:
        L.append(f"== path: {r['path']}  (rec {r['rec_seconds']} s{'  ABORTED' if r.get('aborted') else ''})")
        la = r["latency"]
        L.append(f"latency vs reference: median {la['lag_ms_median']} ms (min {la['lag_ms_min']}, max {la['lag_ms_max']}), "
                 f"jitter {la['jitter_samples']} smp, corr {la['corr_coef']}")
        t = r["tone"]
        L.append(f"1 kHz: {t['fund_peak_dbfs']} dBFS peak (gain vs ref {t['gain_vs_ref_db']} dB), "
                 f"THD+N {t['thd_n_db']} dB ({t['thd_n_pct']} %), THD {t['thd_db']} dB, SNR {t['snr_db']} dB")
        L.append(f"noise 20-20k: total {t['noise_total_dbfs_20_20k']} dBFS, robust {t['noise_robust_dbfs_20_20k']} dBFS; "
                 f"silence rms {r.get('silence_rms_dbfs')} dBFS")
        if "tone_clean" in r:
            c = r["tone_clean"]
            L.append(f"glitch-free {c['window_s']} s: THD+N {c['thd_n_db']} dB ({c['thd_n_pct']} %), THD {c['thd_db']} dB, "
                     f"SNR {c['snr_db']} dB, noise {c['noise_total_dbfs_20_20k']} dBFS; spurs: " +
                     "; ".join(f"{s['hz']} Hz {s['dbc']} dBc" for s in c["spurs"]))
        L.append("spurs: " + "; ".join(f"{s['hz']} Hz {s['dbc']} dBc" for s in t["spurs"]))
        ar = r["artifacts"]
        L.append(f"artifacts: {ar['count']} (e_sigma {ar['e_sigma']:.2e}, thr {ar['threshold']:.2e}), "
                 f"amp p2p {ar['amp_db_p2p']} dB")
        for ev in ar["events"][:40]:
            L.append(f"   t={ev['t_stim_s']}s sample={ev['sample']} len={ev['len']} peak={ev['peak_dbfs']} dBFS "
                     f"slip={ev['phase_slip_samples']} zero_run={ev['zero_run']}")
        L.append(f"xrun delta (hat_out, hat_in, csound6): {r['xrun_delta']}")
        if "alignment" in r:
            L.append(f"channel alignment ok={r['alignment']['aligned']}: " +
                     json.dumps(r["alignment"]["per_channel"]))
        L.append("")
    return "\n".join(L)


def env_state():
    q = sh(["pw-metadata", "-n", "settings"]).stdout
    g = lambda k: (re.search(rf"key:'{k}' value:'([^']*)'", q) or [None, "?"])[1]
    return {"quantum": g("clock.quantum"), "force_quantum": g("clock.force-quantum"), "rate": g("clock.rate")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--paths", default="hw,engine")
    ap.add_argument("--tag", default="")
    a = ap.parse_args()
    os.makedirs(RESULTS, exist_ok=True)
    stim = make_stimulus()
    rep = {"when": time.strftime("%F %T"), "tag": a.tag, "env": env_state(), "runs": []}
    with tempfile.TemporaryDirectory(prefix="primetest_") as tmp:
        for path in a.paths.split(","):
            snap = xrun_snapshot()
            print(f"[{path}] xruns before: {snap}", flush=True)
            rec, info = run_pass(path, path == "engine", stim, tmp)
            np.save(os.path.join(RESULTS, f"raw_{time.strftime('%Y%m%d-%H%M%S')}_{path}.npy"), rec.astype(np.float32))
            res = analyze(rec, info, stim[1], path)
            rep["runs"].append(res)
            print(f"[{path}] done: lat {res['latency']['lag_ms_median']} ms, THD+N {res['tone']['thd_n_db']} dB, "
                  f"artifacts {res['artifacts']['count']}, xrun {res['xrun_delta']}", flush=True)
            if res["aborted"] or any((v or 0) >= ABORT_ERR_JUMP for v in res["xrun_delta"].values()):
                print("xruns climbing; stopping further paths", flush=True); break
            time.sleep(2)
    ts = time.strftime("%Y%m%d-%H%M%S")
    with open(f"{RESULTS}/{ts}.json", "w") as f:
        json.dump(rep, f, indent=1)
    with open(f"{RESULTS}/{ts}.txt", "w") as f:
        f.write(text_report(rep))
    print(text_report(rep))
    print(f"leftover primetest links: {[l for l in sh(['pw-link','-l']).stdout.splitlines() if 'primetest' in l]}")


if __name__ == "__main__":
    main()

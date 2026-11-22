"""
Audio path, two possible modes:

AGGREGATE MODE (preferred): if a macOS Aggregate Device exists that combines
the Pocket OpGorator's input with an output device in a single CoreAudio
clock domain, PortAudio sees it as ONE device with both input and output
channels. We open a single duplex `sounddevice.Stream` on it: no ring
buffer, no resampling, latency bounded only by block size + whatever
CoreAudio/PortAudio reports. The OS's own drift-correction (applied to the
non-master sub-device of the aggregate) absorbs the crystal mismatch
between the two physical devices -- we don't have to.

DUAL-STREAM FALLBACK (used automatically when no aggregate is found): the
capture device (Pocket OpGorator Audio) and the playback device (Mac
speakers, or whatever --output-device points at) are two independent
pieces of hardware with independent clock crystals. CoreAudio will not let
you open a single full-duplex stream spanning two unrelated devices, and
even if it did, the sample clocks would still drift relative to each
other. So:

  input stream  (callback A, its own thread) --writes--> SPSC ring buffer
  output stream (callback B, its own thread) --reads,resamples--> speakers

Both callbacks are written to do zero locking. The ring buffer is a classic
single-producer/single-consumer design using monotonically increasing
frame counters (never wrapped) so fill level is always
`write_frames - read_frames` with no ambiguity between "empty" and "full" --
each counter is only ever mutated by its own thread, so no lock is needed.

Drift + nominal rate mismatch are handled by ONE mechanism: the output
callback resamples on read using linear interpolation, walking a fractional
read pointer whose step size is (input_rate / output_rate) trimmed by a
slow feedback term derived from the ring buffer's fill level. If the buffer
is trending toward full, the step is nudged slightly > nominal (consumes
input slightly faster, i.e. occasionally "drops" by skipping a fractional
sample); if trending toward empty, nudged slightly < nominal ("inserts" by
re-visiting a fractional sample). This is the "small ring buffer + explicit
drift policy" called for in the spec, and it also transparently absorbs a
structural samplerate mismatch (e.g. 48k input feeding a 44.1k output) with
the exact same code path.

See README.md "Aggregate device mode" for how to create one and why this
gets ~8x lower latency than the fallback.
"""
import threading
import time

import numpy as np
import sounddevice as sd

from device_match import matches as _device_name_matches

# Device name matching is centralized in device_match.py (matches both
# "Pocket OpGorator" and "PMOR", case-insensitive) so the incoming firmware
# rename doesn't require touching this file. The aggregate auto-detect
# heuristic below reuses the exact same matcher -- we deliberately do NOT
# just grab "any device with both input and output channels" automatically,
# because this Mac may have unrelated duplex-capable virtual devices
# (Loopback, NDI, etc.) that must never be silently adopted.

# Ring buffer sized generously vs. the block size so scheduling jitter on
# either thread doesn't cause an immediate glitch. At 48kHz this is ~170ms
# of headroom; actual steady-state fill is held much lower by the drift
# controller (see _target_fill_frames).
RING_CAPACITY_FRAMES = 8192
CHANNELS = 2

POLL_INTERVAL_S = 1.0

# Drift-correction feedback gain: how strongly fill-level error nudges the
# resample step size, in fractional-steps-per-frame-of-error. Kept tiny so
# the pitch/time correction stays inaudible (well under a cent) while still
# tracking crystal drift on the order of tens to low-hundreds of ppm.
_DRIFT_GAIN = 6e-6
_MAX_DRIFT_TRIM = 0.02  # clamp correction to +/-2% of nominal rate


class RingBuffer:
    """Lock-free SPSC float32 ring buffer, stereo, fixed capacity."""

    def __init__(self, capacity_frames, channels=CHANNELS):
        self.capacity = capacity_frames
        self.channels = channels
        self.buf = np.zeros((capacity_frames, channels), dtype=np.float32)
        self.write_frames = 0  # owned by producer thread only
        self.read_frames = 0   # owned by consumer thread only (float pointer lives in reader)

    def fill(self):
        # Safe to read both counters from any thread: each is a single
        # monotonically increasing Python int, and reading it is atomic.
        return self.write_frames - self.read_frames

    def write(self, data):
        """Producer side. `data` is (n, channels) float32. Never blocks;
        if the buffer is full, drops the oldest unread frames worth of
        space needed (last-resort overflow safety net -- should only ever
        fire if the consumer stalls completely)."""
        n = data.shape[0]
        cap = self.capacity
        fill = self.fill()
        space = cap - fill
        if n > space:
            # Overflow safety net: advance read_frames to make room. This
            # is a distinct, rare event from the steady-state drift
            # trimming done by the reader; count it separately.
            drop = n - space
            self.read_frames += drop
            return drop
        start = self.write_frames % cap
        end = start + n
        if end <= cap:
            self.buf[start:end] = data
        else:
            first = cap - start
            self.buf[start:cap] = data[:first]
            self.buf[0:end - cap] = data[first:]
        self.write_frames += n
        return 0


def _peak_rms(channel):
    """Peak and RMS of a 1-D float32 array with NO full-size numpy
    temporary allocated. `np.abs(x)` and `x * x` each allocate a new array
    the size of the block; both are avoidable:

      - peak = max(x.max(), -x.min()) -- two scalar reductions, no abs().
      - rms  = sqrt(dot(x, x) / n)     -- dot() is a BLAS reduction that
                                          returns a scalar, never materializing
                                          the elementwise-squared array.
    """
    n = channel.shape[0]
    if n == 0:
        return 0.0, 0.0
    hi = float(channel.max())
    lo = float(-channel.min())
    peak = hi if hi > lo else lo
    rms = float(np.sqrt(np.dot(channel, channel) / n))
    return peak, rms


class AudioEngine:
    def __init__(self, state, blocksize=64, latency="low",
                 input_device_override=None, output_device_override=None,
                 target_fill_ms=8.0, aggregate_override=None):
        self.state = state
        self.blocksize = blocksize
        self.latency = latency
        self.input_device_override = input_device_override
        self.aggregate_override = aggregate_override
        self.output_device_override = output_device_override
        self.target_fill_ms = target_fill_ms

        self.ring = RingBuffer(RING_CAPACITY_FRAMES)
        self._in_stream = None
        self._out_stream = None
        self._duplex_stream = None
        self._active = False
        self._mode = "none"
        self._stop = threading.Event()
        self._poll_thread = None

        # Reader-owned fractional resample state (touched only by output
        # callback + engine setup, never by input callback)
        self._read_pos = 0.0
        self._resample_ratio = 1.0
        self._last_frame = np.zeros(CHANNELS, dtype=np.float32)
        # Preallocated scratch for the mono->stereo upmix path (used only
        # when testing against a mono input device via --input-device;
        # the real Pocket OpGorator Audio device is stereo). Allocated
        # once at stream-open time, never inside the callback.
        self._mono_scratch = np.zeros((blocksize, CHANNELS), dtype=np.float32)

        # EMA of the drift-correction trim, maintained by the output
        # callback only; in steady state this equals the true relative
        # clock-rate mismatch between input and output devices (crystal
        # drift, plus any structural nominal-samplerate difference).
        self._trim_ema = 0.0
        self._trim_accum = 0.0  # fractional frames, for whole-frame drop/insert counting

    # ---------- device discovery ----------

    def _find_input_device(self):
        if self.input_device_override is not None:
            try:
                info = sd.query_devices(self.input_device_override)
                return self.input_device_override, info
            except Exception:
                return None, None
        try:
            devices = sd.query_devices()
        except Exception:
            return None, None
        for idx, d in enumerate(devices):
            if _device_name_matches(d["name"]) and d["max_input_channels"] > 0:
                return idx, d
        return None, None

    def _resolve_output_device(self):
        if self.output_device_override is not None:
            info = sd.query_devices(self.output_device_override)
            return self.output_device_override, info
        idx = sd.default.device[1]
        info = sd.query_devices(idx)
        return idx, info

    def _find_aggregate_device(self):
        """Look for a single CoreAudio device that has BOTH input and
        output channels -- an Aggregate Device combining the Pocket
        OpGorator's capture side with an output, in one clock domain.

        --aggregate NAME_OR_INDEX always wins (explicit user choice; it is
        used as-is even if it doesn't match the name heuristic below, so
        this doubles as the way to test the duplex path against ordinary
        duplex-capable hardware, e.g. `--aggregate "Scarlett 2i2 USB"`).

        With no override, we only auto-adopt a device whose name contains
        "opgorator" (case-insensitive) -- NOT just any duplex-capable
        device. This Mac has Loopback, NDI, and other virtual audio
        devices installed that also present input+output channels; silently
        grabbing one of those would be wrong and could be surprising/
        disruptive, so auto-detection is deliberately narrow.
        """
        if self.aggregate_override is not None:
            try:
                info = sd.query_devices(self.aggregate_override)
            except Exception:
                return None, None
            idx = self.aggregate_override
            if isinstance(idx, str):
                try:
                    devices = sd.query_devices()
                except Exception:
                    devices = []
                for i, d in enumerate(devices):
                    if d["name"] == info["name"]:
                        idx = i
                        break
            if info["max_input_channels"] > 0 and info["max_output_channels"] > 0:
                return idx, info
            return None, None

        try:
            devices = sd.query_devices()
        except Exception:
            return None, None
        for idx, d in enumerate(devices):
            if (d["max_input_channels"] > 0 and d["max_output_channels"] > 0
                    and _device_name_matches(d["name"])):
                return idx, d
        return None, None

    # ---------- lifecycle ----------

    def start(self):
        self._poll_thread = threading.Thread(target=self._poll_loop, daemon=True)
        self._poll_thread.start()

    def stop(self):
        self._stop.set()
        if self._poll_thread:
            self._poll_thread.join(timeout=2.0)
        self._teardown_streams()

    def cycle_output(self):
        """Switch monitoring to the next output-capable device ('O' key).

        Same dual-stream path, same buffer sizes — zero latency cost; the
        only thing that changes is which CoreAudio output the ring drains
        into. The poll thread does the actual reopen so the UI thread never
        blocks on CoreAudio."""
        import sounddevice as sd
        try:
            devs = sd.query_devices()
        except Exception as e:
            self.state.audio.last_error = f"device list failed: {e}"
            return
        outs = [(i, d["name"]) for i, d in enumerate(devs)
                if d["max_output_channels"] > 0]
        if not outs:
            return
        cur = self.state.audio.output_device_name or self.output_device_override or ""
        names = [n for _, n in outs]
        try:
            nxt = names[(names.index(cur) + 1) % len(names)]
        except ValueError:
            nxt = names[0]
        self.output_device_override = nxt
        self.state.audio.output_device_name = nxt
        self._teardown_streams()
        self._active = False   # poll loop reopens on the new device

    def _poll_loop(self):
        while not self._stop.is_set():
            try:
                if not self._active:
                    self._try_connect()
                else:
                    self._check_still_present()
            except Exception as e:
                self.state.audio.last_error = f"poll error: {e}"
            self._stop.wait(POLL_INTERVAL_S)

    def _try_connect(self):
        # Prefer an aggregate device: one duplex stream, one clock domain,
        # no ring buffer, no resampling. Falls through to the dual-stream
        # path below if none is found (or if it fails to open).
        agg_idx, agg_info = self._find_aggregate_device()
        if agg_idx is not None:
            try:
                self._open_aggregate_stream(agg_idx, agg_info)
                self._active = True
                self._mode = "aggregate"
                self.state.audio.connected = True
                self.state.audio.stream_started_at = time.time()
                self.state.audio.last_error = ""
                total_lat = self.state.audio.input_latency_ms + self.state.audio.output_latency_ms
                print(f"[audio] AGGREGATE MODE — '{agg_info['name']}' — "
                      f"{total_lat:.1f}ms (in {self.state.audio.input_latency_ms:.1f}ms "
                      f"+ out {self.state.audio.output_latency_ms:.1f}ms, no ring buffer)")
                return
            except Exception as e:
                self.state.audio.last_error = f"aggregate open failed: {e}"
                self._teardown_streams()
                # fall through to dual-stream fallback

        in_idx, in_info = self._find_input_device()
        if in_idx is None:
            self.state.audio.connected = False
            return
        try:
            out_idx, out_info = self._resolve_output_device()
        except Exception as e:
            self.state.audio.last_error = f"no output device: {e}"
            return

        try:
            self._open_streams(in_idx, in_info, out_idx, out_info)
            self._active = True
            self._mode = "dual"
            self.state.audio.connected = True
            self.state.audio.mode = "dual"
            self.state.audio.input_device_name = in_info["name"]
            self.state.audio.output_device_name = out_info["name"]
            self.state.audio.stream_started_at = time.time()
            self.state.audio.last_error = ""
            ring_ms = 1000.0 * self.ring.fill() / self.state.audio.samplerate if self.state.audio.samplerate else 0
            total_lat = self.state.audio.input_latency_ms + self.state.audio.output_latency_ms + ring_ms
            print(f"[audio] DUAL-STREAM FALLBACK — {total_lat:.1f}ms "
                  f"(in {self.state.audio.input_latency_ms:.1f}ms + out "
                  f"{self.state.audio.output_latency_ms:.1f}ms + ring ~{ring_ms:.1f}ms) — "
                  f"create an aggregate device for ~8x lower latency (see README.md "
                  f"'Aggregate device mode', or run with --create-aggregate)")
        except Exception as e:
            self.state.audio.last_error = f"open failed: {e}"
            self._teardown_streams()

    def _check_still_present(self):
        if self._mode == "aggregate":
            # Re-poll by name unless the user pinned it with --aggregate.
            if self.aggregate_override is None:
                idx, _ = self._find_aggregate_device()
                if idx is None:
                    self.state.audio.last_error = "aggregate device disappeared"
                    self._teardown_streams()
                    self._active = False
                    self.state.audio.connected = False
                    return
            return
        # Dual-stream mode: re-poll by name (unless overridden by fixed
        # index) to catch unplug.
        if self.input_device_override is None:
            idx, _ = self._find_input_device()
            if idx is None:
                self.state.audio.last_error = "input device disappeared"
                self._teardown_streams()
                self._active = False
                self.state.audio.connected = False
                return
        # drift_ppm itself is now updated directly by the output callback
        # (see _trim_ema); nothing to do here.

    def _open_aggregate_stream(self, idx, info):
        """One duplex stream on one CoreAudio device: no ring buffer, no
        resampling. Latency comes entirely from PortAudio's own reported
        stream latency for the block size in use -- there is no separate
        buffering stage to add on top."""
        sr = float(info["default_samplerate"]) if info.get("default_samplerate") else 48000.0
        in_ch = max(1, min(2, info["max_input_channels"]))
        out_ch = max(1, min(2, info["max_output_channels"]))
        self._duplex_in_ch = in_ch
        self._duplex_out_ch = out_ch

        self.state.audio.samplerate = sr
        self.state.audio.blocksize = self.blocksize
        self.state.audio.underrun_count = 0
        self.state.audio.overrun_count = 0
        self.state.audio.input_overflow_count = 0
        self.state.audio.output_underflow_count = 0
        self.state.audio.drop_count = 0
        self.state.audio.insert_count = 0
        self.state.audio.ring_fill_frames = 0
        self.state.audio.ring_capacity_frames = 0
        self.state.audio.drift_ppm = 0.0

        self._duplex_stream = sd.Stream(
            device=(idx, idx),
            channels=(in_ch, out_ch),
            samplerate=sr,
            blocksize=self.blocksize,
            dtype="float32",
            latency=self.latency,
            callback=self._duplex_callback,
            finished_callback=self._on_stream_finished,
        )
        self._duplex_stream.start()

        # sounddevice reports (input_latency, output_latency) for a duplex
        # stream -- both real PortAudio numbers, not estimates.
        lat = self._duplex_stream.latency
        try:
            in_lat, out_lat = lat
        except TypeError:
            in_lat = out_lat = lat
        self.state.audio.input_latency_ms = in_lat * 1000.0
        self.state.audio.output_latency_ms = out_lat * 1000.0

        self.state.audio.mode = "aggregate"
        self.state.audio.aggregate_device_name = info["name"]
        self.state.audio.input_device_name = info["name"]
        self.state.audio.output_device_name = info["name"]

    def _open_streams(self, in_idx, in_info, out_idx, out_info):
        in_sr = 48000.0  # device spec: fixed 48kHz capture
        try:
            out_sr = float(out_info["default_samplerate"])
        except Exception:
            out_sr = 48000.0

        self._resample_ratio = in_sr / out_sr
        self._read_pos = 0.0
        self.ring.write_frames = 0
        self.ring.read_frames = 0
        self._last_frame = np.zeros(CHANNELS, dtype=np.float32)
        self._trim_ema = 0.0
        self._trim_accum = 0.0

        self.state.audio.samplerate = in_sr
        self.state.audio.blocksize = self.blocksize
        self.state.audio.underrun_count = 0
        self.state.audio.overrun_count = 0
        self.state.audio.drop_count = 0
        self.state.audio.insert_count = 0
        self.state.audio.ring_capacity_frames = self.ring.capacity

        self._in_stream = sd.InputStream(
            device=in_idx,
            channels=min(2, in_info["max_input_channels"]),
            samplerate=in_sr,
            blocksize=self.blocksize,
            dtype="float32",
            latency=self.latency,
            callback=self._input_callback,
            finished_callback=self._on_stream_finished,
        )
        self._out_stream = sd.OutputStream(
            device=out_idx,
            channels=CHANNELS,
            samplerate=out_sr,
            blocksize=self.blocksize,
            dtype="float32",
            latency=self.latency,
            callback=self._output_callback,
            finished_callback=self._on_stream_finished,
        )
        # START ORDER MATTERS — output FIRST, then input.
        # Measured 2026-08-14: starting the input first and then calling
        # out_stream.start() blocked forever inside PortAudio (stack showed
        # sounddevice.py:1127 start() with no return). Because that blocks in
        # C holding the GIL, the ENTIRE app freezes — pygame never gets to
        # open its window, so the symptom looks like "the UI won't launch"
        # rather than "audio failed". The running input callback contends
        # with the second stream's start.
        # Output first is safe: its callback just pulls silence from an empty
        # ring until the input side comes up a moment later.
        self._out_stream.start()
        self._in_stream.start()

        self.state.audio.input_latency_ms = self._in_stream.latency * 1000.0
        self.state.audio.output_latency_ms = self._out_stream.latency * 1000.0

    def _on_stream_finished(self):
        # Called by PortAudio if a stream stops itself (e.g. device yanked).
        self._active = False
        self._mode = "none"
        self.state.audio.connected = False
        self.state.audio.mode = "none"

    def _teardown_streams(self):
        for s in (self._in_stream, self._out_stream, self._duplex_stream):
            if s is not None:
                try:
                    s.stop()
                    s.close()
                except Exception:
                    pass
        self._in_stream = None
        self._out_stream = None
        self._duplex_stream = None
        self._mode = "none"
        self.state.audio.mode = "none"

    # ---------- audio callbacks: NO allocation, NO print, NO locks ----------

    def _duplex_callback(self, indata, outdata, frames, time_info, status):
        """Aggregate-device path: straight passthrough, one clock domain.
        No ring buffer, no resampling -- see module docstring."""
        st = self.state.audio
        if status.input_overflow:
            st.input_overflow_count += 1
        if status.input_underflow:
            st.underrun_count += 1
        if status.output_underflow:
            st.underrun_count += 1
            st.output_underflow_count += 1
        if status.output_overflow:
            st.overrun_count += 1

        in_ch = indata.shape[1]
        if in_ch >= 2:
            l = indata[:, 0]
            r = indata[:, 1]
        else:
            l = indata[:, 0]
            r = indata[:, 0]
        st.peak_l, st.rms_l = _peak_rms(l) if frames else (0.0, 0.0)
        st.peak_r, st.rms_r = _peak_rms(r) if frames else (0.0, 0.0)
        st.disp_peak_l = max(st.peak_l, st.disp_peak_l * 0.85)
        st.disp_peak_r = max(st.peak_r, st.disp_peak_r * 0.85)

        out_ch = outdata.shape[1]
        if in_ch == out_ch:
            outdata[:] = indata
        elif in_ch == 1 and out_ch >= 2:
            for c in range(out_ch):
                outdata[:, c] = indata[:, 0]
        elif in_ch >= 2 and out_ch == 1:
            outdata[:, 0] = indata[:, 0]
        else:
            n = min(in_ch, out_ch)
            outdata[:, :n] = indata[:, :n]
            if out_ch > n:
                outdata[:, n:] = 0.0

        st.ring_fill_frames = 0

    def _input_callback(self, indata, frames, time_info, status):
        st = self.state.audio
        if status.input_overflow:
            st.input_overflow_count += 1
        if status.input_underflow:
            st.underrun_count += 1

        # Level metering: no full-size numpy temporaries (see _peak_rms).
        ch = indata.shape[1]
        if ch >= 2:
            l = indata[:, 0]
            r = indata[:, 1]
        else:
            l = indata[:, 0]
            r = indata[:, 0]
        st.peak_l, st.rms_l = _peak_rms(l) if frames else (0.0, 0.0)
        st.peak_r, st.rms_r = _peak_rms(r) if frames else (0.0, 0.0)
        st.disp_peak_l = max(st.peak_l, st.disp_peak_l * 0.85)
        st.disp_peak_r = max(st.peak_r, st.disp_peak_r * 0.85)

        if ch == 1:
            scratch = self._mono_scratch[:frames]
            scratch[:, 0] = indata[:, 0]
            scratch[:, 1] = indata[:, 0]
            dropped = self.ring.write(scratch)
        else:
            dropped = self.ring.write(indata[:, :2])
        if dropped:
            st.overrun_count += 1

        st.ring_fill_frames = self.ring.fill()

    def _output_callback(self, outdata, frames, time_info, status):
        st = self.state.audio
        if status.output_underflow:
            st.underrun_count += 1

        ring = self.ring
        cap = ring.capacity
        buf = ring.buf

        # Slow feedback trim toward a target fill level (drift + nominal
        # rate-mismatch compensation). Computed from cheap integer state,
        # no allocation.
        target = int((self.target_fill_ms / 1000.0) * (st.samplerate or 48000.0))
        if target < frames + 2:
            target = frames + 2
        fill_now = ring.write_frames - int(self._read_pos)
        error = fill_now - target
        trim = max(-_MAX_DRIFT_TRIM, min(_MAX_DRIFT_TRIM, _DRIFT_GAIN * error))
        step = self._resample_ratio * (1.0 + trim)

        write_frames = ring.write_frames
        read_pos = self._read_pos
        last = self._last_frame
        held_frames = 0

        for i in range(frames):
            idx_f = read_pos
            idx0 = int(idx_f)
            if write_frames - idx0 < 2:
                # not enough data buffered yet -- hold last sample (this
                # is a hard underflow, distinct from the steady-state
                # drift trim below; each held sample is a real duplicate)
                outdata[i, 0] = last[0]
                outdata[i, 1] = last[1]
                held_frames += 1
                read_pos = idx_f + step
                continue
            frac = idx_f - idx0
            i0 = idx0 % cap
            i1 = (idx0 + 1) % cap
            s0 = buf[i0]
            s1 = buf[i1]
            v0 = s0[0] + (s1[0] - s0[0]) * frac
            v1 = s0[1] + (s1[1] - s0[1]) * frac
            outdata[i, 0] = v0
            outdata[i, 1] = v1
            last[0] = v0
            last[1] = v1
            read_pos = idx_f + step

        self._read_pos = read_pos
        ring.read_frames = int(read_pos)
        st.ring_fill_frames = write_frames - int(read_pos)

        if held_frames:
            st.output_underflow_count += 1
            st.insert_count += held_frames
        else:
            # Only feed the drift EMA from blocks that weren't stalled --
            # a hard underflow is a separate, rarer event and would bias
            # the steady-state drift estimate.
            self._trim_ema = 0.98 * self._trim_ema + 0.02 * trim
            st.drift_ppm = self._trim_ema * 1e6

            # Convert the continuous trim into whole-frame drop/insert
            # counts by accumulating the fractional deviation from nominal
            # and only counting a discrete event once it totals a full
            # frame -- this is what actually gets dropped/duplicated over
            # time, not a per-block flag.
            self._trim_accum += trim * frames
            while self._trim_accum >= 1.0:
                st.drop_count += 1
                self._trim_accum -= 1.0
            while self._trim_accum <= -1.0:
                st.insert_count += 1
                self._trim_accum += 1.0

"""
Shared state between the audio callback thread, the MIDI thread, and the
pygame UI thread.

Design constraint: the audio callback must never allocate, print, or take a
lock. Every field written from the audio callback is a plain Python
attribute holding an immutable value (float/int/bool) or an index into a
preallocated numpy array. CPython attribute get/set on built-in types is a
single bytecode op protected by the GIL, so this is safe for the
single-writer/multi-reader pattern used here without an explicit lock -- no
field here is ever read-modify-written by more than one thread.

The MIDI thread updates a disjoint set of fields, so it never contends with
the audio thread either. The UI thread only reads.
"""
import time


class AudioState:
    """Fields written ONLY by the audio callback (or the audio engine's own
    control thread for setup/teardown). Read by the UI thread."""

    def __init__(self):
        # Connection
        self.connected = False
        self.input_device_name = ""
        self.output_device_name = ""

        # "aggregate"  = single duplex stream on a CoreAudio aggregate device
        #                (one clock domain, no ring buffer, no resampling)
        # "dual"       = two independent streams + ring buffer + resampler
        #                (fallback, used when no aggregate device is found)
        # "none"       = not connected yet
        self.mode = "none"
        self.aggregate_device_name = ""

        # Level meters (updated once per callback block)
        self.peak_l = 0.0
        self.peak_r = 0.0
        self.rms_l = 0.0
        self.rms_r = 0.0

        # Decaying display peaks (UI-friendly, still written by audio thread
        # so the UI never has to compute decay itself under a lock)
        self.disp_peak_l = 0.0
        self.disp_peak_r = 0.0

        # Latency, as actually reported by PortAudio
        self.input_latency_ms = 0.0
        self.output_latency_ms = 0.0
        self.blocksize = 0
        self.samplerate = 0.0

        # Glitch counters
        self.underrun_count = 0
        self.overrun_count = 0
        self.input_overflow_count = 0
        self.output_underflow_count = 0

        # Ring buffer / drift telemetry
        self.ring_fill_frames = 0
        self.ring_capacity_frames = 0
        self.drift_ppm = 0.0
        self.drop_count = 0          # frames dropped (buffer running full)
        self.insert_count = 0        # frames duplicated (buffer running empty)

        self.last_error = ""
        self.stream_started_at = 0.0


class MidiState:
    """Fields written ONLY by the MIDI thread. Read by the UI thread."""

    def __init__(self):
        self.connected = False
        self.port_name = ""

        self.tick_count = 0
        self.duplicate_count = 0
        self.discarded_glitch_count = 0  # sub-2ms non-duplicate oddities (very rare)

        self.bpm_measured = 0.0      # host-computed, from filtered inter-tick median
        self.bpm_reported = 0.0      # from device CC119
        self.bpm_disagree = False

        self.role = 0                # CC116: 0=unknown, 1=OpGorator master, 2=other master
        self.running = False         # Start/Stop transport
        self.last_start_at = 0.0
        self.last_stop_at = 0.0

        self.bar_tick = 0            # 0..95, position within current bar (96 ticks/bar)
        self.beat_tick = 0           # 0..23, position within current beat

        self.last_tick_at = 0.0
        self.last_message_at = 0.0
        self.last_error = ""

        # Naive ticks/elapsed-time BPM, kept alongside the median-filtered
        # bpm_measured above specifically so the two can be shown side by
        # side -- the discrepancy between them is what originally exposed
        # the duplicate-0xF8 firmware bug, so it's first-class, not derived
        # ad hoc in the UI.
        self.bpm_naive = 0.0
        self.duplicates_per_sec = 0.0

        # MIDI OUTPUT port (role control: sends ch16 CC116 back to the
        # device). Independent connection state from the input port above --
        # one can be present without the other.
        self.out_connected = False
        self.out_port_name = ""
        self.role_sent = 0           # last role value WE sent (0 = never sent)
        self.role_sent_at = 0.0


class CdcState:
    """Fields written ONLY by the CDC (USB serial console) thread. Read by
    the UI thread. Independent of both AudioState and MidiState -- the
    serial console can be present/absent regardless of whether the audio or
    MIDI interfaces are enumerated, and each panel must degrade to
    "waiting" on its own."""

    def __init__(self):
        self.connected = False
        self.port_path = ""
        self.last_error = ""
        self.last_rx_at = 0.0
        self.last_tx_at = 0.0

        # Generic parsed key/value fields from the most recent STATUS/DIAG
        # response (lowercased keys). The exact wire format of STATUS/DIAG
        # isn't part of the documented protocol, so parsing is deliberately
        # tolerant/best-effort -- see cdc_engine.py. Raw text is always kept
        # too, so nothing is lost if a field isn't recognized.
        self.fields = {}
        self.last_status_raw = ""
        self.last_diag_raw = ""
        self.last_diag_at = 0.0

        # Best-effort extracted values (None if not seen/parsed yet)
        self.sample_rate = None
        self.role = None
        self.bpm = None
        self.tick_count = None
        self.tx_count = None
        self.rx_count = None

        # Local control state, reflecting the last command WE sent (not
        # necessarily confirmed by the device -- the next STATUS/DIAG poll
        # will correct these if the device disagrees).
        self.sy_mode = None          # 0..5, or None if never set this session
        self.beat_on = None          # True/False, or None if unknown
        self.beat_level = None       # 0-127, or None if unknown

        self.dfu_sent = False
        self.dfu_sent_at = 0.0


class SharedState:
    def __init__(self):
        self.audio = AudioState()
        self.midi = MidiState()
        self.cdc = CdcState()
        self.started_at = time.time()

"""
pygame UI: radial MIDI clock (pulses traveling around a ring, one
revolution = one bar = 96 ticks at 24 PPQN), stereo level meters, a CDC
serial-console control panel, and a guarded DFU button. Runs entirely on
the UI thread; only ever READS SharedState (all writes to SharedState
happen from the audio/MIDI/CDC engine threads) except for the DFU-arm
countdown, which is UI-thread-local state passed in from main.py.
"""
import math
import time

import pygame

WIDTH, HEIGHT = 1000, 800
BG = (10, 11, 16)
FG = (230, 232, 240)
DIM = (90, 94, 110)
ACCENT = (80, 200, 255)
GREEN = (90, 220, 140)
RED = (235, 90, 90)
AMBER = (240, 180, 70)
PURPLE = (190, 140, 240)

TICKS_PER_BAR = 96
TICKS_PER_BEAT = 24

RING_CENTER = (300, 290)
RING_RADIUS = 190
LED_RADIUS_BEAT = 8
LED_RADIUS_SUB = 4

DECAY_PER_SEC = 3.2  # pulse brightness decay rate

KEYMAP_LINES = [
    "0-5 SY mode   B beat on/off   [ ]  beat level -/+   M role->host   P role->PMOR   D D confirm DFU (reboot!)   Esc quit",
]


class RadialClock:
    def __init__(self):
        self.brightness = [0.0] * TICKS_PER_BAR
        self._last_seen_tick = -1
        self._last_update = time.time()

    def update(self, midi_state):
        now = time.time()
        dt = now - self._last_update
        self._last_update = now
        decay = math.exp(-DECAY_PER_SEC * dt)
        for i in range(TICKS_PER_BAR):
            self.brightness[i] *= decay

        pos = midi_state.bar_tick
        if pos != self._last_seen_tick and midi_state.running:
            self.brightness[pos] = 1.0
            self._last_seen_tick = pos
        elif not midi_state.running:
            self._last_seen_tick = -1

    def draw(self, surf):
        cx, cy = RING_CENTER
        for i in range(TICKS_PER_BAR):
            angle = -math.pi / 2 + (i / TICKS_PER_BAR) * 2 * math.pi
            x = cx + RING_RADIUS * math.cos(angle)
            y = cy + RING_RADIUS * math.sin(angle)
            is_beat = (i % TICKS_PER_BEAT) == 0
            is_eighth = (i % 12) == 0
            b = self.brightness[i]

            if is_beat:
                base = 70
                radius = LED_RADIUS_BEAT
                colcol = ACCENT
            elif is_eighth:
                base = 45
                radius = LED_RADIUS_SUB + 1
                colcol = DIM
            else:
                base = 28
                radius = LED_RADIUS_SUB
                colcol = DIM

            lvl = base + b * (255 - base)
            color = tuple(min(255, int(c * (lvl / 255.0) + 20 * b)) for c in
                          (colcol[0] + (255 - colcol[0]) * b,
                           colcol[1] + (255 - colcol[1]) * b,
                           colcol[2] + (255 - colcol[2]) * b))
            pygame.draw.circle(surf, color, (int(x), int(y)), radius)

        # bar/quarter guide ring
        pygame.draw.circle(surf, (35, 37, 46), RING_CENTER, RING_RADIUS, 1)


def _meter_bar(surf, x, y, w, h, peak, rms, label, font):
    pygame.draw.rect(surf, (30, 32, 40), (x, y, w, h))
    peak = max(0.0, min(1.0, peak))
    rms = max(0.0, min(1.0, rms))
    rms_h = int(h * rms)
    peak_h = int(h * peak)
    pygame.draw.rect(surf, GREEN, (x, y + h - rms_h, w, rms_h))
    if peak_h > 0:
        py = y + h - peak_h
        pygame.draw.line(surf, AMBER if peak < 0.95 else RED, (x, py), (x + w, py), 2)
    txt = font.render(label, True, DIM)
    surf.blit(txt, (x + w // 2 - txt.get_width() // 2, y + h + 4))


def _text(surf, font, s, x, y, color=FG):
    img = font.render(s, True, color)
    surf.blit(img, (x, y))
    return img.get_height()


def _panel_header(surf, font, s, x, y, connected):
    color = FG if connected else DIM
    _text(surf, font, s, x, y, color)


def draw(surf, state, fonts, fps_actual, dfu_armed_until=0.0):
    font_big, font, font_small = fonts
    surf.fill(BG)

    audio = state.audio
    midi = state.midi
    cdc = state.cdc

    _draw_midi_panel(surf, midi, fonts)
    _draw_audio_panel(surf, audio, fonts)
    _draw_cdc_panel(surf, cdc, midi, fonts, dfu_armed_until)
    _draw_keymap(surf, font_small, dfu_armed_until)

    _text(surf, font_small, f"ui {fps_actual:.0f} fps", WIDTH - 90, HEIGHT - 20, DIM)


def _draw_midi_panel(surf, midi, fonts):
    font_big, font, font_small = fonts

    if not hasattr(_draw_midi_panel, "_ring"):
        _draw_midi_panel._ring = RadialClock()
    ring = _draw_midi_panel._ring
    ring.update(midi)

    if midi.connected:
        ring.draw(surf)
        cx, cy = RING_CENTER
        role_txt = {0: "role: unknown", 1: "role: PMOR MASTER", 2: "role: HOST MASTER"}.get(midi.role, "role: ?")
        role_col = {0: DIM, 1: GREEN, 2: AMBER}.get(midi.role, DIM)
        state_txt = "RUNNING" if midi.running else "STOPPED"
        state_col = GREEN if midi.running else DIM

        bpm_m = f"{midi.bpm_measured:6.2f}" if midi.bpm_measured else "  --  "
        img = font_big.render(bpm_m + " BPM", True, ACCENT if not midi.bpm_disagree else RED)
        surf.blit(img, (cx - img.get_width() // 2, cy - 26))

        st_img = font.render(state_txt, True, state_col)
        surf.blit(st_img, (cx - st_img.get_width() // 2, cy - 62))

        role_img = font_small.render(role_txt, True, role_col)
        surf.blit(role_img, (cx - role_img.get_width() // 2, cy + 20))
        if midi.bpm_disagree:
            warn = font_small.render("BPM DISAGREEMENT", True, RED)
            surf.blit(warn, (cx - warn.get_width() // 2, cy + 38))

        y = 30
        _text(surf, font, "PMOR — MIDI clock", 20, y, FG); y += 30
        _text(surf, font_small, f"port: {midi.port_name}", 20, y, DIM); y += 18
        out_txt = f"MIDI out: {midi.out_port_name}" if midi.out_connected else "MIDI out: not found (role control disabled)"
        _text(surf, font_small, out_txt, 20, y, DIM if midi.out_connected else AMBER); y += 18
        if midi.role_sent:
            age = time.time() - midi.role_sent_at
            _text(surf, font_small, f"we last sent role={midi.role_sent} ({age:4.1f}s ago)", 20, y, PURPLE); y += 18
        _text(surf, font_small, f"bar tick: {midi.bar_tick:2d}/96   beat tick: {midi.beat_tick:2d}/24", 20, y, DIM); y += 22

        # ---- the duplicate-tick / BPM-discrepancy panel: the single most
        # diagnostically valuable readout in this app (see README). ----
        _text(surf, font_small, "— duplicate-tick diagnostics —", 20, y, ACCENT); y += 20
        _text(surf, font_small, f"ticks total: {midi.tick_count}", 20, y, DIM); y += 18
        dup_col = AMBER if midi.duplicate_count else DIM
        pct = (100.0 * midi.duplicate_count / (midi.tick_count + midi.duplicate_count)) if (midi.tick_count + midi.duplicate_count) else 0.0
        _text(surf, font_small, f"duplicates: {midi.duplicate_count} total ({pct:.1f}%)  |  {midi.duplicates_per_sec:.2f}/sec now", 20, y, dup_col); y += 18
        naive_txt = f"{midi.bpm_naive:6.2f}" if midi.bpm_naive else "  --  "
        median_txt = f"{midi.bpm_measured:6.2f}" if midi.bpm_measured else "  --  "
        disagree = abs(midi.bpm_naive - midi.bpm_measured) if (midi.bpm_naive and midi.bpm_measured) else 0.0
        disc_col = RED if disagree > 1.0 else DIM
        _text(surf, font_small, f"naive BPM (raw ticks/time, incl. dupes): {naive_txt}", 20, y, AMBER); y += 18
        _text(surf, font_small, f"median-filtered BPM (host, dupes removed): {median_txt}", 20, y, GREEN); y += 18
        _text(surf, font_small, f"discrepancy: {disagree:.2f} BPM", 20, y, disc_col); y += 18
        rep_txt = f"{midi.bpm_reported:6.2f}" if midi.bpm_reported else "  --  "
        _text(surf, font_small, f"device-reported BPM (CC119): {rep_txt}", 20, y, DIM); y += 18
    else:
        cx, cy = RING_CENTER
        img = font.render("waiting for PMOR / Pocket OpGorator (MIDI)...", True, DIM)
        surf.blit(img, (cx - img.get_width() // 2, cy))
        pygame.draw.circle(surf, (30, 32, 40), RING_CENTER, RING_RADIUS, 1)
        _text(surf, font, "PMOR — MIDI clock", 20, 30, FG)
        if midi.last_error:
            _text(surf, font_small, midi.last_error, 20, 60, DIM)


def _draw_audio_panel(surf, audio, fonts):
    font_big, font, font_small = fonts
    ax = 620
    y = 30
    _text(surf, font, "Audio monitor", ax, y, FG); y += 34
    if audio.connected:
        is_agg = audio.mode == "aggregate"
        if is_agg:
            total_lat = audio.input_latency_ms + audio.output_latency_ms
            mode_txt = f"AGGREGATE MODE — {total_lat:.1f}ms"
            mode_col = GREEN
        else:
            total_lat = audio.input_latency_ms + audio.output_latency_ms + (
                1000.0 * audio.ring_fill_frames / audio.samplerate if audio.samplerate else 0)
            mode_txt = f"DUAL-STREAM FALLBACK — {total_lat:.1f}ms"
            mode_col = AMBER
        mode_img = font_small.render(mode_txt, True, mode_col)
        surf.blit(mode_img, (ax, y))
        y += 24

        _meter_bar(surf, ax, y, 40, 190, audio.disp_peak_l, audio.rms_l, "L", font_small)
        _meter_bar(surf, ax + 60, y, 40, 190, audio.disp_peak_r, audio.rms_r, "R", font_small)
        yy = y + 220
        if is_agg:
            _text(surf, font_small, f"aggregate: {audio.aggregate_device_name}", ax, yy, DIM); yy += 18
        else:
            _text(surf, font_small, f"in:  {audio.input_device_name}", ax, yy, DIM); yy += 18
            _text(surf, font_small, f"out: {audio.output_device_name}", ax, yy, DIM); yy += 18
        _text(surf, font_small, f"blocksize {audio.blocksize}  @ {audio.samplerate:.0f}Hz", ax, yy, DIM); yy += 18
        _text(surf, font_small, f"latency: in {audio.input_latency_ms:.2f}ms  out {audio.output_latency_ms:.2f}ms  (~{total_lat:.1f}ms total)", ax, yy, ACCENT); yy += 20
        glitch_col = RED if (audio.underrun_count or audio.overrun_count) else DIM
        _text(surf, font_small, f"underruns {audio.underrun_count}  overruns {audio.overrun_count}", ax, yy, glitch_col); yy += 18
        if not is_agg:
            _text(surf, font_small, f"drift: {audio.drift_ppm:+.1f} ppm  drop {audio.drop_count}  insert {audio.insert_count}", ax, yy, DIM); yy += 18
            if audio.last_error:
                # In dual-stream mode, last_error is usually WHY the
                # aggregate autocreate didn't happen (see audio_engine.py
                # _try_autocreate_aggregate) -- surfaced here rather than
                # hidden, since that's exactly the "why is this glitchy"
                # answer the fallback mode implies.
                _text(surf, font_small, f"no aggregate: {audio.last_error}", ax, yy, AMBER); yy += 18
    else:
        _text(surf, font_small, "waiting for PMOR / Pocket OpGorator Audio...", ax, y, DIM)
        if audio.last_error:
            _text(surf, font_small, audio.last_error, ax, y + 24, DIM)


CDC_Y = 560
CDC_H = 170


def _draw_cdc_panel(surf, cdc, midi, fonts, dfu_armed_until):
    font_big, font, font_small = fonts
    x = 20
    y = CDC_Y
    pygame.draw.rect(surf, (18, 19, 26), (10, y - 10, WIDTH - 20, CDC_H), border_radius=6)
    _panel_header(surf, font, "CDC control console", x, y, cdc.connected)
    y += 28

    if not cdc.connected:
        _text(surf, font_small, "waiting for PMOR / Pocket OpGorator CDC serial console...", x, y, DIM)
        y += 20
        if cdc.last_error:
            _text(surf, font_small, cdc.last_error, x, y, DIM)
        return

    _text(surf, font_small, f"port: {cdc.port_path}", x, y, DIM)
    diag_age = (time.time() - cdc.last_diag_at) if cdc.last_diag_at else None
    diag_txt = f"last DIAG: {diag_age:4.1f}s ago" if diag_age is not None else "last DIAG: (none yet)"
    _text(surf, font_small, diag_txt, x + 420, y, DIM)
    y += 22

    sy_txt = f"SY mode: {cdc.sy_mode if cdc.sy_mode is not None else '?'}"
    beat_txt = "BEAT: ON" if cdc.beat_on else ("BEAT: off" if cdc.beat_on is False else "BEAT: ?")
    beat_col = GREEN if cdc.beat_on else DIM
    lvl_txt = f"level {cdc.beat_level}" if cdc.beat_level is not None else "level ?"
    _text(surf, font_small, sy_txt, x, y, ACCENT)
    _text(surf, font_small, beat_txt, x + 160, y, beat_col)
    _text(surf, font_small, lvl_txt, x + 300, y, DIM)
    y += 22

    # readout parsed from STATUS/DIAG -- best-effort field extraction, see
    # cdc_engine.py. Falls back to "?" per field rather than hiding the row.
    sr = f"{cdc.sample_rate}" if cdc.sample_rate is not None else "?"
    role = f"{cdc.role}" if cdc.role is not None else "?"
    bpm = f"{cdc.bpm:.1f}" if isinstance(cdc.bpm, (int, float)) else "?"
    tick = f"{cdc.tick_count}" if cdc.tick_count is not None else "?"
    tx = f"{cdc.tx_count}" if cdc.tx_count is not None else "?"
    rx = f"{cdc.rx_count}" if cdc.rx_count is not None else "?"
    _text(surf, font_small, f"rate {sr}Hz   role {role}   bpm {bpm}   tick {tick}   tx {tx}   rx {rx}",
          x, y, FG)
    y += 24

    # Raw DIAG text always shown too -- nothing the device sends is hidden
    # even if a field's key didn't match a known alias.
    raw = cdc.last_diag_raw or cdc.last_status_raw
    if raw:
        _text(surf, font_small, "raw:", x, y, DIM)
        rx_x = x + 40
        for line in raw.splitlines()[:4]:
            img = font_small.render(line[:100], True, (120, 122, 132))
            surf.blit(img, (rx_x, y))
            y += 16

    # ---- guarded DFU button ----
    dfu_x = WIDTH - 320
    dfu_y = CDC_Y
    now = time.time()
    armed = dfu_armed_until and now <= dfu_armed_until
    if armed:
        remaining = dfu_armed_until - now
        pygame.draw.rect(surf, (90, 30, 30), (dfu_x, dfu_y, 300, 60), border_radius=6)
        _text(surf, font, "PRESS D AGAIN TO REBOOT TO DFU", dfu_x + 10, dfu_y + 8, RED)
        _text(surf, font_small, f"confirm window: {remaining:.1f}s — any other key cancels", dfu_x + 10, dfu_y + 34, RED)
    else:
        pygame.draw.rect(surf, (40, 24, 24), (dfu_x, dfu_y, 300, 60), border_radius=6, width=1)
        _text(surf, font, "D — arm DFU reboot", dfu_x + 10, dfu_y + 8, AMBER)
        _text(surf, font_small, "reboots device into USB bootloader for flashing", dfu_x + 10, dfu_y + 34, DIM)
    if cdc.dfu_sent:
        age = time.time() - cdc.dfu_sent_at
        _text(surf, font_small, f"dfu sent {age:4.1f}s ago — device will disconnect", dfu_x, dfu_y + 66, AMBER)


def _draw_keymap(surf, font_small, dfu_armed_until):
    y = HEIGHT - 40
    pygame.draw.line(surf, (35, 37, 46), (10, y - 6), (WIDTH - 10, y - 6), 1)
    for line in KEYMAP_LINES:
        _text(surf, font_small, line, 20, y, DIM)
        y += 18

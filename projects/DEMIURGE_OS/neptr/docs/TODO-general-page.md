# TODO — GENERAL page (requested 2026-09-01)

User request, captured verbatim in spirit:

- New **GENERAL** page/menu with:
  - **Master volume** — a true END-of-chain gain. The existing "volume" the
    user normally rides is effectively a PRE volume (before effects); keep
    that as-is and add master as the final stage after everything.
  - **Master BPM** — one authoritative tempo, where the looper or an
    external clock is always allowed to determine/override it (existing
    chase_bliss_clock.orc precedence: loop-derived > external CC119 > manual).
- When BPM is changed **from this menu**, looper/buffer-based effects must
  follow with **smooth varispeed** (no hard jumps/clicks) — resample their
  playback rate toward the new tempo rather than snapping.

Design notes for the implementation pass:
- chase_bliss_clock.orc already owns tempo authority; the GENERAL page's BPM
  becomes the "manual" source feeding it, not a parallel clock (don't expose
  two tempo systems — consolidate per feedback_dont_expose_parallel_systems).
- Master volume: add a final gain stage in signal_chain.orc after the last
  effect return, portk-smoothed; persist in the settings ftab; UI slider +
  encoder binding on the new page.

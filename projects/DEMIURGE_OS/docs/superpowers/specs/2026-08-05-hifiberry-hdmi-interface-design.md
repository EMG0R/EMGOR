# HiFiBerry + HDMI in Interface Selection — Design

Date: 2026-08-05
Status: locked (conversation, 2026-08-05)

Extends `2026-07-26-interface-selection-design.md`. That spec's identity
model (product names, two-layer selection state, one resolver, one CLI,
thin UI wrappers) is unchanged. This spec changes WHICH cards are eligible
and WHERE.

## 1. Platform (I2S HAT) cards are first-class

The HiFiBerry DAC+ADC Pro (`card sndrpihifiberry`, product name
`snd_rpi_hifiberry_dacplusadcpro`) — and any other platform ALSA card —
must work everywhere a USB interface works:

- **Bypass (`sync_layer=off`)**: already works — `list_playback_cards()`
  is bus-agnostic; the resolver adopted it via rule 4 on the live Pi.
  No change beyond verification.
- **Sync layer (`sync_layer=on`)**: `aggregate.rs` / `devices.rs` /
  `events.rs` currently filter PipeWire nodes on `alsa_output.usb-*` /
  `alsa_input.usb-*`. Change the predicate to: any `alsa_output.*` /
  `alsa_input.*` node EXCEPT aloop (`snd_aloop` in the node name) and
  HDMI (see §2 — HDMI eligible only when explicitly chosen or toggled).
  The aloop node keeps its existing dedicated role (fallback clock),
  never appears as a selectable interface.
- The device-header writer (`devices.rs`) lists platform audio nodes in
  the live.conf comment block like USB ones.

## 2. HDMI: selectable, never default

HDMI audio devices (`vc4hdmi*` cards; `*hdmi*` PipeWire nodes) become
**visible and explicitly selectable** in the CLI and all three UIs, but
are **never auto-adopted**:

- Resolver rules 1–3 (set `interface =`, global choice, legacy CARD id):
  an HDMI card CAN match — explicit user intent wins.
- Resolver rule 4 (first attached fallback): HDMI cards are SKIPPED —
  unless the internal toggle is on.
- Sync-layer master/sink enumeration: same rule — HDMI sink only used
  when it is the explicitly preferred interface (or toggle on), never
  auto-picked as clock master / front pair.

**Toggle**: live.conf key `hdmi_default = on|off`, default **off** (and
off for this user). When `on`, HDMI cards join rule-4 fallback ordering
after non-HDMI cards. Internal knob only: hand-edit in set files /
live.conf; NOT surfaced as a UI control. Document in config-reference.

`demiurge-interface` CLI mirrors this: HDMI cards appear in listings
(human + `--list`), can be chosen by name, but its "predicted active"
logic skips them for the fallback rule unless `hdmi_default = on`.

## 3. Display names

UIs and CLI show a prettified display name while matching still uses the
raw ALSA product name. Single shared mapping (in the CLI, since all UIs
read `demiurge-interface --list`): drop `snd_rpi_` prefix, underscores →
spaces, known-pattern title-casing (e.g. `snd_rpi_hifiberry_dacplusadcpro`
→ `HiFiBerry DAC+ADC Pro`; `vc4-hdmi-0` → `HDMI 0`). `--list` gains the
display name as an extra field: `display|name|card_id|flags` stays
backward-orderable — append display as a 4th field (`name|card_id|flags|display`)
so existing parsers keep working.

## 4. Deployment / sync (this round)

- Verify Mac repo is the best version first (audit in flight; anything
  unique on the Pi gets folded back into the repo BEFORE overwriting).
- Sync repo → Pi; build launcher on the Pi (cargo); install wrappers
  incl. the missing `/opt/demiurge/bin/demiurge-interface`; deploy the
  three UIs per the 07-26 spec.
- **Csound guarantee (acceptance tests on the Pi)**:
  1. bypass: Csound opens HiFiBerry in+out (`-iadc`/`-odac`), audio out
     confirmed, launcher log shows the resolved card.
  2. sync layer: `sync_layer=on`, HiFiBerry node adopted as front
     pair/clock master, Csound routes through the graph to it.
  3. switching: `demiurge-interface <usb name>` and back to
     `hifiberry` from CLI + at least one UI; engine reopens on the
     right card each time.
  4. HDMI: shows in listings; explicit select works; with no
     global/set choice and `hdmi_default` absent, resolver never
     picks HDMI.

## HDMI output enablement (2026-08-06)

The 48ab8fa deploy verified the selection layer but exposed two blockers
that made actually SOUNDING through HDMI impossible. Fixed as follows
(user is adding an external HDMI port to the enclosure):

1. **Bypass, playback-only cards.** Csound opened `-iadc` on the selected
   card; HDMI has no capture PCM → device-open failure → restart loop.
   Now `util::run_bypass` checks `/proc/asound/<card-id>/` for a `pcm*c`
   entry; none → export `DEMIURGE_AUDIO_CAPTURE=none`, and
   `demiurge-run-csound` (direct mode) swaps `-iadc:<dev>` for
   `-iadc:null` (ALSA null plugin — capture buffer stays calloc-zeroed,
   `ins`/`inch` read silence). Deploy finding 2026-08-09: merely OMITTING
   `-iadc` does not work — csound 6.18 still honours a bare `-iadc` inside
   the CSD's `<CsOptions>` even under `-+ignore_csopts=1` and restart-loops
   on the 'default' capture device; the explicit `-iadc:null` overrides the
   CSD deterministically. Generic "card has no capture → output-only" —
   also covers playback-only USB DACs. Unreadable `/proc/asound` → assume
   capture (status quo). NOTE: the vc4-hdmi driver refuses the playback PCM
   (`ENOTSUPP`, error 524) when the attached display advertises no audio
   (EDID without CEA extension / sad_count 0) — sound test needs an
   audio-capable HDMI sink (TV/monitor with speakers).
2. **Sync layer, WirePlumber.** `50-demiurge.conf` Rule 1 hard-disabled
   HDMI nodes (`node.disabled=true`), so no HDMI sink ever existed for the
   launcher to route to. Rule 1 now DEMOTES instead: `priority.session=0`,
   `priority.driver=0`, `node.autoconnect=false`,
   `session.suspend-timeout-seconds=5` (idle node suspends, no CPU cost).
   Safe because `is_selectable_audio_node()` already keeps non-preferred
   HDMI out of aggregation/master/direct-output/devices/events, and
   graph.rs `scrub_hw_playback` + aggregate's link diff remove any stray
   links into an enabled HDMI sink.
3. **Hole closed — explicit HDMI front pair.** `aggregate::output_sinks`
   reordered the preferred sink via `iface_name_matches`, but HDMI product
   names (`vc4-hdmi-0`) never appear in PipeWire node names, so a chosen
   HDMI sink could never beat an attached USB interface to the front
   pair/clock master. New `util::sink_matches_preference` (substring OR
   preference-is-HDMI ∧ node-contains-hdmi) used there; predicate reuses
   the same `preference_is_hdmi` helper. Limitation: with both HDMI ports
   cabled, the first-enumerated HDMI node wins regardless of which
   `vc4-hdmi-N` was named.

## Out of scope

Simultaneous multi-card routing, HDMI as UI-exposed toggle, per-set UI
editing — unchanged from 07-26 spec.

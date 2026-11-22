# The Watch — Multi-Source Protocol v2

Extends the existing Bangle.js/Gadgetbridge protocol the firmware already speaks.
Adds: **multiple simultaneous BLE centrals** (phone + Mac), **per-source media
arbitration** (the play screen follows whatever is actually playing), and a
**remote permission-approval** channel (approve Claude prompts from the watch).

This is the single source of truth. Firmware, the Mac app, and the Android app
all conform to it. If you need to change a message shape, edit THIS file and note
it, don't diverge silently.

## Transport (unchanged)
- Watch is the BLE **peripheral / GATT server**, Nordic UART Service:
  - Service `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
  - RX (write, central→watch) `6E400002-…`
  - TX (notify, watch→central) `6E400003-…`
  - Advertised name must start `Bangle.js` (currently `Bangle.js EMGOR`).
- **central→watch** framing: `\x10GB({...json...})\n` (0x10 DLE prefix, watch strips it). Raw-JS `setTime(...)` lines also still supported.
- **watch→central** framing: bare `{...json...}` + `\r\n`.
- Both the phone (Android) and the Mac connect as **centrals**. The watch keeps
  advertising after the first connects so the second can also find it.

## Sources
Every central identifies itself right after connecting:
```
central→watch:  GB({"t":"hello","src":"mac","name":"Emory's MacBook"})
central→watch:  GB({"t":"hello","src":"phone","name":"Pixel"})
```
`src` ∈ `"mac" | "phone"`. The watch remembers which BLE connection handle maps
to which `src`. If a central never sends `hello`, treat it as `src:"phone"`
(back-compat with the current Android app until it's updated).

## Battery (per-device)
```
central→watch:  GB({"t":"devBattery","src":"mac","n":<0..100>,"chg":<bool>})
```
Generalizes the old `phoneBattery`. The watch still accepts legacy
`{"t":"phoneBattery","n":..,"chg":..}` as `src:"phone"`. The Mac app sends
`devBattery src:"mac"`. The watch may show both device batteries (phone + mac)
alongside its own.

## Media + arbitration (the key UX)
Media messages gain a `src` field:
```
central→watch:  GB({"t":"musicinfo","src":"mac","artist":..,"album":..,"track":..,"ab":<bool>})
central→watch:  GB({"t":"musicstate","src":"mac","state":"play"|"pause","position":<sec>})
```
`ab` = audiobook/podcast (drives rewind/forward + sleep-timer UI).

**Active-source rule (watch decides which source the play screen shows):**
1. If exactly one source's last `musicstate` is `"play"`, that source is active.
2. If several are playing, the one whose `musicstate`/`musicinfo` updated most recently.
3. If none is playing, the source whose `musicinfo` updated most recently
   (so "was played last" wins — resume/rewind/timer target it).
The play screen shows the active source's track + a small **MAC / PHONE** badge,
and audiobook/timer controls reflect that source's `ab` flag.

**Transport routing (watch→central) is now TARGETED, not broadcast.** The watch
sends transport/commands ONLY to the active source's connection handle:
```
watch→active:  {"t":"music","n":"play"|"pause"|"next"|"previous"|"rewind"|"forward"|"volumeup"|"volumedown"}
```
The watch-local audiobook sleep timer (already implemented) fires its auto-`pause`
to the **active source** too.

Other existing watch→phone messages (`findPhone`, `notify` DISMISS/REPLY,
`camera`) stay as-is but route to the relevant source (notifications/camera →
`phone`; if only Mac is present, → Mac).

## Remote permission approval ("the super key")
Lets a Claude permission prompt on the Mac (or elsewhere) pop a YES/NO card on the
watch; the tap is relayed back to approve/deny.
```
central→watch:  GB({"t":"permReq","id":<int>,"title":"Approve tool use?","body":"<tool + short summary>","origin":"mac"})
watch→central:  {"t":"permResp","id":<int>,"approve":<bool>,"scope":"once"|"always"}
```
- The watch shows a **distinct** full-screen card (different accent from a normal
  notification) with **YES** and **NO** buttons. It is NOT gated by the
  notification-suppression window — permission prompts always show.
- **Scopes:** `"once"` (this call only), `"session"` (approve everything for the
  rest of that requesting window/session), `"always"` (persist forever). The
  CURRENT watch UI is a single full-screen TAP that sends `scope:"session"`
  (there is no NO button — not tapping = no answer = requester times out). The
  hook maps session→per-session-id auto-allow, always→persistent cache.
- `permResp` goes back to the connection that sent the `permReq` (by `id`).
- If the watch is disconnected/asleep the requester falls back to its normal
  on-screen prompt (never blocks forever — use a timeout).

**Queue + cancel (multiple simultaneous requests, e.g. 3 parallel windows):**
The watch QUEUES permReqs and shows one full-screen card at a time; each tap
answers that request's own id/handle so the right window unblocks, then the next
card appears. If a request is resolved elsewhere (e.g. approved from the Mac menu
bar) the requester sends a cancel so the watch drops it:
```
central→watch:  GB({"t":"permCancel","id":<int>})
```
The watch removes the matching (handle,id); if that card was showing it advances
to the next queued request (or closes). Keeps the Mac menu and watch card in sync.

## IDs
`id` fields are 32-bit ints chosen by the sender; the watch echoes them back so
the sender can correlate responses. Notification ids and permReq ids share no
namespace requirement but should not collide within a source.

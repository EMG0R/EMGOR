# smartGOR Network Speed Log

Room network measurements. Topology at time of testing: main house WiFi → GL.iNet travel
router (repeating/piggybacking the main WiFi with its better radio) → room devices.
Pi (`gorcave`) is wired to the GL.iNet over ethernet; WiFi clients join the GL.iNet SSID
`YOUR_WIFI_SSID`.

## 2026-08-05 baseline

| # | Path | Download | Upload | Latency | Tool |
|---|------|----------|--------|---------|------|
| 1 | Pi → ethernet → GL.iNet → (WiFi hop) → main router | 60.2 Mbps | 29.4 Mbps | 33 ms | speedtest-cli (Ookla) |
| 2 | Mac → GL.iNet WiFi → (WiFi hop) → main router | 41.8 Mbps | 5.75 Mbps | 19 ms | Google speed test (M-Lab, Los Angeles) |
| 3 | Mac → main WiFi directly (no GL.iNet) | ~18 Mbps | ~2 Mbps | — | (user-reported, same Google test) |

## Analysis (2026-08-05)

- **The GL.iNet repeater is a big net win.** The Mac gets ~2.3× the download (41.8 vs 18)
  and ~2.9× the upload (5.75 vs 2) going through the GL.iNet instead of joining the main
  WiFi directly. The GL.iNet's radio/placement reaches the main router far better than the
  Mac's own radio does.
- **Room's internet ceiling ≈ 60/30**, as measured from the Pi. The Pi sees the best
  numbers because ethernet to the GL.iNet removes the second WiFi hop entirely — its only
  wireless leg is the GL.iNet's (strong) link to the main router.
- **Cost of the second WiFi hop:** Mac-over-GL.iNet loses ~30% download and ~80% upload vs
  the wired Pi. The upload drop is disproportionately large — likely the Mac↔GL.iNet WiFi
  leg plus half-duplex airtime contention (the repeater shares one radio hop with the
  backhaul); different test tools/servers also contribute, so don't over-read the exact gap.
- **Practical implications:** keep the Pi (and any future always-on nodes that can take a
  cable) on ethernet to the GL.iNet; point room WiFi devices at `YOUR_WIFI_SSID`, not
  the main SSID. 60/30 is far more than the smart-room stack needs (MQTT, chat backend,
  Claude API, STT audio are all sub-1 Mbps); headroom exists even for HD streaming.
- **Caveats:** single-sample measurements, mixed tools (Ookla vs M-Lab), different test
  servers — treat as a baseline, not a benchmark. Re-test after any router/topology change
  and append below.

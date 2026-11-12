# Chiller Ambient Preset

Slow generative saw voices through reverb + granular shimmer + spectral smear.
SDL parked 1 tick from self-oscillation at shortest time — thick, endless wash.

## Activation sequence (2.5s between each effect)

All effects live on **menu 4** (TIME).

1. Navigate to menu 4: CC73 val127 ×4
2. Clock ON: CC68 val127 held 6s → CC68 val0
3. **Reverb ON**: CC76 val127 — wait 2.5s
4. **Granular ON**: CC71 val127 — wait 2.5s
5. **Granular mix 100%**: CC21 val=65 ×60 steps
6. **Spectral Delay ON**: CC75 val127 (sets lastToggle=16) — wait 2.5s
7. **SDL mix 100%**: CC25 val=65 ×60 steps
8. **SDL maxdel shortest**: CC20 val=63 ×200 steps
9. **SDL feedback 0.99**: CC23 val=65 ×19 steps (0.8 init + 19×0.01 = 0.99)

## MIDI reference

| Step              | CC  | Val | Count | Notes                                    |
|-------------------|-----|-----|-------|------------------------------------------|
| Menu 4 nav ×4     |  73 | 127 |  ×4   | each press = next menu                   |
| Clock ON (hold)   |  68 | 127 |  ×1   | hold 6s then CC68 val0 release           |
| Reverb            |  76 | 127 |  ×1   | menu4 slot2                              |
| Granular          |  71 | 127 |  ×1   | menu4 slot3                              |
| Granular mix 100% |  21 |  65 | ×60   | CW encoder steps (init 0.5, step 0.01)  |
| Spectral Delay    |  75 | 127 |  ×1   | menu4 slot1; sets lastToggle=16          |
| SDL mix 100%      |  25 |  65 | ×60   | CW encoder steps (init 0.5, step 0.01)  |
| SDL maxdel min    |  20 |  63 | ×200  | CCW steps (init 0.3, step 0.002)        |
| SDL FB 0.99       |  23 |  65 | ×19   | CW steps (init 0.8, step 0.01)          |

## Notes

- CC75/76/71/72 map to **different effects per menu** — must be on correct menu.
- SDL self-oscillation threshold is FB=1.0; 0.99 = 1 tick away = intentional thick wash.
- SDL processing character sounds unusual at entry — correct, not an artifact.
- Reverb + granular (100% mix) + SDL (100% mix, shortest time, near-max FB) = infinite thick wash.
- Chuck generates 16th-note saw voices at 124 BPM.
- **Validated 2026-04-24**

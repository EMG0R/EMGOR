# NEPTR mixer and per-device channel map

Channel count and routing are PER NODE, stored outside the instrument tree so a
sync or deploy never overwrites them and the codebase is never forked:

| file | content | effect |
|---|---|---|
| `~/.config/neptr/mode` | `stereo` (default) or `8ch` | wrapper passes `NEPTR_NCHNLS=8 / NEPTR_NCHNLS_I=8`; absent = nchnls 2 as always |
| `~/.config/neptr/routing` | `bus ch [ch...]` lines | wrapper turns the mixer on and passes `MIXER_*` macros |

`routing` lines (1-based, `#` comments, many channels per bus allowed):

    main_l 1 5      # NEPTR main L to outs 1 and 5
    main_r 2 6
    rack_l 1        # physical input(s) feeding the rack's L / R
    rack_r 2
    direct 3 7 8    # input 3 straight to outs 7 and 8

No file = main on outs 1-2, inputs 1-2 into the rack (bit-identical to before).
In `8ch` without `routing`, outs 3-8 are silent. `uss` is a separate engine and is
not reachable from the in-process matrix (sync_layer=off).

`mixer.orc` (macro `MIXER=1`) is a full N x N matrix, N = nchnls (cap 12 by the
256-slot pool table). Live pool params (0..1, unity 0.75, 0..0.75 = -60..0 dB,
0.75..1 = 0..+12 dB, mute >= 0.5): `/mixer/in/<n>/{gain,mute}`,
`/mixer/in/<n>/to/{rackL,rackR}`, `/mixer/in/<n>/to/out/<k>`,
`/mixer/out/<k>/{gain,mute}`, `/mixer/out/<k>/from/{mainL,mainR}`; meters back via
`/pout` as `/mixer/{in,out}/<n>/{peak,rms}` (0..1 = (dBFS+60)/60). Pool port 9030,
stage `mixer`. Needs `--env:INCDIR+=<resources>` (the wrapper adds it).

Turn on 8ch: `echo 8ch > ~/.config/neptr/mode`, restart demiurge. ALSA card must
expose 8 channels (sndrpihifiberry dac8x does).

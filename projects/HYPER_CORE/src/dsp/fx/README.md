# HYPER_CORE/src/dsp/fx -- HYPERTRUMPET V2 FX library

Portable C++17 (host, WASM, Daisy Seed). float32, block processing, no heap in the audio path.

## Contract (every effect)
- Header-only `fx_<name>.h`: `struct Fx<Name> : hx::Fx` + `inline hx::Fx* fx_create_<id>(hx::Arena&, float sr)`. Registered in `fx_registry.cpp`.
- Ctor: define params with `P(i, "NAME", min, max, def, step, flags, enumNames)`; **param 0 is MIX**; names <= 8 chars, upper-case.
- Declare macro roles with `setRole(R_AMT, idx)` etc. (>= 2). Roles: AMT intensity/depth, TIME time/size/rate, TONE brightness/damping, MOD motion/warp, FB feedback, PITCH interval/frequency. The mapper drives any effect in any slot through roles.
- Use `sm[i]` (block-smoothed) in DSP; per-sample interpolate where zipper would be audible. Quantised/enum params (PF_QUANT/PF_ENUM/PF_INT) are not smoothed.
- `process(ctx, L, R, n, stereoIn)`: in place. `stereoIn=false`: only L is valid, produce stereo only if useful and return true; if `stereoIn` is true you MUST return true. Dry/wet via MIX with a per-block ramp (`DryWet`).
- Large buffers: `Arena::floats(n)` (zeroed). If it returns nullptr set `ok=false` and pass audio through; never crash. Total per-effect memory must fit HX_SLOT_ARENA_BYTES (12 MB). Seed SDRAM placement is by arena (`HX_SDRAM` macro in fx_common.h for the engine's arena); do not use static big arrays.
- No heap, no std::function, no per-sample virtuals, no `double` in the audio loop, no denormals (use `flush()`), clamp feedback < 1.
- `action(id, value)`: A_MAIN/A_ALT/A_CLEAR/A_HOLD, effect-specific; document in status json.
- Credit third-party algorithms in the file header (McCurdy CC BY-NC-SA 4.0 for flanger + grain delay).

## Per effect deliverables
1. `fx_<id>.h` (replace the stub). 2. `tests/test_fx_<id>.cpp` calling `hxt::stdSuite(...)` plus effect-specific checks (impulse/sine response, no NaN/denormal, bounded, param sweep). 3. `status/<id>.json`:
`{"id","file","roles":{"AMT":"PARAM",...},"actions":{"MAIN":"..."},"ops_per_sample":"hand count + reasoning","memory_bytes":N,"notes":"..."}`.
Run: `make test` (all) or `make out/test_<id> && ./out/test_<id>`.
`status/gen_status.mjs` (node) builds HYPERTRUMPET_V2/docs/FX_STATUS.md from test results, `make bench` timings and the json files.

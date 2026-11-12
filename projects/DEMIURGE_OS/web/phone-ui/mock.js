// MOCK DATA ONLY. Shapes mirror the planned real feeds; swap for fetch/WebSocket (see README).
// Every param value is a 0..1 float. Patch id == folder name (per-patch index.html).
window.MOCK = {
  patches: [
    {id: "SLIDE_GUITAR", note: "Csound / NEPTR FX chain", active: true},
    {id: "GAMELAN_BELLS", note: "ChucK / granular"},
    {id: "HYPER_DRONE",   note: "RNBO / hyperGuitar"},
    {id: "LYDIAN_PAD",    note: "Faust"},
    {id: "BLANK_NEW",     note: "auto-generated blank index.html"}
  ],
  // per-patch parameter pool (name -> 0..1)
  params: {
    SLIDE_GUITAR:  {vol: .72, drive: .35, delay_mix: .4,  reverb: .55, wah: 0},
    GAMELAN_BELLS: {grain_size: .3, density: .6, pitch: .5, spread: .8},
    HYPER_DRONE:   {formant: .5, resonance: .45, sitar: .1, vibrato: .2},
    LYDIAN_PAD:    {cutoff: .6, detune: .25, attack: .4},
    BLANK_NEW:     {}
  },
  // devices publishing inputs (Bela hyperGuitar, Daisy PMOR, NEPTR body)
  devices: [
    {id: "bela",  name: "BELA hyperGuitar", ok: true,  inputs: ["str1","str2","str3","str4","str5","str6"]},
    {id: "daisy", name: "DAISY PMOR",       ok: true,  inputs: ["lidar","fsr","imu_x","imu_y","joy_x","joy_y"]},
    {id: "body",  name: "NEPTR BODY",       ok: false, inputs: ["enc1","enc2","enc3","enc4","expr"]}
  ],
  mappings: [
    {src: "daisy/lidar", dst: "SLIDE_GUITAR/delay_mix"},
    {src: "bela/str1",   dst: "SLIDE_GUITAR/drive"}
  ]
};

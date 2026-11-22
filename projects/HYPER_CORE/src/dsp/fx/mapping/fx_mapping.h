// fx_mapping.h -- v2 control -> FX mapping, shared by the firmware and the web simulator. INTERFACE FIXED (core, 2026-11-15).
// Implementation: mapping/fx_mapping.cpp (mapping agent). Spec of what each control does: HYPERTRUMPET_V2/docs/FX_MAP.md section 4.
#pragma once
#include "../fx_engine.h"
namespace hx {

struct HxControls {                 // all normalised, filled by the HAL (hardware) or the sim UI
    float valve[3] = {0,0,0};       // 0 = up, 1 = fully pressed (Hall depth)
    float fader[3] = {0,0,0};       // 0..1
    float pot[2]   = {0,0};         // 0..1
    float fsr      = 0;             // 0..1 (already through fsr_scale curve)
    float joyX = 0, joyY = 0;       // -1..1, centre = 0 (deadzone applied by HAL)
    bool  joyClick = false;         // level (held)
    int   encDelta[2] = {0,0};      // detents since last update (+cw)
    bool  encClick[2] = {false,false};   // level (held)
    float tiltX = 0, tiltY = 0;     // -1..1 from MPU6050 accel (X = roll left/right, Y = pitch bell up/down)
    float shake = 0;                // 0..1 gyro magnitude
    float lidar = 0;                // 0..1, 1 = hand close to the bell, 0 = far / no hand
    uint16_t pads = 0;              // NeoTrellis key levels, bit n = key n (row*4+col), held
};

struct HxLeds { uint8_t rgb[16][3]; };   // Trellis LED frame the mapper wants shown (0..255, firmware applies its own brightness cap)

class HxMapper {
public:
    void init(FxEngine* e);                       // loads default slot setup (see FX_MAP.md "default patch"), registers pad layout
    void update(const HxControls& c, float dtSec);// call at control rate (>= 200 Hz). Edge-detects pads/clicks internally. Writes params/actions/bypass into the engine.
    const HxLeds& leds() const { return leds_; }  // current pad colours
    // read-only status for UIs
    int   page() const { return page_; }          // pad page / mode
    int   selectedSlot() const { return selSlot_; }
    int   selectedParam() const { return selParam_; }
    const char* statusLine() const { return status_; }
    // programmatic (UI / protocol) hooks
    void  selectSlot(int s);
    void  loadDefaultPatch();
    // f0 hint from valves: Bb trumpet fingering logic is done by the host and passed to engine via setValveHint; mapper may read engine->f0().
private:
    FxEngine* eng_ = nullptr; HxLeds leds_{}; int page_ = 0, selSlot_ = 0, selParam_ = 0; char status_[48] = "";
    HxControls prev_{}; float t_ = 0;
    struct Impl; Impl* impl_ = nullptr;           // mapping state lives in a static pool (no heap)
};

} // namespace hx

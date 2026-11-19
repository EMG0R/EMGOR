// TEST 11_bench -- effect CPU benchmark on a spare Daisy Seed. No sensors, strip, trellis or ADC. NO QSPI WRITES (no PersistentStorage, no param saves, no flash writes of any kind).
// 48 kHz / 48-sample blocks. A synthetic trumpet-like signal (12 harmonics, vibrato, 15 ms attack, 60 ms release, breath noise, 300 ms silences between notes) goes through the
// firmware's real chain (fx_glue_chain.h: the same gate, template and effects as test 10) with ONE effect on at a time (mix 1.0), then idle, then all on. DWT cycles are
// measured around fxChain() only (the signal generator is outside the timing). Each state runs 5 s: 0.5 s warm-up discarded, 4.5 s measured. Output stays silent (zeros).
// Serial (USB CDC, newline JSON):  {"t":"bench","pass":N,"fx":"reverb","n":4500,"avg":cycles,"worst":cycles,"avgpct":x10,"worstpct":x10}  per state,
//   {"t":"bench_table","pass":N,"clk":Hz,"blk":48,"budget":cycles per block,"rows":[["idle",avg,worst],...]}  once per pass, forever. Commands: hello, dfu (to the Daisy bootloader).
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "daisy_seed.h"
using namespace daisy;
using namespace daisy::seed;
static DaisySeed hw;
// ---- output helpers (the glue headers print through these; the bench itself uses them for its JSON)
static uint8_t g_buf[2400]; static size_t g_len = 0;
static void put(char c) { if(g_len < sizeof(g_buf) - 1) g_buf[g_len++] = (uint8_t)c; }
static void puts_(const char* s) { while(*s) put(*s++); }
static void putu(uint32_t v) { char t[11]; int n = 0; do { t[n++] = '0' + v % 10; v /= 10; } while(v); while(n) put(t[--n]); }
static void putd(int32_t v) { if(v < 0) { put('-'); v = -v; } putu((uint32_t)v); }
static volatile float g_dly_ay = 0.0f;
static void sendOut() { uint32_t t0 = System::GetNow(); while(hw.usb_handle.TransmitInternal(g_buf, g_len) != UsbHandle::Result::OK) { if(System::GetNow() - t0 > 50) break; } g_len = 0; }
#include "fx_glue_chain.h"
// ---- states
static const int NS = GLUE_N + 3;                      // states: idle, one per registry effect, all, combo (harmony 3 voices + chorus + reverb: the user's target combination, must stay under 60 % peak)
static char kNameBuf[GLUE_N + 3][12];
static const char* kName[GLUE_N + 3];
static volatile int g_state = 0; static volatile bool g_collect = false;
static volatile uint32_t g_n = 0, g_sum = 0, g_max = 0; static volatile uint64_t g_sum64 = 0;
// ---- signal
static float g_tab[2049];
static void sigInit() { for(int i = 0; i <= 2048; i++) { float x = 0; for(int h = 1; h <= 12; h++) x += sinf(6.2831853f * h * i / 2048.0f) / powf((float)h, 0.8f); g_tab[i] = x * 0.18f; } }
static void sigBlock(float* o, int n) {
    static float ph = 0, vph = 0, env = 0; static uint32_t t = 0, rng = 12345; static const float notes[6] = { 293.66f, 329.63f, 392.0f, 440.0f, 493.88f, 392.0f };
    for(int i = 0; i < n; i++, t++) {
        const uint32_t per = 48000;                                  // 1 s per note: 0.7 s sound, 0.3 s silence
        const uint32_t ti = t % per; const int note = (t / per) % 6;
        const float f0 = notes[note] * (1.0f + 0.003f * sinf(vph)); vph += 6.2831853f * 5.5f / 48000.0f; if(vph > 6.2831853f) vph -= 6.2831853f;
        const float target = (ti < 33600) ? 1.0f : 0.0f; env += (target - env) * (target > env ? 0.0014f : 0.00035f);       // ~15 ms attack, ~60 ms release
        ph += f0 / 48000.0f; if(ph >= 1.0f) ph -= 1.0f;
        const float fi = ph * 2048.0f; const int ip = (int)fi; const float fr = fi - (float)ip; const float s = g_tab[ip] + (g_tab[ip + 1] - g_tab[ip]) * fr;
        rng = rng * 1664525u + 1013904223u; const float nz = ((float)(rng >> 8) * (1.0f / 16777216.0f) - 0.5f) * 0.02f;
        o[i] = env * (s + nz);
    }
}
static void audioCb(AudioHandle::InputBuffer, AudioHandle::OutputBuffer out, size_t size) {
    static float sig[48], oL[48], oR[48]; const size_t n = size > 48 ? 48 : size;
    sigBlock(sig, (int)n);
    const uint32_t a = DWT->CYCCNT; fxChain(sig, oL, oR, n); const uint32_t c = DWT->CYCCNT - a;
    if(g_collect) { g_n = g_n + 1; g_sum64 = g_sum64 + c; if(c > g_max) g_max = c; }
    for(size_t i = 0; i < size; i++) { out[0][i] = 0.0f; out[1][i] = 0.0f; }          // silent
}
static void setState(int s) {
    for(int gi = 0; gi < GLUE_N; gi++) { g_fxg[gi].latch = false; g_fxg[gi].hold = false; g_extmix[gi] = 0.0f; }
    drGlueControls(0.0f);
    auto on = [](int gi) { if(gi == GI_DRONE) { drGlueControls(1.0f); return; } FxG& g = g_fxg[gi]; if(!g.ok) return; g.mix = 1.0f; g.latch = true; };
    if(s >= 1 && s <= GLUE_N) on(s - 1); else if(s == GLUE_N + 1) for(int k = 0; k < GLUE_N; k++) on(k); else if(s == GLUE_N + 2) { on(GI_HARMONY); on(GI_CHORUS); on(GI_REVERB); }
    { FxG& h = g_fxg[GI_HARMONY]; if(h.ok) { hx::FxHarmony* hm = static_cast<hx::FxHarmony*>(h.fx); hm->setSlot(0, 4); hm->setSlot(1, 7); hm->setSlot(2, 10); } }       // harmony: 3 voices (+4, +7, +10) whenever it is on
    g_state = s;
}
static volatile bool g_dfu = false, g_hello = false;
static void rxCb(uint8_t* buf, uint32_t* len) { static char line[64]; static uint8_t n = 0; for(uint32_t i = 0; i < *len; i++) { char c = (char)buf[i]; if(c == '\r') continue; if(c == '\n') { line[n] = 0; if(strstr(line, "dfu")) g_dfu = true; else if(strstr(line, "hello")) g_hello = true; n = 0; } else if(n < 62) line[n++] = c; } }
int main(void) {
    hw.Init(true);                                          // 480 MHz, as the instrument; qspi stays memory-mapped read-only (nothing here writes it)
    hw.usb_handle.Init(UsbHandle::FS_INTERNAL); hw.usb_handle.SetReceiveCallback(rxCb, UsbHandle::FS_INTERNAL);
    System::Delay(500);
    sigInit(); fxGlueInit(); setState(0);
    kName[0] = "idle"; for(int i = 0; i < GLUE_N; i++) { strncpy(kNameBuf[i + 1], g_glue[i].id ? g_glue[i].id : "?", 11); kName[i + 1] = kNameBuf[i + 1]; } kName[GLUE_N + 1] = "all"; kName[GLUE_N + 2] = "combo";
    hw.SetAudioBlockSize(48); hw.StartAudio(audioCb);
    static uint32_t rows[GLUE_N + 3][2]; uint32_t pass = 0; const uint32_t budget = (uint32_t)(48.0f * g_cyc_per_sample);
    for(;;) {
        for(int s = 0; s < NS; s++) {
            g_collect = false; setState(s); g_n = 0; g_sum64 = 0; g_max = 0;
            const uint32_t t0 = System::GetNow(); bool collecting = false; bool loopA = false, loopB = false, loopC = false;
            while(System::GetNow() - t0 < 5000) {
                const uint32_t e = System::GetNow() - t0;
                if(!collecting && e >= 500) { g_n = 0; g_sum64 = 0; g_max = 0; g_collect = true; collecting = true; }
                if(s == GI_LOOPER + 1 || s == GLUE_N + 1) {                          // looper script: record 1.5 s, close, overdub one pass
                    if(!loopA && e >= 200) { loopA = true; fxgAct(GI_LOOPER, hx::A_MAIN, 1.0f); }
                    if(!loopB && e >= 1700) { loopB = true; fxgAct(GI_LOOPER, hx::A_MAIN, 1.0f); }
                    if(!loopC && e >= 2400) { loopC = true; fxgAct(GI_LOOPER, hx::A_MAIN, 1.0f); }
                }
                if(g_hello) { g_hello = false; g_len = 0; puts_("{\"t\":\"hello\",\"test\":\"11_bench\",\"fw\":\"bench-1.0\",\"qspi_writes\":0,\"pass\":"); putu(pass); puts_(",\"state\":\""); puts_(kName[s]); puts_("\"}\n"); sendOut(); }
                if(g_dfu) { g_len = 0; puts_("{\"t\":\"dfu\"}\n"); sendOut(); System::Delay(100); System::ResetToBootloader(System::BootloaderMode::DAISY_INFINITE_TIMEOUT); }
            }
            g_collect = false; const uint32_t n = g_n; const uint64_t sum = g_sum64; const uint32_t mx = g_max; const uint32_t avg = n ? (uint32_t)(sum / n) : 0;
            rows[s][0] = avg; rows[s][1] = mx;
            g_len = 0; puts_("{\"t\":\"bench\",\"pass\":"); putu(pass); puts_(",\"fx\":\""); puts_(kName[s]); puts_("\",\"n\":"); putu(n); puts_(",\"avg\":"); putu(avg); puts_(",\"worst\":"); putu(mx);
            puts_(",\"avgpct\":"); putu((uint32_t)(1000.0f * avg / (float)budget)); puts_(",\"worstpct\":"); putu((uint32_t)(1000.0f * mx / (float)budget)); puts_("}\n"); sendOut();
        }
        g_len = 0; puts_("{\"t\":\"bench_table\",\"pass\":"); putu(pass); puts_(",\"clk\":"); putu((uint32_t)HAL_RCC_GetSysClockFreq()); puts_(",\"blk\":48,\"budget\":"); putu(budget); puts_(",\"rows\":[");
        for(int s = 0; s < NS; s++) { if(s) put(','); put('['); put('"'); puts_(kName[s]); put('"'); put(','); putu(rows[s][0]); put(','); putu(rows[s][1]); put(']'); }
        puts_("]}\n"); sendOut(); pass++;
    }
}

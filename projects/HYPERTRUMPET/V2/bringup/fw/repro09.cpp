// Minimal repro 09: does starting the audio freeze the ADC DMA buffer? Nothing else: hw.Init, USB CDC, ADC on A0..A2, optional audio passthrough, stream.
// Variants (see Makefile): REPRO_ORDER 0 = adc.Init/Start then StartAudio (canonical), 1 = StartAudio then adc; REPRO_OVS 32 or 0 (no oversampling); REPRO_AUDIO 1/0.
// Frame at 100 Hz: v from hw.adc.Get, buf read straight from the DMA buffer, live DMA/ADC registers, clock-tree registers, audio callback count, boot info.
#include <string.h>
#include "daisy_seed.h"
#include "hyper_hw.h"
void dfuCheck();
using namespace daisy;
DaisySeed hw;
#ifndef REPRO_ORDER
#define REPRO_ORDER 0
#endif
#ifndef REPRO_OVS
#define REPRO_OVS 32
#endif
#ifndef REPRO_AUDIO
#define REPRO_AUDIO 1
#endif
static volatile uint32_t g_cb = 0;
static volatile bool g_dfu = false;
extern "C" bool hyper_dfu_requested(void) { return g_dfu; }
static void rxCb(uint8_t* buf, uint32_t* len) { for(uint32_t i = 0; i + 2 < *len; i++) if(buf[i]=='d' && buf[i+1]=='f' && buf[i+2]=='u') g_dfu = true; }
[[maybe_unused]] static void audioCb(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    for(size_t i = 0; i < size; i++) { out[0][i] = in[0][i]; out[1][i] = in[0][i]; }
    g_cb = g_cb + 1;
}
static uint8_t g_tx[2][1024]; static int g_cur = 0; static size_t g_len = 0;
static void put(char c) { if(g_len < sizeof(g_tx[0]) - 1) g_tx[g_cur][g_len++] = (uint8_t)c; }
static void puts_(const char* s) { while(*s) put(*s++); }
static void putu(uint32_t v) { char t[11]; int n = 0; do { t[n++] = '0' + v % 10; v /= 10; } while(v); while(n) put(t[--n]); }
static void list(const uint32_t* a, int n) { put('['); for(int i = 0; i < n; i++) { if(i) put(','); putu(a[i]); } put(']'); }
static AdcChannelConfig g_cfg[3];
int main(void) {
    hw.Init();
    hw.usb_handle.Init(UsbHandle::FS_INTERNAL);
    hw.usb_handle.SetReceiveCallback(rxCb, UsbHandle::FS_INTERNAL);
    System::Delay(500);
    g_cfg[0].InitSingle(seed::A0, AdcChannelConfig::SPEED_64CYCLES_5); g_cfg[1].InitSingle(seed::A1, AdcChannelConfig::SPEED_64CYCLES_5); g_cfg[2].InitSingle(seed::A2, AdcChannelConfig::SPEED_64CYCLES_5);
#if REPRO_ORDER == 0
    hw.adc.Init(g_cfg, 3, REPRO_OVS ? AdcHandle::OVS_32 : AdcHandle::OVS_NONE); hw.adc.Start();
#if REPRO_AUDIO
    hw.SetAudioBlockSize(48); hw.StartAudio(audioCb);
#endif
#else
#if REPRO_AUDIO
    hw.SetAudioBlockSize(48); hw.StartAudio(audioCb);
#endif
    hw.adc.Init(g_cfg, 3, REPRO_OVS ? AdcHandle::OVS_32 : AdcHandle::OVS_NONE); hw.adc.Start();
#endif
    uint32_t seq = 0, next = System::GetNow();
    for(;;) {
        uint32_t now = System::GetNow();
        if(g_dfu) { System::Delay(100); dfuCheck(); g_dfu = false; }
        if((int32_t)(now - next) < 0) continue;
        next += 10; if((int32_t)(now - next) > 50) next = now + 10;
        g_len = 0; seq++;
        uint32_t v[3], b[3]; uint16_t* bp = hw.adc.GetPtr(0); for(int i = 0; i < 3; i++) { v[i] = hw.adc.Get(i); b[i] = bp[i]; }
        puts_("{\"t\":\"s\",\"n\":"); putu(seq); puts_(",\"ms\":"); putu(now); puts_(",\"v\":"); list(v, 3); puts_(",\"buf\":"); list(b, 3);
        uint32_t d[6] = { DMA1_Stream2->NDTR, ADC1->ISR, DMA1_Stream2->M0AR, g_cb, (uint32_t)System::GetBootloaderVersion(), (uint32_t)System::GetProgramMemoryRegion() };
        puts_(",\"d\":"); list(d, 6);
        uint32_t r[6] = { RCC->CR, RCC->PLLCKSELR, RCC->D2CCIP1R, RCC->D3CCIPR, RCC->PLL3DIVR, SCB->CCR };
        puts_(",\"rcc\":"); list(r, 6); puts_(",\"variant\":["); putu(REPRO_ORDER); put(','); putu(REPRO_OVS); put(','); putu(REPRO_AUDIO); puts_("]}\n");
        if(hw.usb_handle.TransmitInternal(g_tx[g_cur], g_len) == UsbHandle::Result::OK) g_cur ^= 1;
    }
}

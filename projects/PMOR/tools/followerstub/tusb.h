// Host stand-in for TinyUSB's tusb.h: ONLY the symbols usb/clock_engine.c touches. Implemented in tools/follower_sim.cpp
// (a byte-accurate 4-byte-packet RX FIFO; TX packets are counted and dropped).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
enum { MIDI_CIN_SYSCOM_3BYTE = 3, MIDI_CIN_CONTROL_CHANGE = 0xB, MIDI_CIN_1BYTE_DATA = 0xF };   // tusb midi.h values
#ifdef __cplusplus
extern "C" {
#endif
bool     tud_midi_mounted(void);
uint32_t tud_midi_available(void);                 // bytes queued (4 per packet)
bool     tud_midi_packet_read(uint8_t packet[4]);
bool     tud_midi_packet_write(uint8_t const packet[4]);
uint32_t tud_midi_stream_write(uint8_t cable, uint8_t const *buf, uint32_t n);
uint32_t tud_midi_n_stream_write(uint8_t itf, uint8_t cable, uint8_t const *buf, uint32_t n);
uint32_t tud_cdc_write(void const *buf, uint32_t n);
uint32_t tud_cdc_write_flush(void);
void     tud_task(void);
// defined by clock_engine.c itself, called by the sim in place of the dwc2 patch / TinyUSB:
void     dcd_rx_complete_hook(uint8_t epnum);
void     tud_midi_rx_cb(uint8_t itf);
void     tud_midi_tx_complete_cb(uint8_t itf, uint32_t xferred_bytes);
#ifdef __cplusplus
}
#endif

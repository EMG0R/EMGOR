// Host stub of the TinyUSB API surface usb/clock_engine.c uses (tests/test_follower.cpp drives the REAL clock_engine.c with it).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
bool     tud_midi_packet_write(uint8_t const packet[4]);
bool     tud_midi_packet_read(uint8_t packet[4]);
uint32_t tud_midi_available(void);
bool     tud_midi_mounted(void);
uint32_t tud_midi_stream_read(void* buf, uint32_t n);
uint32_t tud_cdc_write(void const* buf, uint32_t n);
uint32_t tud_cdc_write_flush(void);
void     tud_midi_rx_cb(uint8_t itf);
void     tud_midi_tx_complete_cb(uint8_t itf, uint32_t xferred_bytes);
#ifdef __cplusplus
}
#endif
enum { MIDI_CIN_MISC = 0, MIDI_CIN_CABLE_EVENT, MIDI_CIN_SYSCOM_2BYTE, MIDI_CIN_SYSCOM_3BYTE, MIDI_CIN_SYSEX_START, MIDI_CIN_SYSEX_END_1BYTE,
       MIDI_CIN_SYSEX_END_2BYTE, MIDI_CIN_SYSEX_END_3BYTE, MIDI_CIN_NOTE_OFF, MIDI_CIN_NOTE_ON, MIDI_CIN_POLY_KEYPRESS, MIDI_CIN_CONTROL_CHANGE,
       MIDI_CIN_PROGRAM_CHANGE, MIDI_CIN_CHANNEL_PRESSURE, MIDI_CIN_PITCH_BEND_CHANGE, MIDI_CIN_1BYTE_DATA };

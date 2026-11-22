// Analog channel groups for the bring-up tests (IO_LIST section 2). A test header
// lists groups; the order here IS the ADC order and the order in the JSON stream.
// X(name, kind, "Aeven/Dnn", seed_phys_pin, daisy::seed::Axx)
#pragma once
#define CH_FADERS(X) \
  X("FADER1", "slider", "A3/D18",  25, daisy::seed::A3) \
  X("FADER2", "slider", "A4/D19",  26, daisy::seed::A4) \
  X("FADER3", "slider", "A5/D20",  27, daisy::seed::A5)
#define CH_POTS(X) \
  X("POT1", "pot", "A6/D21", 28, daisy::seed::A6) \
  X("POT2", "pot", "A7/D22", 29, daisy::seed::A7)
#define CH_FSR(X) \
  X("FSR", "fsr", "A8/D23", 30, daisy::seed::A8)
#define CH_JOY(X) \
  X("JOYX", "joy", "A9/D24",  31, daisy::seed::A9) \
  X("JOYY", "joy", "A10/D25", 32, daisy::seed::A10)
// 2-wire (rheostat) pots: end lug -> +3V3A pin 21, wiper -> ADC pin, 47 k from the ADC pin to DGND pin 40. Vout = 3.3 * 47k / (47k + Rx).
#define CH_POTS2W(X) \
  X("POT1", "pot2w", "A6/D21", 28, daisy::seed::A6) \
  X("POT2", "pot2w", "A7/D22", 29, daisy::seed::A7)
// 49E linear Hall sensors on the three valves, powered from 3V3 analog, OUT -> ADC pin (optional 100 nF OUT to DGND). Rest is about mid-scale.
#define CH_HALLS(X) \
  X("VALVE1", "hall", "A0/D15", 22, daisy::seed::A0) \
  X("VALVE2", "hall", "A1/D16", 23, daisy::seed::A1) \
  X("VALVE3", "hall", "A2/D17", 24, daisy::seed::A2)

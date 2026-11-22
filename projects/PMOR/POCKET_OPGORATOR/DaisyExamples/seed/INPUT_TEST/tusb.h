// tusb.h — deliberate local stub, shadows the real TinyUSB header.
//
// oled_pmor.h #includes "tusb.h" because in PMOR it lives alongside a full
// TinyUSB composite. These bring-up tests use libDaisy's own CDC instead
// (interrupt-driven, no polling), and linking both USB stacks would collide.
//
// The only thing oled_pmor.h actually calls is pmor_usb_service(), which
// INPUT_TEST.cpp defines as a no-op. Everything else TinyUSB would provide is
// unused here, so an empty header is the correct shim.
//
// -I. comes first in C_INCLUDES so this wins over the real tinyusb/src/tusb.h.
#pragma once

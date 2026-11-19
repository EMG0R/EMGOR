/* HYPER_CORE import
 * origin : ~/Documents/_EMGOR_SYNTH/______2026NEW/Pocket-OpGorator/DaisyExamples/seed/PMOR_V3_live/hw/bbus.cpp
 * commit : PMOR_V3_live ff86e4c
 * changes: none (verbatim). tbus = NeoTrellis (D11 SCL, D12 SDA); sbus = sensors (D13/D14). Instrument may use only tbus
 * licence: Emory (own work) */
// bbus.cpp — the two bit-bang I2C bus instances (PMOR_V2.cpp L280–281).
#include "bbus.h"

BBus tbus; // NeoTrellis — D11=SCL, D12=SDA
BBus sbus; // Sensors    — D13=SCL, D14=SDA

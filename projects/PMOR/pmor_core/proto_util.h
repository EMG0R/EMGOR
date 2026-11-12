// proto_util.h -- CRCs, base64 and the tiny decimal float codec used by the CDC protocol (docs/PROTOCOL.md).
// Hardware-free; shared by the firmware, the host simulator and tools/pmorctl.
#pragma once
#include <stdint.h>
#include <stddef.h>

uint16_t pmorCrc16(const void* data, size_t n);                       // CRC-16/CCITT-FALSE
uint32_t pmorCrc32Update(uint32_t crc, const void* data, size_t n);   // CRC-32/IEEE, running (start 0xFFFFFFFF, finish ~)
static inline uint32_t pmorCrc32Of(const void* d, size_t n) { return ~pmorCrc32Update(0xFFFFFFFFu, d, n); }
uint32_t pmorFnv1a(uint32_t h, const void* data, size_t n);           // FNV-1a 32 (h = running, start 2166136261)

size_t pmorB64Encode(const uint8_t* in, size_t n, char* out);         // returns chars written (no NUL); out needs 4*((n+2)/3)
long   pmorB64Decode(const char* in, size_t n, uint8_t* out);         // bytes written, or -1 on bad input

// Decimal float: up to 6 fractional digits, no exponent. out >= 24 bytes. Returns length.
int    pmorFmtFloat(char* out, float v);
// Parses [-]digits[.digits] (also accepts e-notation). Returns false on junk.
bool   pmorParseFloat(const char* s, float* out);

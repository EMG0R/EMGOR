// proto_util.cpp -- see proto_util.h.
#include "proto_util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

uint16_t pmorCrc16(const void* data, size_t n) {
    const uint8_t* p = (const uint8_t*)data; uint16_t c = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        c ^= (uint16_t)p[i] << 8;
        for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}
uint32_t pmorCrc32Update(uint32_t crc, const void* data, size_t n) {
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
    }
    return crc;
}
uint32_t pmorFnv1a(uint32_t h, const void* data, size_t n) {
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
size_t pmorB64Encode(const uint8_t* in, size_t n, char* out) {
    size_t o = 0;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < n) v |= (uint32_t)in[i + 1] << 8;
        if (i + 2 < n) v |= in[i + 2];
        out[o++] = B64[(v >> 18) & 63]; out[o++] = B64[(v >> 12) & 63];
        out[o++] = (i + 1 < n) ? B64[(v >> 6) & 63] : '=';
        out[o++] = (i + 2 < n) ? B64[v & 63] : '=';
    }
    return o;
}
long pmorB64Decode(const char* in, size_t n, uint8_t* out) {
    if (n % 4) return -1;
    long o = 0;
    for (size_t i = 0; i < n; i += 4) {
        uint32_t v = 0; int pad = 0;
        for (int k = 0; k < 4; k++) {
            char c = in[i + k]; int d;
            if (c >= 'A' && c <= 'Z') d = c - 'A'; else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
            else if (c >= '0' && c <= '9') d = c - '0' + 52; else if (c == '+') d = 62; else if (c == '/') d = 63;
            else if (c == '=' && i + 4 == n && k >= 2) { d = 0; pad++; } else return -1;
            v = (v << 6) | (uint32_t)d;
        }
        out[o++] = (uint8_t)(v >> 16);
        if (pad < 2) out[o++] = (uint8_t)(v >> 8);
        if (pad < 1) out[o++] = (uint8_t)v;
    }
    return o;
}

int pmorFmtFloat(char* out, float v) {
    char* o = out;
    if (!(v == v)) { *o++ = '0'; *o = 0; return 1; }              // NaN -> 0
    if (v < 0.f) { *o++ = '-'; v = -v; }
    if (v > 4.0e9f) v = 4.0e9f;
    double d = (double)v;
    uint64_t scaled = (uint64_t)(d * 1000000.0 + 0.5);
    uint32_t ip = (uint32_t)(scaled / 1000000u), fp = (uint32_t)(scaled % 1000000u);
    if (ip == 0 && fp == 0 && o != out) { o = out; }             // "-0" -> "0"
    char t[12]; int n = 0;
    do { t[n++] = (char)('0' + ip % 10); ip /= 10; } while (ip);
    while (n) *o++ = t[--n];
    if (fp) {
        char f[6]; for (int i = 5; i >= 0; i--) { f[i] = (char)('0' + fp % 10); fp /= 10; }
        int e = 6; while (e > 0 && f[e - 1] == '0') e--;
        *o++ = '.'; for (int i = 0; i < e; i++) *o++ = f[i];
    }
    *o = 0;
    return (int)(o - out);
}
bool pmorParseFloat(const char* s, float* out) {
    if (!s || !*s) return false;
    char* e = nullptr;
    double v = strtod(s, &e);
    if (e == s || *e) return false;
    if (!(v == v) || fabs(v) > 1e12) return false;
    *out = (float)v;
    return true;
}

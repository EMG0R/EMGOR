#include "io/profile.h"

#include <algorithm>
#include <cstdlib>

namespace nv {

const char* ioTransportName(IoTransport transport) {
    switch (transport) {
        case IoTransport::NDI: return "ndi";
        case IoTransport::RTP: return "rtp";
        case IoTransport::SRT: return "srt";
        case IoTransport::RIST: return "rist";
        case IoTransport::WebRTC: return "webrtc";
        case IoTransport::Raw: return "raw";
    }
    return "rtp";
}

IoTransport ioTransportFromName(const std::string& name) {
    if (name == "ndi") return IoTransport::NDI;
    if (name == "rtp") return IoTransport::RTP;
    if (name == "srt") return IoTransport::SRT;
    if (name == "rist") return IoTransport::RIST;
    if (name == "webrtc") return IoTransport::WebRTC;
    if (name == "raw") return IoTransport::Raw;
    return IoTransport::RTP;
}

IoProfile ioProfileFromEnv(const IoProfile& defaults) {
    IoProfile out = defaults;
    if (const char* v = std::getenv("NV_IO_TRANSPORT"))
        out.transport = ioTransportFromName(v);
    if (const char* v = std::getenv("NV_IO_WIDTH")) out.width = std::atoi(v);
    if (const char* v = std::getenv("NV_IO_HEIGHT")) out.height = std::atoi(v);
    if (const char* v = std::getenv("NV_IO_FPS")) out.fps = std::atoi(v);
    if (const char* v = std::getenv("NV_IO_BITRATE")) out.bitrateKbps = std::atoi(v);
    if (const char* v = std::getenv("NV_IO_LATENCY")) out.latencyMs = std::atoi(v);
    if (const char* v = std::getenv("NV_IO_AUTO")) out.autoAdapt = std::atoi(v) != 0;
    out.width = std::clamp(out.width, 16, 7680);
    out.height = std::clamp(out.height, 16, 4320);
    out.fps = std::clamp(out.fps, 1, 240);
    out.bitrateKbps = std::clamp(out.bitrateKbps, 64, 500000);
    out.latencyMs = std::clamp(out.latencyMs, 0, 10000);
    return out;
}

IoProfile adaptIoProfile(const IoProfile& requested, const IoMetrics& metrics) {
    IoProfile out = requested;
    if (!requested.autoAdapt) return out;

    const float frameBudget = requested.fps > 0 ? 1000.0f / requested.fps : 33.3f;
    const bool congested = metrics.queueDepth > 2 || metrics.droppedFrames > 0 ||
                           metrics.encodeMs > frameBudget ||
                           (metrics.linkMbps > 0.0f &&
                            requested.bitrateKbps > metrics.linkMbps * 1000.0f * 0.85f);
    if (!congested) return out;

    if (out.fps > 30) {
        out.fps = 30;
    } else if (out.fps > 24) {
        out.fps = 24;
    } else if (out.width > 640 || out.height > 360) {
        // Preserve the requested aspect while stepping through practical
        // transport sizes. Never touch the unified internal render size.
        if (out.width >= 1600 || out.height >= 900) {
            out.width = 1280;
            out.height = 720;
        } else if (out.width >= 1000 || out.height >= 560) {
            out.width = 960;
            out.height = 540;
        } else {
            out.width = 640;
            out.height = 360;
        }
    } else if (out.bitrateKbps > 256) {
        out.bitrateKbps = std::max(256, out.bitrateKbps * 4 / 5);
    }
    return out;
}

}  // namespace nv

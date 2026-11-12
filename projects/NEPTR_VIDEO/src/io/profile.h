#pragma once

#include <string>

namespace nv {

// Boundary transport only. The render graph remains one size; these values
// describe the copy/encode/send edge for a future NDI/RTP/SRT/WebRTC adapter.
enum class IoTransport { NDI, RTP, SRT, RIST, WebRTC, Raw };

struct IoProfile {
    IoTransport transport = IoTransport::RTP;
    int width = 1280;
    int height = 720;
    int fps = 30;
    int bitrateKbps = 8000;
    int latencyMs = 80;
    bool autoAdapt = true;
};

struct IoMetrics {
    int queueDepth = 0;
    int droppedFrames = 0;
    float encodeMs = 0.0f;
    float linkMbps = 0.0f;
};

const char* ioTransportName(IoTransport transport);
IoTransport ioTransportFromName(const std::string& name);

// Reads only boundary settings (NV_IO_*). It never changes the internal
// render scale or output mode; those remain engine/platform responsibilities.
IoProfile ioProfileFromEnv(const IoProfile& defaults = {});

// Makes at most one quality concession per call, in deliberate order:
// framerate, boundary resolution, then codec bitrate. Internal resolution is
// never part of this decision.
IoProfile adaptIoProfile(const IoProfile& requested, const IoMetrics& metrics);

}  // namespace nv

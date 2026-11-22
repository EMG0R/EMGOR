#include "io/profile.h"
#include "check.h"

using namespace nv;

namespace {

void testManualProfileDoesNotMove() {
    IoProfile p;
    p.autoAdapt = false;
    p.width = 1920;
    p.height = 1080;
    p.fps = 60;
    IoProfile out = adaptIoProfile(p, {20, 4, 100.0f, 10.0f});
    CHECK_EQ(out.width, 1920);
    CHECK_EQ(out.height, 1080);
    CHECK_EQ(out.fps, 60);
}

void testAdaptationOrder() {
    IoProfile p;
    p.width = 1920;
    p.height = 1080;
    p.fps = 60;
    p.bitrateKbps = 8000;
    IoMetrics bad{4, 1, 30.0f, 10.0f};

    IoProfile fps = adaptIoProfile(p, bad);
    CHECK_EQ(fps.fps, 30);
    CHECK_EQ(fps.width, 1920);

    IoProfile resolution = adaptIoProfile(fps, bad);
    CHECK_EQ(resolution.fps, 24);
    CHECK_EQ(resolution.width, 1920);

    IoProfile reduced = adaptIoProfile(resolution, bad);
    CHECK_EQ(reduced.width, 1280);
    CHECK_EQ(reduced.height, 720);
    CHECK_EQ(reduced.bitrateKbps, 8000);
}

void testTransportNames() {
    CHECK(std::string(ioTransportName(IoTransport::NDI)) == "ndi");
    CHECK(std::string(ioTransportName(IoTransport::WebRTC)) == "webrtc");
    CHECK(ioTransportFromName("srt") == IoTransport::SRT);
    CHECK(ioTransportFromName("not-a-transport") == IoTransport::RTP);
}

}  // namespace

int main() {
    testManualProfileDoesNotMove();
    testAdaptationOrder();
    testTransportNames();
    return 0;
}

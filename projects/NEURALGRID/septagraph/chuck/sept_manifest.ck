// sept_manifest.ck
// Septagraph v1 reference renderer -- manifest announcer (Task 2.1).
//
// Announces a "continuous" manifest (spec WIRE_FORMAT_v1.md SS B) on start
// and whenever sept_render_osc.ck signals manifest_requested (a /sept/v1/
// manifest/request arrived on 7700). Runs as its own file/shred so it never
// blocks the fast-plane OscIn loop in sept_render_osc.ck.
//
// Wire detail: WIRE_FORMAT_v1.md says a controller's codec.py sends
// /sept/v1/manifest/* as one OSC *bundle* (begin, bounds, end). ChucK's
// OscOut only builds single messages, so this sends begin/bounds/end as
// three consecutive bare messages on the same UDP socket. This is fine:
// septagraph.codec.Decoder (Python) and the future sept_codec.js decoder
// are both *stateful* (Decoder.feed accumulates begin -> bounds/points ->
// end across separate packets, see codec.py's module docstring and
// test_manifest_across_packets_and_begin_invalidates), so three ordered
// messages resolve to the same Manifest object a bundle would.
//
// KNOWN DEVIATION from the plan text: the plan says "manifest reply to the
// sender's IP". ChucK 1.5.5.5's OscIn/OscMsg API does not expose the
// sender's source address, so this renderer cannot learn the requester's
// IP. Instead it always replies to 127.0.0.1:7701 (covers the laptop demo
// and septagraph.conformance.check_renderer, which listens on that host)
// and best-effort broadcasts to 255.255.255.255:7701 (covers the LAN
// demo). A future revision could add a --reply-to host argument if a
// specific non-loopback, non-broadcast target is needed.

"sept-render-chuck" => string RENDERER_ID;
"sept-render-chuck" => string RENDERER_NAME;
7701 => int CTL_PORT;

global Event manifest_requested;

OscOut xmitLoopback;
xmitLoopback.dest("127.0.0.1", CTL_PORT);

OscOut xmitBroadcast;
xmitBroadcast.dest("255.255.255.255", CTL_PORT);

fun void sendVia(OscOut xmit) {
    xmit.start("/sept/v1/manifest/begin");
    RENDERER_ID => xmit.add;
    "continuous" => xmit.add;
    0 => xmit.add;              // n_points (continuous manifests carry none)
    RENDERER_NAME => xmit.add;
    xmit.send();

    xmit.start("/sept/v1/manifest/bounds");
    RENDERER_ID => xmit.add;
    for (0 => int i; i < 8; i++) { 0.0 => xmit.add; }   // lo = [0]*8
    for (0 => int i; i < 8; i++) { 1.0 => xmit.add; }   // hi = [1]*8
    xmit.send();

    xmit.start("/sept/v1/manifest/end");
    RENDERER_ID => xmit.add;
    xmit.send();
}

fun void announce() {
    sendVia(xmitLoopback);
    sendVia(xmitBroadcast);
}

// announce on start
announce();

// announce again on every manifest/request
while (true) {
    manifest_requested => now;
    announce();
}

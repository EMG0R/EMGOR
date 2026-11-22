//! Reference test vectors (spec §12) — seed of the third-party conformance
//! suite. `generate_reference_vectors` deterministically (re)writes
//! `vectors/*.bin` + `vectors/README.md`; the decode tests read the files
//! back and assert every field.

use std::path::PathBuf;

use omniplex_wire::{
    decode_frame, encode_frame, negotiate, ChannelDescriptor, ChannelId, ChannelKind, ClockProbe,
    ClockReply, DeliveryClass, FormatDescriptor, FrameHeader, FrameType, Hello, HelloAck,
    LatencyClass, PixelFormat, SampleFormat, SessionTimestamp, FLAG_END_OF_MESSAGE,
    MAX_FRAGMENT_PAYLOAD, PROTOCOL_ALPN,
};

fn vectors_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("vectors")
}

fn sample_channels() -> Vec<ChannelDescriptor> {
    vec![
        ChannelDescriptor {
            id: ChannelId(2),
            name: "midi/1".into(),
            kind: ChannelKind::Midi,
            delivery: DeliveryClass::ReliableOrdered,
            latency: LatencyClass::LatencyCritical,
            format: FormatDescriptor::Midi,
        },
        ChannelDescriptor {
            id: ChannelId(4),
            name: "audio/main".into(),
            kind: ChannelKind::Audio,
            delivery: DeliveryClass::UnreliableTimestamped,
            latency: LatencyClass::LatencyCritical,
            format: FormatDescriptor::Audio {
                sample_rate: 48_000,
                channels: 2,
                format: SampleFormat::F32,
            },
        },
        ChannelDescriptor {
            id: ChannelId(6),
            name: "video/test".into(),
            kind: ChannelKind::Video,
            delivery: DeliveryClass::UnreliableTimestamped,
            latency: LatencyClass::Bulk,
            format: FormatDescriptor::Video {
                width: 1280,
                height: 720,
                fps_num: 30,
                fps_den: 1,
                pixel: PixelFormat::Uyvy,
            },
        },
    ]
}

fn hello_vector() -> Hello {
    Hello {
        version_min: 1,
        version_max: 1,
        instance_id: [
            0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD,
            0xEE, 0xFF,
        ],
        channels: sample_channels(),
    }
}

fn hello_ack_vector() -> HelloAck {
    HelloAck {
        version: 1,
        clock_master: true,
        channels: sample_channels(),
    }
}

/// (file name, header, payload, description) for every frame-level vector.
fn frame_vectors() -> Vec<(&'static str, FrameHeader, Vec<u8>, &'static str)> {
    vec![(
        "frame_hello.bin",
        FrameHeader {
            channel: ChannelId::CONTROL,
            frame_type: FrameType::Hello,
            flags: FLAG_END_OF_MESSAGE,
            timestamp: SessionTimestamp(0),
            seq: 0,
            frag_index: 0,
            frag_count: 1,
        },
        postcard::to_allocvec(&hello_vector()).unwrap(),
        "HELLO on control channel 0: postcard-encoded Hello (v1..=1, instance id 00112233…eeff, 3 channels: midi/1, audio/main 48k/2ch/F32, video/test 1280x720@30 UYVY)",
        ),

        (
        "frame_hello_ack.bin",
        FrameHeader {
            channel: ChannelId::CONTROL,
            frame_type: FrameType::HelloAck,
            flags: FLAG_END_OF_MESSAGE,
            timestamp: SessionTimestamp(0),
            seq: 1,
            frag_index: 0,
            frag_count: 1,
        },
        postcard::to_allocvec(&hello_ack_vector()).unwrap(),
        "HELLO-ACK on control channel 0: postcard-encoded HelloAck (version 1, clock_master=true, same 3 channels)",
        ),

        (
        "frame_media_single.bin",
        FrameHeader {
            channel: ChannelId(2),
            frame_type: FrameType::Media,
            flags: FLAG_END_OF_MESSAGE,
            timestamp: SessionTimestamp(1_000_000_000),
            seq: 7,
            frag_index: 0,
            frag_count: 1,
        },
        vec![0x90, 0x3C, 0x64], // MIDI note-on
        "single-fragment media frame on channel 2 (MIDI note-on 90 3C 64), ts=1s, seq=7, end_of_message set",
        ),

        (
        "frame_media_fragment.bin",
        FrameHeader {
            channel: ChannelId(6),
            frame_type: FrameType::Media,
            flags: 0, // not the last fragment
            timestamp: SessionTimestamp(0x0011_2233_4455_6677),
            seq: 0x0102_0304,
            frag_index: 1,
            frag_count: 3,
        },
        (0..64u8).collect(),
        "middle fragment (1 of 3) of a multi-fragment video message on channel 6; flags=0; payload = bytes 0x00..0x3F",
        ),

        (
        "frame_media_max_payload.bin",
        FrameHeader {
            channel: ChannelId(4),
            frame_type: FrameType::Media,
            flags: FLAG_END_OF_MESSAGE,
            timestamp: SessionTimestamp(42),
            seq: 0xFFFF_FFFF,
            frag_index: 0xFFFE,
            frag_count: 0xFFFF,
        },
        (0..MAX_FRAGMENT_PAYLOAD).map(|i| (i % 251) as u8).collect(),
        "maximum-size payload (16384 bytes, byte i = i mod 251) exercising the MAX_FRAGMENT_PAYLOAD bound and max field values",
        ),

        (
        "frame_clock_probe.bin",
        FrameHeader {
            channel: ChannelId::CLOCK,
            frame_type: FrameType::ClockProbe,
            flags: FLAG_END_OF_MESSAGE,
            timestamp: SessionTimestamp(0),
            seq: 3,
            frag_index: 0,
            frag_count: 1,
        },
        postcard::to_allocvec(&ClockProbe { t1_ns: 123_456_789 }).unwrap(),
        "clock probe on channel 1: postcard-encoded ClockProbe { t1_ns: 123456789 }",
        ),

        (
        "frame_clock_reply.bin",
        FrameHeader {
            channel: ChannelId::CLOCK,
            frame_type: FrameType::ClockReply,
            flags: FLAG_END_OF_MESSAGE,
            timestamp: SessionTimestamp(0),
            seq: 3,
            frag_index: 0,
            frag_count: 1,
        },
        postcard::to_allocvec(&ClockReply {
            t1_ns: 123_456_789,
            t2_ns: 123_500_000,
            t3_ns: 123_600_000,
        })
        .unwrap(),
        "clock reply on channel 1: postcard-encoded ClockReply { t1: 123456789, t2: 123500000, t3: 123600000 }",
        ),

        (
        "frame_bye.bin",
        FrameHeader {
            channel: ChannelId::CONTROL,
            frame_type: FrameType::Bye,
            flags: FLAG_END_OF_MESSAGE,
            timestamp: SessionTimestamp(2_000_000_000),
            seq: 99,
            frag_index: 0,
            frag_count: 1,
        },
        Vec::new(),
        "BYE on control channel 0 with empty payload",
        ),
    ]
}

fn write_vectors() {
    let dir = vectors_dir();
    std::fs::create_dir_all(&dir).unwrap();

    let mut readme = String::from(
        "# omniplex-wire reference test vectors\n\n\
         Normative byte-exact vectors for the Omniplex v1 wire format (spec \u{a7}6, \u{a7}12).\n\
         Regenerated by `cargo test -p omniplex-wire --test vectors`.\n\n\
         Frame header (little-endian, 22 bytes):\n\
         `u16 channel_id | u8 frame_type | u8 flags | u64 timestamp_ns | u32 seq | u16 frag_index | u16 frag_count | u16 payload_len | payload...`\n\n\
         Frame types: 0 Hello, 1 HelloAck, 2 ChannelOpen, 3 ChannelOpenAck, 4 ChannelClose,\n\
         5 Media, 6 ClockProbe, 7 ClockReply, 8 Bye. Flags bit 0 = end_of_message.\n\
         Control payloads are postcard-serialized. ALPN: `omniplex/1`.\n\n## Files\n\n",
    );

    for (name, header, payload, desc) in frame_vectors() {
        let mut buf = Vec::new();
        encode_frame(&header, &payload, &mut buf).unwrap();
        std::fs::write(dir.join(name), &buf).unwrap();
        readme.push_str(&format!("- `{name}`: {desc}\n"));
    }

    // Bare control payloads (postcard bytes only, no frame header).
    let hello = postcard::to_allocvec(&hello_vector()).unwrap();
    std::fs::write(dir.join("control_hello.bin"), &hello).unwrap();
    readme.push_str(
        "- `control_hello.bin`: bare postcard Hello payload (identical to frame_hello.bin payload)\n",
    );
    let ack = postcard::to_allocvec(&hello_ack_vector()).unwrap();
    std::fs::write(dir.join("control_hello_ack.bin"), &ack).unwrap();
    readme.push_str(
        "- `control_hello_ack.bin`: bare postcard HelloAck payload (identical to frame_hello_ack.bin payload)\n",
    );

    std::fs::write(dir.join("README.md"), readme).unwrap();
}

/// Guards `write_vectors()` so it runs exactly once no matter how many of
/// this file's tests execute concurrently (the default Rust test harness
/// runs tests within a binary on multiple threads) — without this, a
/// decode test can read a vector file while the generator is still
/// mid-write, since `write_vectors()` writes files one at a time, not
/// atomically as a set.
fn ensure_vectors_written() {
    static ONCE: std::sync::OnceLock<()> = std::sync::OnceLock::new();
    ONCE.get_or_init(write_vectors);
}

/// Generator: deterministically (re)writes every vector file.
#[test]
fn generate_reference_vectors() {
    ensure_vectors_written();
    assert!(vectors_dir().join("README.md").exists());
}

/// Decode-verify: read every vector file from disk and assert expected fields.
#[test]
fn decode_all_frame_vectors() {
    ensure_vectors_written();
    let dir = vectors_dir();

    for (name, expected_header, expected_payload, _) in frame_vectors() {
        let bytes = std::fs::read(dir.join(name)).unwrap();
        let (header, payload) =
            decode_frame(&bytes).unwrap_or_else(|e| panic!("{name}: decode failed: {e}"));
        assert_eq!(header, expected_header, "{name}: header mismatch");
        assert_eq!(
            payload,
            expected_payload.as_slice(),
            "{name}: payload mismatch"
        );
        // Vector files contain exactly one frame — no trailing bytes.
        assert_eq!(bytes.len(), 22 + payload.len(), "{name}: trailing bytes");
    }
}

/// Decode-verify the HELLO payloads semantically, including version negotiation.
#[test]
fn decode_hello_vectors() {
    ensure_vectors_written();
    let dir = vectors_dir();

    let bytes = std::fs::read(dir.join("control_hello.bin")).unwrap();
    let hello: Hello = postcard::from_bytes(&bytes).unwrap();
    assert_eq!(hello, hello_vector());
    assert_eq!(hello.channels.len(), 3);
    assert_eq!(hello.channels[0].name, "midi/1");

    let bytes = std::fs::read(dir.join("control_hello_ack.bin")).unwrap();
    let ack: HelloAck = postcard::from_bytes(&bytes).unwrap();
    assert_eq!(ack, hello_ack_vector());

    assert_eq!(
        negotiate(
            (hello.version_min, hello.version_max),
            (hello.version_min, hello.version_max)
        ),
        Some(ack.version)
    );
    assert_eq!(PROTOCOL_ALPN, b"omniplex/1");
}

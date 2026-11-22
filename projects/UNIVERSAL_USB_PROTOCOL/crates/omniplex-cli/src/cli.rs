//! `clap` CLI surface (Task 7.1) + `omniplex info` (pure, always compiled — no
//! session/mux dependency, so it's unit-testable even with
//! `--no-default-features`).

use clap::{Args, Parser, Subcommand, ValueEnum};

#[derive(Parser, Debug)]
#[command(name = "omniplex", version, about = "Omniplex companion app")]
pub struct Cli {
    #[command(subcommand)]
    pub command: Command,
}

#[derive(Subcommand, Debug)]
pub enum Command {
    /// Start a session and bridge local devices to channels.
    Run(RunArgs),
    /// Load-test / benchmark tooling.
    Bench(BenchArgs),
    /// Print build/version info.
    Info,
}

#[derive(Args, Debug, Clone, Default)]
pub struct RunArgs {
    /// Bridge a virtual "Omniplex In"/"Omniplex Out" MIDI port pair to `midi/1`.
    #[arg(long)]
    pub midi: bool,
    /// Bridge OSC over UDP: `recv_port:send_port` on localhost.
    #[arg(long, value_parser = parse_osc_ports, value_name = "RECV:SEND")]
    pub osc: Option<(u16, u16)>,
    /// Bridge a serial device (e.g. /dev/tty.usbserial-*) to `serial/1`.
    #[arg(long, value_name = "DEV")]
    pub serial: Option<String>,
    /// Bridge the default cpal input/output devices to `audio/main`.
    #[arg(long)]
    pub audio: bool,
    /// cpal input/output stream buffer size in frames. Lower values reduce
    /// latency (e.g. 64 frames @ 48 kHz = 1.33 ms/side) but raise underrun
    /// risk on weaker CPUs (e.g. a Raspberry Pi); raise this if you see
    /// audio glitches or `output_ring_underruns` climbing in the metrics
    /// table.
    #[arg(long, value_name = "N", default_value_t = 64)]
    pub audio_buffer_frames: u32,
    /// Send a synthetic UYVY test pattern on `video/test`. Optional
    /// `WxH@fps` override (default `1280x720@30`).
    #[arg(long, num_args = 0..=1, default_missing_value = "1280x720@30", value_name = "WxH@fps")]
    pub video_test: Option<String>,
    /// Capture the Pi's own screen and send it on `video/screen` (BGRA)
    /// instead of the synthetic test pattern — mutually exclusive with
    /// `--video-test`. Linux-only (this bridges a Raspberry Pi's own
    /// display); on other platforms `run` fails fast with a clear error.
    /// Grammar (see `bridge::screen_capture::parse_screen_capture`):
    ///   full[@WxH][@fps]           e.g. full, full@30, full@1920x1080@30
    ///   region:X,Y,W,H[@fps]       e.g. region:0,0,1280,720@15
    ///   window:NAME[@fps]          e.g. window:Firefox@20 (XComposite)
    #[arg(long, value_name = "SPEC", conflicts_with = "video_test")]
    pub screen_capture: Option<String>,
    /// Write received `video/test`/`video/screen` raw frames to stdout (for
    /// `ffplay -f rawvideo -pixel_format uyvy422|bgra -video_size WxH -`).
    #[arg(long)]
    pub video_stdout: bool,
}

fn parse_osc_ports(s: &str) -> Result<(u16, u16), String> {
    let (recv, send) = s
        .split_once(':')
        .ok_or_else(|| format!("expected RECV:SEND, got {s:?}"))?;
    let recv: u16 = recv.parse().map_err(|e| format!("bad recv port: {e}"))?;
    let send: u16 = send.parse().map_err(|e| format!("bad send port: {e}"))?;
    Ok((recv, send))
}

#[derive(Args, Debug)]
pub struct BenchArgs {
    #[command(subcommand)]
    pub command: BenchCommand,
}

#[derive(Subcommand, Debug)]
pub enum BenchCommand {
    /// 1 kHz MIDI echo RTT, idle vs. video-saturated link (spec §11 headline).
    MidiUnderLoad(MidiUnderLoadArgs),
}

#[derive(ValueEnum, Clone, Copy, Debug, PartialEq, Eq)]
pub enum BenchRole {
    A,
    B,
}

#[derive(Args, Debug, Clone)]
pub struct MidiUnderLoadArgs {
    /// Which side of a two-Mac run this process plays. Ignored with `--loopback`.
    #[arg(long, value_enum, default_value = "a")]
    pub role: BenchRole,
    /// Run both roles in-process over a bandwidth-capped `LoopbackTransport`.
    #[arg(long)]
    pub loopback: bool,
    /// Soak mode: run all bridges for `hours`, assert zero audio underruns,
    /// log RSS periodically, instead of the idle/loaded RTT comparison.
    #[arg(long, value_name = "HOURS")]
    pub soak: Option<f64>,
}

/// `omniplex info` body. Pure string, no I/O — trivially unit-tested.
pub fn info_text() -> String {
    format!(
        "omniplex {} \u{2014} Omniplex companion app\n\
         protocol ALPN: {}\n\
         max wire fragment: {} bytes\n\
         bridges: midi, osc, serial, audio, video (test pattern)\n\
         session wiring compiled in: {}\n",
        env!("CARGO_PKG_VERSION"),
        String::from_utf8_lossy(omniplex_wire::PROTOCOL_ALPN),
        omniplex_wire::MAX_FRAGMENT_PAYLOAD,
        cfg!(feature = "wiring"),
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn info_text_smoke() {
        let t = info_text();
        assert!(t.contains("omniplex "));
        assert!(t.contains("omniplex/1"));
        assert!(t.contains("16384"));
    }

    #[test]
    fn osc_port_parsing() {
        assert_eq!(parse_osc_ports("8000:9000"), Ok((8000, 9000)));
        assert!(parse_osc_ports("nope").is_err());
        assert!(parse_osc_ports("8000").is_err());
    }

    #[test]
    fn cli_parses_run_subcommand() {
        let cli = Cli::parse_from([
            "omniplex",
            "run",
            "--midi",
            "--osc",
            "9000:9001",
            "--video-test",
            "640x360@15",
        ]);
        match cli.command {
            Command::Run(args) => {
                assert!(args.midi);
                assert_eq!(args.osc, Some((9000, 9001)));
                assert_eq!(args.video_test.as_deref(), Some("640x360@15"));
            }
            _ => panic!("expected Run"),
        }
    }

    #[test]
    fn cli_parses_screen_capture_flag() {
        let cli = Cli::parse_from(["omniplex", "run", "--screen-capture", "full@1920x1080@30"]);
        match cli.command {
            Command::Run(args) => {
                assert_eq!(args.screen_capture.as_deref(), Some("full@1920x1080@30"));
                assert_eq!(args.video_test, None);
            }
            _ => panic!("expected Run"),
        }
    }

    #[test]
    fn cli_screen_capture_and_video_test_are_mutually_exclusive() {
        let result = Cli::try_parse_from([
            "omniplex",
            "run",
            "--video-test",
            "640x360@15",
            "--screen-capture",
            "full",
        ]);
        assert!(result.is_err());
    }

    #[test]
    fn cli_parses_bench_subcommand() {
        let cli = Cli::parse_from(["omniplex", "bench", "midi-under-load", "--loopback"]);
        match cli.command {
            Command::Bench(BenchArgs {
                command: BenchCommand::MidiUnderLoad(a),
            }) => {
                assert!(a.loopback);
                assert_eq!(a.role, BenchRole::A);
            }
            _ => panic!("expected Bench"),
        }
    }
}

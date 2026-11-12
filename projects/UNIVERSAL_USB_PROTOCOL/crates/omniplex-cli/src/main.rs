//! `omniplex` companion app entrypoint (Task 7.1). Thin: all logic lives in the
//! `omniplex_cli` library crate so it stays testable independent of this binary.

use clap::Parser;
use omniplex_cli::cli::{Cli, Command};

#[cfg(feature = "wiring")]
use omniplex_cli::{
    bridge::screen_capture::parse_screen_capture, bridge::video::parse_mode, cli::RunArgs,
    run::RunOptions,
};

fn main() {
    tracing_subscriber::fmt::init();
    let cli = Cli::parse();
    match cli.command {
        Command::Info => println!("{}", omniplex_cli::cli::info_text()),
        #[cfg(feature = "wiring")]
        Command::Run(args) => {
            if let Err(e) = run_main(args) {
                eprintln!("omniplex: fatal: {e}");
                std::process::exit(1);
            }
        }
        #[cfg(not(feature = "wiring"))]
        Command::Run(_) => wiring_disabled(),
        #[cfg(feature = "wiring")]
        Command::Bench(args) => {
            let omniplex_cli::cli::BenchArgs {
                command: omniplex_cli::cli::BenchCommand::MidiUnderLoad(a),
            } = args;
            match omniplex_cli::bench::run_midi_under_load(a) {
                Ok(passed) => std::process::exit(if passed { 0 } else { 1 }),
                Err(e) => {
                    eprintln!("omniplex bench: fatal: {e}");
                    std::process::exit(2);
                }
            }
        }
        #[cfg(not(feature = "wiring"))]
        Command::Bench(_) => wiring_disabled(),
    }
}

#[cfg(not(feature = "wiring"))]
fn wiring_disabled() -> ! {
    eprintln!(
        "omniplex: this binary was built with `--no-default-features` (no `wiring`); \
         `run`/`bench` need omniplex-session + omniplex-mux. Rebuild with the default features."
    );
    std::process::exit(2);
}

#[cfg(feature = "wiring")]
fn run_main(args: RunArgs) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let video = match args.video_test {
        Some(spec) => Some(
            parse_mode(&spec)
                .ok_or_else(|| format!("invalid --video-test value {spec:?} (want WxH@fps)"))?,
        ),
        None => None,
    };
    let screen_capture = match args.screen_capture {
        Some(spec) => Some(
            parse_screen_capture(&spec)
                .map_err(|e| format!("invalid --screen-capture value: {e}"))?,
        ),
        None => None,
    };
    let opts = RunOptions {
        midi: args.midi,
        osc: args.osc,
        serial: args.serial,
        audio: args.audio,
        audio_buffer_frames: args.audio_buffer_frames,
        video,
        screen_capture,
        video_stdout: args.video_stdout,
    };
    let rt = tokio::runtime::Runtime::new()?;
    rt.block_on(omniplex_cli::run::run(opts))
}

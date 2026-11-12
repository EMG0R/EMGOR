//! `omniplex-cli` library crate: the minimal companion app (Milestone 7).
//!
//! Split into a lib + thin bin so the pure-logic modules (audio DSP math,
//! video frame gen/decode, OSC address routing, metrics-table formatting,
//! bench statistics) always compile and stay unit-testable independent of
//! `omniplex-mux`/`omniplex-session`, which are gated behind the `wiring` feature (see
//! `Cargo.toml`). `cargo test -p omniplex-cli --no-default-features` runs exactly
//! that pure subset.

pub mod bridge;
pub mod cli;
pub mod stats;
pub mod ui;

#[cfg(feature = "wiring")]
pub mod bench;
#[cfg(feature = "wiring")]
pub mod run;

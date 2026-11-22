//! Session clock: PTP-style exchange + discipline filter (plan Milestone 4).
//!
//! - [`ClockFilter`] — pure-math discipline filter, no I/O, no cross-crate
//!   deps; fully unit-testable with `--no-default-features`.
//! - [`SessionClock`] (feature `proto`, default on) — clock protocol loop
//!   over a `omniplex_transport::TransportStream`.

mod filter;
mod time;

pub use filter::{ClockFilter, ClockSample, ClockStats};
pub use time::local_now_ns;

#[cfg(feature = "proto")]
mod proto;
#[cfg(feature = "proto")]
pub use proto::{LocalClock, SessionClock};

/// Re-exports so integration tests (and downstream test harnesses) can use
/// the exact dependency versions this crate was built against without
/// declaring them as dev-dependencies.
#[cfg(feature = "proto")]
pub mod reexports {
    pub use omniplex_transport;
    pub use omniplex_wire;
    pub use tokio;
}

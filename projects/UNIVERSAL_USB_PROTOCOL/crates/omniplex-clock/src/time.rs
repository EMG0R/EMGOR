//! Local monotonic time source (plan Task 4.2).
//!
//! `std::time::Instant` is `mach_absolute_time`-backed on macOS; this maps
//! it to a u64 nanosecond count since first use. Later cfg'd per OS if a
//! platform needs a different source.

use std::sync::OnceLock;
use std::time::Instant;

static EPOCH: OnceLock<Instant> = OnceLock::new();

/// Local monotonic time in nanoseconds since first call in this process.
pub fn local_now_ns() -> u64 {
    let epoch = *EPOCH.get_or_init(Instant::now);
    Instant::now().duration_since(epoch).as_nanos() as u64
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn monotonic_nonzero_resolution() {
        let a = local_now_ns();
        let b = local_now_ns();
        assert!(b >= a);
        std::thread::sleep(std::time::Duration::from_millis(2));
        let c = local_now_ns();
        assert!(c > b + 1_000_000, "expected >=1ms advance, got {}", c - b);
    }
}

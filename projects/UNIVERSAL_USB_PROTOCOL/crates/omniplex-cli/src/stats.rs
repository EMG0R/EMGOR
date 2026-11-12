//! Pure latency-distribution helpers for `omniplex bench` (Task 7.5).

/// Summary of an RTT sample set, nanoseconds.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RttSummary {
    pub count: usize,
    pub p50: u64,
    pub p90: u64,
    pub p99: u64,
    pub max: u64,
}

/// Nearest-rank percentile over a sorted slice.
pub fn percentile(sorted: &[u64], p: f64) -> u64 {
    assert!(!sorted.is_empty());
    let rank = ((p / 100.0) * sorted.len() as f64).ceil() as usize;
    sorted[rank.clamp(1, sorted.len()) - 1]
}

/// Summarize (sorts internally).
pub fn summarize(mut samples: Vec<u64>) -> Option<RttSummary> {
    if samples.is_empty() {
        return None;
    }
    samples.sort_unstable();
    Some(RttSummary {
        count: samples.len(),
        p50: percentile(&samples, 50.0),
        p90: percentile(&samples, 90.0),
        p99: percentile(&samples, 99.0),
        max: *samples.last().unwrap(),
    })
}

/// The headline spec check: loaded p99 must be within 2x idle p99 (§11).
pub fn passes_spec(idle: &RttSummary, loaded: &RttSummary) -> bool {
    loaded.p99 <= idle.p99.saturating_mul(2)
}

pub fn format_summary(label: &str, s: &RttSummary) -> String {
    let us = |ns: u64| ns as f64 / 1000.0;
    format!(
        "{label:<18} n={:<6} p50={:>9.1}us p90={:>9.1}us p99={:>9.1}us max={:>9.1}us",
        s.count,
        us(s.p50),
        us(s.p90),
        us(s.p99),
        us(s.max)
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn percentiles_nearest_rank() {
        let v: Vec<u64> = (1..=100).collect();
        assert_eq!(percentile(&v, 50.0), 50);
        assert_eq!(percentile(&v, 90.0), 90);
        assert_eq!(percentile(&v, 99.0), 99);
        assert_eq!(percentile(&v, 100.0), 100);
        assert_eq!(percentile(&[7], 99.0), 7);
    }

    #[test]
    fn summarize_and_spec_check() {
        let idle = summarize((1..=1000u64).collect()).unwrap();
        assert_eq!(idle.p99, 990);
        assert_eq!(idle.max, 1000);
        let loaded_ok = summarize((1..=1000u64).map(|x| x * 2).collect()).unwrap();
        assert!(passes_spec(&idle, &loaded_ok));
        let loaded_bad = summarize((1..=1000u64).map(|x| x * 3).collect()).unwrap();
        assert!(!passes_spec(&idle, &loaded_bad));
        assert!(summarize(vec![]).is_none());
    }
}

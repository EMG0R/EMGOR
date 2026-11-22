//! 1 Hz metrics table rendering (Task 7.1, spec §11 "shown in the app").
//! Pure formatting over plain rows; conversion from `MetricsHub::snapshot`
//! happens in `run.rs` so this stays unit-testable.

/// One row of the live metrics table.
#[derive(Clone, Debug)]
pub struct MetricsRow {
    pub channel: String, // e.g. "midi/1 (#2)"
    pub class: String,   // e.g. "rel-ord/critical"
    pub p50_us: f64,
    pub p99_us: f64,
    pub jitter_us: f64,
    pub drops: u64,
    /// Bits per second currently observed (tx+rx).
    pub bandwidth_bps: f64,
}

pub fn format_bandwidth(bps: f64) -> String {
    if bps >= 1e9 {
        format!("{:.2} Gb/s", bps / 1e9)
    } else if bps >= 1e6 {
        format!("{:.2} Mb/s", bps / 1e6)
    } else if bps >= 1e3 {
        format!("{:.1} kb/s", bps / 1e3)
    } else {
        format!("{bps:.0} b/s")
    }
}

/// Render the table; deterministic layout, one line per channel.
pub fn render_table(rows: &[MetricsRow]) -> String {
    let mut out = String::new();
    out.push_str(&format!(
        "{:<20} {:<18} {:>10} {:>10} {:>10} {:>8} {:>12}\n",
        "CHANNEL", "CLASS", "P50(us)", "P99(us)", "JITTER(us)", "DROPS", "BANDWIDTH"
    ));
    for r in rows {
        out.push_str(&format!(
            "{:<20} {:<18} {:>10.1} {:>10.1} {:>10.1} {:>8} {:>12}\n",
            r.channel,
            r.class,
            r.p50_us,
            r.p99_us,
            r.jitter_us,
            r.drops,
            format_bandwidth(r.bandwidth_bps)
        ));
    }
    if rows.is_empty() {
        out.push_str("(no channels open)\n");
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn table_contains_header_and_values() {
        let rows = vec![MetricsRow {
            channel: "midi/1 (#2)".into(),
            class: "rel-ord/critical".into(),
            p50_us: 123.4,
            p99_us: 456.7,
            jitter_us: 12.0,
            drops: 3,
            bandwidth_bps: 2_500_000.0,
        }];
        let t = render_table(&rows);
        assert!(t.contains("CHANNEL"));
        assert!(t.contains("midi/1 (#2)"));
        assert!(t.contains("123.4"));
        assert!(t.contains("2.50 Mb/s"));
        assert!(render_table(&[]).contains("no channels open"));
    }

    #[test]
    fn bandwidth_units() {
        assert_eq!(format_bandwidth(500.0), "500 b/s");
        assert_eq!(format_bandwidth(2_500.0), "2.5 kb/s");
        assert_eq!(format_bandwidth(3_200_000_000.0), "3.20 Gb/s");
    }
}

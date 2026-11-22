//! Serial bridge (Task 7.2): serial device <-> channel `serial/1`
//! (ReliableOrdered, Interactive). Device I/O is generic over `std::io`
//! Read/Write so the pump logic is unit-testable without hardware; the
//! bounded channels give publisher-blocks backpressure end to end (§9).

use std::io::{Read, Write};
use std::sync::mpsc::{Receiver, SyncSender, TrySendError};

/// Bound on in-flight chunks between the blocking pumps and the async side.
pub const SERIAL_QUEUE: usize = 64;
/// Read chunk size.
pub const SERIAL_CHUNK: usize = 1024;

/// Blocking loop: read from the serial port, push chunks into `out`.
/// Blocks (backpressure) when the queue is full. Returns on EOF or I/O error.
pub fn pump_reader<R: Read>(mut port: R, out: SyncSender<Vec<u8>>) {
    let mut buf = [0u8; SERIAL_CHUNK];
    loop {
        match port.read(&mut buf) {
            Ok(0) => return,
            Ok(n) => {
                // send() blocks when full: the serial reader stalls rather
                // than dropping bytes — reliable-ordered semantics.
                if out.send(buf[..n].to_vec()).is_err() {
                    return;
                }
            }
            Err(e) if e.kind() == std::io::ErrorKind::TimedOut => continue,
            Err(e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
            Err(_) => return,
        }
    }
}

/// Blocking loop: pull chunks from `input`, write them to the serial port.
/// Returns on channel close or I/O error.
pub fn pump_writer<W: Write>(mut port: W, input: Receiver<Vec<u8>>) {
    while let Ok(chunk) = input.recv() {
        if port.write_all(&chunk).is_err() {
            return;
        }
        let _ = port.flush();
    }
}

/// Non-blocking try-send helper used by tests to observe backpressure.
pub fn try_forward(out: &SyncSender<Vec<u8>>, chunk: Vec<u8>) -> Result<(), TrySendError<Vec<u8>>> {
    out.try_send(chunk)
}

/// A boxed, generic-I/O serial port split into its read and write halves.
pub type SerialHalves = (Box<dyn Read + Send>, Box<dyn Write + Send>);

/// Open a real serial port (dev path + baud) as a boxed Read/Write pair.
pub fn open_port(
    path: &str,
    baud: u32,
) -> Result<SerialHalves, Box<dyn std::error::Error + Send + Sync>> {
    let port = serialport::new(path, baud)
        .timeout(std::time::Duration::from_millis(100))
        .open()
        .map_err(|e| Box::new(e) as Box<dyn std::error::Error + Send + Sync>)?;
    let writer = port
        .try_clone()
        .map_err(|e| Box::new(e) as Box<dyn std::error::Error + Send + Sync>)?;
    Ok((Box::new(port), Box::new(writer)))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;
    use std::sync::mpsc::sync_channel;

    #[test]
    fn reader_pumps_all_bytes_in_order() {
        let data: Vec<u8> = (0..5000u32).map(|i| (i % 251) as u8).collect();
        let (tx, rx) = sync_channel(SERIAL_QUEUE);
        pump_reader(Cursor::new(data.clone()), tx);
        let mut got = Vec::new();
        while let Ok(chunk) = rx.try_recv() {
            got.extend(chunk);
        }
        assert_eq!(got, data);
    }

    #[test]
    fn writer_writes_chunks_in_order() {
        let (tx, rx) = sync_channel::<Vec<u8>>(SERIAL_QUEUE);
        tx.send(b"hello ".to_vec()).unwrap();
        tx.send(b"world".to_vec()).unwrap();
        drop(tx);
        let mut sink = Vec::new();
        pump_writer(&mut sink, rx);
        assert_eq!(sink, b"hello world");
    }

    #[test]
    fn full_queue_blocks_reader_backpressure() {
        let (tx, rx) = sync_channel::<Vec<u8>>(2);
        assert!(try_forward(&tx, vec![1]).is_ok());
        assert!(try_forward(&tx, vec![2]).is_ok());
        // Queue full: producer must block/refuse, never drop.
        assert!(matches!(
            try_forward(&tx, vec![3]),
            Err(TrySendError::Full(_))
        ));
        // Draining one slot unblocks.
        assert_eq!(rx.recv().unwrap(), vec![1]);
        assert!(try_forward(&tx, vec![3]).is_ok());
    }

    /// A reader that yields timeouts between chunks — pump must keep going.
    struct Choppy {
        chunks: Vec<Vec<u8>>,
        timeouts: usize,
    }
    impl Read for Choppy {
        fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
            if self.timeouts > 0 && self.chunks.len().is_multiple_of(2) {
                self.timeouts -= 1;
                return Err(std::io::Error::new(std::io::ErrorKind::TimedOut, "t"));
            }
            match self.chunks.pop() {
                Some(c) => {
                    buf[..c.len()].copy_from_slice(&c);
                    Ok(c.len())
                }
                None => Ok(0),
            }
        }
    }

    #[test]
    fn reader_survives_timeouts() {
        let (tx, rx) = sync_channel(SERIAL_QUEUE);
        pump_reader(
            Choppy {
                chunks: vec![b"b".to_vec(), b"a".to_vec()],
                timeouts: 3,
            },
            tx,
        );
        assert_eq!(rx.try_recv().unwrap(), b"a");
        assert_eq!(rx.try_recv().unwrap(), b"b");
    }
}

"""
Tests for cdc_engine.py.

Two layers, both genuinely exercised (not just written and assumed to
work):

1. `parse_ioreg_tree` / `resolve_port_path` -- pure text parsing over a
   fabricated `ioreg -p IOUSB -l -w0` dump, no subprocess or hardware
   needed. Proves port resolution finds the RIGHT device among several
   attached USB serial devices, by product-name match through
   device_match.matches (both "Pocket OpGorator" and "PMOR").

2. `CdcEngine` end-to-end against a real pty pair (`pty.openpty()`), with a
   background thread standing in for the firmware: it reads the exact
   command bytes the engine writes and replies with synthetic STATUS/DIAG
   text, so the parsing path (line reading, STATUS/DIAG block capture,
   generic key/value folding, named-field extraction, optimistic local
   state updates for SY/BEAT/BEATLVL/dfu) is driven by real bytes over a
   real serial-like file descriptor, not by calling internal methods
   directly.
"""
import os
import pty
import sys
import threading
import time
import tty
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cdc_engine import CdcEngine, parse_ioreg_tree, resolve_port_path, parse_kv_lines, extract_named_fields
from state import SharedState


FAKE_IOREG_DUMP = r"""
+-o Root  <class IORegistryEntry, id 0x100000100, retain 45>
  +-o AppleUSBXHCI@14000000  <class AppleUSBXHCI, id 0x100000200>
    +-o Some Other Gadget@14100000  <class AppleUSBHostDevice, id 0x1000003a0, registered, matched, active>
      {
        "USB Product Name" = "Some Other USB Gadget"
        "idVendor" = 4660
      }
      +-o IOUSBHostInterface@0  <class IOUSBHostInterface, id 0x1000003b0>
        {
          "bInterfaceClass" = 2
        }
        +-o AppleUSBACMControl  <class AppleUSBACMControl, id 0x1000003c0>
          +-o IOSerialBSDClient  <class IOSerialBSDClient, id 0x1000003d0, registered, matched, active>
            {
              "IOCalloutDevice" = "/dev/cu.usbmodem14440001"
              "IOTTYDevice" = "usbmodem14440001"
            }
    +-o Pocket OpGorator@14200000  <class AppleUSBHostDevice, id 0x100000af0, registered, matched, active>
      {
        "USB Product Name" = "Pocket OpGorator"
        "idVendor" = 4660
      }
      +-o IOUSBHostInterface@0  <class IOUSBHostInterface, id 0x100000b00, registered, matched, active>
        {
          "bInterfaceClass" = 2
        }
        +-o AppleUSBACMControl  <class AppleUSBACMControl, id 0x100000b10, registered, matched, active>
          +-o IOSerialBSDClient  <class IOSerialBSDClient, id 0x100000b20, registered, matched, active>
            {
              "IOCalloutDevice" = "/dev/cu.usbmodem14440001231"
              "IOTTYDevice" = "usbmodem14440001231"
            }
      +-o IOUSBHostInterface@1  <class IOUSBHostInterface, id 0x100000b30, registered, matched, active>
        {
          "bInterfaceClass" = 10
        }
"""

FAKE_IOREG_DUMP_RENAMED = FAKE_IOREG_DUMP.replace('"USB Product Name" = "Pocket OpGorator"',
                                                    '"USB Product Name" = "PMOR"')


class TestIoregParsing(unittest.TestCase):
    def test_finds_correct_device_among_several(self):
        pairs = parse_ioreg_tree(FAKE_IOREG_DUMP)
        products = [p for p, _ in pairs]
        self.assertIn("Some Other USB Gadget", products)
        self.assertIn("Pocket OpGorator", products)

    def test_resolve_picks_pmor_not_other_device(self):
        # Monkeypatch _run_ioreg indirectly by calling the pure pieces:
        # resolve_port_path calls _run_ioreg() -> subprocess, so test the
        # parse+match logic directly here (the subprocess call itself is a
        # thin, untestable-without-macOS wrapper).
        pairs = parse_ioreg_tree(FAKE_IOREG_DUMP)
        from device_match import matches
        match = next((path for product, path in pairs if matches(product)), None)
        self.assertEqual(match, "/dev/cu.usbmodem14440001231")

    def test_resolve_after_rename_to_pmor(self):
        pairs = parse_ioreg_tree(FAKE_IOREG_DUMP_RENAMED)
        from device_match import matches
        match = next((path for product, path in pairs if matches(product)), None)
        self.assertEqual(match, "/dev/cu.usbmodem14440001231")

    def test_override_bypasses_ioreg(self):
        self.assertEqual(resolve_port_path("/dev/cu.explicit"), "/dev/cu.explicit")

    def test_empty_text_no_crash(self):
        self.assertEqual(parse_ioreg_tree(""), [])


class TestKvParsing(unittest.TestCase):
    def test_colon_and_equals(self):
        fields = parse_kv_lines(["Sample Rate: 48000", "Role = 1", "not a kv line"])
        self.assertEqual(fields["sample rate"], "48000")
        self.assertEqual(fields["role"], "1")

    def test_extract_named_fields(self):
        fields = parse_kv_lines([
            "Sample Rate: 48000",
            "Role: 1",
            "BPM: 120.5",
            "Tick Count: 4800",
            "TX Count: 900",
            "RX Count: 12",
        ])
        named = extract_named_fields(fields)
        self.assertEqual(named["sample_rate"], 48000)
        self.assertEqual(named["role"], 1)
        self.assertEqual(named["bpm"], 120.5)
        self.assertEqual(named["tick_count"], 4800)
        self.assertEqual(named["tx_count"], 900)
        self.assertEqual(named["rx_count"], 12)

    def test_no_crash_on_garbage(self):
        fields = parse_kv_lines(["", "===", "!!!weird---", "STATUS"])
        named = extract_named_fields(fields)
        self.assertTrue(all(v is None for v in named.values()))


class FakeFirmware(threading.Thread):
    """Stands in for the real device on the other end of the pty. Reads
    newline-terminated commands and writes back synthetic responses that
    mimic what STATUS/DIAG/etc. would plausibly look like."""

    def __init__(self, master_fd):
        super().__init__(daemon=True)
        self.master_fd = master_fd
        self._stop = threading.Event()
        self.received_commands = []
        self._lock = threading.Lock()

    def stop(self):
        self._stop.set()

    def run(self):
        import select
        buf = b""
        while not self._stop.is_set():
            try:
                # select() with a short timeout, not a blocking os.read():
                # on macOS, closing master_fd from another thread does NOT
                # reliably unblock a thread parked in a blocking read() on
                # that fd, which would hang shutdown forever. Polling with
                # a timeout lets this thread notice self._stop / a closed
                # fd promptly instead.
                ready, _, _ = select.select([self.master_fd], [], [], 0.2)
                if not ready:
                    continue
                chunk = os.read(self.master_fd, 1024)
            except OSError:
                return
            if not chunk:
                return
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                cmd = line.decode("ascii", errors="replace").strip()
                if cmd:
                    with self._lock:
                        self.received_commands.append(cmd)
                    self._respond(cmd)

    def _respond(self, cmd):
        upper = cmd.upper()
        try:
            if upper == "STATUS":
                self._write("Sample Rate: 48000\r\n")
                self._write("Role: 1\r\n")
                self._write("BPM: 120.3\r\n")
                self._write("Tick Count: 2400\r\n")
                self._write("TX Count: 500\r\n")
                self._write("RX Count: 42\r\n")
            elif upper == "DIAG":
                self._write("Sample Rate: 48000\r\n")
                self._write("Role: 1\r\n")
                self._write("BPM: 120.4\r\n")
                self._write("Tick Count: 2401\r\n")
                self._write("TX Count: 501\r\n")
                self._write("RX Count: 43\r\n")
            elif upper.startswith("SY") or upper.startswith("BEAT") or upper == "DFU":
                self._write("OK\r\n")
        except OSError:
            pass

    def _write(self, s):
        os.write(self.master_fd, s.encode("ascii"))


class TestCdcEngineOverPty(unittest.TestCase):
    def setUp(self):
        self.master_fd, self.slave_fd = pty.openpty()
        # Put the pty pair in raw mode: real serial devices don't do
        # canonical-mode line buffering or echo, and CdcEngine/pyserial
        # assume raw semantics (byte-at-a-time reads honoring VTIME). A
        # freshly opened pty defaults to cooked-mode tty discipline, which
        # would make this test behave nothing like a real serial port.
        tty.setraw(self.master_fd)
        self.slave_path = os.ttyname(self.slave_fd)
        self.firmware = FakeFirmware(self.master_fd)
        self.firmware.start()
        self.state = SharedState()
        self.engine = CdcEngine(self.state, port_override=self.slave_path, diag_interval=0.3)
        self.engine.start()

    def tearDown(self):
        self.engine.stop()
        self.firmware.stop()
        self.firmware.join(timeout=2.0)
        for fd in (self.master_fd, self.slave_fd):
            if fd is None or fd < 0:
                continue
            try:
                os.close(fd)
            except OSError:
                pass

    def _wait_for(self, predicate, timeout=3.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if predicate():
                return True
            time.sleep(0.02)
        return False

    def test_connects(self):
        self.assertTrue(self._wait_for(lambda: self.state.cdc.connected))
        self.assertEqual(self.state.cdc.port_path, self.slave_path)

    def test_status_round_trip(self):
        self.assertTrue(self._wait_for(lambda: self.state.cdc.connected))
        self.engine.send_command("STATUS")
        self.assertTrue(self._wait_for(lambda: self.state.cdc.last_status_raw))
        self.assertIn("Sample Rate: 48000", self.state.cdc.last_status_raw)
        self.assertEqual(self.state.cdc.sample_rate, 48000)
        self.assertEqual(self.state.cdc.role, 1)
        self.assertAlmostEqual(self.state.cdc.bpm, 120.3, places=3)
        self.assertEqual(self.state.cdc.tick_count, 2400)
        self.assertEqual(self.state.cdc.tx_count, 500)
        self.assertEqual(self.state.cdc.rx_count, 42)

    def test_diag_auto_polls(self):
        self.assertTrue(self._wait_for(lambda: self.state.cdc.connected))
        # diag_interval=0.3s -- should fire on its own without us calling it
        self.assertTrue(self._wait_for(lambda: self.state.cdc.last_diag_raw, timeout=3.0))
        self.assertIn("Tick Count: 2401", self.state.cdc.last_diag_raw)
        self.assertTrue(self._wait_for(lambda: "DIAG" in self.firmware.received_commands))

    def test_sy_mode_optimistic_update(self):
        self.assertTrue(self._wait_for(lambda: self.state.cdc.connected))
        self.engine.set_sy_mode(3)
        self.assertTrue(self._wait_for(lambda: self.state.cdc.sy_mode == 3))

    def test_beat_toggle_and_level(self):
        self.assertTrue(self._wait_for(lambda: self.state.cdc.connected))
        self.engine.set_beat(True)
        self.assertTrue(self._wait_for(lambda: self.state.cdc.beat_on is True))
        self.engine.set_beat_level(64)
        self.assertTrue(self._wait_for(lambda: self.state.cdc.beat_level == 64))
        self.engine.set_beat(False)
        self.assertTrue(self._wait_for(lambda: self.state.cdc.beat_on is False))

    def test_dfu_command_reaches_firmware(self):
        self.assertTrue(self._wait_for(lambda: self.state.cdc.connected))
        self.engine.send_dfu()
        self.assertTrue(self._wait_for(lambda: "dfu" in self.firmware.received_commands))
        self.assertTrue(self.state.cdc.dfu_sent)

    def test_disconnect_detected(self):
        self.assertTrue(self._wait_for(lambda: self.state.cdc.connected))
        # Simulate unplug: close the firmware's end.
        self.firmware.stop()
        os.close(self.master_fd)
        self.master_fd = -1  # tearDown must not double-close
        self.assertTrue(self._wait_for(lambda: not self.state.cdc.connected, timeout=3.0))


class TestCdcEngineNoDevice(unittest.TestCase):
    """The critical "hardware not attached" case: everything must degrade
    to waiting, never crash, never raise out of a background thread."""

    def test_never_connects_but_does_not_crash(self):
        state = SharedState()
        # No override, and (almost certainly) no real Pocket OpGorator /
        # PMOR attached to whatever machine runs this test -- resolve_port_path
        # will return None via the real ioreg call, or ioreg itself may not
        # exist on non-macOS CI, both handled the same way.
        engine = CdcEngine(state, port_override="/dev/cu.does-not-exist-xyz")
        engine.start()
        time.sleep(0.5)
        self.assertFalse(state.cdc.connected)
        self.assertNotEqual(state.cdc.last_error, "")
        engine.stop()

    def test_send_command_with_no_connection_returns_false(self):
        state = SharedState()
        engine = CdcEngine(state, port_override="/dev/cu.does-not-exist-xyz")
        self.assertFalse(engine.send_command("STATUS"))
        self.assertFalse(engine.set_sy_mode(2))
        self.assertFalse(engine.send_dfu())


if __name__ == "__main__":
    unittest.main()

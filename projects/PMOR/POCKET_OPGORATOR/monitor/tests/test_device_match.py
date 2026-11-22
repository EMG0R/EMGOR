"""Unit tests for device_match.py -- the single place device-name matching
lives, so the incoming "Pocket OpGorator" -> "PMOR" firmware rename doesn't
break device discovery anywhere in the app."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from device_match import matches


class TestDeviceMatch(unittest.TestCase):
    def test_current_name(self):
        self.assertTrue(matches("Pocket OpGorator Audio"))
        self.assertTrue(matches("Pocket OpGorator"))

    def test_future_name(self):
        self.assertTrue(matches("PMOR"))
        self.assertTrue(matches("PMOR Audio"))
        self.assertTrue(matches("PMOR MIDI Port"))

    def test_case_insensitive(self):
        self.assertTrue(matches("pocket opgorator audio"))
        self.assertTrue(matches("pmor"))
        self.assertTrue(matches("Pmor"))
        self.assertTrue(matches("POCKET OPGORATOR"))

    def test_non_matches(self):
        self.assertFalse(matches("Scarlett 2i2 USB"))
        self.assertFalse(matches("MacBook Pro Microphone"))
        self.assertFalse(matches(""))
        self.assertFalse(matches(None))

    def test_substring_within_longer_name(self):
        self.assertTrue(matches("USB Modem (PMOR Console)"))
        self.assertTrue(matches("Something Pocket OpGorator Something"))


if __name__ == "__main__":
    unittest.main()

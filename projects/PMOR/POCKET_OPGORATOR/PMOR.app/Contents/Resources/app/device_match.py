"""
Single source of truth for "is this the Pocket OpGorator / PMOR device".

The hardware's USB product string is currently "Pocket OpGorator" but a
rename to "PMOR" is coming. Every place in this app that matches a device
name (audio input, MIDI port, MIDI output port, USB serial/CDC console)
imports `matches()` from here instead of hardcoding a substring, so the
rename is a one-line change in this file and nowhere else.

Matching is deliberately loose (case-insensitive substring, either name)
because we don't control exactly how the firmware/OS will capitalize or
punctuate the string after the rename ships.
"""

# Add new accepted names here -- and ONLY here -- if the device is renamed
# again later.
NAME_TOKENS = (
    "pocket opgorator",
    "pmor",
)


def matches(name):
    """True if `name` (a USB/MIDI/CoreAudio device or port name, or None)
    looks like the Pocket OpGorator / PMOR device."""
    if not name:
        return False
    n = name.lower()
    return any(tok in n for tok in NAME_TOKENS)

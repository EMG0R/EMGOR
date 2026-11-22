"""Drop ARMv7-only compiler flags when building norns on aarch64.

norns/wscript passes -mfpu=neon (and, under --release, -mfloat-abi=hard and
cortex-a53 tuning). Those are 32-bit ARM options: g++ on aarch64 rejects
-mfpu= outright with "unrecognized command-line option". norns targets a
32-bit Pi 3; this rig is a 64-bit Pi 5, where NEON is mandatory and always on,
so the correct 64-bit form of that flag is no flag at all.

Idempotent: keyed on the DEMIURGE marker, so re-running is a no-op and an
upstream fix is not repeatedly clobbered.
"""
import platform
import sys

MARKER = "DEMIURGE aarch64"
OLD = "    norns_cxxflags = ['-O3', '-Wall', '-mfpu=neon']"
NEW = """    # DEMIURGE aarch64 patch: -mfpu= is an ARMv7 (32-bit) option and g++ on
    # aarch64 rejects it outright. NEON is mandatory and always on in the
    # 64-bit ABI, so the correct 64-bit form of this flag is no flag at all.
    import platform as _platform
    _arm32 = _platform.machine().startswith(('armv', 'arm7'))
    norns_cxxflags = ['-O3', '-Wall'] + (['-mfpu=neon'] if _arm32 else [])"""

OLD_RELEASE = """        norns_cxxflags += [
            '-mcpu=cortex-a53',
            '-mtune=cortex-a53',
            '-mfpu=neon-fp-armv8',
            '-mfloat-abi=hard',
            '-funconstrained-commons',
            '-ffast-math',
            '-fgcse-sm',
        ]"""
NEW_RELEASE = """        # Same split: -mfpu=/-mfloat-abi= are 32-bit only. The Pi 5 is a
        # cortex-a76, so the a53 tuning is wrong for it besides.
        norns_cxxflags += [
            '-funconstrained-commons',
            '-ffast-math',
            '-fgcse-sm',
        ]
        if _arm32:
            norns_cxxflags += [
                '-mcpu=cortex-a53', '-mtune=cortex-a53',
                '-mfpu=neon-fp-armv8', '-mfloat-abi=hard',
            ]"""


def main(path):
    src = open(path).read()
    if MARKER in src:
        print("  01-aarch64-flags: already applied")
        return 0
    if platform.machine().startswith(("armv", "arm7")):
        print("  01-aarch64-flags: 32-bit ARM host, nothing to do")
        return 0
    if OLD not in src:
        sys.exit("  01-aarch64-flags: FAILED -- anchor not found in %s.\n"
                 "  Upstream changed; re-check the -mfpu=neon flag by hand." % path)
    src = src.replace(OLD, NEW)
    if OLD_RELEASE in src:
        src = src.replace(OLD_RELEASE, NEW_RELEASE)
    open(path, "w").write(src)
    print("  01-aarch64-flags: applied")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))

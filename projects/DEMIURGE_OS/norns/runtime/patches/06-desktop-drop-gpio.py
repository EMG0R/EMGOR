"""Stop building matron's GPIO/SPI hardware backends in --desktop mode.

THE ERROR THIS FIXES

    ssd1322.cc: error: 'gpiod_line_set_value' was not declared in this scope
    ssd1322.cc: error: 'gpiod_chip_open_by_name' was not declared in this scope
    ... (8 more)

libgpiod 2.x removed the entire v1 C API that these files use. Debian Trixie
ships libgpiod 2.x, and norns is written against v1.

WHY REMOVAL RATHER THAN A v1->v2 PORT

These two files drive hardware this rig does not have and will never have: an
SSD1322 SPI OLED and GPIO-wired encoders/keys. In --desktop mode their jobs are
done by the SDL screen and SDL input backends instead. Porting several hundred
lines of GPIO code to a new API so it can be compiled and then never executed
would be work whose only product is a larger surface to be wrong.

norns/wscript puts both in matron_sources UNCONDITIONALLY and merely ADDS the
SDL backends under NORNS_DESKTOP, so a desktop build compiles both sets. On
real norns hardware that is correct. Here it is the only thing still requiring
libgpiod at all.

TWO EDITS, BOTH REQUIRED

1. wscript: drop the two sources when NORNS_DESKTOP.
2. io.cc: io_types[] registers enc_gpio_ops and key_gpio_ops unconditionally.
   Dropping the source without guarding these is a link error, and registering
   GPIO input on a machine with no GPIO encoders would be wrong regardless.

(ssd1322 needs no io.cc guard -- it is not in io_types[] and screens.h only
declares the SDL ops, so nothing references it in a desktop build.)

Idempotent: each edit keyed on its own DEMIURGE marker.
"""
import sys

WSCRIPT_MARKER = "DEMIURGE desktop drops gpio"
WSCRIPT_OLD = """	    '../matron/src/hardware/screen/ssd1322.cc',
        '../matron/src/hardware/input/gpio.cc',"""
WSCRIPT_NEW = """	    # DEMIURGE desktop drops gpio: these two drive an SSD1322 SPI OLED
        # and GPIO encoders/keys, and they are written against the libgpiod
        # v1 API that libgpiod 2.x (Debian Trixie) removed. In --desktop mode
        # the SDL backends do both jobs, so these are pulled back in only for
        # a real hardware build.
        # (appended below, outside --desktop)"""

WSCRIPT_ANCHOR2 = """    if bld.env.NORNS_DESKTOP:
        matron_sources += [
            '../matron/src/hardware/screen/sdl.cc',
            '../matron/src/hardware/input/sdl.cc',"""
WSCRIPT_NEW2 = """    if not bld.env.NORNS_DESKTOP:
        matron_sources += [
            '../matron/src/hardware/screen/ssd1322.cc',
            '../matron/src/hardware/input/gpio.cc',
        ]

    if bld.env.NORNS_DESKTOP:
        matron_sources += [
            '../matron/src/hardware/screen/sdl.cc',
            '../matron/src/hardware/input/sdl.cc',"""

IO_MARKER = "DEMIURGE desktop has no gpio"
IO_OLD = """io_ops_t *io_types[] = {
    (io_ops_t *)&enc_gpio_ops,
    (io_ops_t *)&key_gpio_ops,

#ifdef NORNS_DESKTOP"""
IO_NEW = """io_ops_t *io_types[] = {
// DEMIURGE desktop has no gpio: the GPIO encoder/key backends are not built in
// a --desktop build (see wscript), so referencing their ops here would be a
// link error. There are also no GPIO encoders on such a machine to register.
#ifndef NORNS_DESKTOP
    (io_ops_t *)&enc_gpio_ops,
    (io_ops_t *)&key_gpio_ops,
#endif

#ifdef NORNS_DESKTOP"""


def patch(path, marker, pairs, label):
    src = open(path).read()
    if marker in src:
        print("  06-desktop-drop-gpio (%s): already applied" % label)
        return
    for old, _ in pairs:
        if old not in src:
            sys.exit("  06-desktop-drop-gpio (%s): FAILED -- anchor not found in %s.\n"
                     "  Upstream changed; re-check the hardware backend sources by hand."
                     % (label, path))
    for old, new in pairs:
        src = src.replace(old, new)
    open(path, "w").write(src)
    print("  06-desktop-drop-gpio (%s): applied" % label)


def main(wscript, io_cc):
    patch(wscript, WSCRIPT_MARKER,
          [(WSCRIPT_OLD, WSCRIPT_NEW), (WSCRIPT_ANCHOR2, WSCRIPT_NEW2)], "wscript")
    patch(io_cc, IO_MARKER, [(IO_OLD, IO_NEW)], "io.cc")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))

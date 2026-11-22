"""Add explicit void* casts in matron's SDL screen backend.

matron/src/hardware/screen/sdl.cc is a .cc file (compiled as C++) written in C
style. C allows implicit conversion from void* to any object pointer; C++ does
not, so two lines that are perfectly legal C fail to compile:

    error: invalid conversion from 'void*' to 'screen_sdl_priv_t*'

This is the --desktop-only screen backend, which is why upstream does not hit
it: the shipped norns image builds the ssd1322 SPI path instead, and nothing in
their CI compiles this file.

Casting is the correct fix rather than a workaround -- the pointer genuinely is
a screen_sdl_priv_t*, stashed in an opaque field by the io layer, and naming
the type at the point of retrieval is what C++ asks for. -fpermissive would
silence it globally and hide the next real one.

Idempotent: keyed on the DEMIURGE marker.
"""
import sys

MARKER = "DEMIURGE sdl cast"

REPLACEMENTS = [
    (
        "static void screen_sdl_paint(matron_fb_t *fb) {\n"
        "    screen_sdl_priv_t *priv = fb->io.data;",
        "static void screen_sdl_paint(matron_fb_t *fb) {\n"
        "    // DEMIURGE sdl cast: io.data is void*; C++ needs this spelled out.\n"
        "    screen_sdl_priv_t *priv = (screen_sdl_priv_t *)fb->io.data;",
    ),
    (
        "static void screen_sdl_surface_destroy(void *data) {\n"
        "    screen_sdl_priv_t *priv = data;",
        "static void screen_sdl_surface_destroy(void *data) {\n"
        "    // DEMIURGE sdl cast: same reason as screen_sdl_paint above.\n"
        "    screen_sdl_priv_t *priv = (screen_sdl_priv_t *)data;",
    ),
]


def main(path):
    src = open(path).read()
    if MARKER in src:
        print("  03-sdl-void-casts: already applied")
        return 0
    missing = [old for old, _ in REPLACEMENTS if old not in src]
    if missing:
        sys.exit("  03-sdl-void-casts: FAILED -- %d anchor(s) not found in %s.\n"
                 "  Upstream changed; re-check the void* conversions by hand."
                 % (len(missing), path))
    for old, new in REPLACEMENTS:
        src = src.replace(old, new)
    open(path, "w").write(src)
    print("  03-sdl-void-casts: applied")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))

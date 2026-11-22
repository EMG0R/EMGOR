"""Replace nested designated initializers in matron's SDL backends.

Both --desktop backends initialise their ops struct like this:

    screen_ops_t screen_sdl_ops = {
        .io_ops.name = "screen:sdl",
        ...

That NESTED designator form (`.a.b =`) is valid C99 but is not C++ in any
standard. Plain designated initializers arrived in C++20; the nested form
never did, and GCC does not offer it as an extension. norns compiles C++ at
-std=c++14 (see wscript), so g++ rejects it:

    error: expected primary-expression before '.' token

These files are .cc, so they are C++ regardless of being written in C style.
The maintained ssd1322 backend has no equivalent struct, which is why upstream
never sees this: the desktop path is not built by their image or their CI.

Rewritten as positional aggregate initialisation with an inner brace for the
embedded io_ops, which is valid in every C++ standard. Field order is taken
from matron/src/hardware/io.h:

    io_ops_t     { name, type, data_size, config, setup, destroy }
    screen_ops_t { io_ops, paint, bind }
    input_ops_t  { io_ops, poll }

Because positional init is order-sensitive and silently wrong if the struct is
reordered, each replacement is anchored on the full original block: if upstream
changes a field, the anchor stops matching and this aborts rather than building
a struct with its function pointers transposed.
"""
import sys

MARKER = "DEMIURGE positional init"

SCREEN_OLD = """screen_ops_t screen_sdl_ops = {
    .io_ops.name = "screen:sdl",
    .io_ops.type = IO_SCREEN,
    .io_ops.data_size = sizeof(screen_sdl_priv_t),
    .io_ops.config = screen_sdl_config,
    .io_ops.setup = screen_sdl_setup,
    .io_ops.destroy = screen_sdl_destroy,

    .paint = screen_sdl_paint,
    .bind = screen_sdl_bind,
};"""

SCREEN_NEW = """// DEMIURGE positional init: the nested `.io_ops.name =` designator form is
// C99-only and g++ rejects it at -std=c++14. Field order below is from
// io.h: io_ops_t { name, type, data_size, config, setup, destroy },
// then screen_ops_t's own { paint, bind }.
screen_ops_t screen_sdl_ops = {
    {
        "screen:sdl",
        IO_SCREEN,
        sizeof(screen_sdl_priv_t),
        screen_sdl_config,
        screen_sdl_setup,
        screen_sdl_destroy,
    },
    screen_sdl_paint,
    screen_sdl_bind,
};"""

INPUT_OLD = """input_ops_t input_sdl_ops = {
    .io_ops.name = "input:sdl",
    .io_ops.type = IO_INPUT,
    .io_ops.data_size = sizeof(input_sdl_priv_t),
    .io_ops.config = input_sdl_config,
    .io_ops.setup = input_sdl_setup,
    .io_ops.destroy = input_sdl_destroy,
    .poll = input_sdl_poll,
};"""

INPUT_NEW = """// DEMIURGE positional init: see the screen backend for why. Field order is
// io_ops_t { name, type, data_size, config, setup, destroy }, then
// input_ops_t's own { poll }.
input_ops_t input_sdl_ops = {
    {
        "input:sdl",
        IO_INPUT,
        sizeof(input_sdl_priv_t),
        input_sdl_config,
        input_sdl_setup,
        input_sdl_destroy,
    },
    input_sdl_poll,
};"""

TARGETS = {"screen": (SCREEN_OLD, SCREEN_NEW), "input": (INPUT_OLD, INPUT_NEW)}


def main(path, which):
    old, new = TARGETS[which]
    src = open(path).read()
    if MARKER in src:
        print("  04-sdl-designated-init (%s): already applied" % which)
        return 0
    if old not in src:
        sys.exit("  04-sdl-designated-init (%s): FAILED -- anchor not found in %s.\n"
                 "  Upstream changed the ops struct; positional init is order-sensitive,\n"
                 "  so re-derive the field order from io.h by hand before editing."
                 % (which, path))
    open(path, "w").write(src.replace(old, new))
    print("  04-sdl-designated-init (%s): applied" % which)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))

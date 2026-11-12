"""Resolve the conflicting lo_message typedef between norns and liblo.

matron/src/event_types.h declares:

    typedef void *lo_message;

as an opaque stand-in, so that header need not depend on liblo. But
matron/src/oracle.h includes BOTH <lo/lo.h> (which declares
`typedef struct lo_message_ *lo_message`) and event_types.h. Any C++
translation unit that reaches oracle.h therefore sees two CONFLICTING
typedefs for one name, and g++ rejects it:

    error: conflicting declaration 'typedef void* lo_message'
    note:  previous declaration as 'typedef struct lo_message_* lo_message'

Observed building crone/src/crone.cpp against liblo 0.32 on Debian Trixie.

The fix is to include liblo's own type header instead of hand-maintaining a
second declaration of the same type. That leaves exactly one declaration, which
is also why this cannot drift again. liblo lives in /usr/include, so this adds
no new include-path requirement for files that pull in event_types.h.

Idempotent: keyed on the DEMIURGE marker.
"""
import sys

MARKER = "DEMIURGE liblo"
OLD = """// lo_message is opaque here. files using it include oracle.h for the full definition.
typedef void *lo_message;"""
NEW = """// DEMIURGE liblo patch: this was `typedef void *lo_message;`, an opaque
// stand-in so this header would not need liblo. But oracle.h includes BOTH
// lo.h and this file, so the stand-in and the real declaration land in the
// same translation unit and g++ rejects the conflicting typedef. Including
// liblo's own type header gives one declaration of the type instead of two
// that must be kept in agreement by hand.
#include <lo/lo_types.h>"""


def main(path):
    src = open(path).read()
    if MARKER in src:
        print("  02-liblo-typedef: already applied")
        return 0
    if OLD not in src:
        sys.exit("  02-liblo-typedef: FAILED -- anchor not found in %s.\n"
                 "  Upstream changed; re-check the lo_message typedef by hand." % path)
    open(path, "w").write(src.replace(OLD, NEW))
    print("  02-liblo-typedef: applied")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))

"""Add the missing <string> include to crone's BufDiskWorker.h.

    error: field 'path' has incomplete type 'std::string'

BufDiskWorker.h declares a std::string member but never includes <string>. It
built historically because some other libstdc++ header pulled <string> in
transitively; newer libstdc++ tightened its internal includes and no longer
does. This is the single most common way older C++ breaks on a new toolchain,
and the fix is the one the code should always have had: include what you use.

Idempotent: keyed on the DEMIURGE marker.
"""
import sys

MARKER = "DEMIURGE string include"
OLD = """#include <atomic>
#include <condition_variable>
#include <functional>"""
NEW = """#include <atomic>
#include <condition_variable>
#include <functional>
// DEMIURGE string include: this header declares a std::string member. It used
// to arrive transitively via another libstdc++ header; newer libstdc++ does
// not provide it, giving "field 'path' has incomplete type 'std::string'".
#include <string>"""


def main(path):
    src = open(path).read()
    if MARKER in src:
        print("  05-crone-string-include: already applied")
        return 0
    if OLD not in src:
        sys.exit("  05-crone-string-include: FAILED -- anchor not found in %s." % path)
    open(path, "w").write(src.replace(OLD, NEW))
    print("  05-crone-string-include: applied")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))

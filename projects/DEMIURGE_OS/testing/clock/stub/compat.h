// testing/clock/stub/compat.h
//
// Non-Linux compat shims needed to whole-file-compile src/demiurge-clock.cpp
// on macOS, force-included via `-include` (see run.sh) so the daemon
// source itself needs zero macOS-specific changes. clock_nanosleep(3) is
// POSIX but macOS's libc doesn't ship it; TIMER_ABSTIME is its companion
// flag. Both are stubbed as no-ops -- this is a COMPILE check only, never
// used to actually run the daemon.
#pragma once

#include <time.h>

#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
static inline int clock_nanosleep(clockid_t, int, const struct timespec *, struct timespec *) {
    return 0;
}
#endif

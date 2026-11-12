# rtdelay — build ON THE PI, not on a Mac.
#
# rtdelay.c includes <jack/jack.h>. That header ships with libjack-jackd2-dev,
# which is installed on the Pi and is NOT installed on a development Mac. If
# you open rtdelay.c in an editor on the Mac, clangd will flag the include and
# then every JACK type as unknown. Those diagnostics are expected and mean
# nothing — there is no JACK SDK on that machine and the file is not intended
# to compile there.
#
# At runtime the binary is launched through `pw-jack`, which points it at
# PipeWire's libjack replacement rather than jackd2's. Linking against
# jackd2's -ljack at build time and running against PipeWire's at load time is
# the normal, supported arrangement for this distro — the ABI is the same.
#
#   ssh gorpi7@demiurge.local
#   cd ~/demiurge/testing/latency && make
#   ./rtdelay.sh 40 250
#
# rtdelay.sh builds this automatically if ./rtdelay is missing, so `make` is
# only needed when you want to rebuild after editing the source.

CFLAGS ?= -O2 -Wall
LDLIBS ?= -ljack -lm

all: rtdelay

rtdelay: rtdelay.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f rtdelay

.PHONY: all clean

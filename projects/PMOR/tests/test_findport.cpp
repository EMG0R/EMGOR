// test_findport: registry-only PMOR port discovery (never opens a port). Pure logic always; live lookup is informational.
// build: clang++ -std=c++17 -I.. tests/test_findport.cpp -framework IOKit -framework CoreFoundation
#include "../max/pmor_findport.h"
#include <stdio.h>
int main() {
	std::vector<std::string> cu = {"/dev/cu.Bluetooth-Incoming-Port", "/dev/cu.BoseSLFlexSE2", "/dev/cu.STOCKWELLII", "/dev/cu.debug-console",
	                               "/dev/cu.usbmodem213103", "/dev/cu.usbmodem4488BED64B3B5"};
	int bad = 0;
	if (pmorfind::letterFor("/dev/cu.usbmodem213103", cu) != 'e') bad++;
	if (pmorfind::letterFor("/dev/cu.usbmodem4488BED64B3B5", cu) != 'f') bad++;   // the Bela: never what find() returns
	if (pmorfind::letterFor("/dev/cu.nope", cu) != 0) bad++;
	char l; std::string p; bool ok = pmorfind::find(l, p);
	printf("live: found=%d letter=%c path=%s\n", ok, l ? l : '-', p.c_str());
	if (ok && p.find("4488") != std::string::npos) bad++;
	printf("%s (%d failures)\n", bad ? "FAIL" : "PASS", bad);
	return bad ? 1 : 0;
}

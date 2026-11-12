// pmor_findport.h -- find PMOR's serial port by USB identity (IOKit registry walk only, the port is NEVER opened).
// PMOR = USB VID 0xCAFE PID 0x4F70, product "Pocket OpGorator". Max's [serial] letters follow the sorted /dev/cu.* list (a = first entry).
#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <string.h>
#ifdef __APPLE__
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace pmorfind {

static inline std::vector<std::string> cuList() {
	std::vector<std::string> v;
	if (DIR* d = opendir("/dev")) {
		while (dirent* e = readdir(d)) if (strncmp(e->d_name, "cu.", 3) == 0) v.push_back(std::string("/dev/") + e->d_name);
		closedir(d);
	}
	std::sort(v.begin(), v.end());
	return v;
}

// Max letter ('a'..'f') of `path` in the sorted /dev/cu.* list, or 0 when absent / beyond f.
static inline char letterFor(const std::string& path, const std::vector<std::string>& cu) {
	for (size_t i = 0; i < cu.size() && i < 6; i++) if (cu[i] == path) return (char)('a' + i);
	return 0;
}

#ifdef __APPLE__
static inline bool num(io_registry_entry_t e, CFStringRef key, int& out) {
	CFTypeRef r = IORegistryEntryCreateCFProperty(e, key, kCFAllocatorDefault, 0);
	if (!r) return false;
	bool ok = CFGetTypeID(r) == CFNumberGetTypeID() && CFNumberGetValue((CFNumberRef)r, kCFNumberIntType, &out);
	CFRelease(r); return ok;
}
// IOCalloutDevice (/dev/cu.usbmodem...) of the attached PMOR, "" when it is not plugged in / not enumerated yet.
static inline std::string pmorCallout() {
	std::string path;
	io_iterator_t it = 0;
	if (IOServiceGetMatchingServices(0, IOServiceMatching("IOUSBHostDevice"), &it) != KERN_SUCCESS) return path;
	while (io_registry_entry_t dev = IOIteratorNext(it)) {
		int vid = 0, pid = 0;
		if (path.empty() && num(dev, CFSTR("idVendor"), vid) && num(dev, CFSTR("idProduct"), pid) && vid == 0xCAFE && pid == 0x4F70) {
			CFTypeRef c = IORegistryEntrySearchCFProperty(dev, kIOServicePlane, CFSTR("IOCalloutDevice"), kCFAllocatorDefault, kIORegistryIterateRecursively);
			if (c) {
				if (CFGetTypeID(c) == CFStringGetTypeID()) { char b[256]; if (CFStringGetCString((CFStringRef)c, b, sizeof b, kCFStringEncodingUTF8)) path = b; }
				CFRelease(c);
			}
		}
		IOObjectRelease(dev);
	}
	IOObjectRelease(it);
	return path;
}
#else
static inline std::string pmorCallout() { return std::string(); }
#endif

// letter + path of PMOR's port: false = not found (or not in Max's first six letters; path may still be set).
static inline bool find(char& letter, std::string& path) {
	path = pmorCallout(); letter = 0;
	if (path.empty()) return false;
	letter = letterFor(path, cuList());
	return letter != 0;
}

}  // namespace pmorfind

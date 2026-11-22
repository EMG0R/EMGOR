// midisend.c — list CoreMIDI destinations and send one CC to a name-matched device.
// usage: midisend            -> list destinations
//        midisend <match> <cc> <val>  -> send CC to first dest whose name contains <match>
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void name_of(MIDIEndpointRef e, char *buf, int n) {
    CFStringRef s = NULL;
    MIDIObjectGetStringProperty(e, kMIDIPropertyDisplayName, &s);
    if (!s) MIDIObjectGetStringProperty(e, kMIDIPropertyName, &s);
    buf[0] = 0;
    if (s) { CFStringGetCString(s, buf, n, kCFStringEncodingUTF8); CFRelease(s); }
}

int main(int argc, char **argv) {
    MIDIClientRef client; MIDIClientCreate(CFSTR("midisend"), NULL, NULL, &client);
    ItemCount n = MIDIGetNumberOfDestinations();
    if (argc < 4) {
        printf("destinations: %lu\n", (unsigned long)n);
        for (ItemCount i = 0; i < n; i++) {
            char nm[256]; name_of(MIDIGetDestination(i), nm, sizeof nm);
            printf("  dst[%lu] %s\n", (unsigned long)i, nm);
        }
        ItemCount ns = MIDIGetNumberOfSources();
        printf("sources: %lu\n", (unsigned long)ns);
        for (ItemCount i = 0; i < ns; i++) {
            char nm[256]; name_of(MIDIGetSource(i), nm, sizeof nm);
            printf("  src[%lu] %s\n", (unsigned long)i, nm);
        }
        return 0;
    }
    const char *match = argv[1]; int cc = atoi(argv[2]), val = atoi(argv[3]);
    MIDIEndpointRef dest = 0; char found[256] = "";
    for (ItemCount i = 0; i < n; i++) {
        char nm[256]; MIDIEndpointRef e = MIDIGetDestination(i); name_of(e, nm, sizeof nm);
        if (strcasestr(nm, match)) { dest = e; strncpy(found, nm, sizeof found - 1); break; }
    }
    if (!dest) { fprintf(stderr, "no destination matching '%s'\n", match); return 2; }
    MIDIPortRef port; MIDIOutputPortCreate(client, CFSTR("out"), &port);
    Byte buf[64]; MIDIPacketList *pl = (MIDIPacketList *)buf;
    MIDIPacket *p = MIDIPacketListInit(pl);
    Byte msg[3] = { (Byte)(0xB0), (Byte)cc, (Byte)val };
    p = MIDIPacketListAdd(pl, sizeof buf, p, 0, 3, msg);
    OSStatus st = MIDISend(port, dest, pl);
    printf("sent CC %d %d to \"%s\" (status %d)\n", cc, val, found, (int)st);
    return st == 0 ? 0 : 3;
}

// OmniplexAudioPlugIn.c
//
// A minimal AudioServerPlugIn (HAL plugin) that presents a single virtual
// audio device, "Omniplex Audio", with one input stream and one output stream,
// 48kHz stereo float32. Modeled architecturally on Apple's classic
// NullAudio.c sample plugin (single static device, no controls) and on the
// shape of BlackHole (ExistentialAudio/BlackHole) -- a real-world open
// source HAL loopback plugin with the same basic structure.
//
// Where this plugin differs from a pure loopback device: instead of piping
// output straight back to input in-process, both directions are bridged
// through a POSIX shared-memory ring buffer (see include/omniplex_shared_ring.h)
// so an external process (the omniplex-cli Rust binary) can push network-arrived
// audio in as this device's "input", and pull this device's "output" out to
// send over the network.
//
// No dynamic object creation: the plugin, device, and both streams are a
// fixed, static object graph created at Initialize() time. This keeps the
// property/dispatch code a simple switch instead of a real object model.
//
// Build: see ../Makefile. No Xcode project -- built directly with clang.

#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPlugInCOM.h>
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#include <math.h>
#include <os/log.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdatomic.h>

#include "omniplex_shared_ring.h"

#pragma mark - Object IDs (static object graph)

enum {
    kObjectID_PlugIn        = kAudioObjectPlugInObject, // = 1
    kObjectID_Device        = 2,
    kObjectID_Stream_Input  = 3, // data flows device -> client (a DAW recording FROM "Omniplex Audio")
    kObjectID_Stream_Output = 4, // data flows client -> device (a DAW playing OUT to "Omniplex Audio")
};

#define kDevice_UID          CFSTR("OmniplexAudioDevice_UID")
#define kDevice_ModelUID     CFSTR("OmniplexAudioDevice_ModelUID")
#define kDevice_Name         CFSTR("Omniplex Audio")
#define kDevice_Manufacturer CFSTR("Omniplex Project")
#define kPlugIn_BundleID     CFSTR("com.omniplex.audio.hal")

#define kOmniplex_SampleRate ((Float64)OMNIPLEX_SAMPLE_RATE)
#define kOmniplex_Channels   ((UInt32)OMNIPLEX_CHANNELS)
#define kOmniplex_RingPeriodFrames 4096u // frames between successive GetZeroTimeStamp ticks; must be >= 10923? no: min is 10923 per header comment... see note below

// NOTE on kOmniplex_RingPeriodFrames: the AudioServerPlugIn.h doc for
// kAudioDevicePropertyZeroTimeStampPeriod states "The minimum allowed value
// for this is 10923 sample frames." We use 24000 (0.5s @ 48kHz) to stay
// safely above that floor while still ticking the virtual clock often
// enough for smooth timing.
#undef kOmniplex_RingPeriodFrames
#define kOmniplex_RingPeriodFrames 24000u

static os_log_t gLog;

#pragma mark - Global driver state

typedef struct {
    // COM-style vtable pointer; must be first member so a OmniplexDriver* can be
    // reinterpreted as an AudioServerPlugInDriverRef (pointer to pointer to
    // interface struct is the CFPlugIn convention: the "object" IS the
    // interface pointer).
    AudioServerPlugInDriverInterface* interface;
} OmniplexDriver;

static AudioServerPlugInDriverInterface gDriverInterface; // filled in below
static OmniplexDriver gDriverInstance = { &gDriverInterface };
static AudioServerPlugInDriverRef gDriverRef = (AudioServerPlugInDriverRef)&gDriverInstance.interface;

static AudioServerPlugInHostRef gHost = NULL;
static pthread_mutex_t gStateMutex = PTHREAD_MUTEX_INITIALIZER;

static bool gDeviceIsRunning = false;   // true once at least one client has started IO
static UInt32 gStartCount = 0;          // number of StartIO calls outstanding (StopIO decrements)
static UInt64 gAnchorHostTime = 0;      // host time at which gAnchorSampleTime was true
static Float64 gAnchorSampleTime = 0;   // device sample-time counter at gAnchorHostTime
static Float64 gHostTicksPerFrame = 0;  // mach ticks per sample frame at 48kHz

// Shared-memory ring buffer state (see omniplex_shared_ring.h)
static int gShmFD = -1;
static void* gShmBase = NULL;
static OmniplexRingLayout gRing;
static bool gRingMapped = false;

#pragma mark - Shared memory setup

static bool EnsureSharedRingMapped(void) {
    if (gRingMapped) return true;

    uint64_t size = omniplex_segment_size(OMNIPLEX_RING_CAPACITY_FRAMES, OMNIPLEX_CHANNELS);

    // O_CREAT | O_RDWR with 0666: the plugin (root/_coreaudiod) and the
    // Rust bridge (login user) are different UIDs, so the segment must be
    // world read/write. shm_open enforces the umask on creation, so we
    // fchmod explicitly afterward to guarantee 0666 regardless of the
    // creating process's umask.
    int fd = shm_open(OMNIPLEX_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        os_log_error(gLog, "OmniplexAudio: shm_open(%{public}s) failed: %{errno}d", OMNIPLEX_SHM_NAME, errno);
        return false;
    }
    fchmod(fd, 0666);

    if (ftruncate(fd, (off_t)size) != 0) {
        os_log_error(gLog, "OmniplexAudio: ftruncate failed: %{errno}d", errno);
        close(fd);
        return false;
    }

    void* base = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        os_log_error(gLog, "OmniplexAudio: mmap failed: %{errno}d", errno);
        close(fd);
        return false;
    }

    gShmFD = fd;
    gShmBase = base;
    gRing = omniplex_layout(base);

    // Initialize header if we're the first to create it (or re-stamp it --
    // harmless if the Rust side already initialized it identically, since
    // the layout is a fixed compile-time constant on both sides).
    gRing.header->magic = OMNIPLEX_RING_MAGIC;
    gRing.header->version = OMNIPLEX_RING_VERSION;
    gRing.header->sample_rate = OMNIPLEX_SAMPLE_RATE;
    gRing.header->channels = OMNIPLEX_CHANNELS;
    gRing.header->capacity_frames = OMNIPLEX_RING_CAPACITY_FRAMES;
    gRing.header->format = OMNIPLEX_FORMAT_FLOAT32_INTERLEAVED;
    gRing.header->plugin_alive = 1;

    gRingMapped = true;
    os_log(gLog, "OmniplexAudio: shared ring mapped, size=%{public}llu bytes at %{public}s", size, OMNIPLEX_SHM_NAME);
    return true;
}

static void TeardownSharedRing(void) {
    if (!gRingMapped) return;
    if (gRing.header) gRing.header->plugin_alive = 0;
    uint64_t size = omniplex_segment_size(OMNIPLEX_RING_CAPACITY_FRAMES, OMNIPLEX_CHANNELS);
    munmap(gShmBase, (size_t)size);
    close(gShmFD);
    gShmBase = NULL;
    gShmFD = -1;
    gRingMapped = false;
}

// Producer side: HAL output stream -> playback ring (consumed by Rust).
// Drops frames if the ring is full rather than blocking (real-time thread).
static void RingWrite_Playback(const float* interleaved, UInt32 frames) {
    if (!gRingMapped) return;
    RingControl* rc = gRing.playback_ctrl;
    uint64_t w = atomic_load_explicit(&rc->write_frames, memory_order_relaxed);
    uint64_t r = atomic_load_explicit(&rc->read_frames, memory_order_acquire);
    uint64_t used = w - r;
    uint64_t free_frames = (uint64_t)OMNIPLEX_RING_CAPACITY_FRAMES - used;
    UInt32 toWrite = frames;
    if (toWrite > free_frames) toWrite = (UInt32)free_frames; // drop the overflow
    for (UInt32 i = 0; i < toWrite; i++) {
        uint64_t idx = (w + i) % OMNIPLEX_RING_CAPACITY_FRAMES;
        memcpy(&gRing.playback_data[idx * OMNIPLEX_CHANNELS],
               &interleaved[i * OMNIPLEX_CHANNELS],
               OMNIPLEX_CHANNELS * sizeof(float));
    }
    atomic_store_explicit(&rc->write_frames, w + toWrite, memory_order_release);
}

// Consumer side: capture ring (fed by Rust) -> HAL input stream. Emits
// silence for any frames not yet available.
static void RingRead_Capture(float* interleaved, UInt32 frames) {
    if (!gRingMapped) {
        memset(interleaved, 0, (size_t)frames * OMNIPLEX_CHANNELS * sizeof(float));
        return;
    }
    RingControl* rc = gRing.capture_ctrl;
    uint64_t w = atomic_load_explicit(&rc->write_frames, memory_order_acquire);
    uint64_t r = atomic_load_explicit(&rc->read_frames, memory_order_relaxed);
    uint64_t available = w - r;
    UInt32 toRead = frames;
    if (toRead > available) toRead = (UInt32)available;
    for (UInt32 i = 0; i < toRead; i++) {
        uint64_t idx = (r + i) % OMNIPLEX_RING_CAPACITY_FRAMES;
        memcpy(&interleaved[i * OMNIPLEX_CHANNELS],
               &gRing.capture_data[idx * OMNIPLEX_CHANNELS],
               OMNIPLEX_CHANNELS * sizeof(float));
    }
    if (toRead < frames) {
        memset(&interleaved[toRead * OMNIPLEX_CHANNELS], 0, (size_t)(frames - toRead) * OMNIPLEX_CHANNELS * sizeof(float));
    }
    atomic_store_explicit(&rc->read_frames, r + toRead, memory_order_release);
}

#pragma mark - IUnknown

static HRESULT OmniplexAudio_QueryInterface(void* inDriver, REFIID inUUID, LPVOID* outInterface) {
    if (outInterface == NULL) return kAudioHardwareIllegalOperationError;
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, inUUID);
    if (CFEqual(uuid, IUnknownUUID) || CFEqual(uuid, kAudioServerPlugInDriverInterfaceUUID)) {
        *outInterface = gDriverRef;
        CFRelease(uuid);
        return 0;
    }
    CFRelease(uuid);
    *outInterface = NULL;
    return E_NOINTERFACE;
}

static ULONG OmniplexAudio_AddRef(void* inDriver) { return 1; }
static ULONG OmniplexAudio_Release(void* inDriver) { return 1; }

#pragma mark - Basic Operations

static OSStatus OmniplexAudio_Initialize(AudioServerPlugInDriverRef inDriver, AudioServerPlugInHostRef inHost) {
    gHost = inHost;
    gHostTicksPerFrame = 0; // computed lazily from mach_timebase in GetZeroTimeStamp
    EnsureSharedRingMapped();
    os_log(gLog, "OmniplexAudio: Initialize");
    return 0;
}

static OSStatus OmniplexAudio_CreateDevice(AudioServerPlugInDriverRef inDriver, CFDictionaryRef inDescription,
                                       const AudioServerPlugInClientInfo* inClientInfo, AudioObjectID* outDeviceObjectID) {
    return kAudioHardwareUnsupportedOperationError; // static device graph only
}

static OSStatus OmniplexAudio_DestroyDevice(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID) {
    return kAudioHardwareUnsupportedOperationError;
}

static OSStatus OmniplexAudio_AddDeviceClient(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                                          const AudioServerPlugInClientInfo* inClientInfo) {
    return 0;
}

static OSStatus OmniplexAudio_RemoveDeviceClient(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                                             const AudioServerPlugInClientInfo* inClientInfo) {
    return 0;
}

static OSStatus OmniplexAudio_PerformDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                                                           UInt64 inChangeAction, void* inChangeInfo) {
    return 0;
}

static OSStatus OmniplexAudio_AbortDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                                                         UInt64 inChangeAction, void* inChangeInfo) {
    return 0;
}

#pragma mark - Property helpers

static bool IsObjectKnown(AudioObjectID id) {
    return id == kObjectID_PlugIn || id == kObjectID_Device ||
           id == kObjectID_Stream_Input || id == kObjectID_Stream_Output;
}

#pragma mark - HasProperty / IsPropertySettable

// Forward declaration: HasProperty is implemented in terms of
// GetPropertyDataSize (defined further down), so declare its prototype here.
static OSStatus OmniplexAudio_GetPropertyDataSize(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID,
                                              const AudioObjectPropertyAddress* inAddress, UInt32 inQualifierDataSize,
                                              const void* inQualifierData, UInt32* outDataSize);

static Boolean OmniplexAudio_HasProperty(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID,
                                     const AudioObjectPropertyAddress* inAddress) {
    if (!IsObjectKnown(inObjectID)) return false;
    UInt32 dataSize = 0;
    return (OmniplexAudio_GetPropertyDataSize(inDriver, inObjectID, inClientProcessID, inAddress, 0, NULL, &dataSize) == 0);
}

static OSStatus OmniplexAudio_IsPropertySettable(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID,
                                             const AudioObjectPropertyAddress* inAddress, Boolean* outIsSettable) {
    if (!IsObjectKnown(inObjectID)) return kAudioHardwareBadObjectError;
    switch (inAddress->mSelector) {
        case kAudioDevicePropertyNominalSampleRate:
            // Report settable=false: we only support one fixed rate, so
            // there is nothing meaningful to change.
            *outIsSettable = false;
            return 0;
        default:
            *outIsSettable = false;
            return 0;
    }
}

#pragma mark - GetPropertyDataSize / GetPropertyData

static UInt32 SafeCopyCFString(CFStringRef s, void* outData, UInt32 inDataSize, UInt32* outDataSize) {
    CFRetain(s);
    if (inDataSize >= sizeof(CFStringRef)) {
        *(CFStringRef*)outData = s;
    } else {
        CFRelease(s);
    }
    *outDataSize = sizeof(CFStringRef);
    return 0;
}

static OSStatus OmniplexAudio_GetPropertyDataSize(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID,
                                              const AudioObjectPropertyAddress* inAddress, UInt32 inQualifierDataSize,
                                              const void* inQualifierData, UInt32* outDataSize) {
    if (!IsObjectKnown(inObjectID)) return kAudioHardwareBadObjectError;
    AudioObjectPropertySelector sel = inAddress->mSelector;

    // Common AudioObject properties valid on every object.
    switch (sel) {
        case kAudioObjectPropertyBaseClass:
        case kAudioObjectPropertyClass:
        case kAudioObjectPropertyOwner:
            *outDataSize = sizeof(AudioClassID);
            return 0;
        case kAudioObjectPropertyName:
        case kAudioObjectPropertyManufacturer:
            *outDataSize = sizeof(CFStringRef);
            return 0;
        case kAudioObjectPropertyOwnedObjects:
            if (inObjectID == kObjectID_PlugIn) { *outDataSize = sizeof(AudioObjectID) * 1; return 0; }
            if (inObjectID == kObjectID_Device) { *outDataSize = sizeof(AudioObjectID) * 2; return 0; }
            *outDataSize = 0;
            return 0;
        default: break;
    }

    if (inObjectID == kObjectID_PlugIn) {
        switch (sel) {
            case kAudioPlugInPropertyBundleID: *outDataSize = sizeof(CFStringRef); return 0;
            case kAudioPlugInPropertyDeviceList: *outDataSize = sizeof(AudioObjectID) * 1; return 0;
            default: return kAudioHardwareUnknownPropertyError;
        }
    }

    if (inObjectID == kObjectID_Device) {
        switch (sel) {
            case kAudioDevicePropertyDeviceUID:
            case kAudioDevicePropertyModelUID:
                *outDataSize = sizeof(CFStringRef); return 0;
            case kAudioDevicePropertyTransportType: *outDataSize = sizeof(UInt32); return 0;
            case kAudioDevicePropertyRelatedDevices: *outDataSize = sizeof(AudioObjectID); return 0;
            case kAudioDevicePropertyClockDomain: *outDataSize = sizeof(UInt32); return 0;
            case kAudioDevicePropertyDeviceIsAlive: *outDataSize = sizeof(UInt32); return 0;
            case kAudioDevicePropertyDeviceIsRunning: *outDataSize = sizeof(UInt32); return 0;
            case kAudioDevicePropertyIsHidden: *outDataSize = sizeof(UInt32); return 0;
            case kAudioDevicePropertyDeviceCanBeDefaultDevice:
            case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
                *outDataSize = sizeof(UInt32); return 0;
            case kAudioDevicePropertyLatency: *outDataSize = sizeof(UInt32); return 0;
            case kAudioDevicePropertyStreams:
                if (inAddress->mScope == kAudioObjectPropertyScopeInput) { *outDataSize = sizeof(AudioObjectID) * 1; return 0; }
                if (inAddress->mScope == kAudioObjectPropertyScopeOutput) { *outDataSize = sizeof(AudioObjectID) * 1; return 0; }
                *outDataSize = sizeof(AudioObjectID) * 2; return 0;
            case kAudioObjectPropertyControlList: *outDataSize = 0; return 0;
            case kAudioDevicePropertySafetyOffset: *outDataSize = sizeof(UInt32); return 0;
            case kAudioDevicePropertyNominalSampleRate: *outDataSize = sizeof(Float64); return 0;
            case kAudioDevicePropertyAvailableNominalSampleRates: *outDataSize = sizeof(AudioValueRange); return 0;
            case kAudioDevicePropertyZeroTimeStampPeriod: *outDataSize = sizeof(UInt32); return 0;
            // NOTE: kAudioDevicePropertyIcon deliberately NOT advertised here.
            // This plugin has no icon resource in its bundle, and previously
            // GetPropertyDataSize claimed support for this property while
            // GetPropertyData had no case for it (fell through to
            // kAudioHardwareUnknownPropertyError). That Size/Data mismatch
            // made HasProperty() report true but the subsequent
            // GetPropertyData() call fail with 'who?' -- which is exactly
            // what coreaudiod hit while adding the device (HALS_PlugIn.cpp
            // "Caught exception trying to add device"), causing the device
            // to silently never be published even though Activate() had
            // already logged success. Simply not claiming the property here
            // makes HasProperty() correctly report false, so callers fall
            // back to a generic icon instead of tripping this failure.
            case kAudioDevicePropertyPreferredChannelsForStereo: *outDataSize = sizeof(UInt32) * 2; return 0;
            default: return kAudioHardwareUnknownPropertyError;
        }
    }

    if (inObjectID == kObjectID_Stream_Input || inObjectID == kObjectID_Stream_Output) {
        switch (sel) {
            case kAudioStreamPropertyIsActive: *outDataSize = sizeof(UInt32); return 0;
            case kAudioStreamPropertyDirection: *outDataSize = sizeof(UInt32); return 0;
            case kAudioStreamPropertyTerminalType: *outDataSize = sizeof(UInt32); return 0;
            case kAudioStreamPropertyStartingChannel: *outDataSize = sizeof(UInt32); return 0;
            case kAudioStreamPropertyLatency: *outDataSize = sizeof(UInt32); return 0;
            case kAudioStreamPropertyVirtualFormat:
            case kAudioStreamPropertyPhysicalFormat:
                *outDataSize = sizeof(AudioStreamBasicDescription); return 0;
            case kAudioStreamPropertyAvailableVirtualFormats:
            case kAudioStreamPropertyAvailablePhysicalFormats:
                *outDataSize = sizeof(AudioStreamRangedDescription); return 0;
            default: return kAudioHardwareUnknownPropertyError;
        }
    }

    return kAudioHardwareUnknownPropertyError;
}

static OSStatus OmniplexAudio_GetPropertyData(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID,
                                          const AudioObjectPropertyAddress* inAddress, UInt32 inQualifierDataSize,
                                          const void* inQualifierData, UInt32 inDataSize, UInt32* outDataSize, void* outData) {
    if (!IsObjectKnown(inObjectID)) return kAudioHardwareBadObjectError;
    AudioObjectPropertySelector sel = inAddress->mSelector;

    switch (sel) {
        case kAudioObjectPropertyBaseClass:
            *(AudioClassID*)outData = (inObjectID == kObjectID_PlugIn) ? kAudioObjectClassID
                : (inObjectID == kObjectID_Device) ? kAudioObjectClassID : kAudioObjectClassID;
            *outDataSize = sizeof(AudioClassID);
            return 0;
        case kAudioObjectPropertyClass:
            *(AudioClassID*)outData = (inObjectID == kObjectID_PlugIn) ? kAudioPlugInClassID
                : (inObjectID == kObjectID_Device) ? kAudioDeviceClassID : kAudioStreamClassID;
            *outDataSize = sizeof(AudioClassID);
            return 0;
        case kAudioObjectPropertyOwner:
            *(AudioObjectID*)outData = (inObjectID == kObjectID_PlugIn) ? kAudioObjectUnknown
                : (inObjectID == kObjectID_Device) ? (AudioObjectID)kObjectID_PlugIn : (AudioObjectID)kObjectID_Device;
            *outDataSize = sizeof(AudioObjectID);
            return 0;
        case kAudioObjectPropertyName: {
            CFStringRef name = (inObjectID == kObjectID_PlugIn) ? CFSTR("Omniplex Audio HAL Plug-In")
                : (inObjectID == kObjectID_Device) ? kDevice_Name
                : (inObjectID == kObjectID_Stream_Input) ? CFSTR("Omniplex Audio Input")
                : CFSTR("Omniplex Audio Output");
            return SafeCopyCFString(name, outData, inDataSize, outDataSize);
        }
        case kAudioObjectPropertyManufacturer:
            return SafeCopyCFString(kDevice_Manufacturer, outData, inDataSize, outDataSize);
        case kAudioObjectPropertyOwnedObjects: {
            if (inObjectID == kObjectID_PlugIn) {
                AudioObjectID* p = (AudioObjectID*)outData;
                if (inDataSize >= sizeof(AudioObjectID)) p[0] = kObjectID_Device;
                *outDataSize = sizeof(AudioObjectID);
                return 0;
            }
            if (inObjectID == kObjectID_Device) {
                AudioObjectID* p = (AudioObjectID*)outData;
                UInt32 n = inDataSize / sizeof(AudioObjectID);
                if (n > 2) n = 2;
                if (n >= 1) p[0] = kObjectID_Stream_Input;
                if (n >= 2) p[1] = kObjectID_Stream_Output;
                *outDataSize = n * sizeof(AudioObjectID);
                return 0;
            }
            *outDataSize = 0;
            return 0;
        }
        default: break;
    }

    if (inObjectID == kObjectID_PlugIn) {
        switch (sel) {
            case kAudioPlugInPropertyBundleID:
                return SafeCopyCFString(kPlugIn_BundleID, outData, inDataSize, outDataSize);
            case kAudioPlugInPropertyDeviceList: {
                AudioObjectID* p = (AudioObjectID*)outData;
                if (inDataSize >= sizeof(AudioObjectID)) p[0] = kObjectID_Device;
                *outDataSize = sizeof(AudioObjectID);
                return 0;
            }
            default: return kAudioHardwareUnknownPropertyError;
        }
    }

    if (inObjectID == kObjectID_Device) {
        switch (sel) {
            case kAudioDevicePropertyDeviceUID:
                return SafeCopyCFString(kDevice_UID, outData, inDataSize, outDataSize);
            case kAudioDevicePropertyModelUID:
                return SafeCopyCFString(kDevice_ModelUID, outData, inDataSize, outDataSize);
            case kAudioDevicePropertyTransportType:
                *(UInt32*)outData = kAudioDeviceTransportTypeVirtual;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioDevicePropertyRelatedDevices:
                if (inDataSize >= sizeof(AudioObjectID)) *(AudioObjectID*)outData = kObjectID_Device;
                *outDataSize = sizeof(AudioObjectID);
                return 0;
            case kAudioDevicePropertyClockDomain:
                *(UInt32*)outData = 0;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioDevicePropertyDeviceIsAlive:
                *(UInt32*)outData = 1;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioDevicePropertyDeviceIsRunning:
                *(UInt32*)outData = gDeviceIsRunning ? 1 : 0;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioDevicePropertyIsHidden:
                // This is the actual bug that kept "Omniplex Audio" from ever
                // being published: coreaudiod queries this property
                // directly via GetPropertyData while adding a newly
                // activated device -- without a preceding HasProperty /
                // GetPropertyDataSize check -- and having no case for it
                // here fell through to kAudioHardwareUnknownPropertyError
                // ('who?' / 2003332927). That made HALS_PlugIn's
                // "add device" step throw and silently drop the device
                // (visible in coreaudiod's log as "Caught exception trying
                // to add device" immediately after "HALS_Device::Activate:
                // activating device ... OmniplexAudioDevice_UID"). Reporting
                // IsHidden=false lets the device actually get published.
                *(UInt32*)outData = 0;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioDevicePropertyDeviceCanBeDefaultDevice:
            case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
                *(UInt32*)outData = 1;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioDevicePropertyLatency:
                *(UInt32*)outData = 0;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioDevicePropertyStreams: {
                AudioObjectID* p = (AudioObjectID*)outData;
                UInt32 n = 0;
                if (inAddress->mScope == kAudioObjectPropertyScopeInput) {
                    if (inDataSize >= sizeof(AudioObjectID)) { p[0] = kObjectID_Stream_Input; n = 1; }
                } else if (inAddress->mScope == kAudioObjectPropertyScopeOutput) {
                    if (inDataSize >= sizeof(AudioObjectID)) { p[0] = kObjectID_Stream_Output; n = 1; }
                } else {
                    UInt32 avail = inDataSize / sizeof(AudioObjectID);
                    if (avail >= 1) { p[0] = kObjectID_Stream_Input; n = 1; }
                    if (avail >= 2) { p[1] = kObjectID_Stream_Output; n = 2; }
                }
                *outDataSize = n * sizeof(AudioObjectID);
                return 0;
            }
            case kAudioObjectPropertyControlList:
                *outDataSize = 0;
                return 0;
            case kAudioDevicePropertySafetyOffset:
                *(UInt32*)outData = 0;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioDevicePropertyNominalSampleRate:
                *(Float64*)outData = kOmniplex_SampleRate;
                *outDataSize = sizeof(Float64);
                return 0;
            case kAudioDevicePropertyAvailableNominalSampleRates: {
                AudioValueRange* r = (AudioValueRange*)outData;
                if (inDataSize >= sizeof(AudioValueRange)) {
                    r->mMinimum = kOmniplex_SampleRate;
                    r->mMaximum = kOmniplex_SampleRate;
                }
                *outDataSize = sizeof(AudioValueRange);
                return 0;
            }
            case kAudioDevicePropertyZeroTimeStampPeriod:
                *(UInt32*)outData = kOmniplex_RingPeriodFrames;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioDevicePropertyPreferredChannelsForStereo: {
                UInt32* p = (UInt32*)outData;
                if (inDataSize >= sizeof(UInt32) * 2) { p[0] = 1; p[1] = 2; }
                *outDataSize = sizeof(UInt32) * 2;
                return 0;
            }
            default: return kAudioHardwareUnknownPropertyError;
        }
    }

    if (inObjectID == kObjectID_Stream_Input || inObjectID == kObjectID_Stream_Output) {
        bool isInput = (inObjectID == kObjectID_Stream_Input);
        switch (sel) {
            case kAudioStreamPropertyIsActive:
                *(UInt32*)outData = 1;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioStreamPropertyDirection:
                *(UInt32*)outData = isInput ? 1 : 0; // 1 = input, 0 = output per AudioHardwareBase.h docs
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioStreamPropertyTerminalType:
                *(UInt32*)outData = isInput ? kAudioStreamTerminalTypeMicrophone : kAudioStreamTerminalTypeSpeaker;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioStreamPropertyStartingChannel:
                *(UInt32*)outData = 1;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioStreamPropertyLatency:
                *(UInt32*)outData = 0;
                *outDataSize = sizeof(UInt32);
                return 0;
            case kAudioStreamPropertyVirtualFormat:
            case kAudioStreamPropertyPhysicalFormat: {
                AudioStreamBasicDescription* f = (AudioStreamBasicDescription*)outData;
                if (inDataSize >= sizeof(AudioStreamBasicDescription)) {
                    f->mSampleRate = kOmniplex_SampleRate;
                    f->mFormatID = kAudioFormatLinearPCM;
                    f->mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
                    f->mBytesPerPacket = kOmniplex_Channels * sizeof(float);
                    f->mFramesPerPacket = 1;
                    f->mBytesPerFrame = kOmniplex_Channels * sizeof(float);
                    f->mChannelsPerFrame = kOmniplex_Channels;
                    f->mBitsPerChannel = 32;
                    f->mReserved = 0;
                }
                *outDataSize = sizeof(AudioStreamBasicDescription);
                return 0;
            }
            case kAudioStreamPropertyAvailableVirtualFormats:
            case kAudioStreamPropertyAvailablePhysicalFormats: {
                AudioStreamRangedDescription* r = (AudioStreamRangedDescription*)outData;
                if (inDataSize >= sizeof(AudioStreamRangedDescription)) {
                    r->mFormat.mSampleRate = kOmniplex_SampleRate;
                    r->mFormat.mFormatID = kAudioFormatLinearPCM;
                    r->mFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
                    r->mFormat.mBytesPerPacket = kOmniplex_Channels * sizeof(float);
                    r->mFormat.mFramesPerPacket = 1;
                    r->mFormat.mBytesPerFrame = kOmniplex_Channels * sizeof(float);
                    r->mFormat.mChannelsPerFrame = kOmniplex_Channels;
                    r->mFormat.mBitsPerChannel = 32;
                    r->mFormat.mReserved = 0;
                    r->mSampleRateRange.mMinimum = kOmniplex_SampleRate;
                    r->mSampleRateRange.mMaximum = kOmniplex_SampleRate;
                }
                *outDataSize = sizeof(AudioStreamRangedDescription);
                return 0;
            }
            default: return kAudioHardwareUnknownPropertyError;
        }
    }

    return kAudioHardwareUnknownPropertyError;
}

static OSStatus OmniplexAudio_SetPropertyData(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID,
                                          const AudioObjectPropertyAddress* inAddress, UInt32 inQualifierDataSize,
                                          const void* inQualifierData, UInt32 inDataSize, const void* inData) {
    // Nothing is settable in this minimal implementation.
    return kAudioHardwareUnknownPropertyError;
}

#pragma mark - IO Operations

static OSStatus OmniplexAudio_StartIO(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID) {
    pthread_mutex_lock(&gStateMutex);
    if (gStartCount == 0) {
        gDeviceIsRunning = true;
        mach_timebase_info_data_t tb;
        mach_timebase_info(&tb);
        // host ticks per second = 1e9 * tb.denom / tb.numer (mach_absolute_time ticks are in tb.numer/tb.denom ns units)
        double ticksPerSecond = 1e9 * (double)tb.denom / (double)tb.numer;
        gHostTicksPerFrame = ticksPerSecond / kOmniplex_SampleRate;
        gAnchorHostTime = mach_absolute_time();
        gAnchorSampleTime = 0;
        EnsureSharedRingMapped();
    }
    gStartCount++;
    pthread_mutex_unlock(&gStateMutex);
    os_log(gLog, "OmniplexAudio: StartIO client=%{public}u count=%{public}u", inClientID, gStartCount);
    return 0;
}

static OSStatus OmniplexAudio_StopIO(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID) {
    pthread_mutex_lock(&gStateMutex);
    if (gStartCount > 0) gStartCount--;
    if (gStartCount == 0) gDeviceIsRunning = false;
    pthread_mutex_unlock(&gStateMutex);
    os_log(gLog, "OmniplexAudio: StopIO client=%{public}u count=%{public}u", inClientID, gStartCount);
    return 0;
}

static OSStatus OmniplexAudio_GetZeroTimeStamp(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID,
                                           Float64* outSampleTime, UInt64* outHostTime, UInt64* outSeed) {
    pthread_mutex_lock(&gStateMutex);
    if (gHostTicksPerFrame == 0) {
        mach_timebase_info_data_t tb;
        mach_timebase_info(&tb);
        double ticksPerSecond = 1e9 * (double)tb.denom / (double)tb.numer;
        gHostTicksPerFrame = ticksPerSecond / kOmniplex_SampleRate;
        gAnchorHostTime = mach_absolute_time();
        gAnchorSampleTime = 0;
    }
    UInt64 now = mach_absolute_time();
    Float64 elapsedFrames = (Float64)(now - gAnchorHostTime) / gHostTicksPerFrame;
    Float64 currentSampleTime = gAnchorSampleTime + elapsedFrames;
    // Quantize down to the nearest zero-time-stamp period, per the contract
    // that GetZeroTimeStamp returns points on a fixed period grid.
    Float64 periodTime = floor(currentSampleTime / kOmniplex_RingPeriodFrames) * kOmniplex_RingPeriodFrames;
    Float64 framesFromAnchor = periodTime - gAnchorSampleTime;
    UInt64 hostTimeForPeriod = gAnchorHostTime + (UInt64)(framesFromAnchor * gHostTicksPerFrame);

    *outSampleTime = periodTime;
    *outHostTime = hostTimeForPeriod;
    *outSeed = 1;
    pthread_mutex_unlock(&gStateMutex);
    return 0;
}

static OSStatus OmniplexAudio_WillDoIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID,
                                            UInt32 inOperationID, Boolean* outWillDo, Boolean* outWillDoInPlace) {
    switch (inOperationID) {
        case kAudioServerPlugInIOOperationReadInput:
        case kAudioServerPlugInIOOperationWriteMix:
            if (outWillDo) *outWillDo = true;
            if (outWillDoInPlace) *outWillDoInPlace = true;
            return 0;
        default:
            if (outWillDo) *outWillDo = false;
            if (outWillDoInPlace) *outWillDoInPlace = true;
            return 0;
    }
}

static OSStatus OmniplexAudio_BeginIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID,
                                           UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    return 0;
}

static OSStatus OmniplexAudio_DoIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, AudioObjectID inStreamObjectID,
                                        UInt32 inClientID, UInt32 inOperationID, UInt32 inIOBufferFrameSize,
                                        const AudioServerPlugInIOCycleInfo* inIOCycleInfo, void* ioMainBuffer, void* ioSecondaryBuffer) {
    if (inOperationID == kAudioServerPlugInIOOperationReadInput && inStreamObjectID == kObjectID_Stream_Input) {
        RingRead_Capture((float*)ioMainBuffer, inIOBufferFrameSize);
        return 0;
    }
    if (inOperationID == kAudioServerPlugInIOOperationWriteMix && inStreamObjectID == kObjectID_Stream_Output) {
        RingWrite_Playback((const float*)ioMainBuffer, inIOBufferFrameSize);
        return 0;
    }
    return 0;
}

static OSStatus OmniplexAudio_EndIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID,
                                         UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    return 0;
}

#pragma mark - HasProperty needs GetPropertyDataSize declared before use (fix ordering)

// (OmniplexAudio_HasProperty above calls OmniplexAudio_GetPropertyDataSize_fwd, which
// forwards to OmniplexAudio_GetPropertyDataSize defined further down in the
// file; both are declared ahead of first use via the prototypes above.)

#pragma mark - vtable + factory

static AudioServerPlugInDriverInterface gDriverInterface = {
    NULL, // _reserved
    OmniplexAudio_QueryInterface,
    OmniplexAudio_AddRef,
    OmniplexAudio_Release,
    OmniplexAudio_Initialize,
    OmniplexAudio_CreateDevice,
    OmniplexAudio_DestroyDevice,
    OmniplexAudio_AddDeviceClient,
    OmniplexAudio_RemoveDeviceClient,
    OmniplexAudio_PerformDeviceConfigurationChange,
    OmniplexAudio_AbortDeviceConfigurationChange,
    OmniplexAudio_HasProperty,
    OmniplexAudio_IsPropertySettable,
    OmniplexAudio_GetPropertyDataSize,
    OmniplexAudio_GetPropertyData,
    OmniplexAudio_SetPropertyData,
    OmniplexAudio_StartIO,
    OmniplexAudio_StopIO,
    OmniplexAudio_GetZeroTimeStamp,
    OmniplexAudio_WillDoIOOperation,
    OmniplexAudio_BeginIOOperation,
    OmniplexAudio_DoIOOperation,
    OmniplexAudio_EndIOOperation,
};

void* OmniplexAudio_Create(CFAllocatorRef allocator, CFUUIDRef requestedTypeUUID);

void* OmniplexAudio_Create(CFAllocatorRef allocator, CFUUIDRef requestedTypeUUID) {
    (void)allocator;
    if (gLog == NULL) gLog = os_log_create("com.omniplex.audio.hal", "OmniplexAudioPlugIn");
    if (!CFEqual(requestedTypeUUID, kAudioServerPlugInTypeUUID)) return NULL;
    return gDriverRef;
}

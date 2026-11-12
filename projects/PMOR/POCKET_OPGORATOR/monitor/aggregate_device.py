"""
Programmatic macOS Aggregate Device creation for the Pocket OpGorator / PMOR.

Earlier versions of this app refused to do this (`--create-aggregate` only
printed manual Audio MIDI Setup steps) on the theory that scripting
`AudioHardwareCreateAggregateDevice` risked leaving a broken or duplicate
device sitting in the user's system-wide CoreAudio device list, disturbing
Loopback/NDI/other routing this Mac depends on. That tradeoff was reversed
by explicit user instruction: manual setup is a non-starter, and the app
must "just work" with no Audio MIDI Setup detour.

The risk above is avoided a different way: every aggregate this module
creates is marked **private** (`kAudioAggregateDeviceIsPrivateKey = 1`).
A private aggregate:
  - is NOT written to CoreAudio's persisted plist, so it never appears in
    Audio MIDI Setup and can't be confused with a user-made device,
  - is visible only to the process that created it (confirmed empirically:
    a second process's `AudioObjectGetPropertyData(kAudioHardwarePropertyDevices)`
    does not enumerate it at all),
  - is torn down automatically by coreaudiod when the creating process's
    connection to the HAL dies (confirmed empirically: killing the creating
    process without ever calling `AudioHardwareDestroyAggregateDevice` left
    zero trace in a fresh process's device enumeration).
So a crash, force-quit, or forgotten cleanup call cannot leave an orphaned
device behind, and nothing here ever touches an existing device's
properties -- only ever adds a new one.

Implementation note: two different Apple APIs are used here for two
different jobs, because each is better suited to its half:

  - Device *enumeration* (listing devices, reading a device's CFString UID/
    name) is done with plain ctypes against CoreAudio.framework /
    CoreFoundation.framework. These are simple fixed-size-struct C calls;
    the one wrinkle is that `CFStringGetCStringPtr` (the "fast path" some
    ctypes/CoreAudio snippets reach for) frequently returns NULL because the
    string isn't stored in a form that pointer can address directly -- the
    fix is to always go through `CFStringGetCString` with an explicit
    buffer instead, which never takes that shortcut and always works.
  - Aggregate device *creation/destruction* goes through pyobjc's CoreAudio
    binding (`AudioHardwareCreateAggregateDevice` / `...Destroy...`)
    instead of hand-rolled ctypes, specifically so the CFDictionaryRef
    description (with its nested array of per-subdevice dicts) can be
    built as an ordinary Python dict -- pyobjc bridges Python
    dict/list/str/int to NSDictionary/NSArray/NSString/NSNumber (and those
    are toll-free bridged to the CF types CoreAudio expects) automatically.
    Hand-rolling that nested structure in raw ctypes (building
    CFMutableDictionary/CFArray/CFString/CFNumber by hand) is exactly where
    an earlier attempt at this got stuck.
"""
import ctypes
import ctypes.util
import time

BUNDLE_NAME = "PMOR Aggregate"
BUNDLE_UID = "synth.emgor.pmor.aggregate"

# ---------------------------------------------------------------------
# ctypes: device enumeration (read-only, plain CoreAudio/CoreFoundation C
# calls -- no Objective-C bridging needed for this half)
# ---------------------------------------------------------------------

_cf = ctypes.CDLL(ctypes.util.find_library("CoreFoundation"))
_ca = ctypes.CDLL(ctypes.util.find_library("CoreAudio"))

_cf.CFStringGetCString.restype = ctypes.c_bool
_cf.CFStringGetCString.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_long, ctypes.c_uint32]
_cf.CFStringGetLength.restype = ctypes.c_long
_cf.CFStringGetLength.argtypes = [ctypes.c_void_p]
_cf.CFRelease.argtypes = [ctypes.c_void_p]

_K_CF_STRING_ENCODING_UTF8 = 0x08000100


def _cfstring_to_py(ref, release=True):
    """Convert a CFStringRef (as a raw pointer value) to a Python str.
    Always goes through CFStringGetCString with an explicit buffer --
    CFStringGetCStringPtr's "fast path" frequently returns NULL for
    strings that aren't backed by a directly-addressable buffer, which is
    the classic gotcha with this API. Never raises; returns None on any
    failure so a bad/foreign device just doesn't get a name/UID rather
    than crashing enumeration for every other device."""
    if not ref:
        return None
    try:
        length = _cf.CFStringGetLength(ref)
        buf_size = (length * 4) + 1  # worst case UTF-8 expansion + NUL
        buf = ctypes.create_string_buffer(buf_size)
        ok = _cf.CFStringGetCString(ref, buf, buf_size, _K_CF_STRING_ENCODING_UTF8)
        s = buf.value.decode("utf-8", errors="replace") if ok else None
    finally:
        if release:
            _cf.CFRelease(ref)
    return s


class _AudioObjectPropertyAddress(ctypes.Structure):
    _fields_ = [("mSelector", ctypes.c_uint32),
                ("mScope", ctypes.c_uint32),
                ("mElement", ctypes.c_uint32)]


def _fourcc(s):
    return int.from_bytes(s.encode("ascii"), "big")


_K_SYSTEM_OBJECT = 1
_K_SCOPE_GLOBAL = _fourcc("glob")
_K_ELEMENT_MASTER = 0
_K_PROP_DEVICES = _fourcc("dev#")
_K_PROP_DEVICE_UID = _fourcc("uid ")
_K_PROP_NAME = _fourcc("lnam")

_ca.AudioObjectGetPropertyDataSize.argtypes = [
    ctypes.c_uint32, ctypes.POINTER(_AudioObjectPropertyAddress),
    ctypes.c_uint32, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
_ca.AudioObjectGetPropertyData.argtypes = [
    ctypes.c_uint32, ctypes.POINTER(_AudioObjectPropertyAddress),
    ctypes.c_uint32, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32), ctypes.c_void_p]


def _get_prop_cfstring(object_id, selector):
    addr = _AudioObjectPropertyAddress(selector, _K_SCOPE_GLOBAL, _K_ELEMENT_MASTER)
    ref = ctypes.c_void_p()
    size = ctypes.c_uint32(ctypes.sizeof(ref))
    status = _ca.AudioObjectGetPropertyData(object_id, ctypes.byref(addr), 0, None,
                                             ctypes.byref(size), ctypes.byref(ref))
    if status != 0:
        return None
    return _cfstring_to_py(ref.value)


def list_ca_devices():
    """Return [{'id': AudioObjectID, 'name': str|None, 'uid': str|None}, ...]
    for every CoreAudio device currently visible to THIS process (private
    aggregates owned by other processes are correctly invisible here, which
    is exactly the isolation the 'private' flag is for)."""
    addr = _AudioObjectPropertyAddress(_K_PROP_DEVICES, _K_SCOPE_GLOBAL, _K_ELEMENT_MASTER)
    size = ctypes.c_uint32(0)
    status = _ca.AudioObjectGetPropertyDataSize(_K_SYSTEM_OBJECT, ctypes.byref(addr), 0, None, ctypes.byref(size))
    if status != 0 or size.value == 0:
        return []
    n = size.value // 4
    arr = (ctypes.c_uint32 * n)()
    got = ctypes.c_uint32(size.value)
    status = _ca.AudioObjectGetPropertyData(_K_SYSTEM_OBJECT, ctypes.byref(addr), 0, None, ctypes.byref(got), arr)
    if status != 0:
        return []
    out = []
    for dev_id in arr:
        out.append({
            "id": dev_id,
            "name": _get_prop_cfstring(dev_id, _K_PROP_NAME),
            "uid": _get_prop_cfstring(dev_id, _K_PROP_DEVICE_UID),
        })
    return out


def find_uid(devices, name_matcher):
    """First device whose name satisfies name_matcher(name) -> uid, or None."""
    for d in devices:
        if d["name"] and name_matcher(d["name"]):
            return d["uid"]
    return None


def find_device_by_uid(devices, uid):
    for d in devices:
        if d["uid"] == uid:
            return d
    return None


# ---------------------------------------------------------------------
# pyobjc: aggregate device create/destroy
# ---------------------------------------------------------------------

def create_private_aggregate(input_uid, output_uid, name=BUNDLE_NAME, bundle_uid=BUNDLE_UID):
    """Create a private, non-persistent Aggregate Device combining
    input_uid (the PMOR's capture UID, drift-corrected) with output_uid
    (the chosen playback device's UID, clock master).

    Returns (ok: bool, device_id: int|None, error: str).

    Never modifies any existing device. Idempotent in the sense that it's
    always safe to call again -- callers should check for an existing
    device with `bundle_uid` first (see `ensure_aggregate` below) rather
    than relying on this function to detect collisions, since a private
    device from a still-running instance of this same app is the only
    case that could collide, and that case is better handled by reusing
    it than by trying to create a second one.
    """
    try:
        import CoreAudio as CA
    except Exception as e:
        return False, None, f"pyobjc CoreAudio unavailable: {e}"

    if not input_uid or not output_uid:
        return False, None, "missing input or output device UID"

    desc = {
        "name": name,
        "uid": bundle_uid,
        "master": output_uid,   # the Mac's own clock is the stable reference
        "stacked": 0,
        "private": 1,           # <- the whole safety story; see module docstring
        "subdevices": [
            {"uid": input_uid, "drift": 1},   # non-master: gets drift correction
            {"uid": output_uid, "drift": 0},  # master: not corrected against itself
        ],
    }
    try:
        status, device_id = CA.AudioHardwareCreateAggregateDevice(desc, None)
    except Exception as e:
        return False, None, f"AudioHardwareCreateAggregateDevice raised: {e}"
    if status != 0:
        return False, None, f"AudioHardwareCreateAggregateDevice failed, OSStatus={status}"
    return True, device_id, ""


def destroy_aggregate(device_id):
    """Destroy an aggregate device by its current AudioObjectID. Returns
    (ok, error). Safe to call even if it's already gone (reported as a
    non-fatal 'already gone')."""
    try:
        import CoreAudio as CA
    except Exception as e:
        return False, f"pyobjc CoreAudio unavailable: {e}"
    try:
        status = CA.AudioHardwareDestroyAggregateDevice(device_id)
    except Exception as e:
        return False, f"AudioHardwareDestroyAggregateDevice raised: {e}"
    if status != 0:
        return False, f"AudioHardwareDestroyAggregateDevice failed, OSStatus={status}"
    return True, ""


def destroy_aggregate_by_uid(bundle_uid=BUNDLE_UID):
    """Look up the current device matching bundle_uid (its AudioObjectID is
    NOT stable across create calls, so always resolve fresh) and destroy
    it. Returns (ok, error) -- 'not found' is reported as ok=True (nothing
    to clean up is success, not failure)."""
    d = find_device_by_uid(list_ca_devices(), bundle_uid)
    if d is None:
        return True, "no matching aggregate device found (nothing to remove)"
    return destroy_aggregate(d["id"])


def refresh_portaudio(retries=10, delay_s=0.15):
    """Force PortAudio (via sounddevice) to re-scan CoreAudio's device
    list within THIS process, then poll briefly for a device to actually
    appear by name -- creation is not always instantly visible to a
    same-process PortAudio rescan (measured: usually immediate,
    occasionally needs ~1 retry), so this is a short bounded poll, not a
    single one-shot check."""
    import sounddevice as sd
    sd._terminate()
    sd._initialize()


def wait_for_device_name(name, retries=10, delay_s=0.15):
    """Poll sounddevice's device list (refreshing PortAudio each attempt)
    until `name` appears, or give up. Returns the sounddevice info dict or
    None."""
    import sounddevice as sd
    for attempt in range(retries):
        refresh_portaudio()
        try:
            return sd.query_devices(name)
        except Exception:
            if attempt < retries - 1:
                time.sleep(delay_s)
    return None


def ensure_aggregate(input_name_matcher, output_name, name=BUNDLE_NAME, bundle_uid=BUNDLE_UID):
    """Top-level entry point used by AudioEngine. Idempotent:

      1. If a device with `bundle_uid` already exists (a private aggregate
         from a still-running earlier instance of this app), reuse it.
      2. Otherwise, resolve the PMOR input's UID (via input_name_matcher,
         normally device_match.matches) and the chosen output's UID (exact
         name match), and if BOTH are present, create a new private
         aggregate.
      3. If either device is absent, or creation fails for any reason,
         return (False, None, reason) and do nothing else -- callers fall
         back to the dual-stream path and show `reason` in the UI. Never
         raises.

    Returns (ok: bool, sd_device_info: dict|None, message: str).
    """
    try:
        devices = list_ca_devices()
    except Exception as e:
        return False, None, f"CoreAudio device enumeration failed: {e}"

    existing = find_device_by_uid(devices, bundle_uid)
    if existing is not None:
        info = wait_for_device_name(existing["name"] or name, retries=3)
        if info is not None:
            return True, info, f"reused existing private aggregate '{existing['name']}'"
        # UID exists in CoreAudio's list but PortAudio hasn't surfaced it
        # under that name for some reason -- fall through and try to
        # (re)create rather than getting stuck.

    input_uid = find_uid(devices, input_name_matcher)
    if input_uid is None:
        return False, None, "PMOR input device not present (no UID match)"
    output_uid = find_uid(devices, lambda n: n == output_name)
    if output_uid is None:
        return False, None, f"output device '{output_name}' not present (no UID match)"

    ok, device_id, err = create_private_aggregate(input_uid, output_uid, name=name, bundle_uid=bundle_uid)
    if not ok:
        return False, None, f"aggregate creation failed: {err}"

    info = wait_for_device_name(name)
    if info is None:
        return False, None, "aggregate created but did not appear in PortAudio's device list"
    return True, info, f"created private aggregate '{name}' (input={input_uid}, output={output_uid})"

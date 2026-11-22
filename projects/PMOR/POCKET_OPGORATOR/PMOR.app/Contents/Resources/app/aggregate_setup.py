"""Programmatic macOS Aggregate Device creation for PMOR.

Creates a CoreAudio Aggregate Device named "PMOR" combining the PMOR's
USB capture side with a chosen output device, clock-mastered by the
OUTPUT (its clock is the reference; the PMOR row gets drift correction,
which belongs on the non-master by definition).

The earlier design refused to do this programmatically out of caution.
The caution is handled instead of avoided:
  - We only ever create a device named exactly AGG_NAME ("PMOR").
  - If a device with that name already exists, we return it untouched.
  - Sub-device selection goes through device_match (never grabs Loopback/
    NDI/other virtual devices).
  - Nothing else in the system audio config is read or written.

Implementation: the aggregate description is a property-list dictionary;
we build it with plistlib and hand it to CoreFoundation/CoreAudio via
ctypes — no PyObjC dependency.
"""
import ctypes
import ctypes.util
import plistlib

import device_match

AGG_NAME = "PMOR"
AGG_UID = "synth.emgor.pmor.aggregate"

_cf = ctypes.CDLL(ctypes.util.find_library("CoreFoundation"))
_ca = ctypes.CDLL(ctypes.util.find_library("CoreAudio"))

_cf.CFDataCreate.restype = ctypes.c_void_p
_cf.CFDataCreate.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_long]
_cf.CFPropertyListCreateWithData.restype = ctypes.c_void_p
_cf.CFPropertyListCreateWithData.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong,
    ctypes.c_void_p, ctypes.c_void_p]
_cf.CFRelease.argtypes = [ctypes.c_void_p]
_cf.CFStringGetCString.restype = ctypes.c_bool
_cf.CFStringGetCString.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, ctypes.c_long, ctypes.c_uint32]

_ca.AudioHardwareCreateAggregateDevice.restype = ctypes.c_int32
_ca.AudioHardwareCreateAggregateDevice.argtypes = [
    ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
_ca.AudioObjectGetPropertyData.restype = ctypes.c_int32
_ca.AudioObjectGetPropertyData.argtypes = [
    ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32,
    ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32), ctypes.c_void_p]
_ca.AudioObjectGetPropertyDataSize.restype = ctypes.c_int32
_ca.AudioObjectGetPropertyDataSize.argtypes = [
    ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32,
    ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]


def _fourcc(s):
    v = 0
    for ch in s:
        v = (v << 8) | ord(ch)
    return v


class _PropAddr(ctypes.Structure):
    _fields_ = [("selector", ctypes.c_uint32),
                ("scope", ctypes.c_uint32),
                ("element", ctypes.c_uint32)]


_GLOBAL = _fourcc("glob")
_MAIN = 0
_SYSTEM_OBJECT = 1  # kAudioObjectSystemObject

_SEL_DEVICES = _fourcc("dev#")
_SEL_UID = _fourcc("uid ")
_SEL_NAME = _fourcc("lnam")
_SCOPE_INPUT = _fourcc("inpt")
_SCOPE_OUTPUT = _fourcc("outp")
_SEL_STREAMS = _fourcc("stm#")


def _addr(selector, scope=_GLOBAL):
    return _PropAddr(selector, scope, _MAIN)


def _cfstr_to_py(ref):
    if not ref:
        return None
    buf = ctypes.create_string_buffer(512)
    ok = _cf.CFStringGetCString(ref, buf, 512, 0x08000100)  # UTF8
    _cf.CFRelease(ref)
    return buf.value.decode("utf-8") if ok else None


def _device_ids():
    addr = _addr(_SEL_DEVICES)
    size = ctypes.c_uint32(0)
    if _ca.AudioObjectGetPropertyDataSize(_SYSTEM_OBJECT, ctypes.byref(addr),
                                          0, None, ctypes.byref(size)) != 0:
        return []
    n = size.value // 4
    ids = (ctypes.c_uint32 * n)()
    if _ca.AudioObjectGetPropertyData(_SYSTEM_OBJECT, ctypes.byref(addr),
                                      0, None, ctypes.byref(size), ids) != 0:
        return []
    return list(ids)


def _device_str_prop(dev_id, selector):
    addr = _addr(selector)
    ref = ctypes.c_void_p(0)
    size = ctypes.c_uint32(ctypes.sizeof(ref))
    if _ca.AudioObjectGetPropertyData(dev_id, ctypes.byref(addr),
                                      0, None, ctypes.byref(size),
                                      ctypes.byref(ref)) != 0:
        return None
    return _cfstr_to_py(ref)


def _has_streams(dev_id, scope):
    addr = _PropAddr(_SEL_STREAMS, scope, _MAIN)
    size = ctypes.c_uint32(0)
    if _ca.AudioObjectGetPropertyDataSize(dev_id, ctypes.byref(addr),
                                          0, None, ctypes.byref(size)) != 0:
        return False
    return size.value >= 4


def list_devices():
    """[(name, uid, has_input, has_output)] for every CoreAudio device."""
    out = []
    for did in _device_ids():
        name = _device_str_prop(did, _SEL_NAME)
        uid = _device_str_prop(did, _SEL_UID)
        if name and uid:
            out.append((name, uid,
                        _has_streams(did, _SCOPE_INPUT),
                        _has_streams(did, _SCOPE_OUTPUT)))
    return out


def create_pmor_aggregate(output_name_substr="MacBook Pro Speakers"):
    """Create (or find existing) the "PMOR" aggregate. Returns (ok, message)."""
    devs = list_devices()

    for name, _uid, has_in, has_out in devs:
        if name == AGG_NAME and has_in and has_out:
            return True, f"aggregate '{AGG_NAME}' already exists"

    pmor = next((d for d in devs if device_match.matches(d[0]) and d[2]), None)
    if pmor is None:
        return False, "PMOR capture device not found (is it plugged in?)"
    want = output_name_substr.lower()
    outdev = next((d for d in devs if want in d[0].lower() and d[3]), None)
    if outdev is None:
        return False, f"output device matching '{output_name_substr}' not found"

    desc = {
        "name": AGG_NAME,
        "uid": AGG_UID,
        # OUTPUT first and clock master: its clock is the timing reference,
        # so drift correction goes on the PMOR (non-master) row.
        "subdevices": [
            {"uid": outdev[1], "drift": 0},
            {"uid": pmor[1], "drift": 1},
        ],
        "master": outdev[1],
    }
    data = plistlib.dumps(desc, fmt=plistlib.FMT_XML)
    cfdata = _cf.CFDataCreate(None, data, len(data))
    if not cfdata:
        return False, "CFDataCreate failed"
    plist = _cf.CFPropertyListCreateWithData(None, cfdata, 0, None, None)
    _cf.CFRelease(cfdata)
    if not plist:
        return False, "property list conversion failed"
    agg_id = ctypes.c_uint32(0)
    status = _ca.AudioHardwareCreateAggregateDevice(plist, ctypes.byref(agg_id))
    _cf.CFRelease(plist)
    if status != 0:
        return False, f"AudioHardwareCreateAggregateDevice failed (OSStatus {status})"
    return True, (f"created aggregate '{AGG_NAME}' (id {agg_id.value}): "
                  f"[{outdev[0]} (master)] + [{pmor[0]} (drift-corrected)]")


if __name__ == "__main__":
    ok, msg = create_pmor_aggregate()
    print(("OK: " if ok else "FAILED: ") + msg)
    raise SystemExit(0 if ok else 1)

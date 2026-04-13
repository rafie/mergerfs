"""Check volume flags for reparse point support."""
import ctypes

for drive in ["M:", "C:", "N:"]:
    root = drive + "\\"
    buf = ctypes.create_unicode_buffer(256)
    fsbuf = ctypes.create_unicode_buffer(256)
    flags = ctypes.c_uint32()
    maxlen = ctypes.c_uint32()
    ok = ctypes.windll.kernel32.GetVolumeInformationW(
        root, buf, 256, None, ctypes.byref(maxlen),
        ctypes.byref(flags), fsbuf, 256)
    if ok:
        f = flags.value
        print(f"{drive} vol={buf.value!r} fs={fsbuf.value!r} flags={hex(f)}")
        print(f"  REPARSE_POINTS: {bool(f & 0x80)}")
        print(f"  CASE_SENSITIVE: {bool(f & 0x1)}")
        print(f"  UNICODE: {bool(f & 0x4)}")
        print(f"  PERSISTENT_ACLS: {bool(f & 0x8)}")
        print(f"  NAMED_STREAMS: {bool(f & 0x40000)}")
    else:
        err = ctypes.windll.kernel32.GetLastError()
        print(f"{drive} GetVolumeInformation failed: err={err}")

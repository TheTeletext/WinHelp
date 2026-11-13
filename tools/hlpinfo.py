#!/usr/bin/env python3
"""hlpinfo.py - characterise WinHelp .HLP files (header, |SYSTEM, internal files).

Diagnostic tool for the fidelity audit; written for this project from the
published format description (Pete Davis / Mike Wallace "Windows Undocumented
File Formats" and Manfred Winterhoff's HELPFILE.TXT). Not shipped.

Usage: python -I hlpinfo.py FILE... [--json out.json]
"""
import json
import struct
import sys

HLP_MAGIC = 0x00035F3F
SYS_RECORDS = {1: "TITLE", 2: "COPYRIGHT", 3: "CONTENTS", 4: "CONFIG", 5: "ICON",
               6: "WINDOW", 8: "CITATION", 9: "LCID", 10: "CNT", 11: "CHARSET",
               12: "DEFFONT", 13: "GROUPS", 14: "KEYINDEX", 18: "LANGUAGE", 19: "DLLMAPS"}


def cstr(b, off):
    end = b.index(b"\0", off)
    return b[off:end].decode("cp1252", "replace"), end + 1


def read_dir(b):
    magic, dirstart, _free, size = struct.unpack_from("<IiiI", b, 0)
    if magic != HLP_MAGIC:
        raise ValueError("not a WinHelp file (magic %08x)" % magic)
    p = dirstart + 9  # skip FILEHEADER (reserved, used, flags)
    (bmagic, _flags, pagesize, _struct, _z, _splits, root, _neg, totalpages,
     nlevels, entries) = struct.unpack_from("<HHH16sHHHhHHI", b, p)
    if bmagic != 0x293B:
        raise ValueError("bad directory B+ tree magic %04x" % bmagic)
    pages = p + 38
    files = {}
    # descend to the first leaf, then follow NextPage links
    page = root
    for _ in range(nlevels - 1):
        page = struct.unpack_from("<H", b, pages + page * pagesize + 4)[0]
    while page != 0xFFFF:
        base = pages + page * pagesize
        _unused, n, _prev, nxt = struct.unpack_from("<HHhH", b, base)
        q = base + 8
        for _ in range(n):
            name, q = cstr(b, q)
            files[name] = struct.unpack_from("<I", b, q)[0]
            q += 4
        page = nxt
    return files, size


def read_system(b, off):
    _res, used, _fl = struct.unpack_from("<IIB", b, off)
    p = off + 9
    magic, minor, major, gendate, flags = struct.unpack_from("<HHHIH", b, p)
    info = {"minor": minor, "major": major, "flags": flags, "gendate": gendate,
            "records": {}, "macros": [], "windows": []}
    p += 12
    end = off + 9 + used
    if minor <= 16:
        info["title"], _ = cstr(b, p)
        return info
    while p + 4 <= end:
        rtype, size = struct.unpack_from("<HH", b, p)
        data = b[p + 4:p + 4 + size]
        name = SYS_RECORDS.get(rtype, str(rtype))
        info["records"][name] = info["records"].get(name, 0) + 1
        if rtype == 1:
            info["title"] = data.split(b"\0")[0].decode("cp1252", "replace")
        elif rtype == 2:
            info["copyright"] = data.split(b"\0")[0].decode("cp1252", "replace")
        elif rtype == 4:
            info["macros"].append(data.split(b"\0")[0].decode("cp1252", "replace"))
        elif rtype == 6:
            wflags = struct.unpack_from("<H", data, 0)[0]
            wtype = data[2:12].split(b"\0")[0].decode("cp1252", "replace")
            wname = data[12:21].split(b"\0")[0].decode("cp1252", "replace")
            wcap = data[21:72].split(b"\0")[0].decode("cp1252", "replace")
            x, y, w, h, maxi = struct.unpack_from("<HHHHH", data, 72)
            info["windows"].append({"type": wtype, "name": wname, "caption": wcap,
                                    "flags": wflags, "xywh": [x, y, w, h], "max": maxi})
        p += 4 + size
    return info


def describe(path):
    b = open(path, "rb").read()
    r = {"file": path, "size": len(b)}
    if b[:2] == b"LN" or b[:2] == b"\x4c\x4e":
        r["format"] = "QuickHelp (DOS), not WinHelp"
        return r
    try:
        files, size = read_dir(b)
    except Exception as e:  # noqa: BLE001 - diagnostic tool
        r["format"] = "unknown: %s" % e
        return r
    r["format"] = "WinHelp"
    r["internal"] = sorted(files)
    if "|SYSTEM" in files:
        s = read_system(b, files["|SYSTEM"])
        r.update(s)
        r["version"] = {15: "3.0", 21: "3.1", 27: "4.0", 33: "MVB"}.get(s["minor"], "minor %d" % s["minor"])
        r["compression"] = ("none" if s["flags"] == 0 else "LZ77 4k" if s["flags"] == 4
                            else "LZ77 2k" if s["flags"] == 8 else "flags %d" % s["flags"])
    r["bitmaps"] = len([f for f in files if f.startswith("|bm") or f.startswith("bm")])
    return r


def main(argv):
    out = None
    if "--json" in argv:
        i = argv.index("--json")
        out = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    rows = [describe(p) for p in argv]
    for r in rows:
        if r.get("format") != "WinHelp":
            print("%-48s %s" % (r["file"][-48:], r["format"]))
            continue
        print("%-48s v%-4s %-8s bm%-3d win%-2d mac%-2d %s" % (
            r["file"][-48:], r.get("version"), r.get("compression"), r["bitmaps"],
            len(r.get("windows", [])), len(r.get("macros", [])), r.get("title", "")))
    if out:
        json.dump(rows, open(out, "w"), indent=1)


if __name__ == "__main__":
    main(sys.argv[1:])

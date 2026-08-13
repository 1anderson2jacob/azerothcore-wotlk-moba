#!/usr/bin/env python3.10
"""Build the WMO-only WDT that places an exported .wmo on a map.

    python3.10 gen_wdt.py           write the WDT
    python3.10 gen_wdt.py --dump    write it, then re-read and print every chunk

Reads the bounding box and rootWMOID back out of the exported root rather than
taking them on trust: MODF's bound gates every collision query through
ModelInstance::GetLocationInfo's iBound.contains, so a bound that drifts smaller
than the geometry silently kills collision outside it.

Chunk layout is what StormwindPrison, DeeprunTram and AlliancePVPBarracks carry.

Overrides: WBS_PROJECT (default ~/tools/wbs-project).
"""
import os, struct, sys

PROJECT     = os.environ.get("WBS_PROJECT", os.path.expanduser("~/tools/wbs-project"))
MAP_DIR     = "TwistedTreeline"                                   # World\Maps\<dir>\<dir>.wdt
WMO_PATH    = "World\\wmo\\TwistedTreeline\\TwistedTreeline.wmo"  # exactly what MWMO ships
ROOT_WMO_ID = 9000

WDT_VERSION     = 18            # the WMO's own MVER is 17; these are different formats
MPHD_GLOBAL_WMO = 0x1
GRID            = 533.33333


def chunk(tag, payload):
    return tag[::-1].encode("ascii") + struct.pack("<I", len(payload)) + payload


def read_chunks(raw):
    out, p = {}, 0
    while p + 8 <= len(raw):
        tag = raw[p:p + 4][::-1].decode("ascii", "replace")
        size, = struct.unpack_from("<I", raw, p + 4)
        out.setdefault(tag, raw[p + 8:p + 8 + size])
        p += 8 + size
    return out


def tiles_spanned(lo, hi):
    a, b = int(32 - hi / GRID), int(32 - lo / GRID)
    return range(min(a, b), max(a, b) + 1)


def main(dump=False):
    wmo_file = os.path.join(PROJECT, WMO_PATH.replace("\\", os.sep))
    if not os.path.isfile(wmo_file):
        sys.exit("root wmo not found: %s" % wmo_file)

    got = read_chunks(open(wmo_file, "rb").read())
    if "MOHD" not in got:
        sys.exit("%s carries no MOHD" % wmo_file)
    n_groups, = struct.unpack_from("<I", got["MOHD"], 4)
    root_id,  = struct.unpack_from("<I", got["MOHD"], 32)
    bb_min    = struct.unpack_from("<3f", got["MOHD"], 36)
    bb_max    = struct.unpack_from("<3f", got["MOHD"], 48)

    if root_id != ROOT_WMO_ID:
        sys.exit("rootWMOID is %d, expected %d -- re-export before generating the WDT"
                 % (root_id, ROOT_WMO_ID))
    stem = wmo_file[:-4]
    missing = [i for i in range(n_groups) if not os.path.isfile("%s_%03d.wmo" % (stem, i))]
    if missing:
        sys.exit("MOHD declares %d groups but %d group file(s) are missing: %s"
                 % (n_groups, len(missing), missing[:8]))

    # MOHD's box is fixCoords() of the placement box (vmap4_extractor/wmo.h:68),
    # so MODF wants the permutation undone.
    modf_min = (bb_min[1], bb_min[2], bb_min[0])
    modf_max = (bb_max[1], bb_max[2], bb_max[0])

    modf = struct.pack(
        "<II3f3f3f3f4H",
        0,                    # nameId -> MWMO entry 0
        0xFFFFFFFF,           # uniqueId, what every stock global WMO uses
        0.0, 0.0, 0.0,        # position: the extractor rewrites (0,0,0) to the grid
                              # centre, which lands the WMO origin on server (0,0,0)
        0.0, 0.0, 0.0,        # rotation
        *modf_min, *modf_max,
        0,                    # flags: bit 0 means destructible, which MapObject::Extract drops
        0, 0, 0)              # doodadSet, nameSet, scale

    wdt = (chunk("MVER", struct.pack("<I", WDT_VERSION))
           + chunk("MPHD", struct.pack("<8I", MPHD_GLOBAL_WMO, 0, 0, 0, 0, 0, 0, 0))
           + chunk("MAIN", b"\0" * (64 * 64 * 8))
           + chunk("MWMO", WMO_PATH.encode("ascii") + b"\0")
           + chunk("MODF", modf))

    out_dir = os.path.join(PROJECT, "World", "Maps", MAP_DIR)
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, MAP_DIR + ".wdt")
    with open(out, "wb") as f:
        f.write(wdt)

    sx = (-bb_max[0], -bb_min[0])
    sy = (-bb_max[1], -bb_min[1])
    tx, ty = tiles_spanned(*sx), tiles_spanned(*sy)
    print("wrote %s (%d bytes)" % (out, len(wdt)))
    print("  groups       : %d" % n_groups)
    print("  rootWMOID    : %d" % root_id)
    print("  model bbox   : x[%.2f, %.2f] y[%.2f, %.2f] z[%.2f, %.2f]"
          % (bb_min[0], bb_max[0], bb_min[1], bb_max[1], bb_min[2], bb_max[2]))
    print("  MODF bound   : %s .. %s"
          % (tuple(round(v, 2) for v in modf_min), tuple(round(v, 2) for v in modf_max)))
    print("  server extent: X[%.2f, %.2f] Y[%.2f, %.2f] Z[%.2f, %.2f]"
          % (sx[0], sx[1], sy[0], sy[1], bb_min[2], bb_max[2]))
    print("  mmaps tiles  : %d (x %d..%d, y %d..%d)"
          % (len(tx) * len(ty), tx[0], tx[-1], ty[0], ty[-1]))

    if dump:
        print("\n-- re-read --")
        raw = open(out, "rb").read()
        p = 0
        while p + 8 <= len(raw):
            tag = raw[p:p + 4][::-1].decode("ascii", "replace")
            size, = struct.unpack_from("<I", raw, p + 4)
            extra = ""
            if tag == "MVER":
                extra = " version=%d" % struct.unpack_from("<I", raw, p + 8)[0]
            elif tag == "MPHD":
                extra = " flags=0x%X" % struct.unpack_from("<I", raw, p + 8)[0]
            elif tag == "MAIN":
                n = sum(1 for i in range(64 * 64)
                        if struct.unpack_from("<I", raw, p + 8 + i * 8)[0] & 1)
                extra = " tiles_with_exist_bit=%d" % n
            elif tag == "MWMO":
                extra = " %r" % raw[p + 8:p + 8 + size].split(b"\0")[0].decode()
            elif tag == "MODF":
                v = struct.unpack_from("<II3f3f3f3f4H", raw, p + 8)
                extra = (" nameId=%d uniqueId=0x%X pos=%s rot=%s flags=0x%X nameSet=%d"
                         % (v[0], v[1], v[2:5], v[5:8], v[14], v[16]))
            print("  %s size=%-6d%s" % (tag, size, extra))
            p += 8 + size
    return 0


if __name__ == "__main__":
    sys.exit(main("--dump" in sys.argv[1:]))

#!/usr/bin/env python3.10
"""Offline check of an exported WMO against what the 3.3.5a client and
AzerothCore's vmap4extractor actually require.

    python3.10 wmo_verify.py <root.wmo>

Replays the extractor's own decisions (wmo.cpp ConvertToVMAPGroupWmo /
ShouldSkip, model.cpp Doodad::ExtractSet) so a dead map is caught here rather
than after the MPQ pack + extract round trip.
"""
import os, struct, sys

MPQ_DIR   = os.path.expanduser("~/Games/wow335/Data")
STORM_DIR = os.path.expanduser(
    "~/tools/blender-wow-studio/io_scene_wmo/pywowlib/archives/mpq/native")
ARCHIVES  = ["patch-3.MPQ", "patch-2.MPQ", "patch.MPQ", "lichking.MPQ",
             "expansion.MPQ", "common-2.MPQ", "common.MPQ"]

F_DETAIL, F_COLLISION, F_RENDER = 0x04, 0x08, 0x20
MOGP_UNREACHABLE, MOGP_ANTIPORTAL, MOGP_OUTDOOR, MOGP_HASCOLLISION = 0x80, 0x4000000, 0x8, 0x1

ROOT_WMO_ID    = 9000              # must match blender_staging_setup.py
ORIENT_PROBE   = "TT_JWall_14"
ORIENT_PROBE_Y = -55.46            # its centre y once the scene is in the server frame

fails, warns = [], []
def fail(m): fails.append(m); print("  FAIL  " + m)
def warn(m): warns.append(m); print("  warn  " + m)


# ---------------------------------------------------------------- MPQ access
class Client:
    def __init__(self):
        self.h = []
        try:
            sys.path.insert(0, STORM_DIR)
            import storm
            self.storm = storm
            for a in ARCHIVES:
                p = os.path.join(MPQ_DIR, a)
                if os.path.exists(p):
                    self.h.append(storm.SFileOpenArchive(p, 0, 0))
        except Exception as e:                                  # noqa: BLE001
            self.storm = None
            warn("client MPQs unavailable (%s); asset checks skipped" % e)

    def read(self, path, nbytes=None):
        if not self.h:
            return None
        for a in self.h:
            if self.storm.SFileHasFile(a, path):
                f = self.storm.SFileOpenFileEx(a, path, 0)
                n = self.storm.SFileGetFileSize(f)
                d = self.storm.SFileReadFile(f, min(n, nbytes) if nbytes else n)
                self.storm.SFileCloseFile(f)
                return d
        return None

    def has(self, path):
        return any(self.storm.SFileHasFile(a, path) for a in self.h) if self.h else None

M2_FMT = "<4s4B38I14f6I"
def m2_bounding_triangles(client, path):
    """nBoundingTriangles, i.e. whether vmap4extractor's Model::open accepts it."""
    d = client.read(path, struct.calcsize(M2_FMT))
    if d is None or len(d) < struct.calcsize(M2_FMT):
        return None
    v = struct.unpack(M2_FMT, d[:struct.calcsize(M2_FMT)])
    return v[57] if v[0] == b"MD20" else None


# ---------------------------------------------------------------- chunk walk
def chunks(buf, mogp_is_header=False):
    p = 0
    while p + 8 <= len(buf):
        magic = buf[p:p + 4][::-1].decode("ascii", "replace")
        size, = struct.unpack_from("<I", buf, p + 4)
        data_at = p + 8
        if mogp_is_header and magic == "MOGP":
            size = 68                                # extractor does the same
        yield magic, buf[data_at:data_at + size]
        p = data_at + size

def strings(block):
    """offset -> string, for MOTX / MOGN / MODN string tables."""
    out, start = {}, 0
    for i, b in enumerate(block):
        if b == 0:
            if i > start:
                out[start] = block[start:i].decode("ascii", "replace")
            start = i + 1
    return out

def zstr(block, ofs):
    end = block.find(b"\0", ofs)
    return block[ofs:end if end >= 0 else len(block)].decode("ascii", "replace")


# ---------------------------------------------------------------- main
def main(root_path):
    client = Client()
    raw = open(root_path, "rb").read()
    got = {}
    for magic, data in chunks(raw):
        got.setdefault(magic, b"")
        got[magic] += data

    print("\n== ROOT %s (%d bytes) ==" % (os.path.basename(root_path), len(raw)))
    print("  chunks: " + " ".join("%s:%d" % (k, len(v)) for k, v in got.items()))

    ver, = struct.unpack("<I", got["MVER"])
    print("  MVER version = %d" % ver)
    if ver != 17:
        fail("MVER is %d, the 3.3.5a client needs 17" % ver)

    (nTex, nGroups, nPortals, nLights, nDoodadNames, nDoodadDefs, nDoodadSets,
     color, rootId) = struct.unpack_from("<9I", got["MOHD"], 0)
    bb1 = struct.unpack_from("<3f", got["MOHD"], 36)
    bb2 = struct.unpack_from("<3f", got["MOHD"], 48)
    flags, = struct.unpack_from("<I", got["MOHD"], 60)
    print("  MOHD  textures=%d groups=%d portals=%d lights=%d" % (nTex, nGroups, nPortals, nLights))
    print("        doodad names=%d defs=%d sets=%d  rootWMOID=%d flags=0x%X"
          % (nDoodadNames, nDoodadDefs, nDoodadSets, rootId, flags))
    print("        bbox %s .. %s" % (tuple(round(v, 1) for v in bb1), tuple(round(v, 1) for v in bb2)))
    if nGroups == 0:
        fail("MOHD declares 0 groups")
    if rootId != ROOT_WMO_ID:
        fail("rootWMOID is %d, expected %d -- WMOAreaTable will not resolve" % (rootId, ROOT_WMO_ID))
    if rootId > 32767:
        fail("rootWMOID %d exceeds int16; GetWMOAreaTableEntryByTripple narrows the key" % rootId)

    # -- textures -----------------------------------------------------------
    motx = got.get("MOTX", b"")
    momt = got.get("MOMT", b"")
    n_mat = len(momt) // 64
    print("\n-- materials (%d) --" % n_mat)
    if n_mat != nTex:
        warn("MOHD.nTextures=%d but MOMT holds %d materials" % (nTex, n_mat))
    for i in range(n_mat):
        f_, shader, blend, t1 = struct.unpack_from("<4I", momt, i * 64)
        path = zstr(motx, t1)
        ok = client.has(path.replace("/", "\\")) if client.h else None
        mark = {True: "ok ", False: "MISSING", None: "?  "}[ok]
        print("  [%d] shader=%d blend=%d  %s  %s" % (i, shader, blend, mark, path))
        if ok is False:
            fail("MOTX texture not in the client MPQs: %s" % path)

    # -- group info ---------------------------------------------------------
    mogi, mogn = got.get("MOGI", b""), got.get("MOGN", b"")
    names = []
    print("\n-- MOGI (%d) --" % (len(mogi) // 32))
    for i in range(len(mogi) // 32):
        gflags, = struct.unpack_from("<I", mogi, i * 32)
        nofs, = struct.unpack_from("<i", mogi, i * 32 + 28)
        nm = zstr(mogn, nofs) if nofs >= 0 else "<none>"
        names.append(nm)
        print("  [%02d] %-24s flags=0x%X" % (i, nm, gflags))

    # -- doodads ------------------------------------------------------------
    mods, modn, modd = got.get("MODS", b""), got.get("MODN", b""), got.get("MODD", b"")
    sets = []
    print("\n-- doodads --")
    for i in range(len(mods) // 32):
        nm = mods[i * 32:i * 32 + 20].split(b"\0")[0].decode("ascii", "replace")
        start, count = struct.unpack_from("<2I", mods, i * 32 + 20)
        sets.append((nm, start, count))
        print("  set[%d] %-24s start=%d count=%d" % (i, nm, start, count))
    if not sets:
        fail("no MODS doodad set; the WDT's MODF.DoodadSet index will not resolve")
    elif sets[0][0] != "Set_$DefaultGlobal":
        warn("set 0 is %r, not Set_$DefaultGlobal" % sets[0][0])

    valid_name_offsets, spawns = set(), []
    for ofs, path in sorted(strings(modn).items()):
        probe = path
        if probe[-4:].lower() in (".mdx", ".mdl"):
            probe = probe[:-2] + "2"
        nbt = m2_bounding_triangles(client, probe) if client.h else None
        if nbt is None and client.h:
            fail("MODN model not readable from the MPQs: %s" % probe)
        elif nbt == 0:
            warn("%s has 0 bounding triangles: renders in the client, dropped from vmaps" % probe)
        else:
            valid_name_offsets.add(ofs)
        print("  MODN @%d  %s  boundTris=%s" % (ofs, path, nbt))
    for i in range(len(modd) // 40):
        packed, = struct.unpack_from("<I", modd, i * 40)
        pos = struct.unpack_from("<3f", modd, i * 40 + 4)
        rot = struct.unpack_from("<4f", modd, i * 40 + 16)
        scale, = struct.unpack_from("<f", modd, i * 40 + 32)
        name_ofs = packed & 0xFFFFFF
        spawns.append(name_ofs)
        print("  MODD[%d] name@%d pos=%s quat=%s scale=%.2f"
              % (i, name_ofs, tuple(round(v, 1) for v in pos), tuple(round(v, 2) for v in rot), scale))

    # -- groups -------------------------------------------------------------
    stem = root_path[:-4]
    total_tris = total_coll = kept_groups = 0
    probe_c = None
    references = set()
    print("\n-- groups --")
    for gi in range(nGroups):
        gp = "%s_%03d.wmo" % (stem, gi)
        if not os.path.exists(gp):
            fail("missing group file %s" % os.path.basename(gp))
            continue
        gbuf = open(gp, "rb").read()
        g = {}
        for magic, data in chunks(gbuf, mogp_is_header=True):
            g.setdefault(magic, b"")
            g[magic] += data
        gver, = struct.unpack("<I", g["MVER"])
        if gver != 17:
            fail("%s MVER is %d, need 17" % (os.path.basename(gp), gver))
        gflags, = struct.unpack_from("<I", g["MOGP"], 8)
        if names[gi] == ORIENT_PROBE:
            gbb = struct.unpack_from("<6f", g["MOGP"], 12)
            probe_c = tuple((gbb[i] + gbb[i + 3]) / 2.0 for i in range(3))

        mopy = g.get("MOPY", b"")
        n_tri = len(mopy) // 2
        n_coll = 0
        for i in range(n_tri):
            fl, mid = mopy[2 * i], mopy[2 * i + 1]
            is_render = (fl & F_RENDER) and not (fl & F_DETAIL)
            if (fl & F_COLLISION) or is_render or mid == 0xFF:
                n_coll += 1
        n_vert = len(g.get("MOVT", b"")) // 12
        modr = g.get("MODR", b"")
        refs = list(struct.unpack("<%dH" % (len(modr) // 2), modr)) if modr else []

        # The two 16-bit limits a group can silently exceed. MOVI indexes
        # vertices in uint16; a MOBA batch counts MOVI indices in uint16 and WBS
        # emits one batch per material, so an oversized group ships a wrapped
        # count and the client draws the remainder of it. Neither shows up
        # anywhere else -- the geometry and the collision BSP are complete.
        moba = g.get("MOBA", b"")
        n_idx = len(g.get("MOVI", b"")) // 2
        batched = sum(struct.unpack_from("<H", moba, i * 24 + 16)[0]
                      for i in range(len(moba) // 24))
        if n_idx and batched != n_idx:
            fail("group %d %r: batches cover %d of %d MOVI indices, so the client"
                 " draws %.0f%% of it" % (gi, names[gi], batched, n_idx,
                                          100.0 * batched / n_idx))
        if n_vert > 65535:
            fail("group %d %r has %d vertices; MOVI indexes them in uint16"
                 % (gi, names[gi], n_vert))

        if not gflags & MOGP_HASCOLLISION:
            fail("group %d %r MOGP flags=0x%X: no HASCOLLISION(0x1), the client crashes"
                 % (gi, names[gi], gflags))
        if not gflags & MOGP_OUTDOOR:
            warn("group %d %r is not flagged OUTDOOR(0x8); every TT group should be"
                 % (gi, names[gi]))

        skip = bool(gflags & MOGP_UNREACHABLE or gflags & MOGP_ANTIPORTAL
                    or names[gi] == "antiportal")
        if skip:
            fail("group %d %r would be dropped by ShouldSkip (flags=0x%X)" % (gi, names[gi], gflags))
        total_tris += n_tri
        if not skip:
            kept_groups += 1
            total_coll += n_coll
            for r in refs:
                if r < len(spawns) and spawns[r] in valid_name_offsets:
                    references.add(r)
        print("  [%02d] %-22s flags=0x%-9X tris=%-6d collision=%-6d verts=%-6d doodadRefs=%s%s"
              % (gi, names[gi], gflags, n_tri, n_coll, n_vert, refs or "-",
                 "  SKIPPED" if skip else ""))
        if not skip and n_coll == 0 and n_tri:
            warn("group %r contributes no collision triangles" % names[gi])

    # -- ExtractSet replay --------------------------------------------------
    print("\n-- Doodad::ExtractSet replay (WDT MODF.DoodadSet = 0) --")
    emitted = 0
    if sets:
        _, start, count = sets[0]
        for r in sorted(references):
            if start <= r < start + count:
                emitted += 1
                print("  would emit doodad index %d -> %s" % (r, zstr(modn, spawns[r])))
    if not emitted:
        warn("no doodad reaches the vmap output")

    print("\n== SUMMARY ==")
    print("  groups kept by ShouldSkip : %d / %d" % (kept_groups, nGroups))
    print("  triangles total           : %d" % total_tris)
    print("  triangles kept as COLLISION: %d" % total_coll)
    print("  doodads emitted to vmaps  : %d" % emitted)
    if probe_c is None:
        warn("orientation probe %r not among the groups" % ORIENT_PROBE)
    else:
        # All three centres printed because only [0]/[3] were ever confirmed to
        # be x. TT_JWall_14 is (-106.8, -55.5, +3.0) -- distinct enough that a
        # permuted box shows up as y reading one of the other two.
        print("  %-26s: centre (%+.1f, %+.1f, %+.1f), y wants %+.1f"
              % (ORIENT_PROBE, probe_c[0], probe_c[1], probe_c[2], ORIENT_PROBE_Y))
        if abs(probe_c[1] - ORIENT_PROBE_Y) > 1.0:
            fail("model is not in the server frame; the map lands rotated 180 deg")
    if total_coll == 0:
        fail("ZERO collision triangles - the map would look perfect and be unwalkable")
    print("  %d fail, %d warn" % (len(fails), len(warns)))
    print("  " + ("PASS" if not fails else "FAIL"))
    return 1 if fails else 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1]))

#!/usr/bin/env python3.10
"""Read-only queries against the 3.3.5a client MPQs.

    mpq_tool.py index [OUT]           write every archive entry to OUT (default stdout)
    mpq_tool.py find SUBSTRING...     print entries matching all substrings
    mpq_tool.py probe [--json] PATH...  M2 collision header: bounding triangles + boxes
    mpq_tool.py extract PATH OUTDIR   extract a file; for .wmo also pulls its groups

Needs pywowlib's compiled StormLib binding, which is built for Blender 3.4's
interpreter -- so run this with python3.10, not the system python.

Overrides: WOW_DATA (client Data dir), WBS_ROOT (blender-wow-studio checkout),
WOW_LOCALE (client locale, default enUS).
"""
import json, os, re, struct, sys

DATA   = os.environ.get("WOW_DATA", os.path.expanduser("~/Games/wow335/Data"))
WBS    = os.environ.get("WBS_ROOT", os.path.expanduser("~/tools/blender-wow-studio"))
LOCALE = os.environ.get("WOW_LOCALE", "enUS")
STORM  = os.path.join(WBS, "io_scene_wmo/pywowlib/archives/mpq/native")

# Highest priority first: the locale chain outranks the base one, mirroring
# vmap4extractor. Full precedence chain in README.md, step 7.
ARCHIVES = ["%s/patch-%s-4.MPQ" % (LOCALE, LOCALE),   # this fork's client patch
            "%s/patch-%s-3.MPQ" % (LOCALE, LOCALE),
            "%s/patch-%s-2.MPQ" % (LOCALE, LOCALE),
            "%s/patch-%s.MPQ"   % (LOCALE, LOCALE),
            "patch-3.MPQ", "patch-2.MPQ", "patch.MPQ",
            "lichking.MPQ", "expansion.MPQ", "common-2.MPQ", "common.MPQ",
            "%s/lichking-locale-%s.MPQ"  % (LOCALE, LOCALE),
            "%s/expansion-locale-%s.MPQ" % (LOCALE, LOCALE),
            "%s/locale-%s.MPQ"           % (LOCALE, LOCALE)]


def open_archives():
    sys.path.insert(0, STORM)
    try:
        import storm
    except ImportError:
        sys.exit("cannot import storm from %s -- is WBS built, and is this python3.10?" % STORM)
    handles = []
    for a in ARCHIVES:
        p = os.path.join(DATA, a)
        if os.path.exists(p):
            handles.append(storm.SFileOpenArchive(p, 0, 0))
    if not handles:
        sys.exit("no MPQs found under %s" % DATA)
    return storm, handles


def read(storm, handles, path, nbytes=None):
    """None when no archive holds the file, b"" when one does and the read
    failed.

    StormLib RAISES on a file shorter than the count asked for rather than
    returning short, so without this one bad model aborts the whole batch --
    a sweep over every doodad in the archives trips about 26 of them."""
    for a in handles:
        if not storm.SFileHasFile(a, path):
            continue
        f = None
        try:
            f = storm.SFileOpenFileEx(a, path, 0)
            size = storm.SFileGetFileSize(f)
            return storm.SFileReadFile(f, min(size, nbytes) if nbytes else size)
        except storm.error:
            return b""
        finally:
            if f is not None:
                storm.SFileCloseFile(f)
    return None


def listing(storm, handles):
    out = set()
    for a in handles:
        h, f = storm.SFileFindFirstFile(a, "", "*")
        while True:
            out.add(f)
            try:
                f = storm.SFileFindNextFile(h)
            except storm.NoMoreFilesError:
                break
    return sorted(out)


# vmap4_extractor/modelheaders.h ModelHeader, truncated after the bounding block
M2_FMT = "<4s4B38I14f6I"


def cmd_index(argv):
    storm, h = open_archives()
    entries = listing(storm, h)
    if argv:
        with open(argv[0], "w") as fh:
            fh.write("\n".join(entries) + "\n")
        print("%d entries -> %s" % (len(entries), argv[0]), file=sys.stderr)
    else:
        print("\n".join(entries))


def cmd_find(argv):
    if not argv:
        sys.exit("find needs at least one substring")
    storm, h = open_archives()
    pats = [a.lower() for a in argv]
    n = 0
    for e in listing(storm, h):
        low = e.lower()
        if all(p in low for p in pats):
            print(e)
            n += 1
    print("%d match" % n, file=sys.stderr)


def cmd_probe(argv):
    """nBoundingTriangles is what vmap4extractor's Model::open tests: a model
    with zero renders in the client but never reaches the vmaps.

    Both CAaBoxes are reported. box_b collapses to all zeros exactly when
    nBoundingTriangles is 0, which is what identifies them: box_a bounds the
    render geometry, box_b the collision hull. box_a is NOT tight -- some models
    claim over a hundred yards across -- so it sizes a height and never a
    footprint."""
    as_json = "--json" in argv
    paths = [a for a in argv if a != "--json"]
    if not paths:
        sys.exit("probe needs at least one model path")
    storm, h = open_archives()
    size = struct.calcsize(M2_FMT)
    out = {}
    for path in paths:
        probe = path[:-2] + "2" if path[-4:].lower() in (".mdx", ".mdl") else path
        d = read(storm, h, probe, size)
        rec, why = None, None
        if d is None:
            why = "MISSING"
        elif len(d) < size:
            why = "UNREADABLE"
        else:
            v = struct.unpack(M2_FMT, d[:size])
            if v[0] != b"MD20":
                why = "NOT M2"
            else:
                fl = v[43:57]
                rec = {"version": v[1], "bound_tris": v[57], "bound_verts": v[59],
                       "box_a": [list(fl[0:3]), list(fl[3:6])],
                       "box_b": [list(fl[7:10]), list(fl[10:13])]}
        if as_json:
            out[path] = rec
            continue
        if rec is None:
            print("  %-8s %s" % (why, probe))
            continue
        lo, hi = rec["box_b"]
        print("  MD20 v%-3d boundTris=%-6d boundVerts=%-6d  box z %7.2f..%7.2f"
              "  xy %6.2f x %6.2f  %s"
              % (rec["version"], rec["bound_tris"], rec["bound_verts"],
                 lo[2], hi[2], hi[0] - lo[0], hi[1] - lo[1], probe))
    if as_json:
        print(json.dumps(out))


def cmd_extract(argv):
    if len(argv) != 2:
        sys.exit("extract needs PATH and OUTDIR")
    src, outdir = argv
    storm, h = open_archives()
    os.makedirs(outdir, exist_ok=True)
    name = src.replace("\\", "/").rsplit("/", 1)[-1]

    def grab(path, dest):
        d = read(storm, h, path)
        if d is None:
            return None
        with open(dest, "wb") as fh:
            fh.write(d)
        return len(d)

    n = grab(src, os.path.join(outdir, name))
    if n is None:
        sys.exit("not in any archive: %s" % src)
    print("%8d  %s" % (n, name))
    if src.lower().endswith(".wmo") and not re.search(r"_\d{3}\.wmo$", src, re.I):
        stem, base = src[:-4], name[:-4]
        for i in range(512):
            gn = "%s_%03d.wmo" % (base, i)
            if grab("%s_%03d.wmo" % (stem, i), os.path.join(outdir, gn)) is None:
                print("%d group files" % i)
                break


CMDS = {"index": cmd_index, "find": cmd_find, "probe": cmd_probe, "extract": cmd_extract}

if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] not in CMDS:
        sys.exit(__doc__)
    CMDS[sys.argv[1]](sys.argv[2:])

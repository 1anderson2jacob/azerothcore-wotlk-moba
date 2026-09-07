#!/usr/bin/env python3.10
"""Add this fork's rows to the client's DBCs, for the MPQ patch.

    python3.10 dbc_tool.py dump Map.dbc 566     print one row field by field
    python3.10 dbc_tool.py patch                write every patched DBC to the staging dir

The server does not read these -- its rows come from data/sql/custom/db_world/
mod_moba_map.sql. This exists because the CLIENT cannot load a map without the
Map.dbc row, and because vmap4extractor enumerates maps from Map.dbc to find the
WDT, so the row has to be inside the MPQ before extraction can see the map at all.

Rows are CLONED from a comparable stock row and then overridden. Cloning is the
point: it carries every field whose meaning we have not established -- locale
masks, loading screen, corpse coordinates, ambience -- at a value already known
to work on a live map, instead of a guess.

Needs pywowlib's compiled StormLib binding, so run with python3.10.
Overrides: WOW_DATA, WBS_ROOT, WBS_PROJECT.
"""
import os, struct, sys

DATA    = os.environ.get("WOW_DATA", os.path.expanduser("~/Games/wow335/Data"))
WBS     = os.environ.get("WBS_ROOT", os.path.expanduser("~/tools/blender-wow-studio"))
PROJECT = os.environ.get("WBS_PROJECT", os.path.expanduser("~/tools/wbs-project"))
STORM   = os.path.join(WBS, "io_scene_wmo/pywowlib/archives/mpq/native")

# DBCs live in the locale archives, not the base ones -- searched newest first.
ARCHIVES = ["enUS/patch-enUS-3.MPQ", "enUS/patch-enUS-2.MPQ", "enUS/patch-enUS.MPQ",
            "enUS/lichking-locale-enUS.MPQ", "enUS/expansion-locale-enUS.MPQ",
            "enUS/locale-enUS.MPQ"]

MAP_ID      = 900
MAP_DIR     = "TwistedTreeline"
MAP_NAME    = "Twisted Treeline"
AREA_ID     = 5000
AREA_BIT    = 3000
AREA_FLAGS  = 0x04000000       # AREA_FLAG_OUTSIDE
ROOT_WMO_ID = 9000
WMOAREA_ID  = 51200            # +1 is the per-group row
LIGHT_ID    = 3000
EXPANSION   = 2                # WotLK
# The battleground slot. These must match the server's copy of the row in
# data/sql/custom/db_world/mod_moba_bg_map.sql, and the levels must ALSO match
# battleground_template's MinLvl/MaxLvl (base_config.yaml min_level/max_level).
# This row is what decides whether the PvP frame OFFERS the battleground, so a
# client offering a wider range than the server accepts queues into a silent refusal.
BG_ID        = 12
BG_MAX_GROUP = 3               # 3v3
BG_MIN_LEVEL = 61
BG_MAX_LEVEL = 80
# The queue bracket, mirroring pvpdifficulty_dbc row 200 in that same SQL file. Wider
# than BG_MIN/MAX_LEVEL on purpose -- stock Eye of the Storm ships the same mismatch
# (BattlemasterList 61-80, PvpDifficulty 61-85).
PVPDIFF_ID   = 200
PVPDIFF_MIN  = 61
PVPDIFF_MAX  = 85


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
        sys.exit("no locale MPQs found under %s" % DATA)
    return storm, handles


def read_dbc(storm, handles, name):
    path = "DBFilesClient\\" + name
    for a in handles:
        if storm.SFileHasFile(a, path):
            f = storm.SFileOpenFileEx(a, path, 0)
            n = storm.SFileGetFileSize(f)
            d = storm.SFileReadFile(f, n)
            storm.SFileCloseFile(f)
            return d
    sys.exit("%s not found in the locale archives" % path)


class Dbc:
    """A WDBC held as an opaque record block plus its string block.

    Fields are never decoded, only overwritten in place. That is what makes a
    clone safe: a column this tool has no type for keeps the source row's bytes,
    and a string column keeps a still-valid offset because the string block is
    only ever appended to.
    """

    def __init__(self, raw):
        if raw[:4] != b"WDBC":
            sys.exit("not a WDBC file")
        self.n, self.fields, self.rs, _ = struct.unpack_from("<4I", raw, 4)
        cut = 20 + self.n * self.rs
        self.records = bytearray(raw[20:cut])
        self.strings = bytearray(raw[cut:])

    def index_of(self, id_):
        for r in range(self.n):
            if struct.unpack_from("<I", self.records, r * self.rs)[0] == id_:
                return r
        return None

    def uint(self, r, c):
        return struct.unpack_from("<I", self.records, r * self.rs + c * 4)[0]

    def sint(self, r, c):
        return struct.unpack_from("<i", self.records, r * self.rs + c * 4)[0]

    def flt(self, r, c):
        return struct.unpack_from("<f", self.records, r * self.rs + c * 4)[0]

    def text(self, r, c):
        o = self.uint(r, c)
        if o == 0 or o >= len(self.strings):
            return ""
        return self.strings[o:self.strings.index(b"\0", o)].decode("utf-8", "replace")

    def _intern(self, s):
        if s == "":
            return 0                      # offset 0 is the string block's own NUL
        ofs = len(self.strings)
        self.strings += s.encode("utf-8") + b"\0"
        return ofs

    def upsert(self, src_id, new_id, overrides):
        """Clone row src_id as new_id. Replaces new_id in place if it already exists."""
        src = self.index_of(src_id)
        if src is None:
            sys.exit("clone source id %d not present" % src_id)
        rec = bytearray(self.records[src * self.rs:(src + 1) * self.rs])
        struct.pack_into("<i", rec, 0, new_id)
        for c, (kind, val) in sorted(overrides.items()):
            if c >= self.fields:
                sys.exit("field %d is past the record's %d fields" % (c, self.fields))
            if kind == "s":
                struct.pack_into("<I", rec, c * 4, self._intern(val))
            elif kind == "f":
                struct.pack_into("<f", rec, c * 4, float(val))
            else:
                struct.pack_into("<i", rec, c * 4, int(val))
        at = self.index_of(new_id)
        if at is None:
            self.records += rec
            self.n += 1
            return "added"
        self.records[at * self.rs:(at + 1) * self.rs] = rec
        return "replaced"

    def to_bytes(self):
        return (b"WDBC" + struct.pack("<4I", self.n, self.fields, self.rs, len(self.strings))
                + bytes(self.records) + bytes(self.strings))


def map_overrides():
    ov = {1: ("s", MAP_DIR), 22: ("i", AREA_ID), 63: ("i", EXPANSION)}
    for c in range(5, 21):                                       # MapName, all 16 slots
        ov[c] = ("s", MAP_NAME)
    for c in list(range(23, 39)) + list(range(40, 56)):          # both description blocks
        ov[c] = ("s", "")
    return ov


def area_overrides():
    ov = {1: ("i", MAP_ID), 2: ("i", 0), 3: ("i", AREA_BIT), 4: ("i", AREA_FLAGS)}
    for c in range(11, 27):                                      # AreaName, all 16 slots
        ov[c] = ("s", MAP_NAME)
    return ov


def wmoarea_overrides(group_id, flags):
    ov = {1: ("i", ROOT_WMO_ID), 2: ("i", 0), 3: ("i", group_id),
          9: ("i", flags), 10: ("i", AREA_ID)}
    for c in range(11, 27):
        ov[c] = ("s", MAP_NAME)
    return ov


def bml_overrides():
    # 32 fields: 0 ID, 1-8 MapID_1..8, 9 InstanceType, 10 GroupsAllowed, 11-26 Name x16,
    # 27 Name_Lang_Mask, 28 MaxGroupSize, 29 HolidayWorldState, 30 Minlevel, 31 Maxlevel.
    # MapID_2..8 are deliberately untouched: row 7 already carries -1 in all seven, and
    # the core registers a battleground by map only while mapid[1] == -1.
    ov = {1: ("i", MAP_ID), 28: ("i", BG_MAX_GROUP), 29: ("i", 0),
          30: ("i", BG_MIN_LEVEL), 31: ("i", BG_MAX_LEVEL)}
    for c in range(11, 27):                                      # Name, all 16 slots
        ov[c] = ("s", MAP_NAME)
    return ov


def pvpdiff_overrides():
    # 6 fields: 0 ID, 1 MapID, 2 RangeIndex, 3 MinLevel, 4 MaxLevel, 5 Difficulty.
    # One wide bracket at RangeIndex 0, mirroring the server's row so both sides agree
    # on which bracket a player lands in.
    return {1: ("i", MAP_ID), 2: ("i", 0),
            3: ("i", PVPDIFF_MIN), 4: ("i", PVPDIFF_MAX), 5: ("i", 0)}


# file, [(clone_from, new_id, overrides), ...]
PATCHES = [
    # 566 Eye of the Storm: instanceType 3, PVP 1 -- the closest stock analogue.
    ("Map.dbc",          [(566, MAP_ID, map_overrides())]),
    # 3820 Eye of the Storm: keeps its ambience, zone music, MinElevation and mask.
    ("AreaTable.dbc",    [(3820, AREA_ID, area_overrides())]),
    # 14369 is a plain per-group Stormwind row. Two rows, matching Blizzard's shape:
    # WMOGroupID -1 is the client's whole-WMO fallback, 0 is what every one of our
    # groups carries, and 0x4 is the bit Map.cpp reads to force outdoors.
    ("WMOAreaTable.dbc", [(14369, WMOAREA_ID,     wmoarea_overrides(-1, 0x10)),
                          (14369, WMOAREA_ID + 1, wmoarea_overrides(0,  0x04))]),
    # 591 is map 566's light. Client-only: LightEntryfmt skips every parameter.
    ("Light.dbc",        [(591, LIGHT_ID, {1: ("i", MAP_ID)})]),
    # 7 Eye of the Storm: a one-map 61-80 battleground, the same shape as ours, so the
    # clone carries MapID_2..8 = -1, InstanceType 3 and the locale mask unexamined.
    ("BattlemasterList.dbc", [(7, BG_ID, bml_overrides())]),
    # 52 is Eye of the Storm's first bracket (map 566, RangeIndex 0, 61-69). Without a
    # bracket for our map the client's GetBattlegroundInfo returns canEnter = nil: the
    # battleground lists by name and refuses the queue, because the client has nowhere
    # to place the player's level.
    ("PvpDifficulty.dbc", [(52, PVPDIFF_ID, pvpdiff_overrides())]),
]


def cmd_dump(name, id_):
    storm, handles = open_archives()
    dbc = Dbc(read_dbc(storm, handles, name))
    r = dbc.index_of(id_)
    if r is None:
        sys.exit("%s has no row with id %d" % (name, id_))
    print("%s row id=%d (%d fields, recsize %d)" % (name, id_, dbc.fields, dbc.rs))
    for c in range(dbc.fields):
        u, i, f, t = dbc.uint(r, c), dbc.sint(r, c), dbc.flt(r, c), dbc.text(r, c)
        note = "  str=%r" % t if t else ""
        print("  [%2d] u=%-12d i=%-12d f=%-14.4g%s" % (c, u, i, f, note))
    return 0


def cmd_patch():
    storm, handles = open_archives()
    out_dir = os.path.join(PROJECT, "DBFilesClient")
    os.makedirs(out_dir, exist_ok=True)
    for name, rows in PATCHES:
        dbc = Dbc(read_dbc(storm, handles, name))
        before = dbc.n
        for src, new, ov in rows:
            what = dbc.upsert(src, new, ov)
            print("  %-20s %-9s id %-6d (cloned from %d)" % (name, what, new, src))
        out = os.path.join(out_dir, name)
        with open(out, "wb") as f:
            f.write(dbc.to_bytes())
        print("  %-20s %d -> %d records, wrote %s" % (name, before, dbc.n, out))
    return 0


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[:1] == ["patch"]:
        sys.exit(cmd_patch())
    if a[:1] == ["dump"] and len(a) == 3:
        sys.exit(cmd_dump(a[1], int(a[2])))
    sys.exit(__doc__)

#!/usr/bin/env python3.10
"""Browse every texture and model in the client archives, from a browser.

    asset_server.py [--port 8765]      then open http://127.0.0.1:8765/
    asset_server.py --lan              also reachable from the local network

Decodes ON DEMAND. The archives hold ~110k usable textures; decoding them all
up front is about 9 GB, so nothing is converted until something asks to see it.
Both caches are shared with blender_staging_setup.py, so anything the staging
scene already decoded is free here and vice versa.

Needs pywowlib's compiled StormLib and BLP2PNG bindings plus its pure-python M2
parser, all built for Blender 3.4's interpreter -- so run this with python3.10.

Overrides: WOW_DATA, WBS_ROOT, WOW_LOCALE (see mpq_tool.py), TEXCACHE, M2CACHE.
"""
import http.server, json, os, re, socketserver, sys, threading, urllib.parse

HERE     = os.path.dirname(os.path.abspath(__file__))
WBS      = os.environ.get("WBS_ROOT", os.path.expanduser("~/tools/blender-wow-studio"))
VAR      = os.path.abspath(os.path.join(HERE, "../../../var/blender"))
TEXCACHE = os.environ.get("TEXCACHE", os.path.join(VAR, "texcache"))
M2CACHE  = os.environ.get("M2CACHE",  os.path.join(VAR, "m2cache"))
METACACHE = os.path.join(os.path.dirname(TEXCACHE), "assetmeta.json")

sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(WBS, "io_scene_wmo"))
sys.path.insert(0, os.path.join(WBS, "io_scene_wmo/third_party"))
sys.path.insert(0, os.path.join(WBS, "io_scene_wmo/pywowlib/blp/BLP2PNG"))

import mpq_tool

# Triangles past this are dropped rather than shipped: the browser draws a
# silhouette, and a 40k-triangle model reads identically to its first 4k.
TRI_CAP  = 4000
PAGE     = 200

_LOCK = threading.Lock()
_ARC  = []
_META = {}


def archives():
    with _LOCK:
        if not _ARC:
            _ARC.append(mpq_tool.open_archives())
    return _ARC[0]


def cache_name(path):
    """Flat filename for an archive path, matching blender_staging_setup.py.

    Lowercased: a config path is lowercase by validation and an archive listing
    is not, and on a case-sensitive volume that is two cache entries per asset."""
    return re.sub(r"[\\/]", "_", path).lower()


# The trees a map is dressed from. Everything else -- icons, character skins,
# item art, spell effects -- is hidden until the browser asks for it, because
# interface alone is 14k textures against tileset's 1.3k.
MAP_TOPS = ("world", "dungeons", "tileset", "environments")

# Path segments that name no place. Whatever survives is the theme, which is why
# this list is structural words only -- never a zone.
DROP_SEGMENTS = frozenset("""
world dungeons tileset environment environments textures texture tex azeroth kalimdor
northrend outland expansion01 expansion02 expansion03 doodads passivedoodads
activedoodads doodad buildings walls wall rock rocks floor floors detail details
generic misc prop props env instance instances trim stars nodxt cameras
particles spells interface item character creature simple sky skybox lights
""".split())


def drop_segment(seg):
    """Structural, not a place. The *collidabledoodads suffix is a family --
    kl_, ld_, and friends -- and matching it by prefix would leave the zone one
    segment further in unreachable."""
    return (not seg or seg in DROP_SEGMENTS
            or seg.endswith("collidabledoodads") or seg.endswith("doodads"))


def load_tags(path):
    """A strict subset of YAML: `key:` at column 0, `  key: [a, b, c]` indented,
    plus comments and blanks.

    Anything else RAISES. python3.10 has no yaml module -- that is the toolchain
    split, not an oversight -- and a hand-rolled parser that skipped what it did
    not understand would turn a typo into a silently untagged category."""
    out, section = {}, None
    with open(path) as fh:
        buf = ""
        for n, raw in enumerate(fh, 1):
            line = raw.split("#", 1)[0].rstrip()
            if not line.strip():
                continue
            buf = buf + " " + line.strip() if buf else line
            if buf.count("[") != buf.count("]"):      # a list wrapped onto the
                continue                              # next line
            line, buf = buf, ""
            if not line.startswith(" "):
                if not line.endswith(":"):
                    raise ValueError("%s:%d: expected `key:`, got %r" % (path, n, line))
                section = out.setdefault(line[:-1].strip(), {})
                continue
            if section is None or ":" not in line:
                raise ValueError("%s:%d: unparsable %r" % (path, n, line))
            key, val = line.split(":", 1)
            val = val.strip()
            if not (val.startswith("[") and val.endswith("]")):
                raise ValueError("%s:%d: expected a [list], got %r" % (path, n, val))
            section[key.strip()] = [w.strip() for w in val[1:-1].split(",") if w.strip()]
    return out


# ---------------------------------------------------------------- runtime skins
# M2Texture.type != 0 means the CLIENT supplies the image, so the model names no
# file and a preview would render it flat. The DBCs say what the client would
# have picked. Only the types that resolve to a single file are handled: a real
# character is composited from face, underwear and armour layers too, which is a
# texture-atlas job and not a lookup.
TEX_MONSTER = {11: 0, 12: 1, 13: 2}   # type -> CreatureDisplayInfo variation slot
TEX_SKIN, TEX_HAIR = 1, 6

_SKINS = {}


def _dbc(name):
    raw = mpq_tool.read(*archives(), "DBFilesClient\\" + name)
    if not raw or raw[:4] != b"WDBC":
        raise IOError("no readable %s" % name)
    import struct
    n, fields, rs, _ = struct.unpack_from("<4I", raw, 4)
    body, strings = raw[20:20 + n * rs], raw[20 + n * rs:]

    def text(off):
        if off <= 0 or off >= len(strings):
            return ""
        return strings[off:strings.index(b"\x00", off)].decode("latin-1")

    rows = [struct.unpack_from("<%dI" % fields, body, i * rs) for i in range(n)]
    return rows, text


def skins():
    """model path -> {texture type -> archive path}, built once from the DBCs."""
    if _SKINS:
        return _SKINS
    creature, char = {}, {}
    try:
        cmd, cmd_s = _dbc("CreatureModelData.dbc")
        cdi, cdi_s = _dbc("CreatureDisplayInfo.dbc")
        # .mdx is what the DBC stores; the archives hold the converted .m2.
        model = {r[0]: cmd_s(r[2]).lower().replace(".mdx", ".m2").replace(".mdl", ".m2")
                 for r in cmd if r[2]}
        for r in sorted(cdi, key=lambda r: r[0]):     # lowest display id wins,
            path = model.get(r[1])                    # so the pick is stable
            if not path or path in creature:
                continue
            d = path.rsplit("\\", 1)[0]
            got = {}
            for t, slot in TEX_MONSTER.items():
                name = cdi_s(r[6 + slot])
                if name:
                    got[t] = "%s\\%s.blp" % (d, name.lower())
            if got:
                creature[path] = got
    except (IOError, IndexError) as exc:
        print("creature skins unavailable: %s" % exc, file=sys.stderr)
    try:
        cs, cs_s = _dbc("CharSections.dbc")
        # CharSections stores FULL paths, so the model's own directory is the
        # key and ChrRaces never has to be opened at all.
        for r in cs:
            if r[8] or r[9]:                 # variation 0, colour 0 only
                continue
            t = {0: TEX_SKIN, 3: TEX_HAIR}.get(r[3])
            if t is None:
                continue
            tex = cs_s(r[4]).lower()
            if tex:
                char.setdefault(tex.rsplit("\\", 1)[0], {}).setdefault(t, tex)
    except (IOError, IndexError) as exc:
        print("character skins unavailable: %s" % exc, file=sys.stderr)
    _SKINS["creature"], _SKINS["char"] = creature, char
    return _SKINS


def runtime_tex(path, ttype):
    """The file the client would have supplied for a typed texture slot."""
    sk = skins()
    hit = sk["creature"].get(path, {}).get(ttype)
    if hit:
        return hit
    d = path.rsplit("\\", 1)[0]
    return sk["char"].get(d, {}).get(ttype, "")


# ------------------------------------------------------------------- index

class Index:
    """Every browsable archive entry, grouped by directory, role and theme."""

    def __init__(self, roles):
        storm, handles = archives()
        self.roles = roles                      # ordered: first match wins
        self.by_dir, self.role, self.theme = {}, {}, {}
        for e in mpq_tool.listing(storm, handles):
            low = e.lower()
            if low.endswith(".blp") and not low.endswith("_s.blp"):
                kind = "texture"
            elif low.endswith(".m2"):
                kind = "model"
            else:
                continue
            d = low.rsplit("\\", 1)[0] if "\\" in low else ""
            self.by_dir.setdefault((kind, d), set()).add(low)
        # A SET keyed on the lowercased path. listing() dedupes on the archive's
        # own spelling, so one file present in two archives under two spellings
        # survives as two entries -- 68 Grizzly Hills trees that are really 60.
        self.by_dir = {k: sorted(v) for k, v in self.by_dir.items()}
        for (_, d), v in self.by_dir.items():
            theme = self.theme_of(d)
            for p in v:
                self.theme[p] = theme
                self.role[p] = self.role_of(p)

    @staticmethod
    def theme_of(d):
        for seg in d.split("\\"):
            if not drop_segment(seg):
                return seg
        return ""

    def role_of(self, p):
        for role, words in self.roles.items():
            if any(w in p for w in words):
                return role
        return ""

    def pool(self, kind, cat, show_all):
        if cat:
            return self.by_dir.get((kind, cat), [])
        out = [p for (k, d), v in self.by_dir.items()
               if k == kind and (show_all or d.split("\\")[0] in MAP_TOPS)
               for p in v]
        out.sort()
        return out

    def tree(self, kind, show_all, role="", theme=""):
        """Counts reflect the active facets, and a folder that keeps nothing is
        dropped -- a sidebar listing folders the grid cannot show is a list of
        dead ends."""
        out = []
        for (k, d), v in self.by_dir.items():
            if k != kind or not (show_all or d.split("\\")[0] in MAP_TOPS):
                continue
            n = sum(1 for p in v if (not role or self.role[p] == role)
                    and (not theme or self.theme[p] == theme))
            if n:
                out.append((d, n))
        return sorted(out, key=lambda kv: kv[0])

    def facets(self, kind, show_all, role="", theme=""):
        """Each list is narrowed by the OTHER facet, never by itself.

        Narrowing by itself would leave the chosen value as the only option.
        Not narrowing at all is what let `role=creature` still offer `tauren` --
        a playable race, so its models live under character\\ and the pair
        matched nothing. An option that cannot return a row is a dead end."""
        rc, tc = {}, {}
        for p in self.pool(kind, "", show_all):
            r, t = self.role[p], self.theme[p]
            if not theme or t == theme:
                rc[r] = rc.get(r, 0) + 1
            if not role or r == role:
                tc[t] = tc.get(t, 0) + 1
        top = lambda c: sorted(((k, v) for k, v in c.items() if k),
                               key=lambda kv: -kv[1])
        return {"roles": top(rc), "themes": top(tc)}

    def query(self, kind, cat, q, page, role, theme, show_all):
        paths = self.pool(kind, cat, show_all)
        if role:
            paths = [p for p in paths if self.role[p] == role]
        if theme:
            paths = [p for p in paths if self.theme[p] == theme]
        if q:
            terms = q.lower().split()
            paths = [p for p in paths if all(t in p for t in terms)]
        total = len(paths)
        return total, paths[page * PAGE:(page + 1) * PAGE]


# ------------------------------------------------------------------ assets

def texture_png(path):
    """Decode one BLP into the shared texcache; return its file path."""
    png = os.path.join(TEXCACHE, os.path.splitext(cache_name(path))[0] + ".png")
    if os.path.isfile(png):
        return png
    from BLP2PNG import BlpConverter
    storm, handles = archives()
    with _LOCK:
        data = mpq_tool.read(storm, handles, path)
    if not data:
        return None
    os.makedirs(TEXCACHE, exist_ok=True)
    BlpConverter().convert([(data, cache_name(path).encode())], TEXCACHE.encode())
    return png if os.path.isfile(png) else None


def model_geometry(path):
    """Quantised vertices and triangles for a silhouette, plus what the dressing
    pass sorts on.

    The browser picks the projection, so all three axes ship and a front/side/top
    toggle costs no round trip. Coordinates are model space, exactly as MODD
    writes them -- the same convention the box proxies use."""
    from pywowlib.m2_file import M2File
    storm, handles = archives()
    os.makedirs(M2CACHE, exist_ok=True)
    stem = os.path.join(M2CACHE, os.path.splitext(cache_name(path))[0])

    def grab(src, dst):
        if os.path.isfile(dst):
            return
        with _LOCK:
            data = mpq_tool.read(storm, handles, src)
        if not data:
            raise IOError("no archive holds %s" % src)
        with open(dst, "wb") as fh:
            fh.write(data)

    grab(path, stem + ".m2")
    m2 = M2File(2, stem + ".m2")
    # Profile 00 only, read DIRECTLY. read_additional_files() would do this and
    # then walk the animation sequences, indexing its anim_paths argument as a
    # dict keyed (id, variation_index) -- so a list raises TypeError on every
    # animated model, which is 92 of the first 200 entries in the listing. A
    # silhouette needs no anims, and 00 is the highest-detail LOD.
    from pywowlib.file_formats.skin_format import M2SkinProfile
    grab(path[:-3] + "00.skin", stem + "00.skin")
    with open(stem + "00.skin", "rb") as fh:
        skin = M2SkinProfile().read(fh)

    verts = m2.root.vertices
    if not verts:
        # Not an error: ~10% of models are pure particle or effect emitters and
        # legitimately carry no mesh. 500 would be a lie and breaks any client
        # stricter than fetch().
        return {"empty": True, "note": "particle or effect model -- no geometry"}
    xs = [v.pos[0] for v in verts]
    ys = [v.pos[1] for v in verts]
    zs = [v.pos[2] for v in verts]
    lo = (min(xs), min(ys), min(zs))
    hi = (max(xs), max(ys), max(zs))
    span = max(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]) or 1.0

    def q(v, i):
        return int(round((v - lo[i]) / span * 1023))

    flat = [q(v.pos[i], i) for v in verts for i in range(3)]

    # UVs quantise on their own range because they are NOT bounded to [0,1] --
    # a tiling model addresses well past it, and clamping would fold the repeat
    # back onto itself.
    us = [v.tex_coords[0] for v in verts]
    vs = [v.tex_coords[1] for v in verts]
    uv_lo = (min(us), min(vs))
    uv_span = max(max(us) - uv_lo[0], max(vs) - uv_lo[1]) or 1.0
    uvq = [int(round((c - uv_lo[j]) / uv_span * 4095))
           for v in verts for j, c in enumerate(v.tex_coords[:2])]
    nrm = [max(0, min(254, int(round(c * 127)) + 127))
           for v in verts for c in v.normal]

    # submesh -> texture, the same chain blender_staging_setup.py walks.
    lut = list(m2.root.texture_lookup_table)
    texs = list(m2.root.textures)
    tex_of = {tu.skin_section_index: lut[tu.texture_combo_index]
              for tu in skin.texture_units if tu.texture_combo_index < len(lut)}

    tris, parts = [], []
    for i, sm in enumerate(skin.submeshes):
        if len(tris) >= TRI_CAP:
            break
        start = len(tris) * 3
        for k in range(sm.index_start, sm.index_start + sm.index_count, 3):
            if len(tris) >= TRI_CAP:
                break
            tris.append([skin.vertex_indices[skin.triangle_indices[k + o]]
                         for o in range(3)])
        ti = tex_of.get(i, -1)
        tex = ""
        if 0 <= ti < len(texs):
            if texs[ti].type == 0:
                tex = (texs[ti].filename.value or "").rstrip("\x00").lower()
            else:
                tex = runtime_tex(path, texs[ti].type)
        parts.append({"tex": tex, "start": start,
                      "count": len(tris) * 3 - start})

    return {"verts": flat, "tris": [i for t in tris for i in t],
            "uvs": uvq, "uv_lo": [round(c, 5) for c in uv_lo],
            "uv_span": round(uv_span, 5), "norms": nrm, "parts": parts,
            "span": round(span, 4),
            "lo": [round(c, 3) for c in lo], "hi": [round(c, 3) for c in hi],
            "size": [round(hi[i] - lo[i], 3) for i in range(3)],
            "n_verts": len(verts), "n_tris": len(tris),
            "capped": len(tris) >= TRI_CAP,
            "textures": sorted({p["tex"] for p in parts if p["tex"]})}


def model_meta(path):
    """Cheap header facts, cached to disk. bound_tris is load-bearing: a model
    with zero renders in the client but never reaches the vmaps."""
    if path in _META:
        return _META[path]
    import struct
    storm, handles = archives()
    size = struct.calcsize(mpq_tool.M2_FMT)
    with _LOCK:
        d = mpq_tool.read(storm, handles, path, size)
    rec = {"bound_tris": None}
    if d and len(d) >= size:
        v = struct.unpack(mpq_tool.M2_FMT, d[:size])
        if v[0] == b"MD20":
            fl = v[43:57]
            a, b = fl[7:10], fl[10:13]
            rec = {"bound_tris": v[57],
                   "box": [round(c, 3) for c in list(a) + list(b)],
                   "height": round(b[2] - a[2], 3),
                   "width": round(max(b[0] - a[0], b[1] - a[1]), 3)}
            rec["aspect"] = round(rec["width"] / rec["height"], 3) if rec["height"] else None
    _META[path] = rec
    return rec


def save_meta():
    try:
        with open(METACACHE, "w") as fh:
            json.dump(_META, fh)
    except OSError:
        pass


# ------------------------------------------------------------------- server

class Handler(http.server.BaseHTTPRequestHandler):
    index = None

    def log_message(self, *a):
        pass

    def send(self, code, body, ctype, cache=False):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        if cache:
            self.send_header("Cache-Control", "max-age=86400")
        self.end_headers()
        self.wfile.write(body)

    def json(self, obj, code=200):
        self.send(code, json.dumps(obj).encode(), "application/json")

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(u.query)
        one = lambda k, d="": (q.get(k) or [d])[0]
        try:
            if u.path == "/":
                with open(os.path.join(HERE, "asset_browser.html"), "rb") as fh:
                    return self.send(200, fh.read(), "text/html; charset=utf-8")
            if u.path == "/api/tree":
                return self.json(self.index.tree(one("kind", "texture"),
                                                 one("all") == "1",
                                                 one("role"), one("theme")))
            if u.path == "/api/facets":
                return self.json(self.index.facets(one("kind", "texture"),
                                                   one("all") == "1",
                                                   one("role"), one("theme")))
            if u.path == "/api/query":
                total, paths = self.index.query(
                    one("kind", "texture"), one("cat"), one("q"),
                    int(one("page", "0")), one("role"), one("theme"),
                    one("all") == "1")
                return self.json({"total": total, "page_size": PAGE, "paths": paths})
            if u.path == "/api/meta":
                return self.json({p: model_meta(p)
                                  for p in json.loads(one("paths", "[]"))})
            if u.path.startswith("/tex/"):
                path = urllib.parse.unquote(u.path[5:])
                png = texture_png(path)
                if not png:
                    return self.send(404, b"undecodable", "text/plain")
                with open(png, "rb") as fh:
                    return self.send(200, fh.read(), "image/png", cache=True)
            if u.path.startswith("/api/model/"):
                path = urllib.parse.unquote(u.path[11:])
                return self.json(model_geometry(path))
        except Exception as exc:                     # one bad asset is not fatal
            return self.json({"error": "%s: %s" % (type(exc).__name__, exc)}, 500)
        self.send(404, b"not found", "text/plain")


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


def lan_ip():
    """This host's address on the route out. UDP connect() sends nothing -- it
    only asks the kernel which source address that destination would use, which
    beats guessing an interface name."""
    import socket
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.0.2.1", 1))          # TEST-NET-1, never routable
        return s.getsockname()[0]
    except OSError:
        return None
    finally:
        s.close()


def main(argv):
    port, host = 8765, "127.0.0.1"
    if "--port" in argv:
        port = int(argv[argv.index("--port") + 1])
    if "--host" in argv:
        host = argv[argv.index("--host") + 1]
    if "--lan" in argv:
        host = "0.0.0.0"
    if os.path.isfile(METACACHE):
        try:
            with open(METACACHE) as fh:
                _META.update(json.load(fh))
        except (OSError, ValueError):
            pass
    tags = load_tags(os.path.join(HERE, "asset_tags.yaml"))
    print("indexing archives ...", file=sys.stderr)
    Handler.index = Index(tags["roles"])
    n_t = sum(len(v) for (k, _), v in Handler.index.by_dir.items() if k == "texture")
    n_m = sum(len(v) for (k, _), v in Handler.index.by_dir.items() if k == "model")
    print("%d textures, %d models" % (n_t, n_m), file=sys.stderr)
    print("http://127.0.0.1:%d/" % port, file=sys.stderr)
    if host not in ("127.0.0.1", "localhost"):
        ip = lan_ip()
        if ip:
            print("http://%s:%d/   <- from another machine" % (ip, port),
                  file=sys.stderr)
    print("(ctrl-c to stop)", file=sys.stderr)
    try:
        Server((host, port), Handler).serve_forever()
    except KeyboardInterrupt:
        save_meta()
        print("\nmeta cache saved", file=sys.stderr)


if __name__ == "__main__":
    main(sys.argv[1:])

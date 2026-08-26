#!/usr/bin/env python3.10
"""Contact sheets of client textures, tiled at wall scale, as browsable HTML.

    sheet.py OUT.html --find SUBSTRING...   every archive entry matching all
    sheet.py OUT.html --list FILE           one texture path per line
    sheet.py OUT.html --materials JSON      a bundle's *_materials.json

Needs pywowlib's compiled BLP2PNG and StormLib bindings, both built for Blender
3.4's interpreter -- so run this with python3.10, not the system python.

Overrides: WOW_DATA, WBS_ROOT, WOW_LOCALE (see mpq_tool.py), TEXCACHE.
"""
import html, json, os, re, sys

HERE     = os.path.dirname(os.path.abspath(__file__))
WMO      = os.path.abspath(os.path.join(HERE, "..", "wmo"))   # mpq_tool lives there
WBS      = os.environ.get("WBS_ROOT", os.path.expanduser("~/tools/blender-wow-studio"))
BLP2PNG  = os.path.join(WBS, "io_scene_wmo/pywowlib/blp/BLP2PNG")
TEXCACHE = os.environ.get("TEXCACHE", os.path.abspath(
    os.path.join(HERE, "../../../var/blender/texcache")))

PATCH_YD   = (32.0, 16.0)   # wall patch one cell stands for; 16 yd is the wall height
PX_PER_YD  = 10.0
PER_PAGE   = 120
UV_DEFAULT = 4.0


def cache_name(path):
    """Same flattening blender_staging_setup.py uses, so the two share a cache.

    BlpConvert creates no directories on this code path, so DUNGEONS\\TEXTURES\\
    has to collapse into the filename. LOWERCASED because the two callers
    disagree on case -- a config path is lowercase by validation, an archive
    listing is not -- and on a case-sensitive volume that is two cache entries
    per texture rather than one shared."""
    return re.sub(r"[\\/]", "_", path).lower()


def png_for(path):
    return os.path.join(TEXCACHE, os.path.splitext(cache_name(path))[0] + ".png")


def decode(paths):
    """Decode every path not already cached. Returns the ones that have a PNG."""
    sys.path.insert(0, WMO)
    sys.path.insert(0, BLP2PNG)
    import mpq_tool
    from BLP2PNG import BlpConverter

    todo = [p for p in paths if not os.path.isfile(png_for(p))]
    if todo:
        os.makedirs(TEXCACHE, exist_ok=True)
        storm, handles = mpq_tool.open_archives()
        batch = []
        for p in todo:
            data = mpq_tool.read(storm, handles, p)
            if data:
                batch.append((data, cache_name(p).encode()))
        # One convert call for the lot: BlpConvert reopens nothing per file and
        # a per-file call costs a round trip through Cython for no gain.
        if batch:
            BlpConverter().convert(batch, TEXCACHE.encode())
    ok = [p for p in paths if os.path.isfile(png_for(p))]
    for p in paths:
        if p not in ok:
            print("  undecodable: %s" % p, file=sys.stderr)
    return ok


def collect(argv):
    """(label, texture path) pairs from whichever source was named."""
    mode = argv[0]
    if mode == "--materials":
        with open(argv[1]) as fh:
            mats = json.load(fh)
        return sorted((k, v) for k, v in mats.items())
    if mode == "--list":
        with open(argv[1]) as fh:
            paths = [l.strip() for l in fh if l.strip()]
    elif mode == "--find":
        sys.path.insert(0, WMO)
        import mpq_tool
        storm, handles = mpq_tool.open_archives()
        pats = [a.lower() for a in argv[1:]]
        if not pats:
            sys.exit("--find needs at least one substring")
        paths = [e for e in mpq_tool.listing(storm, handles)
                 if all(p in e.lower() for p in pats)]
    else:
        sys.exit(__doc__)
    # gen_blockout rejects all three of these, so a sheet must never offer a
    # path that cannot be pasted into map_source.yaml as it is printed.
    paths = [p.lower() for p in paths if p.lower().endswith(".blp")
             and not p.lower().endswith("_s.blp")]
    return [(os.path.splitext(p.rsplit("\\", 1)[-1])[0], p)
            for p in sorted(set(paths))]


CSS = """
:root { color-scheme: dark; --bg:#1b1b1e; --fg:#e6e6e6; --dim:#8a8a92; }
body { margin:0; padding:16px; background:var(--bg); color:var(--fg);
       font:13px/1.4 ui-monospace,SFMono-Regular,Menlo,monospace; }
h1 { font-size:15px; font-weight:600; margin:0 0 4px; }
.sub { color:var(--dim); margin-bottom:14px; }
.bar { position:sticky; top:0; z-index:2; background:var(--bg); padding:10px 0;
       border-bottom:1px solid #333; margin-bottom:14px;
       display:flex; gap:18px; align-items:center; flex-wrap:wrap; }
.bar label { color:var(--dim); }
.grid { display:grid; gap:14px;
        grid-template-columns:repeat(auto-fill,minmax(%(cw)dpx,1fr)); }
.cell { border:1px solid #333; background:#666; }
.swatch { height:%(ch)dpx; background-repeat:repeat; background-position:0 0;
          image-rendering:auto; }
.name { padding:5px 7px; background:#232327; color:var(--dim);
        font-size:11px; word-break:break-all; }
.name b { color:var(--fg); font-weight:600; }
nav { margin:18px 0 0; color:var(--dim); }
nav a { color:#7aa7ff; margin-right:10px; }
"""


def page(cells, out, title, sub, nav):
    cw, ch = PATCH_YD[0] * PX_PER_YD, PATCH_YD[1] * PX_PER_YD
    parts = ["<!doctype html><meta charset=utf-8>",
             "<title>%s</title>" % html.escape(title),
             "<style>%s</style>" % (CSS % {"cw": int(cw), "ch": int(ch)}),
             "<h1>%s</h1><div class=sub>%s</div>" % (html.escape(title),
                                                     html.escape(sub)),
             "<div class=bar>",
             "<label>uv_scale_yd <input id=uv type=range min=0.5 max=16 step=0.5 "
             "value=%s></label><b id=uvv>%s</b>" % (UV_DEFAULT, UV_DEFAULT),
             "<span class=sub>each cell = %g x %g yd of wall</span>"
             % PATCH_YD, "</div>", "<div class=grid>"]
    for label, path, href in cells:
        parts.append(
            "<div class=cell><div class=swatch data-src=\"%s\"></div>"
            "<div class=name><b>%s</b><br>%s</div></div>"
            % (html.escape(href), html.escape(label), html.escape(path)))
    parts.append("</div>")
    if nav:
        parts.append("<nav>%s</nav>" % nav)
    # background-size is the whole point: uv_scale_yd yards of wall per texture
    # repeat, so the slider changes the repeat rate and nothing else.
    parts.append("""<script>
const cw=%f, patch=%f, sw=document.querySelectorAll('.swatch');
sw.forEach(e=>e.style.backgroundImage='url("'+e.dataset.src+'")');
function draw(uv){document.getElementById('uvv').textContent=uv;
  const px=cw*(uv/patch);
  sw.forEach(e=>e.style.backgroundSize=px+'px '+px+'px');}
const s=document.getElementById('uv');
s.addEventListener('input',()=>draw(parseFloat(s.value)));
draw(parseFloat(s.value));
</script>""" % (cw, PATCH_YD[0]))
    with open(out, "w") as fh:
        fh.write("\n".join(parts))


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    out, rest = argv[0], argv[1:]
    pairs = collect(rest)
    if not pairs:
        sys.exit("no candidates matched")
    print("%d candidates" % len(pairs), file=sys.stderr)
    ok = set(decode([p for _, p in pairs]))
    pairs = [(l, p) for l, p in pairs if p in ok]

    stem, ext = os.path.splitext(out)
    pages = [pairs[i:i + PER_PAGE] for i in range(0, len(pairs), PER_PAGE)] or [[]]
    names = [out if i == 0 else "%s_%d%s" % (stem, i + 1, ext)
             for i in range(len(pages))]
    for i, (chunk, name) in enumerate(zip(pages, names)):
        nav = " ".join(
            ("<b>%d</b>" % (j + 1)) if j == i else
            "<a href=\"%s\">%d</a>" % (html.escape(os.path.basename(n)), j + 1)
            for j, n in enumerate(names)) if len(names) > 1 else ""
        cells = [(l, p, os.path.relpath(png_for(p), os.path.dirname(
            os.path.abspath(name)))) for l, p in chunk]
        page(cells, name, "contact sheet %d/%d" % (i + 1, len(pages)),
             "%d textures  |  %s" % (len(chunk), " ".join(rest)), nav)
        print("%s  (%d)" % (name, len(chunk)), file=sys.stderr)


if __name__ == "__main__":
    main(sys.argv[1:])

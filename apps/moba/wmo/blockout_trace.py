"""Turn a painted map trace into blockout geometry.

    blender --background --python apps/moba/wmo/blockout_trace.py -- --params P

Driven by apps/moba/gen_blockout.py, which resolves the map bundle's config and
invokes this. Blender's python has numpy but no yaml, so parameters arrive as
JSON; that split is the only reason this is two files.

Classifies the painted PNG into floor / base platform / ramp, derives the
registration from the paint itself, traces and low-pass filters the floor
boundary, and writes geometry.json, heights.png and boundary_preview.png.
"""
import json
import math
import os
import sys

import bpy
import numpy as np

AMBIGUOUS_MARGIN = 0.15         # RGB distance; a pixel this close to two keys is a blend
UNIFORM_STEP = 1.0              # yd; arc-length parameterisation for the low-pass


# ------------------------------------------------------------------ raster ops

def load_paint(path, alpha_threshold):
    img = bpy.data.images.load(path, check_existing=False)
    img.colorspace_settings.name = "Non-Color"      # raw stored values, not sRGB->linear
    img.alpha_mode = "CHANNEL_PACKED"               # stop Blender premultiplying RGB
    w, h = img.size
    buf = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(buf)
    px = buf.reshape(h, w, 4).copy()                # row 0 is the BOTTOM row
    bpy.data.images.remove(img)
    return px[:, :, :3], px[:, :, 3] > alpha_threshold


def _stack(a, pad):
    gh, gw = a.shape
    p = np.pad(a, 1, constant_values=pad)
    return np.stack([p[i:i + gh, j:j + gw] for i in range(3) for j in range(3)])


def _dilate(a, n=1):
    for _ in range(n):
        a = _stack(a, False).any(axis=0)
    return a


def _components(mask):
    gh, gw = mask.shape
    lab = np.where(mask, np.arange(gh * gw).reshape(gh, gw), -1)
    while True:
        s = _stack(np.where(mask, lab, gh * gw), gh * gw)
        nxt = np.where(mask, s.min(axis=0), -1)
        if np.array_equal(nxt, lab):
            return lab
        lab = nxt


def blobs(mask, min_px):
    """Components at or above min_px, plus how many were dropped."""
    lab = _components(mask)
    ids, cnt = np.unique(lab[mask], return_counts=True)
    keep = [int(i) for i, c in zip(ids.tolist(), cnt.tolist()) if c >= min_px]
    return [(i, (lab == i)) for i in keep], int((cnt < min_px).sum())


def classify(rgb, opaque, palette):
    names = list(palette)
    d = np.stack([np.linalg.norm(rgb - np.array(palette[n]), axis=2) for n in names])
    srt = np.sort(d, axis=0)
    ambiguous = int((opaque & ((srt[1] - srt[0]) < AMBIGUOUS_MARGIN)).sum())
    idx = d.argmin(axis=0)
    return {n: opaque & (idx == i) for i, n in enumerate(names)}, ambiguous


# --------------------------------------------------------------- registration

def mirror_cx(mask):
    """Best mirror axis. The map is symmetric by design, so this is the x origin
    and its IoU gates the whole registration."""
    h, w = mask.shape
    cols = np.nonzero(mask)[1]
    c0 = int(cols.min()) + int(cols.max())
    best = (c0 / 2.0, 0.0)
    for cx2 in range(c0 - w // 16, c0 + w // 16 + 1):
        b = np.roll(mask[:, ::-1], cx2 - (w - 1), axis=1)
        iou = float((mask & b).sum()) / float((mask | b).sum())
        if iou > best[1]:
            best = (cx2 / 2.0, iou)
    return best


# --------------------------------------------------------------------- tracing

def trace_loops(mask):
    """Closed lattice loops around the mask, interior kept on the left."""
    gh, gw = mask.shape
    seg = {}
    rows, cols = np.nonzero(mask)
    for r, c in zip(rows.tolist(), cols.tolist()):
        if r == 0 or not mask[r - 1, c]:        seg.setdefault((c, r), []).append((c + 1, r))
        if c + 1 >= gw or not mask[r, c + 1]:   seg.setdefault((c + 1, r), []).append((c + 1, r + 1))
        if r + 1 >= gh or not mask[r + 1, c]:   seg.setdefault((c + 1, r + 1), []).append((c, r + 1))
        if c == 0 or not mask[r, c - 1]:        seg.setdefault((c, r + 1), []).append((c, r))

    def step(p, prev):
        outs = seg.get(p)
        if not outs:
            return None
        if len(outs) > 1 and prev is not None:
            d = (p[0] - prev[0], p[1] - prev[1])
            # Sharpest right turn first, so regions meeting at a corner stay separate.
            rank = {(d[1], -d[0]): 0, d: 1, (-d[1], d[0]): 2, (-d[0], -d[1]): 3}
            outs.sort(key=lambda q: rank.get((q[0] - p[0], q[1] - p[1]), 4))
        nxt = outs.pop(0)
        if not outs:
            del seg[p]
        return nxt

    loops = []
    while seg:
        start = next(iter(seg))
        pts, cur, prev = [start], start, None
        while True:
            nxt = step(cur, prev)
            if nxt is None or nxt == start:
                break
            pts.append(nxt)
            prev, cur = cur, nxt
        if len(pts) > 3:
            loops.append(pts)
    return loops


def signed_area(p):
    n = len(p)
    return 0.5 * sum(p[i][0] * p[(i + 1) % n][1] - p[(i + 1) % n][0] * p[i][1]
                     for i in range(n))


def resample(pts, step):
    out, carry = [pts[0]], 0.0
    for i in range(1, len(pts) + 1):
        ax, ay = pts[i - 1]
        bx, by = pts[i % len(pts)]
        length = math.hypot(bx - ax, by - ay)
        if length < 1e-9:
            continue
        t = 0.0
        while carry + (length - t) >= step:
            t += step - carry
            out.append((ax + (bx - ax) * t / length, ay + (by - ay) * t / length))
            carry = 0.0
        carry += length - t
    return out


def smooth_closed(pts, sigma, step):
    """Circular Gaussian low-pass. Douglas-Peucker is the wrong tool here: it keeps
    maximum-deviation points, and on a hand trace those are the tremor."""
    p = np.asarray(pts, dtype=np.float64)
    n = len(p)
    r = max(1, int(math.ceil(3.0 * sigma / step)))
    if n <= 2 * r + 1:
        return [tuple(v) for v in p]
    k = np.exp(-0.5 * (np.arange(-r, r + 1) * step / sigma) ** 2)
    k /= k.sum()
    idx = (np.arange(n)[:, None] + np.arange(-r, r + 1)[None, :]) % n
    return [tuple(v) for v in (p[idx] * k[None, :, None]).sum(axis=1)]


def max_deviation(after, before):
    b = np.asarray(before)
    return max(float(np.min(np.hypot(b[:, 0] - x, b[:, 1] - y))) for x, y in after)


# ------------------------------------------------------------------- elevation

def _spread(seed, region):
    """Dilation step at which seed reaches each pixel of region."""
    d = np.zeros(region.shape, np.float64)
    cur, todo, step = seed.copy(), region.copy(), 0
    while todo.any() and step < 1000:
        step += 1
        nxt = _dilate(cur) & todo
        if not nxt.any():
            break
        d[nxt] = step
        todo &= ~nxt
        cur |= nxt
    return d


def height_field(cls, base_yd, blur_px):
    """0 on floor, base_yd on the platform, linear across the ramp."""
    floor, base, ramp = cls["floor"], cls["base"], cls["ramp"]
    dfl, dba = _spread(floor, ramp), _spread(base, ramp)
    tot = dfl + dba
    t = np.where(tot > 0, dfl / np.maximum(tot, 1e-9), 0.0)
    z = np.where(base, float(base_yd), 0.0)
    z = np.where(ramp, t * base_yd, z)

    solid = floor | base | ramp
    if blur_px >= 1:
        wt = solid.astype(np.float64)
        zz = z * wt
        for _ in range(int(blur_px)):
            zz = _stack(zz, 0.0).mean(axis=0)
            wt = _stack(wt, 0.0).mean(axis=0)
        z = np.where(solid, zz / np.maximum(wt, 1e-9), 0.0)
    return z, solid


def max_slope_deg(z, solid, scale):
    """Excludes pixels touching the boundary -- the platform's outer edge is meant
    to be a cliff, and its gradient would drown out every real slope."""
    interior = solid & ~_dilate(~solid)
    gy, gx = np.gradient(z)
    slope = np.degrees(np.arctan(np.hypot(gx, gy) / scale))
    return float(slope[interior].max()) if interior.any() else 0.0


# --------------------------------------------------------------------- output

def save_gray(values, path, name):
    h, w = values.shape
    px = np.zeros((h, w, 4), np.float32)
    px[:, :, 0] = px[:, :, 1] = px[:, :, 2] = np.clip(values, 0.0, 1.0)
    px[:, :, 3] = 1.0
    img = bpy.data.images.new(name, w, h, alpha=False)
    img.pixels.foreach_set(px.reshape(-1))
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def save_preview(cls, loops, path, cx, cy, scale):
    h, w = cls["floor"].shape
    px = np.zeros((h, w, 4), np.float32)
    px[:, :, 3] = 1.0
    px[cls["floor"]] = [0.13, 0.15, 0.17, 1.0]
    px[cls["base"]] = [0.11, 0.13, 0.30, 1.0]
    px[cls["ramp"]] = [0.34, 0.21, 0.06, 1.0]
    for pts, kind in loops:
        col = (0.13, 1.0, 0.27) if kind == "outer" else (1.0, 0.2, 0.85)
        for i in range(len(pts)):
            ax, ay = pts[i][0] / scale + cx, pts[i][1] / scale + cy
            bx = pts[(i + 1) % len(pts)][0] / scale + cx
            by = pts[(i + 1) % len(pts)][1] / scale + cy
            n = max(int(max(abs(bx - ax), abs(by - ay))), 1)
            for s in range(n + 1):
                t = s / n
                xi, yi = int(round(ax + (bx - ax) * t)), int(round(ay + (by - ay) * t))
                px[max(yi - 1, 0):yi + 2, max(xi - 1, 0):xi + 2, :3] = col
    img = bpy.data.images.new("blockout_preview", w, h, alpha=False)
    img.pixels.foreach_set(px.reshape(-1))
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def region_summary(mask, min_px, cx, cy, scale):
    out = []
    for _, m in blobs(mask, min_px)[0]:
        rr, cc = np.nonzero(m)
        out.append({
            "centre_yd": [round((cc.mean() - cx) * scale, 2),
                          round((rr.mean() - cy) * scale, 2)],
            "area_yd2": round(int(m.sum()) * scale * scale, 1),
            "bbox_yd": {"x": [round((int(cc.min()) - cx) * scale, 1),
                              round((int(cc.max()) - cx) * scale, 1)],
                        "y": [round((int(rr.min()) - cy) * scale, 1),
                              round((int(rr.max()) - cy) * scale, 1)]},
        })
    return sorted(out, key=lambda r: (r["centre_yd"][0], r["centre_yd"][1]))


def ramp_summary(cls, min_px, cx, cy, scale):
    near_floor = _dilate(cls["floor"])
    near_base = _dilate(cls["base"])
    out = []
    for _, m in blobs(cls["ramp"], min_px)[0]:
        rr, cc = np.nonzero(m)
        gr, gc = np.nonzero(m & near_floor)
        br, bc = np.nonzero(m & near_base)
        e = {"centre_yd": [round((cc.mean() - cx) * scale, 1),
                           round((rr.mean() - cy) * scale, 1)],
             "area_yd2": round(int(m.sum()) * scale * scale, 1),
             "touches_floor": bool(len(gr)), "touches_base": bool(len(br))}
        if len(gr) and len(br):
            run = float(np.hypot(gc.mean() - bc.mean(), gr.mean() - br.mean())) * scale
            e["run_yd"] = round(run, 1)
            e["mouth_yd"] = round(int(m.sum()) * scale * scale / max(run, 1e-6), 1)
        out.append(e)
    return sorted(out, key=lambda r: (r["centre_yd"][0], r["centre_yd"][1]))


# ------------------------------------------------------------------------ main

def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    with open(argv[argv.index("--params") + 1]) as fh:
        cfg = json.load(fh)

    out_dir = cfg["out_dir"]
    rgb, opaque = load_paint(cfg["trace"], cfg["alpha_threshold"])
    h, w = opaque.shape
    cls, ambiguous = classify(rgb, opaque, cfg["palette"])

    # Provisional scale, only to size the speck filter in pixels. The real one is
    # recomputed below, once dropping specks has settled the extent.
    cols = np.nonzero(cls["floor"] | cls["base"] | cls["ramp"])[1]
    scale = cfg["map_width_yd"] / float(int(cols.max()) - int(cols.min()) + 1)
    min_blob_px = max(1, int(round(cfg["min_blob_yd2"] / (scale * scale))))

    dropped = 0
    for name in cls:
        keep, n = blobs(cls[name], min_blob_px)
        dropped += n
        cls[name] = (np.logical_or.reduce([m for _, m in keep]) if keep
                     else np.zeros_like(cls[name]))

    solid = cls["floor"] | cls["base"] | cls["ramp"]
    cx, iou = mirror_cx(solid)
    rows, cols = np.nonzero(solid)
    x0, x1, y0, y1 = int(cols.min()), int(cols.max()), int(rows.min()), int(rows.max())
    scale = cfg["map_width_yd"] / float(x1 - x0 + 1)
    cy = (y0 + y1) / 2.0
    parts = [int(m.sum()) for _, m in blobs(solid, min_blob_px)[0]]

    loops = []
    for lp in trace_loops(solid):
        world = [((x - cx) * scale, (y - cy) * scale) for x, y in lp]
        if abs(signed_area(world)) < cfg["min_loop_yd2"]:
            continue
        dense = resample(world, UNIFORM_STEP)
        final = resample(smooth_closed(dense, cfg["sigma_yd"], UNIFORM_STEP),
                         cfg["resample_yd"])
        loops.append({
            "kind": "outer" if signed_area(world) > 0 else "hole",
            "points": final,
            "area_before": abs(signed_area(world)),
            "area_after": abs(signed_area(final)),
            "deviation": max_deviation(final, dense),
        })
    loops.sort(key=lambda d: -d["area_after"])

    z, solid_z = height_field(cls, cfg["base_yd"], cfg["ramp_blur_yd"] / scale)
    slope = max_slope_deg(z, solid_z, scale)
    save_gray(z / max(cfg["base_yd"], 1e-9),
              os.path.join(out_dir, "heights.png"), "blockout_heights")
    save_preview(cls, [(l["points"], l["kind"]) for l in loops],
                 os.path.join(out_dir, "boundary_preview.png"), cx, cy, scale)

    geom = {
        "meta": {
            "source": os.path.relpath(cfg["trace"], cfg["repo"]),
            "generated_by": "apps/moba/wmo/blockout_trace.py",
            "units": "1 unit = 1 WoW yard, origin at map centre, +x = east, +y = north",
        },
        "registration": {"scale_yd_per_px": round(scale, 6), "cx_px": cx, "cy_px": cy,
                         "image_size_px": [w, h]},
        "extent_yd": {"x": [round((x0 - cx) * scale, 2), round((x1 - cx) * scale, 2)],
                      "y": [round((y0 - cy) * scale, 2), round((y1 - cy) * scale, 2)]},
        "elevation": {"floor_yd": 0.0, "base_yd": cfg["base_yd"],
                      "height_map": "heights.png", "height_scale_yd": cfg["base_yd"]},
        "bases": region_summary(cls["base"], min_blob_px, cx, cy, scale),
        "ramps": ramp_summary(cls, min_blob_px, cx, cy, scale),
        "loops": [{"id": "loop_%02d" % i, "kind": l["kind"],
                   "area_yd2": round(l["area_after"], 1),
                   "points": [[round(x, 3), round(y, 3)] for x, y in l["points"]]}
                  for i, l in enumerate(loops)],
        "verification": {
            "mirror_iou": round(iou, 4),
            "ambiguous_px": ambiguous,
            "blobs_dropped": dropped,
            "floor_regions": len(parts),
            "max_slope_deg": round(slope, 1),
            "max_smoothing_deviation_yd": round(max(l["deviation"] for l in loops), 2),
            "area_change_pct": round(100.0 * (sum(l["area_after"] for l in loops)
                                              / sum(l["area_before"] for l in loops) - 1), 3),
            "total_points": sum(len(l["points"]) for l in loops),
        },
    }
    with open(os.path.join(out_dir, "geometry.json"), "w") as fh:
        json.dump(geom, fh, indent=2)
    print("BLOCKOUT " + json.dumps(geom["verification"]))


if __name__ == "__main__":
    main()

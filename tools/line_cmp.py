# -*- coding: utf-8 -*-
"""line_cmp.py -- per-LINE screenshot comparison between the original e.exe and
EClone.

For every rendered code line it writes one image:
    shots/lines/LNNN.png =  orig line  |  mine line  |  diff (magenta)
and prints a table of differing pixels per line (worst first), so a fix can be
judged line by line instead of "the page looks off".

The two editors are not always scrolled to the same line, so the vertical
offset is recovered automatically (best-matching shift over the text columns,
with sub-line precision reported).

usage: python tools/line_cmp.py <orig.png> <mine.png> [--dy N] [--lines 0-200]
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "shots", "lines")


def load(p):
    from PIL import Image
    return Image.open(p).convert("RGB")


def ink_profile(img, x0, x1):
    """Dark-pixel count per row = the line "fingerprint" (white background)."""
    px = img.load()
    W, H = img.size
    x1 = min(x1, W)
    prof = []
    for y in range(H):
        c = 0
        for x in range(x0, x1):
            r, g, b = px[x, y]
            if r < 150 and g < 150 and b < 150:
                c += 1
        prof.append(c)
    return prof


def best_dy_by_profile(pa, pb, lo=-600, hi=600):
    """Align b onto a by maximising the overlap of their ink profiles.

    Raw pixel-sum alignment locks onto the white background; the ink profile is
    zero almost everywhere and peaks exactly on rendered lines, so the peak
    pattern dominates the score.
    """
    Ha, Hb = len(pa), len(pb)
    best, bestscore = 0, None
    for dy in range(lo, hi + 1):
        y0 = max(0, -dy)
        y1 = min(Ha, Hb - dy)
        if y1 - y0 < 200:
            continue
        score = 0
        for y in range(y0, y1):
            a = pa[y]
            b = pb[y + dy]
            score += a if a < b else b
        if bestscore is None or score > bestscore:
            best, bestscore = dy, score
    return best, bestscore


def bands(img, x0, x1, thresh=4):
    """Row bands that contain ink -> the rendered lines."""
    px = img.load()
    W, H = img.size
    x1 = min(x1, W)
    out, inb, start = [], False, 0
    for y in range(H):
        dark = 0
        for x in range(x0, x1):
            r, g, b = px[x, y]
            if r < 150 and g < 150 and b < 150:
                dark += 1
        if dark > thresh and not inb:
            inb, start = True, y
        elif dark <= thresh and inb:
            inb = False
            out.append((start, y))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("orig")
    ap.add_argument("mine")
    ap.add_argument("--dy", type=int, default=None)
    ap.add_argument("--x-gutter", type=int, default=66,
                    help="x where the code text starts (left of it = flow lines)")
    ap.add_argument("--max-lines", type=int, default=0)
    ap.add_argument("--x-max", type=int, default=780,
                    help="crop the strips to this x (text area) for readability")
    ap.add_argument("--zoom", type=int, default=2)
    args = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)

    from PIL import Image
    a, b = load(args.orig), load(args.mine)
    W = min(a.size[0], b.size[0])
    if args.dy is not None:
        dy = args.dy
    else:
        pa_ = ink_profile(a, args.x_gutter, min(W, 1100))
        pb_ = ink_profile(b, args.x_gutter, min(W, 1100))
        dy, score = best_dy_by_profile(pa_, pb_)
        print("profile alignment score=%s" % score)
    print("image %dx%d vs %dx%d  common W=%d  best dy=%d (mine shifted by %d)"
          % (a.size[0], a.size[1], b.size[0], b.size[1], W, dy, dy))

    pa, pb = a.load(), b.load()
    H = min(a.size[1], b.size[1] - max(0, dy))
    bs = bands(a, args.x_gutter, min(W, 1100))
    print("orig ink bands: %d   first tops: %s" % (len(bs), [s for s, e in bs[:8]]))
    pitches = [bs[i + 1][0] - bs[i][0] for i in range(len(bs) - 1)]
    if pitches:
        pitch = max(set(pitches), key=pitches.count)
        print("line pitch (mode) = %d px ; all pitches seen = %s"
              % (pitch, sorted(set(pitches))[:8]))

    rows = []
    for i, (y0, y1) in enumerate(bs):
        pad = 3
        if y0 - pad >= H:
            break
        yy0, yy1 = max(0, y0 - pad), min(H, y1 + pad)
        if yy1 <= yy0:
            continue
        ya = yy0
        yb = yy0 + dy
        if yb < 0 or yb + (yy1 - yy0) > b.size[1]:
            continue
        ca = a.crop((0, ya, W, yy1))
        cb = b.crop((0, yb, W, yb + (yy1 - yy0)))
        d = Image.new("RGB", ca.size)
        dpx = d.load()
        cpa, cpb = ca.load(), cb.load()
        n = 0
        for y in range(ca.size[1]):
            for x in range(W):
                da = cpa[x, y]
                db = cpb[x, y]
                if (abs(da[0] - db[0]) + abs(da[1] - db[1]) + abs(da[2] - db[2])) > 30:
                    dpx[x, y] = (255, 0, 255)
                    n += 1
        rows.append((n, i, y0, yy0, yy1, ca, cb, d))

    tot_all = sum(r[0] for r in rows)
    print("lines compared: %d   total differing px: %d" % (len(rows), tot_all))
    for n, i, y0, yy0, yy1, ca, cb, d in sorted(rows, key=lambda r: -r[0])[:20]:
        print("  L%-3d y=%-4d  diff=%-6d" % (i, y0, n))

    # write per-line strips (orig | mine | diff) for the worst 40 lines
    worst = sorted(rows, key=lambda r: -r[0])[:40]
    for n, i, y0, yy0, yy1, ca, cb, d in worst:
        xm = min(args.x_max, ca.size[0])
        ca, cb, d = ca.crop((0, 0, xm, ca.size[1])), cb.crop((0, 0, xm, cb.size[1])), d.crop((0, 0, xm, d.size[1]))
        if args.zoom > 1:
            z = args.zoom
            ca = ca.resize((ca.size[0] * z, ca.size[1] * z), 0)
            cb = cb.resize((cb.size[0] * z, cb.size[1] * z), 0)
            d = d.resize((d.size[0] * z, d.size[1] * z), 0)
        h = ca.size[1]
        w2 = ca.size[0]
        strip = Image.new("RGB", (w2 * 3 + 16, h), (0, 128, 0))
        strip.paste(ca, (0, 0))
        strip.paste(cb, (w2 + 8, 0))
        strip.paste(d, (w2 * 2 + 16, 0))
        strip.save(os.path.join(OUT, "L%03d_diff%05d.png" % (i, n)))
    print("wrote %d per-line strips -> %s" % (len(worst), OUT))


if __name__ == "__main__":
    main()

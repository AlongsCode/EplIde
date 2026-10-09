# -*- coding: utf-8 -*-
"""逐像素对照两张同内容的 1:1 截图，把差异按连通块报出来。

用法: python diff_png.py <A.png> <B.png> <x0> <y0> <x1> <y1> [out.png] [标签A] [标签B] [--ashift=N] [--bshift=N]
     （坐标是"共同区域"，一般给 (0,0,min(w),min(h))；只统计 A/B 都存在、且颜色不同的像素）
     --ashift=N：取 A 的第 y+N 行来对齐 B 的第 y 行（本工程的参照截图 crop 比真正的
                 客户区原点低 4px，所以跟 ref_dll_page.png / user_ref.png 比要加 --ashift=4）

输出：
  · 每类差异的颜色对 + 像素数
  · 差异连通块的包围盒（按面积倒序，最多 25 个）
  · 对照图 out.png：上=A 下=B 中间=差异标红（原色淡出）
"""
import sys
from collections import Counter, deque
from PIL import Image

MAXBLOCKS = 200


def clusters(diff, x0, y0, x1, y1):
    """差异像素的 8 邻域连通块"""
    seen = set()
    out = []
    for p in diff:
        if p in seen:
            continue
        q = deque([p])
        seen.add(p)
        minx = maxx = p[0]
        miny = maxy = p[1]
        n = 0
        while q:
            x, y = q.popleft()
            n += 1
            minx = min(minx, x); maxx = max(maxx, x)
            miny = min(miny, y); maxy = max(maxy, y)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    np_ = (x + dx, y + dy)
                    if np_ in diff and np_ not in seen:
                        seen.add(np_)
                        q.append(np_)
        out.append((n, minx, miny, maxx, maxy))
    out.sort(reverse=True)
    return out


def main():
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    maxdiff = 0
    ashift = bshift = 0
    for a in sys.argv[1:]:
        if a.startswith("--max="):
            maxdiff = int(a.split("=", 1)[1])
        elif a.startswith("--ashift="):
            ashift = int(a.split("=", 1)[1])
        elif a.startswith("--bshift="):
            bshift = int(a.split("=", 1)[1])
    a_path, b_path = argv[0], argv[1]
    x0, y0, x1, y1 = (int(v) for v in argv[2:6])
    out = argv[6] if len(argv) > 6 else None
    la = argv[7] if len(argv) > 7 else "A"
    lb = argv[8] if len(argv) > 8 else "B"

    A = Image.open(a_path).convert("RGB")
    B = Image.open(b_path).convert("RGB")
    pa, pb = A.load(), B.load()
    W = min(A.size[0], B.size[0], x1)
    H = min(A.size[1] - ashift, B.size[1] - bshift, y1)
    print("A=%s %s   B=%s %s   共同区域 %dx%d  (ashift=%d bshift=%d)"
          % (a_path, A.size, b_path, B.size, x1 - x0, H - y0, ashift, bshift))

    pairs = Counter()
    diff = set()
    for y in range(y0, H):
        for x in range(x0, x1):
            ca, cb = pa[x, y + ashift], pb[x, y + bshift]
            if ca != cb:
                diff.add((x, y))
                pairs[(ca, cb)] += 1
    print("差异像素 = %d / %d (%.2f%%)" % (len(diff), (x1 - x0) * (H - y0),
                                        100.0 * len(diff) / max(1, (x1 - x0) * (H - y0))))
    print("--- 差异颜色对（前 15）---")
    for (ca, cb), n in pairs.most_common(15):
        print("   A=#%02X%02X%02X  B=#%02X%02X%02X   x%d" % (ca[0], ca[1], ca[2], cb[0], cb[1], cb[2], n))

    cl = clusters(diff, x0, y0, x1, H)
    print("--- 差异块（面积倒序，前 %d / 共 %d）---" % (min(MAXBLOCKS, len(cl)), len(cl)))
    for n, minx, miny, maxx, maxy in cl[:MAXBLOCKS]:
        # 这个块里的代表颜色对
        rep = Counter()
        for y in range(miny, maxy + 1):
            for x in range(minx, maxx + 1):
                if (x, y) in diff:
                    rep[(pa[x, y], pb[x, y])] += 1
        (ca, cb), _ = rep.most_common(1)[0]
        print("   x%-4d y%-4d .. x%-4d y%-4d  %4d px   A=#%02X%02X%02X B=#%02X%02X%02X"
              % (minx, miny, maxx, maxy, n, ca[0], ca[1], ca[2], cb[0], cb[1], cb[2]))

    if out:
        cw, ch = x1 - x0, H - y0
        sheet = Image.new("RGB", (cw, ch * 3 + 16), (40, 40, 40))
        sheet.paste(A.crop((x0, y0 + ashift, x1, H + ashift)), (0, 0))
        d = Image.new("RGB", (cw, ch))
        dp = d.load()
        for y in range(ch):
            for x in range(cw):
                ca = pa[x + x0, y + y0 + ashift]
                if (x + x0, y + y0) in diff:
                    dp[x, y] = (255, 0, 255)
                else:
                    dp[x, y] = tuple(int(v * 0.25 + 255 * 0.75) for v in ca)
        sheet.paste(d, (0, ch + 8))
        sheet.paste(B.crop((x0, y0 + bshift, x1, H + bshift)), (0, 2 * ch + 16))
        sheet = sheet.resize((cw * 2, (ch * 3 + 16) * 2), Image.NEAREST)
        sheet.save(out)
        print("对照图 -> %s（上=%s 中=差异 下=%s）" % (out, la, lb))


if __name__ == "__main__":
    main()

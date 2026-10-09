# -*- coding: utf-8 -*-
"""把原版 IDE 截图与 EClone 截图拼成上下对比图（并在关键区域放大）。"""
import os

from PIL import Image, ImageDraw, ImageFont

ORIG = r"D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\ide_probe\ide_opened.png"
MINE = r"D:\eclone\shots\m16.png"
OUT = r"D:\eclone\shots\compare_side_by_side.png"
OUT2 = r"D:\eclone\shots\compare_zoom.png"


def font(sz):
    for p in (r"C:\Windows\Fonts\msyh.ttc", r"C:\Windows\Fonts\simsun.ttc",
              r"C:\Windows\Fonts\msyh.ttf"):
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, sz)
            except Exception:
                pass
    return ImageFont.load_default()


def main():
    a = Image.open(ORIG).convert("RGB")
    b = Image.open(MINE).convert("RGB")
    w = max(a.width, b.width)
    pad, hdr = 8, 30
    canvas = Image.new("RGB", (w + pad * 2, hdr * 2 + a.height + b.height + pad * 3),
                       (40, 40, 40))
    d = ImageDraw.Draw(canvas)
    f = font(18)
    d.text((pad, 6), "原版 易语言 5.9 IDE（实测截图）", fill=(255, 220, 120), font=f)
    canvas.paste(a, (pad, hdr + pad))
    y2 = hdr + pad + a.height + pad
    d.text((pad, y2 - hdr + 4), "EClone（VC6/MFC 复刻，同一尺寸 1440x753）",
           fill=(120, 220, 255), font=f)
    canvas.paste(b, (pad, y2 + pad))
    canvas.save(OUT)
    print("saved", OUT, canvas.size)

    # 放大对比：菜单条+工具条 / 编辑器左上角
    rects = [(0, 40, 700, 120), (330, 80, 760, 300)]
    tiles = []
    for (x1, y1, x2, y2) in rects:
        ca = a.crop((x1, y1, x2, y2)).resize(((x2 - x1) * 2, (y2 - y1) * 2), Image.NEAREST)
        cb = b.crop((x1, y1, x2, y2)).resize(((x2 - x1) * 2, (y2 - y1) * 2), Image.NEAREST)
        tiles.append((ca, cb, (x1, y1, x2, y2)))
    tw = max(t[0].width for t in tiles)
    th = sum(t[0].height * 2 + 26 for t in tiles) + 20
    z = Image.new("RGB", (tw + 16, th + 16), (30, 30, 30))
    dz = ImageDraw.Draw(z)
    y = 8
    for (ca, cb, r) in tiles:
        dz.text((8, y), "原版 %s" % (r,), fill=(255, 220, 120), font=font(15))
        y += 20
        z.paste(ca, (8, y))
        y += ca.height + 6
        dz.text((8, y), "EClone", fill=(120, 220, 255), font=font(15))
        y += 20
        z.paste(cb, (8, y))
        y += cb.height + 10
    z.save(OUT2)
    print("saved", OUT2, z.size)


if __name__ == "__main__":
    main()

# measure.py -- 在截图指定区域里找出"深色像素"的连通块包围盒，用于量原版控件的确切位置
# 用法: python measure.py <png> <x,y,w,h> [thresh]
import sys
from PIL import Image

src, box = sys.argv[1], sys.argv[2]
th = int(sys.argv[3]) if len(sys.argv) > 3 else 96
x, y, w, h = [int(v) for v in box.split(',')]
im = Image.open(src).convert('RGB')
px = im.load()
dark = set()
for j in range(y, min(im.height, y + h)):
    for i in range(x, min(im.width, x + w)):
        r, g, b = px[i, j]
        if r < th and g < th and b < th:
            dark.add((i, j))
seen = set()
out = []
for p in dark:
    if p in seen:
        continue
    stack, comp = [p], []
    seen.add(p)
    while stack:
        cx, cy = stack.pop()
        comp.append((cx, cy))
        for nx, ny in ((cx+1,cy),(cx-1,cy),(cx,cy+1),(cx,cy-1)):
            if (nx, ny) in dark and (nx, ny) not in seen:
                seen.add((nx, ny))
                stack.append((nx, ny))
    xs = [c[0] for c in comp]; ys = [c[1] for c in comp]
    out.append((min(xs), min(ys), max(xs)-min(xs)+1, max(ys)-min(ys)+1, len(comp)))
for b in sorted(out, key=lambda t: -t[4]):
    print('bbox x=%d y=%d w=%d h=%d px=%d' % b)

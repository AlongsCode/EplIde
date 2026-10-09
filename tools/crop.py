# crop.py -- 截图局部放大，用于和原版逐像素对照
# 用法: python crop.py <in.png> <x,y,w,h> <out.png> [scale]
import sys
from PIL import Image

src, box, out = sys.argv[1], sys.argv[2], sys.argv[3]
scale = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0
x, y, w, h = [int(v) for v in box.split(',')]
im = Image.open(src)
print('%s size=%s' % (src, im.size))
x2, y2 = min(im.width, x + w), min(im.height, y + h)
c = im.crop((x, y, x2, y2))
if scale != 1.0:
    c = c.resize((int(c.width * scale), int(c.height * scale)), Image.NEAREST)
c.save(out)
print('-> %s %s' % (out, c.size))

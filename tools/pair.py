# pair.py -- 把若干张截图的局部裁切竖着拼成一张对比图
# 用法: python pair.py <out.png> <scale> "<png,x,y,w,h>" "<png,x,y,w,h>" ...
import sys
from PIL import Image, ImageDraw

out, scale = sys.argv[1], float(sys.argv[2])
crops = []
for spec in sys.argv[3:]:
    path, box = spec.rsplit(',', 4)[0], [int(v) for v in spec.rsplit(',', 4)[1:]]
    im = Image.open(path)
    x, y, w, h = box
    c = im.crop((x, y, min(im.width, x + w), min(im.height, y + h)))
    if scale != 1.0:
        c = c.resize((int(c.width * scale), int(c.height * scale)), Image.NEAREST)
    crops.append((path.rsplit('\\', 1)[-1] + " " + str(box), c))

W = max(c.width for _, c in crops) + 8
H = sum(c.height + 16 for _, c in crops) + 4
sheet = Image.new("RGB", (W, H), (255, 0, 0))
d = ImageDraw.Draw(sheet)
y = 2
for label, c in crops:
    sheet.paste(c, (4, y))
    y += c.height + 16
sheet.save(out)
print("-> %s %s" % (out, sheet.size))

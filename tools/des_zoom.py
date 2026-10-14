import sys
from PIL import Image
a=Image.open(r"shots\des_round\r1\orig.png").convert("RGB")
b=Image.open(r"shots\des_round\r1\mine.png").convert("RGB")
x0,y0,x1,y1=[int(v) for v in sys.argv[1:5]]
z=int(sys.argv[5]) if len(sys.argv)>5 else 4
out=sys.argv[6] if len(sys.argv)>6 else "shots\\des_round\\zoom.png"
ca=a.crop((x0,y0,x1,y1)); cb=b.crop((x0,y0,x1,y1))
w,h=ca.size
sheet=Image.new("RGB",(w, h*2+6),(30,30,30))
sheet.paste(ca,(0,0)); sheet.paste(cb,(0,h+6))
sheet=sheet.resize((w*z,(h*2+6)*z), Image.NEAREST)
sheet.save(out)
print(out, sheet.size)

from PIL import Image
for tag in ("orig","mine"):
    im=Image.open(r"shots\des_round\r6\%s.png"%tag).convert("RGB"); p=im.load()
    xs=[]
    for y in range(5,23):
        for x in range(3,300):
            r,g,b=p[x,y]
            if r<0x70 or (b>0xC0 and r>0x80 and g>0x80 and g<0xB0 and b-r>0x30):
                pass
            if (r<0x78 and g<0x78) or (r>0x90 and g<0x88 and b<0x90):
                xs.append(x)
    ys=[]
    for y in range(3,26):
        for x in range(3,300):
            r,g,b=p[x,y]
            if (r<0x78 and g<0x78) or (r>0x90 and g<0x88 and b<0x90):
                ys.append(y)
    print(tag, "text bbox x=%d..%d (w=%d) y=%d..%d"%(min(xs),max(xs),max(xs)-min(xs)+1,min(ys),max(ys)))

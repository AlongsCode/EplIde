from PIL import Image
for tag in ("orig","mine"):
    im=Image.open(r"shots\des_round\r1\%s.png"%tag).convert("RGB"); p=im.load()
    print("=== %s %s"%(tag,im.size))
    for y in range(0,30):
        row=[]
        prev=None; start=0
        for x in range(0,404):
            c=p[x,y]
            if c!=prev:
                if prev is not None and x-start>2: row.append("%d-%d:%02X%02X%02X"%(start,x-1,prev[0],prev[1],prev[2]))
                prev=c; start=x
        if 404-start>2: row.append("%d-%d:%02X%02X%02X"%(start,403,prev[0],prev[1],prev[2]))
        print("y%02d  %s"%(y," ".join(row)[:230]))

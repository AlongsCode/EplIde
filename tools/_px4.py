from PIL import Image
for tag in ("orig","mine"):
    im=Image.open(r"shots\des_round\r2\%s.png"%tag).convert("RGB"); p=im.load()
    print("=== "+tag)
    for y in (34,35,36,42,50,58):
        runs=[];prev=None;st=270
        for x in range(270,404):
            c=p[x,y]
            if c!=prev:
                if prev is not None: runs.append("%d-%d:%02X%02X%02X"%(st,x-1,prev[0],prev[1],prev[2]))
                prev=c;st=x
        runs.append("%d-%d:%02X%02X%02X"%(st,403,prev[0],prev[1],prev[2]))
        print(" y=%d  %s"%(y," ".join(runs)))

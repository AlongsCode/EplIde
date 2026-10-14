from PIL import Image
for tag in ("orig","mine"):
    im=Image.open(r"shots\des_round\r2\%s.png"%tag).convert("RGB"); p=im.load()
    print("=== "+tag)
    for y in (40,41,42,43,44,45,46,47,48,49,50,300):
        runs=[];prev=None;st=0
        for x in range(0,120):
            c=p[x,y]
            if c!=prev:
                if prev is not None: runs.append("%d-%d:%02X%02X%02X"%(st,x-1,prev[0],prev[1],prev[2]))
                prev=c;st=x
        runs.append("%d-%d:%02X%02X%02X"%(st,119,prev[0],prev[1],prev[2]))
        print(" y=%d  %s"%(y," ".join(runs)))
    for y in (411,412,413,414,415,416,417,418,419,420,421,422):
        runs=[];prev=None;st=0
        for x in range(0,60):
            c=p[x,y]
            if c!=prev:
                if prev is not None: runs.append("%d-%d:%02X%02X%02X"%(st,x-1,prev[0],prev[1],prev[2]))
                prev=c;st=x
        runs.append("%d-%d:%02X%02X%02X"%(st,59,prev[0],prev[1],prev[2]))
        print(" y=%d  %s"%(y," ".join(runs)))

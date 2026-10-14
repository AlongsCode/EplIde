from PIL import Image
for tag in ("orig","mine"):
    im=Image.open(r"shots\des_round\r6\%s.png"%tag).convert("RGB"); p=im.load()
    print("=== "+tag)
    for y in (0,1,2,5,9,12,13,18,25,26,410,411,418,420,421,422):
        row=[];prev=None;st=0
        for x in range(0,60):
            c=p[x,y]
            if c!=prev:
                if prev is not None and x-st>0: row.append("%d-%d:%02X%02X%02X"%(st,x-1,prev[0],prev[1],prev[2]))
                prev=c;st=x
        row.append("%d-%d:%02X%02X%02X"%(st,59,prev[0],prev[1],prev[2]))
        print(" y%02d %s"%(y," ".join(row)[:150]))

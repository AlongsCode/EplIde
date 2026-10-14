from PIL import Image
for tag in ("orig","mine"):
    im=Image.open(r"shots\des_round\r7\%s.png"%tag).convert("RGB"); p=im.load()
    print("=== "+tag)
    for y in range(405,413):
        row=[];prev=None;st=0
        for x in range(0,404):
            c=p[x,y]
            if c!=prev:
                if prev is not None and x-st>1: row.append("%d-%d:%02X%02X%02X"%(st,x-1,prev[0],prev[1],prev[2]))
                prev=c;st=x
        row.append("%d-%d:%02X%02X%02X"%(st,403,prev[0],prev[1],prev[2]))
        print(" y%d %s"%(y," ".join(row)[:220]))

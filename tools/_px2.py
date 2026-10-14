from PIL import Image
im=Image.open(r"shots\des_round\r1\orig.png").convert("RGB"); p=im.load()
def show(y,x0,x1,label=""):
    runs=[];prev=None;st=x0
    for x in range(x0,x1):
        c=p[x,y]
        if c!=prev:
            if prev is not None: runs.append("%d-%d:%02X%02X%02X"%(st,x-1,prev[0],prev[1],prev[2]))
            prev=c;st=x
    runs.append("%d-%d:%02X%02X%02X"%(st,x1-1,prev[0],prev[1],prev[2]))
    print("y=%d %s %s"%(y,label," ".join(runs)))
for y in (32,33,34,35,36,37,38,39,40,50,100,200,405,406,407,408,409,410,411,412):
    show(y,4,60,"")
print()
for x in (8,9,10,11,12,13,14,20,100,278,279,280,281,282,283,284):
    runs=[];prev=None;st=30
    for y in range(30,60):
        c=p[x,y]
        if c!=prev:
            if prev is not None: runs.append("%d-%d:%02X%02X%02X"%(st,y-1,prev[0],prev[1],prev[2]))
            prev=c;st=y
    runs.append("%d-%d:%02X%02X%02X"%(st,59,prev[0],prev[1],prev[2]))
    print("x=%d  %s"%(x," ".join(runs)))

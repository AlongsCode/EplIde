import sys,time,os
sys.path.insert(0, r"C:\Users\Along\Desktop\eclone\tools")
import drive
from des_round import activate_design_doc, MINE, SAMPLE, OUTROOT
from PIL import Image
out=os.path.join(OUTROOT,"r7")
a=Image.open(os.path.join(out,"orig.png")).convert("RGB"); pa=a.load()
for tag,env in (("nohook2",{"ECLONE_NOFORMHOOK":"1"}),):
    app=drive.App(MINE,args=[SAMPLE,"designer"],wait=8,env=env)
    activate_design_doc(app); drive.u.SetCursorPos(2,2); time.sleep(1)
    im=app.shot(app.find("WTWindow"), os.path.join(out,"mine_%s.png"%tag)); app.close()
    pb=im.load()
    band=[(x,y) for y in (408,409,410) for x in range(404) if pa[x,y]!=pb[x,y]]
    print(tag,"band px (rows408-410)=",len(band))
    rows=sorted(set(y for x,y in band)); print("  rows:",rows)
    print("  sample:",[('%d,%d'%(x,y)) for x,y in band[:6]])

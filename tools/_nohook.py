import sys,time,os
sys.path.insert(0, r"C:\Users\Along\Desktop\eclone\tools")
import drive
from des_round import activate_design_doc, MINE, SAMPLE, OUTROOT
from PIL import Image, ImageChops
out=os.path.join(OUTROOT,"r7")
app=drive.App(MINE,args=[SAMPLE,"designer"],wait=8,env={"ECLONE_NOFORMHOOK":"1"})
activate_design_doc(app); drive.u.SetCursorPos(2,2); time.sleep(1)
f=app.find("WTWindow")
im=app.shot(f, os.path.join(out,"mine_nohook.png"))
app.close()
a=Image.open(os.path.join(out,"orig.png")).convert("RGB"); b=im.convert("RGB")
pa,pb=a.load(),b.load()
diff=[(x,y) for y in range(423) for x in range(404) if pa[x,y]!=pb[x,y]]
print("nohook: diff=%d/%d = %.2f%% same"%(len(diff),404*423,100.0*(404*423-len(diff))/(404*423)))
ys=sorted(set(y for x,y in diff))
print("diff rows:",ys[:20])

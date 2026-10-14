import sys,time,os
sys.path.insert(0, r"C:\Users\Along\Desktop\eclone\tools")
import drive
from des_round import activate_design_doc, MINE, SAMPLE, OUTROOT
from PIL import Image
out=os.path.join(OUTROOT,"r7")
a=Image.open(os.path.join(out,"orig.png")).convert("RGB"); pa=a.load()
for tag,env in (("nogrid",{"ECLONE_NOGRID":"1"}),("noovlhook",{"ECLONE_NOOVLHOOK":"1"})):
    app=drive.App(MINE,args=[SAMPLE,"designer"],wait=8,env=env)
    activate_design_doc(app); drive.u.SetCursorPos(2,2); time.sleep(1)
    im=app.shot(app.find("WTWindow"), os.path.join(out,"mine_%s.png"%tag)); app.close()
    pb=im.load()
    diff=[(x,y) for y in range(423) for x in range(404) if pa[x,y]!=pb[x,y]]
    ys=sorted(set(y for x,y in diff))
    print("%-9s diff=%4d (%.2f%%) rows_with_408or409=%s rows408_409_px=%d"%(tag,len(diff),100.0*(404*423-len(diff))/(404*423),
          (408 in ys or 409 in ys), len([1 for x,y in diff if y in (408,409)])))

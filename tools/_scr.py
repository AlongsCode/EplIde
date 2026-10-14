import sys,time,os
sys.path.insert(0, r"C:\Users\Along\Desktop\eclone\tools")
import drive
from des_round import activate_design_doc, MINE, SAMPLE, OUTROOT
from PIL import Image, ImageGrab
app=drive.App(MINE,args=[SAMPLE,"designer"],wait=8)
activate_design_doc(app); drive.u.SetCursorPos(2,2); time.sleep(1)
f=app.find("WTWindow")
x,y,w,h=drive.rect(f)
im=app.shot(f, os.path.join(OUTROOT,"r7","mine_pw.png"))
sc=ImageGrab.grab(bbox=(x,y,x+w,y+h)).convert("RGB")
sc.save(os.path.join(OUTROOT,"r7","mine_screen.png"))
pi,si=im.load(),sc.load()
for yy in (405,406,407,408,409,410,411):
    print("y%d  pw=%s"%(yy,['%02X%02X%02X'%pi[xx,yy] for xx in (11,20,150,281,315,350)]))
    print("     sc=%s"%(['%02X%02X%02X'%si[xx,yy] for xx in (11,20,150,281,315,350)],))
app.close()

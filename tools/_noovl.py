import sys,os
sys.path.insert(0, r"C:\Users\Along\Desktop\eclone\tools")
import drive, time
from PIL import Image
from des_round import activate_design_doc, MINE, SAMPLE, OUTROOT
os.environ["ECLONE_DES_NOOVERLAY"]="1"
for tag,env,args in (("noovl",{"ECLONE_DES_NOOVERLAY":"1"},[SAMPLE,"designer"]),):
    app=drive.App(MINE,args=args,wait=8,env=env)
    activate_design_doc(app)
    drive.u.SetCursorPos(2,2); time.sleep(1)
    f=app.find("WTWindow")
    p=os.path.join(OUTROOT,"r7","mine_%s.png"%tag)
    im=app.shot(f,p)
    # screen grab too
    x,y,w,h=drive.rect(f)
    from PIL import ImageGrab
    scr=ImageGrab.grab(bbox=(x,y,x+w,y+h)); scr.save(os.path.join(OUTROOT,"r7","mine_%s_screen.png"%tag))
    pl=im.load(); ps=scr.convert("RGB").load()
    for yy in (407,408,409,410):
        print("y%d printwin: %s"%(yy, [ "%02X%02X%02X"%pl[xx,yy] for xx in (11,20,150,281,315,350,394)]))
        print("     screen  : %s"%( [ "%02X%02X%02X"%ps[xx,yy] for xx in (11,20,150,281,315,350,394)],))
    app.close()

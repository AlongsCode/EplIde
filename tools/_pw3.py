import sys,time
sys.path.insert(0, r"C:\Users\Along\Desktop\eclone\tools")
import drive
from PIL import Image
res={}
for tag,exe,args,wait in (("orig", r"C:\Program Files (x86)\e\e.exe", [r"C:\Users\Along\AppData\Local\Temp\e_re\samples\flowcmd.e"],14),
                          ("mine", r"C:\Users\Along\Desktop\eclone\bin\EClone.exe", [r"C:\Users\Along\AppData\Local\Temp\e_re\samples\flowcmd.e","designer"],8)):
    app=drive.App(exe,args=args,wait=wait)
    if tag=="orig":
        mdi=None
        for h in drive.descendants(app.frame):
            if drive.cls(h)=="MDIClient" and drive.rect(h)[2]>1000: mdi=h
        for h in drive.descendants(mdi):
            t=drive.title(h)
            if "\u7a97\u53e3" in t and "\u7a0b\u5e8f\u96c6" not in t:
                drive.u.SendMessageW(mdi,0x0222,h,0); time.sleep(2)
    pnl=app.find("_EL_DrawPanel"); btn=app.find("Button")
    im=app.shot(pnl, r"C:\Users\Along\Desktop\eclone\shots\des_round\r7\panel_%s.png"%tag)
    ib=app.shot(btn, r"C:\Users\Along\Desktop\eclone\shots\des_round\r7\btn_%s.png"%tag)
    for name,img,rows in (("panel",im,(0,1,372,373,374,375)),("btn",ib,(0,1,30,31))):
        q=img.load()
        print(tag,name,img.size,[ "%d:%02X%02X%02X"%(y,q[20,y][0],q[20,y][1],q[20,y][2]) for y in rows if y<img.size[1]])
    app.close()

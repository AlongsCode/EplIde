import sys,time,os
sys.path.insert(0, r"C:\Users\Along\Desktop\eclone\tools")
import drive
from PIL import Image
S=r"C:\Users\Along\AppData\Local\Temp\e_re\samples\flowcmd.e"
app=drive.App(r"C:\Program Files (x86)\e\e.exe",args=[S],wait=14)
frame=app.frame
mdi=None
for h in drive.descendants(frame):
    if drive.cls(h)=="MDIClient" and drive.rect(h)[2]>1000: mdi=h
docs=[h for h in drive.descendants(mdi) if drive.cls(h).startswith("Afx:400000:b")]
for h in docs: print("doc",h,drive.rect(h),drive.title(h))
def shot(tag):
    f=app.find("WTWindow")
    im=app.shot(f, r"C:\Users\Along\Desktop\eclone\shots\des_round\probe_%s.png"%tag)
    p=im.load()
    print(tag,"form",drive.rect(f),"y2=",p[200,2],"y25=",p[200,25],"btn x380 y13=",p[380,13],"text x30 y13=",p[30,13])
des=[h for h in docs if "\u7a97\u53e3" in drive.title(h) and "\u7a0b\u5e8f\u96c6" not in drive.title(h)]
cod=[h for h in docs if "\u7a0b\u5e8f\u96c6" in drive.title(h)]
drive.u.SendMessageW(mdi,0x0222,des[0],0); time.sleep(2.5); shot("des_active")
drive.u.SendMessageW(mdi,0x0222,cod[0],0); time.sleep(2.5); shot("code_active")
app.close()

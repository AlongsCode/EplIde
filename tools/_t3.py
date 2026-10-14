import sys,time
sys.path.insert(0, r"C:\Users\Along\Desktop\eclone\tools")
import drive
for s in (r"C:\Users\Along\AppData\Local\Temp\e_re\samples\superlist.e",
          r"C:\Users\Along\Desktop\eclone\shots\e_samples\graphic.e"):
    app=drive.App(r"C:\Program Files (x86)\e\e.exe",args=[s],wait=14)
    mdi=None
    for h in drive.descendants(app.frame):
        if drive.cls(h)=="MDIClient" and drive.rect(h)[2]>1000: mdi=h
    for h in drive.descendants(mdi):
        t=drive.title(h)
        if "\u7a97\u53e3" in t and "\u7a0b\u5e8f\u96c6" not in t:
            drive.u.SendMessageW(mdi,0x0222,h,0); time.sleep(2)
    f=app.find("WTWindow")
    print(s.split("\\")[-1], "WTWindow", drive.rect(f) if f else None, "|%s|"%drive.title(f))
    app.close()

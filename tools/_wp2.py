import sys,time,subprocess
sys.path.insert(0, r"C:\Users\Along\Desktop\eclone\tools")
import drive
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
    print("==== "+tag)
    for cls in ("_EL_DrawPanel","Button"):
        out=subprocess.run([r"C:\Users\Along\Desktop\eclone\tools\wndproc32.exe", str(app.pid), cls], capture_output=True, text=True).stdout
        print(out.strip()[:400])
    app.close()

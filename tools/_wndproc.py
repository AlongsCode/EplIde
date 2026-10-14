import ctypes, ctypes.wintypes as wt, sys, time, os
sys.path.insert(0, r"C:\Users\Along\Desktop\eclone\tools")
import drive
u=ctypes.windll.user32
u.GetClassLongW.argtypes=[wt.HWND, ctypes.c_int]
u.GetClassLongW.restype=ctypes.c_ulong
u.GetWindowLongW.argtypes=[wt.HWND, ctypes.c_int]
u.GetWindowLongW.restype=ctypes.c_long
psapi=ctypes.windll.psapi; k32=ctypes.windll.kernel32
psapi.GetModuleFileNameExW.argtypes=[ctypes.c_void_p, ctypes.c_void_p, wt.LPWSTR, ctypes.c_uint]
psapi.GetModuleInformation.argtypes=[ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint]
def mods(pid, addr):
    h=k32.OpenProcess(0x0410, False, pid)
    if not h: return "OpenProcess failed"
    arr=(ctypes.c_void_p*1024)(); need=wt.DWORD()
    psapi.EnumProcessModules(h, arr, ctypes.sizeof(arr), ctypes.byref(need))
    n=need.value//ctypes.sizeof(ctypes.c_void_p)
    for i in range(n):
        m=arr[i]
        buf=ctypes.create_unicode_buffer(260); psapi.GetModuleFileNameExW(h, m, buf, 260)
        info=ctypes.create_string_buffer(64); psapi.GetModuleInformation(h, m, info, 64)
        base=ctypes.c_void_p.from_buffer(info).value; size=ctypes.c_int.from_buffer(info,4).value
        if base<=addr<base+size:
            k32.CloseHandle(h)
            return "%s +0x%X"%(os.path.basename(buf.value), addr-base)
    k32.CloseHandle(h)
    return "??"
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
                u.SendMessageW(mdi,0x0222,h,0); time.sleep(2)
    f=app.find("WTWindow")
    wp=u.GetClassLongW(f,-24); wg=u.GetWindowLongW(f,-4)
    print("%-5s WTWindow classproc=0x%08X -> %s | gwlp=0x%08X -> %s"%(tag,wp,mods(app.pid,wp),wg & 0xFFFFFFFF, mods(app.pid,wg & 0xFFFFFFFF)))
    app.close()

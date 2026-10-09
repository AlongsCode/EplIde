# -*- coding: utf-8 -*-
"""List the 插入 (insert) menu command ids + their resource-string tooltips from the original.

usage: python tools\\insert_menu.py <out.txt>
"""
import io
import re
import sys

out = sys.argv[1] if len(sys.argv) > 1 else None

# menu_main.rc was generated from e.exe RT_MENU 127; pull the insert popup out of it.
rc = io.open(r"D:\eclone\src\menu_main.rc", encoding="gbk", errors="replace").read()
lines = rc.splitlines()
res = []
in_ins = False
for ln in lines:
    st = ln.strip()
    if st.startswith("POPUP"):
        if in_ins:
            in_ins = False
        if re.search(r",\s*3277\d\s*$", st) or ("插入" in st):
            in_ins = True
        continue
    if in_ins and st.startswith("MENUITEM"):
        res.append(st)
    if in_ins and st == "END":
        in_ins = False

# resource strings carry "hint|menu text" pairs for these ids
rs = io.open(r"D:\eclone\shots\e_res_strings.txt", encoding="utf-8", errors="replace").read().splitlines()
byid = {}
for l in rs:
    m = re.match(r"\s*(\d+)\s\s(.*)$", l, re.S)
    if m:
        byid[m.group(1)] = m.group(2).replace("\n", " | ")

txt = []
txt.append("== insert menu items from src\\menu_main.rc ==")
txt.extend(res)
txt.append("")
txt.append("== resource strings 32777..32831 ==")
for k in range(32777, 32832):
    if str(k) in byid:
        txt.append("%d  %s" % (k, byid[str(k)][:200]))

body = "\n".join(txt)
if out:
    io.open(out, "w", encoding="utf-8").write(body)
    print("wrote " + out)
else:
    print("(pass an out.txt)")

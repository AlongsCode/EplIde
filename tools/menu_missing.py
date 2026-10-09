# -*- coding: utf-8 -*-
"""List original menu commands (from src/menu_main.rc) that have no ON_COMMAND handler.

usage: python tools/menu_missing.py
"""
import re

rc = open(r"D:\eclone\src\menu_main.rc", encoding="utf-8").read()
cpp = open(r"D:\eclone\src\EClone.cpp", encoding="utf-8").read()

items = re.findall(r"MENUITEM [^,]+, (\d+)", rc)
names = {i: n for n, i in re.findall(r'MENUITEM "([^"]+)", (\d+)', rc)}
handled = set(re.findall(r"ON_COMMAND\((\d+)", cpp))
mfc = {"57649", "57650", "57651", "57652", "57653", "57654", "57643", "57644",
       "57634", "57635", "57637", "57616", "57665", "57664"}
missing = [i for i in items if i not in handled and i not in mfc]
out = []
for i in missing:
    out.append("%-6s %s" % (i, names.get(i, "?")))
open(r"D:\eclone\shots\menu_missing.txt", "w", encoding="utf-8").write("\n".join(out) + "\n")
print("wrote %d missing commands" % len(missing))

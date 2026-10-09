# -*- coding: utf-8 -*-
"""Report which original menu commands (from src/menu_main.rc) have no handler yet.

usage: python tools/menu_coverage.py
"""
import re

rc = open(r"D:\eclone\src\menu_main.rc", encoding="utf-8").read()
cpp = open(r"D:\eclone\src\EClone.cpp", encoding="utf-8").read()

items = re.findall(r"MENUITEM [^,]+, (\d+)", rc)
names = {i: n for n, i in re.findall(r'MENUITEM "([^"]+)", (\d+)', rc)}
handled = set(re.findall(r"ON_COMMAND\((\d+)", cpp))
# MFC standard ids handled by CMDIFrameWnd itself (afxres.h)
mfc = {"57649", "57650", "57651", "57652", "57653", "57654", "57643", "57644",
       "57634", "57635", "57637", "57616", "57665", "57664"}
referenced = set(re.findall(r"\b(" + "|".join(sorted(set(items))) + r")\b", cpp))

missing = [i for i in items if i not in handled and i not in mfc]
print("menu items with a numeric id : %d" % len(items))
print("handled by ON_COMMAND        : %d %s" % (len(handled), sorted(handled)))
print("handled by MFC itself        : %d" % len([i for i in items if i in mfc]))
print("NOT handled                  : %d" % len(missing))
print()
for i in missing:
    tag = "  (id字符串在代码里出现过)" if i in referenced else ""
    print("  %-6s %s%s" % (i, names.get(i, "?"), tag))

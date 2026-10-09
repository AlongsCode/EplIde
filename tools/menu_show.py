# -*- coding: utf-8 -*-
"""List menu_main.rc entries relevant to a set of command ids (writes UTF-8 to stdout file).

usage: python tools\menu_show.py [out.txt]
"""
import io
import re
import sys

WANT = {
    "32875", "32954", "33006", "32934", "32862", "32874",
    "32800", "32801", "32802", "32803", "32804", "32805", "32806",
    "32807", "32808", "32809", "32810", "32811", "32812", "32813",
    "32817", "32818", "32819", "32825", "32826", "32827",
    "32777", "32778", "32779", "32780", "32782", "32783", "32786", "32787",
    "32828", "32829", "32830", "32831", "32929",
    "32854", "32860", "32940", "32853", "32852", "32867", "32871", "32945", "32870",
    "32963", "59392", "59393", "133", "135", "110", "129", "130", "132",
    "32989", "32990", "32991", "32992",
}

src = io.open(r"D:\eclone\src\menu_main.rc", encoding="gbk", errors="replace").read()
out = []
popup = ""
for ln in src.splitlines():
    if ln.strip().startswith("POPUP"):
        popup = ln.strip()
        out.append("\n== " + popup)
    m = re.search(r",\s*(\d+)\s*$", ln.strip())
    if m and m.group(1) in WANT:
        out.append("   " + ln.strip())

txt = "\n".join(out)
dest = sys.argv[1] if len(sys.argv) > 1 else None
if dest:
    io.open(dest, "w", encoding="utf-8").write(txt)
    print("wrote " + dest)
else:
    print(txt)

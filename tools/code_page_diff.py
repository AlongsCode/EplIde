# -*- coding: utf-8 -*-
"""Reproduce the documented code-page 1:1 comparison:
    original   shots\\live_tabs\\view0.png   (graphic.e code page, 1174x388)
    clone      shots\\code_view.png          (same 6 lines fed into the sample)
and report the pixel diff via tools\\diff_png.py.

Steps: patch sample -> build -> screenshot -> diff -> restore sample (always).

usage: python tools/code_page_diff.py
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, "tools")
SHOTS = os.path.join(ROOT, "shots")
PY = sys.executable


def run(cmd, **kw):
    print("$ " + " ".join(cmd))
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace", **kw)
    if r.stdout:
        print(r.stdout.rstrip())
    if r.returncode != 0 and r.stderr:
        print("[stderr] " + r.stderr.strip()[-800:])
    return r


def main():
    orig = os.path.join(SHOTS, "live_tabs", "view0.png")
    mine = os.path.join(SHOTS, "code_view.png")
    if not os.path.exists(orig):
        print("[SKIP] original baseline missing: %s" % orig)
        return 2
    # the patch script hard-codes D:\eclone; rewrite to this ROOT on the fly
    patched = 0
    for name in ("patch_sample_graphic.py", "restore_sample.py", "sampleblock.py"):
        p = os.path.join(TOOLS, name)
        if not os.path.exists(p):
            continue
        with open(p, encoding="utf-8") as f:
            t = f.read()
        if r'D:\eclone' in t:
            with open(p, "w", encoding="utf-8", newline="") as f:
                f.write(t.replace(r'D:\eclone', ROOT))
            patched += 1
    print("rewrote D:\\eclone -> %s in %d tool(s)" % (ROOT, patched))

    try:
        r = run([PY, os.path.join(TOOLS, "patch_sample_graphic.py")])
        if r.returncode != 0:
            return 1
        exe = os.path.join(ROOT, "bin", "EClone.exe")
        before = os.path.getmtime(exe) if os.path.exists(exe) else 0
        # --nobuild: skip the (foreground, focus-stealing) relink.  Use when the exe is
        # already current -- e.g. while doing IDA-only research, the user does not want a
        # build.bat + EClone window stealing focus.  Pass it explicitly; the default stays
        # "always rebuild" so nobody silently diffs a stale binary (round 41's lesson).
        if "--nobuild" in sys.argv:
            print("[--nobuild] skipping build.bat (exe mtime left as-is)")
        else:
            r = run(["cmd", "/c", "build.bat"], cwd=ROOT)
            # A failed build leaves the previous exe in place, so every later screenshot
            # would silently come from a STALE binary (round 41 lost a whole cycle to this).
            # Refuse to continue unless the build reported OK and really relinked.
            if r.returncode != 0 or "BUILD OK" not in (r.stdout or ""):
                print("[FAIL] build.bat failed -- refusing to diff a stale exe")
                return 1
            if os.path.getmtime(exe) <= before:
                print("[FAIL] bin\\EClone.exe was not relinked -- refusing to diff a stale exe")
                return 1
        r = run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
                 "-File", os.path.join(TOOLS, "code_shot.ps1"),
                 "-Exe", exe, "-OutDir", SHOTS])
        if not os.path.exists(mine):
            print("[FAIL] %s was not produced" % mine)
            return 1
        # documented baseline: 0.62% difference
        r = run([PY, os.path.join(TOOLS, "diff_png.py"), orig, mine,
                 "0", "0", "1174", "388",
                 os.path.join(SHOTS, "cmp_code_now.png"), "orig", "mine"])
        return 0
    finally:
        run([PY, os.path.join(TOOLS, "restore_sample.py")])


if __name__ == "__main__":
    sys.exit(main())

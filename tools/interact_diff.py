# -*- coding: utf-8 -*-
"""Differential interaction harness: load the SAME .e in the original IDE and in EClone,
drive the SAME real mouse/keyboard actions, and snapshot both after every step.

usage:
  python tools/interact_diff.py --tree            # dump both window trees (identify the editor)
  python tools/interact_diff.py --case flow_basic  # run the action script, shots per step

Outputs: shots/interact/<role>/NN_<name>.png  + shots/interact/<role>_tree.txt
"""
import argparse
import io
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIG = r"C:\Program Files (x86)\e\e.exe"
MINE = os.path.join(ROOT, "bin", "EClone.exe")
# 真实样例：官方核心支持库「流程类命令.e」（如果真 x2 + 循环 x2），复制到 temp 避免改到安装目录
SAMPLE = r"C:\Users\Along\AppData\Local\Temp\e_re\samples\flowcmd.e"
OUT = os.path.join(ROOT, "shots", "interact")

VK_RETURN = 0x0D
VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN = 0x25, 0x27, 0x26, 0x28
VK_BACK, VK_DELETE, VK_HOME, VK_END = 0x08, 0x2E, 0x24, 0x23


def editor_window(app):
    """The code editor view: the biggest visible Afx: child (fallback: any big child)."""
    if not app.frame:
        return None
    best, bestarea = None, 0
    fallback, fbarea = None, 0
    for h in drive.descendants(app.frame):
        if not drive.u.IsWindowVisible(h):
            continue
        w, hh = drive.client(h)
        if hh < 80 or w < 200:
            continue
        area = w * hh
        if drive.cls(h).startswith("Afx:") and area > bestarea:
            best, bestarea = h, area
        if area > fbarea:
            fallback, fbarea = h, area
    return best or fallback


def current_editor(app):
    """Re-resolve frame + editor (handles can change while the app settles)."""
    fr = None
    for h in drive.tops(app.pid):
        if drive.cls(h) in ("ENewFrame", "ECMainFrame"):
            fr = h
    if fr:
        app.frame = fr
    return editor_window(app)


def wait_for_frame(pid, timeout=90.0):
    """Poll until the app's main frame exists (the original is slow on first start)."""
    t0 = time.time()
    while time.time() - t0 < timeout:
        for h in drive.tops(pid):
            if drive.cls(h) in ("ENewFrame", "ECMainFrame") and drive.u.IsWindowVisible(h):
                return h
        time.sleep(1.0)
    for h in drive.tops(pid):
        if drive.cls(h) in ("ENewFrame", "ECMainFrame"):
            return h
    return None


def dismiss_dialogs(app, log):
    """Record and close any visible modal dialog (the original may show one on load)."""
    for h in drive.tops(app.pid):
        if drive.cls(h) != "#32770" or not drive.u.IsWindowVisible(h):
            continue
        log.append("dialog: cls=#32770 title=%r rect=%s" % (drive.title(h), drive.rect(h)))
        # try the default button first (IDOK=1), then WM_CLOSE
        ok = drive.u.GetDlgItem(h, 1)
        if ok:
            drive.u.SendMessageW(ok, 0x00F5, 0, 0)      # BM_CLICK
        else:
            drive.u.PostMessageW(h, 0x0010, 0, 0)       # WM_CLOSE
        time.sleep(1.0)


def dump_tree(role, app, path):
    lines = []

    def visit(h, depth):
        if depth > 12:
            return
        try:
            lines.append("%s%-38s vis=%-5s %s |%s|" % (
                "  " * depth, drive.cls(h), bool(drive.u.IsWindowVisible(h)),
                drive.client(h), drive.title(h)[:60]))
        except Exception:
            return
        for c in drive.children(h):
            visit(c, depth + 1)

    if app.frame is None:
        lines.append("!! frame is None (no ENewFrame/ECMainFrame top-level for pid %s)" % app.pid)
    visit(app.frame, 0)
    io.open(path, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("tree(%s) -> %s (%d lines)" % (role, path, len(lines)))


def run_case(role, exe, case, sample, wait):
    outdir = os.path.join(OUT, role)
    os.makedirs(outdir, exist_ok=True)
    app = drive.App(exe, args=[sample], wait=3.0)
    try:
        frame = wait_for_frame(app.pid, timeout=wait)
        if frame:
            app.frame = frame
        notes = []
        dismiss_dialogs(app, notes)
        time.sleep(2.0)
        if not current_editor(app):
            print("[%s] !! editor not found; retry after settle" % role)
            time.sleep(3.0)
        ed = current_editor(app)
        dump_tree(role, app, os.path.join(OUT, role + "_tree.txt"))
        if notes:
            io.open(os.path.join(OUT, role + "_dialogs.txt"), "w", encoding="utf-8").write(
                "\n".join(notes) + "\n")
            for n in notes:
                print("[%s] %s" % (role, n.encode("utf-8", "replace").decode("utf-8")))
        print("[%s] frame=%s editor=%s cls=%s client=%s" % (
            role, app.frame, ed, drive.cls(ed) if ed else "-",
            drive.client(ed) if ed else "-"))
        step = [0]

        def shot(name):
            step[0] += 1
            ed2 = current_editor(app)
            p = os.path.join(outdir, "%02d_%s.png" % (step[0], name))
            if ed2:
                app.shot(ed2, p)                      # 编辑器客户区
            if app.frame:
                app.shot(app.frame, os.path.join(outdir,
                         "%02d_%s_full.png" % (step[0], name)))
            print("   shot %s (editor=%s)" % (name, ed2))

        def click(x, y):
            ed2 = current_editor(app)
            if ed2:
                app.click(ed2, x, y)
                time.sleep(0.6)

        shot("initial")

        if case == "flow_basic":
            click(120, 120)                 # 正文区第 3 行附近
            shot("click_text")
            app.key(VK_DOWN)
            app.key(VK_DOWN)
            shot("down2")
            app.key(0x41)                   # 'A'
            time.sleep(0.4)
            shot("typed_A")
            app.key(VK_RETURN)
            time.sleep(0.6)
            shot("enter")
            app.key(VK_BACK)
            time.sleep(0.4)
            shot("backspace")
            click(10, 120)                  # 左侧标志列
            shot("click_gutter")
            app.key(0x41, ctrl=True)        # Ctrl+A 全选
            time.sleep(0.6)
            shot("ctrl_a")
        elif case == "caret":
            for (x, y, tag) in ((120, 60, "row1"), (120, 96, "row2"), (120, 132, "row3")):
                click(x, y)
                shot("click_" + tag)
    finally:
        app.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tree", action="store_true")
    ap.add_argument("--case", default="flow_basic")
    ap.add_argument("--sample", default=SAMPLE)
    ap.add_argument("--wait-orig", type=float, default=18.0)
    ap.add_argument("--wait-mine", type=float, default=7.0)
    ap.add_argument("--only", choices=["orig", "mine"], default=None)
    args = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    print("sample = %s" % args.sample)

    if args.tree:
        for role, exe, wait in (("orig", ORIG, args.wait_orig), ("mine", MINE, args.wait_mine)):
            if args.only and args.only != role:
                continue
            app = drive.App(exe, args=[args.sample], wait=3.0)
            try:
                frame = wait_for_frame(app.pid, timeout=wait)
                if frame:
                    app.frame = frame
                notes = []
                dismiss_dialogs(app, notes)
                for n in notes:
                    print("[%s] %s" % (role, n))
                dump_tree(role, app, os.path.join(OUT, role + "_tree.txt"))
                print("[%s] frame=%s cls=%s client=%s" % (
                    role, app.frame, drive.cls(app.frame) if app.frame else "-",
                    drive.client(app.frame) if app.frame else "-"))
            finally:
                app.close()
        return

    for role, exe, wait in (("orig", ORIG, args.wait_orig), ("mine", MINE, args.wait_mine)):
        if args.only and args.only != role:
            continue
        print("=== %s (%s) case=%s ===" % (role, exe, args.case))
        run_case(role, exe, args.case, args.sample, wait)


if __name__ == "__main__":
    main()

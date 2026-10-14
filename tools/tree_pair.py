# -*- coding: utf-8 -*-
"""Capture the 支持库/程序 tree of both apps in the same state and measure the row geometry.

usage: python tools/tree_pair.py
out  : shots/interact/tree_orig.png / tree_mine.png  + a measured report on stdout
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive
import interact_diff as ID

OUT = os.path.join(ID.OUT)
os.makedirs(OUT, exist_ok=True)


def pick_tree(app):
    best, area = None, 0
    for h in drive.descendants(app.frame):
        if drive.cls(h) != "SysTreeView32" or not drive.u.IsWindowVisible(h):
            continue
        w, hh = drive.client(h)
        if w * hh > area:
            best, area = h, w * hh
    return best


def run(role, exe, wait):
    app = drive.App(exe, args=[ID.SAMPLE], wait=3.0)
    try:
        fr = ID.wait_for_frame(app.pid, timeout=wait)
        if fr:
            app.frame = fr
        notes = []
        ID.dismiss_dialogs(app, notes)
        time.sleep(2.0)
        ID.current_editor(app)
        tree = pick_tree(app)
        print("[%s] frame=%s tree=%s client=%s" % (
            role, app.frame, tree, drive.client(tree) if tree else "-"))
        if tree:
            app.shot(tree, os.path.join(OUT, "tree_%s.png" % role))
        else:
            app.shot(app.frame, os.path.join(OUT, "tree_%s_frame.png" % role))
        # 顺便把树的第一行文字读出来（TVM_GETITEM 太啰嗦，这里用窗口树报告代替）
        for h in drive.descendants(app.frame):
            if drive.cls(h) == "SysTreeView32":
                print("   tree %s vis=%s client=%s" % (
                    h, bool(drive.u.IsWindowVisible(h)), drive.client(h)))
    finally:
        app.close()


def main():
    print("sample = %s" % ID.SAMPLE)
    run("orig", ID.ORIG, 20.0)
    run("mine", ID.MINE, 7.0)


if __name__ == "__main__":
    main()

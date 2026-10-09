# -*- coding: utf-8 -*-
"""Recover HANDOFF.md the RIGHT way: pick the single longest `read HANDOFF.md` result whose lines
are consecutively numbered from 1 (i.e. it really is the head of the file), instead of unioning
chunks from different periods (which mixes line numbers and corrupts the file).

usage: python tools/recover_handoff_v2.py [outfile]
"""
import json
import os
import re
import sys

import zstandard as zstd

SESS = r"C:\Users\Along\.dsh\sessions\--C-Program~0020Files~0020~0028x86~0029-e--"
LINE = re.compile(r"^(\d+): ?(.*)$")


def texts_from(obj, out):
    if isinstance(obj, str):
        out.append(obj)
    elif isinstance(obj, dict):
        for v in obj.values():
            texts_from(v, out)
    elif isinstance(obj, list):
        for v in obj:
            texts_from(v, out)


def body_of(s):
    """strip the <path>/<type>/<content> wrapper if present"""
    i = s.find("<content>")
    j = s.rfind("</content>")
    if i >= 0 and j > i:
        return s[i + 9:j]
    return s


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else r"D:\eclone\shots\handoff_best.md"
    best = None
    for dirpath, dirnames, filenames in os.walk(SESS):
        for fn in filenames:
            if not fn.startswith("session.v4.jsonl"):
                continue
            p = os.path.join(dirpath, fn)
            try:
                if p.endswith(".zstd"):
                    with open(p, "rb") as f:
                        raw = zstd.ZstdDecompressor().stream_reader(f).read()
                else:
                    raw = open(p, "rb").read()
            except Exception:
                continue
            txt = raw.decode("utf-8", "replace")
            if "HANDOFF.md" not in txt:
                continue
            for ln in txt.split("\n"):
                if "HANDOFF" not in ln:
                    continue
                try:
                    obj = json.loads(ln)
                except Exception:
                    continue
                strs = []
                texts_from(obj, strs)
                for s in strs:
                    if len(s) < 2000:
                        continue
                    b = body_of(s)
                    # must start at line 1 and be consecutive
                    nums = []
                    for one in b.split("\n"):
                        m = LINE.match(one.rstrip("\r"))
                        if m:
                            nums.append(int(m.group(1)))
                    if not nums or nums[0] != 1:
                        continue
                    # consecutive run from 1
                    run = 1
                    for k in range(1, len(nums)):
                        if nums[k] == nums[k - 1] + 1:
                            run += 1
                        else:
                            break
                    if run < 300:
                        continue
                    # must look like the real head of HANDOFF
                    head = "\n".join(b.split("\n")[:3])
                    if "EClone" not in head or "HANDOFF" not in s[:400]:
                        continue
                    if best is None or run > best[0]:
                        best = (run, b, os.path.basename(dirpath))
                        print("new best: %d lines from %s" % (run, os.path.basename(dirpath)))
    if best is None:
        print("nothing found")
        return
    run, b, where = best
    open(out_path, "w", encoding="utf-8", newline="").write(b)
    print("wrote %d lines (%s) -> %s" % (run, where, out_path))


if __name__ == "__main__":
    main()

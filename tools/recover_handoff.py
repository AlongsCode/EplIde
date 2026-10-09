# -*- coding: utf-8 -*-
"""Recover HANDOFF.md content from DSH session transcripts.

HANDOFF.md got truncated by a buggy patch script; the transcripts contain the file's contents from
earlier `read` results and patch scripts. This scans every session transcript for the largest text
blob that looks like HANDOFF.md and writes the best one out.

usage: python tools/recover_handoff.py [outfile]
"""
import json
import os
import sys

import zstandard as zstd

SESS = r"C:\Users\Along\.dsh\sessions\--C-Program~0020Files~0020~0028x86~0029-e--"
MARK = "\u9009\u4e2d\u6846 + 8 \u4e2a\u624b\u67c4"      # 选中框 + 8 个手柄
MARK2 = "\u8bbe\u8ba1\u5668\u7684"                        # 设计器的


def texts_from(obj, out):
    """walk any JSON structure collecting strings"""
    if isinstance(obj, str):
        out.append(obj)
    elif isinstance(obj, dict):
        for v in obj.values():
            texts_from(v, out)
    elif isinstance(obj, list):
        for v in obj:
            texts_from(v, out)


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else r"D:\eclone\shots\handoff_recovered.md"
    best = ""
    best_where = ""
    chunks = []
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
            except Exception as e:
                print("skip %s: %s" % (p, e))
                continue
            txt = raw.decode("utf-8", "replace")
            if MARK not in txt and MARK2 not in txt:
                continue
            hits = 0
            for line in txt.split("\n"):
                if MARK not in line and MARK2 not in line:
                    continue
                hits += 1
                try:
                    obj = json.loads(line)
                except Exception:
                    continue
                strs = []
                texts_from(obj, strs)
                for s in strs:
                    # a real HANDOFF chunk: has line-numbered content and the section headers
                    if len(s) < 5000 or "\n" not in s:
                        continue
                    if not (MARK in s or "2.6" in s or "\u4ea4\u63a5\u6587\u6863" in s):
                        continue
                    chunks.append((len(s), s, os.path.basename(dirpath)))
            print("%s: %d matching lines, best so far %d chars" % (os.path.basename(dirpath), hits, len(best)))
    chunks.sort(key=lambda c: -c[0])
    if chunks:
        print("top chunks:")
        for n, s, w in chunks[:12]:
            # what sections does it cover?
            secs = [ln for ln in s.split("\n") if ln.strip().startswith(("3.", "4.", "5.", "6.", "7.", "\u00a7"))][:0]
            lo = None
            hi = None
            for ln in s.split("\n"):
                i = ln.find(": ")
                if i > 0 and ln[:i].strip().isdigit():
                    v = int(ln[:i].strip())
                    lo = v if lo is None else min(lo, v)
                    hi = v if hi is None else max(hi, v)
            print("   %6d chars  lines %s..%s  from %s" % (n, lo, hi, w))
    if not chunks:
        print("nothing recovered")
        return
    best = chunks[0][1]
    open(out_path, "w", encoding="utf-8", newline="").write(best)
    print("wrote %d chars to %s" % (len(best), out_path))


if __name__ == "__main__":
    main()

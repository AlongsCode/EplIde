# -*- coding: utf-8 -*-
"""Merge every HANDOFF.md chunk found in the DSH session transcripts back into one file.

The transcripts hold several `read HANDOFF.md` results (different line ranges), so the union of
their numbered lines reconstructs the file. Where two chunks disagree on a line, the longer text
wins (a truncated read would be shorter).

usage: python tools/recover_handoff_merge.py [outfile]
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


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else r"D:\eclone\shots\handoff_merged.md"
    lines = {}          # lineno -> text
    cand = []
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
            for ln in raw.decode("utf-8", "replace").split("\n"):
                if "HANDOFF" not in ln:
                    continue
                try:
                    obj = json.loads(ln)
                except Exception:
                    continue
                strs = []
                texts_from(obj, strs)
                for s in strs:
                    if len(s) < 400:
                        continue
                    # ⚠️ 只认 `<name>...HANDOFF.md...</name>` 那种"读文件"的结果 ——
                    #    别的工具输出（bash 结果等）也带行号，不加这个过滤会把别的东西混进来。
                    head = "\n".join(s.split("\n")[:6])
                    if "HANDOFF" not in head and "HANDOFF" not in s[:200]:
                        continue
                    cand.append((len(s), s, os.path.basename(dirpath)))
            print("%-42s %d candidate chunks" % (os.path.basename(dirpath), len(cand)))
    print("candidates: %d" % len(cand))
    # 每个候选块都是"某个区间"的 read 结果，所以直接按行号并集（同号取更长的那份）
    for n, s, w in cand:
        for one in s.split("\n"):
            m = LINE.match(one.rstrip("\r"))
            if not m:
                continue
            no = int(m.group(1))
            t = m.group(2)
            old = lines.get(no)
            if old is None or len(t) > len(old):
                lines[no] = t
    if not lines:
        print("nothing recovered")
        return
    hi = max(lines)
    out = []
    for n in range(1, hi + 1):
        out.append(lines.get(n, ""))
    open(out_path, "w", encoding="utf-8", newline="\n").write("\n".join(out))
    print("recovered %d/%d lines (%d missing) -> %s"
          % (len(lines), hi, hi - len(lines), out_path))
    missing = [n for n in range(1, hi + 1) if n not in lines]
    if missing:
        print("missing ranges: %s" % missing[:60])


if __name__ == "__main__":
    main()

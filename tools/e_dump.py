# -*- coding: utf-8 -*-
"""Dump the window/component tree of an 易语言 .e file (CNWTEPRG format) as strings, so the
DESIGNER can be compared against the project's own window sample instead of a guess.

The .e file is a nested binary record stream; a full parser is a big job, so this prints every
readable GBK string with its offset plus easy byte patterns (sizes look like small ints near the
component names). Enough to see what a form's 左边/顶边/宽度/高度 are set to.

usage: python tools/e_dump.py "D:\\eclone\\窗口例程.e"
"""
import io
import re
import sys


def main():
    path = sys.argv[1]
    b = open(path, "rb").read()
    print("%s: %d bytes" % (path, len(b)))
    print("magic:", b[:8])
    print()
    print("offset  len  text")
    off = 0
    for m in re.finditer(rb"[\x20-\x7e\x81-\xfe]{2,}", b):
        s = m.group(0)
        try:
            t = s.decode("gbk")
        except Exception:
            continue
        if not any(c.isalnum() or "\u4e00" <= c <= "\u9fff" for c in t):
            continue
        print("%6d %4d  %s" % (m.start(), len(s), t.replace("\r", "\\r").replace("\n", "\\n")))
    # byte-level: show the head as dwords to spot small integers (geometry)
    print()
    print("first 64 bytes as bytes:  " + " ".join("%02X" % c for c in b[:64]))
    print("first 32 bytes as uint16: " + " ".join("%d" % v for v in
          (b[i] | (b[i + 1] << 8) for i in range(0, 64, 2))))
    # look for plausible geometry pairs (380x250 etc.)
    for name, pat in (("380", b"\x7c\x01"), ("250", b"\xfa\x00"), ("292", b"\x24\x01"),
                      ("187", b"\xbb\x00")):
        print("pattern %s (%s) at: %s" % (name, pat.hex(), [m.start() for m in re.finditer(re.escape(pat), b)][:6]))


if __name__ == "__main__":
    main()

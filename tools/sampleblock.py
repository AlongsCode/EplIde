# -*- coding: utf-8 -*-
"""Locate the `g_szSampleCode` block in src\\EClone.cpp -- robustly.

Three traps this has to survive (all three bit during round 22):
  1. `g_szSampleCode` also appears **inside the `EP_CLASS` comment line**, so a plain
     `d.index("static const TCHAR* g_szSampleCode")` can match there instead of at the real
     declaration.
  2. Finding the block end by "the first ';\\n' after the marker" is wrong: one of the sample
     lines ends with `"";` **inside a C string literal** (a DLL command declaration).
  3. The author's own comments contain the word `semicolon` when explaining trap 2, so *that*
     word must not be used as an anchor.

What works: the marker must be a real C statement, i.e. its line ends with `=` (after trimming).
The block ends at the first ';' that the C tokenizer sees at top level (skipping comments and
string/char literals), plus the end of that line (a trailing `// note` may follow).

usage: python tools\\sampleblock.py
"""
import io

PATH = r"C:\Users\Along\Desktop\eclone\src\EClone.cpp"
MARKER_STMT = "static const TCHAR* g_szSampleCode"


def bounds(path=PATH):
    """Return (i, j): i = start of the `g_szSampleCode =` line, j = end of the line with the ';'."""
    d = io.open(path, encoding="utf-8").read()
    n = len(d)

    # --- 1) find the REAL statement header.  The marker also occurs INSIDE the `EP_CLASS`
    # comment, so require: the line is exactly the declaration (only leading spaces before the
    # marker) AND it ends with '='.  A mid-line occurrence (in a comment) fails both.
    i = None
    p = 0
    while True:
        k = d.find(MARKER_STMT, p)
        if k < 0:
            break
        ls = d.rindex("\n", 0, k) + 1
        line_end = d.find("\n", k)
        if line_end < 0:
            line_end = n
        prefix = d[ls:k]
        if prefix.strip() == "" and d[k:line_end].rstrip().endswith("="):
            i = ls
            break
        p = k + 1
    if i is None:
        raise ValueError("no real `%s =` statement found" % MARKER_STMT)

    # --- 2) tokenize from the marker to the first top-level ';'
    p = i
    while p < n:
        c = d[p]
        if c == "/" and p + 1 < n and d[p + 1] == "/":
            while p < n and d[p] != "\n":
                p += 1
            continue
        if c == "/" and p + 1 < n and d[p + 1] == "*":
            q = d.find("*/", p + 2)
            p = n if q < 0 else q + 2
            continue
        if c == '"' or c == "'":
            quote = c
            p += 1
            while p < n:
                if d[p] == "\\":
                    p += 2
                    continue
                if d[p] == quote:
                    p += 1
                    break
                p += 1
            continue
        if c == ";":
            line_end = d.find("\n", p)
            return i, (n if line_end < 0 else line_end)
        p += 1
    raise ValueError("no top-level ';' after the marker")


if __name__ == "__main__":
    i, j = bounds()
    d = io.open(PATH, encoding="utf-8").read()
    print("block from %d to %d (%d chars, %d literals)"
          % (i, j, j - i, d[i:j].count("_T(")))
    print("head: %r" % d[i:i + 48])
    print("tail: %r" % d[j - 48:j])

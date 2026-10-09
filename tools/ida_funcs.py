# -*- coding: utf-8 -*-
"""Index the e.exe idb: page through list_functions and save every name that matches a pattern
(or all of them) so the editor's WndProc / selection code can be located.

usage: python tools/ida_funcs.py <outfile> [pattern ...]
       python tools/ida_funcs.py shots/ida_func_names.txt WndProc Select Sel Paint
"""
import json
import sys
import urllib.request


def call(method, params):
    body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": method,
                       "params": params}).encode()
    req = urllib.request.Request("http://127.0.0.1:13337/mcp", data=body,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=1800) as r:
        d = json.loads(r.read().decode("utf-8", "replace"))
    if "error" in d:
        raise SystemExit("error: %s" % d["error"])
    return d["result"]


def main():
    out = sys.argv[1]
    pats = [p.lower() for p in sys.argv[2:]]
    data = []
    off = 0
    while True:
        page = call("list_functions", [off, 500])
        items = page.get("data") or []
        data.extend(items)
        off = page.get("next_offset")
        if off is None or not items:
            break
    print("total functions: %d" % len(data))
    with open(out, "w", encoding="utf-8") as f:
        for it in data:
            nm = it.get("name") or ""
            if not pats or any(p in nm.lower() for p in pats):
                f.write("%s\t%s\n" % (it.get("address"), nm))
    hits = 0
    for it in data:
        nm = it.get("name") or ""
        if not pats or any(p in nm.lower() for p in pats):
            hits += 1
    print("hits: %d -> %s" % (hits, out))


if __name__ == "__main__":
    main()

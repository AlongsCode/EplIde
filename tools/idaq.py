# -*- coding: utf-8 -*-
"""Small IDA-MCP query helpers for the e.exe idb (via tools/idamcp.py's HTTP endpoint).

usage:
  python tools/idaq.py import <name-substr>          -> address of matching imports
  python tools/idaq.py xref <address>                -> who references that address
  python tools/idaq.py func <address>                -> function containing it
  python tools/idaq.py decomp <address> [outfile]
  python tools/idaq.py strings <substr> [count]
"""
import json
import sys
import urllib.request

PORT = 13337


def call(method, params=None):
    body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": method,
                       "params": [] if params is None else params}).encode()
    req = urllib.request.Request("http://127.0.0.1:%d/mcp" % PORT, data=body,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=1800) as r:
        return json.loads(r.read().decode("utf-8", "replace"))


def result(method, params=None):
    d = call(method, params)
    if "error" in d:
        raise SystemExit("error: %s" % d["error"])
    return d["result"]


if __name__ == "__main__":
    what = sys.argv[1]
    if what == "import":
        sub = sys.argv[2].lower()
        data = result("list_imports", [0, 400])["data"]
        for it in data:
            if sub in it["imported_name"].lower():
                print("%s %s %s" % (it["address"], it["module"], it["imported_name"]))
    elif what == "xref":
        print(json.dumps(result("get_xrefs_to", [sys.argv[2]]), ensure_ascii=False, indent=1))
    elif what == "func":
        print(json.dumps(result("get_function_by_address", [sys.argv[2]]), ensure_ascii=False, indent=1))
    elif what == "decomp":
        txt = result("decompile_function", [sys.argv[2]])
        out = txt if isinstance(txt, str) else json.dumps(txt, ensure_ascii=False)
        if len(sys.argv) > 3:
            import io
            io.open(sys.argv[3], "w", encoding="utf-8").write(out)
            print("-> %s (%d bytes)" % (sys.argv[3], len(out)))
        else:
            print(out)
    elif what == "strings":
        print(json.dumps(result("list_strings_filter", [0, int(sys.argv[3]) if len(sys.argv) > 3 else 50,
                                                        sys.argv[2]]), ensure_ascii=False, indent=1))
    else:
        print(__doc__)

"""Call the running IDA Pro MCP plugin (ida-pro-mcp JSON-RPC on port 13337).

Usage:  python tools/idamcp.py <method> [python-literal-params]
        python tools/idamcp.py get_metadata
        python tools/idamcp.py list_functions_filter "[0, 50, 'main']"
        python tools/idamcp.py decompile_function "['0x100ca6b0']" > out.txt

Params are parsed with ast.literal_eval (NOT json) so PowerShell quote
mangling of double quotes does not matter -- use single-quoted strings.

The plugin (mcp-plugin.py) dispatches method name -> function name and wants
`params` as a JSON list whose length equals the function's parameter count.
"""
import ast
import json
import sys
import urllib.request

PORT = 13337


def call(method, params=None):
    body = json.dumps({
        "jsonrpc": "2.0",
        "id": 1,
        "method": method,
        "params": [] if params is None else params,
    }).encode("utf-8")
    req = urllib.request.Request(
        "http://127.0.0.1:%d/mcp" % PORT,
        data=body,
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=1800) as r:
        return json.loads(r.read().decode("utf-8", "replace"))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    arg = ast.literal_eval(sys.argv[2]) if len(sys.argv) > 2 else []
    resp = call(sys.argv[1], arg)
    print(json.dumps(resp, ensure_ascii=False, indent=1))

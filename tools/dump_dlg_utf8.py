# Dump one RT_DIALOG template from shots\e_dialogs.txt as UTF-8 (console is GBK).
# usage: python tools\dump_dlg_utf8.py <id> [outfile]
import io, sys

src = io.open(r"D:\eclone\shots\e_dialogs.txt", encoding="utf-8").read()
did = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else None

lines = src.splitlines()
start = None
for i, ln in enumerate(lines):
    if ln.startswith("DIALOGEX id=%s " % did) or ln.startswith("DIALOG id=%s " % did) or ln == "DIALOGEX id=%s" % did:
        start = i
        break
if start is None:
    print("not found: id=%s" % did)
    sys.exit(1)
end = start + 1
while end < len(lines) and not (lines[end].startswith("DIALOGEX id=") or lines[end].startswith("DIALOG id=")):
    end += 1
txt = "\n".join(lines[start:end])
if out:
    io.open(out, "w", encoding="utf-8").write(txt)
    print("wrote %s (%d bytes, %d lines)" % (out, len(txt), end - start))
else:
    print(txt)

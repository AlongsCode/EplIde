# -*- coding: utf-8 -*-
"""把 menus_main.txt（从 e.exe 的 RT_MENU 还原的菜单树）转换成 MFC 的 .rc MENU 资源。

用法: python gen_menu_rc.py <menus_main.txt> <out.rc>

输入行样例（缩进=层级）:
    F.程序  >
      &N.新建	Ctrl+O   #57600
      ----
    最近文件   #57616
"""
import re
import sys

SEP_RE = re.compile(r"^-+\s*$|^----$")


def parse(lines):
    items = []
    for raw in lines:
        if not raw.strip():
            continue
        if raw.startswith("=====") or raw.startswith("RT_MENU"):
            continue
        indent = len(raw) - len(raw.lstrip(" "))
        depth = indent // 2
        body = raw.strip()
        m = re.match(r"^(.*?)\s*#(\d+)$", body)
        if m:
            text = m.group(1).strip()
            rid = int(m.group(2))
            if not text and rid == 0:
                items.append((depth, "SEP", None, 0))
            else:
                items.append((depth, "CMD", text, rid))
            continue
        if body.endswith(">"):
            text = body[:-1].strip()
            items.append((depth, "POPUP", text, 0))
            continue
        if SEP_RE.match(body):
            items.append((depth, "SEP", None, 0))
            continue
        items.append((depth, "CMD", body, 0))
    return items


def emit(items, out):
    base = min(d for d, _, _, _ in items) if items else 0
    out.append("// Generated from e.exe RT_MENU (id=127) - do not edit by hand")
    out.append("IDR_MAINFRAME MENU")
    out.append("BEGIN")
    depth = base
    for d, kind, text, rid in items:
        while depth > d:
            out.append("    " * depth + "END")
            depth -= 1
        pad = "    " * (d + 1)
        if kind == "POPUP":
            out.append('%sPOPUP "%s"' % (pad, text))
            out.append("%sBEGIN" % pad)
            depth = d + 1
        elif kind == "SEP":
            out.append("%sMENUITEM SEPARATOR" % pad)
        else:
            out.append('%sMENUITEM "%s", %d' % (pad, text, rid))
    while depth > base:
        out.append("    " * depth + "END")
        depth -= 1
    out.append("END")
    out.append("")


def read_text(path):
    raw = open(path, "rb").read()
    if raw[:2] in (b"\xff\xfe", b"\xfe\xff"):
        return raw.decode("utf-16")
    if raw.count(b"\x00") > len(raw) // 4:
        try:
            return raw.decode("utf-16-le")
        except UnicodeDecodeError:
            pass
    for enc in ("utf-8", "gbk"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("gbk", "replace")


if __name__ == "__main__":
    src, dst = sys.argv[1], sys.argv[2]
    lines = read_text(src).split("\n")
    items = parse(lines)
    buf = []
    emit(items, buf)
    txt = "\n".join(buf)
    head = read_text(sys.argv[3]) if len(sys.argv) > 3 else ""
    open(dst, "w", encoding="utf-8").write(head + txt)
    n_cmd = sum(1 for i in items if i[1] == "CMD")
    n_pop = sum(1 for i in items if i[1] == "POPUP")
    n_sep = sum(1 for i in items if i[1] == "SEP")
    print("items=%d popup=%d cmd=%d sep=%d -> %s" % (len(items), n_pop, n_cmd, n_sep, dst))

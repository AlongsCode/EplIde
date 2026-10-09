# -*- coding: utf-8 -*-
"""导出 e.exe 的 RT_DIALOG 模板（DLGTEMPLATE / DLGTEMPLATEEX）。

用法: python dump_dialogs.py <exe> [out.txt] [filter]

模板里所有字符串字段一律是 Unicode（含 DLGITEMTEMPLATE 的类名/标题），
只有「资源串」形式是 0xFFFF + 序号。序号到系统类名的映射见 SYS_CLASS。
"""
import struct
import sys

import pefile

SYS_CLASS = {0x80: "Button", 0x81: "Edit", 0x82: "Static", 0x83: "ListBox",
             0x84: "ScrollBar", 0x85: "ComboBox"}


def read_str(data, off, strings):
    """读一个模板字符串字段。返回 (text, new_off)。"""
    if off + 2 > len(data):
        return "", off
    v = struct.unpack_from("<H", data, off)[0]
    if v == 0x0000:
        return "", off + 2
    if v == 0xFFFF:
        idx = struct.unpack_from("<H", data, off + 2)[0]
        if idx in SYS_CLASS:
            return SYS_CLASS[idx], off + 4
        return strings.get(idx, "<ord%#x>" % idx), off + 4
    out = b""
    p = off
    while True:
        ch = data[p:p + 2]
        p += 2
        if ch == b"\x00\x00" or len(ch) < 2:
            break
        out += ch
    return out.decode("utf-16-le", "replace"), p


DLG_STYLES = [
    (0x80000000, "WS_POPUP"), (0x40000000, "WS_CHILD"),
    (0x10000000, "WS_VISIBLE"), (0x08000000, "WS_DISABLED"),
    (0x00C00000, "WS_CAPTION"), (0x00080000, "WS_SYSMENU"),
    (0x00040000, "WS_THICKFRAME"), (0x00020000, "WS_MINIMIZEBOX"),
    (0x00010000, "WS_MAXIMIZEBOX"), (0x00000080, "DS_MODALFRAME"),
    (0x00000040, "DS_SETFONT"), (0x00000001, "DS_ABSALIGN"),
    (0x00000400, "DS_CONTROL"), (0x00000800, "DS_CENTER"),
]
CTRL_STYLES = [
    (0x40000000, "WS_CHILD"), (0x10000000, "WS_VISIBLE"),
    (0x08000000, "WS_DISABLED"), (0x04000000, "WS_CLIPSIBLINGS"),
    (0x02000000, "WS_CLIPCHILDREN"), (0x00800000, "WS_BORDER"),
    (0x00400000, "WS_DLGFRAME"), (0x00200000, "WS_VSCROLL"),
    (0x00100000, "WS_HSCROLL"), (0x00010000, "WS_MAXIMIZEBOX"),
    (0x00020000, "WS_MINIMIZEBOX"), (0x00040000, "WS_THICKFRAME"),
    (0x00000001, "WS_TABSTOP"), (0x00000002, "WS_GROUP"),
    (0x80000000, "WS_POPUP"),
]
EX_STYLES = [
    (0x00000001, "WS_EX_DLGMODALFRAME"), (0x00000004, "WS_EX_NOPARENTNOTIFY"),
    (0x00000100, "WS_EX_WINDOWEDGE"), (0x00000200, "WS_EX_CLIENTEDGE"),
    (0x00010000, "WS_EX_CONTROLPARENT"), (0x00020000, "WS_EX_STATICEDGE"),
    (0x00040000, "WS_EX_APPWINDOW"),
]
TREE_STYLES = [
    (0x0001, "TVS_HASBUTTONS"), (0x0002, "TVS_HASLINES"),
    (0x0004, "TVS_LINESATROOT"), (0x0008, "TVS_EDITLABELS"),
    (0x0010, "TVS_DISABLEDRAGDROP"), (0x0020, "TVS_SHOWSELALWAYS"),
    (0x0040, "TVS_RTLREADING"), (0x0080, "TVS_NOTOOLTIPS"),
    (0x0100, "TVS_CHECKBOXES"), (0x0200, "TVS_TRACKSELECT"),
    (0x0400, "TVS_SINGLEEXPAND"), (0x0800, "TVS_INFOTIP"),
    (0x1000, "TVS_FULLROWSELECT"), (0x2000, "TVS_NOSCROLL"),
    (0x4000, "TVS_NONEVENHEIGHT"), (0x8000, "TVS_NOHSCROLL"),
]
LIST_STYLES = [
    (0x0001, "LVS_REPORT"), (0x0002, "LVS_ICON"), (0x0004, "LVS_SMALLICON"),
    (0x0008, "LVS_LIST"), (0x0010, "LVS_SINGLESEL"), (0x0020, "LVS_SHOWSELALWAYS"),
    (0x0100, "LVS_NOCOLUMNHEADER"), (0x0200, "LVS_NOSORTHEADER"),
    (0x2000, "LVS_OWNERDRAWFIXED"),
]
TAB_STYLES = [
    (0x0002, "TCS_BOTTOM"), (0x0004, "TCS_RIGHT"), (0x0008, "TCS_MULTISELECT"),
    (0x0010, "TCS_FLATBUTTONS"), (0x0020, "TCS_FORCEICONLEFT"),
    (0x0040, "TCS_FORCELABELLEFT"), (0x0100, "TCS_HOTTRACK"),
    (0x0200, "TCS_VERTICAL"), (0x0800, "TCS_BUTTONS"),
    (0x1000, "TCS_SINGLELINE"), (0x2000, "TCS_MULTILINE"),
    (0x4000, "TCS_RIGHTJUSTIFY"), (0x8000, "TCS_FIXEDWIDTH"),
]
COMBO_STYLES = [
    (0x0001, "CBS_SIMPLE"), (0x0002, "CBS_DROPDOWN"),
    (0x0003, "CBS_DROPDOWNLIST"), (0x0010, "CBS_OWNERDRAWFIXED"),
    (0x0040, "CBS_AUTOHSCROLL"), (0x0200, "CBS_SORT"),
    (0x0400, "CBS_HASSTRINGS"), (0x0800, "CBS_NOINTEGRALHEIGHT"),
    (0x2000, "CBS_DISABLENOSCROLL"),
]
BUTTON_STYLES = {0: "BS_PUSHBUTTON", 1: "BS_DEFPUSHBUTTON", 2: "BS_CHECKBOX",
                 3: "BS_AUTOCHECKBOX", 4: "BS_RADIOBUTTON", 5: "BS_3STATE",
                 6: "BS_AUTO3STATE", 7: "BS_GROUPBOX", 8: "BS_USERBUTTON",
                 9: "BS_AUTORADIOBUTTON", 10: "BS_PUSHBOX", 11: "BS_OWNERDRAW"}


def bits(v, table):
    return [n for m, n in table if v & m]


def extra_style(cls, style):
    if cls == "SysTreeView32":
        return bits(style, TREE_STYLES)
    if cls == "SysListView32":
        return bits(style, LIST_STYLES)
    if cls == "SysTabControl32":
        return bits(style, TAB_STYLES)
    if cls == "ComboBox":
        return bits(style, COMBO_STYLES)
    if cls == "Button":
        return [BUTTON_STYLES.get(style & 0xF, hex(style & 0xF))]
    return []


def parse_dialog(data, strings, idx):
    ex = (struct.unpack_from("<H", data, 0)[0] == 1 and
          struct.unpack_from("<H", data, 2)[0] == 0xFFFF)
    if ex:
        exstyle, style = struct.unpack_from("<II", data, 8)
        cdit, x, y, cx, cy = struct.unpack_from("<Hhhhh", data, 16)
        off = 26
    else:
        style, exstyle, cdit, x, y, cx, cy = struct.unpack_from("<IIHhhhh", data, 0)
        off = 18
    menu, off = read_str(data, off, strings)
    cls, off = read_str(data, off, strings)
    title, off = read_str(data, off, strings)
    face = ""
    if style & 0x00000040:                       # DS_SETFONT
        pointsize, weight = struct.unpack_from("<HH", data, off)
        italic, charset = struct.unpack_from("<BB", data, off + 4)
        off += 6
        face, off = read_str(data, off, strings)
    lines = ["DIALOG%s id=%s  %dx%d at %d,%d  font=%s" %
             ("EX" if ex else "", idx, cx, cy, x, y, face)]
    lines.append("  style=0x%08X [%s]" % (style, ", ".join(bits(style, DLG_STYLES))))
    lines.append("  exstyle=0x%08X [%s]" % (exstyle, ", ".join(bits(exstyle, EX_STYLES))))
    lines.append("  class=%r  title=%r  menu=%r  items=%d" % (cls, title, menu, cdit))
    for i in range(cdit):
        while off % 4:
            off += 1
        if ex:                                   # DLGITEMTEMPLATEEX
            helpid, cex, cstyle = struct.unpack_from("<III", data, off)
            ix, iy, icx, icy = struct.unpack_from("<hhhh", data, off + 12)
            cid = struct.unpack_from("<I", data, off + 20)[0]
            off += 24
        else:
            cstyle, cex, ix, iy, icx, icy, cid = struct.unpack_from("<IIhhhhH", data, off)
            off += 18
        ccls, off = read_str(data, off, strings)
        ctext, off = read_str(data, off, strings)
        extra = struct.unpack_from("<H", data, off)[0]
        off += 2 + extra
        lines.append("  [%02d] id=%-5d %-16s %4d,%-4d %4dx%-4d  style=0x%08X ex=0x%08X"
                     % (i, cid, ccls, ix, iy, icx, icy, cstyle, cex))
        lines.append("        text=%r" % ctext)
        lines.append("        style=[%s]  %s" % (", ".join(bits(cstyle, CTRL_STYLES)),
                                                 " ".join(extra_style(ccls, cstyle))))
    return "\n".join(lines)


def load_strings(pe, img):
    out = {}
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if t.struct.Id != 6:
            continue
        for rid in t.directory.entries:
            base = rid.struct.Id * 16
            for lang in rid.directory.entries:
                off, size = lang.data.struct.OffsetToData, lang.data.struct.Size
                blob = img[off:off + size]
                p = 0
                for k in range(16):
                    if p + 2 > len(blob):
                        break
                    n = struct.unpack_from("<H", blob, p)[0]
                    p += 2
                    if n:
                        out[base + k] = blob[p:p + n * 2].decode("utf-16-le", "replace")
                    p += n * 2
    return out


def main(exe, outfile=None, want=None):
    pe = pefile.PE(exe, fast_load=True)
    pe.parse_data_directories(directories=[
        pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    img = pe.get_memory_mapped_image()
    strings = load_strings(pe, img)
    chunks = []
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if t.struct.Id != 5:
            continue
        for rid in t.directory.entries:
            for lang in rid.directory.entries:
                off, size = lang.data.struct.OffsetToData, lang.data.struct.Size
                try:
                    txt = parse_dialog(img[off:off + size], strings, rid.struct.Id)
                except Exception as e:
                    txt = "DIALOG id=%s  <parse failed: %r>" % (rid.struct.Id, e)
                if want and want.lower() not in txt.lower():
                    continue
                chunks.append(txt)
    body = "\n\n".join(chunks)
    if outfile:
        open(outfile, "w", encoding="utf-8").write(body)
        print("-> %s (%d dialogs)" % (outfile, len(chunks)))
    else:
        print(body)


if __name__ == "__main__":
    main(sys.argv[1],
         sys.argv[2] if len(sys.argv) > 2 else None,
         sys.argv[3] if len(sys.argv) > 3 else None)

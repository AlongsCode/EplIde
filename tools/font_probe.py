# -*- coding: utf-8 -*-
"""GDI 字体探针：找出哪种 LOGFONT 参数能复现原版截图里标点的点阵。

原版实测（shots\\ref_dll_page.png，12px 正文）：
    ':'  -> 两个 2x2 的方点
    '“'  -> 笔画比本工程粗
本工程（宋体 / GB2312_CHARSET / FIXED_PITCH|FF_MODERN / DEFAULT_QUALITY / -12）：
    ':'  -> 两个 1x1 的点

用法: python font_probe.py [字符...]
"""
import ctypes
import sys

u32 = ctypes.windll.user32
g32 = ctypes.windll.gdi32

LOGPITCH = {"DEFAULT": 0, "FIXED": 1, "VARIABLE": 2}
FF = {"DONTCARE": 0, "ROMAN": 0x10, "SWISS": 0x20, "MODERN": 0x30}
QUALITY = {"DEFAULT": 0, "DRAFT": 1, "PROOF": 2, "NONANTIALIASED": 3,
           "ANTIALIASED": 4, "CLEARTYPE": 5}
CHARSET = {"ANSI": 0, "GB2312": 134, "DEFAULT": 1, "OEM": 255}


class LOGFONTA(ctypes.Structure):
    _fields_ = [("lfHeight", ctypes.c_long), ("lfWidth", ctypes.c_long),
                ("lfEscapement", ctypes.c_long), ("lfOrientation", ctypes.c_long),
                ("lfWeight", ctypes.c_long), ("lfItalic", ctypes.c_byte),
                ("lfUnderline", ctypes.c_byte), ("lfStrikeOut", ctypes.c_byte),
                ("lfCharSet", ctypes.c_byte), ("lfOutPrecision", ctypes.c_byte),
                ("lfClipPrecision", ctypes.c_byte), ("lfQuality", ctypes.c_byte),
                ("lfPitchAndFamily", ctypes.c_byte), ("lfFaceName", ctypes.c_char * 32)]


def render(face, height, weight, charset, pitch, family, quality, text=":", w=12, h=16):
    hdc = g32.CreateCompatibleDC(0)
    hbmp = g32.CreateCompatibleBitmap(g32.CreateCompatibleDC(0), w, h)
    g32.SelectObject(hdc, hbmp)
    g32.PatBlt(hdc, 0, 0, w, h, 0x00FF0062)          # WHITENESS
    lf = LOGFONTA()
    lf.lfHeight = -height
    lf.lfWeight = weight
    lf.lfCharSet = charset
    lf.lfQuality = quality
    lf.lfPitchAndFamily = pitch | family
    lf.lfFaceName = face.encode("gbk")
    f = g32.CreateFontIndirectA(ctypes.byref(lf))
    old = g32.SelectObject(hdc, f)
    g32.SetBkMode(hdc, 1)                            # TRANSPARENT
    g32.SetTextColor(hdc, 0x000000)
    g32.TextOutA(hdc, 2, 1, text.encode("gbk"), len(text.encode("gbk")))
    g32.SelectObject(hdc, old)
    g32.DeleteObject(f)
    art = []
    for y in range(h):
        row = "".join("#" if g32.GetPixel(hdc, x, y) == 0 else "." for x in range(w))
        art.append(row)
    g32.DeleteObject(hbmp)
    g32.DeleteDC(hdc)
    return art


def show(tag, art):
    rows = [r for r in art if "#" in r]
    print("  %-52s %s" % (tag, " | ".join(rows)))


def main():
    texts = sys.argv[1:] or [":", "\u201c"]
    for ch in texts:
        print("=== 字符 %r ===" % ch)
        cases = [
            ("宋体 -12 FW400 GB2312 FIXED|MODERN DEFAULT", "宋体", 12, 400, 134, 1, 0x30, 0),
            ("宋体 -12 FW400 GB2312 DEFAULT(0)|MODERN", "宋体", 12, 400, 134, 0, 0x30, 0),
            ("宋体 -12 FW400 GB2312 DEFAULT|DONTCARE", "宋体", 12, 400, 134, 0, 0, 0),
            ("宋体 -12 FW400 ANSI DEFAULT|DONTCARE", "宋体", 12, 400, 0, 0, 0, 0),
            ("宋体 -12 FW400 GB2312 DEFAULT|MODERN PROOF", "宋体", 12, 400, 134, 0, 0x30, 2),
            ("宋体 -12 FW400 GB2312 DEFAULT|MODERN NONANTI", "宋体", 12, 400, 134, 0, 0x30, 3),
            ("宋体 -12 FW500 GB2312 FIXED|MODERN DEFAULT", "宋体", 12, 500, 134, 1, 0x30, 0),
            ("宋体 -13 FW400 GB2312 FIXED|MODERN DEFAULT", "宋体", 13, 400, 134, 1, 0x30, 0),
            ("新宋体 -12 FW400 GB2312 FIXED|MODERN DEFAULT", "新宋体", 12, 400, 134, 1, 0x30, 0),
            ("新宋体 -12 FW400 GB2312 DEFAULT|DONTCARE", "新宋体", 12, 400, 134, 0, 0, 0),
            ("SimSun -12 FW400 GB2312 DEFAULT|DONTCARE", "SimSun", 12, 400, 134, 0, 0, 0),
            ("宋体 -12 FW400 DEFAULT CHARSET DEFAULT|DONTCARE", "宋体", 12, 400, 1, 0, 0, 0),
        ]
        for tag, face, h, wt, cs, p, fam, q in cases:
            try:
                show(tag, render(face, h, wt, cs, p, fam, q, ch))
            except Exception as e:
                print("  %-52s ERROR %s" % (tag, e))
        print()


if __name__ == "__main__":
    main()

# -*- coding: utf-8 -*-
"""Repair the sample region of src\\EClone.cpp using ONLY unique explicit anchors.

Anchors:
    A = "#define EP_RES_SOUND 8"            -> keep this line, replace everything AFTER it
    B = "static const TCHAR* g_szSampleDll" -> stop right before it (and drop its comment block)

Nothing in the replacement text contains either anchor, and `g_szSampleDll` never appears inside
the sample block, so this is deterministic (unlike a "find the marker" heuristic -- the marker
string also occurs inside the region we are replacing).

usage: python tools\\_repair_sample_region.py
"""
import io

P = r"D:\eclone\src\EClone.cpp"
BAK = r"D:\eclone\shots\zoom\sample_backup.txt"

REGION = r'''#define EP_RES_IMAGE 9
#define EP_MODREF    10
#define EP_EXTFILE   11
#define EP_GLOBAL    12     // 全局变量（圆柱形）；子项同图标
#define EP_TYPE      13     // 自定义数据类型（连着的方块）
#define EP_CLASS     13     // 类模块 —— 原版实测与自定义数据类型**同一个图标**（实测 img=0）
#define EP_DLL       14

// VC6 的 commctrl.h 没有定义这个「不显示图标」的宏
#ifndef I_IMAGENONE
#define I_IMAGENONE (-1)
#endif

// 采样源码：默认就是下面这一份（程序集/子程序/参数/变量/表页都覆盖）。
static const TCHAR* g_szSampleCode =
    _T(".版本 2\r\n")
    _T("\r\n")
    // 「文件夹 + 普通程序集 + 类模块」这三样是照原版实测补的（shots\live_jy\prog_tree.txt，
    //  精易模块.e 的树）：文件夹(img=16) 下面挂 程序集(img=3) 和 类(img=0)
    _T(".文件夹 集_演示\r\n")
    _T(".程序集 集_演示\r\n")
    _T(".子程序 取演示文本, 文本型\r\n")
    _T("    .参数 序号, 整数型\r\n")
    _T("    返回 (“演示” + 到文本 (序号))\r\n")
    _T("\r\n")
    _T(".类 类_示例\r\n")
    _T("\r\n")
    _T(".文件夹\r\n")              // 不带名字 = 分组结束（见 EcBuildProgTree 的注释）
    _T("\r\n")
    _T(".程序集 启动窗口程序集\r\n")
    _T(".程序集变量 记录集, 整数型\r\n")
    // 多行注释的绿框采样：形状照 jy.e 实测（空 `'` 行 / 有字的 `'` 行 交替，首末都是空行）
    _T("'\r\n")
    _T("' 注释块演示：第一行（空行在上下）\r\n")
    _T("'\r\n")
    _T("' 注释块演示：第二行\r\n")
    _T("'\r\n")
    _T("\r\n")
    _T(".子程序 _启动窗口_创建完毕\r\n")
    _T("\r\n")
    _T(".子程序 _图形按钮1_被单击\r\n")
    _T("    .局部变量 i, 整数型\r\n")
    _T("    信息框（\"易语言中国人之骄傲！\", 0 ）\r\n")
    _T("    ' 下面是循环：注释里的 如果真 和 \"字符串\" 都不该被当成关键字染色\r\n")
    _T("    .计次循环首 (10, i)\r\n")
    _T("        如果真 (i ＞ 5)\r\n")
    _T("            信息框（\"这里的关键字\" + 到文本 (i), 0 ）\r\n")
    _T("            跳出循环 ()\r\n")
    _T("        如果真结束\r\n")
    _T("    .计次循环尾 ()\r\n")
    _T("    返回 ()\r\n")
    _T("\r\n")
    _T(".子程序 加法, 整数型, , 公开\r\n")
    _T("    .参数 a, 整数型\r\n")
    _T("    .参数 b, 整数型\r\n")
    _T("    返回 (a + b)\r\n")
    _T("\r\n")
    _T(".全局变量 全_文本, 文本型\r\n")
    _T(".全局变量 普通填表, 普通填表, 公开\r\n")
    _T(".全局变量 内存, 类_内存, 公开, , 内存操作\r\n")
    _T(".全局变量 POST数据类, 类_POST数据类\r\n")
    _T(".全局变量 POST数据类_字节集, 类_POST数据类_字节集\r\n")
    _T(".全局变量 全_Token, 整数型\r\n")
    _T(".全局变量 _WIN32_WINNT, 整数型, 公开\r\n")
    _T(".全局变量 m_Build, 短整数型, , , 内部版本号\r\n")
    _T(".全局变量 m_CodePage, 整数型, 公开, , 代码页\r\n")
    _T(".全局变量 m_stageSize, 整数型, 公开, , 分段长度\r\n")
    _T(".数据类型 数据类型1\r\n")
    _T("    .成员 字段1, 整数型\r\n")
    _T(".数据类型 数据类型2, 公开, 第二个演示类型\r\n")
    _T("    .成员 字段A, 文本型, , 数组\r\n")
    _T("    .成员 字段B, 字节型, 传址\r\n")
    _T("\r\n")
    _T(".常量 常量_演示, 42, 公开, 演示常量\r\n")
    _T(".常量 常量_文本, \"短文本\"\r\n")
    _T("\r\n")
    _T(".常量 常量_长文本, \"这里是一段很长的文本常量，原版对长文本不在表格里画内容，只画「文本长度」，所以这一行的常量值列会显示成长度提示而不是原文。这段话要超过六十个字符才能触发那条规则，于是继续往后写一些填充用的字，确保长度足够长。\", 公开\r\n")
    _T("\r\n")
    _T(".图片资源 图片_演示, 公开\r\n")
    _T(".声音资源 声音_演示\r\n");

'''

A = "#define EP_RES_SOUND 8"
B = "static const TCHAR* g_szSampleDll"

d = io.open(P, encoding="utf-8").read()
if d.count(A) != 1 or d.count(B) != 1:
    raise SystemExit("anchors not unique: A=%d B=%d" % (d.count(A), d.count(B)))
ai = d.index(A)
ai = d.index("\n", ai) + 1                 # keep the anchor line itself
bi = d.index(B)
while True:
    q = d.rindex("\n", 0, bi - 1) + 1
    if d[q:bi].lstrip().startswith("//"):
        bi = q
    else:
        break

new = d[:ai] + REGION + d[bi:]
io.open(P, "w", encoding="utf-8").write(new)

i = new.index("static const TCHAR* g_szSampleCode")
j = new.index(";\n", i) + 1
io.open(BAK, "w", encoding="utf-8").write(new[i:j])
print("repaired: %d -> %d lines; block %d chars, %d literals"
      % (len(d.splitlines()), len(new.splitlines()), j - i, new[i:j].count("_T(")))

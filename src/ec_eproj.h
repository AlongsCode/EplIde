// ec_eproj.h -- 把「真 .e 二进制工程文件」读成 eclone 的文本源码形式。
//
// 为什么是这个做法：eclone 的工作夹->程序树、以及所有表页（程序集头/子程序头/参数表/
// 变量表/数据类型表/常量表…）**全都由一份文本源码驱动**（见 EClone.cpp 的 EcBuildProgTree
// 与 ec_editor.cpp 的 SetText/Rebuild）。所以打开 .e 不需要新写任何渲染代码，
// 只要把 .e 解析出的东西**生成成同一份文本**，下游全自动。
//
// 格式判据：以 e.exe 为准。读取入口 = e.exe `sub_444C40`（0x444C40），
// 段头 100 字节，字段布局与 eplc2 的 SegmentHeader 一致（已实测）。
// 详细坐标见 D:\EPL\HANDOFF.md。
#ifndef __EC_EPROJ_H
#define __EC_EPROJ_H

#include <afxwin.h>

// 打开一个 .e 二进制工程，生成 eclone 文本源码。
// pszPath : .e 文件路径
// strOut  : 生成的源码（CRLF 分行，GBK 内容）
// 返回 TRUE = 成功
BOOL EcLoadEProject(LPCTSTR pszPath, CString& strOut);

// 供自测：只判文件头是不是 .e
BOOL EcIsEFile(LPCTSTR pszPath);

#endif // __EC_EPROJ_H

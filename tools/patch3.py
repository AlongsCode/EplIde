# -*- coding: utf-8 -*-
"""第 3 轮的源码手术：接真实命令数据 + MDI 子窗口类名对齐。"""
import io
import os
import re
import sys

SRC = r"D:\eclone\src\EClone.cpp"


def main():
    t = io.open(SRC, encoding="utf-8").read()
    orig = t

    # 1) include 生成的数据头
    if "ec_krnln_data.h" not in t:
        t = t.replace('#include "ec_design.h"',
                      '#include "ec_design.h"\n#include "ec_krnln_data.h"', 1)

    # 2) 删掉手写的类别数组（改用生成的 g_szLibCats）
    t = re.sub(r"// 填充各面板内容.*?\nstatic const TCHAR\* g_szCategories\[\] = \{.*?\n\};\n\n",
               "// 面板内容：数据取自逆向结果（命令表见 ec_krnln_data.h）\n\n", t, flags=re.S)

    # 3) 支持库页改用真实命令表
    old_lib = """    CTreeCtrl* pLib = (CTreeCtrl*)m_wndWork.GetDlgItem(IDC_TREE_WORK);
    if (pLib)
    {
        HTREEITEM hLib = pLib->InsertItem(_T("系统核心支持库"), TVI_ROOT, TVI_LAST);
        for (int i = 0; g_szCategories[i]; i++)
            pLib->InsertItem(g_szCategories[i], hLib, TVI_LAST);
        pLib->Expand(hLib, TVE_EXPAND);
    }"""
    new_lib = """    CTreeCtrl* pLib = (CTreeCtrl*)m_wndWork.GetDlgItem(IDC_TREE_WORK);
    if (pLib)
    {
        // 支持库 -> 类别 -> 命令：命令名与类别全部来自 krnln.fne 的 LIB_INFO（逆向结果）
        CString sLib;
        sLib.Format(_T("系统核心支持库 (krnln.fne  命令 %d 条)"), g_nLibCmdCount);
        HTREEITEM hLib = pLib->InsertItem(sLib, TVI_ROOT, TVI_LAST);

        HTREEITEM hCat[64];
        memset(hCat, 0, sizeof(hCat));
        for (int i = 0; g_szLibCats[i] && i < 64; i++)
            hCat[i] = pLib->InsertItem(g_szLibCats[i], hLib, TVI_LAST);
        for (int i = 0; g_szLibCmds[i]; i++)
        {
            int c = g_nLibCmdCat[i];
            if (c >= 0 && c < 64 && hCat[c])
                pLib->InsertItem(g_szLibCmds[i], hCat[c], TVI_LAST);
        }
        pLib->Expand(hLib, TVE_EXPAND);
    }"""
    if old_lib not in t:
        print("!! 支持库填充段没匹配上")
    t = t.replace(old_lib, new_lib, 1)

    # 4) MDI 子窗口类名对齐原版 AfxMDIFrame42s
    old_cls = """    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
    afx_msg void OnSize(UINT nType, int cx, int cy);"""
    new_cls = """    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
    afx_msg void OnSize(UINT nType, int cx, int cy);"""
    if old_cls in t:
        t = t.replace(old_cls, new_cls, 1)
    else:
        print("!! CECChildFrame 声明没匹配上")

    old_impl = "BOOL CECChildFrame::OnCreateClient(LPCREATESTRUCT, CCreateContext*)\n{"
    new_impl = """// 原版 MDI 子窗口的窗口类是 AfxMDIFrame42s（Spy 实测），这里对齐
BOOL CECChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CMDIChildWnd::PreCreateWindow(cs))
        return FALSE;
    cs.lpszClass = _T("AfxMDIFrame42s");   // 基类已注册过这个类
    return TRUE;
}

BOOL CECChildFrame::OnCreateClient(LPCREATESTRUCT, CCreateContext*)
{"""
    if old_impl in t:
        t = t.replace(old_impl, new_impl, 1)
    else:
        print("!! OnCreateClient 实现没匹配上")

    if t != orig:
        io.open(SRC, "w", encoding="utf-8", newline="\n").write(t)
        print("patched OK, size=%d" % len(t))
    else:
        print("no change")


if __name__ == "__main__":
    main()

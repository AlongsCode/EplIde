# -*- coding: utf-8 -*-
"""让 CPaneBar 在运行时按 pane 的真实像素尺寸重排：
   tab 控件铺满整个 pane（TCS_BOTTOM → 标签行落在 pane 底部），内容页放在上方。
   否则对话框模板的 DLU 尺寸远大于实际 pane，底部的标签行会被裁掉。"""
import io

SRC = r"D:\eclone\src\EClone.cpp"


def main():
    t = io.open(SRC, encoding="utf-8").read()
    orig = t

    # 1) 类声明：加 LayoutPages / OnSize
    t = t.replace(
        "    void ShowPage(int n);\n    afx_msg void OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult);",
        "    void ShowPage(int n);\n"
        "    void LayoutPages();\n"
        "    afx_msg void OnSize(UINT nType, int cx, int cy);\n"
        "    afx_msg void OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult);", 1)

    # 2) 消息映射
    t = t.replace(
        "BEGIN_MESSAGE_MAP(CPaneBar, CDialogBar)\n    ON_NOTIFY(TCN_SELCHANGE, 0, OnTabSelChange)\nEND_MESSAGE_MAP()",
        "BEGIN_MESSAGE_MAP(CPaneBar, CDialogBar)\n"
        "    ON_WM_SIZE()\n"
        "    ON_NOTIFY(TCN_SELCHANGE, 0, OnTabSelChange)\n"
        "END_MESSAGE_MAP()", 1)

    # 3) CreatePane 末尾调用 LayoutPages
    t = t.replace(
        "        ShowPage(0);\n    }\n    return TRUE;\n}",
        "        ShowPage(0);\n    }\n"
        "    LayoutPages();\n"
        "    return TRUE;\n}", 1)

    # 4) 实现 LayoutPages / OnSize（插在 ShowPage 之前）
    impl = r'''
// 运行时按 pane 的像素尺寸重排：tab 铺满，标签行落在底部，内容页占上方。
// （对话框模板是 DLU 单位，尺寸远大于 pane，不重排的话 TCS_BOTTOM 的标签行会被裁掉）
void CPaneBar::LayoutPages()
{
    if (!GetSafeHwnd())
        return;
    CRect rc;
    GetClientRect(&rc);

    CWnd* pTab = (m_nTabID && m_nTabs > 0) ? GetDlgItem(m_nTabID) : NULL;
    int nPageBottom = rc.Height() - 4;
    if (pTab)
    {
        pTab->MoveWindow(0, 0, rc.Width(), rc.Height());
        nPageBottom = rc.Height() - 26;      // 给底部的标签行留 26px
    }
    if (nPageBottom < 20)
        nPageBottom = rc.Height();

    for (int i = 0; i < m_nTabs; i++)
    {
        CWnd* p = GetDlgItem(m_nFirstPageID + i);
        if (p)
            p->MoveWindow(3, 3, rc.Width() - 6, nPageBottom - 3);
    }
}

void CPaneBar::OnSize(UINT nType, int cx, int cy)
{
    CDialogBar::OnSize(nType, cx, cy);
    LayoutPages();
}

'''
    t = t.replace("void CPaneBar::ShowPage(int n)", impl.lstrip("\n") + "void CPaneBar::ShowPage(int n)", 1)

    if t != orig:
        io.open(SRC, "w", encoding="utf-8", newline="\n").write(t)
        print("patched EClone.cpp, size=%d" % len(t))
    else:
        print("!! 没有改动，检查锚点")


if __name__ == "__main__":
    main()

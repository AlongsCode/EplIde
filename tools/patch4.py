# -*- coding: utf-8 -*-
"""第 4 轮：编辑器高亮改成词法状态机；示例源码加上注释/字符串用于验证。"""
import io
import re

ED = r"D:\eclone\src\ec_editor.cpp"
EC = r"D:\eclone\src\EClone.cpp"

NEW_IMPL = r'''
// ------------------------------------------------------------------ 语法着色（词法状态机）
// 逐字节扫描：注释('到行尾)、字符串("..." / 全角“...”)、数字、标识符/中文命令名。
// 关键字只在"代码区"才着色 —— 注释和字符串里的关键字不会被染。
static const TCHAR* g_szKeywords[] = {
    _T("如果"), _T("如果真"), _T("如果真结束"), _T("判断"), _T("判断循环首"), _T("判断循环尾"),
    _T("循环判断首"), _T("循环判断尾"), _T("计次循环首"), _T("计次循环尾"),
    _T("变量循环首"), _T("变量循环尾"), _T("到循环尾"), _T("跳出循环"), _T("返回"), _T("结束"),
    _T("信息框"), _T("子程序"), _T("参数"), _T("局部变量"), _T("程序集"), _T("程序集变量"),
    _T("窗口"), _T("数据类型"), _T("常量"), _T("真"), _T("假"), _T("空"), NULL
};

static BOOL IsKeyword(const CString& s)
{
    for (int i = 0; g_szKeywords[i]; i++)
        if (s == g_szKeywords[i])
            return TRUE;
    return FALSE;
}

void CECEditor::SetRangeColor(long nFrom, long nLen, COLORREF cr)
{
    if (nLen <= 0)
        return;
    CHARFORMAT cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = cr;
    CHARRANGE cr2;
    cr2.cpMin = nFrom;
    cr2.cpMax = nFrom + nLen;
    ::SendMessage(m_hRich, EM_EXSETSEL, 0, (LPARAM)&cr2);
    ::SendMessage(m_hRich, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

void CECEditor::ApplyHighlight()
{
    if (!m_hRich)
        return;

    CString s = GetText();
    int n = s.GetLength();
    if (n <= 0 || n > 200000)
        return;

    // 1) 先整篇刷黑
    SetRangeColor(0, n, RGB(0, 0, 0));

    // 2) 词法扫描
    const COLORREF C_KEYWORD = RGB(0, 0, 255);
    const COLORREF C_STRING  = RGB(163, 21, 21);
    const COLORREF C_COMMENT = RGB(0, 128, 0);
    const COLORREF C_NUMBER  = RGB(9, 134, 88);

    int i = 0;
    while (i < n)
    {
        unsigned char c = (unsigned char)s[i];

        // 行注释：' 到行尾
        if (c == '\'')
        {
            int j = i;
            while (j < n && s[j] != _T('\r') && s[j] != _T('\n'))
                j++;
            SetRangeColor(i, j - i, C_COMMENT);
            i = j;
            continue;
        }

        // 字符串：半角 " 或全角 “ ”（GBK A1B0 / A1B1）
        if (c == '"' || (c == 0xA1 && i + 1 < n && (unsigned char)s[i + 1] == 0xB0))
        {
            BOOL bFull = (c == 0xA1);
            int j = i + (bFull ? 2 : 1);
            while (j < n)
            {
                if (bFull && (unsigned char)s[j] == 0xA1 && j + 1 < n &&
                    (unsigned char)s[j + 1] == 0xB1)
                {
                    j += 2;
                    break;
                }
                if (!bFull && s[j] == _T('"'))
                {
                    j++;
                    break;
                }
                if (s[j] == _T('\r') || s[j] == _T('\n'))
                    break;
                j++;
            }
            SetRangeColor(i, j - i, C_STRING);
            i = j;
            continue;
        }

        // 数字
        if (c >= '0' && c <= '9')
        {
            int j = i;
            while (j < n && ((s[j] >= '0' && s[j] <= '9') || s[j] == _T('.')))
                j++;
            SetRangeColor(i, j - i, C_NUMBER);
            i = j;
            continue;
        }

        // 标识符：ASCII 字母/下划线，或 GBK 汉字（>=0x81 双字节）
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0x81)
        {
            int j = i;
            while (j < n)
            {
                unsigned char d = (unsigned char)s[j];
                if ((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z') ||
                    (d >= '0' && d <= '9') || d == '_')
                    j++;
                else if (d >= 0x81 && j + 1 < n)
                    j += 2;
                else
                    break;
            }
            CString tok = s.Mid(i, j - i);
            if (IsKeyword(tok))
                SetRangeColor(i, j - i, C_KEYWORD);
            i = j;
            continue;
        }

        i++;
    }

    // 光标放回末尾
    CHARRANGE cr;
    cr.cpMin = n;
    cr.cpMax = n;
    ::SendMessage(m_hRich, EM_EXSETSEL, 0, (LPARAM)&cr);
    m_wndGutter.Refresh();
}
'''


def main():
    # --- 1) ec_editor.cpp：砍掉旧高亮，换新实现 ---
    t = io.open(ED, encoding="utf-8").read()
    k = t.find("// 简易中文关键字高亮")
    if k < 0:
        k = t.find("// ------------------------------------------------------------------ 语法着色")
    if k < 0:
        print("!! 找不到旧高亮函数")
        return
    t = t[:k] + NEW_IMPL.lstrip("\n")
    io.open(ED, "w", encoding="utf-8", newline="\n").write(t)
    print("ec_editor.cpp rewritten, size=%d" % len(t))

    # --- 2) 头文件里声明 SetRangeColor ---
    h = io.open(ED.replace(".cpp", ".h"), encoding="utf-8").read()
    if "SetRangeColor" not in h:
        h = h.replace("    void  ApplyHighlight();",
                      "    void  ApplyHighlight();\n    void  SetRangeColor(long nFrom, long nLen, COLORREF cr);")
        io.open(ED.replace(".cpp", ".h"), "w", encoding="utf-8", newline="\n").write(h)
        print("header updated")

    # --- 3) 示例源码：加入注释与"字符串里的关键字"，用来验证状态机 ---
    c = io.open(EC, encoding="utf-8").read()
    old = re.search(r"static const TCHAR\* g_szSampleCode =\n(?:.*?\n)*?    _T\(\"    返回 \(\)\\r\\n\"\);\n", c)
    if old:
        new = ('''static const TCHAR* g_szSampleCode =
    _T(".版本 2\\r\\n")
    _T("\\r\\n")
    _T(".程序集 启动窗口程序集\\r\\n")
    _T(".程序集变量 记录集, 整数型\\r\\n")
    _T("\\r\\n")
    _T(".子程序 _启动窗口_创建完毕\\r\\n")
    _T("\\r\\n")
    _T(".子程序 _图形按钮1_被单击\\r\\n")
    _T("    .局部变量 i, 整数型\\r\\n")
    _T("    信息框（\\"易语言中国人之骄傲！\\", 0 ）\\r\\n")
    _T("    ' 下面是循环：注释里的 如果真 和 \\"字符串\\" 都不该被当成关键字染色\\r\\n")
    _T("    .计次循环首 (10, i)\\r\\n")
    _T("        如果真 (i ＞ 5)\\r\\n")
    _T("            信息框（\\"这里的关键字\\" + 到文本 (i), 0 ）\\r\\n")
    _T("            跳出循环 ()\\r\\n")
    _T("        如果真结束\\r\\n")
    _T("    .计次循环尾 ()\\r\\n")
    _T("    返回 ()\\r\\n")
    _T("\\r\\n")
    _T(".子程序 加法, 整数型, , 公开\\r\\n")
    _T("    .参数 a, 整数型\\r\\n")
    _T("    .参数 b, 整数型\\r\\n")
    _T("    返回 (a + b)\\r\\n");
''')
        c = c[:old.start()] + new + c[old.end():]
        io.open(EC, "w", encoding="utf-8", newline="\n").write(c)
        print("sample code updated")
    else:
        print("!! 示例源码块没匹配上")


if __name__ == "__main__":
    main()

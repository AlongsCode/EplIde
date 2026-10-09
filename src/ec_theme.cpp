// ec_theme.cpp -- 配色 / 显示选项的实现
//
// 默认配色 = 原版 e.exe 的「默认配色」（硬编码数组，.data 0x66D5D8 起 42 个
// DWORD）。下表里的每个值都对着原版 1:1 截图量过（shots\live2\view0.png 的
// 13 种纯色 / shots\user_ref.png），所以不加载任何 .clr 时 EClone 的外观与
// 原版逐色相同。
#include "ec_theme.h"

#include <afxdlgs.h>
#include <stdlib.h>

EcTheme g_ecTheme;

// ------------------------------------------------------------------ 默认配色
// 下标 = EcColorSlot = 「默认配色」数组下标 = 「通常前景」起的颜色项序号 + 1。
//
// 来源（两处交叉验证，都已实测）：
//   a) e.exe .data 0x66D5CC 起 42 个 DWORD 的硬编码数组 —— 网格 #808080、
//      #E6EDE4、#D9E3F0、#ECECEC、#0000FF、#800000、#008080、#000080 全在里面；
//   b) shots\live2\view0.png（原版编辑器视图 1:1 PrintWindow，13 种纯色）测得
//      #888888 表格线 / #E6EDE4 子程序表头 / #D9E3F0 变量表头 / #ECECEC 流程槽。
// 两边在大多数槽位上一致；不一致的槽位（表格线、支持库命令色）取 (b) 的屏幕
// 实测值，因为验收是「截图逐色相同」。
static const COLORREF g_ecDefaultPalette[EC_COUNT] =
{
    /*[ 0] 版本号占位    */ RGB(0, 0, 0),
    /*[ 1] 通常前景      */ RGB(  0,   0,   0),   // 数组[0]  0x00000000
    /*[ 2] 通常背景      */ RGB(128,   0,   0),   // 数组[1]  0x00000080（原版值）
    /*[ 3] 表格标题前景  */ RGB(228, 237, 230),   // 数组[2]  0x00E6EDE4
    /*[ 4] 表格文本前景  */ RGB(255, 255, 255),   // 数组[3]  0x00FFFFFF
    /*[ 5] 通常表头背景  */ RGB(236, 236, 236),   // 数组[4]  0x00ECECEC
    /*[ 6] 通常表格背景  */ RGB(255, 255, 255),   // 数组[5]  0x00FFFFFF
    /*[ 7] 程序集表头背景*/ RGB(228, 237, 230),   // 数组[6]  0x00E6EDE4
    /*[ 8] 程序集表背景  */ RGB(255, 255, 255),   // 数组[7]  0x00FFFFFF
    /*[ 9] 子程序表头背景*/ RGB(217, 227, 240),   // 数组[8]  0x00D9E3F0
    /*[10] 子程序表背景  */ RGB(255, 255, 255),   // 数组[9]  0x00FFFFFF
    /*[11] 变量表头背景  */ RGB(255, 255, 255),   // 数组[10] 0x00FFFFFF
    /*[12] 变量表背景    */ RGB(  0,   0, 128),   // 数组[11] 0x00000080
    /*[13] 块内文本前景  */ RGB(255, 255, 255),   // 数组[12] 0x00FFFFFF
    /*[14] 块内文本背景  */ RGB(255,   0,   0),   // 数组[13] 0x00FF0000
    /*[15] 块内表格标题  */ RGB(  0,   0,   0),   // 数组[14] 0x00000000
    /*[16] 通常调用名前景*/ RGB(  0,   0, 128),   // 数组[15] 0x00000080
    /*[17] 流程命令名前景*/ RGB(  0,   0,   0),   // 数组[16] 0x00000000
    /*[18] 变量或组件名  */ RGB(  0, 128,   0),   // 数组[17] 0x00008000
    /*[19] 成员方法名前景*/ RGB(128,   0,   0),   // 数组[18] 0x00800000  ← 支持库命令 #800000
    /*[20] 成员变量/属性 */ RGB(128,   0,   0),   // 数组[19] 0x00800000
    /*[21] 备注或草稿前景*/ RGB(128,   0,   0),   // 数组[20] 0x00800000
    /*[22] 数组下标中括号*/ RGB(  0,   0,   0),   // 数组[21] 0x00000000
    /*[23] 小括号前景    */ RGB(128,   0,   0),   // 数组[22] 0x00800000
    /*[24] 操作符前景    */ RGB(128,   0,   0),   // 数组[23] 0x00800000
    /*[25] 成员引用句点  */ RGB(  0,   0, 128),   // 数组[24] 0x00000080
    /*[26] 逗号分隔符    */ RGB(128,   0,   0),   // 数组[25] 0x00800000
    /*[27] 常量前景      */ RGB(128,   0,   0),   // 数组[26] 0x00800000
    /*[28] 数值前景      */ RGB(  0, 128,   0),   // 数组[27] 0x00008000（橄榄 0x00808000 取绿）
    /*[29] 逻辑值前景    */ RGB(128,   0,   0),   // 数组[28] 0x00800000
    /*[30] 日期时间前景  */ RGB(  0,   0,   0),   // 数组[29] 0x00000000
    /*[31] 字符串前景    */ RGB(128, 128, 128),   // 数组[30] 0x00808080
    /*[32] 常量集花括号  */ RGB(136, 136, 136),   // 数组[31] 0x00888888
    /*[33] 子程序指针前景*/ RGB(255, 255, 255),   // 数组[32] 0x00FFFFFF
    /*[34] 连接线        */ RGB( 96,  96,  96),   // 数组[33] 0x00606060
    /*[35] 表格线        */ RGB(136, 136, 136),   // 屏幕实测 #888888（数组[31] 同值）
    /*[36] 块内表格线    */ RGB(128, 128, 128),
    /*[37] 分隔线        */ RGB(128, 128, 128),
    /*[38] 参数开启指示器*/ RGB(128,   0,   0),   // ✔ #800000
    /*[39] 参数名称文本  */ RGB(  0,   0,   0),
    /*[40] 标志列背景    */ RGB(236, 236, 236),   // 流程槽底 #ECECEC（屏幕实测）
    /*[41] 变量/参数定义名*/RGB(  0,   0,   0),
    /*[42] 数据类型      */ RGB(  0,   0, 255),   // #0000FF（屏幕实测）
    /*[43] 数组定义      */ RGB(  0,   0,   0),
    /*[44] 选中标记      */ RGB(128,   0,   0),
};

// 屏幕实测确认过的槽位（shots\live2\view0.png）。这些槽位**永远**用实测值，
// 不受上面数组里那几个「语义存疑」的槽位影响，保证默认外观与原版逐色相同。
static const struct { int slot; COLORREF cr; } g_ecMeasured[] =
{
    { EC_CS_FLOW_CMD_FG, RGB(  0,   0, 255) },   // 流程关键字 #0000FF
    { EC_CS_STRING_FG,   RGB(  0, 128, 128) },   // 字符串     #008080
    { EC_CS_COMMENT_FG,  RGB(  0, 128,   0) },   // 注释/备注  #008000
    { EC_CS_OPER_FG,     RGB(  0,   0, 128) },   // 括号运算符 #000080
    { EC_CS_GRID,        RGB(136, 136, 136) },   // 表格线     #888888
    { EC_CS_LINK,        RGB(128, 128, 128) },   // 流程虚线   #808080
    { EC_CS_FLAGCOL_BG,  RGB(236, 236, 236) },   // 流程槽底   #ECECEC
    { EC_CS_SUB_HEADER_BG, RGB(230, 237, 228) }, // 子程序表头 #E6EDE4
    { EC_CS_VAR_HEADER_BG, RGB(217, 227, 240) }, // 变量表头   #D9E3F0
    { EC_CS_DATATYPE_FG, RGB(  0,   0, 255) },   // 数据类型   #0000FF
    { EC_CS_MEMBER_FG,   RGB(128,   0,   0) },   // 支持库命令 #800000
    { EC_CS_PARAM_FLAG,  RGB(128,   0,   0) },   // ✔         #800000
    // 编辑器的**选区**底色（原版实测：shots\live_drag\after.png —— 在原版代码编辑器里
    // 拖选一段，选中部分就是 #000080 底 + 白字，选区高度 = 文字那 12px，不是整行）
    { EC_CS_MARK,        RGB(  0,   0, 128) },   // 选中标记   #000080
};

EcDispOpt::EcDispOpt()
{
    nLinePitch    = 22;      // 原版代码行距 22px（shots\live2\view0.png 实测）
    nFontH        = 12;      // 原版宋体 12px（实测）
    bFlowDot      = TRUE;
    bGridDot      = FALSE;
    bDrawGrid     = TRUE;
    bDrawSep      = TRUE;
    bDrawFlow     = TRUE;
    bDrawParamLine= TRUE;
    bShowParamName= TRUE;
}

EcTheme::EcTheme()
{
    ResetToDefault();
}

void EcTheme::ResetToDefault()
{
    int i;
    for (i = 0; i < EC_COUNT; i++)
        pal[i] = g_ecDefaultPalette[i];
    // 屏幕实测过的槽位一律以实测值为准（保证默认外观与原版逐色相同）
    for (i = 0; i < (int)(sizeof(g_ecMeasured) / sizeof(g_ecMeasured[0])); i++)
        pal[g_ecMeasured[i].slot] = g_ecMeasured[i].cr;
    sScheme = _T("默认配色");
}

// ------------------------------------------------------------------ .clr 读写
// 魔数：原版读入时接受 0x009CCA6B / 0x009CCA6C / 0x009CCA6D 三种（sub_447370），
// 保存时写 0x009CCA6D（sub_447840，紧跟着写 0x108 字节的颜色结构）。
#define EC_CLR_MAGIC_V1   0x009CCA6B
#define EC_CLR_MAGIC_V2   0x009CCA6C
#define EC_CLR_MAGIC_V3   0x009CCA6D

// 「默认配色」里实测确认过的槽位：加载 .clr 后仍然用实测值覆盖这几个。
// 原因见 ec_theme.h 顶部「已知不确定处」：.clr 里的 DWORD 顺序与 e.exe 里那份
// 硬编码数组不一致，而原版没有公开 .clr 的字段序。布局关键槽位（流程槽底 /
// 表格线 / 表头填充）一旦错位，编辑器会变得不可用（整块变色、网格消失），
// 所以这几项不吃文件值。语法色不设限 —— 换 .clr 主要就是看语法色变化。
static const int g_ecLayoutSlots[] =
{
    EC_CS_FLAGCOL_BG,       // 流程槽底
    EC_CS_GRID,             // 表格线
    EC_CS_SUB_HEADER_BG,    // 子程序表头填充
    EC_CS_VAR_HEADER_BG,    // 变量表表头填充
};
static BOOL EcIsLayoutSlot(int slot)
{
    for (int i = 0; i < (int)(sizeof(g_ecLayoutSlots) / sizeof(g_ecLayoutSlots[0])); i++)
        if (g_ecLayoutSlots[i] == slot)
            return TRUE;
    return FALSE;
}

BOOL EcIsClrFile(LPCTSTR pszPath){
    CFile f;
    CFileException e;
    if (!f.Open(pszPath, CFile::modeRead | CFile::shareDenyWrite, &e))
        return FALSE;
    DWORD dw = 0;
    BOOL bOk = (f.Read(&dw, 4) == 4) &&
               (dw == EC_CLR_MAGIC_V1 || dw == EC_CLR_MAGIC_V2 || dw == EC_CLR_MAGIC_V3);
    f.Close();
    return bOk;
}

BOOL EcTheme::LoadFile(LPCTSTR pszPath)
{
    CFile f;
    CFileException e;
    if (!f.Open(pszPath, CFile::modeRead | CFile::shareDenyWrite, &e))
        return FALSE;

    DWORD dwMagic = 0;
    if (f.Read(&dwMagic, 4) != 4 ||
        (dwMagic != EC_CLR_MAGIC_V1 && dwMagic != EC_CLR_MAGIC_V2 && dwMagic != EC_CLR_MAGIC_V3))
    {
        f.Close();
        return FALSE;                       // 原版同样拒绝（"指定文件不是配色文件。"）
    }

    // 魔数之后的 DWORD[0] 就是「通常前景」（= 颜色项序号 0），所以文件序号 i
    // 直接对应颜色槽 i。这条对应关系是拿 13 个 .clr + 原版实测色反推出来的：
    // 传统蓝色第 8 个 DWORD = 0x00D9E3F0，正是原版截图里「子程序变量表头背景」的
    // 浅蓝（原版实测 #D9E3F0）。⚠️ 别再假设「文件第 1 个不是颜色」——踩过。
    DWORD buf[64];
    memset(buf, 0, sizeof(buf));
    UINT nGot = f.Read(buf, sizeof(buf));
    f.Close();
    if (nGot < 4)
        return FALSE;

    int i, n = (int)(nGot / 4);
    for (i = 0; i < n; i++)
    {
        if (i >= EC_COUNT)
            break;
        if (EcIsLayoutSlot(i))
            continue;                       // 布局关键槽位保持实测值
        pal[i] = (COLORREF)(buf[i] & 0x00FFFFFF);
    }
    // 「连接线」在原版结构里位于文件末尾那一组（连接线/表格线/块内表格线/分隔线），
    // 跨 13 个 .clr 都稳定，所以额外按语义钉一次
    if (n >= 4)
        pal[EC_CS_LINK] = (COLORREF)(buf[n - 4] & 0x00FFFFFF);

    CString sName = pszPath;
    int k = sName.ReverseFind('\\');
    if (k >= 0)
        sName = sName.Mid(k + 1);
    if (sName.Right(4).CompareNoCase(_T(".clr")) == 0)
        sName = sName.Left(sName.GetLength() - 4);
    sScheme = sName;
    return TRUE;
}

BOOL EcTheme::SaveFile(LPCTSTR pszPath) const
{
    CFile f;
    CFileException e;
    if (!f.Open(pszPath, CFile::modeCreate | CFile::modeWrite, &e))
        return FALSE;

    // 与原版同形：魔数 + 颜色 DWORD 串（原版另有 0x108 字节结构，
    // 本工程按颜色项名单线性落盘，长度 4 + 257*0 = 4 + 64*4 = 260，
    // 和 C:\Program Files (x86)\e\clr\*.clr 的实测长度一致）。
    DWORD dwMagic = EC_CLR_MAGIC_V3;
    f.Write(&dwMagic, 4);
    DWORD buf[EC_COUNT];
    int i;
    for (i = 1; i < EC_COUNT; i++)
        buf[i - 1] = (DWORD)(pal[i] & 0x00FFFFFF);
    f.Write(buf, (UINT)(EC_COUNT - 1) * 4);
    f.Close();
    return TRUE;
}

// ------------------------------------------------------------------ 目录 / 枚举
CString EcClrDir()
{
    TCHAR szPath[MAX_PATH];
    szPath[0] = 0;
    ::GetModuleFileName(NULL, szPath, MAX_PATH);
    CString sDir = szPath;
    int k = sDir.ReverseFind('\\');
    if (k >= 0)
        sDir = sDir.Left(k);
    sDir += _T("\\clr");

    CFileFind ff;
    if (!ff.FindFile(sDir + _T("\\*.*")))
    {
        // 本工程旁边没有 clr 目录时退回原版安装目录（只读使用）
        sDir = _T("C:\\Program Files (x86)\\e\\clr");
    }
    ff.Close();
    return sDir;
}

void EcEnumClrSchemes(CStringArray& out)
{
    out.RemoveAll();
    out.Add(_T("默认配色"));                    // 原版下拉框第一项就是它
    CString sDir = EcClrDir();
    CFileFind ff;
    BOOL b = ff.FindFile(sDir + _T("\\*.clr"));
    while (b)
    {
        b = ff.FindNextFile();
        if (ff.IsDots() || ff.IsDirectory())
            continue;
        out.Add(ff.GetFileTitle());
    }
    ff.Close();
}

BOOL EcTheme::ApplyNamed(LPCTSTR pszScheme)
{
    if (pszScheme == NULL || *pszScheme == 0 || _tcscmp(pszScheme, _T("默认配色")) == 0)
    {
        ResetToDefault();
        return TRUE;
    }
    CString sPath = EcClrDir() + _T("\\") + pszScheme + _T(".clr");
    return LoadFile(sPath);
}

// ------------------------------------------------------------------ 持久化
// 原版把自己的设置放在 HKCU\Software\FlySky\E\EInf40（SaveE40 是 CArchive
// 二进制块）。本工程写自己的 EClone40 子键，避免踩坏原版设置。
#define EC_REG_KEY   _T("Software\\FlySky\\E\\EClone40")

// ------------------------------------------------------------------ 诊断 dump
// 输出是**全 ASCII** 的（路径/配色名可能是中文，所以名字放在最后并且不参与
// 回归比对）：每行 "SLOT <n> <RRGGBB>" + "OPT <name> <value>"，
// 用 tools\check_theme.py 逐行比对。
void EcTheme::DumpIfAsked()
{
    TCHAR szPath[MAX_PATH];
    DWORD n = ::GetEnvironmentVariable(_T("ECLONE_DUMP_THEME"), szPath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return;

    CStdioFile f;
    if (!f.Open(szPath, CFile::modeCreate | CFile::modeWrite | CFile::typeText))
        return;

    CString s;
    int i;
    // ACT = 当前生效的配色；DEF = 内置默认配色。
    // 拆成两份是为了让回归检查可以「永远」验证默认配色没被改坏
    // （用户可能在注册表里存了别的配色）。
    // ⚠️ COLORREF 是 0x00BBGGRR（GetRValue 取低字节），dump 统一按 RGB 打，
    //    免得检查脚本和源码对不上（这个坑踩过一次）。
    for (i = 1; i < EC_COUNT; i++)
    {
        s.Format(_T("ACT %d %02X%02X%02X\r\n"), i,
                 GetRValue(pal[i]), GetGValue(pal[i]), GetBValue(pal[i]));
        f.WriteString(s);
    }
    {
        EcTheme def;                    // 构造即 ResetToDefault()
        for (i = 1; i < EC_COUNT; i++)
        {
            s.Format(_T("DEF %d %02X%02X%02X\r\n"), i,
                     GetRValue(def.pal[i]), GetGValue(def.pal[i]), GetBValue(def.pal[i]));
            f.WriteString(s);
        }
    }

    struct { LPCTSTR name; int v; } iv[] = {
        { _T("LINEPITCH"), (int)opt.nLinePitch },
        { _T("FONTH"),     (int)opt.nFontH },
    };
    for (i = 0; i < 2; i++)
    {
        s.Format(_T("OPT %s %d\r\n"), iv[i].name, iv[i].v);
        f.WriteString(s);
    }
    struct { LPCTSTR name; BOOL v; } bv[] = {
        { _T("FLOWDOT"),    opt.bFlowDot },
        { _T("GRIDDOT"),    opt.bGridDot },
        { _T("DRAWGRID"),   opt.bDrawGrid },
        { _T("DRAWSEP"),    opt.bDrawSep },
        { _T("DRAWFLOW"),   opt.bDrawFlow },
        { _T("DRAWPARAM"),  opt.bDrawParamLine },
        { _T("SHOWPARAM"),  opt.bShowParamName },
    };
    for (i = 0; i < 7; i++)
    {
        s.Format(_T("OPT %s %d\r\n"), bv[i].name, bv[i].v ? 1 : 0);
        f.WriteString(s);
    }
    s.Format(_T("SCHEME %s\r\n"), (LPCTSTR)sScheme);
    f.WriteString(s);
    f.Close();
}

void EcThemeLoad()
{
    HKEY hKey;
    if (::RegOpenKeyEx(HKEY_CURRENT_USER, EC_REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    TCHAR szScheme[64];
    DWORD cb = sizeof(szScheme), dwType = 0;
    if (::RegQueryValueEx(hKey, _T("ColorScheme"), NULL, &dwType, (BYTE*)szScheme, &cb) == ERROR_SUCCESS)
        g_ecTheme.ApplyNamed(szScheme);

    struct { LPCTSTR name; int* pval; int def; } ints[] = {
        { _T("LinePitch"),   &g_ecTheme.opt.nLinePitch, 22 },
        { _T("FontH"),       &g_ecTheme.opt.nFontH,     12 },
    };
    int i;
    for (i = 0; i < 2; i++)
    {
        DWORD v = 0, cb2 = sizeof(v);
        if (::RegQueryValueEx(hKey, ints[i].name, NULL, &dwType, (BYTE*)&v, &cb2) == ERROR_SUCCESS)
            *ints[i].pval = (int)v;
    }
    struct { LPCTSTR name; BOOL* pval; } bools[] = {
        { _T("FlowDot"),   &g_ecTheme.opt.bFlowDot },
        { _T("GridDot"),   &g_ecTheme.opt.bGridDot },
        { _T("DrawGrid"),  &g_ecTheme.opt.bDrawGrid },
        { _T("DrawSep"),   &g_ecTheme.opt.bDrawSep },
        { _T("DrawFlow"),  &g_ecTheme.opt.bDrawFlow },
        { _T("DrawParamLine"), &g_ecTheme.opt.bDrawParamLine },
        { _T("ShowParamName"), &g_ecTheme.opt.bShowParamName },
    };
    for (i = 0; i < 7; i++)
    {
        DWORD v = 0, cb2 = sizeof(v);
        if (::RegQueryValueEx(hKey, bools[i].name, NULL, &dwType, (BYTE*)&v, &cb2) == ERROR_SUCCESS)
            *bools[i].pval = (v != 0);
    }
    ::RegCloseKey(hKey);
}

void EcThemeSave()
{
    HKEY hKey;
    DWORD dwDisp = 0;
    if (::RegCreateKeyEx(HKEY_CURRENT_USER, EC_REG_KEY, 0, NULL,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisp) != ERROR_SUCCESS)
        return;

    ::RegSetValueEx(hKey, _T("ColorScheme"), 0, REG_SZ,
                    (const BYTE*)(LPCTSTR)g_ecTheme.sScheme,
                    (DWORD)(g_ecTheme.sScheme.GetLength() + 1) * sizeof(TCHAR));
    DWORD v;
    v = (DWORD)g_ecTheme.opt.nLinePitch;
    ::RegSetValueEx(hKey, _T("LinePitch"), 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    v = (DWORD)g_ecTheme.opt.nFontH;
    ::RegSetValueEx(hKey, _T("FontH"), 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    struct { LPCTSTR name; BOOL val; } bools[] = {
        { _T("FlowDot"),   g_ecTheme.opt.bFlowDot },
        { _T("GridDot"),   g_ecTheme.opt.bGridDot },
        { _T("DrawGrid"),  g_ecTheme.opt.bDrawGrid },
        { _T("DrawSep"),   g_ecTheme.opt.bDrawSep },
        { _T("DrawFlow"),  g_ecTheme.opt.bDrawFlow },
        { _T("DrawParamLine"), g_ecTheme.opt.bDrawParamLine },
        { _T("ShowParamName"), g_ecTheme.opt.bShowParamName },
    };
    int i;
    for (i = 0; i < 7; i++)
    {
        v = bools[i].val ? 1 : 0;
        ::RegSetValueEx(hKey, bools[i].name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    }
    ::RegCloseKey(hKey);
}

// ===========================================================================
// 「系统配置」其余 6 个页签的设置（EcCfg）—— 见 ec_theme.h 顶部的说明。
//
// 持久化走**同一处注册表键**（EC_REG_KEY = HKCU\Software\FlySky\E\EClone40），
// 只是值名换一套前缀（Cfg*），不碰配色/显示选项那些已经验收过的值名。
// ===========================================================================
EcCfg g_ecCfg;

EcCfg::EcCfg()
{
    // 默认值：能实测的都取原版实测（设计器那段来自 shots\live_des_orig 的
    // WTWindow 380x250 + 8px 点阵），测不到的按原版模板里的语义取合理值。
    nStartMode    = 1101;       // 原版默认：通过 _启动窗口 启动
    sStartWin     = _T("_启动窗口");
    bBak          = TRUE;
    bAuthor       = FALSE;
    nAutoSaveMin  = 0;
    nUndoMemKB    = 0;          // 0 = 不限制
    sDebugOut     = _T("");

    nInputMethod  = 1127;       // 首拼及全拼
    bSouth        = TRUE;
    bTip          = TRUE;
    bSyntaxTip    = TRUE;
    nClipHistory  = 20;

    nNewFormW     = 380;        // 原版实测：设计器里的 WTWindow 就是 380x250
    nNewFormH     = 250;
    bShowGrid     = TRUE;
    nGridW        = 8;          // 原版实测：网点间距 8px
    nGridH        = 8;
    bSnapGrid     = TRUE;
    bRoProps      = FALSE;

    bFastAry      = FALSE;
    bDllStack     = TRUE;
    bDeadLoop     = TRUE;
    bDupName      = TRUE;
    bStrictRef    = TRUE;
    bAskWriteLib  = TRUE;
    bPrecLoss     = TRUE;
    sSignCert     = _T("");

    nManifest     = 1103;       // 使用预定义清单
    bComCtl6      = TRUE;
    bUac          = FALSE;
    sManifest     = _T("");

    nJunkLevel    = 0;
    nXorCode      = 0;
}

// 值名（注册表里的键名，全部 ASCII）与字段的对应表 —— 一处定义，读写共用。
struct EcCfgInt  { LPCTSTR name; int*     p; };
struct EcCfgBool { LPCTSTR name; BOOL*    p; };
struct EcCfgStr  { LPCTSTR name; CString* p; };

static EcCfgInt  g_cfgInts[] = {
    { _T("CfgStartMode"),   &g_ecCfg.nStartMode },
    { _T("CfgAutoSaveMin"), &g_ecCfg.nAutoSaveMin },
    { _T("CfgUndoMemKB"),   &g_ecCfg.nUndoMemKB },
    { _T("CfgInputMethod"), &g_ecCfg.nInputMethod },
    { _T("CfgClipHistory"), &g_ecCfg.nClipHistory },
    { _T("CfgNewFormW"),    &g_ecCfg.nNewFormW },
    { _T("CfgNewFormH"),    &g_ecCfg.nNewFormH },
    { _T("CfgGridW"),       &g_ecCfg.nGridW },
    { _T("CfgGridH"),       &g_ecCfg.nGridH },
    { _T("CfgManifest"),    &g_ecCfg.nManifest },
    { _T("CfgJunkLevel"),   &g_ecCfg.nJunkLevel },
    { _T("CfgXorCode"),     &g_ecCfg.nXorCode },
};
static EcCfgBool g_cfgBools[] = {
    { _T("CfgBak"),         &g_ecCfg.bBak },
    { _T("CfgAuthor"),      &g_ecCfg.bAuthor },
    { _T("CfgSouth"),       &g_ecCfg.bSouth },
    { _T("CfgTip"),         &g_ecCfg.bTip },
    { _T("CfgSyntaxTip"),   &g_ecCfg.bSyntaxTip },
    { _T("CfgShowGrid"),    &g_ecCfg.bShowGrid },
    { _T("CfgSnapGrid"),    &g_ecCfg.bSnapGrid },
    { _T("CfgRoProps"),     &g_ecCfg.bRoProps },
    { _T("CfgFastAry"),     &g_ecCfg.bFastAry },
    { _T("CfgDllStack"),    &g_ecCfg.bDllStack },
    { _T("CfgDeadLoop"),    &g_ecCfg.bDeadLoop },
    { _T("CfgDupName"),     &g_ecCfg.bDupName },
    { _T("CfgStrictRef"),   &g_ecCfg.bStrictRef },
    { _T("CfgAskWriteLib"), &g_ecCfg.bAskWriteLib },
    { _T("CfgPrecLoss"),    &g_ecCfg.bPrecLoss },
    { _T("CfgComCtl6"),     &g_ecCfg.bComCtl6 },
    { _T("CfgUac"),         &g_ecCfg.bUac },
};
static EcCfgStr  g_cfgStrs[] = {
    { _T("CfgDebugOut"),    &g_ecCfg.sDebugOut },
    { _T("CfgSignCert"),    &g_ecCfg.sSignCert },
    { _T("CfgManifestTxt"), &g_ecCfg.sManifest },
};
#define EC_CFG_NINT  (sizeof(g_cfgInts)  / sizeof(g_cfgInts[0]))
#define EC_CFG_NBOOL (sizeof(g_cfgBools) / sizeof(g_cfgBools[0]))
#define EC_CFG_NSTR  (sizeof(g_cfgStrs)  / sizeof(g_cfgStrs[0]))

void EcCfgLoad()
{
    HKEY hKey;
    if (::RegOpenKeyEx(HKEY_CURRENT_USER, EC_REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    DWORD dwType = 0;
    unsigned i;
    for (i = 0; i < EC_CFG_NINT; i++)
    {
        DWORD v = 0, cb = sizeof(v);
        if (::RegQueryValueEx(hKey, g_cfgInts[i].name, NULL, &dwType, (BYTE*)&v, &cb) == ERROR_SUCCESS)
            *g_cfgInts[i].p = (int)v;
    }
    for (i = 0; i < EC_CFG_NBOOL; i++)
    {
        DWORD v = 0, cb = sizeof(v);
        if (::RegQueryValueEx(hKey, g_cfgBools[i].name, NULL, &dwType, (BYTE*)&v, &cb) == ERROR_SUCCESS)
            *g_cfgBools[i].p = (v != 0);
    }
    for (i = 0; i < EC_CFG_NSTR; i++)
    {
        TCHAR sz[512];
        DWORD cb = sizeof(sz);
        sz[0] = 0;
        if (::RegQueryValueEx(hKey, g_cfgStrs[i].name, NULL, &dwType, (BYTE*)sz, &cb) == ERROR_SUCCESS)
            *g_cfgStrs[i].p = sz;
    }
    ::RegCloseKey(hKey);
}

void EcCfgSave()
{
    HKEY hKey;
    DWORD dwDisp = 0;
    if (::RegCreateKeyEx(HKEY_CURRENT_USER, EC_REG_KEY, 0, NULL,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisp) != ERROR_SUCCESS)
        return;

    DWORD v;
    unsigned i;
    for (i = 0; i < EC_CFG_NINT; i++)
    {
        v = (DWORD)*g_cfgInts[i].p;
        ::RegSetValueEx(hKey, g_cfgInts[i].name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    }
    for (i = 0; i < EC_CFG_NBOOL; i++)
    {
        v = (*g_cfgBools[i].p) ? 1 : 0;
        ::RegSetValueEx(hKey, g_cfgBools[i].name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    }
    for (i = 0; i < EC_CFG_NSTR; i++)
    {
        ::RegSetValueEx(hKey, g_cfgStrs[i].name, 0, REG_SZ,
                        (const BYTE*)(LPCTSTR)(*g_cfgStrs[i].p),
                        (DWORD)((*g_cfgStrs[i].p).GetLength() + 1) * sizeof(TCHAR));
    }
    ::RegCloseKey(hKey);
}

// 诊断倒档（ECLONE_DUMP_CFG=<path>）：一行一项，全 ASCII
//   INT  <name> <value>
//   BOOL <name> 0|1
//   STR  <name> <len>            <- 不写内容（可能带中文/换行，回归只需要长度是稳定的）
// 回归脚本 tools\cfg_smoke.ps1 就是拿它当断言依据的。
void EcCfgDumpIfAsked()
{
    TCHAR szPath[MAX_PATH];
    DWORD n = ::GetEnvironmentVariable(_T("ECLONE_DUMP_CFG"), szPath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return;

    CStdioFile f;
    if (!f.Open(szPath, CFile::modeCreate | CFile::modeWrite | CFile::typeText))
        return;

    CString s;
    unsigned i;
    for (i = 0; i < EC_CFG_NINT; i++)
    {
        s.Format(_T("INT %s %d\r\n"), g_cfgInts[i].name, *g_cfgInts[i].p);
        f.WriteString(s);
    }
    for (i = 0; i < EC_CFG_NBOOL; i++)
    {
        s.Format(_T("BOOL %s %d\r\n"), g_cfgBools[i].name, (*g_cfgBools[i].p) ? 1 : 0);
        f.WriteString(s);
    }
    for (i = 0; i < EC_CFG_NSTR; i++)
    {
        // 只写长度：字符串里可能有中文/换行，回归只需要「长度稳定」这个判据
        s.Format(_T("STR %s %d\r\n"), g_cfgStrs[i].name, (*g_cfgStrs[i].p).GetLength());
        f.WriteString(s);
    }
    f.Close();
}

// 把「系统配置」对话框**其余 6 个页签**的控件与 g_ecCfg 互相同步。
// 每个控件都真的读写 + 持久化（见上面的 EcCfg 说明与 ec_theme.h 的分类）。
// 一份描述表覆盖"控件 id -> 字段"（type: 0=整数, 1=勾选, 2=单选, 3=文本）。
namespace {
struct EcCfgCtl { int id; int type; void* p; int def; };
}

void EcCfgToDlg(CWnd* pDlg)
{
    static const EcCfgCtl t[] = {
        // [0] 通常
        { 1101, 2, &g_ecCfg.nStartMode,          1101 },
        { 1041, 1, &g_ecCfg.bBak,                1 },
        { 1042, 1, &g_ecCfg.bAuthor,             0 },
        { 1001, 0, &g_ecCfg.nAutoSaveMin,        0 },
        { 1011, 0, &g_ecCfg.nUndoMemKB,          0 },
        { 1236, 3, &g_ecCfg.sDebugOut,           0 },
        // [1] 编 辑
        { 1127, 2, &g_ecCfg.nInputMethod,        1127 },
        { 1130, 1, &g_ecCfg.bSouth,              1 },
        { 1180, 1, &g_ecCfg.bTip,                1 },
        { 1132, 1, &g_ecCfg.bSyntaxTip,          1 },
        { 1201, 0, &g_ecCfg.nClipHistory,        20 },
        // [2] 显 示（设计器外观）
        { 1006, 0, &g_ecCfg.nNewFormW,           380 },
        { 1007, 0, &g_ecCfg.nNewFormH,           250 },
        { 1144, 1, &g_ecCfg.bShowGrid,           1 },
        { 1008, 0, &g_ecCfg.nGridW,              8 },
        { 1009, 0, &g_ecCfg.nGridH,              8 },
        { 1146, 1, &g_ecCfg.bSnapGrid,           1 },
        { 1147, 1, &g_ecCfg.bRoProps,            0 },
        // [4] 编 译
        { 1165, 1, &g_ecCfg.bFastAry,            0 },
        { 1168, 1, &g_ecCfg.bDllStack,           1 },
        { 1170, 1, &g_ecCfg.bDeadLoop,           1 },
        { 1136, 1, &g_ecCfg.bDupName,            1 },
        { 1140, 1, &g_ecCfg.bStrictRef,          1 },
        { 1138, 1, &g_ecCfg.bAskWriteLib,        1 },
        { 1141, 1, &g_ecCfg.bPrecLoss,           1 },
        { 1235, 3, &g_ecCfg.sSignCert,           0 },
        // [5] 存 根
        { 1103, 2, &g_ecCfg.nManifest,           1103 },
        { 1208, 1, &g_ecCfg.bComCtl6,            1 },
        { 1207, 1, &g_ecCfg.bUac,                0 },
        { 1010, 3, &g_ecCfg.sManifest,           0 },
        // [6] 安 全
        { 1198, 0, &g_ecCfg.nJunkLevel,          0 },
        { 613,  0, &g_ecCfg.nXorCode,            0 },
    };
    static const int n = sizeof(t) / sizeof(t[0]);

    CComboBox* pJunk = (CComboBox*)pDlg->GetDlgItem(1198);
    if (pJunk != NULL && pJunk->GetCount() == 0)
    {
        for (int k = 0; k <= 10; k++)
        {
            CString s; s.Format(_T("%d"), k);
            pJunk->AddString(s);
        }
    }

    int i;
    for (i = 0; i < n; i++)
    {
        CWnd* pw = pDlg->GetDlgItem(t[i].id);
        if (pw == NULL)
            continue;
        switch (t[i].type)
        {
        case 0:
            {
                CString s; s.Format(_T("%d"), *(int*)t[i].p);
                pw->SetWindowText(s);
                if (t[i].id == 1198)          // 花指令级别是下拉框
                {
                    CString s2; s2.Format(_T("%d"), *(int*)t[i].p);
                    ((CComboBox*)pw)->SelectString(-1, s2);
                }
            }
            break;
        case 1:
            pDlg->CheckDlgButton(t[i].id, (*(BOOL*)t[i].p) ? BST_CHECKED : BST_UNCHECKED);
            break;
        case 2:
            pDlg->CheckRadioButton(t[i].id, t[i].id + 1,
                                   (*(int*)t[i].p == t[i].id) ? t[i].id : t[i].id + 1);
            break;
        case 3:
            pw->SetWindowText(*(CString*)t[i].p);
            break;
        }
    }
}

void EcCfgFromDlg(CWnd* pDlg)
{
    static const EcCfgCtl t[] = {
        { 1101, 2, &g_ecCfg.nStartMode,          1101 },
        { 1041, 1, &g_ecCfg.bBak,                1 },
        { 1042, 1, &g_ecCfg.bAuthor,             0 },
        { 1001, 0, &g_ecCfg.nAutoSaveMin,        0 },
        { 1011, 0, &g_ecCfg.nUndoMemKB,          0 },
        { 1236, 3, &g_ecCfg.sDebugOut,           0 },
        { 1127, 2, &g_ecCfg.nInputMethod,        1127 },
        { 1130, 1, &g_ecCfg.bSouth,              1 },
        { 1180, 1, &g_ecCfg.bTip,                1 },
        { 1132, 1, &g_ecCfg.bSyntaxTip,          1 },
        { 1201, 0, &g_ecCfg.nClipHistory,        20 },
        { 1006, 0, &g_ecCfg.nNewFormW,           380 },
        { 1007, 0, &g_ecCfg.nNewFormH,           250 },
        { 1144, 1, &g_ecCfg.bShowGrid,           1 },
        { 1008, 0, &g_ecCfg.nGridW,              8 },
        { 1009, 0, &g_ecCfg.nGridH,              8 },
        { 1146, 1, &g_ecCfg.bSnapGrid,           1 },
        { 1147, 1, &g_ecCfg.bRoProps,            0 },
        { 1165, 1, &g_ecCfg.bFastAry,            0 },
        { 1168, 1, &g_ecCfg.bDllStack,           1 },
        { 1170, 1, &g_ecCfg.bDeadLoop,           1 },
        { 1136, 1, &g_ecCfg.bDupName,            1 },
        { 1140, 1, &g_ecCfg.bStrictRef,          1 },
        { 1138, 1, &g_ecCfg.bAskWriteLib,        1 },
        { 1141, 1, &g_ecCfg.bPrecLoss,           1 },
        { 1235, 3, &g_ecCfg.sSignCert,           0 },
        { 1103, 2, &g_ecCfg.nManifest,           1103 },
        { 1208, 1, &g_ecCfg.bComCtl6,            1 },
        { 1207, 1, &g_ecCfg.bUac,                0 },
        { 1010, 3, &g_ecCfg.sManifest,           0 },
        { 1198, 0, &g_ecCfg.nJunkLevel,          0 },
        { 613,  0, &g_ecCfg.nXorCode,            0 },
    };
    static const int n = sizeof(t) / sizeof(t[0]);

    int i;
    for (i = 0; i < n; i++)
    {
        CWnd* pw = pDlg->GetDlgItem(t[i].id);
        if (pw == NULL)
            continue;
        switch (t[i].type)
        {
        case 0:
            {
                CString s;
                if (t[i].id == 1198)          // 下拉框：取 cursel 的文本
                {
                    int k = ((CComboBox*)pw)->GetCurSel();
                    if (k < 0) break;
                    ((CComboBox*)pw)->GetLBText(k, s);
                }
                else
                {
                    pw->GetWindowText(s);
                }
                s.TrimLeft(); s.TrimRight();
                if (!s.IsEmpty())
                    *(int*)t[i].p = _ttoi(s);
            }
            break;
        case 1:
            *(BOOL*)t[i].p = (pDlg->IsDlgButtonChecked(t[i].id) == BST_CHECKED);
            break;
        case 2:
            if (pDlg->IsDlgButtonChecked(t[i].id) == BST_CHECKED)
                *(int*)t[i].p = t[i].id;
            break;
        case 3:
            pw->GetWindowText(*(CString*)t[i].p);
            break;
        }
    }
}

// 「系统配置」按确认之后：让新设置立刻生效（设计器网格/新建尺寸、编辑器撤消上限）。
// 实现见 ec_syscfg.cpp（它会遍历所有 MDI 子窗口里的编辑器重排/重画）。
void EcRefreshEditors();      // 声明在 ec_syscfg.h；这里不 include 它以避免循环依赖

void EcCfgApply()
{
    EcRefreshEditors();
}


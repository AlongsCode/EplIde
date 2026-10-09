// ec_syscfg.cpp -- 「工具->系统配置」对话框的实现
#include "ec_syscfg.h"
#include "ec_editor.h"
#include "resource.h"
#include <afxcmn.h>

#include <afxdlgs.h>
#include <afxcmn.h>

BEGIN_MESSAGE_MAP(CEcSysCfgDlg, CDialog)
    ON_NOTIFY(TCN_SELCHANGE, ID_SYSCFG_TAB, OnTabChange)
    ON_CBN_SELCHANGE(IDC_CFG_SCHEME, OnSchemeChange)
    ON_BN_CLICKED(IDC_CFG_IMPORT, OnImport)
    ON_BN_CLICKED(IDC_CFG_SAVE, OnSaveClr)
    ON_BN_CLICKED(IDC_CFG_FONT, OnSetFont)
    ON_BN_CLICKED(ID_SYSCFG_DEFAULT, OnDefault)
    ON_BN_CLICKED(IDC_CFG_FLOWDOT, OnDrawOpt)
    ON_BN_CLICKED(IDC_CFG_GRIDDOT, OnDrawOpt)
    ON_BN_CLICKED(IDC_CFG_DRAWGRID, OnDrawOpt)
    ON_BN_CLICKED(IDC_CFG_DRAWSEP, OnDrawOpt)
    ON_BN_CLICKED(IDC_CFG_DRAWFLOW, OnDrawOpt)
    ON_BN_CLICKED(IDC_CFG_DRAWPARAM, OnDrawOpt)
    ON_BN_CLICKED(IDC_CFG_SHOWPARAM, OnDrawOpt)
    // 其余 6 个页签里需要弹文件框的两个按钮（存根页「从文件导入」/ 编译页签名证书的「...」）
    ON_BN_CLICKED(IDC_CFG_MANIIMPORT, OnManifestImport)
    ON_BN_CLICKED(IDC_CFG_SIGNBROWSE, OnSignBrowse)
END_MESSAGE_MAP()

CEcSysCfgDlg::CEcSysCfgDlg(CWnd* pParent)
    : CDialog(CEcSysCfgDlg::IDD, pParent)
{
}

// ------------------------------------------------------------------ 编辑器刷新
// 原版改完「程序显示」页确认后编辑器立刻变色。这里遍历主框架下所有 MDI 子窗口，
// 在它们的直接子窗口里找类名以 "Afx:400000:8:10005" 开头的那个自绘编辑器
// （见 README：本工程编辑器窗口类是 Afx:400000:8:10005:6:0），对它重排 + 重画。
static CECEditor* EcFindEditorIn(CWnd* pParent)
{
    CWnd* p = pParent->GetWindow(GW_CHILD);
    while (p != NULL)
    {
        TCHAR szCls[64];
        szCls[0] = 0;
        ::GetClassName(p->GetSafeHwnd(), szCls, 63);
        if (_tcsncmp(szCls, _T("Afx:400000:8:10005"), 19) == 0)
            return (CECEditor*)p;
        p = p->GetWindow(GW_HWNDNEXT);
    }
    return NULL;
}

void EcRefreshEditors()
{
    CWnd* pMain = AfxGetMainWnd();
    if (pMain != NULL)
    {
        int n = 0;
        CWnd* pChild = pMain->GetWindow(GW_CHILD);
        while (pChild != NULL)
        {
            TCHAR szCls[64];
            szCls[0] = 0;
            ::GetClassName(pChild->GetSafeHwnd(), szCls, 63);
            if (_tcscmp(szCls, _T("AfxMDIFrame42s")) == 0)
            {
                CECEditor* pEd = EcFindEditorIn(pChild);
                if (pEd != NULL)
                {
                    pEd->ApplyTheme();
                    n++;
                }
            }
            pChild = pChild->GetWindow(GW_HWNDNEXT);
        }
        pMain->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

// ------------------------------------------------------------------ 页签
// 原版 id=282 是「一个模板放全部 78 个控件 + 一个 SysTabControl32 按页显示/隐藏」的形态
// （不是 MFC 的 CPropertySheet），本工程照做。
//
// 页名与次序 = **原版实拍读出来的**（shots\live_syscfg\tabs_zoom2.png 把标题栏下那条页签
// 放大 4 倍）：[0] 通常  [1] 编 辑  [2] 显示  [3] 设计  [4] 编译  [5] 存根  [6] 安全。
// ⚠️ 别再用「安全/存根/编译/设计/显示/编辑」那 6 个 —— 那是照推断写的，名字和次序都错。
static const TCHAR* const g_szCfgPages[7] = {
    _T("通常"), _T("编 辑"), _T("显示"), _T("设计"), _T("编译"), _T("存根"), _T("安全")
};

// 每页的控件（id 全部照原版 id=282 的编号；静态提示文字用本工程自留的 22xx 段，
// 因为原版那些 Static 的 id 是 65535/1115…，同一个 id 会在别的对话框上撞车）。
//
// ★ 归属 = **原版运行时实测**（`tools\probe_cfgrets.ps1` 真鼠标点每个页签再倒可见控件，
//   结果 shots\live_syscfg\rects.txt）。别照模板的建立次序猜 —— 那个次序不是分页次序
//   （「默认输入法」那一组在模板第 12 行，却属于第 1 页「编 辑」）。
// ⚠️ 只有「显示」页的显示选项**真的接在主题上**（见 WriteCtrls/ReadCtrls）；
//    其余页的控件按原版摆出来、还没有行为。
static const int g_page0[] = {   // 通常
    IDC_CFG_G1, IDC_CFG_STARTWIN, IDC_CFG_STARTSUB, IDC_CFG_BAK, IDC_CFG_AUTHOR,
    IDC_CFG_SAVEMIN, IDC_CFG_UNDOMEM, IDC_CFG_DEBUGOUT, IDC_CFG_DEBUGBROWSE,
    2210, 2211, 2212
};
static const int g_page1[] = {   // 编 辑（默认输入法 + 编辑行为）
    IDC_CFG_G2, IDC_CFG_IM1, IDC_CFG_IM2, IDC_CFG_IM3, IDC_CFG_IMEDIT,
    IDC_CFG_SOUTH, IDC_CFG_TIP, IDC_CFG_SYNTAX, IDC_CFG_CLIPNUM, 2213
};
static const int g_page2[] = {   // 显示（程序显示：外观开关 + 行距/字体/配色）
    IDC_CFG_FLOWDOT, IDC_CFG_GRIDDOT, IDC_CFG_DRAWGRID, IDC_CFG_DRAWSEP,
    IDC_CFG_DRAWFLOW, IDC_CFG_DRAWPARAM, IDC_CFG_SHOWPARAM,
    IDC_CFG_LINEPITCH, IDC_CFG_FONT, IDC_CFG_SCHEME, IDC_CFG_IMPORT, IDC_CFG_SAVE,
    IDC_CFG_COLOR2, IDC_CFG_COLORBOX,
    2220, 2221, 2222
};
static const int g_page3[] = {   // 设计（设计器外观）
    IDC_CFG_NEWW, IDC_CFG_NEWH, IDC_CFG_GRID, IDC_CFG_GRIDW, IDC_CFG_GRIDH,
    IDC_CFG_SNAP, IDC_CFG_ROPROP,
    2230, 2231, 2232, 2233
};
static const int g_page4[] = {   // 编译
    IDC_CFG_FASTARY, IDC_CFG_DLLSTK, IDC_CFG_DEADLOOP, IDC_CFG_DUPNAME, IDC_CFG_STRICTREF,
    IDC_CFG_ASKWRITE, IDC_CFG_PRECLOSS, IDC_CFG_SIGNCERT, IDC_CFG_SIGNBROWSE,
    2234, 2235, 2236
};
static const int g_page5[] = {   // 存根（清单）
    IDC_CFG_MANIPRE, IDC_CFG_COMCTL6, IDC_CFG_UAC, IDC_CFG_MANIUSER,
    IDC_CFG_MANIIMPORT, IDC_CFG_MANITEXT
};
static const int g_page6[] = {   // 安全（混淆）
    IDC_CFG_JUNKLEVEL, IDC_CFG_XORCODE, 2240, 2241, 2242, 2243
};

struct EcCfgPage { const int* ids; int n; };
static const EcCfgPage g_cfgPages[7] = {
    { g_page0, sizeof(g_page0) / sizeof(g_page0[0]) },
    { g_page1, sizeof(g_page1) / sizeof(g_page1[0]) },
    { g_page2, sizeof(g_page2) / sizeof(g_page2[0]) },
    { g_page3, sizeof(g_page3) / sizeof(g_page3[0]) },
    { g_page4, sizeof(g_page4) / sizeof(g_page4[0]) },
    { g_page5, sizeof(g_page5) / sizeof(g_page5[0]) },
    { g_page6, sizeof(g_page6) / sizeof(g_page6[0]) },
};

// 只显示第 n 页的控件。启动时也要调一次 —— 模板里所有控件都是 WS_VISIBLE，
// 不先藏一遍的话「通常」页上会压着另外 6 页的控件（第一版就是这个现象）。
void CEcSysCfgDlg::ShowPage(int n)
{
    for (int p = 0; p < 7; p++)
    {
        BOOL bShow = (p == n);
        for (int i = 0; i < g_cfgPages[p].n; i++)
        {
            CWnd* pw = GetDlgItem(g_cfgPages[p].ids[i]);
            if (pw != NULL)
                pw->ShowWindow(bShow ? SW_SHOW : SW_HIDE);
        }
    }
}

// ------------------------------------------------------------------ 初始化
BOOL CEcSysCfgDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(ID_SYSCFG_TAB);
    if (pTab != NULL)
    {
        for (int i = 0; i < 7; i++)
            pTab->InsertItem(i, g_szCfgPages[i]);
        pTab->SetCurSel(0);             // 原版默认停在第 0 页「通常」
    }
    ShowPage(0);

    m_cboScheme.SubclassDlgItem(IDC_CFG_SCHEME, this);
    FillSchemeCombo();
    WriteCtrls(g_ecTheme);
    EcCfgToDlg(this);              // 其余 6 个页签的控件全部填上当前设置（原版模板里它们是空的）
    m_sSchemeApplied = g_ecTheme.sScheme;
    return TRUE;
}

void CEcSysCfgDlg::FillSchemeCombo()
{
    EcEnumClrSchemes(m_schemes);
    m_cboScheme.ResetContent();
    int nSel = 0;
    for (int i = 0; i < (int)m_schemes.GetSize(); i++)
    {
        m_cboScheme.AddString(m_schemes[i]);
        if (m_schemes[i] == g_ecTheme.sScheme)
            nSel = i;
    }
    // 当前配色是「默认配色」时按原版默认高亮第一项
    m_cboScheme.SetCurSel(nSel);
}

void CEcSysCfgDlg::WriteCtrls(const EcTheme& t)
{
    CString s;
    s.Format(_T("%d"), t.opt.nLinePitch);
    SetDlgItemText(IDC_CFG_LINEPITCH, s);

    CheckDlgButton(IDC_CFG_FLOWDOT,   t.opt.bFlowDot      ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_CFG_GRIDDOT,   t.opt.bGridDot      ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_CFG_DRAWGRID,  t.opt.bDrawGrid     ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_CFG_DRAWSEP,   t.opt.bDrawSep      ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_CFG_DRAWFLOW,  t.opt.bDrawFlow     ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_CFG_DRAWPARAM, t.opt.bDrawParamLine? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_CFG_SHOWPARAM, t.opt.bShowParamName? BST_CHECKED : BST_UNCHECKED);
}

void CEcSysCfgDlg::ReadCtrls(EcTheme& t)
{
    CString s;
    GetDlgItemText(IDC_CFG_LINEPITCH, s);
    int n = _ttoi(s);
    if (n >= 8 && n <= 64)
        t.opt.nLinePitch = n;

    t.opt.bFlowDot      = (IsDlgButtonChecked(IDC_CFG_FLOWDOT)   == BST_CHECKED);
    t.opt.bGridDot      = (IsDlgButtonChecked(IDC_CFG_GRIDDOT)   == BST_CHECKED);
    t.opt.bDrawGrid     = (IsDlgButtonChecked(IDC_CFG_DRAWGRID)  == BST_CHECKED);
    t.opt.bDrawSep      = (IsDlgButtonChecked(IDC_CFG_DRAWSEP)   == BST_CHECKED);
    t.opt.bDrawFlow     = (IsDlgButtonChecked(IDC_CFG_DRAWFLOW)  == BST_CHECKED);
    t.opt.bDrawParamLine= (IsDlgButtonChecked(IDC_CFG_DRAWPARAM) == BST_CHECKED);
    t.opt.bShowParamName= (IsDlgButtonChecked(IDC_CFG_SHOWPARAM) == BST_CHECKED);

    int k = m_cboScheme.GetCurSel();
    if (k >= 0 && k < (int)m_schemes.GetSize())
    {
        // 配色本身由 OnSchemeChange 已经套用过，这里只记名字
        t.sScheme = m_schemes[k];
    }
}

// ------------------------------------------------------------------ 消息
void CEcSysCfgDlg::OnTabChange(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(ID_SYSCFG_TAB);
    int n = (pTab != NULL) ? pTab->GetCurSel() : 0;
    ShowPage(n);
    if (pResult != NULL)
        *pResult = 0;
}

void CEcSysCfgDlg::PreviewScheme()
{
    int k = m_cboScheme.GetCurSel();
    if (k < 0 || k >= (int)m_schemes.GetSize())
        return;
    if (!g_ecTheme.ApplyNamed(m_schemes[k]))
        MessageBox(_T("指定文件不是配色文件。"), _T("系统配置"), MB_ICONWARNING);
    m_sSchemeApplied = g_ecTheme.sScheme;
    PushToEditors();
}

void CEcSysCfgDlg::OnSchemeChange()
{
    PreviewScheme();
}

void CEcSysCfgDlg::PushToEditors()
{
    EcRefreshEditors();
}

// 「导入(&I)」：原版是弹「请输入欲导入的易系统配色文件名：」+ 配色文件(*.clr)|*.clr
void CEcSysCfgDlg::OnImport()
{
    CFileDialog dlg(TRUE, _T("clr"), NULL,
                    OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
                    _T("配色文件(*.clr)|*.clr|所有文件(*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK)
        return;
    if (!g_ecTheme.LoadFile(dlg.GetPathName()))
    {
        MessageBox(_T("指定文件不是配色文件。"), _T("系统配置"), MB_ICONWARNING);
        return;
    }
    // 把当前项加进下拉并选中
    int k = m_cboScheme.FindStringExact(-1, g_ecTheme.sScheme);
    if (k < 0)
    {
        m_schemes.Add(g_ecTheme.sScheme);
        k = m_cboScheme.AddString(g_ecTheme.sScheme);
    }
    m_cboScheme.SetCurSel(k);
    m_sSchemeApplied = g_ecTheme.sScheme;
    PushToEditors();
}

// 「保存颜色设置(&W)」：原版提示「请输入欲保存到的易系统配色文件名：」
void CEcSysCfgDlg::OnSaveClr()
{
    EcTheme tmp = g_ecTheme;
    ReadCtrls(tmp);

    CFileDialog dlg(FALSE, _T("clr"), _T("我的配色.clr"),
                    OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                    _T("配色文件(*.clr)|*.clr||"), this);
    if (dlg.DoModal() != IDOK)
        return;
    if (tmp.SaveFile(dlg.GetPathName()))
        MessageBox(_T("写出文件成功!"), _T("系统配置"), MB_ICONINFORMATION);
    else
        MessageBox(_T("写出文件失败!"), _T("系统配置"), MB_ICONWARNING);

    // 让新文件立刻出现在下拉里
    FillSchemeCombo();
}

void CEcSysCfgDlg::OnSetFont()
{
    CFontDialog dlg;
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -g_ecTheme.opt.nFontH;
    lf.lfCharSet = GB2312_CHARSET;
    lf.lfWeight = FW_NORMAL;
    _tcscpy(lf.lfFaceName, _T("宋体"));
    dlg.m_cf.lpLogFont = &lf;
    dlg.m_cf.Flags |= CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_NOVERTFONTS;
    if (dlg.DoModal() == IDOK)
    {
        // 原版编辑器是等宽像素字体，只取字号；字体名固定宋体
        int h = -dlg.GetSize() / 10;
        if (h < 6) h = 6;
        if (h > 72) h = 72;
        g_ecTheme.opt.nFontH = h;
        PushToEditors();
    }
}

// 「置回默认值(&D)」：把**全部 7 页**的控件和 g_ecCfg 都置回原版默认值。
void CEcSysCfgDlg::OnDefault()
{
    g_ecTheme.ResetToDefault();
    m_cboScheme.SetCurSel(0);
    m_sSchemeApplied = g_ecTheme.sScheme;
    WriteCtrls(g_ecTheme);

    EcCfg def;                     // 构造即默认值
    g_ecCfg = def;
    EcCfgToDlg(this);
    EcCfgSave();
    EcCfgApply();
    PushToEditors();
}

void CEcSysCfgDlg::OnDrawOpt()
{
    // 复选框改动立刻反映到编辑器（原版也是即时刷新）
    EcTheme tmp = g_ecTheme;
    tmp.opt.bFlowDot      = (IsDlgButtonChecked(IDC_CFG_FLOWDOT)   == BST_CHECKED);
    tmp.opt.bGridDot      = (IsDlgButtonChecked(IDC_CFG_GRIDDOT)   == BST_CHECKED);
    tmp.opt.bDrawGrid     = (IsDlgButtonChecked(IDC_CFG_DRAWGRID)  == BST_CHECKED);
    tmp.opt.bDrawSep      = (IsDlgButtonChecked(IDC_CFG_DRAWSEP)   == BST_CHECKED);
    tmp.opt.bDrawFlow     = (IsDlgButtonChecked(IDC_CFG_DRAWFLOW)  == BST_CHECKED);
    tmp.opt.bDrawParamLine= (IsDlgButtonChecked(IDC_CFG_DRAWPARAM) == BST_CHECKED);
    tmp.opt.bShowParamName= (IsDlgButtonChecked(IDC_CFG_SHOWPARAM) == BST_CHECKED);
    g_ecTheme.opt = tmp.opt;
    PushToEditors();
}

// 「从文件导入(&F)」（存根页 id=1246）：把 .manifest 文件内容读进自定义清单编辑框
void CEcSysCfgDlg::OnManifestImport()
{
    CFileDialog dlg(TRUE, _T("manifest"), NULL,
                    OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
                    _T("清单文件(*.manifest)|*.manifest|所有文件(*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK)
        return;
    CFile f;
    if (!f.Open(dlg.GetPathName(), CFile::modeRead | CFile::typeBinary))
        return;
    DWORD n = (DWORD)f.GetLength();
    if (n > 65535) n = 65535;                      // ponytail: 编辑框装这么多够了
    char* p = new char[n + 1];
    if (p != NULL)
    {
        f.Read(p, n);
        p[n] = 0;
        SetDlgItemText(IDC_CFG_MANITEXT, CString(p));
        delete[] p;
    }
    f.Close();
    CheckRadioButton(IDC_CFG_MANIPRE, IDC_CFG_MANIUSER, IDC_CFG_MANIUSER);
}

// 「...」（编译页 id=1241）：挑签名证书文件
void CEcSysCfgDlg::OnSignBrowse()
{
    CFileDialog dlg(TRUE, _T("cer"), NULL,
                    OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
                    _T("证书文件(*.cer;*.pfx;*.p12)|*.cer;*.pfx;*.p12|所有文件(*.*)|*.*||"), this);
    if (dlg.DoModal() == IDOK)
        SetDlgItemText(IDC_CFG_SIGNCERT, dlg.GetPathName());
}

// ------------------------------------------------------------------ 确认 / 取消
void CEcSysCfgDlg::OnOK()
{
    EcTheme tmp = g_ecTheme;
    ReadCtrls(tmp);
    g_ecTheme.opt = tmp.opt;
    g_ecTheme.sScheme = tmp.sScheme;
    EcThemeSave();

    // 其余 6 个页签：全量读回 + 持久化 + 立刻生效（设计器网格/新建尺寸、编辑器撤消上限）
    EcCfgFromDlg(this);
    EcCfgSave();
    EcCfgApply();

    PushToEditors();
    CDialog::OnOK();
}

void CEcSysCfgDlg::OnCancel()
{
    // 预览时已经改过全局配色 -> 回滚到打开对话框时的状态
    g_ecTheme.ApplyNamed(m_sSchemeApplied);
    PushToEditors();
    CDialog::OnCancel();
}

// ------------------------------------------------------------------ 入口
BOOL EcShowSysCfg(CWnd* pParent)
{
    CEcSysCfgDlg dlg(pParent);
    return (dlg.DoModal() == IDOK);
}

// ============================================================================
// 「工具->支持库配置」（原版命令 #32906 / 模板 RT_DIALOG id=321）
//
// 原版那份对话框的模板在 shots\e_dialogs.txt（tools\dump_dialogs.py 倒的），
// 本工程的 IDD_LIBCFG 是照抄它的坐标/控件 ID，所以两者外形一模一样。
//
// 列表项的绘制（原版实测，shots\live_libcfg\item_zoom.png）：
//   · 左边一个 11x11 的方框：1px 灰(#808080)边框 + 白底；选中时方框画在蓝色高亮上
//   · 选中项整行蓝底(#0000FF 系 → 用 COLOR_HIGHLIGHT) + 白字；没选中的白底黑字
//   · 列表是 LBS_OWNERDRAWFIXED：勾选状态由**本对话框自己的数组**维护（原版也是这样，
//     它的 ListBox 风格里没有 LBS_MULTIPLESEL，勾是靠 WM_LBUTTONUP 逐项翻的）
// ----------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CEcLibCfgDlg, CDialog)
    ON_WM_DRAWITEM()
    ON_LBN_SELCHANGE(IDC_LC_LIST, OnSelChange)
    ON_EN_CHANGE(IDC_LC_SEARCH, OnSearchChange)
    ON_BN_CLICKED(IDC_LC_ALL, OnSelectAll)
    ON_BN_CLICKED(IDC_LC_NONE, OnSelectNone)
    ON_BN_CLICKED(IDC_LC_DEL, OnDelete)
    ON_BN_CLICKED(IDC_LC_BUY, OnBuy)
END_MESSAGE_MAP()

// 单击列表行 -> 翻转勾选（见 CEcCheckList 的注释：单选 ListBox 的重复单击不发通知）
LRESULT CEcCheckList::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_LBUTTONUP && m_pOwner != NULL)
        m_pOwner->OnListClick(lParam);
    return CListBox::WindowProc(message, wParam, lParam);
}

void CEcLibCfgDlg::OnListClick(LPARAM lParam)
{
    CPoint pt((DWORD)lParam);               // 低字 x、高字 y（客户区坐标）
    DWORD r = (DWORD)m_lst.SendMessage(LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
    int idx = (int)LOWORD(r);
    if (HIWORD(r) != 0 || idx < 0 || idx >= m_items.GetSize())
        return;
    m_lst.SetCurSel(idx);
    FillDetails(idx);
    Toggle(idx);
}

static CString g_szLibDirForCfg;        // 对话框打开期间的支持库目录

CEcLibCfgDlg::CEcLibCfgDlg(CWnd* pParent)
    : CDialog(CEcLibCfgDlg::IDD, pParent), m_bAll(FALSE), m_bDirty(FALSE)
{
}

CString EcFileBase(LPCTSTR pszPath)
{
    CString s = pszPath;
    int p = s.ReverseFind(_T('\\'));
    if (p >= 0)
        s = s.Mid(p + 1);
    int d = s.ReverseFind(_T('.'));
    if (d > 0)
        s = s.Left(d);
    return s;
}

// 已载入的库用支持库元数据里的"名字 + 版本"，没载入的只有文件名
CString EcLibDisplayName(LPCTSTR pszPath)
{
    int i;
    for (i = 0; i < EcLibCount(); i++)
    {
        const EcLib& L = EcLibAt(i);
        if (L.path.CompareNoCase(pszPath) != 0)
            continue;
        CString s;
        s.Format(_T("%s %d.%d 版"), (LPCTSTR)L.name, L.maj, L.min);
        return s;
    }
    return EcFileBase(pszPath);
}

BOOL CEcLibCfgDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_lst.m_pOwner = this;                  // 单击翻转勾选的回调目标（见 CEcCheckList）
    m_lst.SubclassDlgItem(IDC_LC_LIST, this);
    Reload(_T(""));
    return TRUE;
}

// ⚠️ ListBox 是单选风格（原版模板里没有 LBS_MULTIPLESEL），点同一行不会再发 LBN_SELCHANGE，
//    所以"单击翻转勾选"只能自己拦鼠标消息 —— 见 CEcCheckList（挂在 ListBox 的窗口过程上）。

// 目录里所有 .fne（核心库 krnln 不进列表 —— 原版那份列表里也没有它，它永远载入）
void CEcLibCfgDlg::Reload(LPCTSTR pszFilter)
{
    CStringArray files;
    EcScanLibFiles(g_szLibDirForCfg, &files);
    m_items.RemoveAll();
    m_used.RemoveAll();
    m_lst.ResetContent();

    CString filt = pszFilter;
    filt.MakeLower();
    int i;
    for (i = 0; i < files.GetSize(); i++)
    {
        CString base = EcFileBase(files[i]);
        CString low = base;
        low.MakeLower();
        if (low == _T("krnln"))
            continue;                       // 核心库不列（永远载入）
        if (!filt.IsEmpty() && low.Find(filt) < 0)
            continue;                       // 查找框：只按名字子串过滤
        EcLibCfgItem it;
        it.path = files[i];
        it.base = base;
        it.loaded = EcLibIsLoaded(files[i]);
        it.name = it.loaded ? EcLibDisplayName(files[i]) : base;
        m_items.Add(it);
        m_used.Add(it.loaded ? 1 : 0);
        m_lst.AddString(it.name);           // 实际不显示（ownerdraw），但要占位
    }
    if (m_items.GetSize() > 0)
        m_lst.SetCurSel(0);
    FillDetails(m_lst.GetCurSel());
    m_bDirty = FALSE;
}

// 详情页：载入过的走支持库元数据；没载入的给出文件信息 + 一句"勾选后载入"。
// ⚠️ 原版对**没载入**的库也能显示名字/版本/数字签名，那是它安装时写进注册表的；
//    本工程不读那个注册表，也**故意不**为了显示去 LoadLibrary（实测 spec.fne /
//    shellEx.fne 的 GetNewInf 会直接把进程打成 0xC0000005，见 HANDOFF 坑 3.5-2）。
void CEcLibCfgDlg::FillDetails(int idx)
{
    CString s;
    if (idx < 0 || idx >= m_items.GetSize())
    {
        SetDlgItemText(IDC_LC_DETAILS, _T(""));
        return;
    }
    const EcLibCfgItem& it = m_items[idx];
    int li = -1;
    int i;
    for (i = 0; i < EcLibCount(); i++)
        if (EcLibAt(i).path.CompareNoCase(it.path) == 0)
        {
            li = i;
            break;
        }
    if (li >= 0)
    {
        const EcLib& L = EcLibAt(li);
        s.Format(_T("相关文件：\r\n%s\r\n\r\n支持库名称及版本：%s %d.%d#%d 版\r\n")
                 _T("提供了 %d 种库定义数据类型，提供了 %d 种命令，提供了 %d 个库定义常量。\r\n"),
                 (LPCTSTR)L.path, (LPCTSTR)L.name, L.maj, L.min, L.build,
                 L.nDataType, L.nCmd, L.nConst);
        if (!L.explain.IsEmpty())
        {
            s += _T("\r\n说明：\r\n");
            s += L.explain;
            s += _T("\r\n");
        }
        if (!L.author.IsEmpty()) { s += _T("\r\n作者："); s += L.author; }
        if (!L.email.IsEmpty())  { s += _T("\r\n电子信箱："); s += L.email; }
        if (!L.home.IsEmpty())   { s += _T("\r\n主页地址："); s += L.home; }
    }
    else
    {
        s.Format(_T("相关文件：\r\n%s\r\n\r\n（本库当前未载入：勾选它并点“确认”即可动态载入，"
                    "不需要重启本程序。）\r\n"), (LPCTSTR)it.path);
    }
    SetDlgItemText(IDC_LC_DETAILS, s);
}

// 勾选/取消勾选（原版是单击整行翻转；双击也算）
void CEcLibCfgDlg::Toggle(int idx)
{
    if (idx < 0 || idx >= m_used.GetSize())
        return;
    m_used[idx] = m_used[idx] ? 0 : 1;
    m_bDirty = TRUE;
    m_lst.Invalidate(FALSE);
}

void CEcLibCfgDlg::OnSelChange()
{
    FillDetails(m_lst.GetCurSel());
}

void CEcLibCfgDlg::OnSearchChange()
{
    CString s;
    GetDlgItemText(IDC_LC_SEARCH, s);
    Reload(s);
}

void CEcLibCfgDlg::OnSelectAll()
{
    m_bAll = TRUE;
    int i;
    for (i = 0; i < m_used.GetSize(); i++)
        m_used[i] = 1;
    m_bDirty = TRUE;
    m_bAll = FALSE;
    m_lst.Invalidate(FALSE);
}

void CEcLibCfgDlg::OnSelectNone()
{
    m_bAll = TRUE;
    int i;
    for (i = 0; i < m_used.GetSize(); i++)
        m_used[i] = 0;
    m_bDirty = TRUE;
    m_bAll = FALSE;
    m_lst.Invalidate(FALSE);
}

// 原版的「删除」是卸载一个已安装的支持库（要动安装目录/注册表）。
// 本工程不碰安装目录，只把这一项从"本次要载入"里去掉（= 卸载），并说明清楚。
void CEcLibCfgDlg::OnDelete()
{
    int idx = m_lst.GetCurSel();
    if (idx < 0 || idx >= m_items.GetSize())
        return;
    CString msg;
    msg.Format(_T("把支持库「%s」从当前程序里卸载（从本程序的支持库列表中去掉）吗？\r\n")
               _T("（只改本程序载入哪些库，不会删除磁盘上的 .fne 文件。）"),
               (LPCTSTR)m_items[idx].name);
    if (AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    m_used[idx] = 0;
    m_bDirty = TRUE;
    m_lst.Invalidate(FALSE);
}

void CEcLibCfgDlg::OnBuy()
{
    AfxMessageBox(_T("本工程是界面复刻，不带购买/授权功能。"), MB_OK | MB_ICONINFORMATION);
}

// 确认：把勾选的库收集成"要载入的库名表"（小写基名，EcLoadLibs 按基名匹配）
void CEcLibCfgDlg::OnOK()
{
    m_result.RemoveAll();
    int i;
    for (i = 0; i < m_items.GetSize(); i++)
    {
        if (!m_used[i])
            continue;
        CString low = m_items[i].base;
        low.MakeLower();
        m_result.Add(low);
    }
    CDialog::OnOK();
}

// 列表项自绘（照原版实测的画法）
void CEcLibCfgDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS)
{
    if (nIDCtl != IDC_LC_LIST || lpDIS == NULL)
        return;
    int idx = lpDIS->itemID;
    if (idx < 0 || idx >= m_items.GetSize())
        return;

    CDC dc;
    dc.Attach(lpDIS->hDC);
    CRect rc = lpDIS->rcItem;
    BOOL bSel = (lpDIS->itemState & ODS_SELECTED) ? TRUE : FALSE;
    BOOL bCheck = m_used[idx] ? TRUE : FALSE;

    dc.FillSolidRect(rc, bSel ? ::GetSysColor(COLOR_HIGHLIGHT) : RGB(255, 255, 255));

    // 左边那个 11x11 的方框
    int bx = rc.left + 3, by = rc.top + (rc.Height() - 11) / 2;
    dc.FillSolidRect(bx, by, 11, 11, RGB(255, 255, 255));
    CPen pen(PS_SOLID, 1, RGB(128, 128, 128));
    CPen* pOld = dc.SelectObject(&pen);
    CBrush* pOldB = (CBrush*)dc.SelectStockObject(NULL_BRUSH);
    dc.Rectangle(bx, by, bx + 11, by + 11);
    dc.SelectObject(pOldB);
    if (bCheck)
    {
        CPen cpen(PS_SOLID, 2, RGB(0, 0, 0));
        CPen* po = dc.SelectObject(&cpen);
        dc.MoveTo(bx + 2, by + 5);
        dc.LineTo(bx + 4, by + 8);
        dc.LineTo(bx + 9, by + 2);
        dc.SelectObject(po);
    }
    dc.SelectObject(pOld);

    CRect rt = rc;
    rt.left = bx + 11 + 5;
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(bSel ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : RGB(0, 0, 0));
    dc.DrawText(m_items[idx].name, rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    dc.Detach();
}

// ------------------------------------------------------------------ 入口

// ---------------------------------------------------------------- 「寻找」对话框
// 原版模板 = RT_DIALOG id=284（5 个控件，见 src\ec_syscfg.rc 的 IDD_FIND）
CEcFindDlg::CEcFindDlg(CWnd* pParent) : CDialog(CEcFindDlg::IDD, pParent)
{
    m_bCase = FALSE;
}

void CEcFindDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_FIND_TEXT, m_sText);
    DDX_Check(pDX, IDC_FIND_CASE, m_bCase);
}

BEGIN_MESSAGE_MAP(CEcFindDlg, CDialog)
END_MESSAGE_MAP()

BOOL CEcFindDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    if (!m_sText.IsEmpty())
        SetDlgItemText(IDC_FIND_TEXT, m_sText);
    return TRUE;
}

void CEcFindDlg::OnOK()
{
    GetDlgItemText(IDC_FIND_TEXT, m_sText);
    m_bCase = (IsDlgButtonChecked(IDC_FIND_CASE) == BST_CHECKED) ? TRUE : FALSE;
    if (m_sText.IsEmpty())
        return;                     // 空串不关窗（也没什么可找的）
    m_sText.TrimLeft(); m_sText.TrimRight();
    if (m_sText.IsEmpty())
        return;
    CDialog::OnOK();
}


// ---------------------------------------------------------------- 「寻找替换」对话框
// 原版模板 = RT_DIALOG id=578（10 个控件，见 src\ec_syscfg.rc 的 IDD_REPLACE）。
// 简化说明：原版这个框是**非模态**的（点「搜寻下一个」不关窗）；本工程先用 DoModal，
// 点任一按钮就返回按钮 id、由调用方去执行（HANDOFF §6 记了这条差异）。
CEcReplDlg::CEcReplDlg(CWnd* pParent) : CDialog(CEcReplDlg::IDD, pParent)
{
    m_bCase = FALSE;
    m_nCmd = 0;
}

void CEcReplDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_REPL_TEXT, m_sFind);
    DDX_Text(pDX, IDC_REPL_WITH, m_sWith);
    DDX_Check(pDX, IDC_FIND_CASE, m_bCase);
}

BEGIN_MESSAGE_MAP(CEcReplDlg, CDialog)
    ON_BN_CLICKED(IDC_REPL_DO, OnReplaceOne)
    ON_BN_CLICKED(IDC_REPL_ALL, OnReplaceAll)
END_MESSAGE_MAP()

BOOL CEcReplDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    if (!m_sFind.IsEmpty()) SetDlgItemText(IDC_REPL_TEXT, m_sFind);
    if (!m_sWith.IsEmpty()) SetDlgItemText(IDC_REPL_WITH, m_sWith);
    return TRUE;
}

// 三个按钮共用的收尾：把控件里的文本/勾选读回来再关窗（返回值 = 按钮 id）
void CEcReplDlg::OnFindNext()
{
    GetDlgItemText(IDC_REPL_TEXT, m_sFind);
    GetDlgItemText(IDC_REPL_WITH, m_sWith);
    m_bCase = (IsDlgButtonChecked(IDC_FIND_CASE) == BST_CHECKED) ? TRUE : FALSE;
    if (m_sFind.IsEmpty()) return;
    m_nCmd = IDOK;
    EndDialog(IDOK);
}

void CEcReplDlg::OnReplaceOne()
{
    GetDlgItemText(IDC_REPL_TEXT, m_sFind);
    GetDlgItemText(IDC_REPL_WITH, m_sWith);
    m_bCase = (IsDlgButtonChecked(IDC_FIND_CASE) == BST_CHECKED) ? TRUE : FALSE;
    if (m_sFind.IsEmpty()) return;
    m_nCmd = IDC_REPL_DO;
    EndDialog(IDC_REPL_DO);
}

void CEcReplDlg::OnReplaceAll()
{
    GetDlgItemText(IDC_REPL_TEXT, m_sFind);
    GetDlgItemText(IDC_REPL_WITH, m_sWith);
    m_bCase = (IsDlgButtonChecked(IDC_FIND_CASE) == BST_CHECKED) ? TRUE : FALSE;
    if (m_sFind.IsEmpty()) return;
    m_nCmd = IDC_REPL_ALL;
    EndDialog(IDC_REPL_ALL);
}


// ---------------------------------------------------------------- 「书签跳转」对话框
// 原版模板 = RT_DIALOG id=607（5 个控件，见 src\ec_syscfg.rc 的 IDD_BOOKMARK）。
static CECEditor* g_pBmEd = NULL;        // 本工程既有风格：进出对话框用文件级静态传参（同 g_szLibDirForCfg）

CEcBmDlg::CEcBmDlg(CWnd* pParent) : CDialog(CEcBmDlg::IDD, pParent)
{
}

BEGIN_MESSAGE_MAP(CEcBmDlg, CDialog)
    ON_BN_CLICKED(IDC_BM_DEL, OnDel)
    ON_BN_CLICKED(IDC_BM_DELALL, OnDelAll)
END_MESSAGE_MAP()

static void EcBmFillList(CListCtrl& list, CECEditor* pEd)
{
    list.DeleteAllItems();
    if (pEd == NULL) return;
    int n = pEd->BookmarkCount();
    for (int i = 0; i < n; i++)
    {
        int L = pEd->BookmarkAt(i);
        if (L < 0) continue;
        CString sNo; sNo.Format(_T("%d"), L + 1);
        int k = list.InsertItem(list.GetItemCount(), sNo);
        list.SetItemText(k, 1, pEd->LineTextOf(L));
    }
}

BOOL CEcBmDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_BM_LIST);
    if (pList != NULL)
    {
        pList->InsertColumn(0, _T("行号"), LVCFMT_RIGHT, 60);
        pList->InsertColumn(1, _T("内容"), LVCFMT_LEFT, 300);
        EcBmFillList(*pList, g_pBmEd);
        if (pList->GetItemCount() > 0)
            pList->SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    return TRUE;
}

void CEcBmDlg::OnOK()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_BM_LIST);
    if (pList == NULL || g_pBmEd == NULL) { CDialog::OnOK(); return; }
    POSITION pos = pList->GetFirstSelectedItemPosition();
    if (pos == NULL) return;                 // 没选就不关窗
    int k = pList->GetNextSelectedItem(pos);
    CString sNo = pList->GetItemText(k, 0);
    int nLine = _ttoi(sNo) - 1;              // 列表里显示的是 1 基行号
    if (nLine >= 0) EndDialog(nLine + 1);    // 用行号+1 当返回值，0 = 取消
}

void CEcBmDlg::OnDel()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_BM_LIST);
    if (pList == NULL || g_pBmEd == NULL) return;
    POSITION pos = pList->GetFirstSelectedItemPosition();
    if (pos == NULL) return;
    int k = pList->GetNextSelectedItem(pos);
    int nLine = _ttoi(pList->GetItemText(k, 0)) - 1;
    g_pBmEd->RemoveBookmarkLine(nLine);
    EcBmFillList(*pList, g_pBmEd);
}

void CEcBmDlg::OnDelAll()
{
    if (g_pBmEd != NULL) g_pBmEd->ClearBookmarks();
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_BM_LIST);
    if (pList != NULL) EcBmFillList(*pList, g_pBmEd);
}

BOOL EcShowBookmarks(CWnd* pParent, CECEditor* pEd, int* pnLine)
{
    g_pBmEd = pEd;
    CEcBmDlg dlg(pParent);
    int n = (int)dlg.DoModal();
    g_pBmEd = NULL;
    if (n <= 0) return FALSE;
    if (pnLine != NULL) *pnLine = n - 1;
    return TRUE;
}

BOOL EcShowReplace(CWnd* pParent, CString* pFind, CString* pWith, BOOL* pbCase, int* pnCmd)
{
    CEcReplDlg dlg(pParent);
    if (pFind != NULL) dlg.m_sFind = *pFind;
    if (pWith != NULL) dlg.m_sWith = *pWith;
    if (pbCase != NULL) dlg.m_bCase = *pbCase;
    int n = (int)dlg.DoModal();
    if (n == IDCANCEL) return FALSE;
    if (pFind != NULL) *pFind = dlg.m_sFind;
    if (pWith != NULL) *pWith = dlg.m_sWith;
    if (pbCase != NULL) *pbCase = dlg.m_bCase;
    if (pnCmd != NULL) *pnCmd = dlg.m_nCmd;
    return TRUE;
}

BOOL EcShowFind(CWnd* pParent, CString* pText, BOOL* pbCase)
{
    CEcFindDlg dlg(pParent);
    if (pText != NULL) dlg.m_sText = *pText;
    if (pbCase != NULL) dlg.m_bCase = *pbCase;
    if (dlg.DoModal() != IDOK)
        return FALSE;
    if (pText != NULL) *pText = dlg.m_sText;
    if (pbCase != NULL) *pbCase = dlg.m_bCase;
    return TRUE;
}

BOOL EcShowLibCfg(CWnd* pParent, LPCTSTR pszLibDir, CStringArray* pResult)
{
    if (pszLibDir == NULL)
        return FALSE;
    g_szLibDirForCfg = pszLibDir;
    CEcLibCfgDlg dlg(pParent);
    if (dlg.DoModal() != IDOK)
        return FALSE;
    if (pResult != NULL)
    {
        pResult->RemoveAll();
        int i;
        for (i = 0; i < dlg.Result().GetSize(); i++)
            pResult->Add(dlg.Result()[i]);
    }
    return TRUE;
}

// ---------------------------------------------------------------- 「整体搜寻」对话框
// 原版模板 = RT_DIALOG id=30773（12 个控件，见 src\ec_syscfg.rc 的 IDD_WHOLEFIND）。
// 本对话框只负责取值；真正的扫描在 CECChildFrame::DoWholeFind（ec_editor 的源码上跑）。
//
// 两个下拉的条目原文**没抓到基准**（模板里 combo 是空的，条目是运行时填的），
// 所以这里按控件的语义自己填（见 HANDOFF §6 的存疑条目）。
static const TCHAR* const g_szWfScopes[] = {
    _T("整个程序"), _T("当前程序集"), _T("当前子程序"), _T("当前窗口")
};

CEcWholeFindDlg::CEcWholeFindDlg(CWnd* pParent) : CDialog(CEcWholeFindDlg::IDD, pParent)
{
    m_bCase = FALSE;
    m_bExpand = FALSE;
    m_nPane = 0;
    m_sScope = g_szWfScopes[0];
}

BEGIN_MESSAGE_MAP(CEcWholeFindDlg, CDialog)
END_MESSAGE_MAP()

BOOL CEcWholeFindDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    if (!m_sText.IsEmpty())
        SetDlgItemText(IDC_WF_TEXT, m_sText);

    CComboBox* pScope = (CComboBox*)GetDlgItem(IDC_WF_SCOPE);
    if (pScope != NULL)
    {
        for (int i = 0; i < (int)(sizeof(g_szWfScopes) / sizeof(g_szWfScopes[0])); i++)
            pScope->AddString(g_szWfScopes[i]);
        int k = pScope->FindStringExact(-1, m_sScope);
        pScope->SetCurSel(k >= 0 ? k : 0);
    }
    CheckDlgButton(IDC_WF_CASE,   m_bCase   ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_WF_EXPAND, m_bExpand ? BST_CHECKED : BST_UNCHECKED);
    CheckRadioButton(IDC_WF_PANE1, IDC_WF_PANE2, m_nPane == 1 ? IDC_WF_PANE2 : IDC_WF_PANE1);
    return TRUE;
}

void CEcWholeFindDlg::OnOK()
{
    GetDlgItemText(IDC_WF_TEXT, m_sText);
    m_sText.TrimLeft(); m_sText.TrimRight();
    if (m_sText.IsEmpty())
        return;                         // 空串不关窗
    CComboBox* pScope = (CComboBox*)GetDlgItem(IDC_WF_SCOPE);
    if (pScope != NULL)
    {
        CString s;
        pScope->GetWindowText(s);
        if (!s.IsEmpty()) m_sScope = s;
    }
    m_bCase   = (IsDlgButtonChecked(IDC_WF_CASE)   == BST_CHECKED) ? TRUE : FALSE;
    m_bExpand = (IsDlgButtonChecked(IDC_WF_EXPAND) == BST_CHECKED) ? TRUE : FALSE;
    m_nPane   = (IsDlgButtonChecked(IDC_WF_PANE2)  == BST_CHECKED) ? 1 : 0;
    CDialog::OnOK();
}

BOOL EcShowWholeFind(CWnd* pParent, CString* psText, BOOL* pbCase, int* pnPane)
{
    CEcWholeFindDlg dlg(pParent);
    if (psText != NULL) dlg.m_sText = *psText;
    if (pbCase != NULL) dlg.m_bCase = *pbCase;
    if (dlg.DoModal() != IDOK)
        return FALSE;
    if (psText != NULL) *psText = dlg.m_sText;
    if (pbCase != NULL) *pbCase = dlg.m_bCase;
    if (pnPane != NULL) *pnPane = dlg.m_nPane;
    return TRUE;
}

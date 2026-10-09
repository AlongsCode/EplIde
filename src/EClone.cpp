// EClone.cpp -- 易语言 5.9 IDE 界面复刻
// M1: 主框架(CMDIFrameWnd) + 原生菜单(照抄 e.exe 的 RT_MENU) + 状态栏 + MDI
// M2/M3: 四个停靠面板（用 MFC 标准 dock id，与原版一致）+ 分页内容
// M4/M5: 面板改用 sizecbar(CSizingControlBarG) —— 原版面板窗口类
//        Afx:400000:8:10003:900015:0 正是 sizecbar 的 AfxRegisterWndClass 产物，
//        标题行的"□ / ×"是易语言在 sizecbar 上自己加的一个按钮（原版 bitmaps 595/596 就是这两个字形）
// 目标: 外观与控件层级 1:1，不含编译器功能。
#include <afxwin.h>
#include <afxext.h>
#include <afxcmn.h>
#include "resource.h"
// ⚠️ 组件按钮个数必须是个**编译期字面量**（VC6 不允许用它做数组维度，会报
//    C2057 expected constant expression / C2229 illegal zero-sized array）。
//    resource.h 里也有一个同名的 40，这里再定义一份只是为了让数组维度可用；
//    两处必须是同一个数。改个数记得**两边一起改**。
#define EC_COMP_BTN_N 40
#include "sizecbar.h"
#include "scbarg.h"
#include "ec_editor.h"
#include "ec_design.h"
#include "ec_lib.h"
#include "ec_theme.h"
#include "ec_syscfg.h"
#include "ec_eproj.h"

// 支持库树选中 -> 主框架状态栏显示提示（原版就是这个行为：提示写在 msctls_statusbar32 里）
#define EC_WM_HINT      (WM_USER + 111)
// 「工作夹->程序」树选中 -> MDI 子窗口切到对应文档页（分页导航）
#define EC_WM_OPENDOC   (WM_USER + 112)   // wParam = 文档下标，lParam = 窗体下标（-1/0 = 当前窗体）
// 「工作夹->支持库」树里选中一个**可视组件**类型 -> 让设计器进入放置模式
// （wParam = 支持库下标，lParam = 库内数据类型下标；wParam = -1 表示取消放置）
#define EC_WM_ARMUNIT   (WM_USER + 113)

// 文档页下标（MDI 子窗口的标签条 + 「程序」树的 item data 共用）
// 原版每点一个程序树节点就打开一个**独立 MDI 文档**（实测窗口树里会多出一个
// Afx:400000:b 子窗口，标题 = 页名），本工程用一个编辑器 + 页类别（ECK_*）代替。
#define ECDOC_FORM  0
#define ECDOC_ASM   1
#define ECDOC_DLL   2
#define ECDOC_GVAR  3
#define ECDOC_TYPE  4
#define ECDOC_CONST 5
#define ECDOC_IMG   6
#define ECDOC_SND   7
#define ECDOC_COUNT 8

// 「程序」页树的节点类型 = res\progtree_icons.bmp 的槽位号
// （原版 e.exe 里没有这组图标，从原版 1:1 截图抠的，见 tools\extract_progtree_icons.py）
#define EP_ROOT      0
#define EP_ASM       1      // 程序集（菱形）。原版实测：窗口程序集与普通程序集**同一个图标**（img=3）
#define EP_FOLDER    2      // 文件夹（黄色）。原版实测 img=16，是 IDE 里建的分类节点
#define EP_SUB       3
#define EP_FORM      4
#define EP_WINDOW    5
#define EP_CONST     6
#define EP_RESOURCE  7
#define EP_RES_SOUND 8
#define EP_RES_IMAGE 9
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
    _T(".程序集 启动窗口程序集\r\n")
    _T("\r\n")
    _T(".子程序 _图形按钮1_被单击\r\n")
    _T("    信息框（“易语言中国人之编程！”, 0, ）");































static const TCHAR* g_szSampleDll =
    _T(".版本 2\r\n")
    _T("\r\n")
    _T(".DLL命令 ZwWow64ReadVirtualMemory64, 整数型, \"ntdll.dll\", \"ZwWow64ReadVirtualMemory64\"\r\n")
    _T("    .参数 ProcessHandle, 整数型, , , 对象句柄\r\n")
    _T("    .参数 BaseAddress, 长整数型, , , 内存地址\r\n")
    _T("    .参数 Buffer, 字节集, , , 返回缓冲区\r\n")
    _T("    .参数 BufferSize, 长整数型, , , 写入大小\r\n")
    _T("    .参数 NumberOfBytesRead, 长整数型, 传址, , 实际写入大小\r\n")
    _T("\r\n")
    _T(".DLL命令 ZwWow64WriteVirtualMemory64, 整数型, \"ntdll.dll\", \"ZwWow64WriteVirtualMemory64\"\r\n")
    _T("    .参数 ProcessHandle, 整数型, , , 对象句柄\r\n")
    _T("    .参数 BaseAddress, 长整数型, , , 内存地址\r\n")
    _T("    .参数 Buffer, 字节集, , , 返回缓冲区\r\n")
    _T("    .参数 BufferSize, 长整数型, , , 写入大小\r\n")
    _T("    .参数 NumberOfBytesRead, 长整数型, 传址, , 实际写入大小\r\n")
    _T("\r\n")
    _T(".DLL命令 CopyMemory_DRAWITEMSTRUCT, , \"kernel32.dll\", \"RtlMoveMemory\"\r\n")
    _T("    .参数 Destination, DRAWITEMSTRUCT, 传址, , 目标\r\n")
    _T("    .参数 Source, DRAWITEMSTRUCT, 传址, , 源\r\n")
    _T("    .参数 Length, 整数型, , , 长度\r\n");

static BOOL    g_bEProject = FALSE;
static CString g_strEProgram;    // 生成的程序页源码
static CString g_strEDll;        // 生成的 Dll 命令页源码（暂无，留空）

static const TCHAR* EcSrcProg()
{
    return g_bEProject ? (LPCTSTR)g_strEProgram : g_szSampleCode;
}

static const TCHAR* EcSrcDll()
{
    return g_bEProject ? (LPCTSTR)g_strEDll : g_szSampleDll;
}

static BOOL g_bStartDesigner = FALSE;   // 命令行带 designer 时先显示设计器（便于截图对比）
static BOOL g_bExitAfterInit = FALSE;   // 命令行带 exit 时，建完窗口排完版就退出（自动化取模型用）

// ---------------------------------------------------------------------------
// 命令行带一个真 .e 工程文件时，打开的是**工程本身**而不是采样源码。
// 做法：把 .e 解析出来的东西生成成 eclone 自己的文本源码形式（见 ec_eproj.cpp），
// 然后照常喂给 EcBuildProgTree + m_editor.SetText —— 渲染链路一行都不用改。
// 判据：原版 e.exe 打开同一个 .e 时的树/表格内容。见 D:\EPL\HANDOFF.md。
// ⚠️ 现在只生成**声明层**（程序集/子程序/参数/变量/全局变量/数据类型/常量/DLL命令）。
//    子程序正文（可执行语句）还没生成 —— .e 里它是字节码，语义未解，见 HANDOFF §5.3。

// 调试用：把 .e 生成的文本原样落盘（ECLONE_DUMP_EPROJ=<路径>），便于逐字核对
static void EcDumpEProjectIfAsked(LPCTSTR pszPath)
{
    TCHAR szOut[MAX_PATH * 2];
    if (::GetEnvironmentVariable(_T("ECLONE_DUMP_EPROJ"), szOut, MAX_PATH * 2) <= 0)
        return;
    CString s;
    if (!EcLoadEProject(pszPath, s))
        s = _T("<<EcLoadEProject failed>>");
    CStdioFile f;
    CFileException e;
    if (!f.Open(szOut, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary, &e))
        return;
    // 以 GBK 原字节写盘（本工程文本本来就是 GBK）
    f.Write(s, (UINT)s.GetLength());
    f.Close();
}

// 从命令行里挑出 .e 路径（本工程只认第一个以 .e 结尾的参数）
// ⚠️ 不要自己切 m_lpCmdLine：带空格的路径（"D:\...\Basic Sample\Bubble Sort.e"）
//    MFC 的 m_lpCmdLine 里没有引号，自己按空格切会切成 "Sort.e"（实测踩过）。
//    用 CRT 已经在启动时切好的 __argv —— 它按标准规则处理引号与空格，
//    路径要带空格写引号即可（shell 负责去掉引号）。
static BOOL EcFindEArg(CString& strOut)
{
    for (int i = 1; i < __argc; i++)
    {
        if (__argv[i] == NULL) continue;
        CString tok = CString(__argv[i]);      // ANSI/系统代码页
        if (tok.GetLength() > 2 && tok.Right(2).CompareNoCase(_T(".e")) == 0)
        {
            strOut = tok;
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL FindLibDir(CString& out);   // 定义在下面「面板内容」一节

// 面板窗口 ID 与原版一致：工作夹 129 / 状态夹 130 / 窗口组件箱 110
static const TCHAR* g_szWorkTabs[]   = { _T("支持库"), _T("程序"), _T("属性") };
static const TCHAR* g_szStatusTabs[] = { _T("提示"), _T("输出"), _T("调用表"), _T("监视板"),
                                         _T("变量表"), _T("搜寻1"), _T("搜寻2"), _T("剪辑历史") };

// 标签页图标：取自 e.exe 的 RT_BITMAP（工作夹 146 / 状态夹 148，各 7 个 16x16 图标）
// 映射是按原版截图逐个对出来的：状态夹 = 提示/输出/调用表/监视板/变量表/搜寻1/搜寻2/剪辑历史
static const int g_nWorkTabIcons[]   = { 0, 1, 2 };
static const int g_nStatusTabIcons[] = { 0, 1, 5, 4, 3, 4, 4, 6 };

// ------------------------------------------------------------------ 面板内层对话框
// 原版面板层级：Afx:400000:8...(sizecbar 窗口) -> #32770 -> SysTabControl32 -> 内容
// 这里照做：模板用 panes.rc，运行时把 #32770 撑到 pane 的客户区（模板的 DLU 尺寸不作数）
class CPaneHost : public CWnd
{
    DECLARE_DYNAMIC(CPaneHost)
public:
    BOOL CreateFromTemplate(UINT nIDTemplate, CWnd* pParent);

protected:
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()
};

IMPLEMENT_DYNAMIC(CPaneHost, CWnd)

BEGIN_MESSAGE_MAP(CPaneHost, CWnd)
END_MESSAGE_MAP()

BOOL CPaneHost::CreateFromTemplate(UINT nIDTemplate, CWnd* pParent)
{
    HINSTANCE hInst = AfxFindResourceHandle(MAKEINTRESOURCE(nIDTemplate), RT_DIALOG);
    HRSRC hRes = ::FindResource(hInst, MAKEINTRESOURCE(nIDTemplate), RT_DIALOG);
    if (hRes == NULL)
        return FALSE;
    HGLOBAL hMem = ::LoadResource(hInst, hRes);
    if (hMem == NULL)
        return FALSE;
    LPCDLGTEMPLATE pTpl = (LPCDLGTEMPLATE)::LockResource(hMem);
    if (pTpl == NULL)
        return FALSE;

    HWND hWnd = ::CreateDialogIndirect(hInst, pTpl, pParent->GetSafeHwnd(), NULL);
    if (hWnd == NULL)
        return FALSE;
    if (!SubclassWindow(hWnd))
    {
        ::DestroyWindow(hWnd);
        return FALSE;
    }
    return TRUE;
}

// 通知(WM_NOTIFY/WM_COMMAND)是发给"控件的父窗口"的；控件挂在 #32770 下，
// 所以要转给 pane，pane 的 ON_NOTIFY(TCN_SELCHANGE) 才收得到。
// ⚠️ **WM_DRAWITEM 也一样**：owner-draw 的按钮把画自己的请求发给**直接父窗口**（#32770），
//    不转给 pane 的话 `ON_WM_DRAWITEM` 永远不会被调到 —— 现象是"按钮在窗口树里可见、
//    尺寸也对，就是一片灰什么都不画"（本轮踩过）。
LRESULT CPaneHost::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NOTIFY || message == WM_COMMAND ||
        message == WM_DRAWITEM || message == WM_MEASUREITEM || message == WM_COMPAREITEM)
    {
        CWnd* pPane = GetParent();
        if (pPane != NULL && pPane->GetSafeHwnd() != NULL)
            return pPane->SendMessage(message, wParam, lParam);
    }
    return CWnd::WindowProc(message, wParam, lParam);
}

// ------------------------------------------------------------------ 通用停靠面板
// 组件图标缓存项（见 CPaneBar::EcCompIcon 的说明：不能叫 EcCompIcon，会和成员函数撞名）。
struct EcCompIconRec { int lib; DWORD bmpId; HICON hIcon; };

class CPaneBar : public CSizingControlBarG
{
    DECLARE_DYNAMIC(CPaneBar)
public:
    CPaneBar();

    // nDockSize: 左侧面板=宽(原版 229) / 底部面板=高(原版 188)
    BOOL CreatePane(CWnd* pParent, UINT nIDTemplate, UINT nID, UINT nTabID,
                    const TCHAR** pszTabs, const int* pnIcons, int nTabs, UINT nFirstPageID,
                    int nDockSize, BOOL bDockBottom, BOOL bTabsBottom, UINT nIconBitmap);

    CWnd* GetPaneItem(UINT nID) { return m_host.GetDlgItem(nID); }

    // 窗口组件箱：组件按钮**从已载入的支持库来**（不写死表）。
    // 原版那 6 个组件组（实测字符串表，见 HANDOFF §7 的原版字符串）：
    //   0 基本组件（核心库）/ 1 扩展组件 / 2 外部组件 / 3 COM包装库的 ActiveX 组件 /
    //   4 外部事件组件 / 5 COM包装库的 COM 事件组件 / 6 扩展支持库内的组件 / 7 核心支持库内的组件
    // 本工程按**库的 LBS_* 状态位**把它们分到组 0..5（没有 COM 包装库，所以 3/5 空着）。
    void FillComponentBox(int nGroup);
    int  CompGroupOfLib(int lib) const;
    // 组件图标 = 支持库 .fne 里的 RT_BITMAP（资源号 = EcDataType::bmpId）。
    // 一张图可能被多个类型共用，所以按 (库,资源号) 去重，返回 HICON（由 m_compIcons 持有）。
    // ⚠️ 这个结构体原来嵌在这里，但同名的成员函数 `EcCompIcon` 会让 VC6 在成员函数体里
    //    找不到这个类型名（报 `C2146: syntax error : missing ';' before identifier 'e'`），
    //    所以挪到文件作用域并改名 EcCompIconRec（见本文件上方）。
    CArray<EcCompIconRec, EcCompIconRec&> m_compIcons;
    HICON EcCompIcon(int lib, DWORD bmpId);
    // 当前显示的组 + 每个按钮挂的 (库,类型)；-1 = 空按钮（隐藏）
    int   m_nCompGroup;
    int   m_nCompSel;                       // 当前选中的按钮下标（-1 = 无）
    int   m_nCompBtnLib[EC_COMP_BTN_N];
    int   m_nCompBtnDt[EC_COMP_BTN_N];
    void  LayoutCompButtons(int cx, int cy);
    void  DrawCompItem(LPDRAWITEMSTRUCT lp);

    // 「工作夹-属性」页：按支持库的属性元数据 + ITF 6 列属性；值走 ITF 4 改（双击就地编辑）
    void FillProps(int lib, int dt, void* hunit);

    // 外部（整体搜寻的结果输出）用：翻到第 n 页（设置页签 cursel + 换页显示）。
    // ⚠️ TCM_SETCURSEL 不会给父窗口发 TCN_SELCHANGE，所以必须自己再调一次 ShowPage。
    void GotoPage(int n);

protected:
    CPaneHost     m_host;
    CImageList    m_ilTabs;
    UINT          m_nTabID;
    UINT          m_nFirstPageID;
    const TCHAR** m_pTabs;
    const int*    m_pnIcons;
    int           m_nTabs;
    int           m_nCurPage;
    int           m_nTabH;        // 标签行高度（建完 tab 后按字号实测，原版 23）
    BOOL          m_bTabsBottom;
    CRect         m_rcMin;        // 标题行第二个按钮（□），坐标相对 pane 窗口
    BOOL          m_bCollapsed;
    int           m_nSavedSize;
    // 属性页
    int           m_propLib, m_propDt;
    void*         m_propHunit;
    CArray<int, int> m_propIdx;   // 列表行 -> 属性下标
    CEdit         m_propEdit;
    int           m_propEditRow;
    void CommitPropEdit(BOOL bSave);

    void LayoutPages();
    void ShowPage(int n);
    void ToggleCollapse();
    int  TabHeight() const { return m_nTabH; }

    virtual void NcCalcClient(LPRECT pRc, UINT nDockBarID);
    virtual void NcPaintGripper(CDC* pDC, CRect rcClient);
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnNcLButtonUp(UINT nHitTest, CPoint point);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLibTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnProgTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnCompBtn(UINT nID);      // 组件按钮（owner-draw）被点
    afx_msg void OnCompGroup();            // 组件组下拉框选择变了
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lp);     // owner-draw 画组件按钮
    afx_msg void OnPropDblClk(NMHDR* pNMHDR, LRESULT* pResult);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    DECLARE_MESSAGE_MAP()
};

IMPLEMENT_DYNAMIC(CPaneBar, CSizingControlBarG)

BEGIN_MESSAGE_MAP(CPaneBar, CSizingControlBarG)
    ON_WM_SIZE()
    ON_WM_NCHITTEST()
    ON_WM_NCLBUTTONUP()
    // 每个面板的标签控件 ID 不同，必须逐条登记 —— ON_NOTIFY 的第 2 个参数是
    // 「控件 ID 范围」(nID..nLastID)，写 0 表示范围 0..0，任何控件都匹配不上，
    // 处理器永远不会被调用（这一条当年把「工作夹/状态夹的标签页点击」全废了）。
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_WORK, OnTabSelChange)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_STATUS, OnTabSelChange)
    ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_WORK, OnLibTreeSelChanged)
    ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_WORK + 1, OnProgTreeSelChanged)
    ON_NOTIFY(NM_DBLCLK, IDC_TREE_WORK + 2, OnPropDblClk)
    // 组件箱：第一个按钮是「组件组下拉框」（ListBox，原版是 SysTreeView32 的下拉），
    // 后面 40 个是 owner-draw 的组件图标按钮（id 2300..2339，见 resource.h）。
    // ⚠️ ON_COMMAND_RANGE 的参数必须是字面量常量，不能写表达式，所以这里直接写 2300/2339。
    ON_LBN_SELCHANGE(IDC_TAB_COMP, OnCompGroup)
    ON_COMMAND_RANGE(2300, 2339, OnCompBtn)
    // ⚠️ 组件按钮的 owner-draw 用 **ON_WM_DRAWITEM**（WM_DRAWITEM 的 wParam 不是控件 id，
    //    用 ON_CONTROL_RANGE 路由不到处理函数 —— 实测画不出任何东西）。
    ON_WM_DRAWITEM()
END_MESSAGE_MAP()

CPaneBar::CPaneBar()
    : m_nTabID(0), m_nFirstPageID(0), m_pTabs(NULL), m_pnIcons(NULL), m_nTabs(0),
      m_nCurPage(0), m_nTabH(23), m_bTabsBottom(FALSE), m_bCollapsed(FALSE), m_nSavedSize(0),
      m_propLib(-1), m_propDt(-1), m_propHunit(NULL), m_propEditRow(-1),
      m_nCompGroup(0), m_nCompSel(-1)
{
    // ⚠️ EcRow/EcCell 是 POD；这里的两个数组也必须显式初始化，否则第一个按钮会挂到
    //    栈/堆上的垃圾 (库,类型) 上（第 16 轮 bParam 就是这么崩的）。
    for (int i = 0; i < EC_COMP_BTN_N; i++)
        m_nCompBtnLib[i] = m_nCompBtnDt[i] = -1;
}

BOOL CPaneBar::CreatePane(CWnd* pParent, UINT nIDTemplate, UINT nID, UINT nTabID,
                          const TCHAR** pszTabs, const int* pnIcons, int nTabs,
                          UINT nFirstPageID, int nDockSize, BOOL bDockBottom,
                          BOOL bTabsBottom, UINT nIconBitmap)
{
    m_nTabID       = nTabID;
    m_pTabs        = pszTabs;
    m_pnIcons      = pnIcons;
    m_nTabs        = nTabs;
    m_nFirstPageID = nFirstPageID;
    m_bTabsBottom  = bTabsBottom;

    // 原版实测：工作夹宽 229 / 状态夹高 188；最小尺寸要放小才能收起
    if (bDockBottom)
        m_szHorz = CSize(200, nDockSize);
    else
        m_szVert = CSize(nDockSize, 200);
    m_szMinHorz  = CSize(20, 20);
    m_szMinVert  = CSize(20, 20);
    m_szMinFloat = CSize(37, 32);

    DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_TOOLTIPS | CBRS_SIZE_DYNAMIC |
                    (bDockBottom ? CBRS_BOTTOM : CBRS_LEFT);
    if (!Create(_T(""), pParent, nID, dwStyle))
        return FALSE;

    if (!m_host.CreateFromTemplate(nIDTemplate, this))
        return FALSE;

    // 标签页图标（与原版同一张位图）
    if (nIconBitmap != 0 && nTabs > 0)
    {
        CBitmap bmp;
        if (bmp.LoadBitmap(nIconBitmap))
        {
            if (m_ilTabs.Create(16, 16, ILC_COLOR24 | ILC_MASK, nTabs, 1))
                m_ilTabs.Add(&bmp, RGB(192, 192, 192));   // 位图里的透明色已导成 COLOR_BTNFACE
        }
    }

    if (nTabs > 0 && nTabID != 0)
    {
        CTabCtrl* pTab = (CTabCtrl*)m_host.GetDlgItem(nTabID);
        if (pTab != NULL)
        {
            if (m_ilTabs.GetSafeHandle() != NULL)
                pTab->SetImageList(&m_ilTabs);
            for (int i = 0; i < nTabs; i++)
            {
                TC_ITEM ti;
                memset(&ti, 0, sizeof(ti));
                ti.mask = TCIF_TEXT | TCIF_IMAGE;
                ti.pszText = (LPTSTR)pszTabs[i];
                ti.iImage = pnIcons ? pnIcons[i] : i;
                pTab->InsertItem(i, &ti);
            }
            pTab->SetCurSel(0);

            // 标签行高度按实际字体量（原版实测 23px = 标签项高 20 + 3）
            CRect rcItem;
            if (pTab->GetItemRect(0, &rcItem) && rcItem.Height() > 0)
                m_nTabH = rcItem.Height() + 3;
        }
        m_nCurPage = 0;
        ShowPage(0);
    }
    LayoutPages();
    return TRUE;
}

void CPaneBar::ShowPage(int n)
{
    // ⚠️ 标签控件在某些瞬间（重建/换页/尺寸变化）GetCurSel() 会返回 -1，
    //    不挡住的话这里会把**所有**页都 ShowWindow(SW_HIDE) —— 现象就是"面板内容整块消失、
    //    只剩标签页"，而且再也不会自己回来。这是防它。
    if (n < 0 || n >= m_nTabs)
        return;
    for (int i = 0; i < m_nTabs; i++)
    {
        CWnd* p = m_host.GetDlgItem(m_nFirstPageID + i);
        if (p != NULL)
            p->ShowWindow(i == n ? SW_SHOW : SW_HIDE);
    }
    m_nCurPage = n;
}

// 外部翻页入口（整体搜寻结果输出到「搜寻1/搜寻2」时用）
void CPaneBar::GotoPage(int n)
{
    CTabCtrl* pTab = (CTabCtrl*)m_host.GetDlgItem(m_nTabID);
    if (pTab == NULL || pTab->GetSafeHwnd() == NULL)
        return;
    pTab->SetCurSel(n);            // 不发 TCN_SELCHANGE，所以要自己 ShowPage
    ShowPage(n);
}

void CPaneBar::OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult)
{
    if (m_nTabs > 0 && pNMHDR->idFrom == m_nTabID)
    {
        CTabCtrl* pTab = (CTabCtrl*)m_host.GetDlgItem(m_nTabID);
        if (pTab != NULL)
            ShowPage(pTab->GetCurSel());
    }
    *pResult = 0;
}

// 「工作夹->支持库」树选中项 -> 提示文本 -> 主框架状态栏。
// 节点数据是 EcBuildTree() 生成的全局节点表下标（见 ec_lib.h）。
void CPaneBar::OnLibTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    if (pNMHDR->idFrom != IDC_TREE_WORK)
        return;

    CTreeCtrl* pTree = (CTreeCtrl*)m_host.GetDlgItem(IDC_TREE_WORK);
    if (pTree == NULL)
        return;
    HTREEITEM h = pTree->GetSelectedItem();
    if (h == NULL)
        return;

    CString hint = EcNodeHint((int)pTree->GetItemData(h));
    CFrameWnd* pFrame = GetParentFrame();
    if (pFrame != NULL && pFrame->GetSafeHwnd() != NULL)
        pFrame->SendMessage(EC_WM_HINT, 0, (LPARAM)(LPCTSTR)hint);
}

// 「工作夹->程序」树选中项 -> 让 MDI 子窗口切到对应文档页（原版点程序集/Dll命令 就换页）。
// item data = 文档下标（ECDOC_*），-1 表示这一类还没有对应的文档页。
void CPaneBar::OnProgTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    if (pNMHDR->idFrom != IDC_TREE_WORK + 1)
        return;
    CTreeCtrl* pTree = (CTreeCtrl*)m_host.GetDlgItem(IDC_TREE_WORK + 1);
    if (pTree == NULL)
        return;
    HTREEITEM h = pTree->GetSelectedItem();
    if (h == NULL)
        return;
    int nDoc = (int)pTree->GetItemData(h);
    // 「窗口」节点：高 16 位带窗体下标（0 = 第一个窗体 / _启动窗口）。见 EcBuildProgTree。
    // ⚠️ 别在这里加 EcTrace：它在 1200 行之后才声明（本文件里只有那一处 extern），会编译不过。
    int nForm = -1;
    if (nDoc >= 0)
    {
        int hi = (int)((DWORD)pTree->GetItemData(h) >> 16);
        nDoc &= 0xFFFF;
        if (nDoc == ECDOC_FORM)
            nForm = hi;              // 窗口节点：至少要切到第 0 个窗体（"切换窗体"的入口）
        if (nDoc == ECDOC_FORM && nForm < 0)
            nForm = 0;
    }
    if (nDoc < 0)
        return;
    CWnd* pMain = AfxGetMainWnd();
    if (pMain == NULL || pMain->GetSafeHwnd() == NULL)
        return;
    CMDIChildWnd* pChild = ((CMDIFrameWnd*)pMain)->MDIGetActive();
    if (pChild == NULL || pChild->GetSafeHwnd() == NULL)
        return;
    pChild->PostMessage(EC_WM_OPENDOC, (WPARAM)nDoc, (LPARAM)(nForm + 1));
}

// （定义在下面 PopulatePanes 之前）
static CControlBar* EcBarById(CFrameWnd* pFrame, UINT nID);

// 原版两个单选是「基本组件」(核心库) / 「扩展组件」(其它支持库)。
// ★ 第 25 轮：控件改成**图标网格**（SysListView32 + LVS_ICON），和原版一致；
//   每个组件的图标是**支持库 .fne 自己的 RT_BITMAP 资源**，资源号 = LIB_DATA_TYPE_INFO
//   +20 的 m_nBmpID（实测 krnln.fne：编辑框 261 / 标签 262 / 按钮 265 …，见
//   `ECLONE_DUMP_DTICON` 与 tools\pe_bmp_ids.py，两边逐个相等）。
//   所以图标用 `::LoadBitmap(EcLibModule(lib), MAKEINTRESOURCE(bmpId))` 直接取，不用抠图。
//
// ★ 第 26 轮：控件从 SysListView32 图标视图换成**原版那样的一格一按钮**
//   （原版实测：126x415 的 #32770 里 40 个 28x28 的 Button，4 列 x 10 行、列/行距 29，
//    见 tools\probe_compbox3.py 倒出的 dlg_children.txt）。按钮是 owner-draw，
//    自己把库里的 24x24 图标按 28x28 居中画出来（原版按钮上**没有文字**）。
//
// 组件组：原版那 6 个组名来自 e.exe 的字符串表（实测偏移相邻）：
//   扩展支持库内的组件/扩展组件、核心支持库内的组件/基本组件、外部组件、
//   COM包装支持库所封装的ActiveX组件、外部事件组件、COM包装支持库所封装的COM事件组件
// 本工程按库的 LBS_* 状态位分组（见 ec_lib.h 的 EcLib::state）：
//   0 基本组件   = 核心库 krnln（LBS_IS_SYSTEM_LIB）
//   1 扩展组件   = 其它支持库（LBS_IS_EXT_LIB）
//   2 外部组件   = 易模块/外部文件里的组件（本工程没有这类库，留空）
//   3 外部事件组件 = 同上留空
// 剩下的组名（COM 包装那两个）本工程不产生，所以不列。

// 组名（下标 = 组号，见上面的说明）
static LPCTSTR EcCompGroupName(int n)
{
    switch (n)
    {
    case 0: return _T("基本组件");
    case 1: return _T("扩展组件");
    case 2: return _T("外部组件");
    case 3: return _T("外部事件组件");
    default: return _T("");
    }
}

// 这个库属于哪个组件组。
// ⚠️ 判据**不能**用 LBS_IS_SYSTEM_LIB（1<<6）：实测本机核心库的 state = 0xC0000001，
//    第 6/7 位都是 0（lib2.h 里那个常量是别的含义）。用文件名认核心库 —— 这和本工程
//    别处认 krnln 的办法一致（`_T("krnln")`），实测可靠。
int CPaneBar::CompGroupOfLib(int lib) const
{
    if (lib < 0 || lib >= EcLibCount())
        return -1;
    const EcLib& L = EcLibAt(lib);
    if (L.path.Find(_T("krnln")) >= 0)
        return 0;                       // 基本组件 = 核心支持库
    return 1;                           // 其它 .fne 一律算"扩展组件"
}

// 取（并缓存）某个库的组件图标。库里这些位图是 **24x24、4bpp 带调色板**，所以不能自己
// BitBlt 到 32bpp 的 DC（调色板没选进去 → 全黑）；用 ::CreateIconIndirect 让系统按位图
// 自带的颜色表转换（这也是原版存图标的路子：它把 HBITMAP 转成 HICON 存进按钮的 lData）。
HICON CPaneBar::EcCompIcon(int lib, DWORD bmpId)
{
    if (bmpId == 0)
        return NULL;
    int i;
    for (i = 0; i < (int)m_compIcons.GetSize(); i++)
        if (m_compIcons[i].lib == lib && m_compIcons[i].bmpId == bmpId)
            return m_compIcons[i].hIcon;

    HINSTANCE hMod = EcLibModule(lib);
    if (hMod == NULL)
        return NULL;
    HBITMAP hbm = ::LoadBitmap(hMod, MAKEINTRESOURCE(bmpId));
    if (hbm == NULL)
        return NULL;

    // 位图 -> HICON：hbm 是 DIB，用 GetDIBits 取尺寸和像素；掩码那一位用 CreateBitmap
    // 建一张全 0（=全部不透明）的 1bpp 位图。图标背景色 = 按钮面色的近亲（原版图标就是
    // 在这个底色上画的），所以不做透明键，直接画上去。
    BITMAP bm;
    memset(&bm, 0, sizeof(bm));
    ::GetObject(hbm, sizeof(bm), &bm);

    CDC dc;
    dc.CreateCompatibleDC(NULL);
    HDC hdc = dc.GetSafeHdc();
    HGDIOBJ old = ::SelectObject(hdc, hbm);

    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bm.bmWidth;
    bi.bmiHeader.biHeight = -bm.bmHeight;      // 自顶向下
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    int n = bm.bmWidth * bm.bmHeight;
    DWORD* px = new DWORD[n + 1];
    memset(px, 0, (n + 1) * sizeof(DWORD));
    ::GetDIBits(hdc, hbm, 0, bm.bmHeight, px, &bi, DIB_RGB_COLORS);
    ::SelectObject(hdc, old);
    dc.DeleteDC();

    // 颜色表里的 4bpp 索引已经被 GetDIBits 展开成 32bpp 的 RGB（背景索引 -> 系统调色板的
    // 浅灰）。把最接近"3D 按钮面"的那一色当透明色，做一张 1bpp 掩码。
    // ⚠️ CreateBitmap 的掩码**必须是 wBitsPerPixel=1**：写成 (1, 32) 会返回 NULL，
    //    接着 CreateIconIndirect 也失败、图标全空（第一版就是这么错的，按钮上一片灰）。
    int cx = bm.bmWidth, cyy = bm.bmHeight;
    int stride = ((cx + 15) / 16) * 2;                 // 1bpp 行按 WORD 对齐
    BYTE* mask = new BYTE[stride * cyy + 16];
    memset(mask, 0xFF, stride * cyy + 16);             // 1 = 透明
    COLORREF crFace = ::GetSysColor(COLOR_3DFACE);
    int x, y, j;
    for (y = 0; y < cyy; y++)
    {
        for (x = 0; x < cx; x++)
        {
            j = y * cx + x;
            COLORREF c = px[j] & 0x00FFFFFF;
            int dr = GetRValue(c) - GetRValue(crFace);
            int dg = GetGValue(c) - GetGValue(crFace);
            int db = GetBValue(c) - GetBValue(crFace);
            if ((dr * dr + dg * dg + db * db) >= 900)  // 与按钮面色差 > 30 -> 不透明
                mask[y * stride + x / 8] &= (BYTE)~(0x80 >> (x % 8));
        }
    }
    HBITMAP hAnd = ::CreateBitmap(cx, cyy, 1, 1, mask);
    delete[] mask;

    ICONINFO ii;
    memset(&ii, 0, sizeof(ii));
    ii.fIcon = TRUE;
    ii.hbmColor = hbm;
    ii.hbmMask = hAnd;
    HICON hIcon = ::CreateIconIndirect(&ii);
    if (hAnd) ::DeleteObject(hAnd);
    delete[] px;
    ::DeleteObject(hbm);

    EcCompIconRec e;
    e.lib = lib;
    e.bmpId = bmpId;
    e.hIcon = hIcon;
    m_compIcons.Add(e);
    return hIcon;
}

// 按 pane 客户区宽度把 40 个按钮排成网格。
// ⚠️ 原版是**固定 4 列**（实测 5/34/63/92 = 列距 29，40 个按钮 x 4 列 x 10 行），
//    不会因为面板变宽就多排几列 —— 本工程原来按宽度算列数，面板一宽就变成 6 列，
//    和原版截图一眼就不一样（用户 2026-10 截图指出）。所以这里**最多 4 列**。
void CPaneBar::LayoutCompButtons(int cx, int cy)
{
    const int CELL = 28, GAP = 1, M = 5, TOP = 34, HEADH = 20;
    int nCols = (cx - 2 * M + GAP) / (CELL + GAP);
    if (nCols < 1) nCols = 1;
    if (nCols > 4) nCols = 4;                 // 原版最多 4 列
    if (nCols > EC_COMP_BTN_N) nCols = EC_COMP_BTN_N;
    for (int i = 0; i < EC_COMP_BTN_N; i++)
    {
        CWnd* p = m_host.GetDlgItem(IDC_COMP_BTN_FIRST + i);
        if (p == NULL)
            continue;
        int r = i / nCols, c = i % nCols;
        int x = M + c * (CELL + GAP);
        int y = TOP + r * (CELL + GAP);
        if (y + CELL > cy - M)
            y = cy - M - CELL;                       // 放不下就贴着底边（别跑出客户区）
        p->MoveWindow(x, y, CELL, CELL);
    }
    CWnd* pHead = m_host.GetDlgItem(IDC_TAB_COMP);
    if (pHead != NULL)
        pHead->MoveWindow(M, M, cx - 2 * M, HEADH);
}

void CPaneBar::FillComponentBox(int nGroup)
{
    if (nGroup < 0)
        nGroup = m_nCompGroup;
    m_nCompGroup = nGroup;

    // ---- 组下拉框的条目：只列"当前载入的库里真的有组件"的组，当前组选中
    CComboBox* pCbo = NULL;
    {
        CWnd* pw = GetPaneItem(IDC_TAB_COMP);
        if (pw != NULL && pw->GetSafeHwnd() != NULL && pw->IsKindOf(RUNTIME_CLASS(CComboBox)))
            pCbo = (CComboBox*)pw;
    }
    // 模板里是 ListBox（原版那个 116x20 的控件本来就是下拉）；用 CListBox 填。
    CListBox* pLb = (CListBox*)GetPaneItem(IDC_TAB_COMP);
    (void)pCbo;
    int nGroups = 4;
    if (pLb != NULL)
    {
        pLb->ResetContent();
        for (int g = 0; g < nGroups; g++)
        {
            // 有组件的组才列出来
            BOOL bHas = FALSE;
            for (int i = 0; i < EcLibCount() && !bHas; i++)
                if (CompGroupOfLib(i) == g)
                    bHas = TRUE;
            if (bHas)
            {
                int n = pLb->AddString(EcCompGroupName(g));
                pLb->SetItemData(n, (DWORD)g);
                if (g == m_nCompGroup)
                    pLb->SetCurSel(n);
            }
        }
    }

    // ---- 组件按钮：按 (库, 类型) 填，超出的按钮隐藏
    int nBtn = 0;
    for (int i = 0; i < EcLibCount(); i++)
    {
        if (CompGroupOfLib(i) != m_nCompGroup)
            continue;
        const EcLib& L = EcLibAt(i);
        for (int k = 0; k < L.nDataType; k++)
        {
            if (!EcDtIsVisualUnit(i, k))
                continue;                  // 只有"可视窗口组件"能放到窗体上
            const EcDataType* d = EcDtAt(i, k);
            if (d == NULL || d->name.IsEmpty())
                continue;
            // ⚠️ 两个必须排掉的：① 窗口类型本身（krnln 的 dt=0「窗口」，bmpId=0，它是要设计的
            //    窗体不是组件）；② 载不进来的占位类型「无法载入的窗口组件」（bmpId=1153，
            //    库里那张图是问号）。原版那 40 格全是真组件，没有这两个。
            if (d->bmpId == 0)
                continue;
            if (d->name.Find(_T("无法载入")) >= 0)
                continue;
            if (nBtn >= EC_COMP_BTN_N)
                break;
            m_nCompBtnLib[nBtn] = i;
            m_nCompBtnDt[nBtn]  = k;
            CWnd* p = m_host.GetDlgItem(IDC_COMP_BTN_FIRST + nBtn);
            if (p != NULL)
            {
                p->ShowWindow(SW_SHOW);
                p->SetWindowText(d->name);   // 只用来做 tooltip，不画在按钮上（原版也不画）
                p->Invalidate(FALSE);
            }
            nBtn++;
        }
    }
    for (int j = nBtn; j < EC_COMP_BTN_N; j++)
    {
        m_nCompBtnLib[j] = m_nCompBtnDt[j] = -1;
        CWnd* p = m_host.GetDlgItem(IDC_COMP_BTN_FIRST + j);
        if (p != NULL)
            p->ShowWindow(SW_HIDE);
    }

    // 排列（模板给的坐标只在启动那一下就够，缩放要走这里）
    CRect rc;
    GetClientRect(&rc);
    LayoutCompButtons(rc.Width(), rc.Height());
    m_host.Invalidate(TRUE);

    // 诊断：ECLONE_DUMP_COMP=<路径> 时把"这一组该显示哪些按钮"倒出来
    // （组件箱是自绘按钮，窗口树里只看得到 Button 的可见性，看不出它挂的是哪个组件）。
    TCHAR szDump[_MAX_PATH];
    if (::GetEnvironmentVariable(_T("ECLONE_DUMP_COMP"), szDump, _MAX_PATH) > 0)
    {
        FILE* f = fopen(szDump, "w");
        if (f != NULL)
        {
            fprintf(f, "group=%d buttons=%d libs=%d\n", m_nCompGroup, nBtn, EcLibCount());
            for (int qi = 0; qi < EcLibCount(); qi++)
                fprintf(f, "lib %d state=0x%X group=%d name=%s path=%s\n", qi,
                        (unsigned)EcLibAt(qi).state, CompGroupOfLib(qi),
                        (LPCTSTR)EcLibAt(qi).name, (LPCTSTR)EcLibAt(qi).path);
            for (int q = 0; q < nBtn; q++)
            {
                const EcDataType* dd = EcDtAt(m_nCompBtnLib[q], m_nCompBtnDt[q]);
                fprintf(f, "btn %d lib=%d dt=%d bmp=%d name=%s\n", q, m_nCompBtnLib[q],
                        m_nCompBtnDt[q], (dd != NULL) ? (int)dd->bmpId : 0,
                        (dd != NULL) ? (LPCTSTR)dd->name : "");
            }
            fclose(f);
        }
    }
}

void CPaneBar::OnCompGroup()
{
    // 组下拉框选择变了 -> 换一组组件（原版那个 116x20 的控件就是干这个的）
    CListBox* pLb = (CListBox*)GetPaneItem(IDC_TAB_COMP);
    if (pLb == NULL)
        return;
    int n = pLb->GetCurSel();
    if (n < 0)
        return;
    m_nCompGroup = (int)pLb->GetItemData(n);
    FillComponentBox(m_nCompGroup);

    // 自己把焦点还给 pane，别让下拉框一直吃键盘（原版这是个下拉，选完就收）
    SetFocus();
}

// 组件按钮被点 -> 进放置模式（wParam = 库, lParam = 类型；-1 = 取消放置）
void CPaneBar::OnCompBtn(UINT nID)
{
    int idx = (int)nID - IDC_COMP_BTN_FIRST;
    if (idx < 0 || idx >= EC_COMP_BTN_N)
        return;
    int lib = m_nCompBtnLib[idx], dt = m_nCompBtnDt[idx];
    if (lib < 0 || dt < 0)
        return;

    // 选中态：重画整个网格（选中的那个按钮画一圈黑框）
    m_nCompSel = idx;
    m_host.Invalidate(TRUE);

    CWnd* pMain = AfxGetMainWnd();
    if (pMain == NULL || pMain->GetSafeHwnd() == NULL)
        return;
    CMDIChildWnd* pChild = ((CMDIFrameWnd*)pMain)->MDIGetActive();
    if (pChild == NULL || pChild->GetSafeHwnd() == NULL)
        return;
    pChild->PostMessage(EC_WM_ARMUNIT, (WPARAM)lib, (LPARAM)dt);

    CFrameWnd* pFrame = GetParentFrame();
    if (pFrame != NULL && pFrame->GetSafeHwnd() != NULL)
    {
        const EcDataType* d = EcDtAt(lib, dt);
        CString s;
        s.Format(_T("已进入放置模式：%s   —— 切到设计器页，在窗体上单击（默认尺寸）或拖出矩形"),
                 (d != NULL) ? (LPCTSTR)d->name : _T("组件"));
        pFrame->SendMessage(EC_WM_HINT, 0, (LPARAM)(LPCTSTR)s);
    }
}

// WM_DRAWITEM 的入口（ON_WM_DRAWITEM -> AfxSig_vwi，签名必须是 (int, LPDRAWITEMSTRUCT)）
void CPaneBar::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lp)
{
    if (nIDCtl < IDC_COMP_BTN_FIRST || nIDCtl >= IDC_COMP_BTN_FIRST + EC_COMP_BTN_N)
    {
        CSizingControlBarG::OnDrawItem(nIDCtl, lp);       // 不是组件按钮 -> 交回基类
        return;
    }
    extern void EcTrace(LPCTSTR pszFmt, ...);
    EcTrace(_T("drawitem ctl=%d idx=%d"), nIDCtl, nIDCtl - IDC_COMP_BTN_FIRST);
    DrawCompItem(lp);
}

// owner-draw 组件按钮：画 3D 边框 + 库里那张 24x24 图标居中（原版按钮上不写文字）。
// 选中的那个按钮外面套一圈 1px 黑框（和设计器里选中组件的那圈框一个观感）。
void CPaneBar::DrawCompItem(LPDRAWITEMSTRUCT lp)
{
    CDC* pDC = CDC::FromHandle(lp->hDC);
    CRect rc(lp->rcItem);
    int idx = (int)lp->CtlID - IDC_COMP_BTN_FIRST;
    BOOL bSel = (idx == m_nCompSel);
    BOOL bDown = (lp->itemState & ODS_SELECTED) ? TRUE : FALSE;

    pDC->FillSolidRect(rc, ::GetSysColor(COLOR_3DFACE));
    if (bDown)
    {
        pDC->Draw3dRect(rc, ::GetSysColor(COLOR_3DSHADOW), ::GetSysColor(COLOR_3DHILIGHT));
    }
    else
    {
        pDC->Draw3dRect(rc, ::GetSysColor(COLOR_3DHILIGHT), ::GetSysColor(COLOR_3DSHADOW));
    }

    if (idx >= 0 && idx < EC_COMP_BTN_N)
    {
        int lib = m_nCompBtnLib[idx], dt = m_nCompBtnDt[idx];
        const EcDataType* d = (lib >= 0 && dt >= 0) ? EcDtAt(lib, dt) : NULL;
        if (d != NULL)
        {
            HICON hIcon = EcCompIcon(lib, d->bmpId);
            if (hIcon != NULL)
            {
                int x = rc.left + (rc.Width() - 24) / 2 + (bDown ? 1 : 0);
                int y = rc.top + (rc.Height() - 24) / 2 + (bDown ? 1 : 0);
                ::DrawIconEx(lp->hDC, x, y, hIcon, 24, 24, 0, NULL, DI_NORMAL);
            }
        }
    }
    if (bSel)
        pDC->Draw3dRect(rc, RGB(0, 0, 0), RGB(0, 0, 0));
}

// -------------------------------------------------- 「工作夹-属性」页：属性值全部来自支持库
// 列的是支持库给的属性元数据（EcPropAt），值走 ITF 6（EcGetUnitProperty），改走 ITF 4（双击就地编辑）。
void CPaneBar::FillProps(int lib, int dt, void* hunit)
{
    CListCtrl* p = (CListCtrl*)GetPaneItem(IDC_TREE_WORK + 2);
    if (p == NULL)
        return;
    p->DeleteAllItems();
    m_propIdx.RemoveAll();
    m_propLib = lib;
    m_propDt = dt;
    m_propHunit = hunit;
    m_propEditRow = -1;
    if (m_propEdit.GetSafeHwnd() != NULL)
        m_propEdit.ShowWindow(SW_HIDE);

    // 自测用：ECLONE_DESIGN_DUMP=<文件> 时把属性表倒成 "prop|下标|名|值"
    FILE* f = NULL;
    char dump[_MAX_PATH];
    if (::GetEnvironmentVariableA("ECLONE_DESIGN_DUMP", dump, sizeof(dump)) > 0)
        f = fopen(dump, "wb");

    const EcDataType* d = EcDtAt(lib, dt);
    if (d != NULL && hunit != NULL)
    {
        int row = 0;
        for (int i = 0; i < d->propCount; i++)
        {
            const EcProp* P = EcPropAt(lib, dt, i);
            if (P == NULL || P->name.IsEmpty())
                continue;
            if (P->state & EC_UW_IS_HIDED)
                continue;                       // 隐藏属性不进表（原版属性表也不列）
            CString v;
            if (!EcGetUnitProperty(lib, dt, hunit, i, &v))
                v = _T("");
            int r = p->InsertItem(row, P->name);
            p->SetItemText(r, 1, v);
            m_propIdx.Add(i);
            if (f != NULL)
                fprintf(f, "prop|%d|%s|%s\n", i, (LPCSTR)(LPCTSTR)P->name, (LPCSTR)(LPCTSTR)v);
            row++;
        }
        if (f != NULL)
            fprintf(f, "end|%d|lib=%d|dt=%d\n", row, lib, dt);
    }
    if (f != NULL)
        fclose(f);
}

void CPaneBar::OnPropDblClk(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    if (pNMHDR->idFrom != IDC_TREE_WORK + 2)
        return;
    CListCtrl* p = (CListCtrl*)GetPaneItem(IDC_TREE_WORK + 2);
    if (p == NULL || m_propHunit == NULL)
        return;
    NMITEMACTIVATE* ia = (NMITEMACTIVATE*)pNMHDR;
    if (ia->iItem < 0 || ia->iItem >= (int)m_propIdx.GetSize())
        return;
    int idx = m_propIdx[ia->iItem];
    if (!EcPropertyEditable(m_propLib, m_propDt, m_propHunit, idx))
    {
        CFrameWnd* pFrame = GetParentFrame();
        if (pFrame != NULL)
            pFrame->SendMessage(EC_WM_HINT, 0, (LPARAM)(LPCTSTR)_T("该属性此刻不可修改（支持库的 ITF 2 说的）"));
        return;
    }
    CRect rc;
    if (!p->GetSubItemRect(ia->iItem, 1, LVIR_BOUNDS, rc))
        return;
    rc.InflateRect(0, 0, 0, 1);
    if (m_propEdit.GetSafeHwnd() == NULL)
    {
        if (!m_propEdit.Create(WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, rc, p, 4001))
            return;
    }
    else
        m_propEdit.MoveWindow(&rc);
    m_propEditRow = ia->iItem;
    m_propEdit.SetWindowText(p->GetItemText(ia->iItem, 1));
    m_propEdit.ShowWindow(SW_SHOW);
    m_propEdit.SetFocus();
    m_propEdit.SetSel(0, -1);
}

void CPaneBar::CommitPropEdit(BOOL bSave)
{
    if (m_propEdit.GetSafeHwnd() == NULL || m_propEditRow < 0)
        return;
    int row = m_propEditRow;
    CString txt;
    m_propEdit.GetWindowText(txt);
    m_propEdit.ShowWindow(SW_HIDE);
    m_propEditRow = -1;
    if (!bSave || row < 0 || row >= (int)m_propIdx.GetSize() || m_propHunit == NULL)
        return;

    int idx = m_propIdx[row];
    BOOL bRecreate = FALSE;
    if (EcSetUnitProperty(m_propLib, m_propDt, m_propHunit, idx, txt, &bRecreate))
    {
        CListCtrl* p = (CListCtrl*)GetPaneItem(IDC_TREE_WORK + 2);
        CString v;
        if (p != NULL && EcGetUnitProperty(m_propLib, m_propDt, m_propHunit, idx, &v))
        {
            p->SetItemText(row, 1, v);      // 回读：库没有"设置失败"信号，只能看回读值
            if (v != txt)
            {
                CFrameWnd* pFrame = GetParentFrame();
                if (pFrame != NULL)
                {
                    CString msg;
                    msg.Format(_T("支持库没有接受这个值（回读是 %s）"), (LPCTSTR)v);
                    pFrame->SendMessage(EC_WM_HINT, 0, (LPARAM)(LPCTSTR)msg);
                }
            }
        }
        CFrameWnd* pFrame = GetParentFrame();
        if (pFrame != NULL && bRecreate)
            pFrame->SendMessage(EC_WM_HINT, 0,
                (LPARAM)(LPCTSTR)_T("属性已改。支持库要求重建窗口才会改外形（原版是 ITF 5 取数据→ITF 1 重建，本轮还没接）"));
        // 改的可能就是窗体的"底色"（设计器那块"纸"的颜色）或组件几何 -> 让所有设计器重画一遍
        EcDesignerRedrawAll();
    }
    else
        ::MessageBeep(0);
}

BOOL CPaneBar::PreTranslateMessage(MSG* pMsg)
{
    if (m_propEdit.GetSafeHwnd() != NULL && pMsg->hwnd == m_propEdit.GetSafeHwnd())
    {
        if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
        {
            CommitPropEdit(TRUE);
            return TRUE;
        }
        if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
        {
            CommitPropEdit(FALSE);
            return TRUE;
        }
        if (pMsg->message == WM_KILLFOCUS)
            CommitPropEdit(TRUE);
    }
    return CSizingControlBarG::PreTranslateMessage(pMsg);
}

// 运行时按 pane 的真实客户区（像素）摆控件。
// 原版实测：tab 控件离 pane 客户区四边各 3px；内容页再内缩 4px；标签行 23px。
void CPaneBar::LayoutPages()
{
    if (!GetSafeHwnd() || !m_host.GetSafeHwnd())
        return;

    CRect rc;
    GetClientRect(&rc);          // sizecbar 已按像素算好客户区，不再是模板的 DLU 尺寸
    m_host.MoveWindow(0, 0, rc.Width(), rc.Height());

    const int M = 3, PAD = 4, TABH = TabHeight();
    int cx = rc.Width(), cy = rc.Height();
    CWnd* pTab = (m_nTabID != 0) ? m_host.GetDlgItem(m_nTabID) : NULL;

    if (pTab != NULL && m_nTabs > 0)
    {
        CRect rt(M, M, cx - M, cy - M);
        pTab->MoveWindow(rt);

        CRect rp(rt.left + PAD, rt.top + PAD, rt.right - PAD, rt.bottom - PAD);
        if (m_bTabsBottom)
            rp.bottom -= TABH;          // 工作夹：标签在下，内容占上方
        else
            rp.top += TABH;             // 状态夹：标签在上，内容占下方
        for (int i = 0; i < m_nTabs; i++)
        {
            CWnd* pg = m_host.GetDlgItem(m_nFirstPageID + i);
            if (pg != NULL)
            {
                pg->MoveWindow(rp);
                // 顺手把"当前页可见/其它页隐藏"重新声明一遍：缩放/重排之后自愈，
                // 免得万一哪次通知把页藏掉了就再也回不来
                pg->ShowWindow(i == m_nCurPage ? SW_SHOW : SW_HIDE);
            }
        }
        return;
    }

    // 窗口组件箱：组下拉框（116x20）+ 40 个 owner-draw 组件按钮的网格
    // （原版实测：对话框 126x415、按钮 28x28、4 列 x 10 行、列/行距 29，见 tools\probe_compbox3.py）
    CWnd* pHead = m_host.GetDlgItem(IDC_TAB_COMP);
    if (pHead != NULL && pHead->GetSafeHwnd() != NULL)
    {
        LayoutCompButtons(cx, cy);
        return;
    }

    // （兼容旧版残留：模板里如果还有 SysTreeView32 就当内容页铺满）
    CWnd* pTree = m_host.GetDlgItem(IDC_TREE_COMP);
    if (pTree != NULL)
        pTree->MoveWindow(M, M + 24, cx - 2 * M, cy - 2 * M - 24);
}

void CPaneBar::OnSize(UINT nType, int cx, int cy)
{
    CSizingControlBarG::OnSize(nType, cx, cy);
    LayoutPages();
    SendMessage(WM_NCPAINT);
    // 缩放后显式让内容区+所有子窗口失效重画。
    // 用户实测遇到过一次"改窗口大小后面板内容整块不重绘"（只剩背景色和标签页），
    // 5 种复现路径都没重现，所以这里不赌：每次尺寸变化都强制失效一次（不含 RDW_ERASE，避免拖动时闪）。
    if (m_host.GetSafeHwnd() != NULL)
        m_host.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// -------------------------------------------------- 标题行（原版：两条凹线 + □ + ×）
// × 由 sizecbar 自己画(Marlett 'r')；□ 是易语言加的第二个按钮，原版位图 596 实测 6x6。
void CPaneBar::NcCalcClient(LPRECT pRc, UINT nDockBarID)
{
    CRect rcWnd(*pRc);                       // 窗口矩形（基类会改写 pRc，得先存下来）
    CSizingControlBarG::NcCalcClient(pRc, nDockBarID);
    if (!HasGripper())
        return;

    CRect rcClient(*pRc);
    BOOL bHorz = (nDockBarID == AFX_IDW_DOCKBAR_TOP ||
                  nDockBarID == AFX_IDW_DOCKBAR_BOTTOM);

    // 原版实测（相对客户区）：
    //   左右停靠：× 在 (right-13, top-9)，□ 在它左边 13px（两钮并排）
    //   上下停靠：× 在 (left-5, top+13)，□ 在它下边 13px（两钮竖排）
    CPoint ptHide = bHorz ? CPoint(rcClient.left - 5, rcClient.top + 13)
                          : CPoint(rcClient.right - 13, rcClient.top - 9);
    m_biHide.Move(ptHide - rcWnd.TopLeft());

    CPoint ptMin = bHorz ? CPoint(ptHide.x, ptHide.y + 13)
                         : CPoint(ptHide.x - 13, ptHide.y);
    CPoint ptBtn = ptMin - rcWnd.TopLeft();     // CRect 在 MFC4.2 里没有 (CPoint,CSize) 构造
    m_rcMin = CRect(ptBtn.x, ptBtn.y, ptBtn.x + 11, ptBtn.y + 11);
}

void CPaneBar::NcPaintGripper(CDC* pDC, CRect rcClient)
{
    CSizingControlBarG::NcPaintGripper(pDC, rcClient);
    if (!HasGripper())
        return;

    COLORREF clrHi = ::GetSysColor(COLOR_BTNHIGHLIGHT);
    COLORREF clrSh = ::GetSysColor(COLOR_BTNSHADOW);
    COLORREF clrTx = ::GetSysColor(COLOR_BTNTEXT);

    // 原版两个按钮常态就是"浮起"的 3d 框（不是只在鼠标悬停时才浮起）
    CRect rcHide = m_biHide.GetRect();
    if (!m_biHide.bPushed && !m_biHide.bRaised)
        pDC->Draw3dRect(rcHide, clrHi, clrSh);
    if (!m_bCollapsed)
        pDC->Draw3dRect(m_rcMin, clrHi, clrSh);

    // □ 字形（原版位图 596：6x6，上边 2px、其余 1px）
    int x = m_rcMin.left + 2, y = m_rcMin.top + 2;
    pDC->FillSolidRect(x, y, 6, 2, clrTx);
    pDC->FillSolidRect(x, y + 2, 1, 4, clrTx);
    pDC->FillSolidRect(x + 5, y + 2, 1, 4, clrTx);
    pDC->FillSolidRect(x, y + 5, 6, 1, clrTx);
}

LRESULT CPaneBar::OnNcHitTest(CPoint point)
{
    LRESULT nRet = CSizingControlBarG::OnNcHitTest(point);
    if (nRet != HTCLIENT && nRet != HTCLOSE)
        return nRet;                    // 停靠边缘等交给库

    CRect rcBar, rc;
    GetWindowRect(rcBar);
    rc = m_rcMin;
    rc.OffsetRect(rcBar.TopLeft());
    if (rc.PtInRect(point))
        return HTCLOSE;                 // 复用 HTCLOSE，具体动作在 OnNcLButtonUp 里分
    return nRet;
}

void CPaneBar::OnNcLButtonUp(UINT nHitTest, CPoint point)
{
    if (nHitTest == HTCLOSE)
    {
        CRect rcBar, rc;
        GetWindowRect(rcBar);
        rc = m_rcMin;
        rc.OffsetRect(rcBar.TopLeft());
        if (rc.PtInRect(point))
        {
            ToggleCollapse();
            return;
        }
    }
    CSizingControlBarG::OnNcLButtonUp(nHitTest, point);
}

// □ = 缩小/还原（原版这个按钮的作用；不做功能的话至少语义要对）
void CPaneBar::ToggleCollapse()
{
    if (IsFloating() || m_pDockSite == NULL)
        return;

    if (IsHorzDocked())
    {
        if (!m_bCollapsed)
        {
            m_nSavedSize = m_szHorz.cy;
            m_szHorz.cy = 20;
        }
        else
        {
            m_szHorz.cy = m_nSavedSize > 0 ? m_nSavedSize : 188;
        }
    }
    else
    {
        if (!m_bCollapsed)
        {
            m_nSavedSize = m_szVert.cx;
            m_szVert.cx = 20;
        }
        else
        {
            m_szVert.cx = m_nSavedSize > 0 ? m_nSavedSize : 229;
        }
    }
    m_bCollapsed = !m_bCollapsed;
    m_pDockSite->RecalcLayout();
    SendMessage(WM_NCPAINT);
}

// ------------------------------------------------------------------ 选择器条
// 编辑器上方那两个下拉框（子程序选择器 / 事件选择器）。
// 原版实测：它们不是 MDI 子窗口里的控件，而是主框架上一条 25px 高的停靠条，
// 两个 ComboBox 分别位于 x=15 宽 200 / x=215 宽 300，高 20。
class CSelectorBar : public CControlBar
{
    DECLARE_DYNAMIC(CSelectorBar)
public:
    CSelectorBar();
    BOOL Create(CWnd* pParent, UINT nID);

protected:
    CComboBox m_cboProc;
    CComboBox m_cboEvent;
    CFont     m_font;

    virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);
    virtual void OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    DECLARE_MESSAGE_MAP()
};

IMPLEMENT_DYNAMIC(CSelectorBar, CControlBar)

BEGIN_MESSAGE_MAP(CSelectorBar, CControlBar)
    ON_WM_SIZE()
END_MESSAGE_MAP()

CSelectorBar::CSelectorBar()
{
}

BOOL CSelectorBar::Create(CWnd* pParent, UINT nID)
{
    ASSERT_VALID(pParent);

    m_dwStyle = CBRS_TOP | CBRS_TOOLTIPS | CBRS_GRIPPER;

    CString wndclass = ::AfxRegisterWndClass(CS_DBLCLKS,
        ::LoadCursor(NULL, IDC_ARROW), ::GetSysColorBrush(COLOR_BTNFACE), 0);
    if (!CWnd::Create(wndclass, _T("选择器"), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                      CRect(0, 0, 0, 0), pParent, nID))
        return FALSE;

    m_font.CreatePointFont(90, _T("宋体"));

    const DWORD dwCombo = WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST;
    if (m_cboProc.Create(dwCombo, CRect(15, 2, 215, 200), this, IDC_CBO_PROC))
    {
        m_cboProc.SetFont(&m_font);
        m_cboProc.AddString(_T("() 启动窗口程序集"));
        m_cboProc.SetCurSel(0);
    }
    if (m_cboEvent.Create(dwCombo, CRect(215, 2, 515, 200), this, IDC_CBO_EVENT))
    {
        m_cboEvent.SetFont(&m_font);
        m_cboEvent.AddString(_T("_启动窗口_创建完毕"));
        m_cboEvent.AddString(_T("_图形按钮1_被单击"));
        m_cboEvent.SetCurSel(1);
    }
    return TRUE;
}

CSize CSelectorBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
    // 原版这一行高 25px（两个下拉框 20px 高，上下各留 2px）；
    // 不被拉伸时用自然宽度 515（= 15 + 200 + 300）
    if (bHorz)
        return CSize(bStretch ? 32767 : 515, 25);
    return CSize(25, bStretch ? 32767 : 515);
}

void CSelectorBar::OnUpdateCmdUI(CFrameWnd* /*pTarget*/, BOOL /*bDisableIfNoHndler*/)
{
    // 这条不是工具条，没有按钮要更新
}

void CSelectorBar::OnSize(UINT nType, int cx, int cy)
{
    CControlBar::OnSize(nType, cx, cy);
    m_cboProc.MoveWindow(15, 2, 200, 20);
    m_cboEvent.MoveWindow(215, 2, 300, 20);
}

// ------------------------------------------------------------------ 工具条按钮表
// 完全照抄原版：用 tools\tbprobe.exe（32 位小程序，进原版进程把 TBBUTTON 表 dump 出来）拿到实测值。
// ⚠️ 两点必须按表来，顺序摆会错位：
//   1) 表里有 TBSTYLE_SEP（-1）—— 分隔条**不占位图格**；
//   2) iBitmap **不是顺序的**：标准条跳过位图格 6，且"查找"用的是第 22 格。
// 表里每个元素的含义：>=0 = 取该位图格；-1 = 分隔条。
static const int kStdBtns[] = {
    /* 0 新建*/ 0, 1, 2, -1,            // 新建/打开/保存 |
    /* 4     */ 3, 4, 5, 7, 8, -1,     // 剪切/复制/粘贴/撤销/恢复 |（跳过格 6）
    /*10     */ 22, -1,                // 查找（用第 22 格）|
    /*12     */ 10, 11, 12, -1,        // 3 个带下拉的方块 |
    /*16     */ 13, 14, 15, 16, 17, 18, 19, 20, -1,
    /*25     */ 21
};
static const int kAlignBtns[] = {
    0, -1, 1, 2, 3, 4, -1, 6, 5, 7, 8, 9, 10, -1, 11, 12, 13   // 注意 6、5 是反的
};

// 标准条里第 12/13/14 项（那 3 个方块）在原版是 TBSTYLE_DROPDOWN + CHECKED
static const int kStdDropdownIdx[] = { 12, 13, 14 };   // 原版这 3 项是 TBSTYLE_DROPDOWN

// VC6 的 commctrl.h 不一定有 TBBUTTONINFO，自己声明一份（字段布局与 comctl32 一致）
typedef struct tagEC_TBBUTTONINFO {
    UINT   cbSize;
    DWORD  dwMask;
    int    idCommand;
    int    iImage;
    BYTE   fsState;
    BYTE   fsStyle;
    WORD   cx;
    DWORD  lParam;
    LPTSTR pszText;
    int    cchText;
} EC_TBBUTTONINFO;
#define EC_TBIF_STYLE      0x0002
#define EC_TBIF_BYINDEX    0x80000000   // dwMask 里带这一位，wParam 才是"下标"而不是命令 ID
#define EC_TB_SETBUTTONINFO (WM_USER + 66)

// 按原版表建按钮：先 SetButtons（顺序分配位图格），再用 TB_CHANGEBITMAP 改成表里的格号。
static void EcSetToolbarButtons(CToolBar& bar, const int* img, int n,
                                const int* pDropdown, int nDropdown)
{
    UINT ids[40];
    UINT cmd[40];                       // 表下标 -> 命令 ID（分隔条不占命令 ID）
    int i, k = 0;
    ASSERT(n <= 40);
    for (i = 0; i < n; i++)
    {
        if (img[i] < 0) { ids[i] = 0; cmd[i] = 0; }
        else            { ids[i] = cmd[i] = (UINT)(IDC_TB_FIRST + k); k++; }
    }
    bar.SetButtons(ids, n);

    for (i = 0; i < n; i++)
    {
        if (img[i] < 0)
            continue;
        // ⚠️ lParam 是 **0 基**图序号（因为这里用的是 TB_SETIMAGELIST；TB_ADDBITMAP 那套才是 1 基，
        //    网上"+1"的说法就是后者）。写成 +1 会让整条工具条整体错一格 —— 实测踩过。
        bar.SendMessage(TB_CHANGEBITMAP, (WPARAM)cmd[i], (LPARAM)img[i]);
    }

    for (i = 0; i < nDropdown; i++)
    {
        int idx = pDropdown[i];
        if (idx < 0 || idx >= n || img[idx] < 0)
            continue;
        EC_TBBUTTONINFO tbi;
        memset(&tbi, 0, sizeof(tbi));
        tbi.cbSize    = sizeof(tbi);
        tbi.dwMask    = EC_TBIF_STYLE;
        tbi.idCommand = cmd[idx];
        tbi.fsStyle   = TBSTYLE_BUTTON | TBSTYLE_DROPDOWN;
        // wParam 按命令 ID；失败再试"按索引"（dwMask 要带 TBIF_BYINDEX，wParam 才是下标）
        if (!bar.SendMessage(EC_TB_SETBUTTONINFO, (WPARAM)cmd[idx], (LPARAM)&tbi))
        {
            tbi.dwMask = EC_TBIF_STYLE | EC_TBIF_BYINDEX;
            bar.SendMessage(EC_TB_SETBUTTONINFO, (WPARAM)idx, (LPARAM)&tbi);
        }
        bar.SendMessage(TB_SETSTATE, (WPARAM)cmd[idx],
                        (LPARAM)(TBSTATE_ENABLED | TBSTATE_CHECKED));
    }
}

// ------------------------------------------------------------------ 主框架
// 建工具条用的 CImageList：单元尺寸由我们指定（16x15）。
// 见 CMainFrame::OnCreate 里那段注释：交给 comctl32 自己从位图猜高度会歪 2 行。
static BOOL LoadToolbarImageList(CImageList& il, UINT nBitmapID, int nCount)
{
    CBitmap bmp;
    if (!bmp.LoadBitmap(nBitmapID))
        return FALSE;
    if (!il.Create(16, 15, ILC_COLOR8 | ILC_MASK, nCount, 1))
        return FALSE;
    il.Add(&bmp, RGB(192, 192, 192));   // 位图里的透明色已导成 COLOR_BTNFACE
    return TRUE;
}

class CMainFrame : public CMDIFrameWnd
{
    DECLARE_DYNAMIC(CMainFrame)
public:
    CMainFrame();

protected:
    CStatusBar   m_wndStatusBar;
    CToolBar     m_wndToolBar;
    CToolBar     m_wndToolBarAlign;
    CToolBar     m_wndToolBarPos;
    CSelectorBar m_wndSelector;
    CPaneBar     m_wndWork;
    CPaneBar     m_wndStatus;
    CPaneBar     m_wndComp;
    CImageList   m_ilTree;
    CImageList   m_ilProg;      // 「程序」页树的图标（从原版截图抠的 15 个 16x16）
    CImageList   m_ilTbStd;
    CImageList   m_ilTbAlign;
    CImageList   m_ilTbPos;
    CString      m_sLibDir;
    CString      m_sLibLog;

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnNcPaint();
    afx_msg void OnTbCmd(UINT nID);
    afx_msg void OnTbUpdate(CCmdUI* pCmdUI);
    afx_msg void OnSysConfig();          // 工具->系统配置 (#32876)
    afx_msg void OnLibCfg();             // 工具->支持库配置 (#32906)：动态载入/卸载支持库
    afx_msg void OnCloseAll();           // 窗口->全部关闭 (#32982)
    // 「窗口」菜单里的工具条/面板开关。原版的 id 就是 MFC 的 control bar id
    // （59392=AFX_IDW_TOOLBAR 标准条 / 59393=AFX_IDW_STATUS_BAR / 133 对齐条 / 135 定位条 /
    //   129 工作夹 / 130 状态夹 / 110 组件箱 / 132 工作与状态组合夹）。
    // ⚠️ MFC 的 CFrameWnd 自带这些开关，但本主框架是 **CMDIFrameWnd** —— `grep OnBarCheck
    //    afxwin.h` 只出现在 CFrameWnd 里，所以实测发 59392/129 都不动（见 HANDOFF）。
    //    所以这里自己接：CMainFrame 有全部分支的成员指针，一行就够。
    afx_msg void OnToggleBar(UINT nID);
    afx_msg void OnUpdateBar(CCmdUI* pCmdUI);
    afx_msg LRESULT OnHint(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSelUnit(WPARAM wParam, LPARAM lParam);
    void PopulatePanes();
public:
    // 「窗口组件箱」是设计器界面的一部分（不是代码编辑器的）：设计器视图显示它、程序集视图收起它。
    // 见 CECChildFrame::ShowView()。m_bCompBoxUserHidden 记住"用户自己收起来过"，别硬弹。
    BOOL m_bCompBoxUserHidden;
    BOOL IsCompBoxShown() const;
    BOOL IsCompBoxUserHidden() const { return m_bCompBoxUserHidden; }
    void ShowCompBox(BOOL bShow);
    // 把文本写进状态夹的「搜寻1」(nPane=0，页 5) / 「搜寻2」(nPane=1，页 6) 并翻到那一页。
    // 整体搜寻 (#32934) 的结果就往这里倒。
    void ShowSearchPane(LPCTSTR pszText, int nPane);
protected:
    DECLARE_MESSAGE_MAP()
};

// 原版主框架窗口类名就是 "ENewFrame"（Spy 实测），这里注册同名类
static BOOL RegisterENewFrameClass()
{
    static BOOL bDone = FALSE;
    if (bDone)
        return TRUE;
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.style         = CS_DBLCLKS;
    wc.lpfnWndProc   = ::DefWindowProc;
    wc.hInstance     = AfxGetInstanceHandle();
    wc.hCursor       = ::LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.hIcon         = ::LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME));
    wc.lpszClassName = _T("ENewFrame");
    bDone = AfxRegisterClass(&wc);
    return bDone;
}

// 设窗口大/小图标。IDR_MAINFRAME 的 ICON 资源里是 16/32/48 三档，LoadImage 按尺寸挑。
// ⚠️ 必须给 **MDI 子窗口** 也设：子窗口最大化时，Windows 会把子窗口的小图标画在
//    父窗口菜单条的最左端 —— 原版左上角那个"易"就是这么来的（不是单独画的装饰）。
static void EC_SetWindowIcon(CWnd* pWnd, UINT nIconID)
{
    if (pWnd == NULL || pWnd->GetSafeHwnd() == NULL)
        return;
    HINSTANCE hInst = AfxGetInstanceHandle();
    HICON hBig = (HICON)::LoadImage(hInst, MAKEINTRESOURCE(nIconID), IMAGE_ICON, 32, 32, 0);
    HICON hSm  = (HICON)::LoadImage(hInst, MAKEINTRESOURCE(nIconID), IMAGE_ICON, 16, 16, 0);
    if (hBig != NULL)
        pWnd->SendMessage(WM_SETICON, ICON_BIG, (LPARAM)hBig);
    if (hSm != NULL)
        pWnd->SendMessage(WM_SETICON, ICON_SMALL, (LPARAM)hSm);
}

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWnd)
    ON_WM_CREATE()
    ON_WM_NCPAINT()
    ON_COMMAND_RANGE(IDC_TB_FIRST, IDC_TB_LAST, OnTbCmd)
    ON_UPDATE_COMMAND_UI_RANGE(IDC_TB_FIRST, IDC_TB_LAST, OnTbUpdate)
    ON_COMMAND(32876, OnSysConfig)
    ON_COMMAND(32906, OnLibCfg)
    // 「窗口」菜单：层叠 57650 / 平铺 57651 / 排列图标 57649 / 分隔 57653 就是 MFC 的标准 id
    // （ID_WINDOW_CASCADE/TILE_HORZ/ARRANGE/SPLIT），CMDIFrameWnd 自己的消息映射已经处理了
    // —— 实测：发 57650 子窗口从最大化变成层叠、57651 变成平铺，本工程一行代码都不用写。
    // 只有「全部关闭」是易语言自己的命令（32982），要自己实现。
    ON_COMMAND(32982, OnCloseAll)
    // 工具条 / 面板开关（见声明处的说明）。
    // ⚠️ **必须用 ON_COMMAND_RANGE**：本工程实测（EC_HANDLE_TRACE=… 的倒档）ON_COMMAND 带
    //    UINT 参数时 nID 是垃圾值 4560984 —— 和「插入菜单」那次同一个坑（AfxSig 不对）。
    //    范围宏会正确传 id。菜单命令 id 分两段：110/129/130/132/133/135 和 59392/59393。
    ON_COMMAND_RANGE(110, 135, OnToggleBar)
    ON_COMMAND_RANGE(59392, 59393, OnToggleBar)
    ON_UPDATE_COMMAND_UI(59392, OnUpdateBar)
    ON_UPDATE_COMMAND_UI(59393, OnUpdateBar)
    ON_UPDATE_COMMAND_UI(133,   OnUpdateBar)
    ON_UPDATE_COMMAND_UI(135,   OnUpdateBar)
    ON_UPDATE_COMMAND_UI(129,   OnUpdateBar)
    ON_UPDATE_COMMAND_UI(130,   OnUpdateBar)
    ON_UPDATE_COMMAND_UI(110,   OnUpdateBar)
    ON_UPDATE_COMMAND_UI(132,   OnUpdateBar)
    ON_MESSAGE(EC_WM_HINT, OnHint)
    ON_MESSAGE(EC_WM_SELUNIT, OnSelUnit)
END_MESSAGE_MAP()

// 「窗口组件箱」(id 110) 的显示控制：**设计器视图要它、程序集视图不要它**。
// 原版实拍对照：shots\orig_designer_activated.png（设计器页，右侧有「基本组件/扩展组件」+
// 图标网格）vs shots\orig_des_full.png（程序集页，右侧什么都没有）。
// 由 CECChildFrame::ShowView() 在切页时调用。
BOOL CMainFrame::IsCompBoxShown() const
{
    CControlBar* p = EcBarById((CMainFrame*)this, 110);
    return (p != NULL && p->IsWindowVisible()) ? TRUE : FALSE;
}

void CMainFrame::ShowCompBox(BOOL bShow)
{
    CControlBar* p = EcBarById(this, 110);
    if (p == NULL)
        return;
    ShowControlBar(p, bShow, FALSE);
    RecalcLayout();
}

// 工具->系统配置（原版命令 #32876）。原版是 6 页对话框（RT_DIALOG id=282），
// 本工程实现了影响代码编辑框外观的「程序显示」页。
void CMainFrame::OnSysConfig()
{
    EcShowSysCfg(this);
}

// 工具->支持库配置（原版命令 #32906）。原版那份对话框的模板是 RT_DIALOG id=321，
// 本工程的 IDD_LIBCFG 照抄它的坐标/控件 ID（见 ec_syscfg.cpp）。
//
// 这里做的是**动态换库**（不需要重启）：
//   1. 让所有设计器先销毁设计期真窗口 —— 那些对象是支持库建出来的，FreeLibrary 之后就是野指针
//   2. EcUnloadAllLibs() 把已载入的 .fne 全部 FreeLibrary 并清空元数据表
//   3. EcLoadLibs(目录, 新勾选表) 重新载入
//   4. EcBuildTree() + PopulatePanes() 重建"工作夹-支持库/程序/属性"三页与组件箱
//   5. EcDesignerRecreateAllForms() 让设计器用新库重新建窗体
// ⚠️ 全程不要泵消息（重建期间全局表是空的，任何一次重绘都可能踩到空表）。
void CMainFrame::OnLibCfg()
{
    CStringArray want;
    if (!EcShowLibCfg(this, m_sLibDir, &want))
        return;
    if (m_sLibDir.IsEmpty())
    {
        AfxMessageBox(_T("没有找到支持库目录（lib\\krnln.fne），无法载入支持库。"));
        return;
    }
    // 核心库必须在表里（原版也一样：核心库恒载入，列表里不出现它）
    BOOL bKrn = FALSE;
    int i;
    for (i = 0; i < want.GetSize(); i++)
        if (want[i] == _T("krnln"))
            bKrn = TRUE;
    if (!bKrn)
        want.InsertAt(0, CString(_T("krnln")));

    CString sLog;
    EcDesignerDestroyAllUnits();
    EcForgetDefaultPropertyData();      // 默认属性数据是从旧 .fne 的代码里序列化出来的，必须清
    EcUnloadAllLibs();
    EcLoadLibs(m_sLibDir, &want, &sLog);
    m_sLibLog = sLog;
    EcBuildTree();
    PopulatePanes();
    EcDesignerRecreateAllForms();

    // 支持库数变了 -> 状态栏提示一下
    CString msg;
    msg.Format(_T("支持库已重新载入：共 %d 个库（核心库 %s）。"),
               EcLibCount(), (EcLibFindByBaseName(_T("krnln")) >= 0) ? _T("已载入") : _T("缺失"));
    if (m_wndStatusBar.GetSafeHwnd() != NULL)
        m_wndStatusBar.SetPaneText(0, msg);
}

// 窗口->全部关闭（原版命令 #32982）：把所有 MDI 文档窗口关掉（原版关完只剩空的工作区）。
void CMainFrame::OnCloseAll()
{
    int nGuard = 0;
    while (nGuard++ < 64)
    {
        CMDIChildWnd* pChild = MDIGetActive();
        if (pChild == NULL || pChild->GetSafeHwnd() == NULL)
            break;
        pChild->SendMessage(WM_CLOSE);
    }
}

// 「工具条/面板开关」用的诊断追踪（实现和声明都在 ec_editor.cpp；这里只用一次）
extern void EcTrace(LPCTSTR pszFmt, ...);

// 工具条 / 面板的显示开关（原版 id = MFC 的 control bar id，见声明处的说明）。
// 每个 id 只对应一个控制条，所以现查就行，不用建表。
static CControlBar* EcBarById(CFrameWnd* pFrame, UINT nID)
{
    if (pFrame == NULL)
        return NULL;
    POSITION pos = pFrame->m_listControlBars.GetHeadPosition();
    while (pos != NULL)
    {
        CControlBar* p = (CControlBar*)pFrame->m_listControlBars.GetNext(pos);
        if (p != NULL && p->GetDlgCtrlID() == (int)nID)
            return p;
    }
    return NULL;
}

void CMainFrame::OnToggleBar(UINT nID)
{
    // ⚠️ 再确认一次 id：ON_COMMAND_RANGE 会正确传参，但走别的路径时 wParam 才是真值
    //（见上面消息映射里的说明）。两个都试，取落在已知集合里的那个。
    UINT w = 0;
    const MSG* pMsg = GetCurrentMessage();
    if (pMsg != NULL)
        w = (UINT)pMsg->wParam;
    if (w == 110 || w == 129 || w == 130 || w == 132 || w == 133 || w == 135 ||
        w == 59392 || w == 59393)
        nID = w;
    EcTrace(_T("bartoggle id=%u"), nID);
    // 用户手动收起/展开「窗口组件箱」时记一笔：ShowView() 靠它决定"切回设计器要不要自动弹出来"
    //（用户自己收起的，就别硬弹）。
    if (nID == 110)
    {
        CControlBar* pComp = EcBarById(this, 110);
        m_bCompBoxUserHidden = (pComp != NULL && pComp->IsWindowVisible()) ? TRUE : FALSE;
    }
    if (nID == 132)                       // 工作与状态组合夹：两个面板一起开关
    {
        CControlBar* pWork = EcBarById(this, 129);
        BOOL bShow = (pWork == NULL) || !pWork->IsWindowVisible();
        ShowControlBar(EcBarById(this, 129), bShow, FALSE);
        ShowControlBar(EcBarById(this, 130), bShow, FALSE);
        RecalcLayout();
        return;
    }
    CControlBar* pBar = EcBarById(this, nID);
    if (pBar == NULL)
        return;
    ShowControlBar(pBar, !pBar->IsWindowVisible(), FALSE);
    RecalcLayout();
}

void CMainFrame::OnUpdateBar(CCmdUI* pCmdUI)
{
    UINT nID = (pCmdUI != NULL) ? pCmdUI->m_nID : 0;
    if (nID == 132)
    {
        CControlBar* pBar = EcBarById(this, 129);
        pCmdUI->SetCheck((pBar != NULL && pBar->IsWindowVisible()) ? 1 : 0);
        return;
    }
    CControlBar* pBar = EcBarById(this, nID);
    pCmdUI->SetCheck((pBar != NULL && pBar->IsWindowVisible()) ? 1 : 0);
}

// 支持库树的提示文本 -> 主框架状态栏第 0 段 + 状态夹的「提示」页（页 0）。
// 原版是**两处都有**：状态栏那一条 1424x19 显示摘要，状态夹的「提示」页显示完整说明
// （`调用格式：〈…〉…` + 逐个参数 + 操作系统需求，见 ec_lib.cpp 的 CmdHint/LibHint）。
LRESULT CMainFrame::OnHint(WPARAM /*wParam*/, LPARAM lParam)
{
    if (m_wndStatusBar.GetSafeHwnd() != NULL)
        m_wndStatusBar.SetPaneText(0, (LPCTSTR)lParam);
    // 「提示」页 = 状态夹的第 0 页（g_szStatusTabs 的第一项），控件 id 同样是页号偏移
    CEdit* pHint = (CEdit*)m_wndStatus.GetPaneItem(IDC_EDIT_OUTPUT);
    if (pHint != NULL && pHint->GetSafeHwnd() != NULL)
    {
        pHint->SetWindowText((LPCTSTR)lParam);
        pHint->SetSel(0, 0);           // 每次从头看，别停在上一次的滚动位置
    }
    return 0;
}

// 把文本写进状态夹的「搜寻1」(页 5) / 「搜寻2」(页 6) 并翻到那一页（整体搜寻的结果）。
// 状态夹的页序见 g_szStatusTabs：提示/输出/调用表/监视板/变量表/搜寻1/搜寻2/剪辑历史；
// 内容控件的 id 是连续的（IDC_EDIT_OUTPUT + 页号）。
void CMainFrame::ShowSearchPane(LPCTSTR pszText, int nPane)
{
    static const int kPage[2] = { 5, 6 };          // 搜寻1 / 搜寻2
    int nPage = kPage[(nPane == 1) ? 1 : 0];
    CEdit* pEd = (CEdit*)m_wndStatus.GetPaneItem(IDC_EDIT_OUTPUT + nPage);
    if (pEd != NULL && pEd->GetSafeHwnd() != NULL)
        pEd->SetWindowText(pszText != NULL ? pszText : _T(""));
    CTabCtrl* pTab = (CTabCtrl*)m_wndStatus.GetPaneItem(IDC_TAB_STATUS);
    if (pTab != NULL && pTab->GetSafeHwnd() != NULL)
        m_wndStatus.GotoPage(nPage);               // TCM_SETCURSEL 不发 TCN_SELCHANGE，得自己换
}

// 设计器选中变了（wParam = MAKELONG(库, 类型)，lParam = 组件对象）-> 刷新"工作夹-属性"页。
// 属性值由支持库的 ITF 6 提供（见 ec_lib.cpp 的 EcGetUnitProperty）。
LRESULT CMainFrame::OnSelUnit(WPARAM wParam, LPARAM lParam)
{
    m_wndWork.FillProps((int)(short)LOWORD(wParam), (int)(short)HIWORD(wParam), (void*)lParam);
    return 0;
}

// 原版菜单条那一行最前面也有一根 gripper 竖杠（iDraw 的菜单条本身就是个停靠条，停靠条的 gripper）。
// 我们用的是原生 HMENU，它画不了 gripper，只能在非客户区自己补一根一样的 3d 竖线。
// 位置取窗口坐标 x=12：和本工程工具条那几行 gripper 的实测位置一致（原版也是 x=12）。
void CMainFrame::OnNcPaint()
{
    CFrameWnd::OnNcPaint();
    if (::GetMenu(GetSafeHwnd()) == NULL)
        return;

    CRect rcWnd, rcClient;
    GetWindowRect(&rcWnd);
    GetClientRect(&rcClient);
    ClientToScreen(&rcClient);
    int nMenuH = ::GetSystemMetrics(SM_CYMENU);      // 一行菜单条的高度
    int top = rcClient.top - nMenuH + 3 - rcWnd.top;
    int bottom = rcClient.top - 3 - rcWnd.top;
    if (bottom <= top)
        return;

    CWindowDC dc(this);                              // 原点 = 窗口左上角
    dc.Draw3dRect(CRect(12, top, 13, bottom),
                  ::GetSysColor(COLOR_BTNHIGHLIGHT), ::GetSysColor(COLOR_BTNSHADOW));
}

void CMainFrame::OnTbCmd(UINT /*nID*/)
{
    // demo：工具条按钮暂无功能，只保证外观
}

void CMainFrame::OnTbUpdate(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(TRUE);
}

static UINT indicators[] =
{
    ID_SEPARATOR,
};

CMainFrame::CMainFrame()
    : m_bCompBoxUserHidden(FALSE)
{
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CMDIFrameWnd::PreCreateWindow(cs))
        return FALSE;
    if (RegisterENewFrameClass())
        cs.lpszClass = _T("ENewFrame");
    return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CMDIFrameWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    // 状态栏：原版为 msctls_statusbar32
    if (!m_wndStatusBar.Create(this) ||
        !m_wndStatusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(UINT)))
    {
        return -1;
    }

    EnableDocking(CBRS_ALIGN_ANY);

    // 第 1 行：标准工具条。位图取自 e.exe 的 RT_BITMAP 131（369x15，23 个按钮）
    // 原版实测：行高 25px、按钮 23px 宽、图标 16x15 上下各留 3px。
    //
    // ⚠️ 不要只用 CToolBar::LoadBitmap：它走 comctl32 的 TB_ADDBITMAP，而 comctl32 是
    //    **自己按位图高度猜图标单元高度**的（这张位图只有 15 行），实测猜歪 2 行 ——
    //    现象是每个按钮上沿多出两条黑带、图标整体被顶掉两行（上一轮截图里就有这毛病）。
    //    这里自己建 CImageList 再 TB_SETIMAGELIST，单元尺寸由我们说了算。
    // ⚠️ 用 CreateEx + TBSTYLE_FLAT，别用 Create：MFC 的 CToolBar::Create 传的 dwCtrlStyle 是 0，
    //    按钮会带"凸起方框"（普通状态就有），原版是**平的**、只有热点/按下才出框。
    if (m_wndToolBar.CreateEx(this, TBSTYLE_FLAT,
                                 WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS |
                                 CBRS_GRIPPER | CBRS_FLYBY, CRect(0, 3, 0, 3),
                                 IDR_TOOLBAR_STANDARD))
    {
        m_wndToolBar.SetSizes(CSize(23, 21), CSize(16, 15));
        m_wndToolBar.SetHeight(23);     // MFC 的 SetHeight(n) 实际得到 n+2 的行高 -> 25
        if (LoadToolbarImageList(m_ilTbStd, IDB_TOOLBAR_STD, 23))
            m_wndToolBar.SendMessage(TB_SETIMAGELIST, 0,
                                     (LPARAM)(HIMAGELIST)m_ilTbStd.GetSafeHandle());
        // 按钮表照抄原版（26 项 = 21 按钮 + 5 分隔条，位图格号见 kStdBtns）
        EcSetToolbarButtons(m_wndToolBar, kStdBtns, sizeof(kStdBtns) / sizeof(kStdBtns[0]),
                            kStdDropdownIdx, sizeof(kStdDropdownIdx) / sizeof(kStdDropdownIdx[0]));
        m_wndToolBar.SetWindowText(_T("标准工具条"));
        m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
        DockControlBar(&m_wndToolBar, AFX_IDW_DOCKBAR_TOP);
    }

    // 第 2 行：对齐工具条。原版这一行图标是"灰的"（位图 296 是彩色、297 是灰版），
    // 直接用 297，和原版截图一致。
    if (m_wndToolBarAlign.CreateEx(this, TBSTYLE_FLAT,
                                      WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS |
                                      CBRS_GRIPPER, CRect(0, 3, 0, 3), IDR_TOOLBAR_ALIGN))
    {
        m_wndToolBarAlign.SetSizes(CSize(23, 21), CSize(16, 15));
        m_wndToolBarAlign.SetHeight(23);
        if (LoadToolbarImageList(m_ilTbAlign, IDB_TOOLBAR_ALIGN, 14))
            m_wndToolBarAlign.SendMessage(TB_SETIMAGELIST, 0,
                                          (LPARAM)(HIMAGELIST)m_ilTbAlign.GetSafeHandle());
        // 按钮表照抄原版（17 项 = 14 按钮 + 3 分隔条；注意位图格 6、5 是反的）
        EcSetToolbarButtons(m_wndToolBarAlign, kAlignBtns, sizeof(kAlignBtns) / sizeof(kAlignBtns[0]), NULL, 0);
        m_wndToolBarAlign.SetWindowText(_T("对齐工具条"));
        m_wndToolBarAlign.EnableDocking(CBRS_ALIGN_ANY);
        DockControlBar(&m_wndToolBarAlign, AFX_IDW_DOCKBAR_TOP);
    }

    // 第 3 行：子程序选择器 + 事件选择器
    if (m_wndSelector.Create(this, IDR_SELECTOR_BAR))
    {
        m_wndSelector.EnableDocking(CBRS_ALIGN_ANY);
        DockControlBar(&m_wndSelector, AFX_IDW_DOCKBAR_TOP);
    }

    // 定位工具条：原版有这一条但默认收起（它那一行被两个下拉框盖住）
    if (m_wndToolBarPos.CreateEx(this, TBSTYLE_FLAT,
                                    WS_CHILD | CBRS_TOP | CBRS_TOOLTIPS | CBRS_GRIPPER,
                                    CRect(0, 3, 0, 3), IDR_TOOLBAR_POSITION))
    {
        m_wndToolBarPos.SetSizes(CSize(23, 21), CSize(16, 15));
        m_wndToolBarPos.SetHeight(23);
        if (LoadToolbarImageList(m_ilTbPos, IDB_TOOLBAR_POS, 17))
            m_wndToolBarPos.SendMessage(TB_SETIMAGELIST, 0,
                                        (LPARAM)(HIMAGELIST)m_ilTbPos.GetSafeHandle());
        UINT ids[17];
        for (int i = 0; i < 17; i++)
            ids[i] = IDC_TB_FIRST + i;
        m_wndToolBarPos.SetButtons(ids, 17);
        m_wndToolBarPos.SetWindowText(_T("定位工具条"));
        m_wndToolBarPos.EnableDocking(CBRS_ALIGN_ANY);
        DockControlBar(&m_wndToolBarPos, AFX_IDW_DOCKBAR_TOP);
        ShowControlBar(&m_wndToolBarPos, FALSE, FALSE);     // 与原版一致：默认收起
    }

    // 工作夹(左) —— 原版 ID 129，标签页在底部
    if (m_wndWork.CreatePane(this, IDD_WORKPANE, ID_VIEW_WORKPANE,
                             IDC_TAB_WORK, g_szWorkTabs, g_nWorkTabIcons, 3, IDC_TREE_WORK,
                             229, FALSE, TRUE, IDB_TABICON_WORK))
    {
        m_wndWork.SetWindowText(_T("工作夹"));
        m_wndWork.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT);
        DockControlBar(&m_wndWork, AFX_IDW_DOCKBAR_LEFT);
    }

    // 状态夹(下) —— 原版 ID 130，标签页在顶部
    if (m_wndStatus.CreatePane(this, IDD_STATUSPANE, ID_VIEW_STATUSPANE,
                               IDC_TAB_STATUS, g_szStatusTabs, g_nStatusTabIcons, 8,
                               IDC_EDIT_OUTPUT, 188, TRUE, FALSE, IDB_TABICON_STATUS))
    {
        m_wndStatus.SetWindowText(_T("状态夹"));
        m_wndStatus.EnableDocking(CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM);
        DockControlBar(&m_wndStatus, AFX_IDW_DOCKBAR_BOTTOM);
    }

    // 窗口组件箱(右) —— 原版 ID 110，默认收起。
    // 宽度取原版的**展开后内层尺寸**：原版控制条展开时内层 Afx 宽 135、#32770 客户区 126
    // （实测 shots\live_compbox\dlg_children.txt）。本工程控制条含边框，取 135 让客户区接近 126。
    if (m_wndComp.CreatePane(this, IDD_COMPPANE, ID_VIEW_COMPPANE, 0, NULL, NULL, 0, 0,
                             135, FALSE, FALSE, 0))
    {
        m_wndComp.SetWindowText(_T("窗口组件箱"));
        m_wndComp.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT);
        DockControlBar(&m_wndComp, AFX_IDW_DOCKBAR_RIGHT);
        ShowControlBar(&m_wndComp, FALSE, FALSE);   // 原版实测：窗口组件箱默认收起
    }

    // 支持库树的图标（原版来自 krnln.fne，e.exe 里没有，是从原版截图抠的）。
    // ★ 第 24 轮：从 24 格扩到 **31 格**（0..30），因为节点现在直接用支持库给的
    //   **4 位图标号前缀** 当 image index（最大 30），见 EcCatIconOf 的说明。
    // ⚠️ 条带必须是 24bpp DIB：`CImageList::Add(&bmp, mask)` 对 4bpp/8bpp 才走颜色表掩码那条路，
    //    24bpp 时它**直接用 mask 参数**，所以这里给白色。
    if (m_ilTree.Create(16, 16, ILC_COLOR24 | ILC_MASK, 31, 1))
    {
        CBitmap bmp;
        if (bmp.LoadBitmap(IDB_TREE_ICONS))
            m_ilTree.Add(&bmp, RGB(255, 255, 255));
    }

    // 「程序」页树的图标（同样从原版 1:1 截图抠，15 个 16x16）
    if (m_ilProg.Create(16, 16, ILC_COLOR24 | ILC_MASK, 15, 1))
    {
        CBitmap bmp;
        if (bmp.LoadBitmap(IDB_PROGTREE_ICONS))
            m_ilProg.Add(&bmp, RGB(255, 255, 255));
    }

    // 真载入支持库（原版只把"当前程序用到的库"放进工作夹树，默认就是核心库）
    if (FindLibDir(m_sLibDir))
    {
        CStringArray load;
        load.Add(_T("krnln"));
        // 额外支持库（可选）：ECLONE_EXTRA_LIBS=iext2;EChartBar
        // 这些库里的**可视组件**会出现在「窗口组件箱 -> 扩展组件」里，可以放到设计窗体上。
        // 默认不载入：第三方 .fne 的 GetNewInf/接口函数不是都经得起在 IDE 里直接调
        //（实测 spec.fne / shellEx.fne 会崩），所以只让用户显式指定。
        char extra[512];
        if (::GetEnvironmentVariableA("ECLONE_EXTRA_LIBS", extra, sizeof(extra)) > 0)
        {
            char* p = extra;
            while (p != NULL && *p != '\0')
            {
                char* q = strchr(p, ';');
                if (q != NULL)
                    *q = '\0';
                CString nm(p);
                if (!nm.IsEmpty())
                    load.Add(nm);
                p = (q != NULL) ? q + 1 : NULL;
            }
        }
        EcLoadLibs(m_sLibDir, &load, &m_sLibLog);
    }
    else
        m_sLibLog = _T("未找到支持库目录（lib\\krnln.fne）\r\n");
    EcBuildTree();

    PopulatePanes();
    EcDumpDtIconsIfAsked();   // ECLONE_DUMP_DTICON=<path>：组件箱图标字段（m_nBmpID）诊断（必须在载库之后）
    return 0;
}

// ------------------------------------------------------------------ 面板内容
// 「工作夹->支持库」树的数据来自运行期真正载入的支持库（见 ec_lib.cpp）。
// 原版实测（shots\orig_tree_libs.txt，1560 项）的结构：
//   支持库 -> 命令类别 -> 命令
//           -> 数据类型 -> 成员命令 / 成员属性
//           -> 常量
// 过滤规则也是照抄：隐含命令(CT_IS_HIDED)与隐含数据类型(LDT_IS_HIDED)不进树，
// 空类别（核心库的"易向导"3 条全是隐含命令）整个不出现。

// 支持库目录：原版从 <安装目录>\lib 载入。先找 exe 旁边的 lib，再退回本机易语言安装目录。
static BOOL FindLibDir(CString& out)
{
    TCHAR sz[MAX_PATH];
    ::GetModuleFileName(NULL, sz, MAX_PATH);
    CString dir = sz;
    int p = dir.ReverseFind(_T('\\'));
    if (p >= 0)
        dir = dir.Left(p);

    CString cand;
    cand.Format(_T("%s\\lib"), (LPCTSTR)dir);
    if (::GetFileAttributes(cand + _T("\\krnln.fne")) != 0xFFFFFFFF)
    {
        out = cand;
        return TRUE;
    }
    cand = _T("C:\\Program Files (x86)\\e\\lib");
    if (::GetFileAttributes(cand + _T("\\krnln.fne")) != 0xFFFFFFFF)
    {
        out = cand;
        return TRUE;
    }
    return FALSE;
}

// ------------------------------------------------------------------ 「工作夹->程序」页树
// 节点构成照抄原版实测（shots\live_progtree\prog_tree.txt + tree0.png，最小工程 11 个节点）：
//   程序数据(img8)
//     启动窗口程序集(img3) -> 子程序(img2)...
//     窗口(img6) -> _启动窗口(img6)
//     常量表...(img9)
//     资源表(img12) -> 声音...(img10) / 图片或图片组...(img11)
//     模块引用表(img14)
//     外部文件记录表(img13)
// 实测的两条规则：
//   · 全局变量 / 自定义数据类型 / Dll命令 这三类【有内容才出现】——最小工程里一个都没有；
//     用户那张 精易模块 截图（shots\ref_prog_tree.png）里三者都在，说明是"非空才建节点"。
//     本工程目前只有 Dll命令 有内容，所以就只多这一个节点。
//   · 窗口程序集 与 普通程序集 的图标不同（img3 菱形 / 黄色文件夹），见 tools\extract_progtree_icons.py
// item data = 文档页下标（选中时切页），-1 = 还没有对应文档页。
static CString EcDeclName(const CString& line, LPCTSTR pszHead)
{
    // line 形如 ".子程序 加法, 整数型" / ".程序集 启动窗口程序集"，返回逗号/空格后的第一个词
    CString rest = line.Mid(1);
    CString head = rest;
    int sp = -1;
    for (int i = 0; i < rest.GetLength(); i++)
    {
        unsigned char c = (unsigned char)rest[i];
        if (c == ' ' || c == '\t') { sp = i; break; }
    }
    if (sp < 0) return _T("");
    head = rest.Left(sp);
    if (head != pszHead) return _T("");
    CString body = rest.Mid(sp);
    body.TrimLeft();
    int e = 0;
    while (e < body.GetLength() && body[e] != _T(',') && (unsigned char)body[e] != 0xA3) e++;
    CString name = body.Left(e);
    name.TrimRight();
    return name;
}

static BOOL EcTextHasLine(LPCTSTR psz, LPCTSTR pszHead)
{
    CString s(psz);
    int a = 0;
    while (a <= s.GetLength())
    {
        int b = s.Find(_T('\n'), a);
        CString line = (b < 0) ? s.Mid(a) : s.Mid(a, b - a);
        a = (b < 0) ? s.GetLength() + 1 : b + 1;
        line.TrimLeft(); line.TrimRight();
        if (line.GetLength() > 1 && line[0] == _T('.') && !EcDeclName(line, pszHead).IsEmpty())
            return TRUE;
    }
    return FALSE;
}

// 「全局变量 / 自定义数据类型」：原版是**有内容才出现**的节点，子项 = 每个声明名，
// 子项与父节点同图标（shots\live_jy\prog_tree.txt 实测：全局变量与子项都是 img=1，
// 自定义数据类型与子项都是 img=0）。pszHead = 源码里的声明头（"全局变量" / "数据类型"）。
static void EcInsertMemberNodes(CTreeCtrl* pTree, HTREEITEM hRoot, LPCTSTR pszAsm,
                                LPCTSTR pszHead, LPCTSTR pszNodeName, int nImg, int nDoc)
{
    CString s(pszAsm);
    HTREEITEM hParent = NULL;
    int a = 0;
    while (a <= s.GetLength())
    {
        int b = s.Find(_T('\n'), a);
        CString line = (b < 0) ? s.Mid(a) : s.Mid(a, b - a);
        a = (b < 0) ? s.GetLength() + 1 : b + 1;
        line.TrimLeft(); line.TrimRight();
        if (line.GetLength() < 2 || line[0] != _T('.'))
            continue;
        CString name = EcDeclName(line, pszHead);
        if (name.IsEmpty())
            continue;
        if (hParent == NULL)
        {
            // 父节点（全局变量 / 自定义数据类型）点了也开对应的表页（原版实测会开）
            hParent = pTree->InsertItem(pszNodeName, nImg, nImg, hRoot, TVI_LAST);
            pTree->SetItemData(hParent, (DWORD)nDoc);
        }
        HTREEITEM hItem = pTree->InsertItem(name, nImg, nImg, hParent, TVI_LAST);
        pTree->SetItemData(hItem, (DWORD)nDoc);
    }
}

static void EcBuildProgTree(CTreeCtrl* pTree, LPCTSTR pszAsm, LPCTSTR pszDll)
{
    pTree->DeleteAllItems();
    HTREEITEM hRoot = pTree->InsertItem(_T("程序数据"), EP_ROOT, EP_ROOT, TVI_ROOT, TVI_LAST);
    pTree->SetItemData(hRoot, (DWORD)-1);

    // 扫「.文件夹 / .程序集 / .类 / .子程序」，按出现顺序建树。
    // 原版实测（shots\live_jy\prog_tree.txt，精易模块.e 全树倒档）：
    //   窗口程序集 与 普通程序集 是**同一个**菱形图标（img=3）；
    //   黄色文件夹（img=16）是「文件夹」节点（作者在 IDE 里建的分类，程序集/类挂在它下面）；
    //   类模块 与 自定义数据类型 也是同一个图标（都是 img=0）。
    CString s(pszAsm);
    int a = 0;
    HTREEITEM hAsm = NULL;
    HTREEITEM hFolder = NULL;
    while (a <= s.GetLength())
    {
        int b = s.Find(_T('\n'), a);
        CString line = (b < 0) ? s.Mid(a) : s.Mid(a, b - a);
        a = (b < 0) ? s.GetLength() + 1 : b + 1;
        line.TrimLeft(); line.TrimRight();
        if (line.GetLength() < 2 || line[0] != _T('.'))
            continue;
        // 「.文件夹 名」之后的 .程序集 / .类 都挂到这个文件夹下面（原版就是这样分组的）；
        // 一个**不带名字**的 `.文件夹` 表示分组结束（文本形式原本没有文件夹，
        // 这是本工程为复刻原版树加的写法，只影响程序树，编辑器里不画这两行）
        // 注意：这里不能用 ec_editor.cpp 里的 EcHeadWord/EcTrim（static，另一个编译单元）
        if (line.GetLength() >= 7 && line.Mid(1, 6) == _T("文件夹"))
        {
            CString fname = EcDeclName(line, _T("文件夹"));
            if (fname.IsEmpty())
            {
                hFolder = NULL;
            }
            else
            {
                hFolder = pTree->InsertItem(fname, EP_FOLDER, EP_FOLDER, hRoot, TVI_LAST);
                pTree->SetItemData(hFolder, (DWORD)-1);
            }
            hAsm = NULL;
            continue;
        }
        CString name = EcDeclName(line, _T("程序集"));
        if (!name.IsEmpty())
        {
            hAsm = pTree->InsertItem(name, EP_ASM, EP_ASM, hFolder ? hFolder : hRoot, TVI_LAST);
            pTree->SetItemData(hAsm, ECDOC_ASM);
            continue;
        }
        name = EcDeclName(line, _T("类"));
        if (!name.IsEmpty())
        {
            HTREEITEM hCls = pTree->InsertItem(name, EP_CLASS, EP_CLASS,
                                              hFolder ? hFolder : hRoot, TVI_LAST);
            pTree->SetItemData(hCls, (DWORD)-1);
            continue;
        }
        name = EcDeclName(line, _T("子程序"));
        if (!name.IsEmpty() && hAsm != NULL)
        {
            HTREEITEM hSub = pTree->InsertItem(name, EP_SUB, EP_SUB, hAsm, TVI_LAST);
            pTree->SetItemData(hSub, ECDOC_ASM);
        }
    }

    HTREEITEM hForm = pTree->InsertItem(_T("窗口"), EP_FORM, EP_FORM, hRoot, TVI_LAST);
    pTree->SetItemData(hForm, (DWORD)-1);
    // 「切换窗体」：窗口节点除了文档页下标，还要带**窗体下标**（存在 item data 的高 16 位）。
    // 源码里每个 `.窗口 名字` 都会在树里出现一个节点，点它 -> 切到对应的设计窗体。
    // 没写 `.窗口` 时仍然给一个 `_启动窗口`（本工程样本就是这种），它指第 0 个窗体。
    HTREEITEM hWin = pTree->InsertItem(_T("_启动窗口"), EP_WINDOW, EP_WINDOW, hForm, TVI_LAST);
    pTree->SetItemData(hWin, (DWORD)(ECDOC_FORM | (0 << 16)));
    {
        int nWin = 0;
        int p = 0;
        while (p <= s.GetLength())
        {
            int e = s.Find(_T('\n'), p);
            CString ln = (e < 0) ? s.Mid(p) : s.Mid(p, e - p);
            p = (e < 0) ? s.GetLength() + 1 : e + 1;
            ln.TrimLeft(); ln.TrimRight();
            if (ln.GetLength() < 2 || ln[0] != _T('.'))
                continue;
            CString wname = EcDeclName(ln, _T("窗口"));
            if (wname.IsEmpty())
                continue;
            nWin++;
            HTREEITEM hw = pTree->InsertItem(wname, EP_WINDOW, EP_WINDOW, hForm, TVI_LAST);
            // 高 16 位 = 窗体下标（最多 16 个；够用），低 16 位 = ECDOC_FORM
            pTree->SetItemData(hw, (DWORD)(ECDOC_FORM | ((nWin & 0xFFFF) << 16)));
        }
    }

    // 全局变量 / 自定义数据类型：「有内容才出现」，子项与父节点同图标
    // （原版倒档 shots\live_jy\prog_tree.txt：全局变量与其子项都是 img=1，
    //   自定义数据类型与其子项都是 img=0），位置在 窗口 之后、Dll命令 之前。
    EcInsertMemberNodes(pTree, hRoot, pszAsm, _T("全局变量"), _T("全局变量"), EP_GLOBAL, ECDOC_GVAR);
    EcInsertMemberNodes(pTree, hRoot, pszAsm, _T("数据类型"), _T("自定义数据类型"), EP_TYPE, ECDOC_TYPE);

    if (EcTextHasLine(pszDll, _T("DLL命令")))
    {
        HTREEITEM hDll = pTree->InsertItem(_T("Dll命令"), EP_DLL, EP_DLL, hRoot, TVI_LAST);
        pTree->SetItemData(hDll, ECDOC_DLL);
    }

    HTREEITEM hConst = pTree->InsertItem(_T("常量表..."), EP_CONST, EP_CONST, hRoot, TVI_LAST);
    // 原版实测（tools\probe_page2m.ps1 真鼠标点节点，标题栏跟着变）：
    //   全局变量 -> [全局变量表]      自定义数据类型 -> [自定义数据类型表]
    //   常量表...-> [常量数据表]      声音...-> [声音资源表]   图片或图片组...-> [图片资源表]
    //   资源表（父节点）/ 模块引用表 / 外部文件记录表 点了**不开页**（后两个在本工程样本里是空的）
    pTree->SetItemData(hConst, ECDOC_CONST);

    HTREEITEM hRes = pTree->InsertItem(_T("资源表"), EP_RESOURCE, EP_RESOURCE, hRoot, TVI_LAST);
    pTree->SetItemData(hRes, (DWORD)-1);
    HTREEITEM h1 = pTree->InsertItem(_T("声音..."), EP_RES_SOUND, EP_RES_SOUND, hRes, TVI_LAST);
    pTree->SetItemData(h1, ECDOC_SND);
    HTREEITEM h2 = pTree->InsertItem(_T("图片或图片组..."), EP_RES_IMAGE, EP_RES_IMAGE, hRes, TVI_LAST);
    pTree->SetItemData(h2, ECDOC_IMG);

    HTREEITEM hMod = pTree->InsertItem(_T("模块引用表"), EP_MODREF, EP_MODREF, hRoot, TVI_LAST);
    pTree->SetItemData(hMod, (DWORD)-1);
    HTREEITEM hExt = pTree->InsertItem(_T("外部文件记录表"), EP_EXTFILE, EP_EXTFILE, hRoot, TVI_LAST);
    pTree->SetItemData(hExt, (DWORD)-1);

    pTree->Expand(hRoot, TVE_EXPAND);          // 原版根节点是展开的
}

static const TCHAR* g_szComponents[] = {
    _T("窗口"), _T("编辑框"), _T("图片框"), _T("外形框"), _T("画板"), _T("分组框"),
    _T("标签"), _T("按钮"), _T("选择框"), _T("单选框"), _T("组合框"), _T("列表框"),
    _T("选择列表框"), _T("横向滚动条"), _T("纵向滚动条"), _T("进度条"), _T("滑块条"),
    _T("选择夹"), _T("影像框"), _T("日期框"), _T("月历"), _T("驱动器框"), _T("目录框"),
    _T("文件框"), _T("颜色选择器"), _T("超级链接框"), _T("调节器"), _T("通用对话框"),
    _T("时钟"), _T("打印机"), _T("表格"), _T("数据源"), _T("通用提供者"),
    _T("数据库提供者"), _T("图形按钮"), _T("外部数据库"), _T("外部数据提供者"),
    _T("数据报"), _T("客户"), _T("服务器"), _T("端口"), _T("菜单"), NULL
};

// 支持库类别串形如 `0014算术运算`：**前 4 位就是图标号**（原版拿它当 image index），
// 后面才是显示名。0000 表示"没有图标"（原版树里 img=0）。
// 证据：`tools\libprobe.exe "C:\Program Files (x86)\e\lib\krnln.fne"` 的 `-- categories --`，
//      与 `shots\live_page2\trees.txt` 里每个类别的 img= 逐个相等（详见 tools\cat_order.py）。
// ⚠️ 别改回"按第几个类别数 1..23"：显示顺序可能与串表顺序不同（实测 时间操作/字节集操作
//    两个类别换位），那样图标会整体错位。
void CMainFrame::PopulatePanes()
{
    // 组件箱的列表现在由支持库生成（见 CPaneBar::FillComponentBox），g_szComponents 只留作对照清单
    // 工作夹 - 支持库页
    CTreeCtrl* pLib = (CTreeCtrl*)m_wndWork.GetPaneItem(IDC_TREE_WORK);
    if (pLib != NULL)
    {
        // ⚠️ 必须先清空：PopulatePanes 现在会被「支持库配置」在运行期再调一次（动态换库），
        //    不清就会把整棵树再插一遍（实测 1560 -> 3120 -> 4680）。
        pLib->DeleteAllItems();
        if (m_ilTree.GetSafeHandle() != NULL)
            pLib->SetImageList(&m_ilTree, TVSIL_NORMAL);

        // 节点表由 ec_lib.cpp 按 lib2.h 的 LIB_INFO 生成；item data = 节点下标，
        // 选中时 CPaneBar::OnLibTreeSelChanged 用它换状态栏提示。
        // 节点表本身就是 DFS 序，所以只要记住当前父节点就行。
        HTREEITEM hLib = TVI_ROOT, hCmdCat = TVI_ROOT, hDtCat = TVI_ROOT;
        HTREEITEM hDt = TVI_ROOT, hConstCat = TVI_ROOT;

        for (int i = 0; i < EcTreeCount(); i++)
        {
            const EcTreeNode& n = EcTreeNodeAt(i);
            HTREEITEM hParent = TVI_ROOT;
            // 图标号由 ec_lib.cpp 在**建节点时**算好并存进 EcTreeNode::icon
            // （支持库类别串的 4 位前缀 = 原版的 image index；见 CatIconNo 的说明）。
            // ⚠️ 别在这里从 n.text 反解：那是 CatDisplay() 之后的显示名，前缀已经没了 ——
            //    第一版就是这么写的，结果所有类别都落到 0、整棵树显示同一个图标。
            // ⚠️ 也别"按第几个类别数 1..23"：显示顺序与串表顺序可能不同，图标会整体错位。
            int nImg = ((n.icon >= 0) && (n.icon <= 30)) ? n.icon : I_IMAGENONE;

            switch (n.kind)
            {
            case EN_LIB:       hParent = TVI_ROOT;   break;
            case EN_CMD_CAT:   hParent = hLib;       break;
            case EN_CMD:       hParent = hCmdCat;                 break;
            case EN_DT_CAT:    hParent = hLib;                    break;
            case EN_DT:        hParent = hDtCat;                  break;
            case EN_MEMBER:
            case EN_PROP:
            case EN_EVENT:
            case EN_ELEM:      hParent = hDt;                     break;
            case EN_CONST_CAT: hParent = hLib;                    break;
            case EN_CONST:     hParent = hConstCat;               break;
            }

            HTREEITEM h = pLib->InsertItem(n.text, nImg, nImg, hParent, TVI_LAST);
            pLib->SetItemData(h, (DWORD)i);

            switch (n.kind)
            {
            case EN_LIB:       hLib = h;       hCmdCat = hDtCat = hConstCat = hDt = TVI_ROOT; break;
            case EN_CMD_CAT:   hCmdCat = h;    break;
            case EN_DT_CAT:    hDtCat = h;     break;
            case EN_DT:        hDt = h;        break;
            case EN_CONST_CAT: hConstCat = h;  break;
            }
        }
        if (hLib != TVI_ROOT)
            pLib->Expand(hLib, TVE_EXPAND);    // 原版默认只展开支持库这一层
    }

    // 工作夹 - 程序页（节点构成照抄原版实测，见函数头注释）
    CTreeCtrl* pPrg = (CTreeCtrl*)m_wndWork.GetPaneItem(IDC_TREE_WORK + 1);
    if (pPrg != NULL)
    {
        if (m_ilProg.GetSafeHandle() != NULL)
            pPrg->SetImageList(&m_ilProg, TVSIL_NORMAL);
        EcBuildProgTree(pPrg, EcSrcProg(), EcSrcDll());
    }

    // 工作夹 - 属性页：列由支持库的属性元数据生成（见 CPaneBar::FillProps），
    // 启动时还没选中任何东西 -> 先清空，设计器建好后会发 EC_WM_SELUNIT 显示窗体属性
    CListCtrl* pProp = (CListCtrl*)m_wndWork.GetPaneItem(IDC_TREE_WORK + 2);
    if (pProp != NULL)
    {
        // 同上：动态换库时会再进来一次，列要先删干净再建
        while (pProp->DeleteColumn(0))
            ;
        pProp->InsertColumn(0, _T("属性"), LVCFMT_LEFT, 70);
        pProp->InsertColumn(1, _T("值"), LVCFMT_LEFT, 130);
        m_wndWork.FillProps(-1, -1, NULL);
    }

    // 状态夹 - 输出页
    CEdit* pOut = (CEdit*)m_wndStatus.GetPaneItem(IDC_EDIT_OUTPUT + 1);
    if (pOut != NULL)
    {
        CString s = m_sLibLog;
        if (s.IsEmpty())
            s = _T("未载入任何支持库\r\n");
        s += m_sLibDir;
        s += _T("\r\n");
        pOut->SetWindowText(s);
    }

    CEdit* pHint = (CEdit*)m_wndStatus.GetPaneItem(IDC_EDIT_OUTPUT);
    if (pHint != NULL)
        pHint->SetWindowText(_T("就绪"));

    // 窗口组件箱：组件按钮来自已载入的支持库（"基本组件" = 核心库）
    // ⚠️ 只在还没建过的时候调一次：它会往组下拉框里塞条目并 SetCurSel(0)，
    //    而 SetCurSel 会发 LBN_SELCHANGE -> OnCompGroup -> 又调 FillComponentBox。
    //    先判空（下拉框有内容就说明建过了），避免"载库/换库时把列表塞两遍"。
    CListBox* pCompHead = (CListBox*)m_wndComp.GetPaneItem(IDC_TAB_COMP);
    if (pCompHead == NULL || pCompHead->GetCount() == 0)
        m_wndComp.FillComponentBox(0);
}

extern void EcTrace(LPCTSTR pszFmt, ...);   // 定义在 ec_editor.cpp（诊断追踪）

// ------------------------------------------------------------------ MDI 子窗口（代码编辑器）
class CECChildFrame : public CMDIChildWnd
{
    DECLARE_DYNAMIC(CECChildFrame)
public:
    CECChildFrame();

protected:
    CECTabStrip m_tabs;
    CECEditor   m_editor;
    CECDesigner m_designer;
    int         m_nDoc;         // 当前文档页（ECDOC_*）
    int         m_nTabOfDoc[ECDOC_COUNT];   // 页 -> 标签条下标（-1 = 还没打开过）

    void ShowView(int n);
    void OpenDoc(int n);        // 切文档页（标签条 + 视图 + 编辑器内容）
    void OpenDoc(int n, int nForm);   // 同上，并切到第 nForm 个设计窗体（-1 = 不动窗体）
    void SetDocTab(int n);            // 只切标签条/标题/视图，**不动编辑器内容**（见实现说明）
    void LayoutViews();
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    // 「切换窗体」的键盘入口：设计器页上按 Ctrl+Tab 在所有窗体之间轮换
    // （原版是一个窗体一个文档页，本工程所有窗体共用一个设计器页，见 HANDOFF §2.11e）。
    // ⚠️ 为什么放这里而不是覆盖层的 OnKeyDown：跨进程 PostMessage 的 WM_KEYDOWN 到不了覆盖层
    //    （它既没有焦点、也不是消息队列焦点），实测 `SurfaceKey` 一次都没被调到。
    //    子框架的 PreTranslateMessage 是消息泵的必经之路，稳定。
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg LRESULT OnTabSel(WPARAM w, LPARAM l);
    afx_msg LRESULT OnOpenDoc(WPARAM w, LPARAM l);
    afx_msg LRESULT OnArmUnit(WPARAM w, LPARAM l);
    // 「编辑」菜单：菜单命令先送到**活动 MDI 子窗口**（MFC 的命令路由），子窗口再转给编辑器。
    // 用的是原版 menu_main.rc 里的命令 id：
    //   57643 撤消 Ctrl+Z / 57644 重复 Ctrl+Y / 32939 删除行 F10 /
    //   32936 屏蔽 Ctrl+K / 32937 解除屏蔽 Ctrl+M /
    //   32927 收缩 / 32932 展开子程序 / 32972 展开所有子程序
    afx_msg void OnEditUndo();
    afx_msg void OnEditRedo();
    afx_msg void OnEditDelLine();
    afx_msg void OnEditMask();
    afx_msg void OnEditUnmask();
    afx_msg void OnEditCollapse();
    afx_msg void OnEditExpandSub();
    afx_msg void OnEditExpandAll();
    afx_msg void OnEditFindDlg();        // 编辑->开始寻找 Ctrl+F (#32877)
    afx_msg void OnEditReplace();        // 编辑->寻找替换 Ctrl+H (#3)
    afx_msg void OnEditBookmark();       // 编辑->设置或取消书签 Ctrl+F6 (#32964)
    afx_msg void OnEditBookPrev();       // 编辑->到前书签 Shift+F6      (#32967)
    afx_msg void OnEditBookNext();       // 编辑->到后书签 F6            (#32968)
    afx_msg void OnEditBookmarkDlg();    // 编辑->书签跳转 Ctrl+F7       (#32965)
    afx_msg void OnDebugToggleBp();      // 运行->断点 F9                (#32855)
    afx_msg void OnDebugClearBp();       // 运行->清除所有断点           (#32858)
    afx_msg void OnEditFindNext();       // 编辑->寻找下一个 F3   (#32878)
    afx_msg void OnEditFindPrev();       // 编辑->寻找上一个 Shift+F3 (#32879)
    // 编辑->整体搜寻 Ctrl+Alt+F (#32934)：原版「整体搜寻对话框」RT_DIALOG id=30773，
    // 搜到的结果写进状态夹的「搜寻1 / 搜寻2」两页（输出夹）。
    afx_msg void OnEditWholeFind();
    void DoWholeFind();
    // 编辑->加到当前块首尾 (#32800-32806) / 转换为 -> (#32807-32813)
    afx_msg void OnEditWrapBlock(UINT nID);
    afx_msg void OnEditConvBlock(UINT nID);
    // 「窗口」菜单里的表页命令（自定义数据类型表/全局变量表/Dll命令定义表/常量数据表/
    // 图片资源表/声音资源表 = #32817..#32827）：切到对应的文档页。
    // ⚠️ 无参（AfxSig_vv），取 id 用 GetCurrentMessage()->wParam。
    afx_msg void OnViewTablePage();
    // 插入菜单（子程序/程序集/窗口/…）。⚠️ ON_COMMAND 的签名是 AfxSig_vv ——
    // 处理函数**必须无参**（写成 `void f(UINT nID)` 会收到垃圾值，见下方的取 id 办法）。
    afx_msg void OnInsertDecl();
    void DoInsertDecl(UINT nID);        // 真正的实现（13 个 ON_COMMAND 都指向它）
    // 「插入->窗口」(#32831)：除了插一行 `.窗口 窗口N`，**还要真的多建一个设计窗体**
    // ——就是 HANDOFF 里"设计器：新建窗体/切换窗体"那一项。
    afx_msg void OnInsertWindow();
    afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
    afx_msg void OnUpdateEditRedo(CCmdUI* pCmdUI);
    DECLARE_MESSAGE_MAP()
};

IMPLEMENT_DYNAMIC(CECChildFrame, CMDIChildWnd)

BEGIN_MESSAGE_MAP(CECChildFrame, CMDIChildWnd)
    ON_WM_SIZE()
    ON_MESSAGE(EC_WM_TABSEL, OnTabSel)
    ON_MESSAGE(EC_WM_OPENDOC, OnOpenDoc)
    ON_MESSAGE(EC_WM_ARMUNIT, OnArmUnit)
    ON_COMMAND(57643, OnEditUndo)
    ON_COMMAND(57644, OnEditRedo)
    ON_COMMAND(32939, OnEditDelLine)
    ON_COMMAND(32936, OnEditMask)
    ON_COMMAND(32937, OnEditUnmask)
    ON_COMMAND(32927, OnEditCollapse)
    ON_COMMAND(32932, OnEditExpandSub)
    ON_COMMAND(32972, OnEditExpandAll)
    ON_COMMAND(32877, OnEditFindDlg)
    ON_COMMAND(3, OnEditReplace)
    ON_COMMAND(32964, OnEditBookmark)
    ON_COMMAND(32967, OnEditBookPrev)
    ON_COMMAND(32968, OnEditBookNext)
    ON_COMMAND(32965, OnEditBookmarkDlg)
    ON_COMMAND(32855, OnDebugToggleBp)
    ON_COMMAND(32858, OnDebugClearBp)
    ON_COMMAND(32878, OnEditFindNext)
    ON_COMMAND(32879, OnEditFindPrev)
    ON_COMMAND(32934, OnEditWholeFind)   // 整体搜寻 Ctrl+Alt+F
    // 加到当前块首尾 = #32800..32806；转换为 = #32807..32813（各自 7 条，顺序与菜单一致）
    ON_COMMAND_RANGE(32800, 32806, OnEditWrapBlock)
    ON_COMMAND_RANGE(32807, 32813, OnEditConvBlock)
    // 「窗口」菜单的表页命令（见 OnViewTablePage）
    ON_COMMAND(32817, OnViewTablePage)   // 自定义数据类型表
    ON_COMMAND(32818, OnViewTablePage)   // 全局变量表
    ON_COMMAND(32819, OnViewTablePage)   // Dll命令定义表
    ON_COMMAND(32825, OnViewTablePage)   // 常量数据表
    ON_COMMAND(32826, OnViewTablePage)   // 图片资源表
    ON_COMMAND(32827, OnViewTablePage)   // 声音资源表
    // 插入菜单：这些 id **不连续**（见 src\menu_main.rc 的「插入」弹出），逐条登记
    ON_COMMAND(32777, OnInsertDecl)   // 子程序
    ON_COMMAND(32778, OnInsertDecl)   // 数据类型
    ON_COMMAND(32779, OnInsertDecl)   // 全局变量 Ctrl+G
    ON_COMMAND(32780, OnInsertDecl)   // Dll命令
    ON_COMMAND(32782, OnInsertDecl)   // 程序集
    ON_COMMAND(32783, OnInsertDecl)   // 现行单元 Ctrl+N
    ON_COMMAND(32786, OnInsertDecl)   // 参数
    ON_COMMAND(32787, OnInsertDecl)   // 局部变量 Ctrl+L
    ON_COMMAND(32828, OnInsertDecl)   // 常量
    ON_COMMAND(32829, OnInsertDecl)   // 图片或图片组
    ON_COMMAND(32830, OnInsertDecl)   // 声音
    ON_COMMAND(32831, OnInsertWindow) // 窗口：插声明 + **真的新建一个设计窗体**（见下）
    ON_COMMAND(32929, OnInsertDecl)   // 类模块
    ON_UPDATE_COMMAND_UI(57643, OnUpdateEditUndo)
    ON_UPDATE_COMMAND_UI(57644, OnUpdateEditRedo)
END_MESSAGE_MAP()

CECChildFrame::CECChildFrame() : m_nDoc(ECDOC_ASM)
{
}

// 原版 MDI 子窗口的窗口类是 AfxMDIFrame42s（Spy 实测），这里对齐
BOOL CECChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CMDIChildWnd::PreCreateWindow(cs))
        return FALSE;
    cs.lpszClass = _T("AfxMDIFrame42s");   // 基类已注册过这个类
    return TRUE;
}

BOOL CECChildFrame::OnCreateClient(LPCREATESTRUCT, CCreateContext*)
{
    // 注意：视图不用 AFX_IDW_PANE_FIRST —— 否则 MFC 的 RecalcLayout 会把视图铺满客户区，
    // 压掉我们自己的文档标签条。这里自己布局。
    if (!m_tabs.Create(this, 2003))
        return FALSE;
    // 文档页标签。原版的标签条**只列已经打开过的文档**（新点的节点才加一个标签），
    // 所以这里也懒加：m_nTabOfDoc[页] = -1 表示这个页还没打开过。
    // 标签文字照抄原版实测（shots\live_tabs\tabbar0.png：`_启动窗口` / `启动窗口程序集`；
    // 表页的标签文字 = 页名，见文档页那一行的注释）
    for (int i = 0; i < ECDOC_COUNT; i++)
        m_nTabOfDoc[i] = -1;
    m_nTabOfDoc[ECDOC_FORM] = m_tabs.AddTab(_T("_启动窗口"));
    m_nTabOfDoc[ECDOC_ASM]  = m_tabs.AddTab(_T("启动窗口程序集"));

    if (!m_designer.Create(this, 2002))
        return FALSE;
    if (!m_editor.Create(this, 2001))
        return FALSE;

    int nStart = g_bStartDesigner ? ECDOC_FORM : ECDOC_ASM;  // 与原版截图一致：默认停在程序集（代码）页
    m_editor.SetText(EcSrcProg());
    m_nDoc = ECDOC_ASM;
    m_tabs.SetCurSel(m_nTabOfDoc[nStart]);
    ShowView(nStart == ECDOC_FORM ? 0 : 1);

    // 自动化用：`ECLONE_START_PAGE=gvar|type|const|img|snd|dll|asm` 把启动页切到某页。
    // 为什么需要：`DumpModel()` 只倒**当前页**建出来的表（表页是同一份源码按 m_nKind 过滤出来的），
    // 不切页就没法机械验证"全局变量表/自定义数据类型表/常量数据表"的排版。
    // ⚠️ 不要走 `OpenDoc()`：它在 `n == m_nDoc` 时**提前 return 不重建**，
    //    而 m_nDoc 初始就是 ECDOC_ASM —— 那样切页会静默不生效（实测踩过）。
    {
        TCHAR szPage[64];
        if (::GetEnvironmentVariable(_T("ECLONE_START_PAGE"), szPage, 64) > 0)
        {
            static const int kinds[ECDOC_COUNT] = { ECK_ASM, ECK_ASM, ECK_DLL,
                                                    ECK_GVAR, ECK_TYPE, ECK_CONST,
                                                    ECK_IMG, ECK_SND };
            int nPage = -1;
            if      (_tcsicmp(szPage, _T("asm"))   == 0) nPage = ECDOC_ASM;
            else if (_tcsicmp(szPage, _T("dll"))   == 0) nPage = ECDOC_DLL;
            else if (_tcsicmp(szPage, _T("gvar"))  == 0) nPage = ECDOC_GVAR;
            else if (_tcsicmp(szPage, _T("type"))  == 0) nPage = ECDOC_TYPE;
            else if (_tcsicmp(szPage, _T("const")) == 0) nPage = ECDOC_CONST;
            else if (_tcsicmp(szPage, _T("img"))   == 0) nPage = ECDOC_IMG;
            else if (_tcsicmp(szPage, _T("snd"))   == 0) nPage = ECDOC_SND;
            else if (_tcsicmp(szPage, _T("form"))  == 0) nPage = ECDOC_FORM;
            if (nPage >= 0)
            {
                m_editor.SetText((nPage == ECDOC_DLL) ? EcSrcDll() : EcSrcProg());
                m_editor.SetKind(kinds[nPage]);
                // DumpModel 是 protected，走 ec_editor 里现成的诊断消息强制重倒一次
                m_editor.SendMessage(EC_WM_REDUMP, 0, 0);
            }
        }
    }
    return TRUE;
}

// 文档页 -> 标签条文字（原版实测的页名，见 tools\probe_page2m.ps1 打在标题栏上的那一段）
static LPCTSTR EcDocTabText(int n)
{
    switch (n)
    {
    case ECDOC_FORM:  return _T("_启动窗口");
    case ECDOC_ASM:   return _T("启动窗口程序集");
    case ECDOC_DLL:   return _T("Dll命令");
    case ECDOC_GVAR:  return _T("全局变量表");
    case ECDOC_TYPE:  return _T("自定义数据类型表");
    case ECDOC_CONST: return _T("常量数据表");
    case ECDOC_IMG:   return _T("图片资源表");
    case ECDOC_SND:   return _T("声音资源表");
    }
    return _T("");
}

// 文档页 -> MDI 子窗口标题。原版实测（tools\probe_page2m.ps1 打的标题栏）：
//   程序集页 `[程序集: 启动窗口程序集 / _启动窗口]`、窗口页 `[窗口: _启动窗口]`、
//   表页就是页名（`[全局变量表]` / `[Dll命令定义表]` / `[常量数据表]` …）。
static LPCTSTR EcDocTitleText(int n)
{
    switch (n)
    {
    case ECDOC_FORM:  return _T("窗口: _启动窗口");
    case ECDOC_ASM:   return _T("程序集: 启动窗口程序集 / _启动窗口");
    case ECDOC_DLL:   return _T("Dll命令定义表");
    case ECDOC_GVAR:  return _T("全局变量表");
    case ECDOC_TYPE:  return _T("自定义数据类型表");
    case ECDOC_CONST: return _T("常量数据表");
    case ECDOC_IMG:   return _T("图片资源表");
    case ECDOC_SND:   return _T("声音资源表");
    }
    return _T("");
}

// 切文档页。程序集页与 Dll命令页各有一段自己的源码文本（原版是两页两套表格）；
// 表页（全局变量/数据类型/常量/资源）用的是**同一份**源码，只换编辑器的页类别。
void CECChildFrame::OpenDoc(int n)
{
    OpenDoc(n, -1);
}

void CECChildFrame::OpenDoc(int n, int nForm)
{
    if (n < 0 || n >= ECDOC_COUNT)
        return;
    // 「切换窗体」：点程序树里不同的窗口节点时带着窗体下标进来（见 CPaneBar::OnProgTreeSelChanged）。
    // 先切窗体再切页 —— SelectForm 自己会 Invalidate 设计器。
    if (nForm >= 0 && nForm < m_designer.FormCount())
        m_designer.SelectForm(nForm);
    if (m_nTabOfDoc[n] < 0)                 // 这个页还没打开过 -> 加一个标签
        m_nTabOfDoc[n] = m_tabs.AddTab(EcDocTabText(n));
    SetWindowText(EcDocTitleText(n));
    if (n == m_nDoc)
    {
        m_tabs.SetCurSel(m_nTabOfDoc[n]);
        ShowView(n == ECDOC_FORM ? 0 : 1);
        return;
    }
    m_nDoc = n;
    m_tabs.SetCurSel(m_nTabOfDoc[n]);
    ShowView(n == ECDOC_FORM ? 0 : 1);
    if (n == ECDOC_ASM)
    {
        m_editor.SetKind(ECK_ASM);
        m_editor.SetText(EcSrcProg());
    }
    else if (n == ECDOC_DLL)
    {
        m_editor.SetKind(ECK_DLL);
        m_editor.SetText(EcSrcDll());
    }
    else
    {
        static const int kinds[ECDOC_COUNT] = { ECK_ASM, ECK_ASM, ECK_DLL,
                                                ECK_GVAR, ECK_TYPE, ECK_CONST,
                                                ECK_IMG, ECK_SND };
        m_editor.SetText(EcSrcProg());   // 表页都是同一份源码
        m_editor.SetKind(kinds[n]);
    }
}

LRESULT CECChildFrame::OnOpenDoc(WPARAM w, LPARAM l)
{
    OpenDoc((int)w, (l == 0) ? -1 : (int)l - 1);   // lParam = 窗体下标 + 1（0 = 不带窗体）
    return 0;
}

// 支持库树里选中一个可视组件 -> 设计器进入放置模式。
// 这里**不**强行切到设计器页（否则支持库树就没法当资料浏览器用了）；
// 状态栏上会提示"已进入放置模式"，切到设计器页后点窗体即可放上去。
LRESULT CECChildFrame::OnArmUnit(WPARAM w, LPARAM l)
{
    int lib = (int)w, dt = (int)l;
    if (lib < 0 || dt < 0)
    {
        m_designer.CancelPlace();
        return 0;
    }
    const EcDataType* d = EcDtAt(lib, dt);
    if (d == NULL)
    {
        m_designer.CancelPlace();
        return 0;
    }
    m_designer.ArmPlace(lib, dt, d->name);
    return 0;
}

// 切文档页（**不碰编辑器内容**）：只动标签条、标题和视图。
// 给「插入->窗口」用 —— 它刚往源码里插了一行，绝不能让 `OpenDoc` 的
// `SetText(g_szSampleCode)` 把源码冲掉（那是两次踩坑换来的，见 OnInsertWindow 的说明）。
void CECChildFrame::SetDocTab(int n)
{
    if (n < 0 || n >= ECDOC_COUNT)
        return;
    if (m_nTabOfDoc[n] < 0)
        m_nTabOfDoc[n] = m_tabs.AddTab(EcDocTabText(n));
    m_nDoc = n;
    m_tabs.SetCurSel(m_nTabOfDoc[n]);
    SetWindowText(EcDocTitleText(n));
    ShowView(n == ECDOC_FORM ? 0 : 1);
}

void CECChildFrame::ShowView(int n)
{
    m_designer.ShowWindow(n == 0 ? SW_SHOW : SW_HIDE);
    m_editor.ShowWindow(n == 1 ? SW_SHOW : SW_HIDE);
    if (n == 1)
        m_editor.FocusEdit();

    // ★ 「窗口组件箱」是**设计器界面的一部分**，不是代码编辑器的一部分（用户 2026-10 指出）：
    //   原版实拍（shots\orig_designer_activated.png vs shots\orig_des_full.png）：设计器视图里
    //   右侧有「基本组件/扩展组件 + 图标网格」，切到代码编辑器（程序集）页就没有了。
    //   ⚠️ 用户手动收起过就不要硬弹出来 —— 只有"本来就没显示"时才跟着视图自动显示。
    CMainFrame* pMain = (CMainFrame*)AfxGetMainWnd();
    if (pMain != NULL && pMain->GetSafeHwnd() != NULL)
    {
        if (n == 0)
        {
            if (!pMain->IsCompBoxShown() && !pMain->IsCompBoxUserHidden())
                pMain->ShowCompBox(TRUE);
        }
        else if (pMain->IsCompBoxShown())
        {
            pMain->ShowCompBox(FALSE);
        }
    }
}

// 设计器页上 Ctrl+Tab = 在所有窗体之间轮换（见声明处的说明）。
// ⚠️ 不要加 `m_nDoc == ECDOC_FORM` 这个条件：MDI 子窗口一建出来当前页就是「程序集」
//    （构造函数 `m_nDoc(ECDOC_ASM)`），加了之后这个键在任何正常路径下都不会触发
//    —— 实测就是这么发现的（ui_smoke 的 Ctrl+Tab 断言一直不动）。
BOOL CECChildFrame::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg != NULL && pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
    {
        if ((GetKeyState(VK_CONTROL) | GetAsyncKeyState(VK_CONTROL)) & 0x8000)
        {
            int n = m_designer.FormCount();
            if (n > 1)
                m_designer.SelectForm((m_designer.CurForm() + 1) % n);
            return TRUE;
        }
    }
    return CMDIChildWnd::PreTranslateMessage(pMsg);
}

void CECChildFrame::LayoutViews()
{
    // 原版实测（shots\live2\tree.txt）：文档标签条是窗口类 CCustomTabCtrl 的独立条，
    // 1195x22，挂在 MDI 客户区**下侧**（y=685..707，紧贴状态夹之上），编辑器占满上方。
    CRect rc;
    GetClientRect(&rc);
    int top = rc.Height() - CECTabStrip::TAB_H;
    if (top < 0)
        top = 0;
    m_tabs.MoveWindow(0, top, rc.Width(), CECTabStrip::TAB_H);
    m_designer.MoveWindow(0, 0, rc.Width(), top);
    m_editor.MoveWindow(0, 0, rc.Width(), top);
}

LRESULT CECChildFrame::OnTabSel(WPARAM w, LPARAM)
{
    OpenDoc((int)w);       // 直接点标签条也是切文档页（含编辑器内容）
    return 0;
}

// ---------------------------------------------------------------- 「编辑」菜单
// 这些命令都作用在代码页上；表页（Dll命令/全局变量…）整页都是表格，先不做（原版那边
// 也是按文档类型分派的）。设计器页同理。
static BOOL EcIsEditorDoc(int nDoc)
{
    return (nDoc != ECDOC_FORM);
}

void CECChildFrame::OnEditUndo()      { if (EcIsEditorDoc(m_nDoc)) m_editor.Undo(); }
void CECChildFrame::OnEditRedo()      { if (EcIsEditorDoc(m_nDoc)) m_editor.Redo(); }
void CECChildFrame::OnEditDelLine()   { if (EcIsEditorDoc(m_nDoc)) m_editor.DeleteCurrentLine(); }
void CECChildFrame::OnEditMask()      { if (EcIsEditorDoc(m_nDoc)) m_editor.ToggleComment(TRUE); }
void CECChildFrame::OnEditUnmask()    { if (EcIsEditorDoc(m_nDoc)) m_editor.ToggleComment(FALSE); }
void CECChildFrame::OnEditCollapse()  { if (EcIsEditorDoc(m_nDoc)) m_editor.CollapseSubOfCaret(TRUE); }
void CECChildFrame::OnEditExpandSub() { if (EcIsEditorDoc(m_nDoc)) m_editor.CollapseSubOfCaret(FALSE); }
void CECChildFrame::OnEditExpandAll() { if (EcIsEditorDoc(m_nDoc)) m_editor.ExpandAllSubs(); }
// 开始寻找（Ctrl+F / #32877）：弹原版那个「寻找」对话框（IDD_FIND = RT_DIALOG id=284），
// 确定后把文本与「区分大小写」交给编辑器，并立刻找下一个。
void CECChildFrame::OnEditFindDlg()
{
    if (!EcIsEditorDoc(m_nDoc)) return;
    CString s; BOOL bCase = FALSE;
    m_editor.GetFindText(s, bCase);
    if (!EcShowFind(this, &s, &bCase)) return;
    m_editor.SetFindText(s, bCase);
    m_editor.FindNext(TRUE);
}
// 寻找替换（Ctrl+H / #3）：弹原版 RT_DIALOG id=578；按哪个按钮就做哪件事。
void CECChildFrame::OnEditReplace()
{
    if (!EcIsEditorDoc(m_nDoc)) return;
    CString sFind, sWith; BOOL bCase = FALSE; int nCmd = 0;
    m_editor.GetFindText(sFind, bCase);
    if (!EcShowReplace(this, &sFind, &sWith, &bCase, &nCmd)) return;
    m_editor.SetFindText(sFind, bCase);
    m_editor.SetReplaceText(sWith);
    if (nCmd == 4)          // 全部替换
    {
        int n = m_editor.ReplaceAll();
        CString msg; msg.Format(_T("已替换 %d 处。"), n);
        if (m_nDoc == ECDOC_ASM || m_nDoc == ECDOC_DLL)
        {
            CWnd* pMain = AfxGetMainWnd();
            if (pMain != NULL) pMain->SendMessage(EC_WM_HINT, 0, (LPARAM)(LPCTSTR)msg);
        }
    }
    else if (nCmd == 3)     // 替换（当前这一处）
        m_editor.ReplaceCurrent();
    else                    // 搜寻下一个
        m_editor.FindNext(TRUE);
}
// 书签三条（#32964 加/取消，#32967 到前，#32968 到后）。原版还有一个「书签跳转」
// (#32965) 会弹一个列表对话框，本工程还没倒那个模板，先不接。
// 书签跳转（Ctrl+F7 / #32965）：弹原版 RT_DIALOG id=607 那个列表对话框。
void CECChildFrame::OnEditBookmarkDlg()
{
    if (!EcIsEditorDoc(m_nDoc)) return;
    int nLine = -1;
    if (EcShowBookmarks(this, &m_editor, &nLine))
        m_editor.GotoLine(nLine);
}
// 调试的第一步（第 21 轮第二十四批）：断点的状态先做起来。
// #32855 断点（F9）/ #32858 清除所有断点。运行(F5)/单步这些需要执行引擎的还没做。
void CECChildFrame::OnDebugToggleBp() { if (EcIsEditorDoc(m_nDoc)) m_editor.ToggleBreakpoint(); }
void CECChildFrame::OnDebugClearBp()  { if (EcIsEditorDoc(m_nDoc)) m_editor.ClearBreakpoints(); }
void CECChildFrame::OnEditBookmark() { if (EcIsEditorDoc(m_nDoc)) m_editor.ToggleBookmark(); }
void CECChildFrame::OnEditBookPrev() { if (EcIsEditorDoc(m_nDoc)) m_editor.GotoBookmark(FALSE); }
void CECChildFrame::OnEditBookNext() { if (EcIsEditorDoc(m_nDoc)) m_editor.GotoBookmark(TRUE); }
void CECChildFrame::OnEditFindNext() { if (EcIsEditorDoc(m_nDoc)) m_editor.FindNext(TRUE); }
void CECChildFrame::OnEditFindPrev() { if (EcIsEditorDoc(m_nDoc)) m_editor.FindNext(FALSE); }

// 整体搜寻（Ctrl+Alt+F / #32934）。
// 原版模板 RT_DIALOG id=30773「整体搜寻对话框」；结果写进状态夹的「搜寻1 / 搜寻2」页
// （原版那两页是**结果输出夹**，不是搜索栏 —— 上一轮的推断已在本次推翻，见 ec_syscfg.h）。
void CECChildFrame::OnEditWholeFind()
{
    if (!EcIsEditorDoc(m_nDoc))
        return;
    DoWholeFind();
}

void CECChildFrame::DoWholeFind()
{
    CString sText, sCase;
    CString sFind;
    BOOL bCase = FALSE;
    int  nPane = 0;
    if (!EcShowWholeFind(this, &sFind, &bCase, &nPane))
        return;

    // 拿当前文档的源码：优先编辑器里的实时内容（含用户改过的），拿不到就用当前文档的源码
    CString src = m_editor.GetText();
    if (src.IsEmpty())
        src = (m_nDoc == ECDOC_DLL) ? CString(EcSrcDll()) : CString(EcSrcProg());

    CString out;
    int nHit = 0;
    int nLine = 0, i = 0;
    const int nLen = src.GetLength();
    while (i <= nLen)
    {
        int e = src.Find(_T('\n'), i);
        CString line = (e < 0) ? src.Mid(i) : src.Mid(i, e - i);
        line.TrimRight(_T('\r'));
        CString hay = line, needle = sFind;
        if (!bCase) { hay.MakeLower(); needle.MakeLower(); }
        int at = hay.Find(needle);
        while (at >= 0 && nHit < 500)
        {
            nHit++;
            CString sPre = line.Left(at);
            CString sHit = line.Mid(at, sFind.GetLength());
            CString sPost = line.Mid(at + sFind.GetLength());
            CString sOne;
            sOne.Format(_T("%d: %s[%s]%s\r\n"), nLine + 1, (LPCTSTR)sPre, (LPCTSTR)sHit, (LPCTSTR)sPost);
            out += sOne;
            int nNext = at + (sFind.GetLength() > 0 ? sFind.GetLength() : 1);
            if (nNext >= hay.GetLength()) break;
            at = hay.Find(needle, nNext);
        }
        if (e < 0) break;
        i = e + 1;
        nLine++;
    }

    CString sHead;
    sHead.Format(_T("整体搜寻“%s”：共 %d 处\r\n"), (LPCTSTR)sFind, nHit);
    out = sHead + out;

    // 写到状态夹的「搜寻1」(页 5) 或「搜寻2」(页 6) 并翻到那一页
    CMainFrame* pMain = (CMainFrame*)AfxGetMainWnd();
    if (pMain != NULL && pMain->GetSafeHwnd() != NULL)
        pMain->ShowSearchPane(out, nPane);
}
// 加到当前块首尾 #32800..32806 / 转换为 #32807..32813（块种类 = 命令 id - 基址）
void CECChildFrame::OnEditWrapBlock(UINT nID) { if (EcIsEditorDoc(m_nDoc)) m_editor.WrapSelectionInBlock((int)nID - 32800); }
void CECChildFrame::OnEditConvBlock(UINT nID) { if (EcIsEditorDoc(m_nDoc)) m_editor.ConvertSelectionToBlock((int)nID - 32807); }

// 「窗口」菜单的表页命令。命令 id 和文档下标**不是**简单偏移（32817/32818/32819 是
// 数据类型/全局变量/Dll命令，而 32825/32826/32827 是常量/图片/声音 —— 中间隔着两条别的命令），
// 所以老老实实查表，别算偏移。
void CECChildFrame::OnViewTablePage()
{
    const MSG* pMsg = GetCurrentMessage();
    UINT nID = (pMsg != NULL) ? (UINT)pMsg->wParam : 0;
    int nDoc = -1;
    switch (nID)
    {
    case 32817: nDoc = ECDOC_TYPE;  break;   // 自定义数据类型表
    case 32818: nDoc = ECDOC_GVAR;  break;   // 全局变量表
    case 32819: nDoc = ECDOC_DLL;   break;   // Dll命令定义表
    case 32825: nDoc = ECDOC_CONST; break;   // 常量数据表
    case 32826: nDoc = ECDOC_IMG;   break;   // 图片资源表
    case 32827: nDoc = ECDOC_SND;   break;   // 声音资源表
    default: return;
    }
    EcTrace(_T("viewtable id=%u doc=%d"), nID, nDoc);
    OpenDoc(nDoc);
}

// 插入菜单：把菜单命令 id 映射到编辑器里的「插哪种声明」（见 CECEditor::InsertDeclaration 的 nWhat）
// ⚠️ ON_COMMAND 走的是 AfxSig_vv（无参），所以这里用 GetCurrentMessage() 取命令 id ——
//    踩过：把处理函数写成 `void f(UINT nID)` 时 nID 是**垃圾值**（实测 4548960），
//    switch 一个都匹配不上，命令静默无效。
void CECChildFrame::OnInsertDecl()
{
    const MSG* pMsg = GetCurrentMessage();
    DoInsertDecl((pMsg != NULL) ? (UINT)pMsg->wParam : 0);
}

void CECChildFrame::DoInsertDecl(UINT nID)
{
    if (!EcIsEditorDoc(m_nDoc)) return;
    int nWhat = -1;
    switch (nID)
    {
    case 32777: nWhat = 0;  break;   // 子程序
    case 32782: nWhat = 1;  break;   // 程序集
    case 32831: nWhat = 2;  break;   // 窗口
    case 32778: nWhat = 3;  break;   // 数据类型
    case 32779: nWhat = 4;  break;   // 全局变量
    case 32780: nWhat = 5;  break;   // Dll命令
    case 32828: nWhat = 6;  break;   // 常量
    case 32829: nWhat = 7;  break;   // 图片或图片组
    case 32830: nWhat = 8;  break;   // 声音
    case 32929: nWhat = 9;  break;   // 类模块
    case 32786: nWhat = 10; break;   // 参数
    case 32787: nWhat = 11; break;   // 局部变量
    default: return;                 // 32783 现行单元：暂不实现（原版是切到当前单元那页）
    }
    EcTrace(_T("insertcmd id=%u doc=%d what=%d"), nID, m_nDoc, nWhat);
    m_editor.InsertDeclaration(nWhat);
}

// 「插入->窗口」(#32831)：先插一行 `.窗口 窗口N`（走既有的 InsertDeclaration），
// 再让设计器**真的多建一个支持库窗体**。
// ⚠️⚠️ **顺序不能反**：`OpenDoc(ECDOC_FORM)` 会给编辑器 `SetKind(ECK_ASM)` +
//     `SetText(g_szSampleCode)` —— 也就是**把源码恢复成采样**，所以在它之后再插声明
//     会把刚插的那一行（以及前面几次插入）全冲掉。
//     现象：`insert_smoke.ps1` 从「插入窗口」那条起后面每条都失败、SRC 行数一直卡在 63
//     （回归第 26 步 10 条假失败就是这么来的）。
//     ⚠️ 但**必须**切到设计器视图（`ShowView(0)`）—— 窗体窗口的 WS_VISIBLE 是靠
//     `m_designer` 视图可见才真正"看得见"的（`EcCreateUnit` 把窗体 SetParent 到设计器视图上），
//     实测只 `ShowWindow(窗体, SW_SHOW)` 而视图隐藏时 `IsWindowVisible(窗体) = 0`。
//     所以：**先插声明，再切视图**。
// 「插入->窗口」(#32831)：先插一行 `.窗口 窗口N`（走既有的 InsertDeclaration），
// 再让设计器**真的多建一个支持库窗体**。
//
// ⚠️⚠️ 这段的顺序是**两次踩坑换来的**，别改：
//   1) **绝对不要在这里调 `OpenDoc(ECDOC_FORM)`**。`OpenDoc` 末尾会给编辑器
//      `SetKind(ECK_ASM)` + `SetText(g_szSampleCode)` —— 也就是**把源码恢复成采样**
//      并且**立刻重倒模型**（`SetText` 里那句 `DumpModel()`）。于是：
//        · 刚插进去的那一行连同前面几次插入一起被冲掉；
//        · `insert_smoke.ps1` 读到的模型永远是 63 行（采样的行数）。
//      现象就是「插入窗口」之后每一条插入都失败（回归第 26 步 10 条假失败）。
//   2) 但**必须让设计器视图可见**：窗体窗口是 `SetParent` 到设计器视图上的，
//      视图隐藏时 `IsWindowVisible(窗体)` 恒为 0（实测：`CreateForm` 里刚
//      `ShowWindow(窗体, SW_SHOW)` 完，`IsWindowVisible` 还是 0）。
//      所以用 **`ShowView(0)`（只切视图、不碰文档）**，而不是 `OpenDoc(0)`。
void CECChildFrame::OnInsertWindow()
{
    if (EcIsEditorDoc(m_nDoc))
        m_editor.InsertDeclaration(2);          // 2 = "窗口"（见 InsertDeclaration 的 nWhat 表）
    m_designer.CreateNewForm(NULL);
    // ⚠️ 只切**视图**，不动文档页/标签条、也不动编辑器内容：
    //    `OpenDoc(ECDOC_FORM)` 会把源码重置成采样（并立刻重倒模型），
    //    `SetDocTab` 也会改 `m_nDoc` 而让后面「插入」类命令落到别的页类别上。
    //    实测两者都会让 `insert_smoke.ps1` 假失败（第 26 步），所以这里什么都不切。
    //    代价：如果用户当时不在设计器页，新窗体不会自动显示 —— 切到设计器页即可看到。
    if ((m_designer.GetStyle() & WS_VISIBLE) == 0)
        ShowView(0);
}

// 菜单项的灰化：撤消/重复有没有东西可做（原版这两个菜单项也会灰）
void CECChildFrame::OnUpdateEditUndo(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(EcIsEditorDoc(m_nDoc) && m_editor.CanUndo());
}
void CECChildFrame::OnUpdateEditRedo(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(EcIsEditorDoc(m_nDoc) && m_editor.CanRedo());
}

void CECChildFrame::OnSize(UINT nType, int cx, int cy)
{
    CMDIChildWnd::OnSize(nType, cx, cy);
    LayoutViews();
}

// ---------------- 应用
class CECloneApp : public CWinApp
{
public:
    virtual BOOL InitInstance();
};

CECloneApp theApp;

BOOL CECloneApp::InitInstance()
{
    InitCommonControls();
    // ⚠️⚠️ **不要**在这里调 `SetThemeAppProperties(0)`（第 26 轮我加过，用户指正后撤掉了）。
    //    它是**进程级**开关：关掉之后不只是设计期窗体，**主框架自己的标题栏/边框也变成经典样式**
    //    —— 用户一眼就看出"变成 VC98 古老风格了、还有厚重边框"，而且主框架非客户区高度从
    //    原版的 39px 变成 59px（实测 `python tools\probe_frame_metrics.py`）。
    //    原版 e.exe 自己**没有 RT_MANIFEST**、也没关主题，主框架是正常的现代外观；
    //    设计期窗体那条"经典 3D 渐亮标题栏"要单独处理就单独钩它的 WM_NCPAINT，
    //    绝不能拿全局开关去换。
    //
    //    留下这段注释是因为这个坑很容易再踩：**进程级的显示开关永远先怀疑**。
    // 「工具->系统配置」上次保存的配色 / 显示选项（HKCU\Software\FlySky\E\EClone40）
    EcThemeLoad();
    g_ecTheme.DumpIfAsked();
    // 同一个注册表键下的「系统配置」其余 6 个页签设置（通常/编辑/显示/编译/存根/安全）
    EcCfgLoad();
    EcCfgDumpIfAsked();
    if (m_lpCmdLine && _tcsstr(m_lpCmdLine, _T("designer")) != NULL)
        g_bStartDesigner = TRUE;
    if (m_lpCmdLine && _tcsstr(m_lpCmdLine, _T("exit")) != NULL)
        g_bExitAfterInit = TRUE;

    // 命令行给了真 .e 就打开工程；没有就用采样源码（原行为不变）
    {
        CString strE;
        if (EcFindEArg(strE))
        {
            EcDumpEProjectIfAsked(strE);
            if (EcIsEFile(strE) && EcLoadEProject(strE, g_strEProgram))
            {
                g_bEProject = TRUE;
            }
            else
            {
                CString msg;
                msg.Format(_T("无法读取 .e 工程文件：\r\n%s"), (LPCTSTR)strE);
                ::AfxMessageBox(msg, MB_ICONWARNING);
            }
        }
    }

    CMainFrame* pFrame = new CMainFrame();
    // 原版实测: 窗口矩形 134,57 尺寸 1440x753
    if (!pFrame->Create(NULL, _T("易语言-加密狗版 - Windows窗口程序 - [起始页]"),
                        WS_OVERLAPPEDWINDOW,
                        CRect(134, 57, 134 + 1440, 57 + 753),
                        NULL, MAKEINTRESOURCE(IDR_MAINFRAME)))
    {
        return FALSE;
    }
    m_pMainWnd = pFrame;
    EC_SetWindowIcon(pFrame, IDR_MAINFRAME);
    pFrame->ShowWindow(m_nCmdShow);
    pFrame->UpdateWindow();

    // 创建一个 MDI 子窗口，标题与原版一致
    CECChildFrame* pChild = new CECChildFrame();
    if (pChild->Create(NULL, _T("程序集: 启动窗口程序集 / _启动窗口"),
                       WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW, CRect(0, 0, 0, 0),
                       pFrame))
    {
        EC_SetWindowIcon(pChild, IDR_MAINFRAME);   // 菜单条最左端那个图标就是它
        pChild->ShowWindow(SW_SHOWMAXIMIZED);
    }
    // 自动化取模型用：`EClone.exe <x.e> exit` 建完窗口就退出
    //（SetText 里那句 DumpModel() 已经把模型写到 ECLONE_DUMP_MODEL 指的文件）
    if (g_bExitAfterInit)
    {
        // 退出前**强制重倒一次**：切页那一步（ECLONE_START_PAGE）是在子窗口建好之后做的，
        // 而 SetText 里自动倒的那次用的是切页**之前**的 m_nKind，会把文件覆盖成程序集页的模型。
        // EC_WM_REDUMP 是 ec_editor 里现成的诊断消息（OnRedump -> DumpModel）。
        if (pChild != NULL && pChild->GetSafeHwnd() != NULL)
            pChild->SendMessage(EC_WM_REDUMP, 0, 0);
        ::PostMessage(pFrame->GetSafeHwnd(), WM_CLOSE, 0, 0);
    }
    return TRUE;
}

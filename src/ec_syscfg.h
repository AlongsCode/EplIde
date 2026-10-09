// ec_syscfg.h -- 「工具->系统配置」对话框
//
// 原版：菜单命令 #32876，模板 RT_DIALOG id=282，6 个页（安全/存根/编译/设计/
// 显示/编辑）。本工程实现「程序显示」页（唯一影响代码编辑框外观的一页），
// 其余页留空标签。见 ec_theme.h 顶部的完整逆向依据。
#ifndef _EC_SYSCFG_H_
#define _EC_SYSCFG_H_

#include <afxwin.h>
#include <afxtempl.h>
#include "ec_theme.h"
#include "ec_lib.h"

// 「系统配置」其它页签的控件 id：**自带一份**，不依赖调用方先包含 resource.h
// （⚠️ 同 IDD_FIND 那条坑：只在 resource.h 里加，C++ 侧编译这个头文件时看不到；
//   .rc 那边走 assemble_rc.py 内联 resource.h 的路，本来就能看到。
//   两边都定义也不冲突 —— 值一样。）
#ifndef IDC_CFG_G1
// [0] 通常
#define IDC_CFG_G1              1116
#define IDC_CFG_G2              1117
#define IDC_CFG_STARTWIN        1101
#define IDC_CFG_STARTSUB        1102
#define IDC_CFG_BAK             1041
#define IDC_CFG_AUTHOR          1042
#define IDC_CFG_SAVEMIN         1001
#define IDC_CFG_UNDOMEM         1011
#define IDC_CFG_DEBUGOUT        1236
#define IDC_CFG_DEBUGBROWSE     1242
#define IDC_CFG_IM1             1127
#define IDC_CFG_IM2             1128
#define IDC_CFG_IM3             1129
#define IDC_CFG_IMEDIT          8
#define IDC_CFG_SOUTH           1130
#define IDC_CFG_TIP             1180
#define IDC_CFG_SYNTAX          1132
#define IDC_CFG_CLIPNUM         1201
// [2] 显示（设计器外观）
#define IDC_CFG_NEWW            1006
#define IDC_CFG_NEWH            1007
#define IDC_CFG_GRID            1144
#define IDC_CFG_GRIDW           1008
#define IDC_CFG_GRIDH           1009
#define IDC_CFG_SNAP            1146
#define IDC_CFG_ROPROP          1147
#define IDC_CFG_COLOR2          1142
#define IDC_CFG_COLORBOX        2250
// [3] 设计
#define IDC_CFG_FASTARY         1165
#define IDC_CFG_DLLSTK          1168
#define IDC_CFG_DEADLOOP        1170
#define IDC_CFG_DUPNAME         1136
#define IDC_CFG_STRICTREF       1140
#define IDC_CFG_ASKWRITE        1138
#define IDC_CFG_PRECLOSS        1141
#define IDC_CFG_SIGNCERT        1235
#define IDC_CFG_SIGNBROWSE      1241
// [4] 编译
#define IDC_CFG_MANIPRE         1103
#define IDC_CFG_COMCTL6         1208
#define IDC_CFG_UAC             1207
#define IDC_CFG_MANIUSER        1104
#define IDC_CFG_MANIIMPORT      1246
#define IDC_CFG_MANITEXT        1010
// [5] 存根
#define IDC_CFG_JUNKLEVEL       1198
#define IDC_CFG_XORCODE         613
#endif // IDC_CFG_G1

class CEcSysCfgDlg : public CDialog
{
public:
    CEcSysCfgDlg(CWnd* pParent = NULL);

    enum { IDD = 282 };             // 与原版 dialog id 相同

protected:
    CComboBox m_cboScheme;
    CStringArray m_schemes;
    CString   m_sSchemeApplied;     // 预览中途已经套用的配色（取消时回滚）

    void FillSchemeCombo();
    void ReadCtrls(EcTheme& t);
    void WriteCtrls(const EcTheme& t);
    void ShowPage(int n);           // 只显示第 n 个页签的控件（原版形态：一个模板 + 页签切换）
    void PreviewScheme();           // 下拉框换项时立即预览
    void PushToEditors();           // 让所有已打开的编辑器重排/重画

    virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual void OnCancel();
    afx_msg void OnTabChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSchemeChange();
    afx_msg void OnImport();
    afx_msg void OnSaveClr();
    afx_msg void OnSetFont();
    afx_msg void OnDefault();
    afx_msg void OnDrawOpt();
    // 其余 6 个页签：控件值 <-> g_ecCfg 的同步在 ec_theme.cpp 的 EcCfgToDlg/EcCfgFromDlg；
    // 这里只管两个要弹文件框的按钮。
    afx_msg void OnManifestImport();     // 存根页 id=1246「从文件导入(&F)」
    afx_msg void OnSignBrowse();         // 编译页 id=1241「...」（签名证书）
    DECLARE_MESSAGE_MAP()
};

// 从菜单调起来；返回 TRUE 表示用户按了「确认」
BOOL EcShowSysCfg(CWnd* pParent);

// ---------------------------------------------------------------------------
// 「工具->支持库配置」（原版命令 #32906，模板 RT_DIALOG id=321）
//
// 列表里是**支持库目录下所有 .fne**（核心库 krnln 不列，它永远载入）；
// 勾选 = 本程序要载入这个库。点「确认」时把勾选结果（小写基名）回传给调用方，
// 由调用方销毁设计期真窗口 -> EcUnloadAllLibs -> EcLoadLibs 重载 -> 重建界面。
// 见 ec_syscfg.cpp 顶部的实现说明。
// ---------------------------------------------------------------------------
struct EcLibCfgItem
{
    CString path;      // .fne 全路径
    CString base;      // 文件基名（"iext2"）
    CString name;      // 显示名（载入过的用支持库元数据的"名字 版本 版"，否则就是基名）
    BOOL    loaded;    // 打开对话框时它是不是已经载入
};

class CEcLibCfgDlg;

// 支持库列表。
// ⚠️ 它是**单选**风格（原版模板 id=321 里没有 LBS_MULTIPLESEL），点同一行不会再发
//    LBN_SELCHANGE；而"单击整行翻转勾选"又必须每次都响应。实测往对话框的
//    PreTranslateMessage 里拦 WM_LBUTTONUP **拦不到**（这条消息到不了那里），
//    所以干脆给 ListBox 挂一个自己的窗口过程。
class CEcCheckList : public CListBox
{
public:
    CEcLibCfgDlg* m_pOwner;
    CEcCheckList() : m_pOwner(NULL) {}
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
};

class CEcLibCfgDlg : public CDialog
{
public:
    CEcLibCfgDlg(CWnd* pParent = NULL);

    enum { IDD = 321 };                     // 与原版 dialog id 相同

    const CStringArray& Result() const { return m_result; }

    void OnListClick(LPARAM lParam);        // 单击列表行 -> 翻转勾选（CEcCheckList 转进来）

protected:
    CEcCheckList      m_lst;
    CArray<EcLibCfgItem, const EcLibCfgItem&> m_items;
    CArray<int, int>  m_used;               // 每项的勾选状态
    CStringArray      m_result;             // 确认时收集的"要载入的库"（小写基名）
    BOOL              m_bAll;                // 全选/全不选进行中（防重入）
    BOOL              m_bDirty;

    void Reload(LPCTSTR pszFilter);
    void FillDetails(int idx);
    void Toggle(int idx);

    virtual BOOL OnInitDialog();
    virtual void OnOK();
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS);
    afx_msg void OnSelChange();
    afx_msg void OnSearchChange();
    afx_msg void OnSelectAll();
    afx_msg void OnSelectNone();
    afx_msg void OnDelete();
    afx_msg void OnBuy();
    DECLARE_MESSAGE_MAP()
};

// 文件基名（"D:\e\lib\iext2.fne" -> "iext2"）
CString EcFileBase(LPCTSTR pszPath);
// 已载入的库的显示名（"名字 1.2 版"）；没载入的返回基名
CString EcLibDisplayName(LPCTSTR pszPath);

// 弹出「支持库配置」。返回 TRUE = 用户点了确认，pResult 收"要载入的库"（小写基名）
BOOL EcShowLibCfg(CWnd* pParent, LPCTSTR pszLibDir, CStringArray* pResult);

// 「寻找」对话框的 id：**自带一份**，不依赖调用方先包含 resource.h
// （⚠️ 踩过：只在 resource.h 里加，C++ 侧编译这个头文件时仍报
//   `ec_syscfg.h(129) : error C2065: 'IDD_FIND' : undeclared identifier`；
//   .rc 那边走的是 assemble_rc.py 内联 resource.h 的路，本来就能看到。
//   用 #ifndef 兜住"两边都定义"的情况。）
#ifndef IDD_FIND
#define IDD_FIND                284     // 原版「寻找」对话框 (RT_DIALOG id=284)
#endif
#ifndef IDC_FIND_TEXT
#define IDC_FIND_TEXT           1151    // 查找文本框（下拉；原版控件 id）
#endif
#ifndef IDC_FIND_CASE
#define IDC_FIND_CASE           1041    // 区分大小写（原版控件 id）
#endif

// 「寻找」对话框（工具->编辑->开始寻找 Ctrl+F #32877；原版模板 RT_DIALOG id=284）
class CEcFindDlg : public CDialog
{
public:
    CEcFindDlg(CWnd* pParent = NULL);
    CString m_sText;            // 查找文本
    BOOL    m_bCase;            // 区分大小写
    enum { IDD = IDD_FIND };
protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    DECLARE_MESSAGE_MAP()
};

// 弹「寻找」对话框；返回 FALSE = 用户取消。进来时 pText/pbCase 是上次的值，出去是新的值。
BOOL EcShowFind(CWnd* pParent, CString* pText, BOOL* pbCase);

#ifndef IDD_REPLACE
#define IDD_REPLACE             578     // 原版「寻找替换」对话框 (RT_DIALOG id=578)
#endif
#ifndef IDC_REPL_TEXT
#define IDC_REPL_TEXT           1181    // 搜索文本（下拉；原版控件 id）
#endif
#ifndef IDC_REPL_WITH
#define IDC_REPL_WITH           1152    // 替换文本（下拉；原版控件 id）
#endif
#ifndef IDC_REPL_HINT
#define IDC_REPL_HINT           1047    // 一行提示/状态（原版控件 id，模板里初始为空）
#endif
#ifndef IDC_REPL_DO
#define IDC_REPL_DO             3       // 「替换(&R)」
#endif
#ifndef IDC_REPL_ALL
#define IDC_REPL_ALL            4       // 「全部替换(&A)」
#endif
// ⚠️ 两个对话框共用原版的 id：1041 是「区分大小写」（见上面的 IDC_FIND_CASE），
//    1/2 就是 IDOK/IDCANCEL（"搜寻下一个"/"关闭"），所以这里不另起名字。

// 「寻找替换」对话框（工具->编辑->寻找替换 Ctrl+H #3；原版模板 RT_DIALOG id=578）
// 用法：EcShowReplace() 返回按钮（IDOK=搜寻下一个 / IDC_REPL_DO=替换 / IDC_REPL_ALL=全部替换），
// 用户点「关闭」或关窗时返回 FALSE。
class CEcReplDlg : public CDialog
{
public:
    CEcReplDlg(CWnd* pParent = NULL);
    CString m_sFind;            // 欲搜寻文本
    CString m_sWith;            // 替换文本
    BOOL    m_bCase;            // 区分大小写
    int     m_nCmd;             // 出来的按钮 id
    enum { IDD = IDD_REPLACE };
protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnFindNext();
    virtual void OnReplaceOne();
    virtual void OnReplaceAll();
    afx_msg void OnClose();
    DECLARE_MESSAGE_MAP()
};

BOOL EcShowReplace(CWnd* pParent, CString* pFind, CString* pWith, BOOL* pbCase, int* pnCmd);

#ifndef IDD_BOOKMARK
#define IDD_BOOKMARK            607
#endif
#ifndef IDC_BM_LIST
#define IDC_BM_LIST             1036
#endif
#ifndef IDC_BM_DEL
#define IDC_BM_DEL              3
#endif
#ifndef IDC_BM_DELALL
#define IDC_BM_DELALL           4
#endif

// 「书签跳转」对话框（编辑->书签跳转 Ctrl+F7 #32965；原版模板 RT_DIALOG id=607）
class CEcBmDlg : public CDialog
{
public:
    CEcBmDlg(CWnd* pParent = NULL);
    enum { IDD = IDD_BOOKMARK };
protected:
    virtual BOOL OnInitDialog();
    virtual void OnOK();                 // 「跳转(&G)」
    afx_msg void OnDel();
    afx_msg void OnDelAll();
    DECLARE_MESSAGE_MAP()
};

// 弹「书签跳转」；返回 TRUE = 用户点了「跳转」，此时 *pnLine 是目标行。
// 删除/全部删除直接在对话框里生效（改的是编辑器的书签表）。
BOOL EcShowBookmarks(CWnd* pParent, class CECEditor* pEd, int* pnLine);

// ---------------------------------------------------------------------------
// 「编辑->整体搜寻」（Ctrl+Alt+F / 原版命令 #32934，模板 **RT_DIALOG id=30773**
// 「整体搜寻对话框」254x163 对话框单位、12 个控件）。
//
// ⚠️ 上一轮 HANDOFF 里写「原版状态夹的搜寻1/搜寻2 页里有个搜索栏」是**推断，错的**：
//    原版那个「搜寻1/搜寻2」是**系统输出夹**（结果输出到哪里，单选），搜索栏就是本对话框。
//    证据：tools\dlg_list.py 30773 的控件原文 ——
//      [06] BS_GROUPBOX '搜寻结果列出位置'
//      [07] BS_AUTORADIOBUTTON '“搜寻1”系统输出夹'   (id=1101)
//      [08] BS_AUTORADIOBUTTON '“搜寻2”系统输出夹'   (id=1102)
// ---------------------------------------------------------------------------
#ifndef IDD_WHOLEFIND
#define IDD_WHOLEFIND           30773   // 原版「整体搜寻对话框」
#endif
#ifndef IDC_WF_TEXT
#define IDC_WF_TEXT             1151    // 欲搜寻文本（下拉；原版控件 id）
#endif
#ifndef IDC_WF_SCOPE
#define IDC_WF_SCOPE            1152    // 搜寻范围（下拉；原版控件 id）
#endif
#ifndef IDC_WF_CASE
#define IDC_WF_CASE             1041    // 区分大小写（原版控件 id）
#endif
#ifndef IDC_WF_EXPAND
#define IDC_WF_EXPAND           1042    // 搜寻前自动展开所有被收缩子程序（原版控件 id）
#endif
#ifndef IDC_WF_PANE1
#define IDC_WF_PANE1            1101    // “搜寻1”系统输出夹
#endif
#ifndef IDC_WF_PANE2
#define IDC_WF_PANE2            1102    // “搜寻2”系统输出夹
#endif
#ifndef IDC_WF_HINT
#define IDC_WF_HINT             1047    // 一行提示/状态（模板里初始为空）
#endif

// 「整体搜寻」对话框。结果通过 pnPane(0/1) 返回要输出到哪个搜寻夹。
class CEcWholeFindDlg : public CDialog
{
public:
    CEcWholeFindDlg(CWnd* pParent = NULL);
    CString m_sText;            // 欲搜寻文本
    CString m_sScope;           // 搜寻范围
    BOOL    m_bCase;            // 区分大小写
    BOOL    m_bExpand;          // 搜寻前自动展开
    int     m_nPane;            // 0 = 搜寻1，1 = 搜寻2
    enum { IDD = IDD_WHOLEFIND };
protected:
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    DECLARE_MESSAGE_MAP()
};

// 弹「整体搜寻」；返回 FALSE = 取消。*psText 进出 = 文本，*pnPane 出 = 输出到哪个搜寻夹。
BOOL EcShowWholeFind(CWnd* pParent, CString* psText, BOOL* pbCase, int* pnPane);



// 让主框架下所有 MDI 子窗口里的编辑器按当前主题重排/重画
void EcRefreshEditors();

#endif // _EC_SYSCFG_H_

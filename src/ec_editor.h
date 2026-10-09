// ec_editor.h -- 易语言风格的「结构化」代码编辑器（全部自绘）
//
// 逆向依据（原版 e.exe 实测，详见 HANDOFF.md 第 8 节）：
//   · 原版代码编辑器的窗口类 Afx:400000:b:10003:900015:0 里【没有任何子控件】
//     （子程序/参数/变量表格、行号槽、流程线全是同一个窗口自己画的）
//   · e.exe 自己 CreateCaret/SetCaretPos/ShowCaret 并用 ExtTextOutA/TextOutA 画字
//   · 表格列宽 = 该列所有单元格文字宽度最大值 + 12（左右各 6 内边距）
//   · 行高：表格 21px(--1px 网格线 + 20px 单元格)，代码行 18px
//   · 网格 #888888；表头填充：子程序/参数表 #E6EDE4、变量表 #D9E3F0、程序集表 不填充
#ifndef _EC_EDITOR_H_
#define _EC_EDITOR_H_

#include <afxwin.h>
#include <vector>

#define EC_WM_SYNC_GUTTER   (WM_USER + 101)
#define EC_WM_REDUMP        (WM_USER + 102)   // 诊断：强制重倒一次显示模型

// ------------------------------------------------------------------ 代码编辑器右键菜单
// ★ 照原版那个菜单做（整棵倒下来存在 shots\ctxmenu_orig.txt；原版 27 个顶层项 + 2 个子菜单，
//   菜单矩形 311x535）。凡是原版有命令 id 的**一律沿用原版 id**，这样"哪些还没实现"
//   一眼能看出来（grayed 的几条是本工程还没有对应子系统的）。
// 这里只定义原版**没有**的几条（本工程自编的）；
// 判断"是不是本菜单的命令"用 EC_CTX_FIRST..EC_CTX_LAST。
#define EC_CTX_FIRST        40001
#define EC_CTX_NEWSUB       40001
#define EC_CTX_UNDO         40002
#define EC_CTX_COPY         40003
#define EC_CTX_CUT          40004
#define EC_CTX_PASTE        40005
#define EC_CTX_SELALL       40006
#define EC_CTX_DELLINE      40007
#define EC_CTX_INSLINE      40008
#define EC_CTX_COMPILE      40009
#define EC_CTX_LAST         40099

// 文档页类别。原版点程序树里的节点会打开一个**独立的 MDI 文档**（实拍证据：
// 点「全局变量」后窗口树里多出一个 Afx:400000:b 子窗口，标题 |全局变量表|，
// 而原来的 |程序集: 调试| 那个还在）。本工程只有一个编辑器窗口，所以改成
// 「同一个编辑器只显示本页那一段声明」—— 观感一致，实现省一层。
#define ECK_ASM   0     // 程序集页：程序集/子程序表 + 代码
#define ECK_DLL   1     // Dll命令定义表
#define ECK_GVAR  2     // 全局变量表
#define ECK_TYPE  3     // 自定义数据类型表
#define ECK_CONST 4     // 常量数据表
#define ECK_IMG   5     // 图片资源表
#define ECK_SND   6     // 声音资源表

class CECEditor : public CWnd
{
    DECLARE_DYNAMIC(CECEditor)
public:
    CECEditor();
    virtual ~CECEditor();

    BOOL  Create(CWnd* pParent, UINT nID);
    void    SetText(LPCTSTR psz);
    void    SetKind(int k);             // 切「表页」（ECK_*）；只影响显示哪一段声明
    int     GetKind() const { return m_nKind; }
    CString GetText() const;
    void    FocusEdit();
    void    InvalidateAll();
    void    ApplyTheme();               // 「系统配置->程序显示」改了字号/行距后重排
    void    SetCollapsed(LPCTSTR pszSub, BOOL bCollapsed);
    BOOL    IsCollapsed(LPCTSTR pszSub) const;
    int     CollapsedCount() const { return (int)m_setCollapsed.size(); }

    // 单元格就地编辑（本轮新增）：原版点「公开/传址/数组…」这些格子会直接改源码，
    // 双击文字格会临时建一个 EDIT 子控件（见 HANDOFF 第 5 节的做法）
    // ---- 「编辑」菜单要用的（第 21 轮）：撤消/重复、删除行、屏蔽/解除屏蔽、收缩展开
    void    Undo();
    void    Redo();
    BOOL    CanUndo() const { return m_nUndo > 0; }
    BOOL    CanRedo() const { return m_nUndo + 1 < (int)m_undo.size(); }
    void    DeleteCurrentLine();
    void    ToggleComment(BOOL bComment);        // 屏蔽 / 解除屏蔽
    void    CollapseSubOfCaret(BOOL bCollapse);  // 收缩 / 展开子程序（光标所在的那个）
    void    ExpandAllSubs();                     // 展开所有子程序
    // 书签（编辑菜单 #32964 加书签 Ctrl+F6 / #32967 到前书签 Shift+F6 / #32968 到后书签 F6）。
    // ⚠️ 原版还会在流程槽里画一个书签标记，**本工程暂时不画**（没拍到那个标记的基准，见 HANDOFF §6）。
    void    ToggleBookmark();               // 光标行加/去书签
    void    GotoBookmark(BOOL bForward);    // 到后/前一个书签（绕回）
    int     BookmarkCount() const { return (int)m_bookmarks.size(); }
    int     BookmarkAt(int i) const;                    // 第 i 个书签的行号（无则 -1）
    CString LineTextOf(int L) const;                    // 某行的去空白文本（书签列表用）
    void    RemoveBookmarkLine(int L);                  // 去掉某个书签
    void    ClearBookmarks();                           // 全部去掉
    void    GotoLine(int L);                            // 跳到某行（插入点到行首）
    // 断点（运行菜单 #32855 断点 F9 / #32858 清除所有断点）。按源码行号存，做法同书签。
    // ⚠️ 原版还会在流程槽里画断点标记，**本工程暂时不画**（没拍到基准，见 HANDOFF §6）；
    //    状态可以从 ECLONE_DUMP_MODEL 的 `bp=` 字段看到，冒烟脚本就靠它断言。
    void    ToggleBreakpoint();                         // 光标行加/去断点
    BOOL    HasBreakpoint(int L) const;                 // 某源码行是不是断点
    void    ClearBreakpoints();                         // 清除所有断点
    int     BreakpointCount() const { return (int)m_breakpoints.size(); }
    // 寻找下一个 / 上一个（#32878 / #32879，F3 / Shift+F3）：
    // 找的词 = 当前选区（单行内），没有选区就用插入点所在的那个「词」（GBK 整字推进）
    BOOL    FindNext(BOOL bForward);
    // 编辑->「加到当前块首尾」(#32800-32806) / 「转换为 ->」(#32807-32813)。
    // 两者都是**纯源码变换**（照 §2.9 的路子：改 m_lines -> AfterEdit()）：
    //   · 加到当前块首尾 = 把选到的那些行包进一个块（首行前面插块首、末行后面插块尾）
    //   · 转换为        = 把选到的那些行本身变成块首，并在其后插入对应的块尾
    // nKind 取 EC_BLK_*（见 ec_editor.cpp 的 g_szBlockPair）。
    // ⚠️ 块首/块尾的具体文字与缩进量是**推断**的（原版这两个命令的行为没拍到基准，见 HANDOFF §6）。
    void    WrapSelectionInBlock(int nKind);
    void    ConvertSelectionToBlock(int nKind);
    // 右键菜单那一批（见 ec_editor.cpp 的 OnContextMenu）
    void    InsertLineBelow();              // 插入新行
    void    GotoRelatedWord(BOOL bForward); // 到上/下相关位置
    void    GotoComponent(BOOL bForward);   // 到上/下一组件
    void    DoContextCmd(UINT nID);         // 右键菜单命令分派

    // 「插入」菜单（原版 #32777 子程序 / #32782 程序集 / #32831 窗口 / #32778 数据类型 /
    // #32779 全局变量 / #32780 Dll命令 / #32828 常量 / #32829 图片资源 / #32830 声音资源 /
    // #32786 参数 / #32787 局部变量 / #32929 类模块 / #32783 现行单元）。
    // 做法 = 往源码里插一行合法声明（名字自动取不重名的），再 Rebuild + 把插入点停在名字上。
    // nWhat 取 EC_INS_*（见 ec_editor.cpp）。
    void    InsertDeclaration(int nWhat);
    // 诊断/冒烟用：行数与某行文本（插入命令的断言只看显示模型不够，见 HANDOFF §6）
    int     LineCount() const { return (int)m_lines.size(); }
    CString LineAt(int i) const;
    // 「寻找」对话框填进来的文本（空 = 用选区/光标处的词，见 FindNext）
    void    SetFindText(LPCTSTR psz, BOOL bCase) { m_sFindText = psz; m_bFindCase = bCase; }
    void    GetFindText(CString& s, BOOL& bCase) const { s = m_sFindText; bCase = m_bFindCase; }
    void    SetReplaceText(LPCTSTR psz) { m_sReplaceText = psz; }
    // 「寻找替换」的两个动作（Ctrl+H / #3）
    BOOL    ReplaceCurrent();
    int     ReplaceAll();
    BOOL    HitTestCell(CPoint pt, int& nRow, int& nCol, CString& sHead) const;
    BOOL    HitTestCellByRow(int nRow, int nCol, CString& sHead) const;
    BOOL    ToggleFlagCell(int nRow, LPCTSTR pszFlag);
    BOOL    SetCellText(int nRow, int nCol, LPCTSTR pszText);
    void    BeginEditCell(int nRow, int nCol);
    void    CommitEditCell(BOOL bCancel);
    void    EnsureCellEdit();
    BOOL    IsEditingCell() const { return m_bEditing; }

protected:
    // ---------------------------------------------------------- 显示模型
    struct EcCell
    {
        CString  s;
        COLORREF cr;
        int      span;      // 占几列（默认 1；子程序行的备注占 2）
        BOOL     under;     // 画下划线（子程序名 / 程序集名）
        BOOL     check;     // 画 ✔
    };

    struct EcRow
    {
        int   kind;         // 0 = 表格行，1 = 代码行
        int   table;        // 所属表格号（同号共享列宽）
        int   y, h;         // 布局结果
        int   src;          // 源码行下标
        int   indent;       // 代码行缩进级
        int   mark;         // 流程槽标记（见 EC_MARK_*）
        int   fill;         // 表头填充色（COLORREF；-1 = 不填充）
        BOOL  bParam;       // 该语句有「参数归属线」（原版在流程槽画 -/+）
        // 流程线：块首行用它记住自己的块尾（对齐原版 —— 块尾关键字「可见/隐藏」两种形态）
        //   blkEnd = -2  这一行不是块首
        //   blkEnd = -1  块尾关键字**不显示**（如果结束/如果真结束/判断结束）→ 竖线延到
        //                blkLast 行下面一格，用 ▼ 收尾
        //   blkEnd >= 0  块尾关键字**可见**（计次循环尾/判断循环尾/循环判断尾/变量循环尾）
        //                → 竖线画到那一行的中线，两条横虚线收尾
        int   blkEnd;
        int   blkLast;      // 块尾不显示时：块体最后一个显示行（▼ 画在它下面一格）
        CString code;
        std::vector<EcCell> cells;
    };

    struct EcTable
    {
        int cols;
        int kind;       // ECT_ASM / ECT_SUB / ECT_VAR
        std::vector<int> w;
    };

    // ---------------------------------------------------------- 文档 / 视图状态
    std::vector<CString> m_lines;       // 源码（原始行）
    std::vector<EcRow>   m_rows;        // 显示行
    std::vector<EcTable> m_tables;
    int   m_nKind;                      // 文档页类别（ECK_*）
    int   m_nContentH;                  // 内容总高（像素）
    int   m_nScroll;                    // 垂直滚动量

    // 就地编辑中的那个 EDIT 子控件（单元格双击时创建）
    CEdit m_wndEdit;
    BOOL  m_bEditing;
    int   m_nEditRow, m_nEditCol;

    // 「收缩/展开子程序」状态：按子程序名记，Rebuild 时跳过被收起子程序的数据行
    // （键用名字而不是行号，因为编辑会让行号漂移）
    std::vector<CString> m_setCollapsed;
    // 撤消/重复：整篇源码 + 插入点的快照栈（懒但绝对正确；文档不大，见 .cpp 的说明）
    struct EcSnapshot
    {
        CString text;
        int cl, cc, al, ac, scroll;
    };
    std::vector<EcSnapshot> m_undo;
    CString m_sFindText;        // 「寻找」文本框里的内容（空 = 用选区/光标处的词）
    std::vector<int> m_bookmarks;   // 书签（按源码行号升序）
    std::vector<int> m_breakpoints; // 断点（按源码行号升序）；F9 设置/清除
    CString m_sReplaceText;     // 「寻找替换」里的替换文本
    BOOL    m_bFindCase;        // 「区分大小写」
    int   m_nUndo;                      // 当前状态在下标几（栈里永远存着"当前"这一份）
    void  PushUndo();                   // 一次改动完成后调
    void  ApplySnapshot(const EcSnapshot& s);
    void  AfterEdit();                  // 改完源码统一收尾：Rebuild + Layout + 重画
    // 每个表格行的"折叠标记"命中框（Rebuild/Layout 时算好，点击时用）
    std::vector<CRect>   m_foldHit;

    // 「参数归属线」：原版在每条带参数的支持库命令语句上画一个 -/+，
    // 点它整篇文档显示/隐藏语句下面那些「※参数名：值」行（全局开关，原版实测如此）。
    // 标记的命中框按行记（和 m_foldHit 同形），参数行本身不建显示行。
    std::vector<CRect>   m_paramHit;
    BOOL  m_bParamShow;

    int   m_nCaretLine, m_nCaretCol;    // 插入点（源码行 / 列，列按字节）
    int   m_nAnchorLine, m_nAnchorCol;  // 选区另一端
    // ★ 第 27 轮（照原版 sub_4B5000 / sub_4B4980 改）：插入点落在**表格行**时记下在哪一格。
    //   原版点正文走 sub_4B5000，命中对象 sub_4B4980(obj)==13 就是"表格格"，
    //   然后用该单位自己的**每格左右边界数组**算出格内偏移。以前本工程点表格行直接把
    //   光标扔到下一条语句（用户 2026-10 报的「光标选不中表格里的地方」）。
    //   列按**该源码行里的字节偏移**记（和 m_nCaretCol 同一套语义），-1 = 不在表格格里。
    int   m_nCaretCell;
    int   m_nCaretCellCol;
    BOOL  m_bDragging;
    // 原版实测（shots\live_mark\before.png + after_click*.png）：文档刚打开时，
    // 流程槽里在**插入点所在行**画一个红色铅笔标记；用户第一次点鼠标/敲键盘后消失。
    BOOL  m_bOpenMark;

    CFont m_font, m_fontU, m_fontTable;
    int   m_nLineH;                     // 代码行高

    // ---------------------------------------------------------- 内部
    void  Rebuild();                    // 源码 -> 显示模型
    void  Layout();                     // 显示模型 -> 坐标 + 列宽
    void  EndTable(int nTable, int nFromRow);
    void  AddTableRow(int nTable, int nCols, int nFill, LPCTSTR const* pszCells,
                      const int* pColors, const BOOL* pUnder, const BOOL* pCheck,
                      const int* pSpan, int nCells, int nSrc);
    void  AddCodeRow(const CString& s, int nSrc);
    int   RowTopOfSrc(int nSrc) const;
    int   TextXOfIndent(int nIndent) const;

    void  DrawAll(CDC& dc);
    void  DrawTableRow(CDC& dc, int nRow);
    void  DrawCodeRow(CDC& dc, int nRow);
    void  DrawFlowLines(CDC& dc);
    void  DrawCommentBoxes(CDC& dc);        // 连续注释行的绿框（原版实测，见 .cpp）
    void  DrawParamLines(CDC& dc);          // 语句下面那些「※参数名：值」行
    int   ParamLineH() const;
    void  DrawOpenMark(CDC& dc);            // 流程槽里的「红色铅笔」（刚打开文档时）
    void  ClearOpenMark();                  // 首次交互后收掉它
    int   RowOfCaret() const;               // 插入点所在行（代码行优先，其次表格行）
    void  TokenizeDraw(CDC& dc, const CString& s, int x, int y,
                       int nSelA = -1, int nSelB = -1);
    void  DrawCheck(CDC& dc, int x, int y);

    int   HitTestRow(CPoint pt) const;                  // 返回 m_rows 下标，-1 = 无
    int   HitTestFold(CPoint pt) const;                 // 返回 m_rows 下标（点中折叠标记），-1 = 无
    int   HitTestParam(CPoint pt) const;                // 返回 m_rows 下标（点中参数归属线 -/+），-1 = 无
    void  CaretFromPoint(CPoint pt);
    CRect CaretRect() const;
    void  UpdateCaret();
    void  EnsureVisible();
    void  UpdateScrollBar();
    void  ScrollTo(int nNew);
    void  InsertText(LPCTSTR psz);
    void  DeleteRange(int nL0, int nC0, int nL1, int nC1);
    void  InsertNewline();                  // 在插入点断行（回车 / 粘贴里的换行）
    BOOL  HasSel() const;
    void  GetSel(int& l0, int& c0, int& l1, int& c1) const;
    // 该显示行落在选区里的字节范围（返回 FALSE = 这一行没被选到）
    BOOL  SelRangeOfRow(int nRow, int& nA, int& nB) const;
    void  SelectAll();
    void  CopySelToClipboard(BOOL bCut);
    void  PasteFromClipboard();

    // ---------------------------------------------------------- 消息
    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);       // 就地编辑的 EDIT 通知
    virtual BOOL PreTranslateMessage(MSG* pMsg);                // 编辑中的回车/ESC
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg LRESULT OnImeChar(WPARAM w, LPARAM l);
    afx_msg LRESULT OnSyncGutter(WPARAM w, LPARAM l);
    afx_msg LRESULT OnRedump(WPARAM w, LPARAM l);
    void DumpModel();                   // ECLONE_DUMP_MODEL 诊断
    DECLARE_MESSAGE_MAP()
};

#endif // _EC_EDITOR_H_

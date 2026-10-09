// ec_design.h -- MDI 文档标签条 + 窗体设计器视图
//
// 设计期的"真窗口"现在全部由**支持库**创建（原版 IDE 的做法）：
//   窗体 = 支持库里的"窗口"类型，组件 = 支持库里的窗口组件类型，
//   统一走 LIB_DATA_TYPE_INFO.m_pfnGetInterface(+44) → ITF 1 = PFN_CREATE_UNIT；
//   原版 e.exe 的设计器就是这么做的（e.exe sub_50D520），见
//   work\20260930-222630-5-9-e-exe-ide-krnln-sdk\krnln\window_designer_notes.md §7。
// 所以本文件不再自己注册 WTWindow/_EL_* 假类，也不再自己画组件。
#ifndef _EC_DESIGN_H_
#define _EC_DESIGN_H_

#include <afxwin.h>
#include <afxtempl.h>

#define EC_WM_TABSEL    (WM_USER + 110)
// 状态栏提示（主框架 CMainFrame::OnHint 收）。和 EClone.cpp 里的同名宏同值，
// 设计器要往状态栏写"放置失败"这类提示，所以这里也留一份。
#define EC_WM_HINT      (WM_USER + 111)
// 设计器选中变化 -> 主框架刷新"属性"页（wParam = MAKELONG(库, 类型)，lParam = 组件对象；lParam=0 表示无）
#define EC_WM_SELUNIT   (WM_USER + 114)

// 设计器视图里"基准窗口"（设计窗体）的位置：**固定在客户区左上角偏移 (10,10)**。
// 实测：原版 WTWindow 在屏幕 (275,189)，它的设计器视图在 (265,179) ⇒ 偏移 (10,10)。
// ★ 用户 2026-10 要求：基准窗口**不能拖动**（只能改尺寸），能拖的只有里面的组件。
//   原版也是这个行为：设计器里没有任何"拖动窗体本身"的交互。
#define EC_DESIGN_FORM_X  10
#define EC_DESIGN_FORM_Y  10
// 覆盖层比窗体矩形每边多铺这么多像素：原版的选中手柄中心落在"窗体矩形外扩 3px"的框线上，
// 覆盖层不多铺的话那圈手柄画不出来也点不到。
#define EC_DESIGN_PAD     6

// 文档标签条（对应原版的 "启动窗口" / "() 启动窗口程序集"）
class CECTabStrip : public CWnd
{
    DECLARE_DYNAMIC(CECTabStrip)
public:
    CECTabStrip();
    BOOL Create(CWnd* pParent, UINT nID);
    int  AddTab(LPCTSTR pszText);
    void SetCurSel(int n);
    int  GetCurSel() const { return m_nSel; }
    enum { TAB_H = 22 };   // VC6 不支持类内 static const 初始化
protected:
    CArray<CString, LPCTSTR> m_texts;
    CArray<CRect, const CRect&> m_rects;
    int      m_nSel;
    CFont    m_font;
    CFont    m_fontBold;   // 选中的标签是粗体（原版实测）
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP()
};

// 设计期的一个真组件实例（hunit = 支持库里的组件对象；hwnd = 它的窗口 = 对象 +28）
struct EcUnitInst
{
    void* hunit;
    HWND  hwnd;
    int   lib;             // 支持库下标
    int   dt;              // 库内数据类型下标
    TCHAR name[64];        // 显示名（"标签1"）
    // ★ 组件的**全部属性数据**（ITF 5 `PFN_GET_ALL_PROPERTY_DATA` 返回的 HGLOBAL 里那份字节）。
    // 建完组件立刻取一份存下来（用户 2026-10 明确指出："每个属性有返回的接口，拿过来要保存"）。
    // 用途：① 属性被改后"重建窗口"时把这份数据喂回 ITF 1（原版 sub_4AE230 就是这么干的：
    // ITF 5 取旧数据 -> ITF 1 重建）；② 以后存 .e 时直接写这份数据。
    // 为空表示"库没给"（拷不出来就留空，不能拿垃圾数据去建窗）。
    BYTE* pData;
    int   nData;
};

// 设计期的一个窗体实例。
// ★ 「新建窗体 / 切换窗体」= 这个数组里多一项 / 换 m_curForm（见 ec_design.cpp 的
//   CreateNewForm / SelectForm）。窗体**全部由支持库真建**（和原来一样），
//   非当前窗体整窗 ShowWindow(SW_HIDE)，所以切换是"藏一个显一个"，不重建。
struct EcFormInst
{
    void*   hunit;              // 窗体对象（支持库的）
    HWND    hwnd;               // 窗体窗口
    int     lib, dt;            // 窗体类型（属性表要用）
    CString name;               // "窗口1" / "_启动窗口"
    int     x, y, cx, cy;       // 设计期位置与尺寸（工作区坐标）
    CArray<EcUnitInst, const EcUnitInst&> units;   // 这个窗体上的组件
};

class CECDesigner;

// 透明覆盖窗口：设计期由它接管鼠标。
// （组件窗口自己也要收设计期鼠标，原版是靠组件把事件抛回宿主实现的；这里先用覆盖层，
//   效果一样——设计期点组件是"选中/拖动"，不是给组件用。）
class CECOverlay : public CWnd
{
    DECLARE_DYNAMIC(CECOverlay)
public:
    CECOverlay();
    BOOL Create(CWnd* pParent, UINT nID);
    CECDesigner* m_owner;
    // 覆盖层挂上之后，头 2 秒每 250ms 整层重画 + 顶 z 序（真窗体窗口后建会盖住它）。
    // 见 ec_design.cpp 的 CECOverlay::OnTimer。
    void StartRedrawBurst();
    int  m_nRedrawRounds;
protected:
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg LRESULT OnOverlayRedraw(WPARAM w, LPARAM l);   // EC_WM_OVREDRAW
    DECLARE_MESSAGE_MAP()
};

// 窗体设计器视图：深灰工作区 + 支持库建的真窗体 + 支持库建的真组件 + 选中手柄
class CECDesigner : public CWnd
{
    DECLARE_DYNAMIC(CECDesigner)
public:
    CECDesigner();
    ~CECDesigner();
    BOOL Create(CWnd* pParent, UINT nID);

    // 放置模式：在「工作夹->支持库」树里选中一个可视组件时由 CECChildFrame 调进来
    void ArmPlace(int lib, int dt, LPCTSTR pszName);
    void CancelPlace();
    BOOL IsPlacing() const { return m_armDt >= 0; }
    CString PlaceHint() const;
    int  UnitCount() const { return (m_curForm >= 0) ? (int)F().units.GetSize() : 0; }

    // 「工具->支持库配置」换库时用：先 DestroyUnits（销毁全部设计期真窗口），
    // 重载支持库之后再 RecreateForm（用新库重新建窗体）。
    void DestroyUnits();
    BOOL RecreateForm();

    // 「新建窗体 / 切换窗体」（原版菜单：插入->窗口 / 程序树里的窗口节点）
    int  FormCount() const { return (int)m_forms.GetSize(); }
    int  CurForm() const { return m_curForm; }
    LPCTSTR FormName(int i) const;           // i 越界返回 ""
    BOOL CreateNewForm(LPCTSTR pszName);     // 再建一个窗体（尺寸取 g_ecCfg 的"新建窗口宽/高"）
    BOOL SelectForm(int i);                  // 切到第 i 个窗体（藏旧的、显新的）

    LPCTSTR CursorAtCursor() const;          // 当前鼠标位置该用哪个光标（缩放手柄用）
    void    Redraw();                        // 重画自己 + 覆盖层（属性改了之后刷新画面用）

    // 由覆盖层转进来的鼠标/键盘（坐标 = 设计窗体客户区）
    void SurfaceDown(CPoint pt, BOOL bCtrl);
    void SurfaceMove(CPoint pt);
    void SurfaceUp(CPoint pt);
    void SurfaceKey(UINT nChar);
    void SurfacePaint(CDC* pDC);
    void RedrawOverlay();        // 覆盖层整层重画（钩子/定时器里要调，所以放 public）
    void SyncOverlay();          // 覆盖层重新贴合窗体（同上，放 public）
    void RememberOverlayState(); // 记下覆盖层"该画什么"的指纹
    BOOL OverlayStateChanged() const;   // 指纹变了没（常驻定时器据此决定要不要补画）
    int  m_ovState[4];
    // 设计期那块"纸 + 8px 点阵"画在**设计器视图**上（视图是窗体的父窗口 + WS_CLIPCHILDREN，
    // 所以天然在窗体下面）。⚠️ 别改回由覆盖层画：覆盖层被窗体盖住，屏幕上看不见。
    void DrawPaperOnView(CDC* pDC);
    // 覆盖层的"迟到整层重画"消息（覆盖层是 sizecbar 之外的独立子窗口，见 ec_design.cpp 的
    // CECOverlay::OnOverlayRedraw）：窗体窗口第一次 WM_PAINT 会盖掉覆盖层刚画的纸/点阵，
    // 所以挂完覆盖层之后要再排一次消息队列级的重画。
#define EC_WM_OVREDRAW  (WM_USER + 301)

protected:
    void CreateForm();                       // 用支持库建真窗体（"窗口"类型 → WTWindow）
    BOOL EcBuildFormWindow(int i);           // 真正建第 i 个窗体的窗口（CreateForm/CreateNewForm 共用）
    void EcAttachOverlay();                  // 把覆盖层挂到当前窗体上
    void DrawFormFrame(CDC* pDC);            // 没选组件时，选中框画在**窗体**上（原版开窗就是这样）
    void EcHookFormWindow(HWND hwnd);        // 钩住设计窗体：它每次自绘后重画覆盖层（见 ec_design.cpp）
    BOOL PlaceAt(const CRect& rc);           // 真正建组件（ITF 1）；成功返回 TRUE
    int  HitTest(CPoint pt) const;           // 命中的组件序号，-1 = 没有
    BOOL UnitRect(int idx, CRect& rc) const; // 组件在设计窗体客户区里的矩形
    BOOL UnitRectInOverlay(int idx, CRect& rc) const;  // 同上，但换算到覆盖层坐标（画/命中用）
    BOOL HitHandle(CPoint pt, int* pdx, int* pdy) const;   // 命中了哪个缩放手柄
    void Select(int idx);
    void DeleteSelected();
    void NotifySelection();                  // 告诉主框架"现在选中的是谁"（刷新属性表）

    CECOverlay m_overlay;
    // 覆盖层铺满"窗体矩形外扩 EC_DESIGN_PAD"，所以覆盖层坐标 = 设计器视图坐标，
    // 而"窗体客户区坐标"要减去 m_formOff 才落进覆盖层。所有画/命中判定都要走这个换算。
    CPoint     m_formOff;
    // ★ 窗体表 + 当前窗体。原来只有一个窗体（m_formUnit/m_formHwnd/m_formLib/m_formDt
    //   + m_units），本轮升级成多窗体；四个旧名字改成从"当前窗体"取的访问器，
    //   这样下面所有既有代码（用 FormHwnd() / Units() / FormUnit() / FormLib()…）一行都不用改。
    // ⚠️ 表里存**指针**：EcFormInst 里有 CArray 成员，VC6 的 CArray<T,T&> 装不下
    //    （afxtempl.h:277 报 "no copy constructor available"）。
    CArray<EcFormInst*, EcFormInst*> m_forms;
    int        m_curForm;                    // 当前窗体下标（-1 = 还没有）
    int        m_seqForm;                    // 新建窗体的命名计数（窗口1、窗口2…）

    void*      FormUnit() const { return (m_curForm >= 0) ? F().hunit : NULL; }
    HWND       FormHwnd() const { return (m_curForm >= 0) ? F().hwnd  : NULL; }
    int        FormLib()  const { return (m_curForm >= 0) ? F().lib   : -1; }
    int        FormDt()   const { return (m_curForm >= 0) ? F().dt    : -1; }
    // ⚠️ CArray 没有 const operator[]（VC6 的 MFC 6.0），而 UnitRect/DrawFormFrame 这些
    //    const 成员函数也要读当前窗体，所以这里统一走一个把 this 转掉的私有访问器。
    EcFormInst&       F() const { return *((CECDesigner*)this)->m_forms[m_curForm]; }
    CArray<EcUnitInst, const EcUnitInst&>& Units() const { return F().units; }
    int        m_sel;                        // 选中序号，-1 = 无
    int        m_armLib, m_armDt;            // 放置模式的目标类型
    CString    m_armName;
    BOOL       m_drag;
    int        m_dragMode;                   // 0=无 1=拖出新建 2=移动已选组件 3=缩放已选组件
    CPoint     m_dragPt;
    CRect      m_dragRect;                   // 模式 3 时 = 拖动开始时的组件矩形（窗体客户区坐标）
    int        m_rzDx, m_rzDy;               // 正在拖的那条边：-1 左/上，+1 右/下，0 不拖
    int        m_seq;                        // 命名/ID 计数
    CFont      m_font;

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()
};

// 「工具->支持库配置」重载支持库前后的两步（实现见 ec_design.cpp）：
// 卸载 .fne 之前必须先销毁所有设计期真窗口（它们是库建出来的对象）。
void EcDesignerDestroyAllUnits();
void EcDesignerRecreateAllForms();
void EcDesignerRedrawAll();

#endif // _EC_DESIGN_H_

// ec_design.cpp -- MDI 文档标签条 + 窗体设计器视图
//
// 设计期的真窗口**全部由支持库创建**：窗体是支持库的"窗口"类型，组件是支持库的窗口组件类型，
// 统一走 LIB_DATA_TYPE_INFO(+44).m_pfnGetInterface → ITF 1 = PFN_CREATE_UNIT。
// 原版 e.exe 的设计器就是这么做的（sub_50D520），所以这里**不再自己注册 WTWindow/_EL_* 假类**、
// 也不再自己画组件——组件长什么样由支持库决定，第三方 .fne 的组件同样能用。
#include "ec_design.h"
#include "ec_lib.h"
#include "ec_theme.h"      // g_ecCfg：设计器网格 / 新建窗体尺寸（系统配置「设计」页）

// 诊断追踪（定义在 ec_editor.cpp）。ECLONE_DUMP_MODEL=<path> 时写到 <path>.trace.txt。
extern void EcTrace(LPCTSTR pszFmt, ...);

// ================================================================== 标签条
IMPLEMENT_DYNAMIC(CECTabStrip, CWnd)

BEGIN_MESSAGE_MAP(CECTabStrip, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

CECTabStrip::CECTabStrip() : m_nSel(0)
{
}

BOOL CECTabStrip::Create(CWnd* pParent, UINT nID)
{
    m_font.CreateFont(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, GB2312_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                      DEFAULT_PITCH | FF_SWISS, _T("宋体"));
    m_fontBold.CreateFont(-12, 0, 0, 0, FW_BOLD, 0, 0, 0, GB2312_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                      DEFAULT_PITCH | FF_SWISS, _T("宋体"));
    return CWnd::CreateEx(0,
        AfxRegisterWndClass(0, ::LoadCursor(NULL, IDC_ARROW), NULL, NULL),
        _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        CRect(0, 0, 0, 0), pParent, nID);
}

int CECTabStrip::AddTab(LPCTSTR pszText)
{
    m_texts.Add(CString(pszText));
    m_rects.Add(CRect(0, 0, 0, 0));
    Invalidate(FALSE);
    return (int)m_texts.GetSize() - 1;
}

void CECTabStrip::SetCurSel(int n)
{
    if (n >= 0 && n < m_texts.GetSize())
    {
        m_nSel = n;
        Invalidate(FALSE);
    }
}

BOOL CECTabStrip::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

// 原版实测（tools\probe_live.ps1 把原版那个 CCustomTabCtrl 1:1 拍下来，
// 存 shots\live_tabs\tabbar0.png，1195x22；用 tools\px_map.py 打在点阵上量的）：
//   · 条底色 #F0F0F0；y=0 #A0A0A0、y=1 #696969 两道顶线；y=20 #E3E3E3、y=21 白
//   · 标签是「下边上斜」的梯形：上边 [x, x+w]、下边左右各内缩 16px，
//     只画左 '\' 和右 '/' 两条斜边（没有横边），左起 x=2
//   · 选中的画白面 + 黑字；没选中的是条底色 + 藏青字 #000080；标签上没有 ×
void CECTabStrip::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);
    const int w = rc.Width(), h = rc.Height();

    // 3D 边框（原版逐列实测）：左上两列/两行是 #A0A0A0 + #696969，
    // 右下一列/一行是 #E3E3E3 + 白（最右/最下那条是纯白）
    dc.FillSolidRect(rc, RGB(240, 240, 240));
    dc.FillSolidRect(0, 0, w - 1, 1, RGB(160, 160, 160));
    dc.FillSolidRect(0, 0, 1, h - 1, RGB(160, 160, 160));
    if (h > 4)
    {
        dc.FillSolidRect(1, 1, w - 2, 1, RGB(105, 105, 105));
        dc.FillSolidRect(1, 1, 1, h - 2, RGB(105, 105, 105));
        dc.FillSolidRect(0, h - 2, w - 1, 1, RGB(227, 227, 227));
        dc.FillSolidRect(w - 2, 1, 1, h - 2, RGB(227, 227, 227));
        dc.FillSolidRect(w - 1, 0, 1, h, RGB(255, 255, 255));
        dc.FillSolidRect(0, h - 1, w, 1, RGB(255, 255, 255));
    }

    CFont* pOld = dc.SelectObject(&m_font);
    dc.SetBkMode(TRANSPARENT);

    const int TOP = 2, BOT = h - 4, SLANT = 8, OVERLAP = 9;
    int x = 2;
    for (int i = 0; i < m_texts.GetSize(); i++)
    {
        BOOL bSel = (i == m_nSel);
        CFont* pF = bSel ? &m_fontBold : &m_font;   // 原版选中的标签是粗体（实测字形更宽）
        CFont* pOldF = dc.SelectObject(pF);
        CSize sz = dc.GetTextExtent(m_texts[i]);
        // 原版实测：tab1 `_启动窗口` 上边 [2,83]（tw=81，文字步进 54..55）；
        //            tab2 `启动窗口程序集`（选中/粗体）上边 [74,187]（tw=113）
        // 本工程 GDI 粗体步进比原版窄 7px，（+26/+33）是量出来的补偿，不是拍脑袋
        int tw = sz.cx + (bSel ? 33 : 26);
        if (tw < 40) tw = 40;

        POINT p[4];
        p[0].x = x;              p[0].y = TOP;      // 左上
        p[1].x = x + tw;         p[1].y = TOP;      // 右上
        p[2].x = x + tw - SLANT; p[2].y = BOT;      // 右下
        p[3].x = x + SLANT;      p[3].y = BOT;      // 左下

        CRgn rgn;
        rgn.CreatePolygonRgn(p, 4, ALTERNATE);
        CBrush br(bSel ? RGB(255, 255, 255) : RGB(240, 240, 240));
        dc.FillRgn(&rgn, &br);

        // 斜边：每 2 行走 1 列（原版实测斜率 0.5，16 行共走 8 列），两条都画
        for (int k = 0; k <= BOT - TOP; k++)
        {
            int dx = k / 2;
            dc.FillSolidRect(x + dx, TOP + k, 1, 1, RGB(0, 0, 0));
            dc.FillSolidRect(x + tw - dx, TOP + k, 1, 1, RGB(0, 0, 0));
        }
        // 标签底边那条深灰线（只在本标签的下边跨度里，原版 y=19）
        dc.FillSolidRect(x + SLANT + 1, BOT + 1, tw - 2 * SLANT - 2, 1, RGB(105, 105, 105));

        // 文字画在「半高处的那个跨度」里（原版实测文字中心 = 梯形半高处的中点）
        // 粗体那条再左挪 3px：GDI 合成粗体把字面往右撑，DT_CENTER 按步进居中会偏右 3px
        int dx = bSel ? 3 : 0;
        CRect rt(x + SLANT / 2 - dx, TOP, x + tw - SLANT / 2 - dx, BOT + 1);
        dc.SetTextColor(bSel ? RGB(0, 0, 0) : RGB(0, 0, 128));
        dc.DrawText(m_texts[i], rt, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        dc.SelectObject(pOldF);      // 还原字体（pOldF 就是进入本标签前选中的那个）

        m_rects[i] = CRect(x, 0, x + tw, h);
        x += tw - OVERLAP;           // 相邻标签在**上边**重叠 9px（原版实测：梯形互相咬合）
    }
    dc.SelectObject(pOld);
}

void CECTabStrip::OnLButtonDown(UINT nFlags, CPoint point)
{
    for (int i = 0; i < m_rects.GetSize(); i++)
    {
        if (m_rects[i].PtInRect(point))
        {
            if (i != m_nSel)
            {
                m_nSel = i;
                Invalidate(FALSE);
                GetParent()->SendMessage(EC_WM_TABSEL, (WPARAM)i, 0);
            }
            break;
        }
    }
    CWnd::OnLButtonDown(nFlags, point);
}

// ================================================================== 透明覆盖窗口（设计期鼠标）
IMPLEMENT_DYNAMIC(CECOverlay, CWnd)

BEGIN_MESSAGE_MAP(CECOverlay, CWnd)
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_KEYDOWN()
    ON_WM_SETCURSOR()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_TIMER()
    ON_MESSAGE(EC_WM_OVREDRAW, OnOverlayRedraw)
END_MESSAGE_MAP()

// "迟到"的整层重画：见 ec_design.h 里 EC_WM_OVREDRAW 的说明。
// 实现上是**短时定时器**：只 PostMessage 一次不够（窗体窗口建出来/第一次 WM_PAINT 的时机
// 在消息队列里排在它后面），挂上覆盖层之后头 2 秒每 250ms 整层重画一次就稳了。
#define EC_OVREDRAW_TIMER_ID  3201
#define EC_OVREDRAW_MS        250
#define EC_OVREDRAW_ROUNDS    8

LRESULT CECOverlay::OnOverlayRedraw(WPARAM, LPARAM)
{
    Invalidate(TRUE);
    UpdateWindow();
    return 0;
}

void CECOverlay::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == EC_OVREDRAW_TIMER_ID)
    {
        // ★ 常驻轻量自愈定时器：只在"该画的东西变了"时才同步重画一次。
        //   为什么不能只靠事件钩子：自动化脚本/用户在**设计器刚出现的那一瞬间**就去点、就去抓屏，
        //   这时壳窗口自己的重画还排在消息队列里（实测抓屏拿到过"上一次运行留下的页面"），
        //   覆盖层可能整块还没铺上。钩子负责"别人画完我补画"，这个定时器负责"没人画我兜底"。
        //   没变化时什么都不做（一次比较 + 一次消息，开销可忽略）。
        m_nRedrawRounds++;
        if (m_owner != NULL && m_owner->OverlayStateChanged())
        {
            ::SetWindowPos(GetSafeHwnd(), HWND_TOP, 0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            m_owner->SyncOverlay();
            m_owner->RedrawOverlay();
            if (::GetEnvironmentVariableA("ECLONE_TRACE_DES", NULL, 0) > 0)
                ::EcTrace(_T("ovtimer tick=%d (state changed -> repaint)"), m_nRedrawRounds);
        }
        return;
    }
    CWnd::OnTimer(nIDEvent);
}

void CECOverlay::StartRedrawBurst()
{
    m_nRedrawRounds = 0;
    // 常驻（不 KillTimer）：见 OnTimer 的说明。250ms 一次，且只在状态变了才真画。
    SetTimer(EC_OVREDRAW_TIMER_ID, EC_OVREDRAW_MS, NULL);
}

CECOverlay::CECOverlay() : m_owner(NULL), m_nRedrawRounds(0)
{
}

BOOL CECOverlay::Create(CWnd* pParent, UINT nID)
{
    // 用**固定类名**注册（自测脚本要按类名找它）。MFC 的注册套路是先用 DefWindowProc 注册，
    // 创建窗口时由 AfxHookWindowCreate 换成 AfxWndProc —— 所以必须走 CWnd::CreateEx 建窗。
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = ::DefWindowProc;
    wc.hInstance     = AfxGetInstanceHandle();
    wc.hCursor       = ::LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = _T("ECDesignOverlay");
    AfxRegisterClass(&wc);                 // 已注册过会返回 FALSE，忽略
    // ⚠️⚠️ **绝对不要加 `WS_EX_TRANSPARENT`**（第 26 轮真机踩到）：
    //    加了它，真实的鼠标点击会**穿过覆盖层**送给下面的设计窗体，覆盖层自己的
    //    `OnLButtonDown` 一次都不会被调到 —— 现象就是"在窗体上点组件选不中、8 个手柄
    //    怎么拖都不缩放"，而 `tools\resize_smoke.ps1` 却是绿的（它用 PostMessage 直接
    //    往覆盖层里塞鼠标消息，绕过了命中测试）。
    //    去掉它以后覆盖层正常吃鼠标（它本来就是透明绘制的：只画框/手柄/点阵，不挡视觉）。
    return CreateEx(0,
                    _T("ECDesignOverlay"), _T(""),
                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                    CRect(0, 0, 0, 0), pParent, nID);
}

BOOL CECOverlay::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CECOverlay::OnPaint()
{
    CPaintDC dc(this);
    if (m_owner != NULL)
        m_owner->SurfacePaint(&dc);
}

BOOL CECOverlay::OnSetCursor(CWnd*, UINT, UINT)
{
    if (m_owner == NULL)
    {
        ::SetCursor(::LoadCursor(NULL, IDC_ARROW));
        return TRUE;
    }
    LPCTSTR psz = m_owner->CursorAtCursor();
    ::SetCursor(::LoadCursor(NULL, psz));
    return TRUE;
}

// 当前鼠标位置该用哪个光标：放置模式 = 十字；压在缩放手柄上 = 对应的双向箭头；其余 = 箭头
LPCTSTR CECDesigner::CursorAtCursor() const
{
    if (IsPlacing())
        return IDC_CROSS;
    POINT sp;
    if (!::GetCursorPos(&sp))
        return IDC_ARROW;
    CPoint pt(sp);
    ::ScreenToClient(FormHwnd(), &pt);
    int dx = 0, dy = 0;
    if (!HitHandle(pt, &dx, &dy))
        return IDC_ARROW;
    if (dx < 0 && dy < 0) return IDC_SIZENWSE;
    if (dx > 0 && dy > 0) return IDC_SIZENWSE;
    if (dx > 0 && dy < 0) return IDC_SIZENESW;
    if (dx < 0 && dy > 0) return IDC_SIZENESW;
    if (dx != 0) return IDC_SIZEWE;
    return IDC_SIZENS;
}

void CECOverlay::OnLButtonDown(UINT nFlags, CPoint point)
{
    SetFocus();
    if (m_owner != NULL)
        m_owner->SurfaceDown(point, (nFlags & MK_CONTROL) ? TRUE : FALSE);
    CWnd::OnLButtonDown(nFlags, point);
}

void CECOverlay::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_owner != NULL)
        m_owner->SurfaceMove(point);
    CWnd::OnMouseMove(nFlags, point);
}

void CECOverlay::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_owner != NULL)
        m_owner->SurfaceUp(point);
    CWnd::OnLButtonUp(nFlags, point);
}

void CECOverlay::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (m_owner != NULL)
        m_owner->SurfaceKey(nChar);
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

// ================================================================== 设计画布
// 原版设计器实测（shots\live_des_orig\view2_client.png / form0.png，逐像素量的）：
//   · 工作区（画布）底色 **#A0A0A0**（本工程原来写的是 #808080，是错的）
//   · 窗体客户区那块"纸"是 **#F0F0F0**（= COLOR_BTNFACE；也就是窗体"底色"属性的默认值）
//   · 纸上打 1px 纯黑点阵，间距 **8px**，锚在**窗体客户区原点**：client (8i, 8j) 处有点，(0,0) 也有
//   · 选中框 = 离窗口矩形 3px 的 1px 黑点线；8 个手柄 = 7x7 的**白框**（1px 白边、内部透空），
//     中心落在框的四角与四边中点上
//
// ⚠️ 谁画"纸 + 点阵"：**只能由透明覆盖层 `ECDesignOverlay` 画**，别再试另外两条路：
//   ① 窗体的窗口类背景刷是 NULL_BRUSH 那一类，`WM_PRINTCLIENT`/`WM_PRINT` 什么都不画
//      （`probe_formpaint.ps1`：给它品红 DC，回来还是满屏品红）——
//      ⚠️ 但这个实验**证明不了"窗体不画点阵"**：krnln 的窗口过程压根不处理这两个消息。
//      真正管用的实验是 `tools\probe_repaint.ps1`：先在**屏幕上**用品红盖住窗体客户区，
//      再 `InvalidateRect(窗体,NULL,TRUE)+UpdateWindow(窗体)`，然后抓屏 ——
//      **原版的点阵和纸都回来了** ⇒ 原版是窗体自己的 WM_PAINT/WM_ERASEBKGND 画的。
//   ② 本工程的窗体不画点阵（同一份 krnln、同一个类、同一个窗口过程，实测两边
//      `class WndProc` 都是 `0x1C3E0A60`，差别只能在"设计期网格对象"那条建窗分支上，见下）。
//   ③ 在设计器视图（窗体的父窗口）里画 —— **不行**：Windows 会把父窗口 WM_PAINT 的裁剪区
//      排除掉子窗口占的矩形（实测本视图铺满客户区，窗体那块从来没被盖过）。
//
// krnln 侧的线索（静态伪 C，**没在调试器里确认**）：窗口类型的接口分发器 = `sub_100CBC90`
// （`sub_1005D120` 把它写进 `dword_1012F164` = LIB_INFO.pDataType(0x1012F138) + 0x2C），
// ITF1 → `sub_100CA6B0` 建窗。`sub_100CA6B0` 的 `if (a14 == 1)`（设计模式）里会建一个
// 0x10C 字节的"网格对象"（`sub_100C5D20`）挂到窗口 +368，再 `sub_100C5E80(v15, 0x50000000, 59396)`，
// **失败就把 +368 清 0**。推断本工程这条链没走通。要根治就在 `sub_100CA6B0`(0x100ca6b0) /
// `sub_100C5E80`(0x100c5e80) 下断点，在 IDE 里放一个组件。
#define EC_DESIGN_CANVAS   RGB(160, 160, 160)

// 窗体客户区那块"纸"的颜色。
// ⚠️ 本来想改成"读窗体自己的**底色**属性"，**没成**：本工程的窗体是 NULL 数据建的（底色字段是
//    垃圾值 3），而且拿 ITF 4 显式写 `底色`（属性下标 10，用 EcPropIndexByName 找的）**写不进去**
//    （写完回读还是 #030000）—— 和 §2.6 那条"有的类 ITF 4/6 的属性序号跟元数据顺序对不上"同类。
//    要根治得先逆出"窗口"类 ITF 4 的 `sub_100CAD20` 里 case 10 到底写哪个字段。
//    在那之前固定用 COLOR_BTNFACE（原版设计期的纸色本来就是它）。
#define EC_DESIGN_PAPER    RGB(240, 240, 240)
#define EC_DESIGN_GRID     8

// 选中框 + 8 个手柄（窗体与组件共用同一种画法）
//
// ★ 第 26 轮**照原版逐像素重做**（量法：`tools\handles_navy.py` 先按颜色找手柄、
//   `tools\frame_measure.py` 再打横竖扫描线；基准图 shots\desdeep\orig_navy_form.png）。
//   原版的真实画法（相对被选中单位的**窗口矩形**；单位在 (0,0)-(w,h)）：
//     · 阴影：单位右下各外扩 5px 的 L 形实心 #C0C0C0（上、左没有）
//     · 红框：外扩 3px 的**3px 粗实线** #FF0000（所以外边缘在 -3、内边缘在 0）
//     · 手柄：**5x5 实心藏青 #000080**，中心 = 红框的四角与四边中点
//       （角手柄的 ink 落在单位矩形 -3..+1，边手柄中心在框线上）
//   ⚠️ 本工程以前画的是"1px 黑色点线框 + 7x7 白色空框手柄"——那是**错的**（HANDOFF
//      第 20 轮那条"黑点线 + 白手柄"的结论来自误读，别再改回去）。
static void EcHandlePoints(const CRect& rcUnit, int hx[3], int hy[3])
{
    CRect f = rcUnit;
    f.InflateRect(3, 3);
    hx[0] = f.left; hx[1] = (f.left + f.right) / 2; hx[2] = f.right;
    hy[0] = f.top;  hy[1] = (f.top + f.bottom) / 2; hy[2] = f.bottom;
}

static void EcDrawUnitFrame(CDC* pDC, const CRect& rcUnit)
{
    CRect f = rcUnit;
    f.InflateRect(3, 3);

    // 1) 阴影：单位右下各外扩 5px 的 L 形
    CRect sh(rcUnit.left + rcUnit.Width(), rcUnit.top + 5,
             rcUnit.left + rcUnit.Width() + 5, rcUnit.bottom + 5);
    pDC->FillSolidRect(sh, RGB(192, 192, 192));
    CRect sh2(rcUnit.left + 5, rcUnit.top + rcUnit.Height(),
              rcUnit.left + rcUnit.Width() + 5, rcUnit.bottom + 5);
    pDC->FillSolidRect(sh2, RGB(192, 192, 192));

    // 2) 红框：外扩 3px、3px 粗的实心边框（原版没有点线效果）
    CPen pen(PS_SOLID, 3, RGB(255, 0, 0));
    CPen* pOldPen = pDC->SelectObject(&pen);
    CGdiObject* pOldB = pDC->SelectStockObject(NULL_BRUSH);
    pDC->Rectangle(f);
    pDC->SelectObject(pOldB);
    pDC->SelectObject(pOldPen);

    // 3) 8 个 5x5 实心藏青手柄（画在框线上，角上盖住红框的角）
    int hx[3], hy[3];
    EcHandlePoints(rcUnit, hx, hy);
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
        {
            if (i == 1 && j == 1)
                continue;
            pDC->FillSolidRect(hx[i] - 2, hy[j] - 2, 5, 5, RGB(0, 0, 128));
        }
}

// 组件在**设计窗体客户区**里的矩形（手柄命中判定 / 缩放都用它）
BOOL CECDesigner::UnitRect(int idx, CRect& rc) const
{
    if (m_curForm < 0 || idx < 0 || idx >= (int)Units().GetSize() || FormHwnd() == NULL)
        return FALSE;
    RECT wr;
    if (!::GetWindowRect(Units()[idx].hwnd, &wr))
        return FALSE;
    POINT p;
    p.x = wr.left;
    p.y = wr.top;
    ::ScreenToClient(FormHwnd(), &p);
    rc.SetRect(p.x, p.y, p.x + (wr.right - wr.left), p.y + (wr.bottom - wr.top));
    return TRUE;
}

// 同上，但换算到**覆盖层坐标**（画笔和鼠标都在覆盖层坐标系里）
BOOL CECDesigner::UnitRectInOverlay(int idx, CRect& rc) const
{
    if (!UnitRect(idx, rc))
        return FALSE;
    rc.OffsetRect(m_formOff);
    return TRUE;
}

// 鼠标现在落在哪个手柄上：dx/dy ∈ {-1,0,1}，都没命中返回 FALSE。
// idx < 0 表示"选中的是窗体本身"——窗体也有同样的 8 个手柄（原版就是靠它们改尺寸的，
// 窗体自己不能拖动）。原来这里 `if (m_sel < 0) return FALSE;` 直接把窗体手柄砍掉了，
// 所以窗体只能看着有手柄却拖不动 —— 用户 2026-10 报的就是这个。
BOOL CECDesigner::HitHandle(CPoint pt, int* pdx, int* pdy) const
{
    CRect rc;
    if (m_sel >= 0)
    {
        if (!UnitRectInOverlay(m_sel, rc))
            return FALSE;
    }
    else
    {
        if (FormHwnd() == NULL)
            return FALSE;
        RECT wr;
        if (!::GetWindowRect(FormHwnd(), &wr))
            return FALSE;
        rc.SetRect(EC_DESIGN_PAD, EC_DESIGN_PAD,
                   EC_DESIGN_PAD + (wr.right - wr.left), EC_DESIGN_PAD + (wr.bottom - wr.top));
    }
    int hx[3], hy[3];
    EcHandlePoints(rc, hx, hy);
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
        {
            if (i == 1 && j == 1)
                continue;
            if (abs(pt.x - hx[i]) <= 4 && abs(pt.y - hy[j]) <= 4)
            {
                if (pdx) *pdx = (i == 0) ? -1 : ((i == 2) ? 1 : 0);
                if (pdy) *pdy = (j == 0) ? -1 : ((j == 2) ? 1 : 0);
                return TRUE;
            }
        }
    return FALSE;
}

// 覆盖层里画的"窗体框 + 8 个手柄"。覆盖层已铺满"窗体矩形外扩 EC_DESIGN_PAD"，
// 所以窗体矩形在覆盖层坐标里就是 (PAD, PAD, PAD+w, PAD+h)，手柄正好全在层内。
void CECDesigner::DrawFormFrame(CDC* pDC)
{
    if (FormHwnd() == NULL)
        return;
    RECT wr;
    if (!::GetWindowRect(FormHwnd(), &wr))
        return;
    CRect rc(EC_DESIGN_PAD, EC_DESIGN_PAD,
             EC_DESIGN_PAD + (wr.right - wr.left), EC_DESIGN_PAD + (wr.bottom - wr.top));
    EcDrawUnitFrame(pDC, rc);
}

// ================================================================== 设计器视图
IMPLEMENT_DYNAMIC(CECDesigner, CWnd)

BEGIN_MESSAGE_MAP(CECDesigner, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

// 活着的设计器表：「支持库配置」重载支持库时要能找到它们（先销毁真窗口再 FreeLibrary）。
// 用 CPtrArray 而不是 std::vector：CECDesigner 是 MFC 对象，指针数组够用。
static CPtrArray g_liveDesigners;

CECDesigner::~CECDesigner()
{
    int i;
    for (i = 0; i < (int)g_liveDesigners.GetSize(); i++)
        if (g_liveDesigners[i] == this)
        {
            g_liveDesigners.RemoveAt(i);
            break;
        }
}

void CECDesigner::OnDestroy()
{
    CWnd::OnDestroy();
    int i;
    for (i = 0; i < (int)g_liveDesigners.GetSize(); i++)
        if (g_liveDesigners[i] == this)
        {
            g_liveDesigners.RemoveAt(i);
            break;
        }
}

// 销毁设计期全部真窗口：先覆盖层（窗体客户区上的子窗口），再组件，最后窗体本身。
// ⚠️ 顺序不能反：组件/窗体是支持库建的对象，销毁要走 EcDestroyUnit（对象虚表 vtbl[1]）。
// 销毁**全部**窗体（连同它们上面的组件）。换支持库之前必须先走这一步。
void CECDesigner::DestroyUnits()
{
    int f;
    for (f = (int)m_forms.GetSize() - 1; f >= 0; f--)
    {
        EcFormInst* p = m_forms[f];
        if (p == NULL)
            continue;
        int i;
        for (i = (int)p->units.GetSize() - 1; i >= 0; i--)
            EcDestroyUnit(p->units[i].hunit);
        p->units.RemoveAll();
    }
    m_sel = -1;

    // 覆盖层是"当前窗体的子窗口"，窗体一销毁它就没了 —— 先显式干一次
    if (m_overlay.GetSafeHwnd() != NULL)
        m_overlay.DestroyWindow();

    for (f = (int)m_forms.GetSize() - 1; f >= 0; f--)
    {
        EcFormInst* p = m_forms[f];
        if (p == NULL)
            continue;
        if (p->hunit != NULL)
            EcDestroyUnit(p->hunit);
        delete p;
    }
    m_forms.RemoveAll();
    m_curForm = -1;
    CancelPlace();
    Invalidate(FALSE);
}

BOOL CECDesigner::RecreateForm()
{
    // 换支持库之后重建：**只重建第一个窗体**（保持原来的行为；多窗体不跨换库保留，
    // 因为组件对象是旧库建出来的，全部已经销毁了）。
    DestroyUnits();
    CreateForm();
    return FormHwnd() != NULL;
}

void EcDesignerDestroyAllUnits()
{
    int i;
    for (i = (int)g_liveDesigners.GetSize() - 1; i >= 0; i--)
    {
        CECDesigner* p = (CECDesigner*)g_liveDesigners[i];
        if (p != NULL && p->GetSafeHwnd() != NULL)
            p->DestroyUnits();
    }
}

void EcDesignerRecreateAllForms()
{
    int i;
    for (i = 0; i < (int)g_liveDesigners.GetSize(); i++)
    {
        CECDesigner* p = (CECDesigner*)g_liveDesigners[i];
        if (p != NULL && p->GetSafeHwnd() != NULL)
            p->RecreateForm();
    }
}

void EcDesignerRedrawAll()
{
    int i;
    for (i = 0; i < (int)g_liveDesigners.GetSize(); i++)
    {
        CECDesigner* p = (CECDesigner*)g_liveDesigners[i];
        if (p != NULL && p->GetSafeHwnd() != NULL)
            p->Redraw();
    }
}

void CECDesigner::Redraw()
{
    RedrawOverlay();
    Invalidate(FALSE);
}

CECDesigner::CECDesigner()
    : m_curForm(-1), m_seqForm(0), m_sel(-1),
      m_armLib(-1), m_armDt(-1), m_drag(FALSE), m_dragMode(0), m_rzDx(0), m_rzDy(0), m_seq(0)
{
    m_ovState[0] = m_ovState[1] = m_ovState[2] = m_ovState[3] = -1;   // 强制第一次补画
    g_liveDesigners.Add(this);
}

// 在已载入的支持库里按名字找数据类型（不写死下标，第三方库一样能用）
static BOOL EcFindType(LPCTSTR pszLibBase, LPCTSTR pszType, int* plib, int* pdt)
{
    for (int i = 0; i < EcLibCount(); i++)
    {
        const EcLib& L = EcLibAt(i);
        if (pszLibBase != NULL && L.path.Find(pszLibBase) < 0)
            continue;
        for (int k = 0; k < L.nDataType; k++)
        {
            const EcDataType* d = EcDtAt(i, k);
            if (d != NULL && d->name == pszType)
            {
                *plib = i;
                *pdt = k;
                return TRUE;
            }
        }
    }
    return FALSE;
}

// 真正用支持库建第 i 个窗体的窗口，并把覆盖层挂上去。
// （拆出来是因为"新建窗体 / 切换窗体"两边都要用；原来这段是内联在 CreateForm 里的。）
// ---------------------------------------------------------------- "事后重画"钩子
// ⚠️⚠️ 为什么要钩：覆盖层（ECDesignOverlay）虽然被 `SetWindowPos(HWND_TOP)` 顶到兄弟窗口最上面，
//    但**别人后来重画时会把覆盖层那一片盖回去**（Windows 不保证"z 序在上"的兄弟窗口最后画）。
//    实测不稳定：连开 4 次同样的操作，藏青手柄像素 = 0/85 / 200 / 200 / 200。
//    只在挂覆盖层之后"定时补画 8 帧"治不了根（补完还是可能被盖）。
//    正解 = **事件驱动**：钩住三个窗口的窗口过程，它们的 WM_PAINT/WM_ERASEBKGND/WM_NCPAINT/
//    WM_SIZE/WM_MOVE 之后立刻让覆盖层整层重画一次。这样"谁最后画"就永远是覆盖层：
//      * **设计窗体**（真组件窗口）—— 它自己画自己那块
//      * **设计器视图**（覆盖层和窗体的父窗口）—— 它一画，子窗口那块会被重绘
//      * **MDI 子框架**（视图的父）—— 它移动/激活时也会重画
//    实现用 `SetProp` 把「旧窗口过程 + 设计器」挂在**窗口自己**的属性上：多窗口各自独立，
//    不用全局表，也不会把 A 窗口的消息送给 B 窗口。
#define EC_GWLP_WNDPROC      (-4)
#define EC_FORMPROP_HOOK     _T("ECloneFormHook")
struct EcFormHook { WNDPROC oldproc; CECDesigner* owner; };

// 重入闸门：`RedrawOverlay()` 是同步重画，会经过被钩窗口的窗口过程，必须挡住自我递归
static int g_inOverlayRedraw = 0;
static inline BOOL EcInOverlayRedraw() { return g_inOverlayRedraw != 0; }
struct EcOverlayRedrawGuard
{
    EcOverlayRedrawGuard()  { g_inOverlayRedraw++; }
    ~EcOverlayRedrawGuard() { g_inOverlayRedraw--; }
};

static LRESULT CALLBACK EcFormHookProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    EcFormHook* h = (EcFormHook*)::GetProp(hwnd, EC_FORMPROP_HOOK);
    LRESULT r = (h != NULL && h->oldproc != NULL)
                    ? ::CallWindowProc(h->oldproc, hwnd, msg, wp, lp)
                    : ::DefWindowProc(hwnd, msg, wp, lp);
    // 这个窗口画完之后，把覆盖层整层补画一次（纸/点阵/框/手柄都在覆盖层上）
    // ⚠️ 必须防重入：`RedrawOverlay()` 内部是 `RDW_UPDATENOW`（同步画），覆盖层一画又会
    //    经过它自己的窗口过程 —— 不挡的话就是无限递归/自我打断，实测症状正是
    //    "navy 像素 0/85/200/232 每次都不一样"。
    if (h != NULL && h->owner != NULL && !EcInOverlayRedraw() &&
        (msg == WM_PAINT || msg == WM_ERASEBKGND || msg == WM_NCPAINT ||
         msg == WM_SIZE || msg == WM_MOVE || msg == WM_MDIACTIVATE))
        h->owner->RedrawOverlay();
    return r;
}

void CECDesigner::EcHookFormWindow(HWND hwnd)
{
    if (hwnd == NULL || ::GetProp(hwnd, EC_FORMPROP_HOOK) != NULL)
        return;                                     // 已经钩过（或本来就是我们钩的）
    EcFormHook* h = new EcFormHook;
    if (h == NULL)
        return;
    h->oldproc = (WNDPROC)(INT_PTR)::GetWindowLongA(hwnd, EC_GWLP_WNDPROC);
    h->owner = this;
    ::SetProp(hwnd, EC_FORMPROP_HOOK, (HANDLE)h);
    ::SetWindowLongA(hwnd, EC_GWLP_WNDPROC, (LONG)(INT_PTR)EcFormHookProc);
    if (::GetEnvironmentVariableA("ECLONE_TRACE_DES", NULL, 0) > 0)
    {
        extern void EcTrace(LPCTSTR pszFmt, ...);
        EcTrace(_T("hook hwnd=%p class=%s oldproc=%p"),
                hwnd, hwnd == GetSafeHwnd() ? _T("<view>") : _T("<form>"), h->oldproc);
    }
}

BOOL CECDesigner::EcBuildFormWindow(int i)
{
    if (i < 0 || i >= (int)m_forms.GetSize() || m_forms[i] == NULL)
        return FALSE;
    EcFormInst& fi = *m_forms[i];
    fi.hunit = EcCreateUnit(fi.lib, fi.dt, GetSafeHwnd(), 1,
                            fi.x, fi.y, fi.cx, fi.cy, GetSafeHwnd(), TRUE);
    fi.hwnd = EcUnitHwnd(fi.hunit);
    if (fi.hwnd == NULL)
    {
        fi.hunit = NULL;
        return FALSE;
    }
    // ⚠️ 原版设计期的第一个窗体**没有标题文字**（实测 `GetWindowText` 是空串，抓屏那条 25px 高的
    //    标题栏上什么字都没有）。所以这里**不要**再 SetWindowText —— 加了就会多出一行
    //    "Windows窗口程序"（用户截图里一眼就看出来的差异）。程序树里那个名字是窗体**名**属性，
    //    跟窗口文字不是一回事。
    // ::SetWindowText(fi.hwnd, (i == 0) ? _T("Windows窗口程序") : (LPCTSTR)fi.name);
    ::ShowWindow(fi.hwnd, SW_SHOW);   // 支持库建的窗口默认不带 WS_VISIBLE，得自己显出来
    // 钩住它：之后它每次自己重画，都把覆盖层补画一遍（见 EcFormHookProc 的说明）
    EcHookFormWindow(fi.hwnd);
    return TRUE;
}

// 把覆盖层挂到**当前窗体**上（切换窗体时先 DestroyWindow 再重建）。
// 覆盖层现在是**设计器视图的子窗口**（原来挂在窗体上），这样它能盖住窗体**外面**那圈
// 选中框和 8 个手柄 —— 原版的手柄中心落在"窗体矩形外扩 3px"的边框上，也就是窗体客户区
// **之外**；挂成窗体的子窗口时那圈画了也点不到（会被客户区裁掉）。
// 于是覆盖层的客户区坐标 = 设计器视图坐标；窗体客户区坐标要减 m_formOff 才落进来。
void CECDesigner::EcAttachOverlay()
{
    if (m_overlay.GetSafeHwnd() != NULL)
        m_overlay.DestroyWindow();
    if (GetSafeHwnd() == NULL)
        return;
    if (m_overlay.Create(this, 3001))
    {
        m_overlay.m_owner = this;
        SyncOverlay();
        // ★ 挂完**必须整层失效重画一次**。覆盖层从 0x0 被 SetWindowPos 撑到整个窗体，
        //   新露出来的区域不会被自动重画（WM_PAINT 只画更新区），屏幕上是"上一次的旧像素"
        //   —— 实测症状：纸和点阵都不出来，只有手柄/框可见（那是后来拖动才触发重画的）。
        //   强制 UpdateWindow 一次，启动时就是完整的一层。
        m_overlay.Invalidate(TRUE);
        m_overlay.UpdateWindow();
    }
}

// ---------------------------------------------------------------- 新建 / 切换窗体
LPCTSTR CECDesigner::FormName(int i) const
{
    if (i < 0 || i >= (int)m_forms.GetSize() || m_forms[i] == NULL)
        return _T("");
    return m_forms[i]->name;
}

BOOL CECDesigner::CreateNewForm(LPCTSTR pszName)
{
    int lib = -1, dt = -1;
    if (!EcFindType(_T("krnln"), _T("窗口"), &lib, &dt))
        return FALSE;

    EcFormInst* pfi = new EcFormInst;
    if (pfi == NULL)
        return FALSE;
    pfi->hunit = NULL;
    pfi->hwnd  = NULL;
    pfi->lib   = lib;
    pfi->dt    = dt;
    if (pszName != NULL && *pszName != 0)
    {
        pfi->name = pszName;
    }
    else
    {
        // 名字查重：窗口1、窗口2…
        for (;;)
        {
            m_seqForm++;
            CString s;
            s.Format(_T("窗口%d"), m_seqForm);
            BOOL bUsed = FALSE;
            for (int i = 0; i < (int)m_forms.GetSize(); i++)
                if (m_forms[i] != NULL && m_forms[i]->name == s) { bUsed = TRUE; break; }
            if (!bUsed) { pfi->name = s; break; }
        }
    }
    pfi->x = 30;
    pfi->y = 30 + 20 * (int)m_forms.GetSize();    // 错开一点，多个窗体不至于完全重叠
    pfi->cx = g_ecCfg.nNewFormW;                  // 尺寸取「系统配置->显示->新建窗口宽/高」
    pfi->cy = g_ecCfg.nNewFormH;
    if (pfi->cx < 30) pfi->cx = 380;
    if (pfi->cy < 30) pfi->cy = 250;

    m_forms.Add(pfi);
    int n = (int)m_forms.GetSize() - 1;
    if (!EcBuildFormWindow(n))
    {
        m_forms.RemoveAt(n);
        delete pfi;
        return FALSE;
    }
    // ⚠️ 必须**先藏旧的再设当前**：原来只写 m_curForm = n，于是旧窗体不藏 ——
    //    实测「插入->窗口」连按两次会看到 3 个 WTWindow 全 visible（见 probe_forms.ps1）。
    int old = m_curForm;
    m_curForm = n;
    if (old >= 0 && m_forms[old] != NULL && m_forms[old]->hwnd != NULL)
        ::ShowWindow(m_forms[old]->hwnd, SW_HIDE);
    EcAttachOverlay();
    m_sel = -1;
    CancelPlace();
    Select(-1);
    Invalidate(FALSE);
    return TRUE;
}

BOOL CECDesigner::SelectForm(int i)
{
    if (i < 0 || i >= (int)m_forms.GetSize() || m_forms[i] == NULL)
        return FALSE;
    if (i == m_curForm)
        return TRUE;
    int old = m_curForm;
    if (old >= 0 && m_forms[old] != NULL && m_forms[old]->hwnd != NULL)
        ::ShowWindow(m_forms[old]->hwnd, SW_HIDE);    // 旧窗体整窗藏起来（连它的组件一起）
    m_curForm = i;
    if (m_forms[i]->hwnd != NULL)
        ::ShowWindow(m_forms[i]->hwnd, SW_SHOW);
    EcAttachOverlay();
    m_sel = -1;
    CancelPlace();
    Select(-1);
    Invalidate(FALSE);
    return TRUE;
}

void CECDesigner::CreateForm()
{
    int lib = -1, dt = -1;
    if (!EcFindType(_T("krnln"), _T("窗口"), &lib, &dt))
        return;                       // 核心库没载入：设计器只剩一片空白工作区
    // 原版实测（shots\live_des_orig\windows.txt，逐个窗口量出来的）：
    //   WTWindow(380x250) 在屏幕 (275,189)，它的**设计器视图**在 (265,179)
    //   ⇒ 窗体在视图客户区的偏移 = **(10,10) 固定**，不是可拖动的自由位置。
    //   ★ 用户 2026-10 明确要求：基准窗口**不能拖动**，只能改尺寸，组件才可以拖。
    //     （原版也一样：整个设计器里没有任何"拖动窗体本身"的交互。）
    //   ⚠️ 本工程原来写的是 (30,30)，与实测差 20px。
    EcFormInst* pfi = new EcFormInst;
    if (pfi == NULL)
        return;
    pfi->hunit = NULL;
    pfi->hwnd  = NULL;
    pfi->lib   = lib;
    pfi->dt    = dt;
    pfi->name  = _T("_启动窗口");     // 第一个窗体的名字（原版程序树里就是它）
    pfi->x = EC_DESIGN_FORM_X; pfi->y = EC_DESIGN_FORM_Y;
    pfi->cx = 380; pfi->cy = 250;
    m_forms.Add(pfi);
    m_curForm = (int)m_forms.GetSize() - 1;
    if (!EcBuildFormWindow(m_curForm))
    {
        m_forms.RemoveAt(m_curForm);
        delete pfi;
        m_curForm = -1;
        return;
    }
    // 覆盖层是"设计窗体的子窗口"：它的客户区坐标就等于窗体客户区坐标
    EcAttachOverlay();
    Invalidate(FALSE);          // 窗体不画自己的背景，"纸"+点阵由本视图画（见 DrawPaper）

    // 自测/截图用：ECLONE_DESIGN_DEMO=1 时开机就在原版那个位置放一个真"图形按钮"，
    // 并给**每个额外载入的支持库**（ECLONE_EXTRA_LIBS）也放一个它的第一个可视组件
    // —— 用来证明"扩展组件"同样能放到窗体上。
    char buf[8];
    if (::GetEnvironmentVariableA("ECLONE_DESIGN_DEMO", buf, sizeof(buf)) > 0)
    {
        int bl = -1, bd = -1;
        if (EcFindType(_T("krnln"), _T("图形按钮"), &bl, &bd))
        {
            m_armLib = bl; m_armDt = bd; m_armName = _T("图形按钮");
            PlaceAt(CRect(142, 74, 142 + 104, 74 + 112));   // 原版实测的 104x112
        }
        int y = 74 + 130;
        if (y + 80 > 210)
            y = 130;                     // 别放到窗体客户区外面
        for (int i = 0; i < EcLibCount(); i++)
        {
            const EcLib& L = EcLibAt(i);
            if (L.path.Find(_T("krnln")) >= 0)
                continue;
            for (int k = 0; k < L.nDataType; k++)
            {
                if (!EcDtIsVisualUnit(i, k))
                    continue;
                const EcDataType* d = EcDtAt(i, k);
                if (d == NULL)
                    continue;
                m_armLib = i; m_armDt = k; m_armName = d->name;
                if (PlaceAt(CRect(20, y, 20 + 120, y + 80)))
                {
                    y += 90;
                    break;                   // 每个库只放第一个能建出来的
                }
            }
        }
        CancelPlace();
    }

    NotifySelection();      // 让"工作夹-属性"页一开始就显示窗体自己的属性（原版也是这样）
}

BOOL CECDesigner::Create(CWnd* pParent, UINT nID)
{
    m_font.CreateFont(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, GB2312_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                      DEFAULT_PITCH | FF_SWISS, _T("宋体"));
    if (!CWnd::CreateEx(0,
            AfxRegisterWndClass(0, ::LoadCursor(NULL, IDC_ARROW), NULL, NULL),
            _T(""), WS_CHILD | WS_CLIPSIBLINGS,
            CRect(0, 0, 0, 0), pParent, nID))
        return FALSE;
    CreateForm();
    return TRUE;
}

// ------------------------------------------------------------------ 放置模式
void CECDesigner::ArmPlace(int lib, int dt, LPCTSTR pszName)
{
    m_armLib = lib;
    m_armDt  = dt;
    m_armName = pszName;
    if (m_overlay.GetSafeHwnd() != NULL)
        ::SetCursor(::LoadCursor(NULL, IDC_CROSS));
}

void CECDesigner::CancelPlace()
{
    m_armLib = m_armDt = -1;
    m_armName.Empty();
    if (m_overlay.GetSafeHwnd() != NULL)
        ::SetCursor(::LoadCursor(NULL, IDC_ARROW));
}

CString CECDesigner::PlaceHint() const
{
    if (!IsPlacing())
        return CString(_T(""));
    CString s;
    s.Format(_T("   [放置模式：%s —— 在窗体上单击用默认尺寸，拖出矩形用指定尺寸，ESC 取消]"),
             (LPCTSTR)m_armName);
    return s;
}

// ------------------------------------------------------------------ 建 / 选 / 拖 / 删
BOOL CECDesigner::PlaceAt(const CRect& rcIn)
{
    if (FormHwnd() == NULL || m_armLib < 0)
        return FALSE;
    CRect rc = rcIn;
    rc.NormalizeRect();
    if (rc.Width() < 8 || rc.Height() < 8)
    {
        // 单击 = 用支持库给的设计期默认尺寸（ITF 12 → PFN_ON_NOTIFY_UNIT(NU_GET_CREATE_SIZE_IN_DESIGNER)）
        int cx = 100, cy = 100;
        EcDtDefaultSize(m_armLib, m_armDt, &cx, &cy);
        rc.right = rc.left + cx;
        rc.bottom = rc.top + cy;
    }

    const EcDataType* d = EcDtAt(m_armLib, m_armDt);
    m_seq++;
    CString nm;
    nm.Format(_T("%s%d"), (d != NULL && !d->name.IsEmpty()) ? (LPCTSTR)d->name : _T("组件"), m_seq);

    // ★ 用**默认属性数据**建组件（不是 NULL/0）：这样库才会把每个属性填成默认值
    //   （原版那 8 个固定属性里"标记"默认是 1、"可视"默认真），否则 `_EL_RgnButton`
    //   会画成一块纯黑。见 ec_lib.cpp 的 EcGetDefaultPropertyData。
    void* u = EcCreateUnitDefault(m_armLib, m_armDt, FormHwnd(), (UINT)(2000 + m_seq - 1),
                                  rc.left, rc.top, rc.Width(), rc.Height(), FormHwnd(), TRUE);
    HWND hw = EcUnitHwnd(u);
    if (u == NULL || hw == NULL)
    {
        // 有的库的类型给 NULL 数据建不出来（HANDOFF 坑 3.5-2/3.5-3）。原版什么提示没实测，
        // 这里给个看得见的提示 + 一声响，别让人以为"点了没反应"。
        ::MessageBeep(0);
        CWnd* pMain = AfxGetMainWnd();
        if (pMain != NULL)
        {
            CString msg;
            msg.Format(_T("这个支持库组件建不出来（%s）：支持库拒绝了 NULL 数据的建窗请求。"),
                       (d != NULL && !d->name.IsEmpty()) ? (LPCTSTR)d->name : _T("组件"));
            pMain->SendMessage(EC_WM_HINT, 0, (LPARAM)(LPCTSTR)msg);
        }
        return FALSE;
    }
    ::SetWindowTextA(hw, nm);       // 兜底：窗口文字
    // 固定属性写成我们摆的位置（不写的话 NULL 数据建出来的对象里这些字段是垃圾值）
    EcInitUnitGeometry(m_armLib, m_armDt, u, rc.left, rc.top, rc.Width(), rc.Height());
    // 正路：走 ITF 4 设"标题"属性（原版属性表改属性就是这条）
    EcSetUnitTitle(m_armLib, m_armDt, u, nm);
    ::ShowWindow(hw, SW_SHOW);      // 支持库建的窗口默认不带 WS_VISIBLE

    // ★ 建完立刻把组件的**全部属性数据**取一份存下来（ITF 5）。
    //   用户 2026-10 明确要求："每个属性有返回的接口，拿过来要保存"。
    //   这份字节就是以后"重建窗口"（属性改了要重建）和"存 .e"要用的东西，
    //   必须在**属性设完之后**取，否则存的是建窗那一刻的中间状态。
    EcUnitInst inst;
    memset(&inst, 0, sizeof(inst));
    inst.hunit = u;
    inst.hwnd  = hw;
    inst.lib   = m_armLib;
    inst.dt    = m_armDt;
    lstrcpyn(inst.name, nm, 64);
    inst.pData = NULL;
    inst.nData = 0;
    EcGetAllPropertyData(m_armLib, m_armDt, u, &inst.pData, &inst.nData);

    Units().Add(inst);
    Select((int)Units().GetSize() - 1);
    SyncOverlay();
    return TRUE;
}

int CECDesigner::HitTest(CPoint pt) const
{
    // ⚠️ pt 是**覆盖层坐标**（SurfaceDown 从覆盖层的鼠标消息里传进来），而这里的矩形要按
    //    "窗体客户区 + m_formOff"换算 —— 以前拿窗体客户区坐标直接比，整体偏了 (PAD,PAD)，
    //    点组件边缘会选不中（真机 bug）。
    for (int i = (int)Units().GetSize() - 1; i >= 0; i--)   // 后建的在上层
    {
        CRect r;
        if (!UnitRectInOverlay(i, r))
            continue;
        if (r.PtInRect(pt))
            return i;
    }
    return -1;
}

void CECDesigner::Select(int idx)
{
    m_sel = idx;
    RedrawOverlay();
    Invalidate(FALSE);   // 选中框要么画在窗体上、要么画在组件上（在覆盖层里）
    NotifySelection();
}

// 选中的组件（没选组件时就是窗体本身）-> 主框架刷新"工作夹-属性"页
// ⚠️ 要发给**主框架**（CMainFrame 才有 OnSelUnit）；GetParentFrame() 给的是 MDI 子窗口。
void CECDesigner::NotifySelection()
{
    CWnd* pMain = AfxGetMainWnd();
    if (pMain == NULL || pMain->GetSafeHwnd() == NULL)
        return;
    if (m_sel >= 0 && m_sel < (int)Units().GetSize())
        pMain->PostMessage(EC_WM_SELUNIT,
                           (WPARAM)MAKELONG(Units()[m_sel].lib, Units()[m_sel].dt),
                           (LPARAM)Units()[m_sel].hunit);
    else
        pMain->PostMessage(EC_WM_SELUNIT,
                           (WPARAM)MAKELONG(FormLib(), FormDt()), (LPARAM)FormUnit());
}

void CECDesigner::DeleteSelected()
{
    if (m_sel < 0 || m_sel >= (int)Units().GetSize())
        return;
    EcDestroyUnit(Units()[m_sel].hunit);   // 对象虚表 vtbl[1](obj,1)：与 krnln 自己的回滚路径一致
    Units().RemoveAt(m_sel);
    m_sel = -1;
    RedrawOverlay();
    Invalidate(FALSE);                     // 回到"选中窗体"，窗体框要画出来
    NotifySelection();                     // 删掉后属性表回到"窗体"
}

// 覆盖层铺满"**窗体客户区**外扩 EC_DESIGN_PAD"，并记住窗体客户区坐标 -> 覆盖层坐标的偏移。
// 之后所有"窗体客户区坐标"都要加 m_formOff 才落进覆盖层。
//
// ⚠️⚠️ 覆盖层必须从**客户区**起铺（用 ClientToScreen 拿客户区原点做锚）：
// 原版设计器里窗体的**标题栏**（蓝色渐变 + 最小化/最大化/关闭）是画出来的
// （实拍 shots\orig_designer_activated.png 上半）。早先用"窗体窗口矩形"定位，覆盖层就把
// 标题栏和边框整圈盖掉 —— 用户 2026-10 说"你得和原版一样"就是这个。
// 形状 = **Windows 窗口矩形 + 每边 PAD**：客户区之上那部分不铺，所以标题栏露出来；
// 客户区四周各多铺 PAD，好让"外扩 3px"的选中手柄画得出来、点得到。
// ⚠️ 覆盖层是弹出式子窗口（ECOverlay::Create 用 WS_POPUP），位置参数是**屏幕坐标**。
void CECDesigner::SyncOverlay()
{
    if (FormHwnd() == NULL || m_overlay.GetSafeHwnd() == NULL)
        return;
    RECT fr;
    if (!::GetWindowRect(FormHwnd(), &fr))
        return;

    // 窗体客户区原点（屏幕）
    POINT c0;
    c0.x = 0;
    c0.y = 0;
    ::ClientToScreen(FormHwnd(), &c0);

    // ⚠️ 覆盖层是**设计器视图的 WS_CHILD**（ECOverlay::Create），所以 SetWindowPos 的坐标是
    //    **相对视图客户区**的，不是屏幕坐标 —— 曾经用屏幕坐标直接 SetWindowPos，覆盖层被摆到
    //    (765,454) 那种离谱位置，纸和点阵整块跑到窗体外面去了。
    POINT o0;
    o0.x = c0.x - EC_DESIGN_PAD;
    o0.y = c0.y - EC_DESIGN_PAD;
    ::ScreenToClient(GetSafeHwnd(), &o0);

    // 覆盖层原点 == 窗体客户区原点 - PAD，所以**窗体客户区坐标就是覆盖层坐标 - PAD**。
    // 之后所有"窗体客户区坐标"用 m_formOff 换算，它就是 (PAD,PAD)，与窗体边框/标题栏无关。
    m_formOff.x = EC_DESIGN_PAD;
    m_formOff.y = EC_DESIGN_PAD;

    // ⚠️ 覆盖层是弹出窗口，坐标是屏幕坐标；而且形状是 **Windows 窗口矩形 + 每边 PAD**
    //    （为了盖住窗体边框外那圈手柄；标题栏在客户区之上，覆盖层从客户区原点起铺，所以不会
    //      盖住它 —— 见上面那段说明）。客户区原点在窗口矩形内的偏移 = c0 - fr。
    m_overlay.SetWindowPos(&CWnd::wndTop, o0.x, o0.y,
                           (fr.right - fr.left) + 2 * EC_DESIGN_PAD,
                           (fr.bottom - fr.top) + 2 * EC_DESIGN_PAD,
                           SWP_NOACTIVATE | SWP_SHOWWINDOW);
    // ⚠️ 尺寸一变就**整层失效 + 立刻重画一次**。不这么做的话新露出来的那片区域是
    //    "上一次留下的旧像素"，而 WM_PAINT 只会重画**更新区**那一小块 ——
    //    实测症状就是"纸和点阵在启动时完全不出现，只有拖动过之后才有"。
    m_overlay.Invalidate(TRUE);
    m_overlay.UpdateWindow();
    // ★ 再排一次"迟到"的整层重画（见 ec_design.h 的 EC_WM_OVREDRAW 说明）。
    //   窗体窗口是**之后**才被建/显出来的，它的第一次 WM_PAINT 会把覆盖层刚画的像素盖掉；
    //   只在 SyncOverlay 里同步重画一次是不够的（实测启动时点阵仍然看不到）。
    m_overlay.StartRedrawBurst();

    {   // 诊断：`ECLONE_TRACE_DES=1` 时把几何倒到 trace
        char b[8];
        if (::GetEnvironmentVariableA("ECLONE_TRACE_DES", b, sizeof(b)) > 0)
        {
            RECT orr;
            ::GetWindowRect(m_overlay.GetSafeHwnd(), &orr);
            RECT oc;
            ::GetClientRect(m_overlay.GetSafeHwnd(), &oc);
            POINT o0;
            o0.x = 0; o0.y = 0;
            ::ClientToScreen(m_overlay.GetSafeHwnd(), &o0);
            ::EcTrace(_T("desync form=(%d,%d %dx%d) client0=(%d,%d) ovl_win=(%d,%d %dx%d) ")
                      _T("ovl_client0=(%d,%d) ovl_client=%dx%d formOff=(%d,%d) pad=%d"),
                      fr.left, fr.top, fr.right - fr.left, fr.bottom - fr.top,
                      c0.x, c0.y,
                      orr.left, orr.top, orr.right - orr.left, orr.bottom - orr.top,
                      o0.x, o0.y, oc.right, oc.bottom,
                      m_formOff.x, m_formOff.y, EC_DESIGN_PAD);
        }
    }
}

// ⚠️⚠️ 必须用 `Invalidate(TRUE)`（带 erase）。
//    覆盖层只画"纸 + 点阵 + 框 + 手柄"，而**纸和点阵下面还有一层底**：
//    真组件的窗口、以及覆盖层自己的背景刷（COLOR_BTNFACE = #F0F0F0）。
//    原来用 `Invalidate(FALSE)`（不擦背景）时，只有**更新区**那一小块会被重画，
//    其余部分永远是"上一次留下的像素"——实测症状就是：
//      * 点阵只在极小的更新区里出现（抓屏看到的洋红测试对角线只有 3x3 的一块块）
//      * 窗体被拖动/缩放后纸和点阵留在旧位置（用户截图里"背景乱了"）
//    带 erase 之后 Windows 会先用类背景刷擦一遍更新区，我们再整块重画，才对得上。
void CECDesigner::RedrawOverlay()
{
    if (m_overlay.GetSafeHwnd() != NULL)
    {
        EcOverlayRedrawGuard guard;      // 挡住钩子的自我递归（见 EcInOverlayRedraw 的说明）
        RememberOverlayState();          // 记下"这一次画的是哪个状态"，定时器据此判断要不要补画
        // ⚠️ 用"立刻重画"而不是只 Invalidate：钩子在别的窗口**刚画完**的那一刻被调，
        //    如果只排队一个 WM_PAINT，中间还可能被别的重画插进来（实测就是它导致
        //    "藏青手柄像素 0/85/200 不稳定"）。RDW_UPDATENOW 让它同步画完再返回。
        ::RedrawWindow(m_overlay.GetSafeHwnd(), NULL, NULL,
                       RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
    }
}

// 覆盖层"该画什么"的指纹：选中项、窗体几何、组件个数、当前窗体句柄。
// 定时器靠它判断"有没有变"，不变就不重画（常驻定时器才不会白烧 CPU）。
void CECDesigner::RememberOverlayState()
{
    m_ovState[0] = m_sel;
    m_ovState[1] = (int)(INT_PTR)FormHwnd();
    m_ovState[2] = (int)Units().GetSize();
    m_ovState[3] = m_formOff.x * 1000 + m_formOff.y;
}

BOOL CECDesigner::OverlayStateChanged() const
{
    if (m_ovState[0] != m_sel) return TRUE;
    if (m_ovState[1] != (int)(INT_PTR)FormHwnd()) return TRUE;
    if (m_ovState[2] != (int)Units().GetSize()) return TRUE;
    if (m_ovState[3] != m_formOff.x * 1000 + m_formOff.y) return TRUE;
    return FALSE;
}

// ------------------------------------------------------------------ 覆盖层转进来的鼠标/键盘
void CECDesigner::SurfaceDown(CPoint pt, BOOL /*bCtrl*/)
{
    // 诊断：ECLONE_TRACE_DES=1 + ECLONE_DUMP_MODEL=<path> 时记下每次按下（坐标是覆盖层坐标）。
    // 用途 = 判断"是没收到鼠标"还是"收到了但命中判定错了"（第 26 轮就是靠它抓到
    // `WS_EX_TRANSPARENT` 让真实点击穿过去了）。
    if (::GetEnvironmentVariable(_T("ECLONE_TRACE_DES"), NULL, 0) > 0)
        ::EcTrace(_T("down pt=(%d,%d) placing=%d sel=%d formOff=(%d,%d)"),
                  pt.x, pt.y, (int)IsPlacing(), m_sel, m_formOff.x, m_formOff.y);
    if (IsPlacing())
    {
        m_drag = TRUE;
        m_dragMode = 1;
        m_dragPt = pt;
        m_dragRect = CRect(pt, CSize(0, 0));
        RedrawOverlay();
        return;
    }
    // 先看是不是抓住了选中组件的缩放手柄（原版设计器就是这样缩放的）
    int dx = 0, dy = 0;
    if (HitHandle(pt, &dx, &dy))
    {
        m_drag = TRUE;
        m_dragMode = 3;
        m_rzDx = dx;
        m_rzDy = dy;
        m_dragPt = pt;                 // 缩放的位移基准点
        if (m_sel >= 0)
        {
            if (!UnitRect(m_sel, m_dragRect))
            {
                m_drag = FALSE;
                m_dragMode = 0;
            }
        }
        else
        {
            // 窗体本身：矩形就是客户区 (0,0,cx,cy)
            RECT wr;
            if (!::GetWindowRect(FormHwnd(), &wr))
            {
                m_drag = FALSE;
                m_dragMode = 0;
            }
            else
            {
                m_dragRect.SetRect(0, 0, wr.right - wr.left, wr.bottom - wr.top);
            }
        }
        return;
    }
    int hit = HitTest(pt);
    Select(hit);
    if (hit >= 0)
    {
        m_drag = TRUE;
        m_dragMode = 2;
        m_dragPt = pt;
    }
}

void CECDesigner::SurfaceMove(CPoint pt)
{
    if (!m_drag)
        return;
    if (m_dragMode == 1)
    {
        m_dragRect = CRect(m_dragPt, pt);
        RedrawOverlay();
    }
    else if (m_dragMode == 2 && m_sel >= 0)
    {
        RECT rc;
        if (!::GetWindowRect(Units()[m_sel].hwnd, &rc))
            return;
        POINT p;
        p.x = rc.left;
        p.y = rc.top;
        ::ScreenToClient(FormHwnd(), &p);
        ::SetWindowPos(Units()[m_sel].hwnd, NULL,
                       p.x + (pt.x - m_dragPt.x), p.y + (pt.y - m_dragPt.y), 0, 0,
                       SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        m_dragPt = pt;
        RedrawOverlay();
    }
    else if (m_dragMode == 3)
    {
        // 缩放：按**鼠标位移**拖动被抓住的那条边（手柄画在框外 3px，用绝对坐标会让边跳 3px）。
        // 另一条边不动；组件最小 8x8，窗体最小 40x30。
        CRect r = m_dragRect;
        int dx = pt.x - m_dragPt.x;
        int dy = pt.y - m_dragPt.y;
        if (m_rzDx < 0)
            r.left += dx;
        else if (m_rzDx > 0)
            r.right += dx;
        if (m_rzDy < 0)
            r.top += dy;
        else if (m_rzDy > 0)
            r.bottom += dy;
        int nMinW = (m_sel >= 0) ? 8 : 40;
        int nMinH = (m_sel >= 0) ? 8 : 30;
        if (r.Width() < nMinW)
        {
            if (m_rzDx < 0) r.left = r.right - nMinW;
            else if (m_rzDx > 0) r.right = r.left + nMinW;
        }
        if (r.Height() < nMinH)
        {
            if (m_rzDy < 0) r.top = r.bottom - nMinH;
            else if (m_rzDy > 0) r.bottom = r.top + nMinH;
        }

        if (m_sel >= 0)
        {
            ::SetWindowPos(Units()[m_sel].hwnd, NULL, r.left, r.top, r.Width(), r.Height(),
                           SWP_NOZORDER | SWP_NOACTIVATE);
        }
        else
        {
            // ★ 窗体：**只改尺寸，不动位置** —— 用户要求"基准窗口不能拖动，只能调整大小"，
            //   原版也是这个行为（shots\live_des_orig\windows.txt 里窗体一直在视图的 (10,10)）。
            //   抓住的若是左/上边，位置照样钉死在 (10,10)，只把差值折进尺寸。
            int nW = r.Width(), nH = r.Height();
            ::SetWindowPos(FormHwnd(), NULL, EC_DESIGN_FORM_X, EC_DESIGN_FORM_Y, nW, nH,
                           SWP_NOZORDER | SWP_NOACTIVATE);
            if (m_curForm >= 0 && m_curForm < (int)m_forms.GetSize() && m_forms[m_curForm] != NULL)
            {
                m_forms[m_curForm]->x = EC_DESIGN_FORM_X;
                m_forms[m_curForm]->y = EC_DESIGN_FORM_Y;
                m_forms[m_curForm]->cx = nW;
                m_forms[m_curForm]->cy = nH;
            }
            SyncOverlay();                 // 窗体尺寸变了 -> 覆盖层跟着重新铺
        }
        RedrawOverlay();
    }
}

void CECDesigner::SurfaceUp(CPoint /*pt*/)
{
    if (m_drag && m_dragMode == 1)
    {
        CRect r = m_dragRect;
        r.NormalizeRect();
        PlaceAt(r);
    }
    else if (m_drag && m_dragMode == 3 && m_sel >= 0)
    {
        // 把新几何**同步回支持库的属性**（和放置时一样走 ITF 4）。
        // ⚠️ 原版属性表里改"宽度/高度"不一定会动窗口（见 HANDOFF 2.6 那条"序号对不上"），
        //    所以窗口位置由这里的 SetWindowPos 负责，属性只是记录下来。
        CRect r;
        if (UnitRect(m_sel, r))
            EcInitUnitGeometry(Units()[m_sel].lib, Units()[m_sel].dt, Units()[m_sel].hunit,
                               r.left, r.top, r.Width(), r.Height());
    }
    m_drag = FALSE;
    m_dragMode = 0;
    RedrawOverlay();
}

void CECDesigner::SurfaceKey(UINT nChar)
{
    if (nChar == VK_DELETE)
        DeleteSelected();
    else if (nChar == VK_ESCAPE)
        CancelPlace();
    else if (nChar == VK_TAB)
    {
        // 「切换窗体」：设计器里 Ctrl+Tab 在所有窗体之间轮流切（原版是每个窗体一个文档页；
        // 本工程所有窗体共用一个设计器页，所以用这个键来在它们之间翻）。
        if (m_forms.GetSize() > 1)
            SelectForm((m_curForm + 1) % (int)m_forms.GetSize());
    }
}

void CECDesigner::SurfacePaint(CDC* pDC)
{
    // ★ 覆盖层画三样：**纸 + 8px 点阵**、拖出的新组件矩形、选中框 + 8 个手柄。
    //   ⚠️ 纸/点阵**必须**在这里画（覆盖层是同级的兄弟窗口里最上面那个）：
    //   设计视图是窗体的**父窗口**，父窗口的绘制会被子窗口（真窗体）盖住 —— 实测把纸/点阵
    //   挪到视图的 OnPaint 里画，屏幕上一点都看不到（PrintWindow 能拍到、抓屏拍不到）。
    //
    // ---- 1) 纸 + 点阵 -----------------------------------------------------------
    CRect rcAll;
    m_overlay.GetClientRect(&rcAll);
    CRect rcPaper;
    {
        RECT cr;
        ::GetClientRect(FormHwnd(), &cr);
        rcPaper.SetRect(m_formOff.x, m_formOff.y,
                        m_formOff.x + (cr.right - cr.left), m_formOff.y + (cr.bottom - cr.top));
        if (rcPaper.right > rcAll.right) rcPaper.right = rcAll.right;
        if (rcPaper.bottom > rcAll.bottom) rcPaper.bottom = rcAll.bottom;
    }
    int nSaved = pDC->SaveDC();
    int i;
    for (i = 0; i < (int)Units().GetSize(); i++)
    {
        RECT wr;
        if (!::GetWindowRect(Units()[i].hwnd, &wr))
            continue;
        POINT q;
        q.x = wr.left;
        q.y = wr.top;
        ::ScreenToClient(FormHwnd(), &q);
        // 组件占的格子留出来（覆盖层在它们上面，不排除会把真组件盖住）
        pDC->ExcludeClipRect(q.x + m_formOff.x, q.y + m_formOff.y,
                             q.x + m_formOff.x + (wr.right - wr.left),
                             q.y + m_formOff.y + (wr.bottom - wr.top));
    }
    pDC->FillSolidRect(rcPaper, EC_DESIGN_PAPER);
    int gx = g_ecCfg.nGridW, gy = g_ecCfg.nGridH;
    if (gx < 2) gx = 2;
    if (gy < 2) gy = 2;
    if (g_ecCfg.bShowGrid)
    {
        int x, y;
        for (y = m_formOff.y; y < rcPaper.bottom; y += gy)
            for (x = m_formOff.x; x < rcPaper.right; x += gx)
                pDC->FillSolidRect(x, y, 1, 1, RGB(0, 0, 0));
    }
    pDC->RestoreDC(nSaved);
    if (::GetEnvironmentVariableA("ECLONE_TRACE_DES", NULL, 0) > 0)
        ::EcTrace(_T("overlaypaper=(%d,%d)-(%d,%d) formOff=(%d,%d) grid=%d"),
                  rcPaper.left, rcPaper.top, rcPaper.right, rcPaper.bottom,
                  m_formOff.x, m_formOff.y, (int)g_ecCfg.bShowGrid);

    // ---- 2) 拖出的新组件矩形 ---------------------------------------------------
    if (m_drag && m_dragMode == 1)
    {
        CPen pen(PS_DOT, 1, RGB(0, 0, 0));
        CPen* pOldPen = pDC->SelectObject(&pen);
        int oldBk = pDC->SetBkMode(TRANSPARENT);
        CRect r = m_dragRect;
        r.NormalizeRect();
        pDC->MoveTo(r.left, r.top);
        pDC->LineTo(r.right, r.top);
        pDC->LineTo(r.right, r.bottom);
        pDC->LineTo(r.left, r.bottom);
        pDC->LineTo(r.left, r.top);
        pDC->SelectObject(pOldPen);
        pDC->SetBkMode(oldBk);
    }

    // ---- 2) 选中组件的框 + 8 个手柄 --------------------------------------------
    if (m_sel >= 0 && m_sel < (int)Units().GetSize())
    {
        CRect b;
        if (UnitRectInOverlay(m_sel, b))
            EcDrawUnitFrame(pDC, b);        // 与窗体同一种画法（原版实测）
    }
}

// ------------------------------------------------------------------ 视图自身
BOOL CECDesigner::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CECDesigner::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);

    // 工作区（原版实测 #A0A0A0）
    dc.FillSolidRect(rc, EC_DESIGN_CANVAS);
    // ★ 设计期那块"纸 + 8px 点阵"必须**画在视图上**（视图是设计窗体的父窗口，且带
    //   WS_CLIPCHILDREN），这样它天然在窗体**下面**。
    //   ⚠️ 别再改回"由覆盖层画"：覆盖层和窗体的**窗口矩形**大量重叠，屏幕上是窗体盖住覆盖层，
    //   覆盖层的纸/点阵根本不可见（PrintWindow 能拍到、抓屏拍不到，实测抓屏只剩 172 px 的
    //   洋红测试线）。覆盖层只负责"窗体外面那圈框和 8 个手柄"。
    DrawPaperOnView(&dc);
    if (m_sel < 0)
        DrawFormFrame(&dc);      // 没选组件 = 选中窗体本身（原版开窗就是这个样子）
}

// 把"纸 + 点阵"画在**设计器视图**上（坐标 = 视图客户区坐标）。
// 纸的范围 = 窗体客户区；点阵锚在窗体客户区原点、间距取「系统配置->显示」页的网点间距。
void CECDesigner::DrawPaperOnView(CDC* pDC)
{
    if (FormHwnd() == NULL)
        return;
    RECT fr, cr;
    if (!::GetWindowRect(FormHwnd(), &fr))
        return;
    ::GetClientRect(FormHwnd(), &cr);
    POINT p;
    p.x = fr.left;
    p.y = fr.top;
    ::ScreenToClient(GetSafeHwnd(), &p);          // 窗体的**窗口矩形**在视图客户区里的位置
    CRect rcPaper(p.x, p.y, p.x + (fr.right - fr.left), p.y + (fr.bottom - fr.top));

    // 把窗体的子组件（真组件窗口）排除掉，别把纸色糊到它们上面
    int nSaved = pDC->SaveDC();
    int i;
    for (i = 0; i < (int)Units().GetSize(); i++)
    {
        RECT wr;
        if (!::GetWindowRect(Units()[i].hwnd, &wr))
            continue;
        POINT q;
        q.x = wr.left;
        q.y = wr.top;
        ::ScreenToClient(GetSafeHwnd(), &q);
        pDC->ExcludeClipRect(q.x, q.y, q.x + (wr.right - wr.left), q.y + (wr.bottom - wr.top));
    }
    pDC->FillSolidRect(rcPaper, EC_DESIGN_PAPER);
    int gx = g_ecCfg.nGridW, gy = g_ecCfg.nGridH;
    if (gx < 2) gx = 2;
    if (gy < 2) gy = 2;
    if (g_ecCfg.bShowGrid)
    {
        // 点阵从窗体**客户区原点**起铺（= 窗口矩形 + 边框/标题栏偏移），到客户区右下角为止
        POINT c0;
        c0.x = 0;
        c0.y = 0;
        ::ClientToScreen(FormHwnd(), &c0);
        ::ScreenToClient(GetSafeHwnd(), &c0);
        int x, y;
        for (y = c0.y; y < rcPaper.bottom; y += gy)
            for (x = c0.x; x < rcPaper.right; x += gx)
                pDC->FillSolidRect(x, y, 1, 1, RGB(0, 0, 0));
    }
    pDC->RestoreDC(nSaved);

    if (::GetEnvironmentVariableA("ECLONE_TRACE_DES", NULL, 0) > 0)
        ::EcTrace(_T("viewpaper=(%d,%d)-(%d,%d) form=%d,%d %dx%d grid=%d"),
                  rcPaper.left, rcPaper.top, rcPaper.right, rcPaper.bottom,
                  p.x, p.y, fr.right - fr.left, fr.bottom - fr.top,
                  (int)g_ecCfg.bShowGrid);
}

void CECDesigner::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    SyncOverlay();
}

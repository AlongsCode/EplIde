// ec_editor.cpp -- 易语言风格「结构化」代码编辑器的实现（全部自绘）
//
// 原版实测（详见 HANDOFF.md 第 8 节）：
//   编辑器窗口里没有任何子控件 —— 表格、流程槽、流程线、光标全是同一个窗口画的。
//   本文件按实测的像素规则重画：列宽 = 列内文字最大宽度 + 12；表格行距 21px
//   （1px 网格 + 20px 单元格）；代码行 18px；流程槽 20px 灰底 #ECECEC；
//   正文区左边界在 x=44，有一条全高的 #888888 竖线。
#include "ec_editor.h"
#include "ec_lib.h"
#include "ec_theme.h"
#include <string.h>
#include <stdarg.h>

// 诊断追踪（定义在文件下半部分，这里先声明）：块变换命令用它区分
// 「命令根本没执行」和「执行了但结果没进显示模型」。
void EcTrace(LPCTSTR pszFmt, ...);

// ------------------------------------------------------------------ 度量
#define EC_FONT_H        12
#define EC_TOP_PAD        4
#define EC_GAP_SMALL      4
#define EC_GAP_BIG        8
#define EC_GUTTER_W      20
#define EC_ID_CELLEDIT   0xE001   // 就地编辑那个 EDIT 子控件的 ID
#define EC_TEXT_X        44
#define EC_CELL_PAD       6
#define EC_CODE_PAD       7
#define EC_INDENT_W      22
// 原版实测（本轮用「同一份内容」对拍定死的：shots\live_tabs\view0.png = 原版打开
// graphic.e 的 1:1 编辑器截图，1174x388；本工程用 tools\patch_sample_graphic.py 喂同一段源码）：
//   缩进级别 n 的正文左边缘 x = 46 + 22n   -> 级别 0/1/2 = 46 / 68 / 90
//       证据：原版那句 `信息框（…）` 的深红字从 x=46 起；精易模块里嵌在「如果真」里的
//       那一句从 x=68 起（= 46+22）
//   级别 n 的流程竖虚线 x = 24 + 22n      -> 24 / 46 / 68（精易模块实测那条灰虚线在 47）
//   子程序第一条语句：流程槽里一个 ↓（竖线 x=9、7px + 底下一个 7x4 实心三角，藏青）
//   ⚠️ 别再按 EC_TEXT_X(44)+7 当基准（那是上一轮从另一张截图推的，深了一格）
#define EC_TEXT_BASE     46
#define EC_TEXT_X_OF(n)  (EC_TEXT_BASE + (n) * EC_INDENT_W)
#define EC_FLOW_X(n)     (EC_TEXT_BASE - EC_INDENT_W + (n) * EC_INDENT_W)

// ------------------------------------------------------------------ 颜色
// 全部改成从全局主题取（配色的唯一来源是 ec_theme.cpp 的 g_ecTheme）：
// 默认值 = 原版「默认配色」实测值，用户可以在「工具->系统配置->程序显示->
// 颜色配置」里换成 e\clr\*.clr。宏名保持不变，绘制代码一个字都不用改。
#define EC_CS(s)  (g_ecTheme.At(EC_CS_##s))
#define EC_C_TEXT      EC_CS(FG)
#define EC_C_KEY       EC_CS(FLOW_CMD_FG)   // 流程关键字 / 数据类型
#define EC_C_CMD       EC_CS(MEMBER_FG)     // 支持库命令 / ✔
#define EC_C_STR       EC_CS(STRING_FG)     // 字符串
#define EC_C_CMT       EC_CS(COMMENT_FG)    // 注释 / 备注
#define EC_C_PUNCT     EC_CS(OPER_FG)       // 括号运算符 / 表格里的名称
#define EC_C_GRID      EC_CS(GRID)
#define EC_C_FILL_SUB  EC_CS(SUB_HEADER_BG) // 子程序/参数表头
#define EC_C_FILL_VAR  EC_CS(VAR_HEADER_BG) // 变量表表头
#define EC_C_FILL_ASM  EC_CS(ASM_HEADER_BG) // 程序集表头
#define EC_C_FLOW      EC_CS(LINK)          // 流程控制线
#define EC_C_GUTTER    EC_CS(FLAGCOL_BG)
#define EC_C_SELBG     EC_CS(MARK)
#define EC_C_PNAME     EC_CS(PARAM_NAME_FG)  // 参数归属线里的「参数名：」
#define EC_C_NUM       EC_CS(NUMBER_FG)      // 数值
// 选区：底色 = 「选中标记」槽，文字 = 白（原版实测 —— 在它的代码编辑器里拖选一段，
// 选中的字就是 #000080 底 + 白字，选区高度 = 文字那 12px，见 shots\live_drag\after.png）
#define EC_C_SELBG     EC_CS(MARK)
#define EC_C_SELFG     RGB(255, 255, 255)

// 表格线样式：原版「程序显示」页的「表格线画为虚线」(id=129) 控制
// 下面三个度量本来是编译期常量（EC_CELL_H/EC_ROW_PITCH/EC_CODE_H），
// 现在必须跟着「语句行间距」和字号走，所以改成运行时函数。
static int EcCellH()    { return 20; }                          // 单元格像素高
static int EcCodeH()                                            // 代码行距
{
    int h = g_ecTheme.opt.nLinePitch;
    // 原版实测代码行距 = 22px（shots\live2\view0.png 里同一列的字形顶逐行 +22）
    return (h >= 8 && h <= 64) ? h : 22;
}
static int EcRowPitch() { return EcCellH() + 1; }               // 1px 线 + 单元格

#define EC_CELL_H   EcCellH()
#define EC_ROW_PITCH EcRowPitch()
#define EC_CODE_H   EcCodeH()
static int EcFlowStyle()
{
    return g_ecTheme.opt.bFlowDot ? PS_DOT : PS_SOLID;
}

// 画 1px 流程虚线。原版实测是「3 亮 3 灭」（shots\live_fold_jy\view_before.png 的
// x=47 上 y=305,306,307 亮 / 308,309,310 灭 / 311,312,313 亮 …），
// 比 GDI 的 PS_DOT 更规则，所以自己铺段；不勾「分支流程线画为虚线」时走实线。
// ⚠️ LineTo 不含终点，所以「3 亮」要 LineTo(y+3)：写 y+2 只会亮 2px（本轮踩过）。
static void EcFlowLine(CDC& dc, int x0, int y0, int x1, int y1)
{
    if (!g_ecTheme.opt.bFlowDot)
    {
        dc.MoveTo(x0, y0);
        dc.LineTo(x1, y1);
        return;
    }
    if (x0 == x1)
    {
        int y = (y0 < y1 ? y0 : y1), ye = (y0 < y1 ? y1 : y0);
        for (; y <= ye; y += 6)
        {
            dc.MoveTo(x0, y);
            dc.LineTo(x0, (y + 3 <= ye) ? y + 3 : ye + 1);
        }
    }
    else
    {
        int x = (x0 < x1 ? x0 : x1), xe = (x0 < x1 ? x1 : x0);
        for (; x <= xe; x += 6)
        {
            dc.MoveTo(x, y0);
            dc.LineTo((x + 3 <= xe) ? x + 3 : xe + 1, y0);
        }
    }
}

// 实心等腰三角形（原版：底 7px 宽、高 4px）
static void EcTri(CDC& dc, int xc, int yTop, int hw, int h, COLORREF cr)
{
    CPoint p[3];
    p[0] = CPoint(xc, yTop);
    p[1] = CPoint(xc - hw, yTop + h);
    p[2] = CPoint(xc + hw, yTop + h);
    CBrush br(cr);
    CBrush* pb = dc.SelectObject(&br);
    CPen pen(PS_SOLID, 1, cr);
    CPen* pp = dc.SelectObject(&pen);
    dc.Polygon(p, 3);
    dc.SelectObject(pb);
    dc.SelectObject(pp);
}

// 画 1px 表格线：原版「表格线画为虚线」(id=129) 选中时用 PS_DOT，否则实心填充
static void EcHLine(CDC& dc, int x, int y, int w)
{
    if (w <= 0) return;
    if (g_ecTheme.opt.bGridDot)
    {
        CPen pen(PS_DOT, 1, EC_C_GRID);
        CPen* pOld = dc.SelectObject(&pen);
        dc.MoveTo(x, y); dc.LineTo(x + w, y);
        dc.SelectObject(pOld);
    }
    else
    {
        dc.FillSolidRect(x, y, w, 1, EC_C_GRID);
    }
}

static void EcVLine(CDC& dc, int x, int y, int h)
{
    if (h <= 0) return;
    if (g_ecTheme.opt.bGridDot)
    {
        CPen pen(PS_DOT, 1, EC_C_GRID);
        CPen* pOld = dc.SelectObject(&pen);
        dc.MoveTo(x, y); dc.LineTo(x, y + h);
        dc.SelectObject(pOld);
    }
    else
    {
        dc.FillSolidRect(x, y, 1, h, EC_C_GRID);
    }
}

// 表格类别
#define ECT_ASM   0
#define ECT_SUB   1
#define ECT_VAR   2
#define ECT_DLL   3     // Dll命令块（原版实测：块与块之间也是 8px 空隙）
#define ECT_BLK   4     // 全局变量/常量/资源/数据类型**块**：块间 8px 空隙（实测）
#define ECT_MEM   5     // 自定义数据类型的成员子表：紧贴上一张表，不加空隙（实测）

// ------------------------------------------------------------------ 两个自绘小位图
// 都是从原版 1:1 截图里逐像素抠出来的（tools\gen_masks.py 生成，别手改）：
//   g_szTick —— 表格里的「✔」。原版不是一条 1px 折线，而是一个 9x7 的实心勾，
//               长笔画 3px 宽、短笔画 2px 宽；画在「单元格水平居中、行顶 +7」处。
//   g_szPen  —— 文档刚打开时流程槽里的「红色铅笔」（14x14，三色）。
static const char* g_szTick[7] = {
    ".......##",
    "......###",
    "##...###.",
    "###.###..",
    ".#####...",
    "..###....",
    "...#.....",
};
#define EC_TICK_W 9
#define EC_TICK_H 7

static const char* g_szPen[14] = {
    "01110000000000",
    "12211000000000",
    "12321100000000",
    "01232110000000",
    "00123211000000",
    "00012311000000",
    "00001122100000",
    "00000123210000",
    "00000012321000",
    "00000001232100",
    "00000000123210",
    "00000000012320",
    "00000000001111",
    "00000000000001",
};
#define EC_PEN_W 14
#define EC_PEN_H 14

// 流程槽标记
#define ECM_NONE  0
#define ECM_DOWN  1     // ↓ 子程序第一条语句
#define ECM_FOLD  2     // -/+ 折叠

// ------------------------------------------------------------------ 小工具
static CString EcTrim(const CString& s)
{
    int a = 0, b = s.GetLength();
    while (a < b && ((unsigned char)s[a] == ' ' || (unsigned char)s[a] == '\t')) a++;
    while (b > a && ((unsigned char)s[b - 1] == ' ' || (unsigned char)s[b - 1] == '\t' ||
                     (unsigned char)s[b - 1] == '\r' || (unsigned char)s[b - 1] == '\n')) b--;
    return s.Mid(a, b - a);
}

// 取开头那个「词」（到空格/逗号为止）—— 用于识别 .子程序 / .参数 这类声明
// GBK 后继字节（0x40..0xFE 去掉 0x7F）
static BOOL EcGbTail(unsigned char d) { return (d >= 0x40 && d <= 0xFE && d != 0x7F); }

// 取行首的「声明头」：到空格/制表/半角逗号/全角标点为止。
// ⚠️ 必须按 GBK **整字**推进：早先这里写的是「见到字节 0xA1/0xA3 就断」，而 0xA1/0xA3
//    也会作为**后继字节**出现在普通汉字里（常 = B3 A3、一 = D2 BB…），于是
//    `.常量` 的头被切在 常 的中间、认不出声明（现象：常量表页整页变成代码行）。
//    正解：0xA1/0xA3 只有当它**引导**一个双字节字时才算标点。
static CString EcHeadWord(const CString& s)
{
    int i = 0, n = s.GetLength();
    while (i < n)
    {
        unsigned char c = (unsigned char)s[i];
        if (c == ' ' || c == '\t' || c == ',') break;
        if (i + 1 < n && c >= 0x81 && EcGbTail((unsigned char)s[i + 1]))
        {
            if (c == 0xA1 || c == 0xA3) break;      // 全角标点区 = 分隔符
            i += 2; continue;
        }
        i++;
    }
    return s.Left(i);
}

// 去掉文本形式里字符串字段的英文双引号（Dll命令的库文件名 / 在库中对应命令名）
static CString EcUnquote(const CString& s)
{
    CString t = EcTrim(s);
    if (t.GetLength() >= 2 && t[0] == _T('"') && t[t.GetLength() - 1] == _T('"'))
        t = t.Mid(1, t.GetLength() - 2);
    return t;
}

// 代码行第一个标识符（跳过空格）
static CString EcFirstIdent(const CString& s)
{
    int i = 0, n = s.GetLength();
    while (i < n && (unsigned char)s[i] == ' ') i++;
    int a = i;
    while (i < n)
    {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
            (c >= '0' && c <= '9')) i++;
        else if (c >= 0x81 && i + 1 < n) i += 2;
        else break;
    }
    return s.Mid(a, i - a);
}

static const TCHAR* g_szBlockStart[] = {
    _T("如果"), _T("如果真"), _T("判断"), _T("判断循环首"), _T("循环判断首"),
    _T("计次循环首"), _T("变量循环首"), NULL
};
static const TCHAR* g_szBlockEnd[] = {
    _T("如果结束"), _T("如果真结束"), _T("判断结束"), _T("判断循环尾"),
    _T("循环判断尾"), _T("计次循环尾"), _T("变量循环尾"), NULL
};
// 原版**不画**的行：如果/判断家族的块尾 + 分支关键字（否则/默认）。
// 实测（本轮重新拍的 1:1 基准）：
//   · shots\live_flowcmd\view0.png（原版自带例程「流程控制类命令演示」，1:1）：
//     `.如果真 (变量1 ＞ 3)` 的体后面**直接**是下一条语句 —— `如果真结束` 不画；
//     但 `.计次循环尾 ()` / `.变量循环尾 ()` **照画**（它在源码里就是一条独立语句，
//     行号和普通语句一样占一行）。
//   · shots\live_learn2\view0.png（一步一步跟我学写易程序二）：`如果` / `如果真` 都
//     没有对应的「结束」行，也没有 `否则` 行 —— 分支用流程槽里的 ▼/► 表示。
// ⚠️ 第 15 轮把 4 个「循环尾」也塞进这张表是**错的**（用户 2026-10 的截图 b/c 里
//    `计次循环尾 ()` `循环判断尾 ()` 明明都在）。去掉它们之后，流程线的块尾配对才
//    对得上：循环块的竖线收在可见的「循环尾」行上，如果家族的竖线延一格用 ▼ 收尾。
static const TCHAR* g_szHidden[] = {
    _T("如果结束"), _T("如果真结束"), _T("判断结束"), _T("否则"), _T("默认"), NULL
};
static const TCHAR* g_szKeywords[] = {
    _T("如果"), _T("如果真"), _T("如果结束"), _T("如果真结束"), _T("否则"),
    _T("判断"), _T("判断结束"), _T("判断循环首"), _T("判断循环尾"),
    _T("循环判断首"), _T("循环判断尾"), _T("计次循环首"), _T("计次循环尾"),
    _T("变量循环首"), _T("变量循环尾"), _T("返回"), _T("跳出循环"), _T("到循环尾"),
    _T("结束"), _T("真"), _T("假"), _T("空"), _T("默认"), NULL
};

static BOOL EcInList(const TCHAR* const* list, const CString& s)
{
    for (int i = 0; list[i]; i++)
        if (s == list[i]) return TRUE;
    return FALSE;
}

// 从 s[a] 起吃掉一个「名字」：ASCII 字母/数字/下划线 + GBK 汉字（前导 0x81..0xFE、
// 次字节 0x40..0xFE）。
// ⚠️ 必须把全角标点的次字节排除掉（tools\_enc_probe.py 量过 graphic.e）：
//    全角左括号 a3a8 / 右括号 a3a9 / 逗号 a3ac / 引号 a1b0,a1b1 / 冒号 a3ba。
//    只写 `c >= 0x81 -> i += 2` 会把「信息框（“易语言…」整段当成名字，
//    于是 EcNameKind 认不出、EcCmdArgsByName 也查不到（这个坑真踩过：参数行一行都不画）。
//    EcGbTail 在文件上面（EcHeadWord 也要用同一份判据）。
static int EcIdentHead(const CString& s, int a)
{
    int n = s.GetLength(), i = a;
    while (i < n)
    {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
            (c >= '0' && c <= '9')) { i++; continue; }
        if (i + 1 >= n) break;
        unsigned char d = (unsigned char)s[i + 1];
        if (c < 0x81 || !EcGbTail(d)) break;
        // 全角标点区：GBK 里 0xA1/0xA3 区的次字节 0x01..0x7E 是标点（a3a8 左括号、
        // a3a9 右括号、a3ac 逗号、a1b0/a1b1 引号）。这两区一律不算名字的一部分。
        if (c != 0xA1 && c != 0xA3) { i += 2; continue; }
        break;
    }
    return i;
}

// ------------------------------------------------------------------ 参数归属线
// 原版把「一条支持库命令调用的参数」在语句下面逐行展开成「※参数名：值」。
// 几何全部是对着原始截图逐像素量的（shots\live_fold\view_after.png = 原版打开
// graphic.e、且点了「信息框（…）」那一行的 + 之后的 1:1 客户区）：
//   · 语句那句 `信息框（…）`              正文 x=46，行 y=101..112
//   · 第一行参数 `※提示信息： “易语言中国人之编程！”` 的「※」左边缘 x=91、行 y=117..128
//     （= 语句正文列 46 + 44，即缩进 2 级；提示信息/按钮/窗口标题三行逐项量过，列一致）
//   · 行距 16px（三行行顶 117 / 133 / 149）
//   · 左侧连接线：竖虚线 x=77（= 语句正文列 46 + 31），**3 亮 3 灭**（亮 y=121..123、
//     灭 124..126、亮 127..129…），颜色 #808080（= EC_CS_LINK）；
//     每行行顶往下第 6 行起还有一条 3px 横虚线（y=123/139/155 的 x=77..79），
//     再接第 3 行的 2px（同一条 3 亮 3 灭被 ※ 的字形遮掉一列，只画 77..79 + 83..84 够用）
//   · 「※」= 藏青 #000080（= EC_CS_OPER_FG，和括号运算符同色）
//   · 参数名 + 冒号 = 黑色（= EC_CS_PARAM_NAME_FG）
//   · 冒号右边一个空格，再画参数值本身（沿用代码行的词法着色：字符串 #008080、数值…）
//   · 值后面还有两个空格（原版量到 `按钮：0` 的行在 x=152..153 是那个 0，之后是空白）
// 可见性：原版是**全局开关**（用户给的 shots\user_ref.png 里连续的「如果真」「计次循环首」
// 每一句下面都展开了参数行）——所以用 CECEditor::m_bParamShow 一个标志统管，
// 点任意一句的 -/+ 就整篇切换。
// 默认**收起**：1:1 基准 shots\live_tabs\view0.png（原版打开 graphic.e）里
// `信息框（…）` 那一行的流程槽是 9x9「+」、且下面一行参数都没有 —— 所以初值 FALSE。
struct EcCallParams
{
    CString              cmd;      // 命令名
    std::vector<CString> vals;     // 各参数的实际文本（已 trim，可能为空）
    BOOL                 bLastSep; // 最后一个参数后面还有分隔符（`(a, b, )` 这种写法）
    EcCallParams() : bLastSep(FALSE) {}
};

// 在代码行里找「命令名（ 实参, 实参, … ）」，返回括号内按顶层逗号切开的实参。
// ⚠️ 命令名必须**紧跟**全角左括号（a3a8）才是调用；命令名后面有空格的是流程语句
//    （`返回 （x）`、`计次循环首 (10, i)`），原版不给它们画参数归属线。
// ⚠️ 易语言的括号是**全角**的：a3a8 = 左括号 / a3a9 = 右括号 / a3ac = 逗号。
//    只认半角会一个参数都切不出来（tools\_enc_probe.py 量过 graphic.e 那句）。
// ⚠️⚠️ 行注释里别写全角方括号那类字符：转成 GBK 后字节里可能夹着块注释的结束符号，
//    会把文件后面某个块注释提前关掉，于是它下面第一条语句被注释掉、编译还不报错
//    （真踩过：`s[i] != 0xA3` 那句被吃掉，参数行一行都不画）。
//    构建后必须跑 tools\check_block_comments.py gbk 兜底。
// 返回 0 = 这一行不是一条带参的支持库命令调用。
static BOOL EcParseCall(const CString& s, EcCallParams& out)
{
    int i = 0, n = s.GetLength();
    while (i < n && (unsigned char)s[i] == ' ') i++;
    int a = i;
    i = EcIdentHead(s, i);
    if (i == a) return FALSE;
    out.cmd = s.Mid(a, i - a);
    if (EcNameKind(out.cmd) != 1) return FALSE;      // 只认支持库命令
    if (i + 1 >= n) return FALSE;                    // 命令名后面什么都不剩
    // 命令名必须**紧接**左括号才算调用：`返回 （x）` 这种流程语句命令名后面有空格，
    // 原版不给它画参数归属线；而 `信息框（…）` 是紧挨着的。
    if ((unsigned char)s[i] != 0xA3 || (unsigned char)s[i + 1] != 0xA8) return FALSE;
    i += 2;

    int depth = 0, seg = i, k;
    BOOL bTrail = FALSE;
    for (k = i; k < n; k++)
    {
        unsigned char c = (unsigned char)s[k];
        if (c >= 0x81)
        {
            if (c == 0xA3 && k + 1 < n)
            {
                unsigned char d = (unsigned char)s[k + 1];
                if (d == 0xA8) { depth++; k++; continue; }          // （
                if (d == 0xA9) { if (depth == 0) break; depth--; k++; continue; }  // ）
                if (d == 0xAC && depth == 0)                        // ，
                {
                    out.vals.push_back(EcTrim(s.Mid(seg, k - seg)));
                    bTrail = TRUE;
                    k++;
                    seg = k + 1;
                    continue;
                }
            }
            k++; continue;
        }
        if (c == '(') depth++;
        else if (c == ')') { if (depth == 0) break; depth--; }
        else if (c == ',' && depth == 0)
        {
            out.vals.push_back(EcTrim(s.Mid(seg, k - seg)));
            bTrail = TRUE;
            seg = k + 1;
        }
    }
    if (k >= n) return FALSE;                        // 没闭合
    {
        CString sLast = EcTrim(s.Mid(seg, k - seg));
        if (!sLast.IsEmpty()) bTrail = FALSE;
        out.vals.push_back(sLast);
    }
    out.bLastSep = bTrail;
    return TRUE;
}
// 按 , 和 ，切分。pSep 不为空时，把**原样的分隔符**也带出来 —— 备注字段要原样重拼，
// 原版是把二进制里存的字符串直接画出来（用户写了全角逗号就显示全角）。
static void EcSplit(const CString& s, std::vector<CString>& out, std::vector<CString>* pSep = NULL)
{
    out.clear();
    if (pSep != NULL) pSep->clear();
    int a = 0, i = 0, n = s.GetLength();
    BOOL bInStr = FALSE;            // 英文双引号里的逗号不是分隔符（长文本常量/备注里常见）
    while (i < n)
    {
        unsigned char c = (unsigned char)s[i];
        BOOL bSep = (c == ',');
        int nSepLen = 1;
        if (c == 0xA3 && i + 1 < n && (unsigned char)s[i + 1] == 0xAC) { bSep = TRUE; nSepLen = 2; }
        if (c == '"' && i + 1 < n && (unsigned char)s[i + 1] == '"') { i += 2; continue; }  // 转义引号
        if (bSep && !bInStr)
        {
            out.push_back(EcTrim(s.Mid(a, i - a)));
            if (pSep != NULL) pSep->push_back(s.Mid(i, nSepLen));
            i += nSepLen;
            a = i;
        }
        else
        {
            if (c == '"') bInStr = !bInStr;
            i += (c >= 0x81 && i + 1 < n) ? 2 : 1;
        }
    }
    out.push_back(EcTrim(s.Mid(a, n - a)));
}

// ------------------------------------------------------------------ 编辑器
IMPLEMENT_DYNAMIC(CECEditor, CWnd)

BEGIN_MESSAGE_MAP(CECEditor, CWnd)
    ON_WM_CREATE()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_CONTEXTMENU()
    ON_WM_MOUSEWHEEL()
    ON_WM_VSCROLL()
    ON_WM_KEYDOWN()
    ON_WM_CHAR()
    ON_MESSAGE(WM_IME_CHAR, OnImeChar)
    ON_MESSAGE(EC_WM_SYNC_GUTTER, OnSyncGutter)
    ON_MESSAGE(EC_WM_REDUMP, OnRedump)
END_MESSAGE_MAP()

CECEditor::CECEditor()
    : m_nContentH(0), m_nScroll(0), m_nCaretLine(0), m_nCaretCol(0),
      m_nAnchorLine(0), m_nAnchorCol(0), m_bDragging(FALSE), m_bOpenMark(FALSE),
      m_bEditing(FALSE), m_nEditRow(-1), m_nEditCol(-1),
      m_bParamShow(FALSE),
      m_nKind(ECK_ASM), m_nUndo(-1),
      m_nLineH(EC_CODE_H)
{
}

CECEditor::~CECEditor()
{
}

BOOL CECEditor::Create(CWnd* pParent, UINT nID)
{
    return CWnd::CreateEx(0,
        AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(NULL, IDC_IBEAM),
                            (HBRUSH)(COLOR_WINDOW + 1), NULL),
        _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL,
        CRect(0, 0, 0, 0), pParent, nID);
}

int CECEditor::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;
    ApplyTheme();
    return 0;
}

// 字号 / 行距可以随「系统配置 -> 程序显示」改，所以字体重建放在这里
void CECEditor::ApplyTheme()
{
    int nFontH = g_ecTheme.opt.nFontH;
    if (nFontH < 6 || nFontH > 72)
        nFontH = 12;

    // 原版代码编辑器用的就是 12px 宋体（实测 CJK 步进 12px、ASCII 6px）
    m_font.DeleteObject();
    m_fontU.DeleteObject();
    m_fontTable.DeleteObject();
    m_font.CreateFont(-nFontH, 0, 0, 0, FW_NORMAL, 0, 0, 0, GB2312_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                      FIXED_PITCH | FF_MODERN, _T("宋体"));
    m_fontU.CreateFont(-nFontH, 0, 0, 0, FW_NORMAL, 0, TRUE, 0, GB2312_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       FIXED_PITCH | FF_MODERN, _T("宋体"));
    m_fontTable.CreateFont(-nFontH, 0, 0, 0, FW_NORMAL, 0, 0, 0, GB2312_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                           FIXED_PITCH | FF_MODERN, _T("宋体"));
    m_nLineH = EcCodeH();

    if (!m_lines.empty())
    {
        Rebuild();
        Layout();
    }
    UpdateCaret();
    InvalidateAll();
}

// 切「表页」：只影响 Rebuild 建哪些行。滚动/插入点回开头（原版打开一个新文档就是这样）。
void CECEditor::SetKind(int k)
{
    if (k == m_nKind)
        return;
    m_nKind = k;
    Rebuild();
    m_nCaretLine = m_nCaretCol = 0;
    m_nCaretCell = -1;
    m_nAnchorLine = 0; m_nAnchorCol = 0;
    if (m_rows.size() > 0)
    {
        // 表格页：插入点落在第一行（原版整页是表格时也这样，见 SetText）
        m_nCaretLine = m_rows[0].src;
    }
    m_bOpenMark = TRUE;
    m_nScroll = 0;
    Layout();
    UpdateScrollBar();
    Invalidate(FALSE);
}

// ------------------------------------------------------------------ 撤消 / 重复
// 做法：**整篇源码 + 插入点**的快照栈（懒但绝对正确）。文档只有几十行，内存无所谓；
// 想省内存再换成「只存被改的那几行」的增量栈（现在没必要，见 ponytail）。
// 语义与 Windows 一致：栈里永远存着"当前"这一份，m_nUndo 指向它；
// 新的改动会砍掉 m_nUndo 之后的重做分支。
static CString EcJoinLines(const std::vector<CString>& v)
{
    // ⚠️ 性能：**必须用 std::string + 预计算 reserve**，不能用 CString 逐行 += 。
    //   MFC 的 CString 在引用计数 >1 时每次 += 都整体复制缓冲区 -> O(n^2)。
    //   实测（精易模块 32172 行）：旧写法 **44141 ms**，
    //   占了整个 SetText（44906 ms）的 98% 。
    size_t total = 0;
    for (size_t k1 = 0; k1 < v.size(); k1++)
        total += (size_t)v[k1].GetLength() + 2;
    std::string s;
    s.reserve(total + 16);
    for (size_t k2 = 0; k2 < v.size(); k2++)
    {
        if (k2) s += "\r\n";
        s += (LPCSTR)v[k2];
    }
    return CString(s.c_str());
}

void CECEditor::PushUndo()
{
    EcSnapshot s;
    s.text = EcJoinLines(m_lines);
    s.cl = m_nCaretLine; s.cc = m_nCaretCol;
    s.al = m_nAnchorLine; s.ac = m_nAnchorCol;
    s.scroll = m_nScroll;
    // 砍掉重做分支，再压入当前状态
    if (m_nUndo + 1 < (int)m_undo.size())
        m_undo.erase(m_undo.begin() + m_nUndo + 1, m_undo.end());
    if (!m_undo.empty() && m_undo.back().text == s.text)
    {
        m_undo.back() = s;              // 内容没变（只挪了光标）就不新开一步
        m_nUndo = (int)m_undo.size() - 1;
        return;
    }
    m_undo.push_back(s);
    // 步数上限：原来写死 200。现在受原版「系统配置->通常->支持撤消功能时可用内存尺寸
    // （千字节）」（控件 id=1011，g_ecCfg.nUndoMemKB）约束：0 = 不限制，这时仍保留
    // 200 步的硬顶（免得无限长）。一步粗估 256 字节。
    size_t nMax = 200;
    if (g_ecCfg.nUndoMemKB > 0)
    {
        size_t nByMem = (size_t)g_ecCfg.nUndoMemKB * 1024 / 256;
        if (nByMem < 4) nByMem = 4;
        if (nByMem < nMax) nMax = nByMem;
    }
    while (m_undo.size() > nMax)
        m_undo.erase(m_undo.begin());
    m_nUndo = (int)m_undo.size() - 1;
}

void CECEditor::ApplySnapshot(const EcSnapshot& s)
{
    m_lines.clear();
    int a = 0, n = s.text.GetLength();
    while (a <= n)
    {
        int b = s.text.Find(_T('\n'), a);
        CString line;
        if (b < 0) { line = s.text.Mid(a); a = n + 1; }
        else { line = s.text.Mid(a, b - a); a = b + 1; }
        line.Replace(_T("\r"), _T(""));
        m_lines.push_back(line);
        if (a > n) break;
    }
    if (m_lines.empty()) m_lines.push_back(_T(""));
    Rebuild();
    m_nCaretLine = s.cl; m_nCaretCol = s.cc;
    m_nAnchorLine = s.al; m_nAnchorCol = s.ac;
    if (m_nCaretLine >= (int)m_lines.size()) m_nCaretLine = (int)m_lines.size() - 1;
    if (m_nCaretLine < 0) m_nCaretLine = 0;
    Layout();
    m_nScroll = s.scroll;
    ScrollTo(m_nScroll);
    UpdateScrollBar();
    InvalidateAll();
    UpdateCaret();
}


// ---------------------------------------------------------------- 书签
// 存在源码行号上（按行号升序），这样编辑让行号漂移时顺序仍然对。
// ponytail: 用有序数组 + 线性查找，几十个书签够用；真要上千个再换 set。

// 「书签跳转」(#32965) 要用到的几个小接口
int CECEditor::BookmarkAt(int i) const
{
    if (i < 0 || i >= (int)m_bookmarks.size()) return -1;
    return m_bookmarks[i];
}

CString CECEditor::LineTextOf(int L) const
{
    if (L < 0 || L >= (int)m_lines.size()) return CString();
    CString s = m_lines[L];
    s.TrimLeft(); s.TrimRight();
    if (s.GetLength() > 120) s = s.Left(120);
    return s;
}

void CECEditor::RemoveBookmarkLine(int L)
{
    for (size_t i = 0; i < m_bookmarks.size(); i++)
        if (m_bookmarks[i] == L) { m_bookmarks.erase(m_bookmarks.begin() + i); break; }
    InvalidateAll();
}

void CECEditor::ClearBookmarks()
{
    m_bookmarks.clear();
    InvalidateAll();
}

void CECEditor::GotoLine(int L)
{
    if (L < 0 || L >= (int)m_lines.size()) return;
    m_nCaretLine = L; m_nCaretCol = 0;
    m_nAnchorLine = L; m_nAnchorCol = 0;
    ClearOpenMark();
    InvalidateAll();
    UpdateCaret();
    EnsureVisible();
}


// ---------------------------------------------------------------- 断点（调试的第一步）
// 只做「状态」：光标行加/去断点、查询、全清。运行/单步这些要执行引擎的还没做。
BOOL CECEditor::HasBreakpoint(int L) const
{
    if (L < 0) return FALSE;
    for (size_t i = 0; i < m_breakpoints.size(); i++)
        if (m_breakpoints[i] == L) return TRUE;
    return FALSE;
}

void CECEditor::ToggleBreakpoint()
{
    int L = m_nCaretLine;
    for (size_t i = 0; i < m_breakpoints.size(); i++)
    {
        if (m_breakpoints[i] == L)
        {
            m_breakpoints.erase(m_breakpoints.begin() + i);
            InvalidateAll();
            return;
        }
    }
    size_t k = 0;
    while (k < m_breakpoints.size() && m_breakpoints[k] < L) k++;
    m_breakpoints.insert(m_breakpoints.begin() + k, L);
    InvalidateAll();
}

void CECEditor::ClearBreakpoints()
{
    m_breakpoints.clear();
    InvalidateAll();
}

void CECEditor::ToggleBookmark()
{
    int L = m_nCaretLine;
    for (size_t i = 0; i < m_bookmarks.size(); i++)
    {
        if (m_bookmarks[i] == L)
        {
            m_bookmarks.erase(m_bookmarks.begin() + i);
            InvalidateAll();
            return;
        }
    }
    size_t k = 0;
    while (k < m_bookmarks.size() && m_bookmarks[k] < L) k++;
    m_bookmarks.insert(m_bookmarks.begin() + k, L);
    InvalidateAll();
}

void CECEditor::GotoBookmark(BOOL bForward)
{
    if (m_bookmarks.empty()) return;
    int L = -1;
    if (bForward)
    {
        for (size_t i = 0; i < m_bookmarks.size(); i++)
            if (m_bookmarks[i] > m_nCaretLine) { L = m_bookmarks[i]; break; }
        if (L < 0) L = m_bookmarks[0];              // 绕回第一个
    }
    else
    {
        for (size_t i = m_bookmarks.size(); i > 0; i--)
            if (m_bookmarks[i - 1] < m_nCaretLine) { L = m_bookmarks[i - 1]; break; }
        if (L < 0) L = m_bookmarks[m_bookmarks.size() - 1];   // 绕回最后一个
    }
    if (L < 0 || L >= (int)m_lines.size()) return;
    m_nCaretLine = L; m_nCaretCol = 0;
    m_nAnchorLine = L; m_nAnchorCol = 0;
    ClearOpenMark();
    InvalidateAll();
    UpdateCaret();
    EnsureVisible();
}

void CECEditor::Undo()
{
    if (m_nUndo <= 0) return;
    m_nUndo--;
    ApplySnapshot(m_undo[m_nUndo]);
    ClearOpenMark();
}

void CECEditor::Redo()
{
    if (m_nUndo + 1 >= (int)m_undo.size()) return;
    m_nUndo++;
    ApplySnapshot(m_undo[m_nUndo]);
    ClearOpenMark();
}

// 改完源码的统一收尾（所有编辑命令都走这里，别各自抄一遍）
void CECEditor::AfterEdit()
{
    PushUndo();
    if (m_nCaretLine >= (int)m_lines.size()) m_nCaretLine = (int)m_lines.size() - 1;
    if (m_nCaretLine < 0) m_nCaretLine = 0;
    Rebuild();
    Layout();
    UpdateScrollBar();
    ClearOpenMark();
    InvalidateAll();
    UpdateCaret();
    EnsureVisible();
}

// 删除行（编辑->删除行 F10 #32939）：整行连行尾一起删；有选区就删选到的那些行
void CECEditor::DeleteCurrentLine()
{
    int n = (int)m_lines.size();
    if (n <= 0) return;
    int l0 = m_nCaretLine, l1 = m_nCaretLine;
    if (HasSel())
    {
        int c0, c1;
        GetSel(l0, c0, l1, c1);
    }
    if (l0 < 0) l0 = 0;
    if (l1 >= n) l1 = n - 1;
    if (l1 < l0) return;
    m_lines.erase(m_lines.begin() + l0, m_lines.begin() + l1 + 1);
    if (m_lines.empty()) m_lines.push_back(_T(""));
    m_nCaretLine = (l0 < (int)m_lines.size()) ? l0 : (int)m_lines.size() - 1;
    m_nCaretCol = 0;
    m_nAnchorLine = m_nCaretLine; m_nAnchorCol = 0;
    AfterEdit();
}

// 屏蔽 / 解除屏蔽（编辑->收缩展开/屏蔽->屏蔽 Ctrl+K #32936 / 解除屏蔽 Ctrl+M #32937）
// 屏蔽 = 行首插一个 `'`；解除屏蔽 = 去掉行首那个 `'`（后面紧跟的一个空格也一起去掉，
// 因为原版样本里的注释都是 `' 文字` 这种写法 —— 原版到底删不删那个空格**没对拍**，见 §6）
void CECEditor::ToggleComment(BOOL bComment)
{
    int n = (int)m_lines.size();
    if (n <= 0) return;
    int l0 = m_nCaretLine, l1 = m_nCaretLine;
    if (HasSel())
    {
        int c0, c1;
        GetSel(l0, c0, l1, c1);
    }
    if (l0 < 0) l0 = 0;
    if (l1 >= n) l1 = n - 1;
    if (l1 < l0) return;
    for (int i = l0; i <= l1; i++)
    {
        CString& s = m_lines[i];
        if (bComment)
        {
            if (s.GetLength() > 0 && s[0] == _T('\'')) continue;   // 已经屏蔽过了
            s = CString(_T("'")) + s;
        }
        else
        {
            if (s.GetLength() == 0 || s[0] != _T('\'')) continue;
            s.Delete(0);
            if (s.GetLength() > 0 && s[0] == _T(' ')) s.Delete(0);
        }
    }
    AfterEdit();
}

// 光标当前落在哪个子程序里（收缩/展开子程序要用子程序名当键）
static CString EcSubOfLine(const std::vector<CString>& lines, int nLine)
{
    for (int i = nLine; i >= 0 && i < (int)lines.size(); i--)
    {
        CString t = lines[i];
        t.TrimLeft(); t.TrimRight();
        if (t.GetLength() >= 5 && t.Left(4) == _T(".子程序"))
        {
            CString rest = t.Mid(4);
            CString name = EcTrim(rest);
            int k = 0;
            while (k < name.GetLength() && name[k] != _T(',') && name[k] != _T(' ')) k++;
            return name.Left(k);
        }
    }
    return CString(_T(""));
}

void CECEditor::CollapseSubOfCaret(BOOL bCollapse)
{
    CString name = EcSubOfLine(m_lines, m_nCaretLine);
    if (name.IsEmpty()) return;
    SetCollapsed(name, bCollapse);
    Rebuild();
    Layout();
    UpdateScrollBar();
    InvalidateAll();
}

void CECEditor::ExpandAllSubs()
{
    if (m_setCollapsed.empty()) return;
    m_setCollapsed.clear();
    Rebuild();
    Layout();
    UpdateScrollBar();
    InvalidateAll();
}

// ---------------------------------------------------------------------------
// 「加到当前块首尾」(#32800-32806) / 「转换为 ->」(#32807-32813)
//
// 两个命令共用的块首/块尾表（顺序 = 原版菜单顺序）：
//   0 如果 / 如果真结束      1 如果真 / 如果真结束   2 判断 / 判断结束
//   3 判断循环首 / 判断循环尾  4 循环判断首 / 循环判断尾
//   5 计次循环首 / 计次循环尾  6 变量循环首 / 变量循环尾
//
// ⚠️ 依据：菜单文字本身（src\menu_main.rc，从 e.exe RT_MENU 127 还原）+
//    原版例程里的实际写法（shots\live_flowcmd\view0.png、sample 源码）：
//      `.计次循环首 (10, i)` … `.计次循环尾 ()`、`如果真 (i ＞ 5)` … `如果真结束`。
//    **没有**拍到原版执行这两个命令的基准（所以括号里的占位、缩进宽度是推断的，见 HANDOFF §6）。
static const TCHAR* g_szBlockPair[7][2] = {
    { _T("如果"),       _T("如果结束")   },
    { _T("如果真"),     _T("如果真结束") },
    { _T("判断"),       _T("判断结束")   },
    { _T("判断循环首"), _T("判断循环尾") },
    { _T("循环判断首"), _T("循环判断尾") },
    { _T("计次循环首"), _T("计次循环尾") },
    { _T("变量循环首"), _T("变量循环尾") },
};
#define EC_BLOCK_KINDS 7

// 行首的空白（用来给块首/块尾对齐）
static CString EcLeadingBlanks(const CString& s)
{
    int i = 0;
    while (i < s.GetLength() && (s[i] == _T(' ') || s[i] == _T('\t'))) i++;
    return s.Left(i);
}

CString CECEditor::LineAt(int i) const
{
    if (i < 0 || i >= (int)m_lines.size()) return CString(_T(""));
    return m_lines[i];
}

// 起一个不重名的名字：基名后面接 1,2,3…（在整篇源码里找，够用）
// ⚠️ 调用点传的基名**不带数字**（"子程序" 而不是 "子程序1"）—— 第一版传了带数字的，
//    结果名字变成「全局变量11」（基名 全局变量1 + 计数 1）。
static CString EcNewName(const std::vector<CString>& lines, LPCTSTR pszBase)
{
    for (int k = 1; k < 10000; k++)
    {
        CString cand;
        cand.Format(_T("%s%d"), pszBase, k);
        BOOL bUsed = FALSE;
        for (int i = 0; i < (int)lines.size(); i++)
        {
            if (lines[i].Find(cand) >= 0) { bUsed = TRUE; break; }
        }
        if (!bUsed) return cand;
    }
    return CString(pszBase);
}

// 「插入参数/局部变量」的落点：参数插在当前子程序的「.参数」段末尾，
// 局部变量插在「.局部变量」段末尾（都没有就插在子程序行之后）。
static int EcInsertPointForLocal(const std::vector<CString>& lines, BOOL bParam)
{
    LPCTSTR pszWant = bParam ? _T(".参数") : _T(".局部变量");
    int nSub = -1, nLast = -1;
    for (int i = 0; i < (int)lines.size(); i++)
    {
        CString t = lines[i];
        t.TrimLeft();
        if (t.GetLength() >= 4 && t.Left(4) == _T(".子程序")) nSub = i;
        if (nSub >= 0 && t.GetLength() >= 3 && t.Left(3) == pszWant) nLast = i;
    }
    if (nLast >= 0) return nLast + 1;
    if (nSub >= 0) return nSub + 1;
    return (int)lines.size();
}

// ---------------------------------------------------------------------------
// 「插入」菜单
//
// 每个命令插的**声明写法**照原版例程（shots\zoom\sample_backup.txt / EplParser.g4）：
//     .子程序 名字, 返回值类型, , 公开       （原版默认是「子程序」+ 无返回值）
//     .程序集 名字
//     .程序集变量 名字, 类型
//     .全局变量 名字, 类型
//     .数据类型 名字        （成员由用户接着写 .成员）
//     .类 名字
//     .DLL命令 名字, 返回值类型, "库文件名", "库中命令名"
//     .常量 名字, 值
//     .图片资源 名字        /  .声音资源 名字
//     .参数 名字, 类型      /  .局部变量 名字, 类型
//
    // ⚠️ 每个命令的**插入位置**：原版是「插入到当前位置后面」，本工程按"就近"实现 ——
    //    参数/局部变量插在当前子程序的对应段末尾，其它声明插在文件末尾。
    //    这是**推断**（原版这两个命令的实际落点没拍到基准，见 HANDOFF §6）。
void CECEditor::InsertDeclaration(int nWhat)
{
    if (m_lines.empty()) m_lines.push_back(_T(""));

    CString name;
    CString line;
    int nAt = (int)m_lines.size();          // 默认插到末尾

    switch (nWhat)
    {
    case 0:  name = EcNewName(m_lines, _T("子程序"));  line.Format(_T(".子程序 %s"),  (LPCTSTR)name); break;
    case 1:  name = EcNewName(m_lines, _T("程序集"));  line.Format(_T(".程序集 %s"),  (LPCTSTR)name); break;
    case 2:  name = EcNewName(m_lines, _T("窗口"));    line.Format(_T(".窗口 %s"),    (LPCTSTR)name); break;
    case 3:  name = EcNewName(m_lines, _T("数据类型"));line.Format(_T(".数据类型 %s"),(LPCTSTR)name); break;
    case 4:  name = EcNewName(m_lines, _T("全局变量"));line.Format(_T(".全局变量 %s, 整数型"), (LPCTSTR)name); break;
    case 5:  name = EcNewName(m_lines, _T("DLL命令")); line.Format(_T(".DLL命令 %s, 整数型, \"\", \"\""), (LPCTSTR)name); break;
    case 6:  name = EcNewName(m_lines, _T("常量"));    line.Format(_T(".常量 %s, 0"),  (LPCTSTR)name); break;
    case 7:  name = EcNewName(m_lines, _T("图片"));    line.Format(_T(".图片资源 %s"), (LPCTSTR)name); break;
    case 8:  name = EcNewName(m_lines, _T("声音"));    line.Format(_T(".声音资源 %s"), (LPCTSTR)name); break;
    case 9:  name = EcNewName(m_lines, _T("类"));      line.Format(_T(".类 %s"),      (LPCTSTR)name); break;
    case 10: // 参数：插在当前子程序的「.参数」那一段最后
        name = EcNewName(m_lines, _T("参数"));
        line.Format(_T(".参数 %s, 整数型"), (LPCTSTR)name);
        nAt = EcInsertPointForLocal(m_lines, TRUE);
        break;
    case 11: // 局部变量：插在当前子程序里最后一条 .局部变量 之后
        name = EcNewName(m_lines, _T("变量"));
        line.Format(_T(".局部变量 %s, 整数型"), (LPCTSTR)name);
        nAt = EcInsertPointForLocal(m_lines, FALSE);
        break;
    default: return;
    }

    m_lines.insert(m_lines.begin() + nAt, line);
    EcTrace(_T("ins what=%d at=%d lines=%d line=%s"), nWhat, nAt, (int)m_lines.size(), (LPCTSTR)line);
    m_nCaretLine = nAt;
    m_nCaretCol = 0;
    m_nAnchorLine = m_nCaretLine; m_nAnchorCol = 0;
    EcTrace(_T("ins step=before-afteredit"));
    AfterEdit();
    EcTrace(_T("ins step=after-afteredit"));

    // 插入的如果是「表的声明」，顺手把编辑器切到对应的表页（原版的插入命令会跳到那张表）
    switch (nWhat)
    {
    case 3:  SetKind(ECK_TYPE);  break;
    case 4:  SetKind(ECK_GVAR);  break;
    case 5:  SetKind(ECK_DLL);   break;
    case 6:  SetKind(ECK_CONST); break;
    case 7:  SetKind(ECK_IMG);   break;
    case 8:  SetKind(ECK_SND);   break;
    default: break;
    }
    EcTrace(_T("ins step=done kind=%d lines=%d"), m_nKind, (int)m_lines.size());
    // 收尾再倒一次模型：`SRC` 行是插入类命令的唯一断言依据（见 DumpModel 的说明），
    // 而 SetKind 之后不会自动倒 —— 不补这一下，自动化测试读到的还是插入前那份。
    DumpModel();
}

// 诊断追踪：只在设了 ECLONE_DUMP_MODEL 时才往 <model>.trace.txt 追加一行。
// 用途 = 区分「命令根本没执行」和「执行了但结果没进显示模型」——
// 比如「转换为 -> 如果真」插进去的 `如果真结束` 是 g_szHidden 里的行，
// 显示模型里**一行都不会多**，只靠模型断言会误判成"命令没生效"。
void EcTrace(LPCTSTR pszFmt, ...)
{
    TCHAR szPath[MAX_PATH];
    DWORD n = GetEnvironmentVariable(_T("ECLONE_DUMP_MODEL"), szPath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    CString sPath(szPath);
    sPath += _T(".trace.txt");
    FILE* f = fopen(sPath, "a");
    if (f == NULL) return;
    va_list ap;
    va_start(ap, pszFmt);
    vfprintf(f, pszFmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

// 「加到当前块首尾」：选到的行整体缩进一级，前后各插一行块首/块尾
void CECEditor::WrapSelectionInBlock(int nKind)
{
    if (nKind < 0 || nKind >= EC_BLOCK_KINDS) return;
    int n = (int)m_lines.size();
    if (n <= 0) return;

    int l0 = m_nCaretLine, l1 = m_nCaretLine;
    if (HasSel())
    {
        int c0, c1;
        GetSel(l0, c0, l1, c1);
    }
    if (l0 < 0) l0 = 0;
    if (l1 >= n) l1 = n - 1;
    if (l1 < l0) return;

    CString pad = EcLeadingBlanks(m_lines[l0]);
    CString inner = pad + _T("    ");           // 块体比块首深一级（编写时的习惯写法）

    std::vector<CString> ins;
    ins.push_back(pad + g_szBlockPair[nKind][0]);
    for (int i = l0; i <= l1; i++)
        ins.push_back(m_lines[i].IsEmpty() ? m_lines[i] : inner + m_lines[i]);
    ins.push_back(pad + g_szBlockPair[nKind][1]);

    m_lines.erase(m_lines.begin() + l0, m_lines.begin() + l1 + 1);
    m_lines.insert(m_lines.begin() + l0, ins.begin(), ins.end());

    m_nCaretLine = l0 + 1;                      // 停在块体第一行
    m_nCaretCol = 0;
    m_nAnchorLine = m_nCaretLine; m_nAnchorCol = 0;
    AfterEdit();
}

// 「转换为 ->」：选到的那些行**自己**当块首，紧跟其后插一行块尾
void CECEditor::ConvertSelectionToBlock(int nKind)
{
    if (nKind < 0 || nKind >= EC_BLOCK_KINDS) return;
    int n = (int)m_lines.size();
    if (n <= 0) return;

    int l0 = m_nCaretLine, l1 = m_nCaretLine;
    if (HasSel())
    {
        int c0, c1;
        GetSel(l0, c0, l1, c1);
    }
    if (l0 < 0) l0 = 0;
    if (l1 >= n) l1 = n - 1;
    if (l1 < l0) return;

    // ⚠️ 行号会随着插入往后漂 —— 从后往前插，前面的行号才不会失效
    for (int i = l1; i >= l0; i--)
    {
        if (m_lines[i].IsEmpty()) continue;
        CString pad = EcLeadingBlanks(m_lines[i]);
        m_lines.insert(m_lines.begin() + i + 1, pad + g_szBlockPair[nKind][1]);
    }
    EcTrace(_T("conv kind=%d l0=%d l1=%d lines=%d"), nKind, l0, l1, (int)m_lines.size());
    m_nCaretLine = l0;                          // 停在块首那一行
    m_nCaretCol = 0;
    m_nAnchorLine = m_nCaretLine; m_nAnchorCol = 0;
    AfterEdit();
}


// 光标处的「词」：按 GBK 整字推进；汉字/字母/数字/下划线算词的一部分，全角标点（A1/A3 引导）不算
static BOOL EcIsWordChar(const CString& s, int i, int& nLen)
{
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || (c >= '0' && c <= '9'))
    { nLen = 1; return TRUE; }
    if (c >= 0x81 && i + 1 < s.GetLength() && EcGbTail((unsigned char)s[i + 1]))
    {
        nLen = 2;
        return (c != 0xA1 && c != 0xA3);
    }
    nLen = 1;
    return FALSE;
}

static void EcWordAt(const CString& s, int col, int& a, int& b)
{
    int n = s.GetLength();
    if (col > n) col = n;
    a = b = col;
    int i = 0;
    while (i < n)
    {
        int len = 1;
        if (!EcIsWordChar(s, i, len)) { i += len; continue; }
        int s0 = i, e0 = i + len;
        while (e0 < n) { int l2 = 1; if (!EcIsWordChar(s, e0, l2)) break; e0 += l2; }
        if (col >= s0 && col <= e0) { a = s0; b = e0; return; }
        i = e0;
    }
}

// 寻找下一个 / 上一个（编辑菜单 #32878 / #32879，F3 / Shift+F3）。
// 只做「找到并选中」这一层：原版那个「寻找」对话框（DIALOG id=284：查找文本下拉框 +
// 「区分大小写」勾 + 确定/取消）还没做，见 HANDOFF §5 编辑菜单那条。
// 找的词：优先用当前选区（单行内），否则用插入点所在的词。找不到就绕回文档开头再找一圈。

// 不区分大小写的查找（只在 ASCII 上折叠大小写；GBK 首字节 >= 0x81 永远不会落到 A-Z 上，
// 所以逐字节折叠对汉字是安全的）。原版「寻找」对话框的「区分大小写」勾选对应这里。
// 在一行源码里从第 nFrom 个字节起找 needle，返回它的起始字节下标（找不到 -1）。
// 用途 = 把表格单元格的文字映射回源码那一行的字节范围（选区的反白要按字节列算）。
// ⚠️ 按**字节**比较（本工程全用 GBK 字节串 + CString 的 char 语义），不要用 MBCS 的 Find，
//    它对全角字符的边界处理跟我们要的"字节列"不是一回事。
static int FindInSourceFrom(const CString& s, int nFrom, const CString& needle)
{
    int n = s.GetLength(), m = needle.GetLength();
    if (m <= 0 || n < m) return -1;
    for (int i = (nFrom > 0 ? nFrom : 0); i + m <= n; i++)
    {
        int k = 0;
        while (k < m && s[i + k] == needle[k]) k++;
        if (k == m) return i;
    }
    return -1;
}

static int EcFindNoCase(const CString& s, const CString& needle, int nFrom)
{
    int n = s.GetLength(), m = needle.GetLength();
    if (m <= 0 || n < m) return -1;
    for (int i = (nFrom > 0 ? nFrom : 0); i + m <= n; i++)
    {
        int k = 0;
        while (k < m)
        {
            unsigned char a = (unsigned char)s[i + k], b = (unsigned char)needle[k];
            if (a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
            if (a != b) break;
            k++;
        }
        if (k == m) return i;
    }
    return -1;
}


// 「替换」（编辑->寻找替换 Ctrl+H #3 里的「替换(&R)」按钮）：
// 当前选区必须正好是"要找的文本"，把它换成替换文本，然后接着找下一个。
BOOL CECEditor::ReplaceCurrent()
{
    if (!HasSel() || m_sFindText.IsEmpty()) return FALSE;
    int l0, c0, l1, c1;
    GetSel(l0, c0, l1, c1);
    if (l0 != l1) return FALSE;
    if (c0 < 0 || c1 > m_lines[l0].GetLength()) return FALSE;
    CString cur = m_lines[l0].Mid(c0, c1 - c0);
    if (cur.GetLength() != m_sFindText.GetLength()) return FALSE;
    if (m_bFindCase ? (cur != m_sFindText) : (cur.CompareNoCase(m_sFindText) != 0))
        return FALSE;
    m_lines[l0] = m_lines[l0].Left(c0) + m_sReplaceText + m_lines[l0].Mid(c1);
    m_nCaretLine = l0;
    m_nCaretCol = c0 + m_sReplaceText.GetLength();
    m_nAnchorLine = m_nCaretLine;
    m_nAnchorCol = m_nCaretCol;
    AfterEdit();
    return TRUE;
}

// 「全部替换(&A)」：找到就换、换完接着找。返回换了几处。
// ponytail: 靠一个计数上限防死循环（替换文本里又包含查找文本时会原地打转），
//           真要做得像原版那样精确得先算一遍总数再替换。
int CECEditor::ReplaceAll()
{
    int n = 0, nGuard = 0;
    while (nGuard++ < 5000)
    {
        if (!FindNext(TRUE)) break;
        if (!ReplaceCurrent()) break;
        n++;
    }
    return n;
}

BOOL CECEditor::FindNext(BOOL bForward)
{
    int nL = (int)m_lines.size();
    if (nL == 0) return FALSE;
    if (m_nCaretLine < 0 || m_nCaretLine >= nL) m_nCaretLine = 0;

    CString needle = m_sFindText;   // 对话框里填的（#32877「开始寻找」）
    if (needle.IsEmpty() && HasSel())
    {
        int l0, c0, l1, c1;
        GetSel(l0, c0, l1, c1);
        if (l0 == l1 && c1 > c0 && c0 >= 0 && c1 <= m_lines[l0].GetLength())
            needle = m_lines[l0].Mid(c0, c1 - c0);
    }
    if (needle.IsEmpty())
    {
        int a = 0, b = 0;
        EcWordAt(m_lines[m_nCaretLine], m_nCaretCol, a, b);
        if (b > a) needle = m_lines[m_nCaretLine].Mid(a, b - a);
    }
    if (needle.IsEmpty()) return FALSE;

    // 搜索原点：**有选区时用选区的"远端"** —— 正向用选区末尾、反向用选区开头。
    // ⚠️ 不能用插入点的当前值：找到之后选区正好是「词首..词尾」、插入点在词尾，
    //    反向搜 `at + len <= caret` 会把**刚找到的这一个**再命中一次
    //    （现象 = Shift+F3 原地不动；本轮 find_smoke 抓到的就是这个）。
    int nOrg = m_nCaretCol;
    if (HasSel())
    {
        int l0, c0, l1, c1;
        GetSel(l0, c0, l1, c1);
        if (l0 == l1)
            nOrg = bForward ? (c1 > c0 ? c1 : c0) : (c1 < c0 ? c1 : c0);
    }

    for (int k = 0; k <= nL; k++)
    {
        int L = bForward ? (m_nCaretLine + k) % nL
                         : ((m_nCaretLine - k) % nL + nL) % nL;
        const CString& s = m_lines[L];
        int best = -1, idx = 0;
        while (TRUE)
        {
            int at = m_bFindCase ? s.Find(needle, idx) : EcFindNoCase(s, needle, idx);
            if (at < 0) break;
            BOOL bOk = TRUE;
            if (k == 0)     // 起始行只认「方向正确」的那些出现位置
            {
                if (bForward) bOk = (at >= nOrg);
                else          bOk = (at + needle.GetLength() <= nOrg);
            }
            if (bOk) best = at;             // 正向取第一个 / 反向取最后一个（都是"最近的"）
            if (bOk && bForward) break;
            idx = at + 1;
        }
        if (best >= 0)
        {
            m_nAnchorLine = L; m_nAnchorCol = best;
            m_nCaretLine = L;  m_nCaretCol = best + needle.GetLength();
            ClearOpenMark();
            InvalidateAll();
            UpdateCaret();
            EnsureVisible();
            return TRUE;
        }
    }
    return FALSE;
}

// ECLONE_TIME=1 时的阶段计时（打到 stderr）
static void EcEdTime(const char* pszStage, DWORD dwStart)
{
    char szOn[8];
    if (::GetEnvironmentVariableA("ECLONE_TIME", szOn, sizeof(szOn)) == 0) return;
    char m[192];
    ::wsprintfA(m, "[ECLONE_TIME] %s : %lu ms\n", pszStage,
                (unsigned long)(::GetTickCount() - dwStart));
    ::fputs(m, stderr);
}

void CECEditor::SetText(LPCTSTR psz)
{
    DWORD t0 = ::GetTickCount();
    m_lines.clear();
    CString s(psz);
    int a = 0;
    while (a <= s.GetLength())
    {
        int b = s.Find(_T('\n'), a);
        CString line;
        if (b < 0) { line = s.Mid(a); a = s.GetLength() + 1; }
        else { line = s.Mid(a, b - a); a = b + 1; }
        line.Replace(_T("\r"), _T(""));
        m_lines.push_back(line);
        if (a > s.GetLength()) break;
    }
    EcEdTime("S1_splitlines", t0);
    DWORD t1 = ::GetTickCount();
    Rebuild();
    EcEdTime("S2_Rebuild", t1);
    DWORD t2 = ::GetTickCount();
    m_nCaretLine = m_nCaretCol = 0;
    m_nCaretCell = -1;
    for (int i = 0; i < (int)m_rows.size(); i++)
        if (m_rows[i].kind == 1) { m_nCaretLine = m_rows[i].src; break; }
    // 整页都是表格（Dll命令页）时没有代码行 —— 原版把插入点放在第一行
    // （证据：用户那张 Dll 页截图的流程槽铅笔就画在第一行上）
    if (m_rows.size() > 0 && m_rows[0].kind == 0)
        m_nCaretLine = m_rows[0].src;
    m_bOpenMark = TRUE;             // 刚打开：流程槽画红色铅笔，首次交互后消失
    m_nAnchorLine = m_nCaretLine; m_nAnchorCol = 0;
    m_nScroll = 0;                  // 打开时停在程序集头（与原版一致，不要跟着光标滚）
    Layout();
    EcEdTime("S3_Layout", t2);
    DWORD t3 = ::GetTickCount();
    UpdateScrollBar();
    Invalidate(FALSE);
    // 打开文档 = 撤消栈的第 0 份（Undo 回不到"打开之前"）
    m_undo.clear();
    m_nUndo = -1;
    EcEdTime("S4_scroll_invalidate", t3);
    DWORD t4 = ::GetTickCount();
    PushUndo();
    EcEdTime("S5_PushUndo", t4);
    // 诊断：设了 ECLONE_DUMP_MODEL=<路径> 就把排好的显示模型倒出来（量像素对位用）
    DWORD t5 = ::GetTickCount();
    DumpModel();
    EcEdTime("S6_DumpModel", t5);
    EcEdTime("S0_SetText_total", t0);
}

// 把当前排好版的显示模型倒成文本。除了 SetText 会自动倒一次，
// 还可以发 EC_WM_REDUMP 强制再倒（自动化测试用：改完状态再倒一次比对）。
void CECEditor::DumpModel()
{
    TCHAR szDump[512];
    if (::GetEnvironmentVariable(_T("ECLONE_DUMP_MODEL"), szDump, 512) <= 0)
        return;
    FILE* f = fopen(szDump, "w");
    if (!f)
        return;
    int k, c;
    for (k = 0; k < (int)m_tables.size(); k++)
    {
        // kind 是表的排版类别；再补一个 `pkind` = 当前页类别（m_nKind），
        // 否则只按 kind 分不出"程序集页的变量表"和"全局变量页的表"
        fprintf(f, "TABLE %d cols=%d kind=%d pkind=%d w=",
                k, m_tables[k].cols, m_tables[k].kind, m_nKind);
        for (c = 0; c < m_tables[k].cols; c++) fprintf(f, "%d,", m_tables[k].w[c]);
        fprintf(f, "\n");
    }
    for (k = 0; k < (int)m_rows.size(); k++)
    {
        EcRow& rr = m_rows[k];
        fprintf(f, "row %3d kind=%d table=%2d y=%4d h=%2d indent=%d mark=%d src=%d bp=%d | ",
                k, rr.kind, rr.table, rr.y, rr.h, rr.indent, rr.mark, rr.src,
                HasBreakpoint(rr.src) ? 1 : 0);
        if (rr.kind == 0)
        {
            for (c = 0; c < (int)rr.cells.size(); c++)
                fprintf(f, "[%s]%s", (LPCTSTR)rr.cells[c].s, rr.cells[c].check ? "*" : "");
            // 跨列/底色也倒出来（check_dll.py 要断言 Dll命令块的表格结构）
            fprintf(f, " span=");
            for (c = 0; c < (int)rr.cells.size(); c++) fprintf(f, "%d,", rr.cells[c].span);
            fprintf(f, " fill=%d", rr.fill);
        }
        else
        {
            fprintf(f, "%s", (LPCTSTR)rr.code);
            // 参数归属线的诊断：par=N = 这一行下面**真的画了** N 行参数（0 = 没画）。
            // 自动化测试靠它断言展开/收起，比数像素稳（参数行本身是叠着画的，不占模型行）。
            EcCallParams cp;
            int nPar = 0;
            if (rr.bParam && EcParseCall(rr.code, cp))
            {
                std::vector<CString> nm, ty;
                std::vector<BOOL>    df;
                int nLib = 0;
                if (EcCmdArgsByName(cp.cmd, &nm, &ty, &df)) nLib = (int)nm.size();
                int m = (int)cp.vals.size();
                if (nLib > 0 && m > nLib) m = nLib;
                if (!cp.bLastSep)
                    while (m > 0 && cp.vals[m - 1].IsEmpty()) m--;
                if (m > 0 && nLib > 0) nPar = m;
            }
            fprintf(f, "  par=%d", nPar);
        }
        fprintf(f, "\n");
    }
    fprintf(f, "paramshow=%d\n", m_bParamShow ? 1 : 0);
    // 原始源码行（`SRC <下标> <行文本>`）—— 放在最后，免得动到前面那些按行序解析的断言脚本。
    // ⚠️ 插入菜单那类命令只能靠这个断言：插进去的 `.参数`/`.局部变量` 行在显示模型里是隐藏的。
    for (k = 0; k < (int)m_lines.size(); k++)
        fprintf(f, "SRC %3d %s\n", k, (LPCTSTR)m_lines[k]);
    fclose(f);
}

LRESULT CECEditor::OnRedump(WPARAM w, LPARAM l)
{
    DumpModel();
    // 顺带把折叠命中框倒出来（诊断：ECLONE_DUMP_MODEL 同目录 .fold.txt）
    TCHAR szDump[512];
    if (::GetEnvironmentVariable(_T("ECLONE_DUMP_MODEL"), szDump, 512) > 0)
    {
        CString sPath = szDump;
        int k = sPath.ReverseFind('.');
        if (k > 0) sPath = sPath.Left(k);
        sPath += _T(".fold.txt");
        FILE* f = fopen(sPath, "w");
        if (f)
        {
            fprintf(f, "scroll=%d rows=%d anchor=%d,%d caret=%d,%d lines=%d\n",
                    m_nScroll, (int)m_foldHit.size(),
                    m_nAnchorLine, m_nAnchorCol, m_nCaretLine, m_nCaretCol,
                    (int)m_lines.size());
            for (int r = 0; r < (int)m_foldHit.size(); r++)
                if (!m_foldHit[r].IsRectEmpty())
                    fprintf(f, "fold row=%d rect=(%d,%d)-(%d,%d)\n", r,
                            m_foldHit[r].left, m_foldHit[r].top,
                            m_foldHit[r].right, m_foldHit[r].bottom);
            for (int r2 = 0; r2 < (int)m_paramHit.size(); r2++)
                if (!m_paramHit[r2].IsRectEmpty())
                    fprintf(f, "param row=%d rect=(%d,%d)-(%d,%d)\n", r2,
                            m_paramHit[r2].left, m_paramHit[r2].top,
                            m_paramHit[r2].right, m_paramHit[r2].bottom);
            fclose(f);
        }
        (void)w; (void)l;
    }
    return 0;
}

CString CECEditor::GetText() const
{
    CString s;
    for (int i = 0; i < (int)m_lines.size(); i++)
    {
        s += m_lines[i];
        if (i + 1 < (int)m_lines.size()) s += _T("\r\n");
    }
    return s;
}

void CECEditor::FocusEdit()
{
    SetFocus();
    UpdateCaret();
}

void CECEditor::InvalidateAll()
{
    Rebuild();
    Layout();
    UpdateScrollBar();
    Invalidate(FALSE);
}

// ------------------------------------------------------------------ 建模型
void CECEditor::AddTableRow(int nTable, int nCols, int nFill, LPCTSTR const* psz,
                            const int* pColors, const BOOL* pUnder, const BOOL* pCheck,
                            const int* pSpan, int nCells, int nSrc)
{
    EcRow r;
    r.kind   = 0;
    r.table  = nTable;
    r.fill   = nFill;
    r.src    = nSrc;
    r.y = r.h = 0;
    r.indent = 0;
    r.mark   = ECM_NONE;
    r.blkEnd = -2;
    r.blkLast = -1;
    // ⚠️ EcRow 是 POD，成员不初始化就是栈上的垃圾值。第 16 轮加 bParam 时漏了这句，
    //    于是「这一行有没有参数归属线」变成随机真 —— 实测同一进程里连着几行都读到
    //    4402570，流程槽上凭空多出一堆 -/+（原版只给带参的支持库命令画）。
    r.bParam = FALSE;
    for (int i = 0; i < nCells; i++)
    {
        EcCell c;
        c.s     = psz[i] ? psz[i] : _T("");
        c.cr    = pColors ? (COLORREF)pColors[i] : EC_C_TEXT;
        c.span  = pSpan ? pSpan[i] : 1;
        c.under = pUnder ? pUnder[i] : FALSE;
        c.check = pCheck ? pCheck[i] : FALSE;
        r.cells.push_back(c);
    }
    m_rows.push_back(r);
    if (nTable >= (int)m_tables.size())
    {
        EcTable t;
        t.cols = nCols;
        t.kind = ECT_ASM;
        t.w.assign(nCols, 0);
        m_tables.push_back(t);
    }
}

void CECEditor::AddCodeRow(const CString& s, int nSrc)
{
    EcRow r;
    r.kind   = 1;
    r.table  = -1;
    r.src    = nSrc;
    r.code   = s;
    r.indent = 0;
    r.mark   = ECM_NONE;
    r.fill   = -1;
    r.blkEnd = -2;
    r.blkLast = -1;
    r.bParam = FALSE;       // ⚠️ 见 AddTableRow 里的说明：不初始化就是栈垃圾
    r.y = r.h = 0;
    m_rows.push_back(r);
}

// 块首关键字里，哪些是「循环类」——只有它们的块首行会在横虚线末端画一个 ►
// （实测：shots\live_flowcmd\view0.png 的 `变量循环首` 行 x=59..62 有一个 4x7 灰三角；
//   而 `如果真 (…)` 行只有横虚线、没有三角）。块尾行一律不画 ►。
static const TCHAR* g_szLoopKw[] = {
    _T("计次循环首"), _T("变量循环首"), _T("判断循环首"), _T("循环判断首"), NULL
};
static BOOL EcIsLoopKw(const CString& s) { return EcInList(g_szLoopKw, s); }

// 解析 ".子程序 名, 返回值, , 公开" 之类
static void EcParseDecl(const CString& rest, std::vector<CString>& tok,
                        std::vector<CString>* pSep = NULL)
{
    EcSplit(rest, tok, pSep);
}

// 把 tok[nFrom..] 用原来的分隔符拼回来（备注里本来就可能有逗号，切分时要还原）
static CString EcJoinFrom(const std::vector<CString>& tok, int nFrom,
                          const std::vector<CString>* pSep = NULL)
{
    CString s;
    for (int i = nFrom; i < (int)tok.size(); i++)
    {
        if (i > nFrom)
        {
            if (pSep != NULL && i - 1 < (int)pSep->size())
                s += (*pSep)[i - 1];
            else
                s += _T(",");
        }
        s += tok[i];
    }
    return s;
}

void CECEditor::Rebuild()
{
    m_rows.clear();
    m_tables.clear();
    m_foldHit.clear();

    // 先规范化源码：语句行开头那个 "." 只是「文本形式」的写法，去掉后源码与显示一致
    int i0;
    for (i0 = 0; i0 < (int)m_lines.size(); i0++)
    {
        CString t = EcTrim(m_lines[i0]);
        if (t.IsEmpty()) continue;
        if (t[0] == _T('.'))
        {
            CString head = EcHeadWord(EcTrim(t.Mid(1)));
            // ⚠️ 判据别写成「关键字白名单」：第 21 轮加「插入菜单」时，白名单漏了
            //    `窗口`（`.窗口 x`），于是插入命令写出的声明被这里当普通语句把前导 `.` 抹掉 ——
            //    源码静默损坏（插入即坏）。改成**通用规则**：
            //    `.` 后面紧接一个汉字（或英文字母，DLL命令那种）就是声明，不再逐个数关键字。
            CString rest = EcTrim(t.Mid(1));
            BOOL bDecl = (rest.GetLength() > 0) &&
                         (((unsigned char)rest[0] >= 0x81) ||
                          (rest[0] >= _T('A') && rest[0] <= _T('Z')) ||
                          (rest[0] >= _T('a') && rest[0] <= _T('z')));
            (void)head;
            if (!bDecl) m_lines[i0] = EcTrim(t.Mid(1));
        }
    }

    int  nTable = -1, nFill = -1, nKind = ECT_ASM;
    int  nVarTable = -1;
    int  nMember = -1;            // 「自定义数据类型」页：当前成员子表的表号（-1 = 还没起）
    int  nContext = 0;              // 当前所在「块体」的缩进级别（不在任何块里 = 0）
    BOOL bParamHdr = FALSE;
    BOOL bDll = FALSE;           // 当前块是 Dll命令（参数表 5 列，语义也不同）
    BOOL bNeedDown = FALSE;      // 子程序表格后第一条语句 -> 流程槽画 ↓
    BOOL bSkipBody = FALSE;      // 当前子程序被收起 -> 跳过它的数据行
    std::vector<CString> tok;
    std::vector<CString> seps;      // 原样的分隔符（备注原样重拼用）
    std::vector<int>     blkStack;  // 块首关键字的显示行号栈（配对源码里的块尾）

    for (int i = 0; i < (int)m_lines.size(); i++)
    {
        CString s = EcTrim(m_lines[i]);
        if (s.IsEmpty())
        {
            // 常量/资源表页：原版把源里的空行也画成一行空表格（实测
            // shots\live_pg_常量表___\view_常量表....png 里每个常量组之间都有一行空的；
            // 全局变量表/自定义数据类型表里没有这种空行）
            if ((m_nKind == ECK_CONST || m_nKind == ECK_IMG || m_nKind == ECK_SND) && nTable >= 0)
            {
                int nc = m_tables[nTable].cols;
                if (nc > 5) nc = 5;
                LPCTSTR bl[5] = { _T(""), _T(""), _T(""), _T(""), _T("") };
                AddTableRow(nTable, nc, -1, bl, NULL, NULL, NULL, NULL, nc, i);
            }
            continue;
        }

        // 「表页」（全局变量/数据类型/常量/资源）**只有声明表格**：正文语句一行都不画
        // （原版这些页整页都是表格 —— 实拍 shots\live_page_gvar\view_全局变量.png）
        if (m_nKind >= ECK_GVAR && s[0] != _T('.'))
            continue;

        // 被收起的子程序：只保留它自己的头表格行，其余行全部不显示
        if (bSkipBody)
        {
            CString h = EcHeadWord(EcTrim(s));
            if (h == _T(".子程序") || h == _T(".程序集") || h == _T(".程序集变量") ||
                h == _T(".全局变量") || h == _T(".数据类型") || h == _T(".DLL命令") ||
                h == _T(".常量") || h == _T(".资源") || h == _T(".版本") ||
                h == _T(".图片资源") || h == _T(".声音资源") || h == _T(".文本资源"))
                bSkipBody = FALSE;      // 遇到下一个声明段，收起结束
            else
            {
                continue;
            }
        }

        if (s[0] == _T('.'))
        {
            CString rest = EcTrim(s.Mid(1));
            CString head = EcHeadWord(rest);
            CString body = EcTrim(rest.Mid(head.GetLength()));

            if (head == _T("版本"))
                continue;

            // 文件夹只是程序树的分组写法（原版是 IDE 里建的），编辑器里不画
            if (head == _T("文件夹"))
                continue;

            // ---------------------------------------------------------------- 「表页」
            // 原版把程序数据里的每个声明段做成**一个独立的文档页**（点程序树里的节点打开，
            // 标题就是页名）。实拍基准（tools\probe_page2m.ps1，真鼠标点树节点）：
            //   shots\live_page_gvar\view_全局变量.png            -> [全局变量表]
            //   shots\live_pg_自定义数据类型\view_自定义数据类型.png -> [自定义数据类型表]
            //   shots\live_pg_常量表___\view_常量表....png          -> [常量数据表]
            //   shots\live_pg_图片或图片组___\...                   -> [图片资源表]
            //   shots\live_pg_声音___\...                           -> [声音资源表]
            // 本工程只有一个编辑器窗口，所以这里按 m_nKind 过滤：**不是本页的声明一律不建行**，
            // 于是「表页」= 同一份源码的一段。
            // 量出来的列与配色（三张页表头底色都是 #E6EDE4 = EC_C_FILL_SUB）：
            //   全局变量表  全局变量名(#000000) 类 型(#0000FF) 数 组(✔#800000) 公 开(✔) 备 注(#008000)
            //   数据类型表  数据类型名(#000080) 公 开(✔) 备 注；成员表 成员名(#000000) 类 型(#0000FF)
            //               传 址(✔) 数 组(✔) 备 注
            //   常量数据表  常量名称(#000080) 常量值(数值 #800000 / 文本 #008080) 公 开(✔) 备 注
            //   图片/声音表 名称(#000080) 内 容(#800000) 公 开(✔) 备 注
            if (m_nKind >= ECK_GVAR)
            {
                if (m_nKind == ECK_GVAR && head == _T("全局变量"))
                {
                    EcParseDecl(body, tok, &seps);
                    if (nTable < 0)
                    {
                        nTable = (int)m_tables.size();
                        EcTable t; t.cols = 5; t.kind = ECT_BLK; t.w.assign(5, 0);
                        m_tables.push_back(t);
                        LPCTSTR hdr[5] = { _T("全局变量名"), _T("类 型"), _T("数组"),
                                           _T("公开"), _T("备 注") };
                        int hc[5] = { EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                        AddTableRow(nTable, 5, EC_C_FILL_SUB, hdr, hc, NULL, NULL, NULL, 5, i);
                    }
                    CString nm = tok.size() > 0 ? tok[0] : _T("");
                    CString ty = tok.size() > 1 ? tok[1] : _T("");
                    // 文本形式的位置语义（refs\OpenEpl 的 EplParser.g4 globalVariableElem）：
                    //   名, 类型, 公开, 数组, 备注   —— 注意**表格把「数组」画在「公开」前面**
                    BOOL bPub = (tok.size() > 2 && !tok[2].IsEmpty());
                    BOOL bArr = (tok.size() > 3 && !tok[3].IsEmpty());
                    CString note = (tok.size() > 4) ? EcJoinFrom(tok, 4, &seps) : _T("");
                    LPCTSTR dat[5] = { nm, ty, _T(""), _T(""), note };
                    int  dc[5] = { EC_C_TEXT, EC_C_KEY, EC_C_CMD, EC_C_CMD, EC_C_CMT };
                    BOOL ck[5] = { FALSE, FALSE, bArr, bPub, FALSE };
                    AddTableRow(nTable, 5, -1, dat, dc, NULL, ck, NULL, 5, i);
                    continue;
                }

                if (m_nKind == ECK_TYPE && head == _T("数据类型"))
                {
                    EcParseDecl(body, tok, &seps);
                    nTable = (int)m_tables.size();
                    EcTable t; t.cols = 3; t.kind = ECT_BLK; t.w.assign(3, 0);
                    m_tables.push_back(t);
                    nMember = -1;               // 下一个 .成员 起一张新表
                    LPCTSTR hdr[3] = { _T("数据类型名"), _T("公开"), _T("备 注") };
                    int hc[3] = { EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                    AddTableRow(nTable, 3, EC_C_FILL_SUB, hdr, hc, NULL, NULL, NULL, 3, i);
                    CString nm = tok.size() > 0 ? tok[0] : _T("");
                    BOOL bPub = (tok.size() > 1 && !tok[1].IsEmpty());
                    CString note = (tok.size() > 2) ? EcJoinFrom(tok, 2, &seps) : _T("");
                    LPCTSTR dat[3] = { nm, _T(""), note };
                    int  dc[3] = { EC_C_PUNCT, EC_C_CMD, EC_C_CMT };
                    BOOL ck[3] = { FALSE, bPub, FALSE };
                    AddTableRow(nTable, 3, -1, dat, dc, NULL, ck, NULL, 3, i);
                    continue;
                }

                if (m_nKind == ECK_TYPE && head == _T("成员"))
                {
                    EcParseDecl(body, tok, &seps);
                    if (nMember < 0)
                    {
                        nMember = (int)m_tables.size();
                        // 成员子表紧贴在类型行下面（实测两张表之间**没有**空隙），
                        // 所以它的类目用 ECT_MEM（Layout 里不加空隙）
                        EcTable t; t.cols = 5; t.kind = ECT_MEM; t.w.assign(5, 0);
                        m_tables.push_back(t);
                        LPCTSTR hdr[5] = { _T("成员名"), _T("类 型"), _T("传址"),
                                           _T("数组"), _T("备 注") };
                        int hc[5] = { EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                        AddTableRow(nMember, 5, EC_C_FILL_SUB, hdr, hc, NULL, NULL, NULL, 5, i);
                    }
                    // 文本形式（structMemberElem）：名, 类型, 传址, 数组, 备注
                    CString nm = tok.size() > 0 ? tok[0] : _T("");
                    CString ty = tok.size() > 1 ? tok[1] : _T("");
                    BOOL bRef = (tok.size() > 2 && !tok[2].IsEmpty());
                    BOOL bArr = (tok.size() > 3 && !tok[3].IsEmpty());
                    CString note = (tok.size() > 4) ? EcJoinFrom(tok, 4, &seps) : _T("");
                    LPCTSTR dat[5] = { nm, ty, _T(""), _T(""), note };
                    int  dc[5] = { EC_C_TEXT, EC_C_KEY, EC_C_CMD, EC_C_CMD, EC_C_CMT };
                    BOOL ck[5] = { FALSE, FALSE, bRef, bArr, FALSE };
                    AddTableRow(nMember, 5, -1, dat, dc, NULL, ck, NULL, 5, i);
                    continue;
                }

                if (m_nKind == ECK_CONST && head == _T("常量"))
                {
                    EcParseDecl(body, tok, &seps);
                    if (nTable < 0)
                    {
                        nTable = (int)m_tables.size();
                        EcTable t; t.cols = 4; t.kind = ECT_BLK; t.w.assign(4, 0);
                        m_tables.push_back(t);
                        LPCTSTR hdr[4] = { _T("常量名称"), _T("常量值"), _T("公开"), _T("备 注") };
                        int hc[4] = { EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                        AddTableRow(nTable, 4, EC_C_FILL_SUB, hdr, hc, NULL, NULL, NULL, 4, i);
                    }
                    // 文本形式（constantElem）：名, 值, 公开, 备注
                    CString nm = tok.size() > 0 ? tok[0] : _T("");
                    CString val = (tok.size() > 1) ? tok[1] : _T("");
                    int     cv  = EC_C_CMD;         // 数值：实测 #800000（= EC_C_CMD）
                    if (val.GetLength() >= 2 && val[0] == _T('"') &&
                        val[val.GetLength() - 1] == _T('"'))
                    {
                        CString inner = val.Mid(1, val.GetLength() - 2);
                        cv = EC_C_STR;              // 文本：实测 #008080（= EC_C_STR）
                        // 原版对**长**文本常量不画内容，画 `<文本长度： N>`（实测
                        // `<文本长度： 1339>` / `<文本长度： 64759>`）。阈值没定死，先取 60。
                        if (inner.GetLength() > 60)
                            val.Format(_T("<文本长度： %d>"), inner.GetLength());
                    }
                    BOOL bPub = (tok.size() > 2 && !tok[2].IsEmpty());
                    CString note = (tok.size() > 3) ? EcJoinFrom(tok, 3, &seps) : _T("");
                    LPCTSTR dat[4] = { nm, val, _T(""), note };
                    int  dc[4] = { EC_C_PUNCT, cv, EC_C_CMD, EC_C_CMT };
                    BOOL ck[4] = { FALSE, FALSE, bPub, FALSE };
                    AddTableRow(nTable, 4, -1, dat, dc, NULL, ck, NULL, 4, i);
                    continue;
                }

                if ((m_nKind == ECK_IMG && head == _T("图片资源")) ||
                    (m_nKind == ECK_SND && head == _T("声音资源")))
                {
                    EcParseDecl(body, tok, &seps);
                    if (nTable < 0)
                    {
                        nTable = (int)m_tables.size();
                        EcTable t; t.cols = 4; t.kind = ECT_BLK; t.w.assign(4, 0);
                        m_tables.push_back(t);
                        LPCTSTR hdr[4] = { (m_nKind == ECK_IMG) ? _T("图片或图片组名称")
                                                                : _T("声音名称"),
                                           _T("内容"), _T("公开"), _T("备 注") };
                        int hc[4] = { EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                        AddTableRow(nTable, 4, EC_C_FILL_SUB, hdr, hc, NULL, NULL, NULL, 4, i);
                    }
                    // 文本形式（imageResourceElem / soundResourceElem）：名, 公开, 备注
                    // ⚠️「内 容」列原版画的是**二进制里那份资源的字节数**，文本形式里没有这个信息，
                    //    所以本工程这一列先空着（见 HANDOFF 第 6 节）
                    CString nm = tok.size() > 0 ? tok[0] : _T("");
                    BOOL bPub = (tok.size() > 1 && !tok[1].IsEmpty());
                    CString note = (tok.size() > 2) ? EcJoinFrom(tok, 2, &seps) : _T("");
                    LPCTSTR dat[4] = { nm, _T(""), _T(""), note };
                    int  dc[4] = { EC_C_PUNCT, EC_C_CMD, EC_C_CMD, EC_C_CMT };
                    BOOL ck[4] = { FALSE, FALSE, bPub, FALSE };
                    AddTableRow(nTable, 4, -1, dat, dc, NULL, ck, NULL, 4, i);
                    continue;
                }

                continue;       // 「表页」里其它声明一概不显示
            }

            if (head == _T("程序集"))
            {
                nTable = (int)m_tables.size();
                EcTable t; t.cols = 4; t.kind = ECT_ASM; t.w.assign(4, 0);
                m_tables.push_back(t);
                nVarTable = -1; bParamHdr = FALSE; bDll = FALSE;

                CString h0 = (body.Find(_T("窗口")) >= 0) ? _T("窗口程序集名") : _T("程序集名");
                // ⚠️ 两个「保留」列的表头**不一样**：原版实测两列宽 54 / 42（见 HANDOFF §5 第 5 条），
                //    差的一个 12px 就是这个字体里两个空格的宽度 —— 所以第一列是三个空格、第二列一个。
                LPCTSTR hdr[4] = { h0, _T("保   留"), _T("保 留"), _T("备 注") };
                int  hc[4] = { EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                // 原版实测（shots\live_tabs\view0.png y=4..25、shots\live_fold_jy\view_before.png
                // y=5..25）：当前程序集那张表的**表头行**整行是 #ECECEC（不是白底，也不是
                // 子程序表那种 #E6EDE4）—— 这是「当前程序集」的高亮底色；**数据行是白底**。
                AddTableRow(nTable, 4, EC_C_GUTTER, hdr, hc, NULL, NULL, NULL, 4, i);

                LPCTSTR dat[4] = { body, _T(""), _T(""), _T("") };
                int  dc2[4] = { EC_C_PUNCT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                BOOL un[4]  = { TRUE, FALSE, FALSE, FALSE };
                AddTableRow(nTable, 4, -1, dat, dc2, un, NULL, NULL, 4, i);
                continue;
            }

            if (head == _T("子程序"))
            {
                EcParseDecl(body, tok, &seps);
                CString name = tok.size() > 0 ? tok[0] : _T("");
                CString ret  = tok.size() > 1 ? tok[1] : _T("");
                CString note;
                BOOL bPub = FALSE, bPack = FALSE;
                for (int k = 2; k < (int)tok.size(); k++)
                {
                    if (tok[k] == _T("公开")) bPub = TRUE;
                    else if (tok[k] == _T("易包")) bPack = TRUE;
                    else if (!tok[k].IsEmpty() && note.IsEmpty()) note = EcJoinFrom(tok, k, &seps);
                }

                nTable = (int)m_tables.size();
                EcTable t; t.cols = 6; t.kind = ECT_SUB; t.w.assign(6, 0);
                m_tables.push_back(t);
                nVarTable = -1; bParamHdr = FALSE; bDll = FALSE;
                // 「收缩」状态：这个子程序被收起时，它后面的数据行整段不显示
                // （原版「编辑->收缩展开/屏蔽->收缩」= 命令 #32927 的行为）
                if (IsCollapsed(name))
                    bSkipBody = TRUE;

                LPCTSTR hdr[5] = { _T("子程序名"), _T("返回值类型"), _T("公开"), _T("易包"), _T("备 注") };
                int  span[5] = { 1, 1, 1, 1, 2 };
                int  hc[5] = { EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                AddTableRow(nTable, 6, EC_C_FILL_SUB, hdr, hc, NULL, NULL, span, 5, i);

                LPCTSTR dat[5] = { name, ret, _T(""), _T(""), note };
                int  dc2[5] = { EC_C_PUNCT, EC_C_KEY, EC_C_CMD, EC_C_CMD, EC_C_CMT };
                BOOL un[5]  = { TRUE, FALSE, FALSE, FALSE, FALSE };
                BOOL ck[5]  = { FALSE, FALSE, bPub, bPack, FALSE };
                int  sp2[5] = { 1, 1, 1, 1, 2 };
                AddTableRow(nTable, 6, -1, dat, dc2, un, ck, sp2, 5, i);
                m_rows[m_rows.size() - 1].mark = ECM_FOLD;
                bNeedDown = TRUE;
                continue;
            }

            // Dll命令块。原版实测（shots\ref_dll_page.png，用户实例截图 1:1）：
            //   一个块 = 一张 5 列表，行依次是
            //     表头   Dll命令名 | 返回值类型 | 公开 | 备注(占 2 列，跨第 4/5 列)
            //     数据   命令名     | 返回值类型 | ✔    | 备注(占 2 列)
            //     库文件名:            (整行 1 格，占 5 列，表头底色)
            //     库文件名值           (整行 1 格)
            //     在库中对应命令名:     (整行 1 格，表头底色)
            //     在库中对应命令名值    (整行 1 格)
            //     参数表头 参数名 | 类 型 | 传址 | 数组 | 备 注
            //     参数行 …
            //   5 列宽度在整块内共用（实测：表头"备注"列的宽 = 参数表 数组列+备注列 之和）
            //   文本形式的位置语义：名, 返回值类型, "库文件名", "在库中对应命令名", 公开, 备注
            if (head == _T("DLL命令"))
            {
                EcParseDecl(body, tok, &seps);
                CString name = tok.size() > 0 ? tok[0] : _T("");
                CString ret  = tok.size() > 1 ? tok[1] : _T("");
                CString lib  = EcUnquote(tok.size() > 2 ? tok[2] : _T(""));
                CString ent  = EcUnquote(tok.size() > 3 ? tok[3] : _T(""));
                BOOL    bPub = FALSE;
                CString note;
                for (int k = 4; k < (int)tok.size(); k++)
                {
                    if (tok[k] == _T("公开")) bPub = TRUE;
                    else if (!tok[k].IsEmpty() && note.IsEmpty()) note = EcJoinFrom(tok, k, &seps);
                }

                nTable = (int)m_tables.size();
                EcTable t; t.cols = 5; t.kind = ECT_DLL; t.w.assign(5, 0);
                m_tables.push_back(t);
                nVarTable = -1; bParamHdr = FALSE; bDll = TRUE;

                int  sp4[4] = { 1, 1, 1, 2 };            // 备注占后 2 列（原版实测）
                int  hc[4]  = { EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                LPCTSTR hdr[4] = { _T("Dll命令名"), _T("返回值类型"), _T("公开"), _T("备 注") };
                AddTableRow(nTable, 5, EC_C_FILL_SUB, hdr, hc, NULL, NULL, sp4, 4, i);

                LPCTSTR dat[4] = { name, ret, _T(""), note };
                int  dc2[4] = { EC_C_PUNCT, EC_C_KEY, EC_C_CMD, EC_C_CMT };
                BOOL ck[4]  = { FALSE, FALSE, bPub, FALSE };
                AddTableRow(nTable, 5, -1, dat, dc2, NULL, ck, sp4, 4, i);

                int  sp5[1] = { 5 };                     // 整行 1 格
                int  vc[1]  = { EC_C_CMD };              // 库文件名/命令名 = 成员方法名颜色
                LPCTSTR l1[1] = { _T("库文件名：") };
                AddTableRow(nTable, 5, EC_C_FILL_SUB, l1, NULL, NULL, NULL, sp5, 1, i);
                LPCTSTR v1[1] = { lib };
                AddTableRow(nTable, 5, -1, v1, vc, NULL, NULL, sp5, 1, i);
                LPCTSTR l2[1] = { _T("在库中对应命令名：") };
                AddTableRow(nTable, 5, EC_C_FILL_SUB, l2, NULL, NULL, NULL, sp5, 1, i);
                LPCTSTR v2[1] = { ent };
                AddTableRow(nTable, 5, -1, v2, vc, NULL, NULL, sp5, 1, i);
                continue;
            }

            if (head == _T("参数") && nTable >= 0)
            {
                if (!bParamHdr)
                {
                    if (bDll)
                    {
                        // Dll命令的参数表：5 列，没有「可空」，第 3 列是「传址」
                        LPCTSTR hdr5[5] = { _T("参数名"), _T("类 型"), _T("传址"), _T("数组"), _T("备 注") };
                        int  hc5[5] = { EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                        AddTableRow(nTable, 5, EC_C_FILL_SUB, hdr5, hc5, NULL, NULL, NULL, 5, i);
                    }
                    else
                    {
                        LPCTSTR hdr[6] = { _T("参数名"), _T("类 型"), _T("参考"), _T("可空"), _T("数组"), _T("备 注") };
                        int  hc[6] = { EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                        AddTableRow(nTable, 6, EC_C_FILL_SUB, hdr, hc, NULL, NULL, NULL, 6, i);
                    }
                    bParamHdr = TRUE;
                }
                EcParseDecl(body, tok, &seps);
                if (bDll)
                {
                    // 位置语义：名, 类型, 传址/参考, 数组, 备注
                    CString name = tok.size() > 0 ? tok[0] : _T("");
                    CString type = tok.size() > 1 ? tok[1] : _T("");
                    BOOL bRef = (tok.size() > 2 && (tok[2] == _T("传址") || tok[2] == _T("参考")));
                    BOOL bArr = (tok.size() > 3 && tok[3] == _T("数组"));
                    CString note = EcJoinFrom(tok, 4, &seps);
                    LPCTSTR dat[5] = { name, type, _T(""), _T(""), note };
                    int  dc5[5] = { EC_C_TEXT, EC_C_KEY, EC_C_CMD, EC_C_CMD, EC_C_CMT };
                    BOOL ck5[5] = { FALSE, FALSE, bRef, bArr, FALSE };
                    AddTableRow(nTable, 5, -1, dat, dc5, NULL, ck5, NULL, 5, i);
                    continue;
                }
                // 位置语义（与原版文本形式一致）：名, 类型, 参考, 可空, 数组, 备注
                CString name = tok.size() > 0 ? tok[0] : _T("");
                CString type = tok.size() > 1 ? tok[1] : _T("");
                BOOL bRef  = (tok.size() > 2 && tok[2] == _T("参考"));
                BOOL bNull = (tok.size() > 3 && tok[3] == _T("可空"));
                CString arr  = (tok.size() > 4 && tok[4] != _T("数组")) ? tok[4] : _T("");
                BOOL bArr = (tok.size() > 4 && tok[4] == _T("数组"));
                CString note = EcJoinFrom(tok, 5, &seps);
                LPCTSTR dat[6] = { name, type, _T(""), _T(""), arr, note };
                int  dc2[6] = { EC_C_TEXT, EC_C_KEY, EC_C_CMD, EC_C_CMD, EC_C_CMT, EC_C_CMT };
                BOOL ck[6]  = { FALSE, FALSE, bRef, bNull, bArr, FALSE };
                AddTableRow(nTable, 6, -1, dat, dc2, NULL, ck, NULL, 6, i);
                continue;
            }

            if (head == _T("局部变量") || head == _T("程序集变量"))
            {
                if (nVarTable < 0)
                {
                    nVarTable = (int)m_tables.size();
                    EcTable t; t.cols = 5; t.kind = ECT_VAR; t.w.assign(5, 0);
                    m_tables.push_back(t);
                    LPCTSTR hdr[5] = { _T("变量名"), _T("类 型"), _T("静态"), _T("数组"), _T("备 注") };
                    int  hc[5] = { EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT, EC_C_TEXT };
                    AddTableRow(nVarTable, 5, EC_C_FILL_VAR, hdr, hc, NULL, NULL, NULL, 5, i);
                }
                EcParseDecl(body, tok, &seps);
                // 位置语义：名, 类型, 静态, 数组, 备注
                CString name = tok.size() > 0 ? tok[0] : _T("");
                CString type = tok.size() > 1 ? tok[1] : _T("");
                BOOL bStatic = (tok.size() > 2 && tok[2] == _T("静态"));
                CString arr  = (tok.size() > 3 && tok[3] != _T("数组")) ? tok[3] : _T("");
                BOOL bArr = (tok.size() > 3 && tok[3] == _T("数组"));
                CString note = EcJoinFrom(tok, 4, &seps);
                LPCTSTR dat[5] = { name, type, _T(""), arr, note };
                int  dc2[5] = { EC_C_TEXT, EC_C_KEY, EC_C_CMD, EC_C_CMT, EC_C_CMT };
                BOOL ck[5]  = { FALSE, FALSE, bStatic, bArr, FALSE };
                // ⚠️ 数据行**不能**填表头底色 —— 原版实测（shots\user_ref.png 的变量表：
                //    #D9E3F0 只出现在表头那一行 y=231..250，下面每一行都是白底）
                AddTableRow(nVarTable, 5, -1, dat, dc2, NULL, ck, NULL, 5, i);
                continue;
            }
            // 其它 .xxx 声明（暂不支持）当普通语句
            s = EcTrim(s.Mid(1));
        }

        nTable = -1; nVarTable = -1; bParamHdr = FALSE; bDll = FALSE;

        CString first = EcFirstIdent(s);
        BOOL bStart = EcInList(g_szBlockStart, first);
        BOOL bEnd   = EcInList(g_szBlockEnd,   first);
        BOOL bHide  = EcInList(g_szHidden,     first);

        // ------------------------------------------------------------------
        // 缩进：**原版不是「每进一层块就深一级」**。本轮拿三个原版自带样本逐行量正文左边缘
        // （tools\live_flowcmd\view0.png / live_learn2\view0.png / live_fold_jy\view_before.png），
        // 规则是：
        //   · 块首行   = 外层上下文 + 1 级
        //   · 块体的语句 = **和块首行同一级**（不再深一级）
        //   · 可见的块尾行（循环尾）= 和块首行同一级
        //   · 块结束之后 = 回到外层上下文的级别
        // 实测（流程类命令.e，正文列 46+22n）：
        //      ' 注释                46     计次循环首 68     如果真 90
        //      到循环尾              90  ← 和「如果真」同一列！
        //      画板1.滚动写行        68  ← 回到循环体那一级
        //      计次循环尾            68
        // ⚠️ 以前写成「块首用上下文级别、块体再+1」，于是整棵块结构都**浅了一级**，
        //    而且「如果真的体」比「如果真」多缩进一格 —— 用户一眼就看出来了。
        if (bEnd && !blkStack.empty())
        {
            int srow = blkStack.back();
            int nLv  = (srow >= 0 && srow < (int)m_rows.size()) ? m_rows[srow].indent : 0;
            if (bHide)
            {
                // 被隐藏的块尾（如果真结束 / 如果结束 / 判断结束）：块首行记「隐藏收尾」，
                // 块体最后一个显示行号留给 DrawAll 画 ▼ 用（原版就在那一行下面一格的位置画）
                m_rows[srow].blkEnd  = -1;
                m_rows[srow].blkLast = (int)m_rows.size() - 1;
                blkStack.pop_back();
                nContext = blkStack.empty() ? 0 : m_rows[blkStack.back()].indent;
                bNeedDown = FALSE;
                continue;
            }
            // 可见的块尾（计次循环尾 / 判断循环尾 / 循环判断尾 / 变量循环尾）：照常建行，
            // 级别和块首行一样，并把它记到块首行上
            AddCodeRow(s, i);
            m_rows[m_rows.size() - 1].indent = nLv;
            blkStack.pop_back();
            nContext = blkStack.empty() ? 0 : m_rows[blkStack.back()].indent;
            if (srow >= 0 && srow < (int)m_rows.size() - 1)
                m_rows[srow].blkEnd = (int)m_rows.size() - 1;
            // 落到下面共用的「参数归属线」判断（块尾不是支持库命令，自然不会命中）
        }
        else
        {
            if (bHide)
            {
                // 分支关键字（否则 / 默认）：原版不画，只跳过
                bNeedDown = FALSE;
                continue;
            }
            AddCodeRow(s, i);
            m_rows[m_rows.size() - 1].indent = bStart ? (nContext + 1) : nContext;
            if (bNeedDown) { m_rows[m_rows.size() - 1].mark = ECM_DOWN; bNeedDown = FALSE; }
            if (bStart)
            {
                blkStack.push_back((int)m_rows.size() - 1);
                nContext = m_rows[m_rows.size() - 1].indent;    // 块体沿用块首的级别
                // 块首行的缩进已经定好，直接进入参数归属线判断（不会命中）
            }
        }
        // 带参数的支持库命令语句：流程槽里要画 -/+，点它整篇显示/隐藏参数归属线。
        // 判据不是「叫得出名字的调用」，而是**至少有一个真的传了值的参数** ——
        // 否则 `返回 ()` / `类 类_示例` 这种零参数的也会挂上标记，原版没有。
        {
            EcCallParams cp;
            if (EcParseCall(s, cp))
            {
                std::vector<CString> nm, ty;
                std::vector<BOOL>    df;
                BOOL bVal = FALSE;
                for (int z = 0; z < (int)cp.vals.size(); z++)
                    if (!cp.vals[z].IsEmpty()) { bVal = TRUE; break; }
                if (bVal && EcCmdArgsByName(cp.cmd, &nm, &ty, &df) && !nm.empty())
                    m_rows[m_rows.size() - 1].bParam = TRUE;
            }
        }
    }

    // -/+ 折叠标记只留在「有参数或局部变量」的子程序行上 —— 原版实测两个样本：
    //   · 本工程用 tools\patch_sample_graphic.py 喂 graphic.e 的同一份源码对拍时，
    //     原版那张 `_图形按钮1_被单击` 表（无参数无变量）**没有** -/+；
    //   · 精易模块的 `外部超级列表框_保存数据_NEW`（5 个参数 + 11 个局部变量）在
    //     子程序数据行上有一个 -（shots\_ref_code.png 实测 x=22..30, y=89）。
    //   原版画在「子程序体第一条语句」上的那个 +/-（切换参数归属线）本工程还没做。
    for (size_t r2 = 0; r2 < m_rows.size(); r2++)
    {
        if (m_rows[r2].kind != 0 || m_rows[r2].mark != ECM_FOLD) continue;
        int t = m_rows[r2].table;
        BOOL bHas = FALSE;
        for (size_t k = r2 + 1; k < m_rows.size(); k++)
        {
            if (m_rows[k].kind != 0) continue;                  // 体里的代码行跳过
            if (m_rows[k].mark == ECM_FOLD) break;              // 到下一个子程序了
            if (m_rows[k].table < 0) continue;
            if (m_rows[k].table != t) { bHas = TRUE; break; }    // 变量表（另起一张表）
            if (k > r2 + 1) { bHas = TRUE; break; }              // 同一张表里的参数行
        }
        if (!bHas) m_rows[r2].mark = ECM_NONE;
    }
}

// ------------------------------------------------------------------ 取关键字/命令分类
static COLORREF EcClassify(const CString& tok)
{
    if (EcInList(g_szKeywords, tok)) return EC_C_KEY;
    int k = EcNameKind(tok);
    if (k == 2) return EC_C_KEY;     // 数据类型
    if (k == 1) return EC_C_CMD;     // 支持库命令
    return EC_C_TEXT;
}

// ------------------------------------------------------------------ 布局
void CECEditor::Layout()
{
    // 1) 列宽 = 该列单元格文字最大宽度 + 12（跨列的不参与）
    CClientDC dc(this);
    CFont* pOld = dc.SelectObject(&m_fontTable);

    size_t t, r, c;
    for (t = 0; t < m_tables.size(); t++)
    {
        EcTable& tb = m_tables[t];
        for (int c0 = 0; c0 < tb.cols; c0++) tb.w[c0] = 0;
    }
    for (r = 0; r < m_rows.size(); r++)
    {
        EcRow& row = m_rows[r];
        if (row.kind != 0 || row.table < 0) continue;
        EcTable& tb = m_tables[row.table];
        int col = 0;
        for (c = 0; c < row.cells.size() && col < tb.cols; c++)
        {
            EcCell& cell = row.cells[c];
            int need = 0;
            if (cell.span == 1)
                need = dc.GetTextExtent(cell.s).cx + 2 * EC_CELL_PAD;
            if (cell.check && need < EC_TICK_W + 2 * EC_CELL_PAD) need = EC_TICK_W + 2 * EC_CELL_PAD;
            if (cell.span == 1 && need > tb.w[col]) tb.w[col] = need;
            col += cell.span;
        }
    }
    for (t = 0; t < m_tables.size(); t++)
        for (int c1 = 0; c1 < m_tables[t].cols; c1++)
            if (m_tables[t].w[c1] < 24) m_tables[t].w[c1] = 24;

    // 跨列单元格（子程序行的备注占 2 列）：合并后必须放得下，差额补到跨区最后一列
    for (r = 0; r < m_rows.size(); r++)
    {
        EcRow& row2 = m_rows[r];
        if (row2.kind != 0 || row2.table < 0) continue;
        EcTable& tb2 = m_tables[row2.table];
        int col2 = 0;
        for (c = 0; c < row2.cells.size() && col2 < tb2.cols; c++)
        {
            EcCell& cell2 = row2.cells[c];
            if (cell2.span > 1 && !cell2.s.IsEmpty())
            {
                int need = dc.GetTextExtent(cell2.s).cx + 2 * EC_CELL_PAD;
                // 原版实测：**子程序表「备 注」这个跨列格的最小宽是 78**（HANDOFF §5 第 5 条）——
                // graphic.e 那个子程序根本没有备注、那一格是空的，原版照样是 78；
                // jy.e 有一条 17 字的备注时是 216 = 内容宽 + 12。所以规则是 max(内容宽+12, 78)。
                if (tb2.kind == ECT_SUB && need < 78)
                    need = 78;
                int have = 0;
                for (int k = 0; k < cell2.span && col2 + k < tb2.cols; k++) have += tb2.w[col2 + k];
                if (need > have)
                    tb2.w[col2 + cell2.span - 1] += (need - have);
            }
            col2 += cell2.span;
        }
    }
    dc.SelectObject(pOld);

    // 2) 纵向排布
    int y = EC_TOP_PAD;
    int prevTable = -2;
    BOOL bPrevTable = FALSE;
    for (r = 0; r < m_rows.size(); r++)
    {
        EcRow& row = m_rows[r];
        if (row.kind == 0)
        {
            if (!bPrevTable || row.table != prevTable)
            {
                if (bPrevTable)
                {
                    int nk = m_tables[row.table].kind;
                    if (nk == ECT_SUB || nk == ECT_DLL || nk == ECT_BLK)
                        y += EC_GAP_BIG;
                    else if (nk != ECT_MEM)     // 成员子表紧贴类型行：原版实测没有空隙
                        y += EC_GAP_SMALL;
                }
            }
            row.y = y;
            row.h = EC_ROW_PITCH;
            y += EC_ROW_PITCH;
            prevTable = row.table;
            bPrevTable = TRUE;
        }
        else
        {
            if (bPrevTable) { y += EC_GAP_SMALL; bPrevTable = FALSE; prevTable = -2; }
            row.y = y;
            row.h = EC_CODE_H;
            y += EC_CODE_H;
        }
    }
    m_nContentH = y + EC_GAP_SMALL;
    // 参数属线是**叠在行上**画的（原版也不给它们占纵向空间），但最后一条语句的参数行
    // 会伸出内容区，够不到就滚不到底 —— 按「最多 6 行参数」留一点余量
    m_nContentH += EC_CODE_H + ParamLineH() * 6;

    // 3) 折叠标记命中框（原版画在流程槽里，x≈21..31，与本工程的表格左边界 44 一致）
    m_foldHit.assign(m_rows.size(), CRect(0, 0, 0, 0));
    for (r = 0; r < (int)m_rows.size(); r++)
    {
        if (m_rows[r].kind != 0 || m_rows[r].mark != ECM_FOLD) continue;
        int cy = m_rows[r].y + m_rows[r].h / 2;
        m_foldHit[r] = CRect(17, cy - 7, 37, cy + 7);
    }

    // 3b) 参数归属线的 -/+ 命中框（原版实测：语句行竖直中央，与原版同一个 x 区间）
    m_paramHit.assign(m_rows.size(), CRect(0, 0, 0, 0));
    for (r = 0; r < (int)m_rows.size(); r++)
    {
        if (m_rows[r].kind != 1 || !m_rows[r].bParam) continue;
        int cy = m_rows[r].y + m_rows[r].h / 2;
        m_paramHit[r] = CRect(17, cy - 7, 37, cy + 7);
    }
}

// 参数归属线一行多高（原版实测 16px：三行行顶 117/133/149）
int CECEditor::ParamLineH() const
{
    return EC_CODE_H * 8 / 11;
}

int CECEditor::TextXOfIndent(int nIndent) const
{
    return EC_TEXT_X_OF(nIndent);
}

// ---------------------------------------------------------------- 收缩 / 展开
// 原版：子程序数据行竖直中央那个「-」/「+」（实测 x≈21..31，藏青）。
// 本工程点击后把该子程序的数据行整段隐藏（源码不动，只影响显示模型）。
void CECEditor::SetCollapsed(LPCTSTR pszSub, BOOL bCollapsed)
{
    CString s(pszSub);
    int i;
    for (i = 0; i < (int)m_setCollapsed.size(); i++)
        if (m_setCollapsed[i] == s) break;

    if (bCollapsed)
    {
        if (i >= (int)m_setCollapsed.size())
            m_setCollapsed.push_back(s);
    }
    else
    {
        if (i < (int)m_setCollapsed.size())
            m_setCollapsed.erase(m_setCollapsed.begin() + i);
    }
    Rebuild();
    Layout();
    DumpModel();            // 诊断：折叠/展开后立刻重倒一次，便于自动化比对
    InvalidateAll();
}

BOOL CECEditor::IsCollapsed(LPCTSTR pszSub) const
{
    CString s(pszSub);
    for (int i = 0; i < (int)m_setCollapsed.size(); i++)
        if (m_setCollapsed[i] == s) return TRUE;
    return FALSE;
}

// 首次交互（点鼠标 / 按键）后收掉「刚打开」的红色铅笔
void CECEditor::ClearOpenMark()
{
    if (!m_bOpenMark) return;
    m_bOpenMark = FALSE;
    Invalidate(FALSE);
}

int CECEditor::HitTestFold(CPoint pt) const
{
    CPoint p(pt.x, pt.y + m_nScroll);
    for (int r = 0; r < (int)m_foldHit.size(); r++)
        if (!m_foldHit[r].IsRectEmpty() && m_foldHit[r].PtInRect(p))
            return r;
    return -1;
}

// 点中的是「参数归属线」的 -/+ 吗（返回代码行下标，-1 = 不是）
int CECEditor::HitTestParam(CPoint pt) const
{
    CPoint p(pt.x, pt.y + m_nScroll);
    for (int r = 0; r < (int)m_paramHit.size(); r++)
        if (!m_paramHit[r].IsRectEmpty() && m_paramHit[r].PtInRect(p))
            return r;
    return -1;
}

// （第 17 轮删掉了 ComputeBlocks：它按「缩进」推块边界，块尾关键字被隐藏的那几行
//   不在显示模型里，于是把 `返回 ()` 也吞进循环体、竖线一直拖到文档尾。
//   现在块首/块尾是在 Rebuild() 里用源码关键字栈直接配对的，见 EcRow::blkEnd。）

// ------------------------------------------------------------------ 绘制
BOOL CECEditor::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CECEditor::DrawAll(CDC& dc)
{
    CRect rc;
    GetClientRect(&rc);
    dc.FillSolidRect(rc, RGB(255, 255, 255));

    // 流程槽（全高灰底）
    dc.FillSolidRect(0, 0, EC_GUTTER_W, rc.Height(), EC_C_GUTTER);
    // ⚠️ 正文区左边界那条 #888888 竖线**不是全高**的：原版实测（shots\ref_dll_page.png
    //    的 x=44 只在 y=8..259 / 268..519 / 528..… 有，块间的 8px 空隙里没有；
    //    shots\live2\view0.png 更明显 —— 表格下面整片代码区都没有这条线）。
    //    它其实是**每张表格自己的左边界**，由 DrawTableRow 逐行画，这里绝对不能再补一条。

    // 文档刚打开时，插入点所在行的流程槽里画一个红色铅笔（原版实测，见 m_bOpenMark）
    if (m_bOpenMark)
        DrawOpenMark(dc);

    // 选区底色（画在文字下面）。原版实测：底色 #000080、高度就是文字那 12px
    // （不是整行 22px），见 shots\live_drag\after.png。
    //
    // ★ 第 26 轮：**表格单元格也要涂**，而且原版涂的是**整个格子**（涂满格子矩形、
    //   格里的字变白），见 1:1 放大图 shots\zoom_orig_sel.png。本工程原来只涂 kind==1
    //   的代码行，所以用户看到的"全选只选到函数体"。
    //   表格那部分**不在这里涂**：DrawTableRow 要按"这一格是否整个落在选区里"决定
    //   涂满还是只涂文字段，并且随之把字改成白色 —— 都放在 DrawTableRow 里做。
    if (HasSel())
    {
        dc.SelectObject(&m_font);
        int l0, c0, l1, c1;
        GetSel(l0, c0, l1, c1);
        for (size_t k = 0; k < m_rows.size(); k++)
        {
            if (m_rows[k].kind != 1) continue;
            int sa = -1, sb = -1;
            if (!SelRangeOfRow((int)k, sa, sb) || sb <= sa) continue;
            int y = m_rows[k].y - m_nScroll;
            if (y + m_rows[k].h < 0 || y > rc.Height()) continue;
            int tx = TextXOfIndent(m_rows[k].indent);
            int xa = tx + dc.GetTextExtent(m_rows[k].code.Left(sa)).cx;
            int xb = tx + dc.GetTextExtent(m_rows[k].code.Left(sb)).cx;
            if (xb <= xa) xb = xa + 4;              // 空行/行尾也留一小段，看着像选中了
            dc.FillSolidRect(xa, y + 3, xb - xa, EC_FONT_H, EC_C_SELBG);
        }
    }

    dc.SetBkMode(TRANSPARENT);
    dc.SelectObject(&m_fontTable);

    // 流程控制线（画在文字下面）。原版「程序显示」页的
    // 「画程序分支流程线」(id=1135) + 「分支流程线画为虚线」(id=128) 控制。
    //
    // 几何 = 本轮对着**原版自带例程**的 1:1 截图逐像素量的，基准：
    //   shots\live_flowcmd\view0.png（`流程类命令.e`：6 个循环块 + 2 个嵌套如果真）
    //   shots\live_self\after_a.png（同一份的整窗，验证块 4/5/6 的 ►► 位置）
    //   shots\live_learn2\view0.png（如果/如果真 块 + ▼ 收尾）
    //   shots\live_fold_jy\view_before.png（如果真 块 + 参数归属线）
    // 记 x = 块首缩进级的流程列 + 1 = EC_TEXT_X_OF(nLv-1) + 1（级别 1 → 47，级别 0 → 25）：
    //   · 竖虚线：块首行中线 → 块尾行中线，3 亮 3 灭，灰 #808080（EC_CS_LINK）
    //   · 块首行：横虚线 x .. x+15（如果家族从 x+2 起，实测差 2px；循环类从 x 起）；
    //     循环类在末端再画一个 4x7 的灰实心右三角 ►（x+12 .. x+15）——如果家族没有
    //   · 块尾行**可见**（循环尾）：横虚线 x .. x+15，**不画 ►**（第 15 轮猜成有 ►，实测没有）
    //   · 块尾行**隐藏**（如果真结束 / 如果结束 / 判断结束）：竖线一路延到块体最后一行
    //     下面一格的中线，末端一个 7x4 的灰实心下三角 ▼（逐行宽度 1/7/5/3/1），
    //     那一格本身不占显示行（源码里的「结束」行不画，但高度照算）
    //   · 块体第一行：竖线左边 14/10px 两个藏青 ►（各 3px 宽 5px 高，竖直中心 = 行中线）；
    //     「第一行」= 块首行之后第一条**缩进刚好深一级、且自身不是块首**的语句
    //     （实测：块体第一条是嵌套的如果真时，►► 落在嵌套块结束后的那条语句上）
    if (g_ecTheme.opt.bDrawFlow)
    {
        static const int dw[5] = { 1, 7, 5, 3, 1 };      // ▼/◆ 逐行宽度
        static const int aw[5] = { 1, 2, 3, 2, 1 };      // ►/►► 逐行宽度（5 行高）
        static const int pw[7] = { 1, 2, 3, 4, 3, 2, 1 };// 块首行那个 ►（7 行高）
        int n = (int)m_rows.size();
        for (int i0 = 0; i0 < n; i0++)
        {
            if (m_rows[i0].kind != 1 || m_rows[i0].blkEnd == -2) continue;
            int nLv = m_rows[i0].indent;
            int x   = EC_TEXT_X_OF(nLv - 1) + 1;
            int yTop = m_rows[i0].y + 9 - m_nScroll;
            CString kw = EcFirstIdent(m_rows[i0].code);
            BOOL bLoop = EcIsLoopKw(kw);

            CPen pen(PS_SOLID, 1, EC_C_FLOW);
            CPen* pOld = dc.SelectObject(&pen);
            if (m_rows[i0].blkEnd >= 0)
            {
                int i1 = m_rows[i0].blkEnd;
                int yBot = m_rows[i1].y + 9 - m_nScroll;
                EcFlowLine(dc, x, yTop, x, yBot);
                EcFlowLine(dc, x + (bLoop ? 0 : 2), yTop, x + 15, yTop);
                EcFlowLine(dc, x, yBot, x + 15, yBot);
                dc.SelectObject(pOld);
                if (bLoop)                                  // 块首行的 ►（块尾行没有）
                    for (int r = 0; r < 7; r++)
                        dc.FillSolidRect(x + 12, yTop - 3 + r, pw[r], 1, EC_C_FLOW);
            }
            else
            {
                int iLast = m_rows[i0].blkLast;
                if (iLast < i0) iLast = i0;
                int yBot = m_rows[iLast].y + EC_CODE_H - 1 - m_nScroll;
                EcFlowLine(dc, x, yTop, x, yBot);
                EcFlowLine(dc, x + 2, yTop, x + 15, yTop);
                dc.SelectObject(pOld);
                for (int r = 0; r < 5; r++)                 // ▼ 收尾
                    dc.FillSolidRect(x - dw[r] / 2, yBot - 2 + r, dw[r], 1, EC_C_FLOW);
            }

            // ►►：块体第一条「直接语句」。⚠️ 新的缩进语义下，**块体的直接语句和块首行同级**
            // （见 Rebuild 里的缩进说明），嵌套块的块首才会深一级 —— 所以要找的是
            // 「块首行之后第一条 indent == 块首 indent、且自身不是块首关键字」的语句。
            // ⚠️ 只有**跨度 ≥ 3 个显示行**的块才画 —— 实测（本轮拍的原版自带例程）：
            //   · `如果真 (…)` 体里只有 1 条语句 → 没 ►►
            //     （shots\live_learn2\view0.png 的 4 个如果/如果真、
            //      shots\live_flowcmd\view0.png 里嵌在循环里的那个如果真）
            //   · `如果真 (…)` 体里有 2 条语句 → 第一条上有一个 ►►
            //     （shots\live_fold_jy\view_before.png 的 `如果真（是否为调试版（））`，y=322..326）
            //   · 循环块 `首+体+尾` 天然 3 行 → 总是有
            int iEnd = (m_rows[i0].blkEnd >= 0) ? m_rows[i0].blkEnd : m_rows[i0].blkLast;
            if (iEnd - i0 < 2) continue;
            int iA = -1;
            for (int j = i0 + 1; j < iEnd && j < n && m_rows[j].kind == 1; j++)
            {
                if (m_rows[j].indent < nLv) break;
                if (m_rows[j].indent == nLv &&
                    !EcInList(g_szBlockStart, EcFirstIdent(m_rows[j].code)))
                { iA = j; break; }
            }
            if (iA < 0) continue;
            int yA = m_rows[iA].y + 9 - m_nScroll;
            for (int t = 0; t < 2; t++)
                for (int r = 0; r < 5; r++)
                    dc.FillSolidRect(x - 14 + t * 4, yA - 2 + r, aw[r], 1, EC_C_PUNCT);
        }
    }

    int nFirst = 0, nLast = (int)m_rows.size() - 1;
    int r0;
    for (r0 = 0; r0 <= nLast; r0++)
    {
        int y = m_rows[r0].y - m_nScroll;
        if (y + m_rows[r0].h < 0) { nFirst = r0 + 1; continue; }
        if (y > rc.Height()) break;
        if (m_rows[r0].kind == 0) DrawTableRow(dc, r0);
        else                      DrawCodeRow(dc, r0);
    }
    (void)nFirst;

    // 子程序数据行竖直中央的「-」/「+」（点击=收缩/展开；原版实测 x≈21..31）
    for (size_t k2 = 0; k2 < m_foldHit.size(); k2++)
    {
        if (m_foldHit[k2].IsRectEmpty()) continue;
        CRect rcFold = m_foldHit[k2];
        // 原版 1:1 实测（shots\live_fold\view_before.png）：横杠 x=22..30（9px、藏青 #000080），
        // 「+」的竖杠在 x=26（正中），整条画在白底上
        int cx = rcFold.left + 5, cy = (rcFold.top + rcFold.bottom) / 2 - m_nScroll;
        if (cy < -8 || cy > rc.Height() + 8) continue;

        // 从该行单元格里取子程序名，判断当前是展开还是收起
        CString sName;
        if (m_rows[k2].cells.size() > 0) sName = m_rows[k2].cells[0].s;
        // 子程序名前面有下划线（原版表格里的名称带下划线），取纯名字
        BOOL bCollapsed = IsCollapsed(sName);

        CPen pen(PS_SOLID, 1, EC_C_PUNCT);
        CPen* pOld = dc.SelectObject(&pen);
        dc.MoveTo(cx, cy); dc.LineTo(cx + 8, cy);          // 横杠（9px）
        if (bCollapsed)                                     // 收起时补一竖 -> 9x9「+」
        {
            dc.MoveTo(cx + 4, cy - 4);
            dc.LineTo(cx + 4, cy + 4);
        }
        dc.SelectObject(pOld);
    }

    // 代码行上的「参数归属线」-/+：原版实测（shots\live_fold\view_before/after.png）
    // 画在语句行竖直中央、x=22..30，点它整篇显示/隐藏参数行；展开态是 9px 横杠（-）。
    for (size_t k3 = 0; k3 < m_paramHit.size(); k3++)
    {
        if (m_paramHit[k3].IsRectEmpty()) continue;
        int cx = m_paramHit[k3].left + 5;
        int cy = (m_paramHit[k3].top + m_paramHit[k3].bottom) / 2 - m_nScroll;
        if (cy < -8 || cy > rc.Height() + 8) continue;
        CPen pen(PS_SOLID, 1, EC_C_PUNCT);
        CPen* pOld = dc.SelectObject(&pen);
        dc.MoveTo(cx, cy); dc.LineTo(cx + 8, cy);
        if (!m_bParamShow)                                  // 收起时补一竖 -> 9x9「+」
        {
            dc.MoveTo(cx + 4, cy - 4);
            dc.LineTo(cx + 4, cy + 4);
        }
        dc.SelectObject(pOld);
    }

    // 参数行本身最后画（叠在下面的行上，和原版一致）
    DrawParamLines(dc);
    // 多行注释的绿框（画在最后：它的竖线落在注释文字区里，不能被后面的绘制盖掉）
    DrawCommentBoxes(dc);
}

// 「参数归属线」：语句下面逐行画「※参数名：值」+ 左侧的连接虚线。
// 画在**所有行之后**（覆盖在上层）——原版也不给这些行占纵向空间，只是叠在下面的行上。
// 「参数归属线」：语句下面逐行画「※参数名：值」+ 左侧的连接虚线。
// 画在**所有行之后**（覆盖在上层）——原版也不给这些行占纵向空间，只是叠在下面的行上。
// ---------------------------------------------------------------- 多行注释的绿框
// 原版把「连续 ≥2 行的注释」框起来（单行注释不画 —— 实证：graphic.e 的
// shots\live_tabs\view0.png 与 shots\user_ref_code2.png 绿像素都是 0）。
// 几何全部是逐像素量的，基准 = shots\live_fold_jy\view_before.png（原版打开精易模块 jy.e
// 的「程序集: 调试」页 1:1 客户区，那里有 11 行 `'` 注释、其中 5 行有文字）：
//   · 颜色 #008000（= EC_C_CMT，整块绿像素 21957 个）
//   · 左竖线 x=63..64（= 正文列 46 + 17），2px 宽的**虚线段**：每行一段
//     [行文字顶, 行文字顶+11]，被框的上下边界裁掉（实测第一段 106..112、最后一段 281..287）
//   · 横线 2px 高，x=63..868，**11 亮 1 灭**（实测第一段 6px：63..68，然后 70..80、82..92…）
//   · 横线只画在**空注释行**的行顶 +5 处（实测 106/142/178/214/250/286 = 每个空 `'` 行顶 +5）；
//     整段没有空行时按「框住整段」画上下两条（规则没拍到第二个样本，见 HANDOFF 第 6 节）
//   · 右竖线 x = 本段最长那行的文字右边缘 + 3（实测 866+2 = 867..868）
// ⚠️ 本工程把右竖线按**整框**画满（原版实测只在空行那几段有）；差别在右侧 2px 的断续上。
static BOOL EcIsCommentCode(const CString& s)
{
    int i = 0;
    while (i < s.GetLength() && ((unsigned char)s[i] == ' ' || (unsigned char)s[i] == '\t')) i++;
    return i < s.GetLength() && s[i] == _T('\'');
}

void CECEditor::DrawCommentBoxes(CDC& dc)
{
    int n = (int)m_rows.size();
    if (n <= 0) return;
    CFont* pOld = dc.SelectObject(&m_font);
    int i = 0;
    while (i < n)
    {
        if (m_rows[i].kind != 1 || !EcIsCommentCode(m_rows[i].code)) { i++; continue; }
        int a = i;
        while (i < n && m_rows[i].kind == 1 && EcIsCommentCode(m_rows[i].code)) i++;
        int b = i - 1;
        if (b - a + 1 < 2) continue;                    // 单行注释不画

        // 段内每行的文字右边缘（屏幕坐标）与本段最宽的右边
        int xL = TextXOfIndent(m_rows[a].indent) + 17;
        int xR = xL + 40, k;
        std::vector<int> right(b - a + 1, 0);
        BOOL bHasBlank = FALSE;
        for (k = a; k <= b; k++)
        {
            int tx = TextXOfIndent(m_rows[k].indent);
            int w = dc.GetTextExtent(m_rows[k].code).cx;
            right[k - a] = tx + w;
            if (right[k - a] + 3 > xR) xR = right[k - a] + 3;
            if (m_rows[k].code == _T("'")) bHasBlank = TRUE;
        }

        // 横线的 y（显示坐标）
        std::vector<int> hy;
        if (bHasBlank)
        {
            for (k = a; k <= b; k++)
                if (m_rows[k].code == _T("'"))
                    hy.push_back(m_rows[k].y + 8 - m_nScroll);
        }
        else
        {
            hy.push_back(m_rows[a].y - 10 - m_nScroll);
            hy.push_back(m_rows[b].y + 26 - m_nScroll);
        }
        if (hy.size() < 2) { dc.SelectObject(pOld); continue; }
        int yTop = hy[0], yBot = hy[hy.size() - 1] + 2;

        CRect rc;
        GetClientRect(&rc);
        if (yBot < 0 || yTop > rc.Height()) { dc.SelectObject(pOld); continue; }

        // 横线：11 亮 1 灭（相位按实测：起点后第 7 个像素开始亮 11）
        for (size_t q = 0; q < hy.size(); q++)
        {
            int y = hy[q];
            if (y + 1 < 0 || y > rc.Height()) continue;
            for (int x = xL; x <= xR; x++)
                if (((x - xL + 5) % 12) != 11)
                    dc.FillSolidRect(x, y, 1, 2, EC_C_CMT);
        }
        // 左/右竖线：按行分段（每段 [行文字顶, +11]，被框高裁掉）
        for (k = a; k <= b; k++)
        {
            int y0 = m_rows[k].y + 3 - m_nScroll;
            int y1 = m_rows[k].y + 14 - m_nScroll;
            if (y0 < yTop) y0 = yTop;
            if (y1 > yBot) y1 = yBot;
            if (y1 <= y0) continue;
            dc.FillSolidRect(xL, y0, 2, y1 - y0, EC_C_CMT);
            dc.FillSolidRect(xR - 1, y0, 2, y1 - y0, EC_C_CMT);
        }
    }
    dc.SelectObject(pOld);
}

void CECEditor::DrawParamLines(CDC& dc)
{
    if (!m_bParamShow || !g_ecTheme.opt.bDrawParamLine) return;

    CRect rc;
    GetClientRect(&rc);
    int   lh = ParamLineH();
    COLORREF crLink = EC_C_FLOW, crStar = EC_C_PUNCT, crName = EC_C_PNAME;

    for (int r = 0; r < (int)m_rows.size(); r++)
    {
        if (m_rows[r].kind != 1 || !m_rows[r].bParam) continue;

        EcCallParams cp;
        if (!EcParseCall(m_rows[r].code, cp)) continue;
        std::vector<CString> nm, ty;
        std::vector<BOOL>    df;
        int nLib = 0;
        if (EcCmdArgsByName(cp.cmd, &nm, &ty, &df))
            nLib = (int)nm.size();

        // 原版的参数名就来自支持库；行数 = **实际写了几个参数槽**：
        //   本来只写到第 m 个（尾巴上的空槽不算），但如果最后一个参数后面还留着
        //   分隔符（`信息框（"…", 0, ）` 这样），那第 3 个槽是作者显式留的，要画出来
        //   —— 原版 view_after.png 三行（提示信息/按钮/窗口标题）就是这么来的；
        //   实测信息框 在 krnln 里声明了 4 个参数，第 4 个（父窗口）没写、不画。
        int m = (int)cp.vals.size();
        if (nLib > 0 && m > nLib) m = nLib;
        if (!cp.bLastSep)
            while (m > 0 && cp.vals[m - 1].IsEmpty()) m--;
        if (m <= 0) continue;
        if (nLib <= 0) continue;                     // 库里查不到这个名字 -> 只认 -/+，不编参数名

        int xStmt = EC_TEXT_X_OF(m_rows[r].indent);
        int xStar = xStmt + 44;                      // 原版实测 46 + 44 = 90/91
        int xLine = xStmt + 31;                      // 原版实测 46 + 31 = 77
        int y0    = m_rows[r].y + 16;                // 第一行参数的行顶（原版实测 100 -> 117）
        // ⚠️ 不是「语句行底 + 若干」：行高是 22 而参数行距是 16，别把两者混着算
        //    （写 y+22 会让三行参数整体低一整行 —— 这个坑真踩过）

        CPen pen(PS_SOLID, 1, crLink);
        CPen* pOld = dc.SelectObject(&pen);
        // 竖虚线：从第一行参数的行顶往下 3 亮 3 灭（与流程线同一节奏），
        // 一直落到最后一行参数往下第 4 行（原版实测：x=77 上 y=121..157 都是这个节奏）
        // 行顶 y0 = 语句行顶 + 16（原版实测 100 -> 117），行距 16px，文字再往下 4px
        // （原版 view_after.png：三行文字的字形顶在 121/137/153，行高 12）
        // 竖虚线从行基线往下 3px（= 121）到末行往下 3px，节奏交给 EcFlowLine 的 3 亮 3 灭
        int yTop = y0 + 3 - m_nScroll;
        int yBot = y0 + 3 + lh * (m - 1) + 5 - m_nScroll;
        if (yBot > yTop) EcFlowLine(dc, xLine, yTop, xLine, yBot);
        for (int kk = 0; kk < m; kk++)
        {
            int yb = y0 + 5 + lh * kk - m_nScroll;     // 该行的横虚线
            dc.MoveTo(xLine, yb);
            dc.LineTo(xLine + 3, yb);
            dc.MoveTo(xLine + 6, yb);                  // 同一条 3/3 被 ※ 的字形遮掉中间一段
            dc.LineTo(xLine + 8, yb);
        }
        dc.SelectObject(pOld);

        // 连接线的顶端一个「▼」（原版实测 shots\live_fold\view_after.png：x=72..80 的
        // y=114..117 是个上宽下尖的小三角，竖直中心就在竖线上）—— 这个是原版行为，
        // 不是本工程补的装饰
        for (int t = 0; t < 4; t++)
            dc.FillSolidRect(xLine - t, y0 - 4 + t - m_nScroll, 1 + 2 * t, 1, crLink);

        for (int k = 0; k < m; k++)
        {
            int y = y0 + lh * k + 4 - m_nScroll;      // 文字比行顶低 4px（原版实测）
            if (y + lh < 0 || y > rc.Height()) continue;
            int x = xStar;
            dc.SetTextColor(crStar);
            dc.TextOut(x, y, _T("※"));
            x += dc.GetTextExtent(_T("※")).cx;
            dc.SetTextColor(crName);
            CString sName = (k < (int)nm.size()) ? nm[k] : CString(_T(""));
            CString sLabel = sName + _T("：");
            dc.TextOut(x, y, sLabel);
            x += dc.GetTextExtent(sLabel).cx;
            // 值本身沿用代码行的词法着色（字符串 #008080、数值…），原版也是这么画的
            CString sVal = _T(" ") + cp.vals[k] + _T(" ");
            dc.SetTextColor(EC_C_TEXT);
            TokenizeDraw(dc, sVal, x, y);
        }
    }
}

void CECEditor::DrawCheck(CDC& dc, int x, int y){
    // 原版实测：✔ 是一个 9x7 的实心勾（不是 1px 折线），颜色 = 单元格文字色
    for (int r = 0; r < EC_TICK_H; r++)
        for (int c = 0; c < EC_TICK_W; c++)
            if (g_szTick[r][c] == '#')
                dc.FillSolidRect(x + c, y + r, 1, 1, EC_C_CMD);
}

void CECEditor::DrawTableRow(CDC& dc, int nRow)
{
    EcRow& row = m_rows[nRow];
    if (row.table < 0) return;
    EcTable& tb = m_tables[row.table];
    int y = row.y - m_nScroll;

    // 填充 + 网格（原版「程序显示」页的「画表格线」(id=1155)、
    // 「表格线画为虚线」(id=129) 控制样式）
    //
    // ★ 第 26 轮：整行都在选区里时，**空单元格也要涂满**。
    //   原版 Ctrl+A 的反白是"整张表涂满"（1:1 放大基准 shots\zoom_orig_sel.png：
    //   数据行后面那几个空格子也是纯藏青）。只涂有字的格会在表格中间留下一排白块。
    int l0 = 0, c0 = 0, l1 = 0, c1 = 0;
    BOOL bHasSel = HasSel();
    if (bHasSel)
        GetSel(l0, c0, l1, c1);
    // 本行整行落在选区里？（合成表头 src<0，同样按"整行选中"处理）
    BOOL bRowAllSel = FALSE;
    if (bHasSel)
    {
        if (row.src < 0)
            bRowAllSel = TRUE;
        else if (row.src > l0 && row.src < l1)
            bRowAllSel = TRUE;
        else if (row.src == l0 && row.src == l1 && c0 == 0 &&
                 c1 >= m_lines[row.src].GetLength())
            bRowAllSel = TRUE;
    }

    int x = EC_TEXT_X;
    int col = 0;
    size_t ci;
    // 只有「本表格的最后一行」才画下边线，否则每个行边界会出现两条相邻的线
    BOOL bLastRow = (nRow + 1 >= (int)m_rows.size()) ||
                    (m_rows[nRow + 1].kind != 0) ||
                    (m_rows[nRow + 1].table != row.table);
    int nVH = bLastRow ? EC_ROW_PITCH - 1 : EC_ROW_PITCH;
    for (ci = 0; ci < row.cells.size() && col < tb.cols; ci++)
    {
        int w = 0;
        for (int k = 0; k < row.cells[ci].span && col + k < tb.cols; k++) w += tb.w[col + k];
        if (row.fill >= 0)
            dc.FillSolidRect(x + 1, y + 1, w - 1, EC_CELL_H, (COLORREF)row.fill);
        if (bRowAllSel && row.cells[ci].s.IsEmpty())
            dc.FillSolidRect(x + 1, y + 1, w - 1, EC_CELL_H, EC_C_SELBG);
        if (g_ecTheme.opt.bDrawGrid)
        {
            EcVLine(dc, x, y, nVH);
            EcHLine(dc, x + 1, y, w - 1);
        }
        x += w;
        col += row.cells[ci].span;
    }
    if (g_ecTheme.opt.bDrawGrid)
    {
        EcVLine(dc, x, y, nVH);
        if (bLastRow)
            EcHLine(dc, EC_TEXT_X, y + EC_ROW_PITCH - 1, x - EC_TEXT_X + 1);
    }

    // 文字（+ 选区反白）
    //
    // ★ 第 26 轮：原版的表格反白是**整格涂满 #000080、格里的字变白**（1:1 放大基准
    //   shots\zoom_orig_sel.png：程序集表/子程序表的表头格和数据格整个矩形都是藏青，
    //   字是白的）。所以这里必须逐格判断"这一格落在选区里的哪一段"，不能只涂文字那几个像素。
    //
    // 判据：格文字的**第一行**是源码那一行的一段（本工程的表格就是这么造的），把它在源码行里
    // 定位（同一个格子重复出现时从上次的位置往后找），得到本格占的字节范围 [a,b)：
    //   · 选区（同一行）**完全包住** [a,b) -> 涂满整格 + 白字
    //   · 只包住一部分                -> 涂"文字宽度"里的对应一段 + 那一段白字
    //   · 完全没交集                  -> 不涂
    // 定位失败（格文字不是源码行的子串，例如合成的表头）就退回"整行都在选区里就涂满"——
    // 这样 Ctrl+A 时表头也跟着反白（原版如此）。
    // （l0/c0/l1/c1/bHasSel 在上面填充那一趟已经取过了）

    x = EC_TEXT_X;
    col = 0;
    for (ci = 0; ci < row.cells.size() && col < tb.cols; ci++)
    {
        EcCell& cell = row.cells[ci];
        int w = 0;
        for (int k = 0; k < cell.span && col + k < tb.cols; k++) w += tb.w[col + k];
        // ✔ 在单元格里居中、行顶 +7（原版实测：传址列 36 宽时勾的左边缘在 cell+13）
        int tx = x + ((cell.check && cell.s.IsEmpty()) ? (w - EC_TICK_W) / 2 : EC_CELL_PAD);
        int ty = y + 7;

        int   nSelA = -1, nSelB = -1;       // 本格里要涂的那一段（按格文字的字节下标）
        BOOL  bFull = FALSE;               // 整格涂满
        if (bHasSel && !cell.s.IsEmpty() && row.src >= l0 && row.src <= l1 &&
            row.src >= 0 && row.src < (int)m_lines.size())
        {
            CString key = cell.s;
            int nl = key.Find(_T('\n'));
            if (nl >= 0) key = key.Left(nl);
            const CString& srcLine = m_lines[row.src];
            int nAt = FindInSourceFrom(srcLine, 0, key);
            int a, b;
            if (nAt >= 0)
            {
                a = nAt;
                b = nAt + key.GetLength();
            }
            else
            {
                // 合成表头：源码行里找不到这段字 -> 用"整行"当范围
                a = 0;
                b = srcLine.GetLength();
            }
            int s0, s1;
            if (row.src > l0 && row.src < l1) { s0 = 0; s1 = srcLine.GetLength(); }
            else if (row.src == l0 && row.src == l1) { s0 = c0; s1 = c1; }
            else if (row.src == l0) { s0 = c0; s1 = srcLine.GetLength(); }
            else { s0 = 0; s1 = c1; }
            if (b > a && s1 > s0 && a < s1 && b > s0)
            {
                if (s0 <= a && s1 >= b)
                {
                    bFull = TRUE;                       // 整格都在选区里
                }
                else
                {
                    // 部分选中：把源码列换算成格文字里的下标
                    int ka = (s0 > a) ? (s0 - a) : 0;
                    int kb = (s1 < b) ? (s1 - a) : (b - a);
                    if (nAt >= 0 && kb > ka) { nSelA = ka; nSelB = kb; }
                    else if (nAt < 0)        { nSelA = 0;  nSelB = key.GetLength(); }
                }
            }
        }

        if (cell.check && cell.s.IsEmpty())
        {
            DrawCheck(dc, tx, ty);
        }
        else if (!cell.s.IsEmpty())
        {
            CFont* pOld = dc.GetCurrentFont();
            dc.SelectObject(cell.under ? &m_fontU : &m_fontTable);
            int tw = dc.GetTextExtent(cell.s).cx;
            dc.SelectObject(pOld);

            if (bFull)
            {
                // 整格涂满：原版连格子的边框内区域一起涂（x+1 .. x+w-1，行高 EC_CELL_H）
                dc.FillSolidRect(x + 1, y + 1, w - 1, EC_CELL_H, EC_C_SELBG);
            }
            else if (nSelA >= 0 && nSelB > nSelA)
            {
                dc.SelectObject(cell.under ? &m_fontU : &m_fontTable);
                int xa = x + EC_CELL_PAD + dc.GetTextExtent(cell.s.Left(nSelA)).cx;
                int xb = x + EC_CELL_PAD + dc.GetTextExtent(cell.s.Left(nSelB)).cx;
                dc.SelectObject(&m_fontTable);
                if (xb > xa)
                    dc.FillSolidRect(xa, y + 1, xb - xa, EC_CELL_H, EC_C_SELBG);
            }
            (void)tw;

            dc.SelectObject(cell.under ? &m_fontU : &m_fontTable);
            dc.SetTextColor((bFull || nSelA >= 0) ? RGB(255, 255, 255) : cell.cr);
            dc.TextOut(x + EC_CELL_PAD, y + 5, cell.s);
            dc.SelectObject(&m_fontTable);
            if (cell.check)
                DrawCheck(dc, x + EC_CELL_PAD + dc.GetTextExtent(cell.s).cx + 4, ty);
        }
        x += w;
        col += cell.span;
    }
}

void CECEditor::DrawCodeRow(CDC& dc, int nRow)
{
    EcRow& row = m_rows[nRow];
    int y = row.y - m_nScroll;
    int x = TextXOfIndent(row.indent);

    // 子程序的第一条语句：流程槽里那个 ↓
    // 原版实测（shots\live_tabs\view0.png，graphic.e 1:1）：竖线在 x=9、高 7px，
    // 下面接一个「上宽 7、下尖」的实心三角（4 行），整条是藏青 #000080（= EC_C_PUNCT）
    if (row.mark == ECM_DOWN)
    {
        int ax = EC_FLOW_X(0) - 15;          // 24 - 15 = 9，与原版实测一致
        int ty = y + 7;
        dc.FillSolidRect(ax, ty, 1, 7, EC_C_PUNCT);
        for (int k = 0; k < 4; k++)
            dc.FillSolidRect(ax - 3 + k, ty + 7 + k, 7 - 2 * k, 1, EC_C_PUNCT);
    }

    if (row.code.IsEmpty()) return;
    dc.SelectObject(&m_font);
    int sa = -1, sb = -1;
    SelRangeOfRow(nRow, sa, sb);
    TokenizeDraw(dc, row.code, x, y + 3, sa, sb);
}

// 词法着色：逐 run 画。nSelA/nSelB = 本行落在选区里的字节范围（-1 = 没有选区），
// 落在选区里的那一段用「选区底 + 白字」（原版实测，见 EC_C_SELBG）。
void CECEditor::TokenizeDraw(CDC& dc, const CString& s, int x, int y, int nSelA, int nSelB)
{
    int n = s.GetLength();
    int i = 0;
    while (i < n)
    {
        unsigned char c = (unsigned char)s[i];
        int start = i;
        CString tok;
        COLORREF cr = EC_C_TEXT;

        if (c == '\'')
        {
            i = n;
            cr = EC_C_CMT;
        }
        else if (c == '"' || (c == 0xA1 && i + 1 < n && (unsigned char)s[i + 1] == 0xB0))
        {
            BOOL bFull = (c == 0xA1);
            i += (bFull ? 2 : 1);
            while (i < n)
            {
                unsigned char d = (unsigned char)s[i];
                if (bFull && d == 0xA1 && i + 1 < n && (unsigned char)s[i + 1] == 0xB1) { i += 2; break; }
                if (!bFull && d == '"') { i++; break; }
                if (d == '\r' || d == '\n') break;
                i += (d >= 0x81 && i + 1 < n) ? 2 : 1;
            }
            cr = EC_C_STR;
        }
        else if (c == 0xA3 || c == 0xA1)
        {
            i += (i + 1 < n) ? 2 : 1;
            cr = EC_C_PUNCT;
        }
        else if (strchr("()[]{}.,;:=+-*/<>%&|!? \t", c) != NULL && c < 0x80)
        {
            i++;
            cr = EC_C_PUNCT;
        }
        else if (c >= '0' && c <= '9')
        {
            while (i < n && (((unsigned char)s[i] >= '0' && (unsigned char)s[i] <= '9') ||
                             (unsigned char)s[i] == '.')) i++;
            // ⚠️ 存疑：原版 `信息框（"…", 0, ）` 那个 0 实测是 #800000（深红），而内置配色的
            //    `数值前景` 槽是 #008000（绿）。但 `shots\live_acc\view0.png` 里绿色像素绝大多数
            //    是注释，无法据此断定原版的数值色 —— 所以这里**保持按语义取「数值前景」槽**，
            //    不做没有实测支撑的改动（详见 HANDOFF 第 6 节）。
            cr = EC_C_NUM;
        }
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0x81)
        {
            while (i < n)
            {
                unsigned char d = (unsigned char)s[i];
                if ((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z') ||
                    (d >= '0' && d <= '9') || d == '_') i++;
                else if (d >= 0x81 && i + 1 < n) i += 2;
                else break;
            }
            tok = s.Mid(start, i - start);
            cr = EcClassify(tok);
        }
        else
        {
            i++;
            cr = EC_C_TEXT;
        }

        CString run = s.Mid(start, i - start);
        if (run.IsEmpty()) break;
        // 与本行选区求交，分成「选区前 / 选区内 / 选区后」三段分别上色
        if (nSelA >= 0 && nSelB > nSelA && start < nSelB && i > nSelA)
        {
            int sa = (nSelA > start) ? nSelA : start;
            int sb = (nSelB < i) ? nSelB : i;
            if (sa > start)
            {
                CString pre = s.Mid(start, sa - start);
                dc.SetTextColor(cr);
                dc.TextOut(x, y, pre);
                x += dc.GetTextExtent(pre).cx;
            }
            CString mid = s.Mid(sa, sb - sa);
            if (!mid.IsEmpty())
            {
                dc.SetTextColor(EC_C_SELFG);
                dc.TextOut(x, y, mid);
                x += dc.GetTextExtent(mid).cx;
            }
            if (sb < i)
            {
                CString post = s.Mid(sb, i - sb);
                dc.SetTextColor(cr);
                dc.TextOut(x, y, post);
                x += dc.GetTextExtent(post).cx;
            }
            continue;
        }
        dc.SetTextColor(cr);
        dc.TextOut(x, y, run);
        x += dc.GetTextExtent(run).cx;
    }
}

void CECEditor::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);

    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = mem.SelectObject(&bmp);
    DrawAll(mem);
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOldBmp);
}

void CECEditor::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    UpdateScrollBar();
    ScrollTo(m_nScroll);        // 只把滚动位置夹回合法范围，不要跟着光标跑
    Invalidate(FALSE);
}

LRESULT CECEditor::OnSyncGutter(WPARAM, LPARAM)
{
    Invalidate(FALSE);
    return 0;
}

// ------------------------------------------------------------------ 滚动条 / 光标
void CECEditor::UpdateScrollBar()
{
    CRect rc;
    GetClientRect(&rc);
    SCROLLINFO si;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = m_nContentH;
    si.nPage  = rc.Height();
    si.nPos   = m_nScroll;
    SetScrollInfo(SB_VERT, &si, TRUE);
}

void CECEditor::ScrollTo(int nNew)
{
    CRect rc;
    GetClientRect(&rc);
    int nMax = m_nContentH - rc.Height();
    if (nMax < 0) nMax = 0;
    if (nNew < 0) nNew = 0;
    if (nNew > nMax) nNew = nMax;
    if (nNew == m_nScroll) return;
    m_nScroll = nNew;
    SetScrollPos(SB_VERT, m_nScroll, TRUE);
    Invalidate(FALSE);
}

void CECEditor::EnsureVisible()
{
    CRect rc;
    GetClientRect(&rc);
    if (rc.Height() <= 0) return;       // 还没布局（SetText 时客户区是 0x0）
    int y = RowTopOfSrc(m_nCaretLine);
    if (y < m_nScroll) ScrollTo(y - 4);
    else if (y + EC_CODE_H > m_nScroll + rc.Height()) ScrollTo(y + EC_CODE_H - rc.Height() + 4);
}

int CECEditor::RowTopOfSrc(int nSrc) const
{
    for (int i = 0; i < (int)m_rows.size(); i++)
        if (m_rows[i].kind == 1 && m_rows[i].src == nSrc) return m_rows[i].y;
    return 0;
}

int CECEditor::HitTestRow(CPoint pt) const
{
    int y = pt.y + m_nScroll;
    for (int i = 0; i < (int)m_rows.size(); i++)
        if (y >= m_rows[i].y && y < m_rows[i].y + m_rows[i].h) return i;
    return -1;
}

// ---------------------------------------------------------------- 单元格就地编辑
// 原版：点「公开」这类格子直接把源码里的标记改掉，双击文字格会临时建一个 EDIT 子控件
// （本轮按这条实现；列语义是**从同一张表、同一格数的最近一条表头行**读出来的，
//   所以「子程序数据行（5 格）」和「参数行（6 格）」各自对得上自己的表头）。
static BOOL EcIsFlagCol(LPCTSTR psz)
{
    return (!_tcscmp(psz, _T("公开")) || !_tcscmp(psz, _T("易包")) ||
            !_tcscmp(psz, _T("参考")) || !_tcscmp(psz, _T("可空")) ||
            !_tcscmp(psz, _T("数组")) || !_tcscmp(psz, _T("静态")) ||
            !_tcscmp(psz, _T("传址")));
}

// 该标记在文本形式的第几个逗号字段（见 HANDOFF 第 7 节；字段 0 = 名字、1 = 类型）
static int EcFlagField(LPCTSTR pszHead, LPCTSTR pszFlag)
{
    if (!_tcscmp(pszHead, _T("子程序")))
        return !_tcscmp(pszFlag, _T("公开")) ? 2 : (!_tcscmp(pszFlag, _T("易包")) ? 3 : -1);
    if (!_tcscmp(pszHead, _T("参数")))
    {
        if (!_tcscmp(pszFlag, _T("参考"))) return 2;
        if (!_tcscmp(pszFlag, _T("可空"))) return 3;
        if (!_tcscmp(pszFlag, _T("数组"))) return 4;
        if (!_tcscmp(pszFlag, _T("传址"))) return 2;   // Dll命令块里的参数：名,类型,传址,数组,备注
        return -1;
    }
    if (!_tcscmp(pszHead, _T("局部变量")) || !_tcscmp(pszHead, _T("程序集变量")))
        return !_tcscmp(pszFlag, _T("静态")) ? 2 : (!_tcscmp(pszFlag, _T("数组")) ? 3 : -1);
    if (!_tcscmp(pszHead, _T("全局变量")))
        return !_tcscmp(pszFlag, _T("公开")) ? 2 : (!_tcscmp(pszFlag, _T("数组")) ? 3 : -1);
    if (!_tcscmp(pszHead, _T("DLL命令")))
        return !_tcscmp(pszFlag, _T("公开")) ? 4 : -1;
    return -1;
}

// 文字格 -> 源码字段号（名字 0 / 类型 1 / 备注 = 最后一个字段）
static int EcTextCellField(LPCTSTR pszHead)
{
    if (!_tcscmp(pszHead, _T("备注"))) return -2;                 // -2 = 最后一个字段
    if (!_tcscmp(pszHead, _T("类型")) || !_tcscmp(pszHead, _T("返回值类型"))) return 1;
    if (!_tcscmp(pszHead, _T("变量名")) || !_tcscmp(pszHead, _T("子程序名")) ||
        !_tcscmp(pszHead, _T("程序集名")) || !_tcscmp(pszHead, _T("窗口程序集名")) ||
        !_tcscmp(pszHead, _T("参数名")) || !_tcscmp(pszHead, _T("Dll命令名")))
        return 0;
    return -1;
}

BOOL CECEditor::HitTestCell(CPoint pt, int& nRow, int& nCol, CString& sHead) const
{
    int r = HitTestRow(pt);
    if (r < 0 || m_rows[r].kind != 0 || m_rows[r].table < 0) return FALSE;
    const EcRow& row = m_rows[r];
    const EcTable& tb = m_tables[row.table];

    int x = EC_TEXT_X, col = 0, hit = -1;
    for (size_t ci = 0; ci < row.cells.size() && col < tb.cols; ci++)
    {
        int w = 0;
        for (int k = 0; k < row.cells[ci].span && col + k < tb.cols; k++) w += tb.w[col + k];
        if (pt.x >= x && pt.x < x + w) { hit = (int)ci; break; }
        x += w;
        col += row.cells[ci].span;
    }
    if (hit < 0) return FALSE;

    CString head;
    for (int k = r - 1; k >= 0; k--)
    {
        if (m_rows[k].kind != 0 || m_rows[k].table != row.table) continue;
        if (m_rows[k].cells.size() == row.cells.size())
        {
            head = m_rows[k].cells[hit].s;
            break;
        }
    }
    head.Replace(_T(" "), _T(""));      // 表头写的是「保 留」「类 型」「备 注」
    nRow = r; nCol = hit; sHead = head;
    return TRUE;
}

// 把某行源码拆成「头词 + 字段表」，改完再用原分隔符拼回去
static BOOL EcSplitDeclLine(const CString& line, CString& head,
                            std::vector<CString>& tok, std::vector<CString>& seps)
{
    CString t = EcTrim(line);
    if (t.GetLength() < 2 || t[0] != _T('.')) return FALSE;
    CString rest = EcTrim(t.Mid(1));
    head = EcHeadWord(rest);
    if (head.IsEmpty()) return FALSE;
    EcParseDecl(rest.Mid(head.GetLength()), tok, &seps);
    return TRUE;
}

static CString EcJoinDeclLine(LPCTSTR pszHead, const std::vector<CString>& tok,
                             const std::vector<CString>& seps)
{
    CString s = _T(".");
    s += pszHead;
    if (!tok.empty())
    {
        s += _T(" ");
        s += EcJoinFrom(tok, 0, &seps);
    }
    return s;
}

BOOL CECEditor::ToggleFlagCell(int nRow, LPCTSTR pszFlag)
{
    if (nRow < 0 || nRow >= (int)m_rows.size()) return FALSE;
    int src = m_rows[nRow].src;
    if (src < 0 || src >= (int)m_lines.size()) return FALSE;

    CString head;
    std::vector<CString> tok, seps;
    if (!EcSplitDeclLine(m_lines[src], head, tok, seps)) return FALSE;
    int fld = EcFlagField(head, pszFlag);
    if (fld < 0) return FALSE;

    while ((int)tok.size() <= fld) tok.push_back(_T(""));
    if (tok[fld] == pszFlag) tok[fld] = _T("");
    else                     tok[fld] = pszFlag;

    m_lines[src] = EcJoinDeclLine(head, tok, seps);
    ClearOpenMark();
    Rebuild();
    Layout();
    Invalidate(FALSE);
    PushUndo();
    DumpModel();
    return TRUE;
}

BOOL CECEditor::SetCellText(int nRow, int nCol, LPCTSTR pszText)
{
    if (nRow < 0 || nRow >= (int)m_rows.size()) return FALSE;
    int src = m_rows[nRow].src;
    if (src < 0 || src >= (int)m_lines.size()) return FALSE;

    CString head, headCell;
    if (!HitTestCellByRow(nRow, nCol, headCell)) return FALSE;
    std::vector<CString> tok, seps;
    if (!EcSplitDeclLine(m_lines[src], head, tok, seps)) return FALSE;
    int fld = EcTextCellField(headCell);
    if (fld == -1) return FALSE;
    if (fld == -2)
    {
        if (tok.empty()) tok.push_back(_T(""));
        tok[tok.size() - 1] = pszText;
    }
    else
    {
        while ((int)tok.size() <= fld) tok.push_back(_T(""));
        tok[fld] = pszText;
    }

    m_lines[src] = EcJoinDeclLine(head, tok, seps);
    ClearOpenMark();
    Rebuild();
    Layout();
    Invalidate(FALSE);
    PushUndo();
    DumpModel();
    return TRUE;
}

// 按行/格号取表头文字（HitTestCell 的按行版本，给 SetCellText 用）
BOOL CECEditor::HitTestCellByRow(int nRow, int nCol, CString& sHead) const
{
    if (nRow < 0 || nRow >= (int)m_rows.size() || m_rows[nRow].kind != 0) return FALSE;
    if (m_rows[nRow].table < 0) return FALSE;
    for (int k = nRow - 1; k >= 0; k--)
    {
        if (m_rows[k].kind != 0 || m_rows[k].table != m_rows[nRow].table) continue;
        if ((int)m_rows[k].cells.size() == (int)m_rows[nRow].cells.size() && nCol < (int)m_rows[k].cells.size())
        {
            sHead = m_rows[k].cells[nCol].s;
            sHead.Replace(_T(" "), _T(""));
            return TRUE;
        }
    }
    return FALSE;
}

// 双击文字格 -> 在那个格子上建一个 EDIT（原版就是这么干的）
void CECEditor::BeginEditCell(int nRow, int nCol)
{
    if (nRow < 0 || nRow >= (int)m_rows.size()) return;
    EnsureCellEdit();           // 只建一次（见下）

    const EcRow& row = m_rows[nRow];
    const EcTable& tb = m_tables[row.table];
    int x = EC_TEXT_X, col = 0;
    CRect rcCell(0, 0, 0, 0);
    for (size_t ci = 0; ci < row.cells.size() && col < tb.cols; ci++)
    {
        int w = 0;
        for (int k = 0; k < row.cells[ci].span && col + k < tb.cols; k++) w += tb.w[col + k];
        if ((int)ci == nCol) { rcCell = CRect(x, row.y - m_nScroll, x + w, row.y + row.h - m_nScroll); break; }
        x += w;
        col += row.cells[ci].span;
    }
    if (rcCell.IsRectEmpty()) return;
    rcCell.InflateRect(-1, -1);

    m_bEditing = TRUE;
    m_nEditRow = nRow;
    m_nEditCol = nCol;
    m_wndEdit.ShowWindow(SW_SHOW);
    m_wndEdit.MoveWindow(rcCell);
    m_wndEdit.SetWindowText(row.cells[nCol].s);
    m_wndEdit.SetSel(0, -1);
    m_wndEdit.SetFocus();
}

void CECEditor::CommitEditCell(BOOL bCancel)
{
    if (!m_bEditing) return;
    CString s;
    m_wndEdit.GetWindowText(s);
    int r = m_nEditRow, c = m_nEditCol;
    m_bEditing = FALSE;
    m_wndEdit.ShowWindow(SW_HIDE);
    SetFocus();
    if (!bCancel) SetCellText(r, c, s);
    Invalidate(FALSE);
}

// 就地编辑用的 EDIT 子控件：第一次双击时才建（原版也是临时建一个 EDIT 子控件）
void CECEditor::EnsureCellEdit()
{
    if (m_wndEdit.GetSafeHwnd() != NULL) return;
    m_wndEdit.Create(WS_CHILD | ES_AUTOHSCROLL, CRect(0, 0, 10, 10), this, EC_ID_CELLEDIT);
    m_wndEdit.SetFont(&m_font);
}

// 代码编辑器的右键菜单。
//
// ★ 第 26 轮：照**原版**做（`tools\dump_ctxmenu.py` 把原版那个菜单整棵倒下来了，
//   存在 `shots\ctxmenu_orig.txt`：27 个顶层项、两个子菜单，尺寸 311x535）。
//   命令 id **沿用原版 id**（这样"哪个还没实现"一眼就能看出来），只有"展开所有子程序"
//   那条原版给的是灰色（它有内容才算可用），本工程一律可点。
//
// ⚠️ 用 **TPM_RETURNCMD** 的返回值主进程里能拿到，但**跨进程/自动化脚本看不到菜单**
//    （TrackPopupMenu 是模态的，返回值不出进程）—— 所以这里**不带 RETURNCMD**：
//    选中项以 WM_COMMAND 发给 this，走 OnCommand 分派；菜单本身是个真实可见的
//    #32768 顶层弹出窗口，量得到（`tools\edit_real.py` / `tools\menu_items.py`）。
void CECEditor::OnContextMenu(CWnd* pWnd, CPoint point)
{
    (void)pWnd;
    SetFocus();
    CMenu menu;
    if (!menu.CreatePopupMenu())
        return;
    CMenu subWrap, subConv, subMove;
    subWrap.CreatePopupMenu();
    subConv.CreatePopupMenu();
    subMove.CreatePopupMenu();

    BOOL bSel = HasSel();

    menu.AppendMenu(MF_STRING, EC_CTX_NEWSUB, _T("&N.新子程序\tCtrl+N"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | (CanUndo() ? 0 : MF_GRAYED), EC_CTX_UNDO, _T("&U.撤消\tCtrl+Z"));
    menu.AppendMenu(MF_STRING | (bSel ? 0 : MF_GRAYED), EC_CTX_COPY, _T("&C.复制\tCtrl+C"));
    menu.AppendMenu(MF_STRING | (bSel ? 0 : MF_GRAYED), EC_CTX_CUT, _T("&T.剪切\tCtrl+X"));
    menu.AppendMenu(MF_STRING, EC_CTX_PASTE, _T("&P.粘贴\tCtrl+V"));
    menu.AppendMenu(MF_STRING, EC_CTX_SELALL, _T("&F. 全选\tCtrl+A"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, EC_CTX_DELLINE, _T("&E.删除行\tF10"));
    menu.AppendMenu(MF_STRING, EC_CTX_INSLINE, _T("I.插入新行\tIns"));
    menu.AppendMenu(MF_STRING | MF_GRAYED, EC_CTX_COMPILE, _T("K.编译当前行\tShift+Enter"));
    menu.AppendMenu(MF_SEPARATOR);

    // 「加到当前块首尾」/「转换为 ->」两个子菜单：命令 id 就是原版那两批
    // （#32800-32806 / #32807-32813，见 §2.9；两个都已在 CMainFrame 里实现）
    subWrap.AppendMenu(MF_STRING, 32800, _T("&I.如果"));
    subWrap.AppendMenu(MF_STRING, 32801, _T("&T.如果真"));
    subWrap.AppendMenu(MF_STRING, 32802, _T("&S.判断"));
    subWrap.AppendMenu(MF_SEPARATOR);
    subWrap.AppendMenu(MF_STRING, 32803, _T("&D.判断循环首"));
    subWrap.AppendMenu(MF_STRING, 32804, _T("&W.循环判断首"));
    subWrap.AppendMenu(MF_STRING, 32805, _T("&R.计次循环首"));
    subWrap.AppendMenu(MF_STRING, 32806, _T("&F.变量循环首"));
    menu.AppendMenu(MF_POPUP | MF_STRING, (UINT)subWrap.GetSafeHmenu(), _T("&L.加到当前块首尾"));

    subConv.AppendMenu(MF_STRING, 32807, _T("&I.如果"));
    subConv.AppendMenu(MF_STRING, 32808, _T("&T.如果真"));
    subConv.AppendMenu(MF_STRING, 32809, _T("&S.判断"));
    subConv.AppendMenu(MF_SEPARATOR);
    subConv.AppendMenu(MF_STRING, 32810, _T("&D.判断循环首"));
    subConv.AppendMenu(MF_STRING, 32811, _T("&W.循环判断首"));
    subConv.AppendMenu(MF_STRING, 32812, _T("&R.计次循环首"));
    subConv.AppendMenu(MF_STRING, 32813, _T("&F.变量循环首"));
    menu.AppendMenu(MF_POPUP | MF_STRING, (UINT)subConv.GetSafeHmenu(), _T("&H.转换为 ->"));

    menu.AppendMenu(MF_STRING | MF_GRAYED, 32815, _T("&W.插入判断分支"));
    menu.AppendMenu(MF_SEPARATOR);

    // 「相对光标移动」子菜单（原版 id #32774/#32775/#32822/#32821）
    subMove.AppendMenu(MF_STRING, 32774, _T("&U.到上相关位置\tCtrl+Shift+Up"));
    subMove.AppendMenu(MF_STRING, 32775, _T("&D.到下相关位置\tCtrl+Shift+Down"));
    subMove.AppendMenu(MF_STRING, 32822, _T("&P.到上一组件\tCtrl+Alt+Up"));
    subMove.AppendMenu(MF_STRING, 32821, _T("&N.到下一组件\tCtrl+Alt+Down"));
    menu.AppendMenu(MF_POPUP | MF_STRING, (UINT)subMove.GetSafeHmenu(), _T("&R.相对光标移动"));

    menu.AppendMenu(MF_STRING | MF_GRAYED, 32885, _T("&S.跳转到定义处\tCtrl+B/Ctrl+单击"));
    menu.AppendMenu(MF_STRING | MF_GRAYED, 33003, _T("&Z.跳回先前位置\tCtrl+J"));
    menu.AppendMenu(MF_STRING | MF_GRAYED, 33010, _T("&J.到对应窗口\tCtrl+U"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 32927, _T("&A.收缩"));
    menu.AppendMenu(MF_STRING, 32932, _T("&X.展开子程序"));
    menu.AppendMenu(MF_STRING, 32972, _T("&D.展开所有子程序"));
    menu.AppendMenu(MF_STRING, 32936, _T("&G.屏蔽\tCtrl+K"));
    menu.AppendMenu(MF_STRING, 32937, _T("&B.解除屏蔽\tCtrl+M"));
    menu.AppendMenu(MF_STRING | MF_GRAYED, 33006, _T("&Y.高亮标记待寻找单词\tCtrl+Q/Alt+双击"));

    CPoint pt = point;
    if (pt.x == -1 && pt.y == -1) { pt = CaretRect().TopLeft(); ClientToScreen(&pt); }
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
}

// 右键菜单那一堆命令：能在编辑器里做的直接做，属于"编辑菜单那一批"的转给父窗口
// （CMainFrame 里的 OnEditWrapBlock / ... 都挂好了，用原版命令 id 转发）。
void CECEditor::DoContextCmd(UINT nID)
{
    CWnd* pParent = GetParent();
    switch (nID)
    {
    // ---- 编辑器自己能做的
    case EC_CTX_UNDO:     Undo();                    return;
    case EC_CTX_COPY:     CopySelToClipboard(FALSE); return;
    case EC_CTX_CUT:      CopySelToClipboard(TRUE);  return;
    case EC_CTX_PASTE:    PasteFromClipboard();      return;
    case EC_CTX_SELALL:   SelectAll();               return;
    case EC_CTX_DELLINE:  DeleteCurrentLine();       return;
    case EC_CTX_INSLINE:  InsertLineBelow();         return;
    case 32927:           CollapseSubOfCaret(TRUE);  return;
    case 32932:           CollapseSubOfCaret(FALSE); return;
    case 32972:           ExpandAllSubs();           return;
    case 32936:           ToggleComment(TRUE);       return;
    case 32937:           ToggleComment(FALSE);      return;
    // ---- 相对光标移动（到上/下相关位置 = 上/下一处相同的词；到上/下一组件 = 位置类型变了）
    case 32774:           GotoRelatedWord(FALSE);    return;
    case 32775:           GotoRelatedWord(TRUE);     return;
    case 32822:           GotoComponent(FALSE);      return;
    case 32821:           GotoComponent(TRUE);       return;
    }
    // ---- 其余交给父窗口（子框架 -> 主框架的编辑命令）
    if (pParent != NULL && pParent->GetSafeHwnd() != NULL)
        pParent->SendMessage(WM_COMMAND, nID, 0);
}

// 就地编辑的 EDIT 通知 + 右键菜单命令分派（菜单不带 TPM_RETURNCMD，命令从这条路由进来）
BOOL CECEditor::OnCommand(WPARAM wParam, LPARAM lParam)
{
    if (m_bEditing && HIWORD(wParam) == EN_KILLFOCUS)
        CommitEditCell(FALSE);
    // 右键菜单的命令：HIWORD==0（不是控件通知）且落在本菜单的 id 段里
    if (HIWORD(wParam) == 0 &&
        (LOWORD(wParam) >= EC_CTX_FIRST || LOWORD(wParam) == 32927 ||
         LOWORD(wParam) == 32932 || LOWORD(wParam) == 32936 || LOWORD(wParam) == 32937 ||
         LOWORD(wParam) == 32972 || LOWORD(wParam) == 32774 || LOWORD(wParam) == 32775 ||
         LOWORD(wParam) == 32821 || LOWORD(wParam) == 32822))
    {
        DoContextCmd(LOWORD(wParam));
        return TRUE;
    }
    return CWnd::OnCommand(wParam, lParam);
}

// ------------------------------------------------------------------ 相对光标移动
// 「到上/下相关位置」（原版 #32774 / #32775，快捷键 Ctrl+Shift+↑/↓）：
// 原版是"跳到同一层次语义相关的上一处/下一处"。这里用最省事又符合直觉的近似：
// 依次停在**同一缩进级**的语句行上（跳过表格行与子程序边界），也就是"上一条/下一条同级语句"。
void CECEditor::GotoRelatedWord(BOOL bForward)
{
    int i, n = (int)m_rows.size();
    // 找当前插入点在显示模型里的位置
    int cur = -1;
    for (i = 0; i < n; i++)
        if (m_rows[i].kind == 1 && m_rows[i].src == m_nCaretLine) { cur = i; break; }
    if (cur < 0)
        return;
    int lv = m_rows[cur].indent;
    int step = bForward ? 1 : -1;
    for (i = cur + step; i >= 0 && i < n; i += step)
    {
        if (m_rows[i].kind != 1) continue;
        if (m_rows[i].indent != lv) continue;
        m_nCaretLine = m_rows[i].src;
        m_nCaretCol  = 0;
        m_nAnchorLine = m_nCaretLine;
        m_nAnchorCol  = m_nCaretCol;
        EnsureVisible();
        UpdateCaret();
        Invalidate(FALSE);
        return;
    }
}

// 「到上/下一组件」（原版 #32822 / #32821，Ctrl+Alt+↑/↓）：
// 跳到上/下一个**子程序或程序集**（显示模型里那两行表格数据行），插入点落到它的第一行。
void CECEditor::GotoComponent(BOOL bForward)
{
    int i, n = (int)m_rows.size();
    int cur = -1;
    for (i = 0; i < n; i++)
        if (m_rows[i].kind == 1 && m_rows[i].src == m_nCaretLine) { cur = i; break; }
    if (cur < 0)
        cur = bForward ? -1 : n;
    int step = bForward ? 1 : -1;
    for (i = cur + step; i >= 0 && i < n; i += step)
    {
        if (m_rows[i].kind != 0) continue;
        if (m_rows[i].table < 0 || m_rows[i].table >= (int)m_tables.size()) continue;
        if (m_tables[m_rows[i].table].kind != ECT_SUB &&
            m_tables[m_rows[i].table].kind != ECT_ASM) continue;
        // 跳到这一行下面的第一条代码行
        int k;
        for (k = i + 1; k < n; k++)
            if (m_rows[k].kind == 1)
            {
                m_nCaretLine = m_rows[k].src;
                m_nCaretCol  = 0;
                m_nAnchorLine = m_nCaretLine;
                m_nAnchorCol  = m_nCaretCol;
                EnsureVisible();
                UpdateCaret();
                Invalidate(FALSE);
                return;
            }
    }
}

// 插入新行（右键菜单「插入新行 Ins」）：在插入点所在行的**下面**插一行空行，
// 插入点落到新行的行首（缩进沿用上一行）
void CECEditor::InsertLineBelow()
{
    if (m_nCaretLine < 0 || m_nCaretLine >= (int)m_lines.size())
        return;
    CString cur = m_lines[m_nCaretLine];
    CString pad;
    for (int i = 0; i < cur.GetLength(); i++)
        if (cur[i] == ' ' || cur[i] == '\t') pad += cur[i]; else break;
    m_lines.insert(m_lines.begin() + m_nCaretLine + 1, pad);
    m_nCaretLine++;
    m_nCaretCol = pad.GetLength();
    m_nAnchorLine = m_nCaretLine;
    m_nAnchorCol  = m_nCaretCol;
    AfterEdit();
}

BOOL CECEditor::PreTranslateMessage(MSG* pMsg)
{
    if (m_bEditing && pMsg->hwnd == m_wndEdit.GetSafeHwnd())
    {
        if (pMsg->message == WM_KEYDOWN)
        {
            if (pMsg->wParam == VK_RETURN) { CommitEditCell(FALSE); return TRUE; }
            if (pMsg->wParam == VK_ESCAPE) { CommitEditCell(TRUE);  return TRUE; }
        }
    }
    return CWnd::PreTranslateMessage(pMsg);
}


void CECEditor::CaretFromPoint(CPoint pt)
{
    int r = HitTestRow(pt);
    if (r < 0) return;

    if (m_rows[r].kind != 1)
    {
        // ★ 表格行：光标落在**点中的那一格**里（照原版 sub_4B5000 + sub_4B4980==13）。
        //   以前这里直接跳到「该子程序的第一条语句」，于是表格行整片点不进去 —— 用户报的
        //   「光标都选不中表格中的地方」。现在按格命中，并把格内列位置算出来。
        const EcRow& row = m_rows[r];
        if (row.table >= 0)
        {
            const EcTable& tb = m_tables[row.table];
            int x = EC_TEXT_X, col = 0;
            for (size_t ci = 0; ci < row.cells.size() && col < tb.cols; ci++)
            {
                int w = 0;
                for (int k = 0; k < row.cells[ci].span && col + k < tb.cols; k++)
                    w += tb.w[col + k];
                if (pt.x >= x && pt.x < x + w)
                {
                    // 格内偏移：用本格文字宽度找最近的字符边界（和代码行同一套办法）
                    CClientDC dc(this);
                    CFont* pOld = dc.SelectObject(&m_fontTable);
                    const CString& s = row.cells[ci].s;
                    int rel = pt.x - (x + EC_CELL_PAD);
                    int best = 0, bestd = 1 << 30;
                    for (int i = 0; i <= s.GetLength(); i++)
                    {
                        int tw = dc.GetTextExtent(s.Left(i)).cx - rel;
                        if (tw < 0) tw = -tw;
                        if (tw < bestd) { bestd = tw; best = i; }
                    }
                    dc.SelectObject(pOld);
                    m_nCaretLine = row.src;
                    m_nCaretCol  = -1;          // -1 = "落在表格格里"，见 ec_editor.h
                    m_nCaretCell = (int)ci;
                    // 格内偏移换算成该源码行里的字节位置：格文字在源码行里往后找
                    m_nCaretCellCol = best;
                    m_nAnchorLine = m_nCaretLine;
                    m_nAnchorCol  = m_nCaretCol;
                    return;
                }
                x += w;
                col += row.cells[ci].span;
            }
        }
        // 表格行但没命中任何格（点在后半段空白）：把光标放到该行第一个格
        if (!row.cells.empty())
        {
            m_nCaretLine = row.src;
            m_nCaretCol  = -1;
            m_nCaretCell = 0;
            m_nCaretCellCol = 0;
            m_nAnchorLine = m_nCaretLine;
            m_nAnchorCol  = m_nCaretCol;
        }
        return;
    }

    m_nCaretCell = -1;
    CClientDC dc(this);
    CFont* pOld = dc.SelectObject(&m_font);
    // ⚠️ 正文左边缘是 TextXOfIndent(缩进级) = 46 + 22n；以前这里写的是 EC_FLOW_X(缩进级)
    //    = 24 + 22n（**流程槽**的列），于是点击位置被算深了 22px —— 点哪儿插入点都跑到行尾，
    //    框选也就永远选不出东西（用户 2026-10 报的「不能选中」根因之一）。
    int x0 = TextXOfIndent(m_rows[r].indent);
    int rel = pt.x - x0;
    CString s = m_rows[r].code;
    int best = 0, bestd = 1 << 30;
    for (int i = 0; i <= s.GetLength(); i++)
    {
        int w = dc.GetTextExtent(s.Left(i)).cx;
        int d = w - rel; if (d < 0) d = -d;
        if (d < bestd) { bestd = d; best = i; }
        if (w > rel + 6 && i > 0) break;
    }
    dc.SelectObject(pOld);
    m_nCaretLine = m_rows[r].src;
    m_nCaretCol  = best;
}

// 插入点所在行：先找代码行，找不到（比如 Dll命令页整页都是表格）就用表格行
int CECEditor::RowOfCaret() const
{
    int i;
    for (i = 0; i < (int)m_rows.size(); i++)
        if (m_rows[i].kind == 1 && m_rows[i].src == m_nCaretLine) return i;
    for (i = 0; i < (int)m_rows.size(); i++)
        if (m_rows[i].src == m_nCaretLine) return i;
    return -1;
}

// 文档刚打开时流程槽里的红色铅笔（14x14，位置：x=3，行内竖直居中）
void CECEditor::DrawOpenMark(CDC& dc)
{
    int r = RowOfCaret();
    if (r < 0) return;
    int y = m_rows[r].y - m_nScroll + (m_rows[r].h - EC_PEN_H) / 2;
    static const COLORREF kPen[4] = { 0, RGB(0, 0, 0), RGB(0x80, 0, 0), RGB(0xFF, 0, 0) };
    for (int i = 0; i < EC_PEN_H; i++)
    {
        if (y + i < 0) continue;
        for (int j = 0; j < EC_PEN_W; j++)
        {
            int c = g_szPen[i][j] - '0';
            if (c > 0) dc.FillSolidRect(3 + j, y + i, 1, 1, kPen[c]);
        }
    }
}

CRect CECEditor::CaretRect() const
{
    CRect r(0, 0, 1, EC_FONT_H);
    int i = RowOfCaret();
    if (i < 0) return r;
    if (m_rows[i].kind == 1)
    {
        CClientDC dc((CECEditor*)this);
        CFont* pOld = dc.SelectObject((CFont*)&m_font);
        int w = dc.GetTextExtent(m_rows[i].code.Left(m_nCaretCol)).cx;
        dc.SelectObject(pOld);
        int x = TextXOfIndent(m_rows[i].indent) + w;
        int y = m_rows[i].y - m_nScroll + 3;
        r.SetRect(x, y, x + 1, y + EC_FONT_H);
    }
    else
    {
        // ★ 表格行：光标记在**点中的那一格**里（照原版 sub_4B5000 的每格左右边界数组）。
        //   以前无论点哪一格都把光标画在流程槽 EC_FLOW_X(0) 上，看起来就是"表格点不进去"。
        const EcRow& row = m_rows[i];
        int y = row.y - m_nScroll + (row.h - EC_FONT_H) / 2;
        int x = EC_FLOW_X(0);            // 兜底：没有格信息时还是画在流程槽
        if (m_nCaretCell >= 0 && row.table >= 0 && m_nCaretCell < (int)row.cells.size())
        {
            const EcTable& tb = m_tables[row.table];
            int cx = EC_TEXT_X, col = 0;
            for (int ci = 0; ci < m_nCaretCell; ci++)
            {
                for (int k = 0; k < row.cells[ci].span && col + k < tb.cols; k++)
                    cx += tb.w[col + k];
                col += row.cells[ci].span;
            }
            CClientDC dc((CECEditor*)this);
            CFont* pOld = dc.SelectObject((CFont*)&m_fontTable);
            const CString& s = row.cells[m_nCaretCell].s;
            int off = m_nCaretCellCol;
            if (off < 0) off = 0;
            if (off > s.GetLength()) off = s.GetLength();
            int w = dc.GetTextExtent(s.Left(off)).cx;
            dc.SelectObject(pOld);
            x = cx + EC_CELL_PAD + w;
        }
        r.SetRect(x, y, x + 1, y + EC_FONT_H);
    }
    return r;
}

void CECEditor::UpdateCaret()
{
    if (GetFocus() != this) { ::DestroyCaret(); return; }
    CRect r = CaretRect();

    // ★ 照原版 e.exe 改（第 27 轮）。原版 WM_SETFOCUS 的处理器 sub_4B4EA0（0x4B4EA0）：
    //     CreateCaret(*(HWND *)(this + 28), nullptr, 2, dword_673904 + 4);
    //   定位在 sub_4B55C0（0x4B55C0）：
    //     SetCaretPos(x, y - 2);
    //   以及 ImmSetCompositionWindow(...) —— 输入法候选框跟着光标走。
    // 三处都和我们原来不一样：
    //   · 宽：原版 2，我们原来 1
    //   · 高：原版 = 行高 + 4，我们原来 = 字号(12)
    //   · y ：原版上偏 2px，我们原来没有偏移
    // ⚠️ 原版那个 dword_673904 是运行期配色/度量全局（文件里初值是 FFFFFFFF），
    //    所以这里**不硬编码数字**，直接按同一个式子写成 EcCodeH() + 4。
    // ⚠️ 光标必须落在窗口的裁剪区内，否则 Windows 会把它藏起来 —— y 用 r.top - 2
    //    （CaretRect 给的 r.top 已经是"文字顶再往下 3px"，减 2 之后仍在行内）。
    ::CreateCaret(GetSafeHwnd(), NULL, 2, EC_CODE_H + 4);
    ::SetCaretPos(r.left, r.top - 2);
    ::ShowCaret(GetSafeHwnd());

    // 输入法候选框跟随光标（原版 sub_4B55C0 里就是这一段）
    HIMC hIMC = ::ImmGetContext(GetSafeHwnd());
    if (hIMC != NULL)
    {
        COMPOSITIONFORM cf;
        cf.dwStyle = CFS_POINT;
        cf.ptCurrentPos.x = r.left;
        cf.ptCurrentPos.y = r.top;
        ::ImmSetCompositionWindow(hIMC, &cf);
        ::ImmReleaseContext(GetSafeHwnd(), hIMC);
    }
}

void CECEditor::OnSetFocus(CWnd* pOldWnd)
{
    CWnd::OnSetFocus(pOldWnd);
    UpdateCaret();
}

void CECEditor::OnKillFocus(CWnd* pNewWnd)
{
    CWnd::OnKillFocus(pNewWnd);
    ::DestroyCaret();
    Invalidate(FALSE);
}

void CECEditor::OnLButtonDown(UINT nFlags, CPoint point)
{
    SetFocus();
    ClearOpenMark();

    // 点在流程槽的「-」/「+」上 -> 收缩/展开该子程序（不吃成光标定位）
    int nFold = HitTestFold(point);
    if (nFold >= 0)
    {
        CString sName;
        if (m_rows[nFold].cells.size() > 0) sName = m_rows[nFold].cells[0].s;
        if (!sName.IsEmpty())
            SetCollapsed(sName, !IsCollapsed(sName));
        CWnd::OnLButtonDown(nFlags, point);
        return;
    }

    // 点在代码行的「-」/「+」上 -> 整篇显示/隐藏参数归属线（原版实测的全局开关）
    if (HitTestParam(point) >= 0)
    {
        m_bParamShow = !m_bParamShow;
        Invalidate(FALSE);
        CWnd::OnLButtonDown(nFlags, point);
        return;
    }

    // 点在「公开/传址/数组/静态/可空/参考/易包」这些格子上 -> 直接改源码里的标记
    // （原版就是这样：点一下勾选/取消，不是先定位光标）
    if (m_bEditing)
        CommitEditCell(FALSE);
    int nCellRow = -1, nCellCol = -1;
    CString sHead;
    if (HitTestCell(point, nCellRow, nCellCol, sHead) && EcIsFlagCol(sHead))
    {
        if (ToggleFlagCell(nCellRow, sHead))
        {
            CWnd::OnLButtonDown(nFlags, point);
            return;
        }
    }

    CaretFromPoint(point);
    m_nAnchorLine = m_nCaretLine;
    m_nAnchorCol  = m_nCaretCol;
    m_bDragging   = TRUE;
    SetCapture();
    UpdateCaret();
    EnsureVisible();
    Invalidate(FALSE);
    CWnd::OnLButtonDown(nFlags, point);
}

void CECEditor::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDragging) { m_bDragging = FALSE; ReleaseCapture(); }
    CWnd::OnLButtonUp(nFlags, point);
}

void CECEditor::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bDragging)
    {
        CaretFromPoint(point);
        UpdateCaret();
        Invalidate(FALSE);
    }
    CWnd::OnMouseMove(nFlags, point);
}

// 右键菜单：剪切/复制/粘贴/全选（原版代码编辑器右键也是这一套）。菜单在运行时建，
// 不进资源 —— 免得动 assemble_rc 那条链。
// ⚠️ 这个旧版 4 项菜单已经被上面那份"照原版 27 项"的实现取代，整段删掉了。

void CECEditor::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    // 双击表格里的文字格 -> 就地编辑（原版会临时建一个 EDIT 子控件）
    int nRow = -1, nCol = -1;
    CString sHead;
    if (HitTestCell(point, nRow, nCol, sHead) && !EcIsFlagCol(sHead) &&
        EcTextCellField(sHead) != -1)
    {
        BeginEditCell(nRow, nCol);
        CWnd::OnLButtonDblClk(nFlags, point);
        return;
    }
    CWnd::OnLButtonDblClk(nFlags, point);
}

BOOL CECEditor::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    // 原版一个滚轮格滚 3 行左右；zDelta 不是 WHEEL_DELTA 的整数倍（触摸板）时至少走一行
    int nLines = (int)zDelta / WHEEL_DELTA;
    if (nLines == 0) nLines = (zDelta > 0) ? 1 : -1;
    ScrollTo(m_nScroll - nLines * 3 * EC_CODE_H);
    return TRUE;
}

void CECEditor::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CRect rc;
    GetClientRect(&rc);
    int nLine = EC_CODE_H;
    switch (nSBCode)
    {
    case SB_LINEUP:    ScrollTo(m_nScroll - nLine); break;
    case SB_LINEDOWN:  ScrollTo(m_nScroll + nLine); break;
    case SB_PAGEUP:    ScrollTo(m_nScroll - rc.Height()); break;
    case SB_PAGEDOWN:  ScrollTo(m_nScroll + rc.Height()); break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: ScrollTo((int)nPos); break;
    case SB_TOP:       ScrollTo(0); break;
    case SB_BOTTOM:    ScrollTo(m_nContentH); break;
    }
    CWnd::OnVScroll(nSBCode, nPos, pScrollBar);
}

// ------------------------------------------------------------------ 编辑
BOOL CECEditor::HasSel() const
{
    return (m_nAnchorLine != m_nCaretLine || m_nAnchorCol != m_nCaretCol);
}

void CECEditor::GetSel(int& l0, int& c0, int& l1, int& c1) const
{
    if (m_nAnchorLine < m_nCaretLine ||
        (m_nAnchorLine == m_nCaretLine && m_nAnchorCol <= m_nCaretCol))
    { l0 = m_nAnchorLine; c0 = m_nAnchorCol; l1 = m_nCaretLine; c1 = m_nCaretCol; }
    else
    { l0 = m_nCaretLine; c0 = m_nCaretCol; l1 = m_nAnchorLine; c1 = m_nAnchorCol; }
}

// 该显示行落在选区里的字节范围（返回 FALSE = 这一行没被选到）
BOOL CECEditor::SelRangeOfRow(int nRow, int& nA, int& nB) const
{
    nA = -1; nB = -1;
    if (nRow < 0 || nRow >= (int)m_rows.size()) return FALSE;
    if (m_rows[nRow].kind != 1 || !HasSel()) return FALSE;
    int l0, c0, l1, c1;
    GetSel(l0, c0, l1, c1);
    int src = m_rows[nRow].src;
    if (src < l0 || src > l1) return FALSE;
    const CString& s = m_rows[nRow].code;
    int len = s.GetLength();
    nA = (src == l0) ? c0 : 0;
    nB = (src == l1) ? c1 : len;
    if (nA > len) nA = len;
    if (nB > len) nB = len;
    if (nB < nA) nB = nA;
    return TRUE;
}

void CECEditor::SelectAll()
{
    if (m_lines.empty()) return;
    m_nAnchorLine = 0;
    m_nAnchorCol  = 0;
    m_nCaretLine  = (int)m_lines.size() - 1;
    m_nCaretCol   = m_lines[m_nCaretLine].GetLength();
    Invalidate(FALSE);
    UpdateCaret();
}

// 复制/剪切到剪贴板。用系统剪贴板（CF_TEXT，GBK），原版导出的格式里也有 Text。
void CECEditor::CopySelToClipboard(BOOL bCut)
{
    if (!HasSel()) return;
    int l0, c0, l1, c1;
    GetSel(l0, c0, l1, c1);
    CString s;
    for (int i = l0; i <= l1 && i < (int)m_lines.size(); i++)
    {
        CString t = m_lines[i];
        int a = (i == l0) ? c0 : 0;
        int b = (i == l1) ? c1 : t.GetLength();
        if (a > t.GetLength()) a = t.GetLength();
        if (b > t.GetLength()) b = t.GetLength();
        if (b > a) s += t.Mid(a, b - a);
        if (i < l1) s += _T("\r\n");
    }

    if (::OpenClipboard(GetSafeHwnd()))
    {
        ::EmptyClipboard();
        int n = s.GetLength();
        HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE, (n + 1) * sizeof(TCHAR));
        if (h)
        {
            LPTSTR p = (LPTSTR)::GlobalLock(h);
            if (p)
            {
                memcpy(p, (LPCTSTR)s, n * sizeof(TCHAR));
                p[n] = 0;
                ::GlobalUnlock(h);
                ::SetClipboardData(CF_TEXT, h);
            }
        }
        ::CloseClipboard();
    }

    if (bCut)
    {
        DeleteRange(l0, c0, l1, c1);
        m_nAnchorLine = m_nCaretLine;
        m_nAnchorCol  = m_nCaretCol;
        InvalidateAll();
        UpdateCaret();
    }
}

void CECEditor::PasteFromClipboard()
{
    CString s;
    if (::OpenClipboard(GetSafeHwnd()))
    {
        HANDLE h = ::GetClipboardData(CF_TEXT);
        if (h)
        {
            LPCSTR p = (LPCSTR)::GlobalLock(h);
            if (p) { s = p; ::GlobalUnlock(h); }
        }
        ::CloseClipboard();
    }
    if (s.IsEmpty()) return;
    if (HasSel())
    {
        int l0, c0, l1, c1;
        GetSel(l0, c0, l1, c1);
        DeleteRange(l0, c0, l1, c1);
    }
    s.Replace(_T("\r\n"), _T("\n"));
    s.Replace(_T("\r"), _T("\n"));
    int p2 = 0;
    while (p2 <= s.GetLength())
    {
        int q = s.Find(_T('\n'), p2);
        if (q < 0) { InsertText(s.Mid(p2)); break; }
        InsertText(s.Mid(p2, q - p2));
        InsertNewline();
        p2 = q + 1;
    }
    m_nAnchorLine = m_nCaretLine;
    m_nAnchorCol  = m_nCaretCol;
    PushUndo();                 // 整次粘贴算一步（InsertText/InsertNewline 自己不压栈）
    InvalidateAll();
    UpdateCaret();
    EnsureVisible();
}

void CECEditor::DeleteRange(int nL0, int nC0, int nL1, int nC1)
{
    if (nL0 < 0 || nL0 >= (int)m_lines.size()) return;
    if (nL1 < 0 || nL1 >= (int)m_lines.size()) return;
    if (nL0 == nL1)
    {
        CString& s = m_lines[nL0];
        if (nC0 > s.GetLength()) nC0 = s.GetLength();
        if (nC1 > s.GetLength()) nC1 = s.GetLength();
        s = s.Left(nC0) + s.Mid(nC1);
    }
    else
    {
        CString head = m_lines[nL0].Left(nC0);
        CString tail = m_lines[nL1].Mid(nC1);
        m_lines[nL0] = head + tail;
        m_lines.erase(m_lines.begin() + nL0 + 1, m_lines.begin() + nL1 + 1);
    }
    m_nCaretLine = nL0;
    m_nCaretCol  = nC0;
}

void CECEditor::InsertText(LPCTSTR psz)
{
    if (HasSel())
    {
        int l0, c0, l1, c1;
        GetSel(l0, c0, l1, c1);
        DeleteRange(l0, c0, l1, c1);
    }
    if (m_nCaretLine < 0 || m_nCaretLine >= (int)m_lines.size()) return;
    CString& s = m_lines[m_nCaretLine];
    if (m_nCaretCol > s.GetLength()) m_nCaretCol = s.GetLength();
    s = s.Left(m_nCaretCol) + CString(psz) + s.Mid(m_nCaretCol);
    m_nCaretCol += (int)_tcslen(psz);
    m_nAnchorLine = m_nCaretLine;
    m_nAnchorCol  = m_nCaretCol;
}

// 在插入点断行（回车键、以及粘贴文本里的换行都走这里）：自动沿用本行开头的空白
void CECEditor::InsertNewline()
{
    if (m_nCaretLine < 0 || m_nCaretLine >= (int)m_lines.size()) return;
    if (m_nCaretCol > m_lines[m_nCaretLine].GetLength())
        m_nCaretCol = m_lines[m_nCaretLine].GetLength();
    CString head = m_lines[m_nCaretLine].Left(m_nCaretCol);
    CString tail = m_lines[m_nCaretLine].Mid(m_nCaretCol);
    CString pad;
    for (int i = 0; i < head.GetLength(); i++)
        if (head[i] == ' ' || head[i] == '\t') pad += head[i]; else break;
    m_lines[m_nCaretLine] = head;
    m_lines.insert(m_lines.begin() + m_nCaretLine + 1, pad + tail);
    m_nCaretLine++;
    m_nCaretCol = pad.GetLength();
    // ⚠️ 必须把选区另一端也挪到新插入点：否则 HasSel() 还是 TRUE，
    //    紧接着的那次 InsertText 会以为用户在跨行选着东西，把刚断开的行又合回去
    //    （粘贴多行文本时表现为「换行全丢、挤成一行」—— 真踩过）。
    m_nAnchorLine = m_nCaretLine;
    m_nAnchorCol  = m_nCaretCol;
}

void CECEditor::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == '\r' || nChar == '\n' || nChar == '\t' || nChar == 8 || nChar == 27)
        return;     // 交给 OnKeyDown
    if (nChar < 32) return;
    ClearOpenMark();

    char mb[4];
    mb[0] = (char)(nChar & 0xFF);
    mb[1] = 0;
    // MBCS 下 WM_CHAR 的 wParam 低位放的是多字节字符的首字节；
    // 中文输入法合成出来的双字节由 WM_IME_CHAR 那条路（见消息映射）处理。
    CString add;
    add = mb;
    InsertText(add);
    PushUndo();
    InvalidateAll();
    UpdateCaret();
    EnsureVisible();
}

// WM_IME_CHAR：wParam 里是 DBCS 字符。首字节到底是高字节还是低字节，
// 文档说的和实测常不一致 —— 两个方向都试一下，哪个像合法的 GBK 双字节就用哪个。
LRESULT CECEditor::OnImeChar(WPARAM w, LPARAM)
{
    if (w < 0x100) { OnChar((UINT)w, 1, 0); return 0; }
    unsigned char lo = (unsigned char)(w & 0xFF);
    unsigned char hi = (unsigned char)((w >> 8) & 0xFF);
    char mb[3];
    if (IsDBCSLeadByte(hi)) { mb[0] = (char)hi; mb[1] = (char)lo; }
    else                    { mb[0] = (char)lo; mb[1] = (char)hi; }
    mb[2] = 0;
    CString add; add = mb;
    InsertText(add);
    InvalidateAll();
    UpdateCaret();
    EnsureVisible();
    return 0;
}

void CECEditor::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    ClearOpenMark();
    // 修饰键用 GetKeyState | GetAsyncKeyState 两条都看：注入式按键（自动化测试用
    // keybd_event 发的 Ctrl）只会更新 GetAsyncKeyState，GetKeyState 读的是消息队列状态。
    BOOL bShift = ((::GetKeyState(VK_SHIFT)   | ::GetAsyncKeyState(VK_SHIFT))   & 0x8000) != 0;
    BOOL bCtrl  = ((::GetKeyState(VK_CONTROL) | ::GetAsyncKeyState(VK_CONTROL)) & 0x8000) != 0;
    int  nLines = (int)m_lines.size();
    if (nLines == 0) { CWnd::OnKeyDown(nChar, nRepCnt, nFlags); return; }
    if (m_nCaretLine >= nLines) m_nCaretLine = nLines - 1;
    CString& cur = m_lines[m_nCaretLine];
    if (m_nCaretCol > cur.GetLength()) m_nCaretCol = cur.GetLength();

    switch (nChar)
    {
    case VK_LEFT:
        if (m_nCaretCol > 0) m_nCaretCol--;
        else if (m_nCaretLine > 0)
        {
            m_nCaretLine--;
            m_nCaretCol = m_lines[m_nCaretLine].GetLength();
        }
        break;
    case VK_RIGHT:
        if (m_nCaretCol < cur.GetLength()) m_nCaretCol++;
        else if (m_nCaretLine + 1 < nLines) { m_nCaretLine++; m_nCaretCol = 0; }
        break;
    case VK_UP:
        if (m_nCaretLine > 0) m_nCaretLine--;
        break;
    case VK_DOWN:
        if (m_nCaretLine + 1 < nLines) m_nCaretLine++;
        break;
    case VK_HOME:
        if (bCtrl) { m_nCaretLine = 0; }
        m_nCaretCol = 0;
        break;
    case VK_END:
        if (bCtrl) m_nCaretLine = nLines - 1;
        m_nCaretCol = m_lines[m_nCaretLine].GetLength();
        break;
    case VK_PRIOR: ScrollTo(m_nScroll - 200); break;
    case VK_NEXT:  ScrollTo(m_nScroll + 200); break;
    // Ctrl + A/C/X/V：全选 / 复制 / 剪切 / 粘贴（用户 2026-10 报「不能选中复制粘贴」）
    case 'A': if (bCtrl) { SelectAll(); return; } break;
    case 'C': if (bCtrl) { CopySelToClipboard(FALSE); return; } break;
    case 'X': if (bCtrl) { CopySelToClipboard(TRUE);  return; } break;
    case 'V': if (bCtrl) { PasteFromClipboard();      return; } break;
    // Ctrl+F：打开原版那个「寻找」对话框（#32877），让子窗口去弹
    case 'F': if (bCtrl) { CWnd* pF = GetParent(); if (pF != NULL) pF->PostMessage(WM_COMMAND, 32877, 0); return; } break;
    // Ctrl+H：打开原版那个「寻找替换」对话框（#3）
    case 'H': if (bCtrl) { CWnd* pF = GetParent(); if (pF != NULL) pF->PostMessage(WM_COMMAND, 3, 0); return; } break;
    // Ctrl+F7：书签跳转对话框（#32965）
    case VK_F7: if (bCtrl) { CWnd* pF = GetParent(); if (pF != NULL) pF->PostMessage(WM_COMMAND, 32965, 0); return; } break;
    // F3 / Shift+F3：寻找下一个 / 上一个（编辑菜单 #32878 / #32879）
    case VK_F3: FindNext(!bShift); return;
    // F6 / Shift+F6：到后/前一个书签；Ctrl+F6：加/取消书签（#32968/32967/32964）
    case VK_F6:
        if (bCtrl) ToggleBookmark();
        else       GotoBookmark(!bShift);
        return;
    // F9：设置或清除断点（运行菜单 #32855）
    case VK_F9: ToggleBreakpoint(); return;
    // Ctrl+Z / Ctrl+Y：撤消 / 重复（编辑菜单同名命令的另一条路，见 §2.9）
    case 'Z': if (bCtrl) { Undo(); return; } break;
    case 'Y': if (bCtrl) { Redo(); return; } break;
    // Ctrl+K / Ctrl+M：屏蔽 / 解除屏蔽（原版命令 #32936 / #32937）
    case 'K': if (bCtrl) { ToggleComment(TRUE);  return; } break;
    case 'M': if (bCtrl) { ToggleComment(FALSE); return; } break;
    // F10：删除行（原版命令 #32939）
    case VK_F10: DeleteCurrentLine(); return;
    case VK_DELETE:
        if (HasSel())
        {
            int l0, c0, l1, c1; GetSel(l0, c0, l1, c1);
            DeleteRange(l0, c0, l1, c1);
        }
        else if (m_nCaretCol < cur.GetLength())
            DeleteRange(m_nCaretLine, m_nCaretCol, m_nCaretLine, m_nCaretCol + 1);
        else if (m_nCaretLine + 1 < nLines)
            DeleteRange(m_nCaretLine, m_nCaretCol, m_nCaretLine + 1, 0);
        PushUndo();
        break;
    case VK_BACK:
        if (HasSel())
        {
            int l0, c0, l1, c1; GetSel(l0, c0, l1, c1);
            DeleteRange(l0, c0, l1, c1);
        }
        else if (m_nCaretCol > 0)
            DeleteRange(m_nCaretLine, m_nCaretCol - 1, m_nCaretLine, m_nCaretCol);
        else if (m_nCaretLine > 0)
        {
            int nPrev = m_lines[m_nCaretLine - 1].GetLength();
            DeleteRange(m_nCaretLine - 1, nPrev, m_nCaretLine, 0);
        }
        PushUndo();
        break;
    case VK_RETURN:
        {
            if (HasSel())
            {
                int l0, c0, l1, c1; GetSel(l0, c0, l1, c1);
                DeleteRange(l0, c0, l1, c1);
            }
            InsertNewline();
            PushUndo();
        }
        break;
    default:
        CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
        return;
    }

    if (!bShift) { m_nAnchorLine = m_nCaretLine; m_nAnchorCol = m_nCaretCol; }
    InvalidateAll();
    UpdateCaret();
    EnsureVisible();
}

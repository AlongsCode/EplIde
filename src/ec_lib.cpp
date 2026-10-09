// ec_lib.cpp -- 载入易语言支持库(.fne/.fnr) 并生成「工作夹->支持库」树 + 状态栏提示。
//
// 事实来源（全部实测，不要凭印象改）：
//   * 结构布局     : C:\Program Files (x86)\e\sdk\cpp\elib\lib2.h
//   * 原版对话框    : tools\dump_dialogs.py 导出的 shots\dialogs_all.txt（工作夹模板 = DIALOGEX 128）
//   * 原版树内容    : tools\treeinfo.c -> shots\orig_tree_libs.txt（1560 项，全量 dump）
//   * 原版提示文本  : treeinfo.exe e.exe sel <N>（把 状态栏 msctls_statusbar32 的文本读出来）
//
// ⚠️ krnln.fne 的 LIB_DATA_TYPE_INFO 数组在文件里只有第 0 项是静态的，其余 50 项由库的
//    初始化代码在运行期填。所以离线静态解析只能拿到 1 个数据类型 —— 必须 LoadLibrary + GetNewInf()。
#include <afxwin.h>
#include <afxtempl.h>
#include <algorithm>
#include "ec_lib.h"

// ================================================================== lib2.h 结构（原样镜像）
// 默认 /Zp8 对齐，RAW_CONST 里的 double 会落在偏移 24（20 处补 4 字节），和 lib2.h 一致。

struct RAW_ARG_INFO
{
    LPSTR name; LPSTR explain;
    short bmpIdx; short bmpCount;
    DWORD type; int def; DWORD state;
};

struct RAW_CMD_INFO
{
    LPSTR name; LPSTR eg; LPSTR explain;
    short cat; WORD state;
    DWORD ret; WORD reserved;
    short level; short bmpIdx; short bmpCount;
    int nArg; RAW_ARG_INFO* args;
};

struct RAW_PROP
{
    LPSTR name; LPSTR eg; LPSTR explain;
    short type; WORD state; LPSTR picks;
};

struct RAW_EVENT_ARG
{
    LPSTR name; LPSTR explain; DWORD state; DWORD type;
};

struct RAW_EVENT
{
    LPSTR name; LPSTR explain; DWORD state;
    int nArg; RAW_EVENT_ARG* args; DWORD ret;
};

struct RAW_ELEM
{
    DWORD type; LPBYTE arySpec;
    LPSTR name; LPSTR eg; LPSTR explain;
    DWORD state; int def;
};

struct RAW_DT
{
    LPSTR name; LPSTR eg; LPSTR explain;
    int nCmd; int* pnCmds;
    DWORD state; DWORD bmpId;
    int nEvent; RAW_EVENT* events;
    int nProp; RAW_PROP* props;
    void* pfnGetInterface;
    int nElem; RAW_ELEM* elems;
};

struct RAW_CONST
{
    LPSTR name; LPSTR eg; LPSTR explain;
    short layout; short type; LPSTR text;
    double value;
};

struct RAW_LIB
{
    DWORD format; LPSTR guid;
    int maj, min, build;
    int rqSysMaj, rqSysMin, rqKrnlMaj, rqKrnlMin;
    LPSTR name; int language; LPSTR explain; DWORD state;
    LPSTR author, zip, addr, phone, qq, email, home, other;
    int nDataType; RAW_DT* dataType;
    int nCategory; LPSTR categories;
    int nCmd; RAW_CMD_INFO* cmds; void* cmdsFunc;
    void* pfnAddIn; LPSTR szzAddIn;
    void* pfnNotify;
    void* pfnSuper; LPSTR szzSuper;
    int nConst; RAW_CONST* consts;
    LPSTR depend;
};

// ================================================================== lib2.h 常量
#define EC_SDT_MASK     0x80000000UL
#define EC_DT_IS_ARY    0x20000000UL
#define EC_SDT_BOOL     (0x80000000UL | 0x0002UL)     // MAKELONG(MAKEWORD(2,0),0x8000)
#define EC_SDT_INT      (0x80000000UL | 0x0301UL)     // MAKELONG(MAKEWORD(1,3),0x8000)

#define EC_CT_IS_HIDED          (1 << 2)
#define EC_CT_IS_ERROR          (1 << 3)
#define EC_CT_ALLOW_APPEND_ARG  (1 << 5)
#define EC_LDT_IS_HIDED         (1 << 0)
#define EC_LDT_WIN_UNIT         (1 << 6)
#define EC_LDT_IS_CONTAINER     (1 << 7)
#define EC_LDT_IS_FUNC_PROVIDER (1 << 15)
#define EC_LDT_ENUM             (1 << 22)

// 窗口组件型数据类型的前 8 个属性是系统固定属性（lib2.h 的 FIXED_WIN_UNIT_PROPERTY）：
// 提示文本里要列（成员属性<1..8>），但原版的树里不出现。编辑框实测 29 个属性 -> 树里 21 个。
#define EC_FIXED_WIN_PROP_COUNT 8

// 隐含命令（或本语言版本不可用的命令）不进树
static BOOL CmdHidden(DWORD state) { return (state & (EC_CT_IS_HIDED | EC_CT_IS_ERROR)) != 0; }

#define EC_AS_HAS_DEFAULT_VALUE     (1 << 0)
#define EC_AS_DEFAULT_VALUE_IS_EMPTY (1 << 1)

#define EC_LES_HAS_DEFAULT_VALUE    (1 << 0)

#define EC_UW_IS_HIDED              (1 << 4)   // UNIT_PROPERTY：隐藏但可用 -> 树里不出现

#define EC_UD_INT           1001
#define EC_UD_DOUBLE        1002
#define EC_UD_BOOL          1003
#define EC_UD_DATE_TIME     1004
#define EC_UD_TEXT          1005
#define EC_UD_PICK_INT      1006
#define EC_UD_PICK_TEXT     1007
#define EC_UD_EDIT_PICK_TEXT 1008
#define EC_UD_PIC           1009
#define EC_UD_ICON          1010
#define EC_UD_CURSOR        1011
#define EC_UD_MUSIC         1012
#define EC_UD_FONT          1013
#define EC_UD_COLOR         1014
#define EC_UD_COLOR_TRANS   1015
#define EC_UD_FILE_NAME     1016
#define EC_UD_COLOR_BACK    1017
#define EC_UD_IMAGE_LIST    1023
#define EC_UD_CUSTOMIZE     1024
#define EC_UD_PICK_SPEC_INT 1000

#define EC_CT_NULL  0
#define EC_CT_NUM   1
#define EC_CT_BOOL  2
#define EC_CT_TEXT  3

#define EC_LVL_SIMPLE     1
#define EC_LVL_SECONDARY  2
#define EC_LVL_HIGH       3

// 段分隔符：实测原版用的是 " ｜ " 加两格（"事件 N ：" 那一类只用一格）
static const TCHAR* kSeg  = _T(" ｜  ");
static const TCHAR* kSeg1 = _T(" ｜ ");

// ================================================================== 全局数据表
// ⚠️ 用 std::vector 而不是 CArray：MFC 4.2 的 CArray::ConstructElements 是 **memset 0**，
//    根本不会调用元素的构造函数/析构函数，装含 CString 的结构体会直接把堆搞坏
//    （实测：EcTreeNode 的析构在垃圾指针上崩，fault offset 落在 ~EcTreeNode 里）。
static std::vector<EcLib>      g_libs;
static std::vector<EcCmd>      g_cmds;
static std::vector<EcArg>      g_args;
static std::vector<EcDataType> g_dts;
static std::vector<EcProp>     g_props;
static std::vector<EcEvent>    g_events;
static std::vector<EcElem>     g_elems;
static std::vector<EcConst>    g_consts;
static std::vector<HINSTANCE>  g_mods;      // 保持 LoadLibrary 引用（原版也一直挂着）
static std::vector<EcTreeNode> g_nodes;

static LPCTSTR Safe(LPCSTR p) { return (p != NULL && p[0] != '\0') ? (LPCTSTR)p : _T(""); }
static CString SafeS(LPCSTR p) { return CString(Safe(p)); }


static void Chomp(CString& s)
{
    while (!s.IsEmpty())
    {
        TCHAR c = s[s.GetLength() - 1];
        if (c == _T('\r') || c == _T('\n') || c == _T(' ') || c == _T('\t'))
            s.Delete(s.GetLength() - 1);
        else
            break;
    }
}

// 原版的解释串后面自己补句号
static void AddSentence(CString& s, const CString& text)
{
    if (text.IsEmpty())
        return;
    s += text;
    if (s[s.GetLength() - 1] != _T('。'))
        s += _T("。");
}

// ================================================================== 类型名
struct EcSysType { int a, b; LPCTSTR cn; LPCTSTR en; };

static const EcSysType kSysTypes[] =
{
    { 0, 0, _T("通用型"),       _T("all") },
    { 1, 1, _T("字节型"),       _T("byte") },
    { 2, 1, _T("短整数型"),     _T("short") },
    { 3, 1, _T("整数型"),       _T("int") },
    { 4, 1, _T("长整数型"),     _T("int64") },
    { 5, 1, _T("小数型"),       _T("float") },
    { 6, 1, _T("双精度小数型"), _T("double") },
    { 0, 2, _T("逻辑型"),       _T("bool") },
    { 0, 3, _T("日期时间型"),   _T("datetime") },
    { 0, 4, _T("文本型"),       _T("text") },
    { 0, 5, _T("字节集"),       _T("bin") },
    { 0, 6, _T("子程序指针"),   _T("subptr") },
    { 0, 8, _T("子语句型"),     _T("statment") },
};

CString EcTypeName(EC_DATA_TYPE v, int lib, BOOL bShort)
{
    if (v == 0)
        return CString(_T(""));
    if (v & EC_DT_IS_ARY)
        v &= ~EC_DT_IS_ARY;

    CString cn, en;
    if (v & 0x40000000)                      // DTM_USER_DATA_TYPE_MASK：用户自定义类型
    {
        cn = _T("自定义数据类型");
    }
    else if (v & EC_SDT_MASK)
    {
        // MAKELONG(MAKEWORD(x,y),0x8000) = 0x80000000 | (y<<8) | x
        // 表里的 a 对应 y（种类），b 对应 x（子号）——SDT_INT=MAKEWORD(1,3)=0x0301 -> a=3 b=1
        int a = (int)((v >> 8) & 0xFF);
        int b = (int)(v & 0xFF);
        int i;
        for (i = 0; i < sizeof(kSysTypes) / sizeof(kSysTypes[0]); i++)
        {
            if (kSysTypes[i].a == a && kSysTypes[i].b == b)
            {
                cn = kSysTypes[i].cn;
                en = kSysTypes[i].en;
                break;
            }
        }
        if (cn.IsEmpty())
            cn.Format(_T("系统类型(%08X)"), (unsigned)v);
    }
    else if (lib >= 0 && lib < g_libs.size())
    {
        int idx = g_libs[lib].dtStart + (int)v;
        if (idx >= 0 && idx < g_dts.size())
        {
            cn = g_dts[idx].name;
            en = g_dts[idx].eg;
        }
        else
            cn.Format(_T("库类型(%u)"), (unsigned)v);
    }
    else
        cn.Format(_T("库类型(%u)"), (unsigned)v);

    if (bShort || en.IsEmpty())
        return cn;
    CString r;
    r.Format(_T("%s（%s）"), (LPCTSTR)cn, (LPCTSTR)en);
    return r;
}

CString EcPropTypeName(short t)
{
    LPCTSTR cn = _T("整数型");
    LPCTSTR en = _T("int");
    switch (t)
    {
    case EC_UD_DOUBLE:                          cn = _T("双精度小数型"); en = _T("double"); break;
    case EC_UD_BOOL:                            cn = _T("逻辑型");       en = _T("bool");   break;
    case EC_UD_DATE_TIME:                       cn = _T("日期时间型");   en = _T("datetime"); break;
    case EC_UD_TEXT:
    case EC_UD_PICK_TEXT:
    case EC_UD_EDIT_PICK_TEXT:
    case EC_UD_FILE_NAME:                       cn = _T("文本型");       en = _T("text");   break;
    case EC_UD_PIC:
    case EC_UD_ICON:
    case EC_UD_CURSOR:
    case EC_UD_MUSIC:                           cn = _T("字节集");       en = _T("bin");    break;
    default:                                    break;
    }
    CString r;
    r.Format(_T("%s（%s）"), cn, en);
    return r;
}

CString EcOsText(DWORD state, BOOL bIsLib)
{
    DWORD m = bIsLib ? ((state >> 16) & 0xE000) : (state & 0xE000);
    CString r;
    if (m & 0x8000) r += _T("Windows");
    if (m & 0x4000) { if (!r.IsEmpty()) r += _T("、"); r += _T("Linux"); }
    if (m & 0x2000) { if (!r.IsEmpty()) r += _T("、"); r += _T("Unix"); }
    if (r.IsEmpty()) r = _T("Windows");     // 老库（格式号 < 3.6）默认只支持 Windows
    return r;
}

static CString LevelText(int lvl)
{
    switch (lvl)
    {
    case EC_LVL_SECONDARY: return CString(_T("中级"));
    case EC_LVL_HIGH:      return CString(_T("高级"));
    default:               return CString(_T("初级"));
    }
}

static CString LanguageText(int lang)
{
    switch (lang)
    {
    case 1:  return CString(_T("中文(大陆)"));
    case 2:  return CString(_T("中文(台湾)"));
    case 3:  return CString(_T("英文"));
    default: return CString(_T("中文(大陆)"));
    }
}

// 类别串形如 "0001流程控制"：前 4 位是图标序号（原版画图用），显示时要去掉
static CString CatDisplay(const CString& raw)
{
    int i = 0;
    while (i < raw.GetLength() && i < 4 && raw[i] >= _T('0') && raw[i] <= _T('9'))
        i++;
    return raw.Mid(i);
}

// ...而那 4 位数字**就是图标号**，原版拿它当节点的 image index：
//   krnln 实测 [1]"0001流程控制" -> 原版树 img=1     [2]"0014算术运算" -> img=14
//              [3]"0005逻辑比较" -> img=5           [4]"0015位运算"   -> img=15
//              [5]"0002变量操作" -> img=2           [6..]"0000xxx"    -> img=0（无图标）
// 证据：`tools\libprobe.exe "C:\Program Files (x86)\e\lib\krnln.fne"` 的 `-- categories --`
//      与 `shots\live_page2\trees.txt`（原版工作夹树 1560 节点）里每个类别的 img= 逐个相等。
// ⚠️ 这个号必须在这里解析并存进 EcTreeNode::icon：`text` 已经过 CatDisplay()，
//    前缀没了，从显示文本里反解不出来（第一版就是那么写的，结果全树一个图标）。
// ⚠️ 也别改成"按第几个类别数 1,2,3..."：显示顺序与串表顺序可能不同
//    （实测 时间操作/字节集操作 两个类别换位），那样图标会整体错位。
static int CatIconNo(const CString& raw)
{
    int v = 0, i;
    for (i = 0; i < 4 && i < raw.GetLength(); i++)
    {
        if (raw[i] < _T('0') || raw[i] > _T('9'))
            return 0;
        v = v * 10 + (raw[i] - _T('0'));
    }
    return (i == 4) ? v : 0;
}

// m_nDefault 的含义随类型变：逻辑型=真值、文本型=字符串指针、数字型=数值本身
static CString DefaultText(EC_DATA_TYPE type, int def, int lib)
{
    CString tn = EcTypeName(type, lib, TRUE);
    if (tn == _T("逻辑型"))
        return CString(def ? _T("真") : _T("假"));
    if (tn == _T("文本型"))
    {
        if (def == 0)
            return CString(_T(""));
        return CString(Safe((LPCSTR)def));
    }
    CString r;
    r.Format(_T("%d"), def);
    return r;
}

static CString DefValueText(const EcArg& a, int lib)
{
    CString tn = EcTypeName(a.type, lib, TRUE);
    if (tn == _T("逻辑型"))
        return CString(a.def ? _T("真") : _T("假"));
    if (tn == _T("文本型") && a.def != 0)
    {
        CString r;
        r.Format(_T("“%s”"), Safe((LPCSTR)a.def));
        return r;
    }
    CString r;
    r.Format(_T("%d"), a.def);
    return r;
}

// UD_PICK_SPEC_INT 的备选串是成对的（值\0说明\0），其余是 "文本\0" 序列，空串结束
static void ReadPicks(LPCSTR p, std::vector<CString>& out, short type)
{
    if (p == NULL)
        return;
    int guard = 0;
    if (type == EC_UD_PICK_SPEC_INT)
    {
        while (p[0] != '\0' && guard++ < 512)
        {
            CString v = p;
            p += lstrlenA(p) + 1;
            CString t = p;
            p += lstrlenA(p) + 1;
            CString one;
            one.Format(_T("%s. %s"), (LPCTSTR)v, (LPCTSTR)t);
            out.push_back(one);
        }
    }
    else
    {
        int n = 0;
        while (p[0] != '\0' && guard++ < 512)
        {
            CString one;
            one.Format(_T("%d. %s"), n++, (LPCTSTR)p);
            out.push_back(one);
            p += lstrlenA(p) + 1;
        }
    }
}

// ================================================================== 载入
static int LoadOneLib(LPCTSTR pszPath)
{
    typedef RAW_LIB* (WINAPI *PFN_GETNEWINF)(void);

    HINSTANCE h = ::LoadLibrary(pszPath);
    if (h == NULL)
        return -1;
    PFN_GETNEWINF f = (PFN_GETNEWINF)::GetProcAddress(h, "GetNewInf");
    if (f == NULL)
        f = (PFN_GETNEWINF)::GetProcAddress(h, "GetLibInf");     // 3.0 以前的旧名
    if (f == NULL)
    {
        ::FreeLibrary(h);
        return -1;
    }
    RAW_LIB* p = f();
    if (p == NULL || p->format == 0 || p->name == NULL)
    {
        ::FreeLibrary(h);
        return -1;
    }

    EcLib L;
    L.name    = Safe(p->name);
    L.guid    = Safe(p->guid);
    L.explain = Safe(p->explain);
    L.path    = pszPath;
    L.maj = p->maj; L.min = p->min; L.build = p->build;
    L.rqSysMaj = p->rqSysMaj; L.rqSysMin = p->rqSysMin;
    L.rqKrnlMaj = p->rqKrnlMaj; L.rqKrnlMin = p->rqKrnlMin;
    L.language = p->language;
    L.state    = p->state;
    L.author = Safe(p->author);
    L.zip    = Safe(p->zip);
    L.addr   = Safe(p->addr);
    L.phone  = Safe(p->phone);
    L.qq     = Safe(p->qq);
    L.email  = Safe(p->email);
    L.home   = Safe(p->home);
    L.other  = Safe(p->other);
    L.nDataType = p->nDataType;
    L.nCategory = p->nCategory;
    L.nCmd      = p->nCmd;
    L.nConst    = p->nConst;

    // --- 类别表 ---
    if (p->categories != NULL)
    {
        LPCSTR q = p->categories;
        int guard = 0;
        while (q[0] != '\0' && guard++ < p->nCategory + 4)
        {
            L.cats.push_back(q);
            q += lstrlenA(q) + 1;
        }
    }

    g_libs.push_back(L);
    int li = g_libs.size() - 1;
    g_mods.push_back(h);

    // --- 命令 + 参数 ---
    g_libs[li].cmdStart = g_cmds.size();
    if (p->cmds != NULL && p->nCmd > 0 && p->nCmd < 20000)
    {
        int i;
        for (i = 0; i < p->nCmd; i++)
        {
            const RAW_CMD_INFO& r = p->cmds[i];
            EcCmd c;
            c.name     = Safe(r.name);
            c.eg       = Safe(r.eg);
            c.explain  = Safe(r.explain);
            c.category = r.cat;
            c.state    = r.state;
            c.ret      = r.ret;
            c.level    = r.level;
            c.bmpIdx   = r.bmpIdx;
            c.argStart = g_args.size();
            c.argCount = 0;
            if (r.args != NULL && r.nArg > 0 && r.nArg < 200)
            {
                int k;
                for (k = 0; k < r.nArg; k++)
                {
                    const RAW_ARG_INFO& a = r.args[k];
                    EcArg e;
                    e.name    = Safe(a.name);
                    e.explain = Safe(a.explain);
                    e.type    = a.type;
                    e.def     = a.def;
                    e.state   = a.state;
                    g_args.push_back(e);
                }
                c.argCount = r.nArg;
            }
            g_cmds.push_back(c);
        }
    }
    g_libs[li].nCmd = g_cmds.size() - g_libs[li].cmdStart;

    // --- 数据类型 ---
    g_libs[li].dtStart = g_dts.size();
    if (p->dataType != NULL && p->nDataType > 0 && p->nDataType < 4000)
    {
        int i;
        for (i = 0; i < p->nDataType; i++)
        {
            const RAW_DT& r = p->dataType[i];
            EcDataType d;
            d.name    = Safe(r.name);
            d.eg      = Safe(r.eg);
            d.explain = Safe(r.explain);
            d.state   = r.state;
            d.bmpId   = r.bmpId;
            d.pfnGetInterface = r.pfnGetInterface;   // +44 = m_pfnGetInterface（设计器建组件要用）
            d.cmdStart = g_cmds.size();
            d.cmdCount = 0;
            if (r.pnCmds != NULL && r.nCmd > 0 && r.nCmd < 4000)
            {
                int k;
                for (k = 0; k < r.nCmd; k++)
                {
                    int ci = g_libs[li].cmdStart + r.pnCmds[k];
                    if (r.pnCmds[k] >= 0 && r.pnCmds[k] < g_libs[li].nCmd)
                    {
                        // ⚠️ 必须先取副本再 Add：CArray::Add 内部会 SetSize 重分配，
                        //    直接 Add(g_cmds[ci]) 会从已释放的旧缓冲里拷（悬垂引用）。
                        EcCmd tmp = g_cmds[ci];
                        g_cmds.push_back(tmp);
                    }
                }
                d.cmdCount = r.nCmd;
            }
            d.propStart = g_props.size();
            d.propCount = 0;
            if (r.props != NULL && r.nProp > 0 && r.nProp < 2000)
            {
                int k;
                for (k = 0; k < r.nProp; k++)
                {
                    const RAW_PROP& rp = r.props[k];
                    EcProp e;
                    e.name    = Safe(rp.name);
                    e.eg      = Safe(rp.eg);
                    e.explain = Safe(rp.explain);
                    e.type    = rp.type;
                    e.state   = rp.state;
                    ReadPicks(rp.picks, e.picks, rp.type);
                    g_props.push_back(e);
                }
                d.propCount = r.nProp;
            }
            d.eventStart = g_events.size();
            d.eventCount = 0;
            if (r.events != NULL && r.nEvent > 0 && r.nEvent < 4000)
            {
                // ⚠️ 事件数组有**两个版本**，元素大小不同，必须按第一个元素的 m_dwState 判：
                //    EVENT_INFO (v1, 20 字节): name explain state nArg pArgs
                //    EVENT_INFO2(v2, 24 字节): name explain state nArg pArgs ret   (state 带 EV_IS_VER2=1<<31)
                //    实测 krnln 5.7 用的是 **v1**（state=0x40000000，没有 bit31）。
                //    v1 里返回类型和参数类型都编码在 state 位里（EV_RETURN_BOOL 1<<4 / EV_RETURN_INT 1<<3
                //    / EAS_IS_BOOL_ARG 1<<0），没有独立的类型字段。
                //    按 24 字节硬读会把每个事件都读错位 —— 现象是"名字"读出上一项的"解释"，
                //    字符串指针当 lParam 用，最终在 ~EcEvent 里崩。
                const DWORD* ev = (const DWORD*)r.events;
                int stride = (ev[2] & 0x80000000UL) ? 6 : 5;
                int k;
                for (k = 0; k < r.nEvent; k++)
                {
                    const DWORD* e0 = ev + k * stride;
                    EcEvent e;
                    e.name     = Safe((LPCSTR)e0[0]);
                    e.explain  = Safe((LPCSTR)e0[1]);
                    e.state    = e0[2];
                    e.argStart = g_args.size();
                    e.argCount = 0;

                    int    nArg  = (int)e0[3];
                    LPCSTR pargs = (LPCSTR)e0[4];

                    if (stride == 6)
                    {
                        e.ret = e0[5];
                        if (pargs != NULL && nArg > 0 && nArg < 200)
                        {
                            int m;
                            for (m = 0; m < nArg; m++)
                            {
                                const DWORD* a0 = (const DWORD*)pargs + m * 4;
                                EcArg a;
                                a.name    = Safe((LPCSTR)a0[0]);
                                a.explain = Safe((LPCSTR)a0[1]);
                                a.state   = a0[2];
                                a.type    = a0[3];
                                a.def     = 0;
                                g_args.push_back(a);
                            }
                            e.argCount = nArg;
                        }
                    }
                    else
                    {
                        if (e.state & (1 << 4))          // EV_RETURN_BOOL
                            e.ret = EC_SDT_BOOL;
                        else if (e.state & (1 << 3))     // EV_RETURN_INT
                            e.ret = EC_SDT_INT;
                        else
                            e.ret = 0;
                        if (pargs != NULL && nArg > 0 && nArg < 200)
                        {
                            int m;
                            for (m = 0; m < nArg; m++)
                            {
                                const DWORD* a0 = (const DWORD*)pargs + m * 3;
                                EcArg a;
                                a.name    = Safe((LPCSTR)a0[0]);
                                a.explain = Safe((LPCSTR)a0[1]);
                                a.state   = a0[2];
                                a.type    = (a0[2] & 1) ? EC_SDT_BOOL : EC_SDT_INT;
                                a.def     = 0;
                                g_args.push_back(a);
                            }
                            e.argCount = nArg;
                        }
                    }
                    g_events.push_back(e);
                }
                d.eventCount = r.nEvent;
            }
            d.elemStart = g_elems.size();
            d.elemCount = 0;
            if (r.elems != NULL && r.nElem > 0 && r.nElem < 2000)
            {
                int k;
                for (k = 0; k < r.nElem; k++)
                {
                    const RAW_ELEM& rl = r.elems[k];
                    EcElem e;
                    e.name    = Safe(rl.name);
                    e.eg      = Safe(rl.eg);
                    e.explain = Safe(rl.explain);
                    e.type    = rl.type;
                    e.state   = rl.state;
                    e.def     = rl.def;
                    g_elems.push_back(e);
                }
                d.elemCount = r.nElem;
            }
            g_dts.push_back(d);
        }
    }
    g_libs[li].nDataType = g_dts.size() - g_libs[li].dtStart;

    // --- 常量 ---
    g_libs[li].constStart = g_consts.size();
    if (p->consts != NULL && p->nConst > 0 && p->nConst < 20000)
    {
        int i;
        for (i = 0; i < p->nConst; i++)
        {
            const RAW_CONST& r = p->consts[i];
            EcConst c;
            c.name    = Safe(r.name);
            c.eg      = Safe(r.eg);
            c.explain = Safe(r.explain);
            c.type    = r.type;
            c.text    = Safe(r.text);
            c.value   = r.value;
            g_consts.push_back(c);
        }
    }
    g_libs[li].nConst = g_consts.size() - g_libs[li].constStart;
    return li;
}

static void ScanLibDir(LPCTSTR pszDir, const CStringArray* pLoad,
                       std::vector<CString>& found)
{
    CString pat;
    pat.Format(_T("%s\\*.fne"), pszDir);
    CFileFind ff;
    BOOL b = ff.FindFile(pat);
    while (b)
    {
        b = ff.FindNextFile();
        if (ff.IsDirectory())
            continue;
        CString base = ff.GetFileTitle();
        base.MakeLower();
        if (pLoad != NULL)
        {
            BOOL want = FALSE;
            int i;
            for (i = 0; i < pLoad->GetSize(); i++)
            {
                CString w = (*pLoad)[i];
                w.MakeLower();
                if (w == base)
                {
                    want = TRUE;
                    break;
                }
            }
            if (!want)
                continue;
        }
        found.push_back(ff.GetFilePath());
    }
    ff.Close();
}

BOOL EcLoadLibs(LPCTSTR pszLibDir, const CStringArray* pLoad, CString* pszLog)
{
    std::vector<CString> files;
    ScanLibDir(pszLibDir, pLoad, files);

    int i;
    for (i = 0; i < files.size(); i++)
    {
        int li = LoadOneLib(files[i]);
        if (pszLog != NULL)
        {
            CString line;
            if (li < 0)
                line.Format(_T("载入支持库失败：%s\r\n"), (LPCTSTR)files[i]);
            else
                line.Format(_T("已载入 %s (%d.%d.%d, 命令 %d 条 / 数据类型 %d 个 / 常量 %d 个)\r\n"),
                            (LPCTSTR)g_libs[li].name, g_libs[li].maj, g_libs[li].min,
                            g_libs[li].build, g_libs[li].nCmd, g_libs[li].nDataType,
                            g_libs[li].nConst);
            *pszLog += line;
        }
    }
    return g_libs.size() > 0;
}

// ================================================================== 访问
int EcLibCount() { return g_libs.size(); }

// ------------------------------------------------------------------ 动态载入/卸载
int EcScanLibFiles(LPCTSTR pszLibDir, CStringArray* pOut)
{
    if (pOut == NULL)
        return 0;
    pOut->RemoveAll();
    std::vector<CString> found;
    ScanLibDir(pszLibDir, NULL, found);
    std::sort(found.begin(), found.end());
    int i;
    for (i = 0; i < (int)found.size(); i++)
        pOut->Add(found[i]);
    return pOut->GetSize();
}

BOOL EcLibIsLoaded(LPCTSTR pszPath)
{
    if (pszPath == NULL)
        return FALSE;
    int i;
    for (i = 0; i < (int)g_libs.size(); i++)
        if (g_libs[i].path.CompareNoCase(pszPath) == 0)
            return TRUE;
    return FALSE;
}

BOOL EcUnloadAllLibs()
{
    int i;
    for (i = 0; i < (int)g_mods.size(); i++)
        if (g_mods[i] != NULL)
            ::FreeLibrary(g_mods[i]);
    g_mods.clear();
    g_libs.clear();
    g_cmds.clear();
    g_args.clear();
    g_dts.clear();
    g_props.clear();
    g_events.clear();
    g_elems.clear();
    g_consts.clear();
    g_nodes.clear();
    return TRUE;
}

const EcLib& EcLibAt(int i) { return g_libs[i]; }

int EcLibFindByBaseName(LPCTSTR pszBase)
{
    CString want = pszBase;
    want.MakeLower();
    int i;
    for (i = 0; i < g_libs.size(); i++)
    {
        CString base = g_libs[i].path;
        int p = base.ReverseFind(_T('\\'));
        if (p >= 0)
            base = base.Mid(p + 1);
        int d = base.ReverseFind(_T('.'));
        if (d >= 0)
            base = base.Left(d);
        base.MakeLower();
        if (base == want)
            return i;
    }
    return -1;
}

// ================================================================== 提示文本
static CString LibHint(int li)
{
    const EcLib& L = g_libs[li];
    CString s;
    s.Format(_T(" 支持库名称及版本：%s （%d.%d#%d版）"),
             (LPCTSTR)L.name, L.maj, L.min, L.build);
    s += _T(" ｜  所支持语言：");
    s += LanguageText(L.language);
    if (!L.explain.IsEmpty())
    {
        s += kSeg;
        s += L.explain;
    }
    s += kSeg;
    {
        CString t;
        t.Format(_T("本库为易语言系统的基本库之一，需要易系统%d.%d版本的支持，"
                    "提供了%d种库定义数据类型，提供了%d种命令，提供了%d个库定义常量。"),
                 L.rqSysMaj, L.rqSysMin, L.nDataType, L.nCmd, L.nConst);
        s += t;
    }
    s += kSeg;
    s += _T("操作系统需求： ");
    s += EcOsText(L.state, TRUE);
    s += kSeg;
    s += _T("支持库文件名： ");
    s += L.path;
    s += _T(" ｜ ----- 支持库的作者信息 -----");
    if (!L.author.IsEmpty()) { s += _T(" ｜ 作者姓名："); s += L.author; }
    if (!L.zip.IsEmpty())    { s += _T(" ｜ 邮政编码："); s += L.zip; }
    if (!L.addr.IsEmpty())   { s += _T(" ｜ 通信地址："); s += L.addr; }
    if (!L.phone.IsEmpty())  { s += _T(" ｜ 电话号码："); s += L.phone; }
    if (!L.qq.IsEmpty())     { s += _T(" ｜ QQ号码：");   s += L.qq; }
    if (!L.email.IsEmpty())  { s += _T(" ｜ 电子信箱："); s += L.email; }
    if (!L.home.IsEmpty())   { s += _T(" ｜ 主页地址："); s += L.home; }
    if (!L.other.IsEmpty())  { s += _T(" ｜ 其它信息："); s += L.other; }
    return s;
}

static CString StdCatHint(int kind)     // 0=命令类别 1=数据类型 2=常量
{
    if (kind == 0)
        return CString(_T("命令类别将支持库中的命令分为若干类，以便于用户寻找所需要的命令。"));
    if (kind == 1)
        return CString(_T("库定义数据类型列表。"));
    return CString(_T("库常量为支持库所定义的逻辑值、数值或文本，可以直接在程序中使用。"));
}

static CString CmdHint(int li, int ci, int dti)
{
    const EcLib& L = g_libs[li];
    const EcCmd& C = g_cmds[ci];
    CString s, t;

    s = _T(" 调用格式： 〈");
    s += (C.ret != 0) ? EcTypeName(C.ret, li, TRUE) : CString(_T("无返回值"));
    s += _T("〉 ");
    if (dti >= 0)
        s += _T("对象．");
    s += C.name;
    s += _T(" （");

    CString argList;
    int i;
    for (i = 0; i < C.argCount; i++)
    {
        const EcArg& A = g_args[C.argStart + i];
        CString one;
        one.Format(_T("%s %s"), (LPCTSTR)EcTypeName(A.type, li, TRUE), (LPCTSTR)A.name);
        if (A.state & (EC_AS_HAS_DEFAULT_VALUE | EC_AS_DEFAULT_VALUE_IS_EMPTY))
            one = CString(_T("［")) + one + _T("］");
        if (!argList.IsEmpty())
            argList += _T("，");
        argList += one;
    }
    if (C.state & EC_CT_ALLOW_APPEND_ARG)
    {
        if (!argList.IsEmpty())
            argList += _T("，");
        argList += _T("... ");
    }
    s += argList;
    s += _T("）");
    s += _T(" - ");
    s += L.name;
    s += _T("->");
    if (dti >= 0 && dti < L.nDataType)
        s += g_dts[L.dtStart + dti].name;
    else if (C.category >= 1 && C.category <= L.cats.size())
        s += CatDisplay(L.cats[C.category - 1]);

    if (!C.eg.IsEmpty())
    {
        s += kSeg;
        s += _T("英文名称：");
        s += C.eg;
    }

    s += kSeg;
    if (!C.explain.IsEmpty())
        AddSentence(s, C.explain);
    s += _T("本命令为");
    s += LevelText(C.level);
    s += (dti >= 0) ? _T("对象成员命令。") : _T("命令。");
    if (C.state & EC_CT_ALLOW_APPEND_ARG)
        s += _T("命令参数表中最后一个参数可以被重复添加。");

    for (i = 0; i < C.argCount; i++)
    {
        const EcArg& A = g_args[C.argStart + i];
        s += kSeg;
        t.Format(_T("参数<%d>的名称为“%s”，类型为“%s”"),
                 i + 1, (LPCTSTR)A.name, (LPCTSTR)EcTypeName(A.type, li, FALSE));
        if (A.state & (EC_AS_HAS_DEFAULT_VALUE | EC_AS_DEFAULT_VALUE_IS_EMPTY))
            t += _T("，可以被省略");
        t += _T("。");
        if (!A.explain.IsEmpty())
            AddSentence(t, A.explain);
        if (A.state & EC_AS_HAS_DEFAULT_VALUE)
        {
            t += _T("如果被省略，则参数默认值为");
            t += DefValueText(A, li);
            t += _T("。");
        }
        s += t;
    }

    s += kSeg;
    s += _T("操作系统需求： ");
    s += EcOsText(C.state, FALSE);
    return s;
}

static void AppendEventRest(CString& s, int li, const EcEvent& E);

static void AppendPicks(CString& s, const EcProp& P){
    // UD_FILE_NAME 的 m_szzPickStr 不是备选值，而是 "对话框标题\0过滤器\0默认后缀\0存/读标志\0"，不能当列表列
    if (P.picks.size() == 0 || P.type == EC_UD_FILE_NAME)
        return;
    s += kSeg;
    s += _T("可供选择的属性值：");
    int i;
    for (i = 0; i < P.picks.size(); i++)
    {
        s += kSeg;
        s += P.picks[i];
    }
}

static CString DtHint(int li, int di)
{
    const EcLib& L = g_libs[li];
    const EcDataType& D = g_dts[L.dtStart + di];
    CString s, t;

    t.Format(_T(" 数据类型名称：%s 英文名称：%s"), (LPCTSTR)D.name, (LPCTSTR)D.eg);
    s = t;

    CString kind = _T("普通类型");
    if (D.state & EC_LDT_IS_FUNC_PROVIDER)
        kind = _T("功能窗口组件型");
    else if (D.state & EC_LDT_ENUM)
        kind = _T("枚举类型");

    std::vector<CString> parts;
    TCHAR buf[64];
    if (D.cmdCount > 0)  { wsprintf(buf, _T("%d 个成员命令"), D.cmdCount);  parts.push_back(buf); }
    if (D.propCount > 0) { wsprintf(buf, _T("%d 个成员属性"), D.propCount); parts.push_back(buf); }
    if (D.eventCount > 0){ wsprintf(buf, _T("%d 个组件事件"), D.eventCount);parts.push_back(buf); }
    if (parts.size() == 0 && D.elemCount > 0)
    { wsprintf(buf, _T("%d 个成员变量"), D.elemCount); parts.push_back(buf); }

    if (parts.size() > 0)
    {
        s += kSeg;
        s += _T("本数据类型为");
        s += kind;
        s += _T("，共包含");
        int i;
        for (i = 0; i < parts.size(); i++)
        {
            s += _T(" ");
            s += parts[i];
            s += (i + 1 < parts.size()) ? _T("，") : _T("。");
        }
    }

    s += kSeg;
    s += _T("操作系统需求： ");
    s += EcOsText(D.state, TRUE);

    if (!D.explain.IsEmpty())
    {
        s += kSeg;
        AddSentence(s, D.explain);
    }

    int i;
    for (i = 0; i < D.propCount; i++)
    {
        const EcProp& P = g_props[D.propStart + i];
        s += kSeg;
        t.Format(_T("成员属性<%d>的中文名称为“%s”，英文名称为“%s”，类型为“%s”。"),
                 i + 1, (LPCTSTR)P.name, (LPCTSTR)P.eg, (LPCTSTR)EcPropTypeName(P.type));
        s += t;
        if (!P.explain.IsEmpty())
        {
            s += kSeg;
            AddSentence(s, P.explain);
        }
        AppendPicks(s, P);
    }

    // 成员变量只在"没有命令/属性/事件"的类型下列出（实测 字体/字段信息/打印设置信息 列，
    // 对象(cmd=27)/变体型(cmd=14) 不列）
    if (D.cmdCount == 0 && D.propCount == 0 && D.eventCount == 0)
    for (i = 0; i < D.elemCount; i++)
    {
        const EcElem& E = g_elems[D.elemStart + i];
        s += kSeg;
        t.Format(_T("成员<%d>的中文名称为“%s”，英文名称为“%s”，类型为“%s”"),
                 i + 1, (LPCTSTR)E.name, (LPCTSTR)E.eg, (LPCTSTR)EcTypeName(E.type, li, FALSE));
        if (E.state & EC_LES_HAS_DEFAULT_VALUE)
        {
            CString dv;
            dv.Format(_T("，初始值为“%s”"), (LPCTSTR)DefaultText(E.type, E.def, li));
            t += dv;
        }
        t += _T("。");
        s += t;
        if (!E.explain.IsEmpty())
        {
            s += kSeg;
            AddSentence(s, E.explain);
        }
    }

    s += kSeg;
    s += _T("组件事件列表：");
    for (i = 0; i < D.eventCount; i++)
    {
        const EcEvent& E = g_events[D.eventStart + i];
        s += kSeg1;
        t.Format(_T("事件 %d ："), i + 1);
        s += t;
        s += kSeg;
        s += _T("事件名称：");
        s += E.name;
        AppendEventRest(s, li, E);
    }
    return s;
}

// 事件正文（解释 + 返回值 + 事件参数）。前提：调用方已经把名字段写好
// （" ｜  事件名称：xxx"，或数据类型提示里的 " ｜ 事件 N ： ｜  事件名称：xxx"）。
// 实测原版格式：
//   事件名称：空闲 ｜  <解释> ｜  此事件的用户处理子程序所需返回数据的类型为“逻辑型（bool）”。 ｜  事件参数<1>的名称为“已空闲时间”，类型为“整数型（int）”。<解释>
static void AppendEventRest(CString& s, int li, const EcEvent& E)
{
    CString t;
    if (!E.explain.IsEmpty())
    {
        s += kSeg;
        AddSentence(s, E.explain);
    }
    s += kSeg;
    if (E.ret != 0)
    {
        t.Format(_T("此事件的用户处理子程序所需返回数据的类型为“%s”。"),
                 (LPCTSTR)EcTypeName(E.ret, li, FALSE));
        s += t;
    }
    else
        s += _T("此事件的用户处理子程序不需要返回数据。");

    int k;
    for (k = 0; k < E.argCount; k++)
    {
        const EcArg& A = g_args[E.argStart + k];
        s += kSeg;
        t.Format(_T("事件参数<%d>的名称为“%s”，类型为“%s”。"),
                 k + 1, (LPCTSTR)A.name, (LPCTSTR)EcTypeName(A.type, li, FALSE));
        if (!A.explain.IsEmpty())
            AddSentence(t, A.explain);
        s += t;
    }
}

static CString EventHint(int li, int di, int ei)
{
    const EcDataType& D = g_dts[g_libs[li].dtStart + di];
    const EcEvent& E = g_events[D.eventStart + ei];
    CString s = _T(" 事件名称：");
    s += E.name;
    AppendEventRest(s, li, E);
    return s;
}

// 子数据成员节点。普通类型： 成员“字体大小”所在数据类型为“字体”，英文名称为“FontSize”，类型为“整数型（int）”，初始值为“9”。 ｜  <解释>
// 枚举类型：             枚举常量值“数值型”所处集合数据类型为“变体类型”，英文名称为“Number”，值为1。
static CString ElemHint(int li, int di, int ei)
{
    const EcDataType& D = g_dts[g_libs[li].dtStart + di];
    const EcElem& E = g_elems[D.elemStart + ei];
    CString s, t;
    if (D.state & EC_LDT_ENUM)
    {
        t.Format(_T(" 枚举常量值“%s”所处集合数据类型为“%s”，英文名称为“%s”，值为%d。"),
                 (LPCTSTR)E.name, (LPCTSTR)D.name, (LPCTSTR)E.eg, E.def);
    }
    else
    {
        t.Format(_T(" 成员“%s”所在数据类型为“%s”，英文名称为“%s”，类型为“%s”"),
                 (LPCTSTR)E.name, (LPCTSTR)D.name, (LPCTSTR)E.eg,
                 (LPCTSTR)EcTypeName(E.type, li, FALSE));
        if (E.state & EC_LES_HAS_DEFAULT_VALUE)
        {
            CString dv;
            dv.Format(_T("，初始值为“%s”"), (LPCTSTR)DefaultText(E.type, E.def, li));
            t += dv;
        }
        t += _T("。");
    }
    s = t;
    if (!E.explain.IsEmpty())
    {
        s += kSeg;
        AddSentence(s, E.explain);
    }
    return s;
}

static CString ConstHint(int li, int ki)
{
    const EcConst& C = g_consts[g_libs[li].constStart + ki];
    CString s, t;
    s.Format(_T(" 名称：%s"), (LPCTSTR)C.name);
    if (!C.explain.IsEmpty())
    {
        s += kSeg;
        AddSentence(s, C.explain);
    }
    s += kSeg;
    LPCTSTR tn = _T("空");
    CString val;
    switch (C.type)
    {
    case EC_CT_NUM:
        tn = _T("数值型");
        if (C.value == (double)(int)C.value)
            val.Format(_T("%d"), (int)C.value);
        else
            val.Format(_T("%g"), C.value);
        break;
    case EC_CT_BOOL:
        tn = _T("逻辑型");
        val = C.value != 0.0 ? _T("真") : _T("假");
        break;
    case EC_CT_TEXT:
        tn = _T("文本型");
        val.Format(_T("“%s”"), (LPCTSTR)C.text);
        break;
    default:
        tn = _T("空");
        break;
    }
    t.Format(_T("类型：%s 值：%s。"), tn, (LPCTSTR)val);
    s += t;
    return s;
}

static CString PropHint(int li, int di, int pi)
{
    const EcDataType& D = g_dts[g_libs[li].dtStart + di];
    const EcProp& P = g_props[D.propStart + pi];
    CString s, t;
    t.Format(_T(" 成员属性“%s”所在数据类型为“%s”，英文名称为“%s”，类型为“%s”。"),
             (LPCTSTR)P.name, (LPCTSTR)D.name, (LPCTSTR)P.eg,
             (LPCTSTR)EcPropTypeName(P.type));
    s = t;
    if (!P.explain.IsEmpty())
    {
        s += kSeg;
        AddSentence(s, P.explain);
    }
    AppendPicks(s, P);
    return s;
}

// ================================================================== 建树
void EcBuildTree()
{
    g_nodes.clear();
    int li;
    for (li = 0; li < g_libs.size(); li++)
    {
        const EcLib& L = g_libs[li];
        EcTreeNode n;

        n.kind = EN_LIB; n.a = li; n.b = 0; n.c = 0; n.icon = 0; n.text = L.name;
        g_nodes.push_back(n);

        // 命令类别（空类别不出现，例如核心库的"易向导"3 条命令全是隐含命令）
        //
        // ★★ 第 24 轮：类别的**显示顺序**。
        // 实测（`python tools\tree_img_rule.py` 打表）：
        //   原版显示顺序 = 支持库串表顺序，**只有一处旋转**：
        //     串表 : … 文本操作, 字节集操作, 数值转换, 时间操作 …
        //     显示 : … 文本操作, 时间操作, 数值转换, 字节集操作 …
        //   （老文档里记的"类别顺序差 3 项"就是这个。）
        // 而原版给树图标分配的 image index 是**按显示位置 1..24** 排的
        //   （dump 里的 img= 是 10 23 14 24 11 1 1 … 1 30 1 1 1 5 6，正好是 1..24 的一个排列），
        //   所以 `src\res\tree_icons.bmp` 的第 N 格 = 显示位置 N 的类别图标。
        // ⚠️ 不能按"4 位前缀升序"排：前缀是 0000 的那 16 个类别会全部挤到最前面。
        // ⚠️ 排序只影响**树里的显示次序**；命令归属仍然用 `C.category == cat`（串表下标），
        //    所以绝不能去改 L.cats 本身。
        int cat;
        int nCats = (int)L.cats.size();
        std::vector<int> ord;                  // 串表下标（1 基），按显示次序
        ord.reserve(nCats);
        for (cat = 1; cat <= nCats; cat++)
            ord.push_back(cat);
        {
            // 唯一的次序差：把 [字节集操作, 数值转换, 时间操作] 旋转成
            //                         [时间操作, 数值转换, 字节集操作]
            int i2;
            for (i2 = 0; i2 + 2 < (int)ord.size(); i2++)
            {
                CString a = CatDisplay(L.cats[ord[i2] - 1]);
                CString b = CatDisplay(L.cats[ord[i2 + 1] - 1]);
                CString c2 = CatDisplay(L.cats[ord[i2 + 2] - 1]);
                if (a == _T("字节集操作") && b == _T("数值转换") && c2 == _T("时间操作"))
                {
                    int tmp = ord[i2];
                    ord[i2] = ord[i2 + 2];
                    ord[i2 + 2] = ord[i2 + 1];
                    ord[i2 + 1] = tmp;
                    break;
                }
            }
        }

        int oi;
        for (oi = 0; oi < (int)ord.size(); oi++)
        {
            cat = ord[oi];
            int ci, cnt = 0;
            for (ci = 0; ci < L.nCmd; ci++)
            {
                const EcCmd& C = g_cmds[L.cmdStart + ci];
                if (C.category == cat && !CmdHidden(C.state))
                    cnt++;
            }
            if (cnt == 0)
                continue;

            n.kind = EN_CMD_CAT; n.a = li; n.b = cat; n.c = 0;
            // ★ 图标号 = **显示位置**（1 基，即 ord 里的序号 + 1）。
            //   `src\res\tree_icons.bmp` 的第 p 格就是原版树里**第 p 个类别**的图标 ——
            //   tools\build_icons_identity.py 逐行从 shots\live2\frame.png 抠出来的
            //   （原版树里第 p 个类别在第 y = 167 + 14*(p-1) 行），位置一一对应。
            //   这样"显示位置 -> 格子号"是恒等映射，中间没有任何换算，不可能再错位。
            // ⚠️ 别用支持库类别串的 4 位前缀：那是**串表**次序，与显示次序不同
            //    （时间操作/数值转换/字节集操作 三个换过位），用了就整体错位一位。
            // ⚠️ 也别用 CatIconNo()：它只保证"支持库自己声明了图标的那些类别"是对的。
            n.icon = oi + 1;
            n.text = CatDisplay(L.cats[cat - 1]);
            g_nodes.push_back(n);

            for (ci = 0; ci < L.nCmd; ci++)
            {
                const EcCmd& C = g_cmds[L.cmdStart + ci];
                if (C.category != cat || CmdHidden(C.state))
                    continue;
                // 命令图标：原版 dump 里 depth=4 的 img 随类别而变（流程控制下 12/17/25/26/28/2，
                // 其余类别基本都是 2），EClone 目前统一给"默认图标"3。
                n.kind = EN_CMD; n.icon = 3;
                n.a = li; n.b = ci; n.c = 0; n.text = C.name;
                g_nodes.push_back(n);
            }
        }

        // 数据类型（含组件），隐含的不要
        n.kind = EN_DT_CAT; n.a = li; n.b = 0; n.c = 0; n.icon = 24; n.text = _T("数据类型");
        g_nodes.push_back(n);
        int di;
        for (di = 0; di < L.nDataType; di++)
        {
            const EcDataType& D = g_dts[L.dtStart + di];
            if (D.state & EC_LDT_IS_HIDED)
                continue;
            n.kind = EN_DT; n.a = li; n.b = di; n.c = 0; n.icon = 24; n.text = D.name;
            g_nodes.push_back(n);

            int k;
            for (k = 0; k < D.cmdCount; k++)
            {
                const EcCmd& C = g_cmds[D.cmdStart + k];
                if (CmdHidden(C.state))
                    continue;
                n.kind = EN_MEMBER; n.icon = 8; n.a = li; n.b = di; n.c = k; n.text = C.name;
                g_nodes.push_back(n);
            }
            // ⚠️ 窗口组件型的前 8 个属性是系统固定属性，原版的树里不出现（提示文本里才列）
            int propFrom = (D.state & EC_LDT_WIN_UNIT) ? EC_FIXED_WIN_PROP_COUNT : 0;
            for (k = propFrom; k < D.propCount; k++)
            {
                if (g_props[D.propStart + k].state & EC_UW_IS_HIDED)
                    continue;
                n.kind = EN_PROP; n.icon = 8; n.a = li; n.b = di; n.c = k;
                n.text = g_props[D.propStart + k].name;
                g_nodes.push_back(n);
            }
            for (k = 0; k < D.eventCount; k++)
            {
                n.kind = EN_EVENT; n.icon = 8; n.a = li; n.b = di; n.c = k;
                n.text = g_events[D.eventStart + k].name;
                g_nodes.push_back(n);
            }
            // 子数据成员只在"没有成员命令/属性/事件"的类型下进树（实测：字体/字段信息/打印设置信息/
            // 变体类型 有，对象(elem=2,cmd=27)/变体型(elem=4,cmd=14) 没有）
            if (D.cmdCount == 0 && D.propCount == 0 && D.eventCount == 0)
            {
                for (k = 0; k < D.elemCount; k++)
                {
                    n.kind = EN_ELEM; n.icon = 8; n.a = li; n.b = di; n.c = k;
                    n.text = g_elems[D.elemStart + k].name;
                    g_nodes.push_back(n);
                }
            }
        }

        // 常量
        n.kind = EN_CONST_CAT; n.a = li; n.b = 0; n.c = 0; n.icon = 25; n.text = _T("常量");
        g_nodes.push_back(n);
        int ki;
        for (ki = 0; ki < L.nConst; ki++)
        {
            n.kind = EN_CONST; n.icon = 25; n.a = li; n.b = ki; n.c = 0;
            n.text = g_consts[L.constStart + ki].name;
            g_nodes.push_back(n);
        }
    }
}

int EcTreeCount() { return g_nodes.size(); }

const EcTreeNode& EcTreeNodeAt(int i) { return g_nodes[i]; }

// ------------------------------------------------------------------ 名字分类
// 编辑器着色要区分「支持库命令」和「普通变量」，直接复用已经载入的命令/数据类型表。
// 建一次 CMap 缓存（载入支持库之后再调用，表大小没变就直接命中）。
int EcNameKind(LPCTSTR pszName)
{
    static CMap<CString, LPCTSTR, int, int> s_map;
    static size_t s_cmd = (size_t)-1, s_dt = (size_t)-1, s_cn = (size_t)-1;

    if (pszName == NULL || pszName[0] == 0)
        return 0;

    if (s_cmd != g_cmds.size() || s_dt != g_dts.size() || s_cn != g_consts.size())
    {
        s_map.RemoveAll();
        size_t i;
        for (i = 0; i < g_cmds.size(); i++)
            if (!g_cmds[i].name.IsEmpty()) s_map[g_cmds[i].name] = 1;
        for (i = 0; i < g_dts.size(); i++)
            if (!g_dts[i].name.IsEmpty()) s_map[g_dts[i].name] = 2;
        for (i = 0; i < g_consts.size(); i++)
            if (!g_consts[i].name.IsEmpty()) s_map[g_consts[i].name] = 3;
        s_cmd = g_cmds.size(); s_dt = g_dts.size(); s_cn = g_consts.size();
    }

    int v = 0;
    if (s_map.Lookup(CString(pszName), v))
        return v;

    // 系统数据类型不在任何 .fne 的 LIB_DATA_TYPE_INFO 里（是编码值），单独列
    static const TCHAR* s_sys[] = {
        _T("字节型"), _T("短整数型"), _T("整数型"), _T("长整数型"), _T("小数型"),
        _T("双精度小数型"), _T("逻辑型"), _T("日期时间型"), _T("文本型"), _T("字节集"),
        _T("子程序指针"), _T("子语句型"), _T("变体型"), _T("通用型"), _T("对象"),
        _T("窗口"), _T("菜单"), _T("字体"), NULL
    };
    for (int i = 0; s_sys[i]; i++)
        if (_tcscmp(s_sys[i], pszName) == 0) return 2;

    return 0;
}

// ------------------------------------------------------------------ 参数名（「参数归属线」用）
// 原版在语句下面按参数个数各画一行「※参数名：值」；参数名只有支持库里有，
// 所以按命令名回查已经载入的 EcCmd 参数表（eg 英文名也认）。
BOOL EcCmdArgsByName(LPCTSTR pszName, std::vector<CString>* pNames,
                     std::vector<CString>* pTypes, std::vector<BOOL>* pDef)
{
    if (pNames) pNames->clear();
    if (pTypes) pTypes->clear();
    if (pDef)   pDef->clear();
    if (pszName == NULL || pszName[0] == 0)
        return FALSE;

    CString want(pszName);
    for (size_t ci = 0; ci < g_cmds.size(); ci++)
    {
        const EcCmd& C = g_cmds[ci];
        if (C.name != want && (C.eg.IsEmpty() || C.eg != want))
            continue;
        // 找这个命令属于哪个库（参数类型名要按库解释）
        int li = 0;
        for (size_t k = 0; k < g_libs.size(); k++)
        {
            const EcLib& L = g_libs[k];
            if ((int)ci >= L.cmdStart && (int)ci < L.cmdStart + L.nCmd) { li = (int)k; break; }
        }
        for (int i = 0; i < C.argCount; i++)
        {
            const EcArg& A = g_args[C.argStart + i];
            if (pNames) pNames->push_back(A.name);
            if (pTypes) pTypes->push_back(EcTypeName(A.type, li, TRUE));
            if (pDef)   pDef->push_back((A.state & (EC_AS_HAS_DEFAULT_VALUE |
                                                   EC_AS_DEFAULT_VALUE_IS_EMPTY)) ? TRUE : FALSE);
        }
        return TRUE;
    }
    return FALSE;
}

CString EcNodeHint(int i)
{
    if (i < 0 || i >= g_nodes.size())
        return CString(_T(""));

    const EcTreeNode& n = g_nodes[i];
    switch (n.kind)
    {
    case EN_LIB:       return LibHint(n.a);
    case EN_CMD_CAT:   return StdCatHint(0);
    case EN_DT_CAT:    return StdCatHint(1);
    case EN_CONST_CAT: return StdCatHint(2);
    case EN_CMD:       return CmdHint(n.a, g_libs[n.a].cmdStart + n.b, -1);
    case EN_DT:        return DtHint(n.a, n.b);
    case EN_MEMBER:
        return CmdHint(n.a, g_dts[g_libs[n.a].dtStart + n.b].cmdStart + n.c, n.b);
    case EN_PROP:      return PropHint(n.a, n.b, n.c);
    case EN_EVENT:     return EventHint(n.a, n.b, n.c);
    case EN_ELEM:      return ElemHint(n.a, n.b, n.c);
    case EN_CONST:     return ConstHint(n.a, n.b);
    default:           return CString(_T(""));
    }
}

// ================================================================== 设计器：用支持库的 ITF 接口建真组件窗口
// 全部依据 lib2.h（ITF_*/PFN_CREATE_UNIT）+ krnln 伪 C（window_designer_notes.md §3/§7）。
// 调用约定：PFN_GET_INTERFACE 与 PFN_CREATE_UNIT 都是 WINAPI(stdcall)，PFN_ON_NOTIFY_UNIT 也是。

const EcDataType* EcDtAt(int lib, int dt)
{
    if (lib < 0 || lib >= (int)g_libs.size())
        return NULL;
    const EcLib& L = g_libs[lib];
    if (dt < 0 || dt >= L.nDataType)
        return NULL;
    int k = L.dtStart + dt;
    if (k < 0 || k >= (int)g_dts.size())
        return NULL;
    return &g_dts[k];
}

BOOL EcDtIsUnit(int lib, int dt)
{
    const EcDataType* d = EcDtAt(lib, dt);
    return (d != NULL && (d->state & EC_LDT_WIN_UNIT)) ? TRUE : FALSE;
}

BOOL EcDtIsVisualUnit(int lib, int dt)
{
    const EcDataType* d = EcDtAt(lib, dt);
    if (d == NULL || !(d->state & EC_LDT_WIN_UNIT))
        return FALSE;
    // 「仅用作提供功能的窗口组件」（时钟/通用对话框/打印机/数据提供者…）没有可视外形，
    // 原版设计器的工具箱把它们和可视组件分开；这里只让可视组件能放到窗体上。
    if (d->state & EC_LDT_IS_FUNCTION_PROVIDER)
        return FALSE;
    return TRUE;
}

// 支持库自己的模块句柄。组件的图标是**这个 .fne 里的 RT_BITMAP 资源**，
// 资源号 = LIB_DATA_TYPE_INFO.dwBmpID（DT+20，见 lib2.h 的 m_nBmpID）。
// 实测 krnln.fne 的 RT_BITMAP 有 261..272 / 1002..1038 / 1080..1147 …
// 和 EcDumpDtIconsIfAsked() 倒出来的 bmpId 逐个对得上（tools\pe_bmp_ids.py）。
HINSTANCE EcLibModule(int lib)
{
    if (lib < 0 || lib >= (int)g_mods.size())
        return NULL;
    return g_mods[lib];
}

// ------------------------------------------------------------------ 组件箱图标诊断
// `ECLONE_DUMP_DTICON=<path>`：把每个库的**窗口组件类型**的图标字段倒出来，一行一个：
//     LIB <libIndex> <libBaseName>
//     DT  <libIndex> <dtIndex> state=0xXXXXXXXX bmpId=<十进制> eg=<英文名> name=<中文名>
// bmpId = LIB_DATA_TYPE_INFO +20 的 m_nBmpID（lib2.h 里的字段，见 HANDOFF 3.9-28）。
// 组件箱（原版是一个图标网格）就是靠它拿每个组件的图标。
void EcDumpDtIconsIfAsked()
{
    TCHAR szPath[MAX_PATH];
    DWORD n = ::GetEnvironmentVariable(_T("ECLONE_DUMP_DTICON"), szPath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return;

    CStdioFile f;
    if (!f.Open(szPath, CFile::modeCreate | CFile::modeWrite | CFile::typeText))
        return;

    CString s;
    for (int i = 0; i < (int)g_libs.size(); i++)
    {
        const EcLib& L = g_libs[i];
        s.Format(_T("LIB %d %s\n"), i, (LPCTSTR)L.name);
        f.WriteString(s);
        for (int k = 0; k < L.nDataType; k++)
        {
            const EcDataType* d = EcDtAt(i, k);
            if (d == NULL || !(d->state & EC_LDT_WIN_UNIT))
                continue;
            s.Format(_T("DT %d %d state=0x%08X bmpId=%u vis=%d eg=%s name=%s\n"),
                     i, k, (unsigned)d->state, (unsigned)d->bmpId,
                     EcDtIsVisualUnit(i, k) ? 1 : 0,
                     (LPCTSTR)d->eg, (LPCTSTR)d->name);
            f.WriteString(s);
        }
    }
    f.Close();
}

void* EcDtInterface(int lib, int dt, int itfNo)
{
    const EcDataType* d = EcDtAt(lib, dt);
    if (d == NULL || d->pfnGetInterface == NULL)
        return NULL;
    typedef void* (WINAPI *PFN_GET_INTERFACE)(int);
    return ((PFN_GET_INTERFACE)d->pfnGetInterface)(itfNo);
}

BOOL EcDtDefaultSize(int lib, int dt, int* pcx, int* pcy)
{
    typedef INT (WINAPI *PFN_ON_NOTIFY_UNIT)(INT, DWORD, DWORD);
    void* p = EcDtInterface(lib, dt, EC_ITF_GET_NOTIFY_RECEIVER);
    if (p == NULL)
        return FALSE;
    int cx = 0, cy = 0;
    // NU_GET_CREATE_SIZE_IN_DESIGNER = 0：dwParam1/2 = &cx / &cy
    if (((PFN_ON_NOTIFY_UNIT)p)(0, (DWORD)(INT_PTR)&cx, (DWORD)(INT_PTR)&cy) == 0)
        return FALSE;
    if (cx <= 0 || cy <= 0)
        return FALSE;
    if (pcx) *pcx = cx;
    if (pcy) *pcy = cy;
    return TRUE;
}

void* EcCreateUnit(int lib, int dt, HWND hParent, UINT uID, int x, int y, int cx, int cy,
                   HWND hDesignWnd, BOOL bInDesignMode)
{
    return EcCreateUnitWithData(lib, dt, NULL, 0, hParent, uID, x, y, cx, cy,
                                hDesignWnd, bInDesignMode);
}

// 带"全部属性数据"建组件。
// ⚠️ ITF 1 的前两个参数就是 (pAllData, nAllDataSize)：**新建**传 NULL/0（krnln 的
//    sub_100311F0 对空数据直接算通过），**重建**时把 ITF 5 取出来的那份数据喂回来。
//    必须是**同一份字节**，不能自己拼格式（那是 sub_100310D0 的通用序列化）。
void* EcCreateUnitWithData(int lib, int dt, LPBYTE pData, int nData,
                           HWND hParent, UINT uID, int x, int y, int cx, int cy,
                           HWND hDesignWnd, BOOL bInDesignMode)
{
    typedef DWORD (WINAPI *PFN_CREATE_UNIT)(LPBYTE, INT, DWORD, HWND, UINT, HMENU,
                                            INT, INT, INT, INT, DWORD, DWORD, HWND, BOOL);
    PFN_CREATE_UNIT f = (PFN_CREATE_UNIT)EcDtInterface(lib, dt, EC_ITF_CREATE_UNIT);
    if (f == NULL)
        return NULL;
    // dwWinFormID/dwUnitID 是给库反调宿主报事件用的，先给 (lib+1, uID)。
    DWORD hu = (DWORD)f(pData, nData, 0, hParent, uID, NULL, x, y, cx, cy,
                        (DWORD)(lib + 1), (DWORD)uID, hDesignWnd, bInDesignMode);
    return (void*)hu;
}

// ================================================================== 组件的默认属性数据
// ★★ 用户 2026-10 的关键指正："设计器画的（组件）黑的不对，因为**有默认值是 1**，
//    而且**每个属性有返回的接口，拿过来要保存**，还有**一个放下的默认值**"。
//
// 我们原来一律用 `pAllData = NULL, nAllDataSize = 0` 建组件 —— 库对空数据是"算通过"
// （krnln sub_100311F0），但**不会替你把属性表填成默认值**：结果 `_EL_RgnButton` 的
// 15 个属性全是空/0（`ECLONE_DESIGN_DUMP` 实测：宽度 0 / 高度 0 / 可视 假 / 正常图片 空），
// 于是组件窗口画成一块纯黑；而原版那 8 个系统固定属性里 **标记=1**、可视=真。
//
// 正解（和原版 e.exe 一条路）：
//   ① 建一个"探针"组件（也用 NULL 数据建，建完立刻销毁）；
//   ② 对它的**每一个属性**走 ITF 6（PFN_GET_PROPERTY_DATA）把默认值取出来；
//   ③ 把取到的默认值装成一份"默认属性数据"字节缓存起来（按 (库,类型) 缓存，只做一次）；
//   ④ 以后新建同类组件就用这份数据（并覆盖"放下的默认尺寸"）。
//
// ⚠️ 这里**没有自己拼序列化格式**：默认值是用 ITF 6 一个一个读出来的，
//    再靠 ITF 4（NOTIFY_PROPERTY_CHANGED）写进探针组件，最后用 ITF 5（GET_ALL_PROPERTY_DATA）
//    让**库自己**序列化成那份字节 —— 格式由库决定，我们只搬运。

// 一个属性项（名字 + 值），从 EcPropAt/EcGetUnitProperty 拿
struct EcPropSnap { CString name; CString value; };

// 把某个组件对象的全部属性值快照下来
static void EcSnapProps(int lib, int dt, void* hunit, std::vector<EcPropSnap>& out)
{
    out.clear();
    const EcDataType* d = EcDtAt(lib, dt);
    if (d == NULL || hunit == NULL)
        return;
    for (int i = 0; i < d->propCount; i++)
    {
        const EcProp* p = EcPropAt(lib, dt, i);
        if (p == NULL)
            continue;
        CString v;
        if (!EcGetUnitProperty(lib, dt, hunit, i, &v))
            continue;
        EcPropSnap s;
        s.name = p->name;
        s.value = v;
        out.push_back(s);
    }
}

// 把快照写回另一个同类组件对象（按**属性名**匹配，不用下标 —— 两个对象的属性表顺序一致，
// 但用名字更稳）
static void EcApplyProps(int lib, int dt, void* hunit, const std::vector<EcPropSnap>& snap)
{
    const EcDataType* d = EcDtAt(lib, dt);
    if (d == NULL || hunit == NULL)
        return;
    for (size_t k = 0; k < snap.size(); k++)
    {
        for (int i = 0; i < d->propCount; i++)
        {
            const EcProp* p = EcPropAt(lib, dt, i);
            if (p == NULL || p->name != snap[k].name)
                continue;
            BOOL bRecreate = FALSE;
            EcSetUnitProperty(lib, dt, hunit, i, snap[k].value, &bRecreate);
            break;
        }
    }
}

// 默认属性数据缓存：按 (库,类型) 存一份字节
struct EcDefPropCache { int lib; int dt; BYTE* pData; int nData; };
static std::vector<EcDefPropCache> g_defProps;

// 取（并缓存）某组件类型的"默认属性数据"。
// ⚠️⚠️ 顺序很关键（本轮返工一次）：
//    ① **先**按 `cx,cy` 建探针 + 把固定属性（左边/顶边/宽度/高度/标记/可视/禁止）写进去；
//    ② **再** ITF 5 取字节 —— 早先反过来（先取字节、后写尺寸），那份数据里"宽度=0/高度=0"，
//       于是所有用默认数据建的组件都变成 0x0 的黑方块（属性表回读也是 宽度 0/可视 假）。
BOOL EcGetDefaultPropertyData(int lib, int dt, int cx, int cy, BYTE** ppData, int* pnData)
{
    if (ppData) *ppData = NULL;
    if (pnData) *pnData = 0;
    size_t k;
    for (k = 0; k < g_defProps.size(); k++)
        if (g_defProps[k].lib == lib && g_defProps[k].dt == dt)
        {
            if (ppData) *ppData = g_defProps[k].pData;
            if (pnData) *pnData = g_defProps[k].nData;
            return g_defProps[k].pData != NULL;
        }
    if (cx <= 0 || cy <= 0)
    {
        cx = 100; cy = 100;
        EcDtDefaultSize(lib, dt, &cx, &cy);      // ITF 12：库给的设计期默认尺寸
    }

    // ① 建探针（用桌面当父窗口：它只用来取属性，不进窗体）
    HWND hProbe = ::GetDesktopWindow();
    void* u = EcCreateUnit(lib, dt, hProbe, 1, 0, 0, cx, cy, hProbe, TRUE);
    if (u == NULL)
    {
        EcDefPropCache e;
        e.lib = lib; e.dt = dt; e.pData = NULL; e.nData = 0;
        g_defProps.push_back(e);
        return FALSE;
    }
    // 固定属性：尺寸用调用方要的，标记=1（用户点名的"默认值是 1"）、可视=真、禁止=假
    EcInitUnitGeometry(lib, dt, u, 0, 0, cx, cy);
    // 标记 = 1：krnln 的窗口组件"标记"默认就是 1，NULL 数据建出来的对象里是空
    {
        const EcDataType* d = EcDtAt(lib, dt);
        int i;
        for (i = 0; d != NULL && i < d->propCount; i++)
        {
            const EcProp* P = EcPropAt(lib, dt, i);
            if (P != NULL && P->name == _T("标记"))
            {
                EcSetUnitProperty(lib, dt, u, i, _T("1"), NULL);
                break;
            }
        }
    }

    // ② ITF 5 取字节（此刻属性已经是我们想要的样子）
    BYTE* data = NULL;
    int n = 0;
    BOOL bOk = EcGetAllPropertyData(lib, dt, u, &data, &n);
    {
        extern void EcTrace(LPCTSTR pszFmt, ...);
        EcTrace(_T("defprop probe lib=%d dt=%d size=(%d,%d) itf5ok=%d bytes=%d"),
                lib, dt, cx, cy, (int)bOk, n);
        // 回读一次宽度，确认"写进去"了（写不进去就是 ITF 4 的用法还没对上）
        int i;
        const EcDataType* d2 = EcDtAt(lib, dt);
        for (i = 0; d2 != NULL && i < d2->propCount; i++)
        {
            const EcProp* P2 = EcPropAt(lib, dt, i);
            if (P2 == NULL || P2->name != _T("宽度"))
                continue;
            CString v;
            if (EcGetUnitProperty(lib, dt, u, i, &v))
                EcTrace(_T("defprop readback 宽度=[%s]"), (LPCTSTR)v);
            break;
        }
    }
    EcDestroyUnit(u);

    EcDefPropCache e;
    e.lib = lib; e.dt = dt; e.pData = data; e.nData = n;
    g_defProps.push_back(e);
    if (ppData) *ppData = data;
    if (pnData) *pnData = n;
    return bOk;
}

void EcForgetDefaultPropertyData()
{
    for (size_t k = 0; k < g_defProps.size(); k++)
        if (g_defProps[k].pData != NULL)
            delete[] g_defProps[k].pData;
    g_defProps.clear();
}

// 用"默认属性数据"建组件（新建组件的正路）。没有默认数据时退回 NULL/0（老行为）。
void* EcCreateUnitDefault(int lib, int dt, HWND hParent, UINT uID, int x, int y, int cx, int cy,
                          HWND hDesignWnd, BOOL bInDesignMode)
{
    BYTE* data = NULL;
    int n = 0;
    EcGetDefaultPropertyData(lib, dt, cx, cy, &data, &n);
    void* u = EcCreateUnitWithData(lib, dt, data, n, hParent, uID, x, y, cx, cy,
                                    hDesignWnd, bInDesignMode);
    // 位置仍按调用方要的（默认数据里的左边/顶边是探针的 0,0）
    if (u != NULL)
    {
        EcInitUnitGeometry(lib, dt, u, x, y, cx, cy);
        if (::GetEnvironmentVariableA("ECLONE_TRACE_DES", NULL, 0) > 0)
        {
            extern void EcTrace(LPCTSTR pszFmt, ...);
            HWND hw = EcUnitHwnd(u);
            RECT wr, cr;
            ::GetWindowRect(hw, &wr);
            ::GetClientRect(hw, &cr);
            POINT p;
            p.x = wr.left;
            p.y = wr.top;
            ::ScreenToClient(hParent, &p);
            EcTrace(_T("createDefault lib=%d dt=%d want=(%d,%d %dx%d) data=%d bytes ")
                    _T("winrect=(%d,%d %dx%d) in-parent=(%d,%d) client=%dx%d"),
                    lib, dt, x, y, cx, cy, n,
                    wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top,
                    p.x, p.y, cr.right - cr.left, cr.bottom - cr.top);
        }
    }
    return u;
}

// ITF 5 = PFN_GET_ALL_PROPERTY_DATA：把组件**当前全部属性**序列化成一块内存（返回 HGLOBAL）。
// ★ 用户 2026-10 明确要求："每个属性有返回的接口，拿过来要保存"。
// 返回的 HGLOBAL 由库分配，我们拷一份自己的字节（*ppData 用 new[]，调用方 delete[]）。
BOOL EcGetAllPropertyData(int lib, int dt, void* hunit, BYTE** ppData, int* pnData)
{
    if (ppData) *ppData = NULL;
    if (pnData) *pnData = 0;
    if (hunit == NULL)
        return FALSE;
    typedef HGLOBAL (WINAPI *PFN_GET_ALL_PROPERTY_DATA)(void*);
    PFN_GET_ALL_PROPERTY_DATA f =
        (PFN_GET_ALL_PROPERTY_DATA)EcDtInterface(lib, dt, EC_ITF_GET_ALL_PROPERTY_DATA);
    if (f == NULL)
        return FALSE;
    HGLOBAL h = f(hunit);
    if (h == NULL)
        return FALSE;
    SIZE_T n = ::GlobalSize(h);
    if (n == 0 || n > (SIZE_T)(4 * 1024 * 1024))
    {
        ::GlobalFree(h);
        return FALSE;
    }
    LPVOID p = ::GlobalLock(h);
    if (p == NULL)
    {
        ::GlobalFree(h);
        return FALSE;
    }
    BYTE* buf = new BYTE[(int)n];
    if (buf == NULL)
    {
        ::GlobalUnlock(h);
        ::GlobalFree(h);
        return FALSE;
    }
    memcpy(buf, p, (size_t)n);
    ::GlobalUnlock(h);
    ::GlobalFree(h);
    if (ppData) *ppData = buf;
    if (pnData) *pnData = (int)n;
    return TRUE;
}

// "属性改了要重建窗口"：ITF 5 取旧数据 -> 销毁旧对象 -> ITF 1 用同一份数据重建。
// e.exe 侧对应 sub_4AE230（0x4ae230）：取数据 -> ITF 1 -> 换窗口句柄。
void* EcRebuildUnitWithProperties(int lib, int dt, void* hunit, HWND hParent, UINT uID,
                                  HWND hDesignWnd, BOOL bInDesignMode)
{
    if (hunit == NULL)
        return NULL;
    RECT wr;
    if (!::GetWindowRect(EcUnitHwnd(hunit), &wr))
        return hunit;
    POINT p;
    p.x = wr.left;
    p.y = wr.top;
    ::ScreenToClient(hParent, &p);
    int cx = wr.right - wr.left, cy = wr.bottom - wr.top;

    BYTE* pData = NULL;
    int nData = 0;
    EcGetAllPropertyData(lib, dt, hunit, &pData, &nData);
    EcDestroyUnit(hunit);
    void* nu = EcCreateUnitWithData(lib, dt, pData, nData, hParent, uID, p.x, p.y, cx, cy,
                                    hDesignWnd, bInDesignMode);
    if (pData != NULL)
        delete[] pData;
    return nu;
}

HWND EcUnitHwnd(void* hunit)
{
    if (hunit == NULL)
        return NULL;
    return *(HWND*)((char*)hunit + 28);      // krnln 的对象是 CWnd 派生类，m_hWnd 在 +28
}

void EcDestroyUnit(void* hunit)
{
    if (hunit == NULL)
        return;
    // krnln 自己的回滚路径就是 (*(vtbl+4))(obj, 1)（见 sub_100359A0 的失败分支）：
    // thiscall(this=ECX, arg=栈) + ret 4 —— 用 __fastcall(ECX, EDX, 栈) 精确对应。
    typedef void (__fastcall *PFN_UNIT_DESTROY)(void*, int, int);
    void** vtbl = *(void***)hunit;
    if (vtbl != NULL && vtbl[1] != NULL)
        ((PFN_UNIT_DESTROY)vtbl[1])(hunit, 0, 1);
}

BOOL EcSetUnitTitle(int lib, int dt, void* hunit, LPCTSTR pszText)
{
    if (hunit == NULL)
        return FALSE;
    const EcDataType* d = EcDtAt(lib, dt);
    if (d == NULL)
        return FALSE;
    int idx = -1;
    for (int i = 0; i < d->propCount; i++)
    {
        const EcProp* P = EcPropAt(lib, dt, i);
        if (P != NULL && (P->name == _T("标题") || P->name == _T("文本")))
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return FALSE;
    return EcSetUnitProperty(lib, dt, hunit, idx, pszText, NULL);
}

const EcProp* EcPropAt(int lib, int dt, int idx)
{
    const EcDataType* d = EcDtAt(lib, dt);
    if (d == NULL || idx < 0 || idx >= d->propCount)
        return NULL;
    int k = d->propStart + idx;
    if (k < 0 || k >= (int)g_props.size())
        return NULL;
    return &g_props[k];
}

const EcEvent* EcEventAt(int lib, int dt, int idx)
{
    const EcDataType* d = EcDtAt(lib, dt);
    if (d == NULL || idx < 0 || idx >= d->eventCount)
        return NULL;
    int k = d->eventStart + idx;
    if (k < 0 || k >= (int)g_events.size())
        return NULL;
    return &g_events[k];
}

// 诊断：把所有支持库数据类型的**事件名表**倒成文本（`ECLONE_DUMP_EVENTS=<路径>`）。
// 为什么要它：打开真 .e 时，文件里的子程序名是**被英化插件改过的英文事件名**
// （`_cmdSort_Click`），而原版左树显示中文名（`_cmdSort_被单击`）。
// 中文事件名的权威来源就是支持库元数据（`被单击` 在 krnln.fne 里、在 e.exe 里搜不到），
// 所以把这张表整体导出，才能定死"英文名 -> 中文名"的对应，而不是靠硬编码几条。
void EcDumpEventsIfAsked()
{
    TCHAR szPath[MAX_PATH * 2];
    if (::GetEnvironmentVariable(_T("ECLONE_DUMP_EVENTS"), szPath, MAX_PATH * 2) <= 0)
        return;
    FILE* f = _tfopen(szPath, _T("wb"));
    if (!f) return;
    int li, dti;
    for (li = 0; li < (int)g_libs.size(); li++)
    {
        fprintf(f, "LIB %d %s\r\n", li, (LPCTSTR)g_libs[li].name);
        for (dti = 0; dti < g_libs[li].nDataType; dti++)
        {
            const EcDataType* d = EcDtAt(li, dti);
            if (d == NULL) continue;
            fprintf(f, "  DT %d state=0x%08X events=%d %s\r\n",
                    dti, d->state, d->eventCount, (LPCTSTR)d->name);
            for (int k = 0; k < d->eventCount; k++)
            {
                const EcEvent* e = EcEventAt(li, dti, k);
                if (e == NULL) continue;
                // eg 是英文名（如果库自己带的话），name 是显示名
                fprintf(f, "    E %d %s\r\n", k, (LPCTSTR)e->name);
            }
        }
    }
    fclose(f);
}

// UNIT_PROPERTY_VALUE：8 字节的联合（lib2.h:583），这里按 UD_* 类型解释
union EC_PROP_VALUE
{
    int         m_int;
    double      m_double;
    BOOL        m_bool;
    double      m_date;
    COLORREF    m_clr;
    LPCTSTR     m_szText;
    struct { LPBYTE p; int n; } m_data;
};

BOOL EcPropertyEditable(int lib, int dt, void* hunit, int idx)
{
    typedef BOOL (WINAPI *PFN_PROPERTY_UPDATE_UI)(DWORD, INT);
    PFN_PROPERTY_UPDATE_UI f =
        (PFN_PROPERTY_UPDATE_UI)EcDtInterface(lib, dt, EC_ITF_PROPERTY_UPDATE_UI);
    if (f == NULL)
        return TRUE;                       // 库没提供这个接口：不拦
    return f((DWORD)(INT_PTR)hunit, idx) ? TRUE : FALSE;
}

BOOL EcGetUnitProperty(int lib, int dt, void* hunit, int idx, CString* pOut)
{
    if (pOut != NULL)
        pOut->Empty();
    if (hunit == NULL)
        return FALSE;
    const EcProp* P = EcPropAt(lib, dt, idx);
    if (P == NULL)
        return FALSE;
    typedef BOOL (WINAPI *PFN_GET_PROPERTY_DATA)(DWORD, INT, void*);
    PFN_GET_PROPERTY_DATA f =
        (PFN_GET_PROPERTY_DATA)EcDtInterface(lib, dt, EC_ITF_GET_PROPERTY_DATA);
    if (f == NULL)
        return FALSE;

    EC_PROP_VALUE v;
    memset(&v, 0, sizeof(v));
    if (!f((DWORD)(INT_PTR)hunit, idx, &v))
        return FALSE;
    if (pOut == NULL)
        return TRUE;

    switch (P->type)
    {
    case EC_UD_INT:
    case EC_UD_PICK_INT:
    case EC_UD_PICK_SPEC_INT:
        pOut->Format(_T("%d"), v.m_int);
        break;
    case EC_UD_DOUBLE:
        pOut->Format(_T("%g"), v.m_double);
        break;
    case EC_UD_BOOL:
        *pOut = v.m_bool ? _T("真") : _T("假");
        break;
    case EC_UD_DATE_TIME:
        pOut->Format(_T("%g"), v.m_date);
        break;
    case EC_UD_COLOR:
    case EC_UD_COLOR_TRANS:
    case EC_UD_COLOR_BACK:
        pOut->Format(_T("#%02X%02X%02X"), GetRValue(v.m_clr), GetGValue(v.m_clr), GetBValue(v.m_clr));
        break;
    case EC_UD_TEXT:
    case EC_UD_PICK_TEXT:
    case EC_UD_EDIT_PICK_TEXT:
    case EC_UD_FILE_NAME:
        // 只抄一份，不释放：库可能返回自己的内部存储（lib2.h:681 说"必须自行释放"，
        // 但那要求宿主实现了 NRS_MALLOC；本工程没注册宿主通知函数，乱释放会崩）。
        // ⚠️ 用 NULL 数据建出来的对象，这些字段可能是**未初始化的垃圾指针**（实测标记=0x00808080），
        // 所以必须先 IsBadStringPtrA 探一下再赋值，否则直接崩。
        if (v.m_szText != NULL && !IsBadStringPtrA(v.m_szText, 2048))
            *pOut = v.m_szText;
        else
            *pOut = _T("");
        break;
    default:
        pOut->Format(_T("<%d 字节>"), v.m_data.n);
        break;
    }
    return TRUE;
}

BOOL EcSetUnitProperty(int lib, int dt, void* hunit, int idx, LPCTSTR pszValue,
                       BOOL* pbNeedRecreate)
{
    if (pbNeedRecreate != NULL)
        *pbNeedRecreate = FALSE;
    if (hunit == NULL || pszValue == NULL)
        return FALSE;
    const EcProp* P = EcPropAt(lib, dt, idx);
    if (P == NULL)
        return FALSE;
    typedef BOOL (WINAPI *PFN_NOTIFY_PROPERTY_CHANGED)(DWORD, INT, void*, void*);
    PFN_NOTIFY_PROPERTY_CHANGED f =
        (PFN_NOTIFY_PROPERTY_CHANGED)EcDtInterface(lib, dt, EC_ITF_NOTIFY_PROPERTY_CHANGED);
    if (f == NULL)
        return FALSE;

    EC_PROP_VALUE v;
    memset(&v, 0, sizeof(v));
    CString s(pszValue);
    switch (P->type)
    {
    case EC_UD_INT:
    case EC_UD_PICK_INT:
    case EC_UD_PICK_SPEC_INT:
        v.m_int = atoi(s);
        break;
    case EC_UD_DOUBLE:
        v.m_double = atof(s);
        break;
    case EC_UD_BOOL:
        v.m_bool = (s == _T("真") || s == _T("1") || s == _T("-1")) ? TRUE : FALSE;
        break;
    case EC_UD_DATE_TIME:
        v.m_date = atof(s);
        break;
    case EC_UD_COLOR:
    case EC_UD_COLOR_TRANS:
    case EC_UD_COLOR_BACK:
        if (s.GetLength() >= 7 && s[0] == _T('#'))
        {
            int r = 0, g = 0, b = 0;
            sscanf((LPCTSTR)s.Mid(1, 2), "%x", &r);
            sscanf((LPCTSTR)s.Mid(3, 2), "%x", &g);
            sscanf((LPCTSTR)s.Mid(5, 2), "%x", &b);
            v.m_clr = RGB(r, g, b);
        }
        else
            v.m_clr = (COLORREF)atoi(s);
        break;
    case EC_UD_TEXT:
    case EC_UD_PICK_TEXT:
    case EC_UD_EDIT_PICK_TEXT:
    case EC_UD_FILE_NAME:
        v.m_szText = pszValue;
        break;
    default:
        return FALSE;                       // 图片/字体/自定义等：本轮不支持
    }
    BOOL bNeedRecreate = f((DWORD)(INT_PTR)hunit, idx, &v, NULL) ? TRUE : FALSE;
    // ⚠️ 这个返回值是"需不需要重建窗口"，**不是**成功/失败：实测 krnln 的标签类每个属性分支都 return 0。
    // 库没有失败信号，成功与否只能靠调用方回读（属性表就是这么做的）。
    if (::GetEnvironmentVariableA("ECLONE_TRACE_DES", NULL, 0) > 0)
    {
        extern void EcTrace(LPCTSTR pszFmt, ...);
        EcTrace(_T("setprop idx=%d name=%s want=[%s] recreate=%d"),
                idx, (LPCTSTR)P->name, pszValue, (int)bNeedRecreate);
    }
    if (pbNeedRecreate != NULL)
        *pbNeedRecreate = bNeedRecreate;
    return TRUE;
}

// 建完组件后把固定属性初始化成"我们给它摆的位置"。
// ⚠️ 不初始化的话，用 NULL 数据建出来的对象里这些字段是**未初始化的垃圾**（实测左边=6249016、
// 禁止=真），属性表会显示一堆乱七八糟的值。按属性名匹配，不写死下标。
BOOL EcInitUnitGeometry(int lib, int dt, void* hunit, int x, int y, int cx, int cy)
{
    const EcDataType* d = EcDtAt(lib, dt);
    if (d == NULL || hunit == NULL)
        return FALSE;
    static const TCHAR* kName[6] = { _T("左边"), _T("顶边"), _T("宽度"), _T("高度"),
                                     _T("可视"), _T("禁止") };
    int kVal[6];
    CString kText[6];
    int i, n = 0;
    kVal[0] = x; kVal[1] = y; kVal[2] = cx; kVal[3] = cy; kVal[4] = 1; kVal[5] = 0;
    kText[4] = _T("真"); kText[5] = _T("假");
    for (i = 0; i < d->propCount; i++)
    {
        const EcProp* P = EcPropAt(lib, dt, i);
        if (P == NULL)
            continue;
        for (int k = 0; k < 6; k++)
        {
            if (P->name != kName[k])
                continue;
            if (k >= 4)
            {
                EcSetUnitProperty(lib, dt, hunit, i, kText[k], NULL);
            }
            else
            {
                CString v;
                v.Format(_T("%d"), kVal[k]);
                EcSetUnitProperty(lib, dt, hunit, i, v, NULL);
            }
            n++;
            break;
        }
    }
    return (n > 0);
}

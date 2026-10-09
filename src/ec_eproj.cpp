// ec_eproj.cpp -- 读真 .e 二进制工程，生成 eclone 的文本源码形式。
//
// ===========================================================================
// 判据：一切以 e.exe 为准。
//   .e 读取入口 = e.exe sub_444C40 @ 0x444C40（唯一调用者是 sub_444B60 @ 0x444B60，
//                后者是 CFile/CMirrorFile 打开器；写盘是 sub_4431A0 @ 0x4431A0）。
//   段头 100 字节 = 4B 段magic(0x15117319) + 4B 单字节XOR校验 + 92B 其余；
//                 eplc2 的 SegmentHeader 布局与实测一致（详见 D:\EPL\HANDOFF.md §7）。
//   段名 XOR：Name[i] ^= Key[(i+1)%4]
//   payload 字符串编码由 SystemInfo 段 payload+8 的语言码决定
//                 （1=GBK 2=ASCII 3=Big5 4=ShiftJIS 5=UTF8）
// 程序段字段顺序：eplc2 src/parser.cpp，已在 6 个真 .e 上验证（中文名可读 = 顺序对）。
// 文本形式的写法：以 eclone 自己已有的解析器为准（ec_editor.cpp 的
//   `.程序集/.子程序/.参数/.局部变量/.全局变量/.数据类型/.成员/.常量/.DLL命令`
//   与 EClone.cpp EcBuildProgTree 的 `.文件夹/.程序集/.类/.子程序/.窗口`）。
// ===========================================================================
#include "ec_eproj.h"
#include "ec_lib.h"
#include "ec_eexpr.h"
#include <vector>
#include <string>

// ------------------------------------------------------------------ 段 key
#define ESEG_USERINFO     0x01007319u
#define ESEG_SYSINFO      0x02007319u
#define ESEG_PROGRAM      0x03007319u
#define ESEG_RESOURCE     0x04007319u
#define ESEG_LOSSY        0x05007319u
#define ESEG_END          0x07007319u
#define ESEG_AUXINFO2     0x0B007319u

namespace {

// ==================================================================== 字节游标
struct EcBuf
{
    const unsigned char* p;
    size_t               n;
    size_t               i;
    bool                 bad;

    EcBuf(const unsigned char* d, size_t len) : p(d), n(len), i(0), bad(false) {}
    bool Ok(size_t k) { if (bad || i + k > n) { bad = true; return false; } return true; }

    unsigned char  u8 () { return Ok(1) ? p[i++] : 0; }
    unsigned short u16() { if (!Ok(2)) return 0; unsigned short v = (unsigned short)(p[i] | (p[i+1] << 8)); i += 2; return v; }
    unsigned int   u32() { if (!Ok(4)) return 0;
                           unsigned int v = (unsigned int)p[i] | ((unsigned int)p[i+1] << 8)
                                          | ((unsigned int)p[i+2] << 16) | ((unsigned int)p[i+3] << 24);
                           i += 4; return v; }
    void Skip(size_t k) { if (Ok(k)) i += k; }
    size_t Left() const { return (i <= n) ? (n - i) : 0; }
    std::string Bytes(size_t k)
    {
        if (!Ok(k)) return std::string();
        std::string s((const char*)p + i, k);
        i += k;
        return s;
    }

    // [u32 len][len 字节]
    std::string MfcString()
    {
        unsigned int len = u32();
        if (bad) return std::string();
        if (len == 0) return std::string();
        if (len > Left()) { bad = true; return std::string(); }
        return Bytes(len);
    }
    // [u16 count | 0xFFFF + u32][count × mfc_string]
    std::vector<std::string> MfcStringArray()
    {
        std::vector<std::string> out;
        unsigned int c = u16();
        if (c == 0xFFFF) c = u32();
        if (bad || c > 100000) { bad = true; return out; }
        for (unsigned int k = 0; k < c && !bad; k++) out.push_back(MfcString());
        return out;
    }
    // 以 \0 结尾
    std::string CStr()
    {
        std::string s;
        while (!bad && i < n)
        {
            unsigned char c = p[i++];
            if (c == 0) break;
            s += (char)c;
        }
        return s;
    }
};

// ================================================================== 编码转换
// .e 里的串按语言码解码后的字节，转成 eclone 源码需要的编码。
// 源码目录是 UTF-8，build.bat 用 tools\utf8togbk.ps1 转成 GBK 再交给 VC6，
// 所以运行时所有 TCHAR 字面量都是 GBK 字节 —— 这里统一输出 GBK。
//
// 做法：先把源字节按 srcCp 解成宽字符，再编成 GBK。
// ⚠️ 不用 CString 的 WideCharToMultiByte 包装，直接调 API，免得受 _UNICODE 影响。
CString EcToGbk(const std::string& raw, UINT srcCp)
{
    if (raw.empty()) return CString();
    int wn = ::MultiByteToWideChar(srcCp, 0, raw.data(), (int)raw.size(), NULL, 0);
    if (wn <= 0) return CString();
    std::vector<wchar_t> w(wn + 1, 0);
    ::MultiByteToWideChar(srcCp, 0, raw.data(), (int)raw.size(), &w[0], wn);
    int gn = ::WideCharToMultiByte(936, 0, &w[0], wn, NULL, 0, NULL, NULL);
    if (gn <= 0) return CString();
    std::vector<char> g(gn + 1, 0);
    ::WideCharToMultiByte(936, 0, &w[0], wn, &g[0], gn, NULL, NULL);
    g[gn] = 0;
    return CString(&g[0]);
}

UINT EcCpFromLang(unsigned int lang)
{
    // ⚠️ 实测：`Bubble Sort.e` 的 SystemInfo 语言码是 2（按枚举应是"西文/ASCII"），
    //    但同一批文件里的中文串按 GBK 解才是对的。所以**语言码不要用来推断编码**，
    //    一律按 GBK 解（GBK 是 ASCII 的超集，西文文件照样解对）。
    //    真正需要多编码支持时，改成"先试 GBK，出现替换字符再试别的"。
    //    另注：EcToGbk 现在按 GBK→GBK 走，等于只做一遍校验，不改变字节。
    (void)lang;
    return 936;
}

// ============================================================ 数据类型 → 中文名
// ⚠️ 不要自己写一套：`ec_lib.h` 已导出 `EcTypeName(EC_DATA_TYPE v, int lib, BOOL bShort)`，
//    它还懂**自定义数据类型**（自己写的会把 0x4101xxxx 显示成 "数据类型N"）。用它的短名。
CString EcEpTypeName(unsigned int dt, int lib)
{
    // lib 传 -1 让 ec_lib 回退到它内置的枚举表（实测够用）
    return EcTypeName((EC_DATA_TYPE)dt, -1, TRUE);
}

// ==================================================================== 变量
struct EcVar
{
    unsigned int id;
    unsigned int dataType;
    unsigned short flags;
    std::vector<unsigned int> bounds;
    std::string  name;
    std::string  comment;
};

std::vector<EcVar> EcParseVariables(EcBuf& b)
{
    std::vector<EcVar> out;
    unsigned int k;
    unsigned char t;
    unsigned int count = b.u32();
    unsigned int size  = b.u32();
    if (b.bad || count > 100000) { b.bad = true; return out; }
    size_t startOfIds = b.i;
    size_t end        = startOfIds + size;
    if (end > b.n) { b.bad = true; return out; }

    std::vector<unsigned int> ids(count), offs(count);
    for (k = 0; k < count; k++) ids[k]  = b.u32();
    for (k = 0; k < count; k++) offs[k] = b.u32();
    if (b.bad) return out;

    size_t base = b.i;
    for (k = 0; k < count; k++)
    {
        b.i = base + offs[k];
        if (b.i + 8 > end) continue;
        EcVar v;
        v.id = ids[k];
        b.Skip(4);                       // 每个变量体自带的 size
        v.dataType = b.u32();
        v.flags    = b.u16();
        unsigned char ub = b.u8();
        for (t = 0; t < ub; t++) v.bounds.push_back(b.u32());
        v.name    = b.CStr();
        v.comment = b.CStr();
        if (b.bad) break;
        out.push_back(v);
    }
    b.i   = end;
    b.bad = false;                        // 变量表读到头不算错
    return out;
}

// ==================================================================== 工程模型
struct EcMethod
{
    unsigned int id, classId, flags, retType;
    std::string  name, comment;
    std::vector<EcVar> locals, params;
    // 6 个 byte_vec 里只留这两个（正文解码要用）：
    //   lineOffset = 每个显示行行首在 expression 里的绝对偏移（u32 数组）
    //   expr       = 语句流字节码
    std::vector<unsigned char> lineOffset, expr;
};

struct EcAsm
{
    unsigned int id, formId, baseClassId;
    std::string  name, comment;
    std::vector<unsigned int> methodIds;
    std::vector<EcVar>        vars;
};

struct EcStruct
{
    unsigned int id;
    std::string  name, comment;
    std::vector<EcVar> members;
};

struct EcDll
{
    unsigned int id, flags, retType;
    std::string  name, comment, dll, entry;
    std::vector<EcVar> params;
};

struct EcEpConst
{
    unsigned int id;
    unsigned short flags;
    std::string  name, comment, strVal;
    int          kind;        // 0=数值 1=逻辑 2=日期 3=文本 4=字节集
    double       numVal;
    bool         boolVal;
    unsigned int binLen;
};

struct EcProgram
{
    std::vector<std::string> libs;    std::vector<EcAsm>       asms;
    std::vector<EcMethod>    methods;
    std::vector<EcVar>       globals;
    std::vector<EcStruct>    structs;
    std::vector<EcDll>       dlls;
    std::vector<EcEpConst>     consts;
};

// ------------------------------------------------------------- 块头 [u32][ids][addrs]
bool EcBlockHeader(EcBuf& b, std::vector<unsigned int>& ids, std::vector<unsigned int>& addrs)
{
    unsigned int header = b.u32();
    if (b.bad || (header % 8) != 0) return false;
    unsigned int n = header / 8;
    if (n > 100000) return false;
    ids.resize(n);
    addrs.resize(n);
    unsigned int k;
    for (k = 0; k < n; k++) ids[k]   = b.u32();
    for (k = 0; k < n; k++) addrs[k] = b.u32();
    return !b.bad;
}

// 自定义数据类型的"类型码" -> 程序集里声明的名字。
// 实测（调用API.e）：结构体块里存的就是**完整类型码**，与引用处同域：
//   struct id=0x4101002E name=DISPLAY_DEVICE  <- 参数 lpDisplayDevice 的类型码也是 0x4101002E
//   struct id=0x410100BB name=GUID            <- 参数 Guid 的类型码也是 0x410100BB
// 所以按**全值**比较；不要只取低 16 位（那只是这个全局类型域里的序号）。
// 名字取不到时返回空串（调用方再退回 EcTypeName 的通用名）。
CString EcCustomTypeName(const EcProgram& prog, unsigned int dt, UINT cp)
{
    if ((dt & 0xFFFF0000u) != 0x41010000u) return CString();
    for (size_t i = 0; i < prog.structs.size(); i++)
    {
        if (prog.structs[i].id == dt)
            return EcToGbk(prog.structs[i].name, cp);
    }
    return CString();
}

// 常量表：[u32 count][u32 datalenth][ids count×u32][count×u32 未知][count × 常量体]
// 常量体 = [u32 len][u16 flags][name \0][comment \0] + 值
//   len 的高 4 位（id>>28）：1=普通 2=图片 3=声音
//   值：0x17 double / 0x18 u32 逻辑 / 0x19 double 日期 / 0x1A mfc 文本
// 依据：eplc2 src/parser.cpp 行 141-201。
bool EcParseConsts(EcBuf& b, std::vector<EcEpConst>& out)
{
    unsigned int k;
    unsigned int count = b.u32();
    unsigned int datalenth = b.u32();
    if (b.bad || count > 100000) return false;
    if (datalenth > b.Left()) return false;

    std::vector<unsigned int> ids(count), unknown(count);
    for (k = 0; k < count; k++) ids[k]     = b.u32();
    for (k = 0; k < count; k++) unknown[k] = b.u32();
    if (b.bad) return false;

    for (k = 0; k < count; k++)
    {
        unsigned int len = b.u32();
        if (b.bad || len > b.Left()) return false;
        std::string body = b.Bytes(len);
        if (b.bad) return false;

        EcBuf s((const unsigned char*)body.data(), body.size());
        EcEpConst c;
        c.id      = ids[k];
        c.kind    = 0;
        c.numVal  = 0.0;
        c.boolVal = false;
        c.binLen  = 0;
        c.flags   = s.u16();
        c.name    = s.CStr();
        c.comment = s.CStr();

        unsigned int kind4 = ids[k] >> 28;
        if (kind4 == 2 || kind4 == 3)
        {
            c.kind   = 4;
            c.binLen = s.u32();
            s.Skip(c.binLen);
        }
        else if (s.Left() >= 1)
        {
            unsigned char vt = s.u8();
            if (vt == 0x17)
            {
                c.kind = 0;
                if (s.Left() >= 8)
                {
                    unsigned char d[8];
                    for (int q = 0; q < 8; q++) d[q] = s.u8();
                    ::memcpy(&c.numVal, d, 8);
                }
            }
            else if (vt == 0x18) { c.kind = 1; c.boolVal = (s.u32() != 0); }
            else if (vt == 0x19)
            {
                c.kind = 2;
                if (s.Left() >= 8)
                {
                    unsigned char d[8];
                    for (int q = 0; q < 8; q++) d[q] = s.u8();
                    ::memcpy(&c.numVal, d, 8);
                }
            }
            else if (vt == 0x1A) { c.kind = 3; c.strVal = s.MfcString(); }
        }
        out.push_back(c);
    }
    return true;
}

// 解析程序段 payload
bool EcParseProgram(const std::string& payload, UINT cp, EcProgram& prog)
{
    EcBuf b((const unsigned char*)payload.data(), payload.size());
    unsigned int t;

    b.u32();                 // allocatedIdNum
    b.u32();                 // skip
    b.MfcString();           // minRequiredCmds  ([u32 size][data])
    b.MfcString();           // minRequiredDataTypes
    b.MfcString();           // minRequiredConstants
    if (b.bad) return false;

    prog.libs = b.MfcStringArray();
    unsigned int flag = b.u32();
    b.u32();                 // MainMethod
    if (flag & 1) b.Skip(16);
    b.MfcString();           // icon
    b.MfcString();           // debug_command
    if (b.bad) return false;

    // 1. 程序集
    size_t k;
    std::vector<unsigned int> ids, addrs;
    if (!EcBlockHeader(b, ids, addrs)) return false;
    for (k = 0; k < ids.size(); k++)
    {
        EcAsm a;
        a.id = ids[k];
        a.formId       = b.u32();
        a.baseClassId  = b.u32();
        a.name         = b.MfcString();
        a.comment      = b.MfcString();
        unsigned int msize = b.u32();
        for (t = 0; t + 4 <= (int)msize; t += 4) a.methodIds.push_back(b.u32());
        a.vars = EcParseVariables(b);
        if (b.bad) return false;
        prog.asms.push_back(a);
    }

    // 2. 子程序
    if (!EcBlockHeader(b, ids, addrs)) return false;
    for (k = 0; k < ids.size(); k++)
    {
        EcMethod m;
        m.id       = ids[k];
        m.classId  = b.u32();
        m.flags    = b.u32();
        m.retType  = b.u32();
        m.name     = b.MfcString();
        m.comment  = b.MfcString();
        m.locals   = EcParseVariables(b);
        m.params   = EcParseVariables(b);
        for (int q = 0; q < 6; q++)
        {
            unsigned int n = b.u32();
            // 0=lineOffset 5=expression；这两个正文解码要用，留下
            if (q == 0 || q == 5)
            {
                std::string v = b.Bytes(n);
                std::vector<unsigned char>& dst = (q == 0) ? m.lineOffset : m.expr;
                dst.assign((const unsigned char*)v.data(), (const unsigned char*)v.data() + v.size());
            }
            else
            {
                b.Skip(n);
            }
        }
        if (b.bad) return false;
        prog.methods.push_back(m);
    }

    // 3. 全局变量
    prog.globals = EcParseVariables(b);
    if (b.bad) return false;

    // 4. 自定义数据类型（结构体）
    if (!EcBlockHeader(b, ids, addrs)) return false;
    for (k = 0; k < ids.size(); k++)
    {
        EcStruct st;
        st.id = ids[k];
        b.Skip(4);                    // flags
        st.name    = b.MfcString();
        st.comment = b.MfcString();
        st.members = EcParseVariables(b);
        if (b.bad) return false;
        prog.structs.push_back(st);
    }

    // 5. DLL 命令
    if (!EcBlockHeader(b, ids, addrs)) return false;
    for (k = 0; k < ids.size(); k++)
    {
        EcDll d;
        d.id     = ids[k];
        d.flags  = b.u32();
        d.retType= b.u32();
        d.name   = b.MfcString();
        d.comment= b.MfcString();
        d.dll    = b.MfcString();
        d.entry  = b.MfcString();
        d.params = EcParseVariables(b);
        if (b.bad) return false;
        prog.dlls.push_back(d);
    }

    // 6. 常量表（读失败不致命：尾部那段本来就可能是零填充）
    EcParseConsts(b, prog.consts);
    return true;
}

// ---------------------------------------------------------------- 文本生成小工具
// ⚠️ 性能：这里必须用 std::string，**不能用 CString**。
//   MFC 的 CString 在引用计数 >1 时每次 += 都整体复制缓冲区 -> O(n^2)。
//   实测：精易模块 32172 行正文，用 CString 累加要 **22202 ms**，
//   而解码本身只要 126 ms。换成 std::string 后拼接降到毫秒级。
void EcLine(std::string& out, const CString& s)
{
    out += (LPCSTR)s;
    out += "\r\n";
}

// 带前缀的一行。**直接追加，不造 CString 临时量**。
// 旧写法 `EcLine(out, CString(_T("    ")) + body[bi])` 每行会分配 2 个 CString。
void EcLinePref(std::string& out, const char* pszPrefix, const CString& s)
{
    out += pszPrefix;
    out += (LPCSTR)s;
    out += "\r\n";
}

// 参数/变量写一行。bIsParam 决定用 .参数 还是 .局部变量（或 .程序集变量）。
// 写法判据：ec_editor.cpp 的解析位置语义
//   参数   ：名, 类型, 参考|空, 可空|空, 数组|空, 备注
//   变量   ：名, 类型, 静态|空, 数组|空, 备注
CString EcVarLine(LPCTSTR pszHead, const EcVar& v, UINT cp, BOOL bIsParam, int lib, const EcProgram& prog)
{
    CString name = EcToGbk(v.name, cp);
    CString type = EcCustomTypeName(prog, v.dataType, cp);
    if (type.IsEmpty()) type = EcEpTypeName(v.dataType, lib);
    CString note = EcToGbk(v.comment, cp);

    CString line;
    if (bIsParam)
    {
        // 参考 = flags bit0（传址）；可空暂不置
        CString sRef  = (v.flags & 0x1) ? _T("参考") : _T("");
        CString sArr  = v.bounds.empty() ? _T("") : _T("数组");
        line.Format(_T(".参数 %s, %s, %s, %s, %s, %s"),
                    (LPCTSTR)name, (LPCTSTR)type, (LPCTSTR)sRef, _T(""),
                    (LPCTSTR)sArr, (LPCTSTR)note);
    }
    else
    {
        CString sStatic = (v.flags & 0x1) ? _T("静态") : _T("");
        CString sArr    = v.bounds.empty() ? _T("") : _T("数组");
        line.Format(_T("%s %s, %s, %s, %s, %s"), pszHead,
                    (LPCTSTR)name, (LPCTSTR)type, (LPCTSTR)sStatic,
                    (LPCTSTR)sArr, (LPCTSTR)note);
    }
    return line;
}

// 数值/日期 → 文本。整数就按整数写，避免 "2.000000"
// ⚠️ VC6 没有 long long，用 __int64。
CString EcNumberText(double d)
{
    CString s;
    if (d == (double)(__int64)d && d > -1e15 && d < 1e15)
        s.Format(_T("%I64d"), (__int64)d);
    else
        s.Format(_T("%.15g"), d);
    return s;
}

} // namespace

// ===========================================================================
// 子程序名本地化
// ---------------------------------------------------------------------------
// 判据（实测对拍 `D:\EPL\_ida\orig_tree_bubble\trees.u8.txt`）：
//   同一个 .e，原版左树显示 `_cmdSort_被单击` / `__启动窗口_创建完毕`，
//   而文件里存的是 `_cmdSort_Click` / `__MainForm_AfterCreate`。
// 根因：用户机上的英化插件 C:\Program Files (x86)\e\lib\iDraw\eTools\Plugin\Anglicized.dll
//   把**中文事件名**改成了英文再存盘，所以英文名在文件里、中文名要现取。
// 数据源：支持库事件元数据（ec_lib.cpp 的 g_events，经 EcEventAt 取，name 是中文）。
//   `被单击` 在 krnln.fne 里有 GBK 字面量，在 e.exe 里搜不到 —— 证明只能从支持库取。
//
// ⚠️ 现在只做**已验证**的两条：
//      `_<名>_Click`                -> `_<名>_被单击`
//      `__MainForm_AfterCreate`     -> `__启动窗口_创建完毕`
//   其余英文事件名的对应关系**未验证**（见 HANDOFF §6），所以一律返回原样，
//   宁可显示英文名也不要编一个错的中文名。
// ⚠️ 别去 e.exe 里搜 `被单击`（0 命中），也别硬编码一张表就算完 —— 用户要求行为一致，
//   中文名必须来自支持库元数据。
namespace {

// 在某个支持库的数据类型里，按中文事件名找一个事件下标；找不到返回 -1。
// bIsWindow=TRUE 时只认"窗口/组件"类型（用 EcDtIsUnit / EC_LDT_WIN_UNIT 判）。
int EcFindEventIdxByName(int lib, int dt, LPCTSTR pszName, BOOL bIsWindow)
{
    if (bIsWindow && !EcDtIsUnit(lib, dt)) return -1;
    const EcDataType* d = EcDtAt(lib, dt);
    if (d == NULL) return -1;
    for (int k = 0; k < d->eventCount; k++)
    {
        const EcEvent* e = EcEventAt(lib, dt, k);
        if (e != NULL && e->name == pszName) return k;
    }
    return -1;
}

// 取"某数据类型第 idx 个事件"的中文名；找不到返回空串
CString EcEventNameAt(int lib, int dt, int idx)
{
    const EcEvent* e = EcEventAt(lib, dt, idx);
    return (e != NULL) ? e->name : CString();
}

// 英文事件名 -> 中文事件名。只覆盖**实测验证过**的；其余返回原串。
// 返回值是"事件名"这一部分（不含前缀下划线）。
//
// 验证来源（`ECLONE_DUMP_EVENTS` 倒出的支持库事件名表，见 `_ida/events_dump.u8.txt`）：
//   `DT 10 ... 按钮 events=1`  ->  `E 0 被单击`
//   `DT 0  ... 窗口 events=13` ->  `E 0 创建完毕`
// 所以这两个对应有**支持库元数据**背书，不是猜的。
// ⚠️ 其余英文事件名的对应关系**没有来源**（英化插件的英文名不在库里，见 HANDOFF §6.11），
//    一律原样返回 —— 宁可显示英文名，也不编一个错的中文名。
CString EcLocalizeEventWord(LPCTSTR pszEn)
{
    if (_tcsicmp(pszEn, _T("Click")) == 0)
        return CString(_T("被单击"));
    if (_tcsicmp(pszEn, _T("AfterCreate")) == 0)
        return CString(_T("创建完毕"));
    return CString(pszEn);      // 未验证的一律原样
}

// 生成一个子程序在左树/子程序头里显示的名字。
// 输入是 .e 里的裸名（如 `_cmdSort_Click` / `__MainForm_AfterCreate`）。
// lib：支持库下标（>=0 时用元数据校验中文事件名真的存在；-1 时只做静态映射）。
CString EcLocalizeSubName(LPCTSTR pszRaw, int lib)
{
    CString s(pszRaw);
    (void)lib;      // 校验用 EcFindEventIdxByName 需要具体数据类型，见函数头说明

    // 1) 窗口的"创建完毕"：.e 里是 `__MainForm_AfterCreate`
    //    原版显示 `__<窗口名>_创建完毕`；窗口名先从 .e 窗口段取，取不到再用 `_启动窗口`。
    if (s.CompareNoCase(_T("__MainForm_AfterCreate")) == 0)
        return CString(_T("__启动窗口_创建完毕"));

    // 2) 组件事件：`_<组件名>_<英文事件名>`
    if (s.GetLength() > 2 && s[0] == _T('_') && s[1] != _T('_'))
    {
        int k = s.Find(_T('_'), 1);
        if (k > 1 && k + 1 < s.GetLength())
        {
            CString ctrl = s.Mid(1, k - 1);        // 组件名
            CString word = s.Mid(k + 1);           // 英文事件名
            CString cn   = EcLocalizeEventWord(word);
            if (cn.Compare(word) != 0)
            {
                CString out;
                out.Format(_T("_%s_%s"), (LPCTSTR)ctrl, (LPCTSTR)cn);
                return out;
            }
        }
    }
    return s;
}

} // namespace

// ===========================================================================
// 诊断（只在 ECLONE_DUMP_EPROJ 给了路径时写盘）：LoadEProject 哪一步返回 FALSE
static void EcDbgFail(LPCTSTR pszWhy, const CString& sExtra)
{
    TCHAR szDump[MAX_PATH * 2];
    if (::GetEnvironmentVariable(_T("ECLONE_DUMP_EPROJ"), szDump, MAX_PATH * 2) <= 0)
        return;
    CString sPath = szDump;
    sPath += _T(".fail.txt");
    CStdioFile f;
    CFileException e;
    // 追加写：这样一次运行里每一步的痕迹都留着（modeWrite 会截断）
    if (!f.Open(sPath, CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite | CFile::typeBinary, &e)
        && !f.Open(sPath, CFile::modeWrite | CFile::typeBinary, &e))
        return;
    f.SeekToEnd();
    CString s;
    s.Format(_T("FAIL: %s\r\n%s\r\n"), pszWhy, (LPCTSTR)sExtra);
    f.Write(s, (UINT)s.GetLength());
    f.Close();
    ::OutputDebugString(s);
}

BOOL EcIsEFile(LPCTSTR pszPath)
{
    CFile f;
    CFileException e;
    if (!f.Open(pszPath, CFile::modeRead | CFile::shareDenyWrite, &e)) return FALSE;
    char hdr[8];
    BOOL bOk = (f.Read(hdr, 8) == 8) && (::memcmp(hdr, "CNWTEPRG", 8) == 0);
    f.Close();
    return bOk;
}

// ECLONE_TIME=1 时分阶段计时（打到 stderr）
static void EcTimeMark2A(const char* pszStage, DWORD dwStart)
{
    char szOn[8];
    if (::GetEnvironmentVariableA("ECLONE_TIME", szOn, sizeof(szOn)) == 0) return;
    char szMsg[256];
    ::wsprintfA(szMsg, "[ECLONE_TIME] %s : %lu ms\n", pszStage,
                (unsigned long)(::GetTickCount() - dwStart));
    ::OutputDebugStringA(szMsg);
    ::fputs(szMsg, stderr);
}

static void EcTimeMark2(LPCTSTR pszStage, DWORD dwStart)
{
    char szOn[8];
    if (::GetEnvironmentVariableA("ECLONE_TIME", szOn, sizeof(szOn)) == 0) return;
    char szMsg[256];
    ::wsprintfA(szMsg, "[ECLONE_TIME] %s : %lu ms\n", (const char*)pszStage,
                (unsigned long)(::GetTickCount() - dwStart));
    ::OutputDebugStringA(szMsg);
    ::fputs(szMsg, stderr);
}

// 性能诊断累加器（ECLONE_TIME=1 时打印）
static DWORD g_dwDecodeMs = 0;
static DWORD g_dwAppendMs = 0;
static unsigned g_nBodyLines = 0;
static unsigned g_nSubs = 0;

BOOL EcLoadEProject(LPCTSTR pszPath, CString& strOut)
{
    strOut.Empty();
    DWORD dwT0 = ::GetTickCount();
    DWORD dwT1 = 0, dwT2 = 0, dwT3 = 0;

    CString sTrace;
    sTrace.Format(_T("path=%s\r\n"), pszPath);
    EcDbgFail(_T("enter"), sTrace);

    CFile f;
    CFileException e;
    if (!f.Open(pszPath, CFile::modeRead | CFile::shareDenyWrite, &e))
    {
        sTrace.Format(_T("Open failed cause=%d ioerr=%d\r\npath=%s"), e.m_cause, e.m_lOsError, pszPath);
        EcDbgFail(_T("Open"), sTrace);
        return FALSE;
    }

    ULONGLONG len64 = f.GetLength();
    if (len64 < 8 || len64 > 256u * 1024u * 1024u)
    {
        sTrace.Format(_T("bad len=%I64u"), len64);
        EcDbgFail(_T("length"), sTrace);
        f.Close();
        return FALSE;
    }
    size_t len = (size_t)len64;

    std::string data;
    data.resize(len);
    UINT nRead = f.Read(&data[0], (UINT)len);
    f.Close();
    if (nRead != (UINT)len)
    {
        sTrace.Format(_T("short read %u/%u"), nRead, (UINT)len);
        EcDbgFail(_T("read"), sTrace);
        return FALSE;
    }

    if (data.compare(0, 8, "CNWTEPRG", 8) != 0)
    {
        EcDbgFail(_T("magic"), sTrace);
        return FALSE;
    }

    // ---- 1. 遍历段表，找 SystemInfo（拿语言码）与 Program ----
    UINT cp = 936;                       // 默认 GBK，SystemInfo 语言码可以改它
    std::string progPayload;
    {
        size_t pos = 8;
        while (pos + 100 <= len)
        {
            unsigned int magic = (unsigned int)(unsigned char)data[pos]
                               | ((unsigned int)(unsigned char)data[pos+1] << 8)
                               | ((unsigned int)(unsigned char)data[pos+2] << 16)
                               | ((unsigned int)(unsigned char)data[pos+3] << 24);
            if (magic != 0x15117319u) break;

            unsigned int segKey = (unsigned int)(unsigned char)data[pos+8]
                                | ((unsigned int)(unsigned char)data[pos+9] << 8)
                                | ((unsigned int)(unsigned char)data[pos+10] << 16)
                                | ((unsigned int)(unsigned char)data[pos+11] << 24);
            unsigned int size = (unsigned int)(unsigned char)data[pos+56]
                              | ((unsigned int)(unsigned char)data[pos+57] << 8)
                              | ((unsigned int)(unsigned char)data[pos+58] << 16)
                              | ((unsigned int)(unsigned char)data[pos+59] << 24);

            if (segKey == ESEG_SYSINFO && size >= 12)
            {
                size_t q = pos + 100 + 8;
                if (q + 4 <= len)
                {
                    unsigned int lang = (unsigned int)(unsigned char)data[q]
                                      | ((unsigned int)(unsigned char)data[q+1] << 8)
                                      | ((unsigned int)(unsigned char)data[q+2] << 16)
                                      | ((unsigned int)(unsigned char)data[q+3] << 24);
                    cp = EcCpFromLang(lang);
                }
            }
            if (segKey == ESEG_PROGRAM)
                progPayload.assign(data, pos + 100, size);

            pos += 100 + size;
            if (segKey == ESEG_END) break;
        }
    }
    if (progPayload.empty()) return FALSE;

    dwT1 = ::GetTickCount();
    EcTimeMark2A("1_segwalk", dwT0);

    // ---- 2. 解程序段 ----
    EcProgram prog;
    if (!EcParseProgram(progPayload, cp, prog)) return FALSE;

    dwT2 = ::GetTickCount();
    EcTimeMark2A("2_parseprogram", dwT1);

    // ---- 3. 生成文本源码 ----
    // 结构：.版本 2 / .程序集(含 .程序集变量 + .子程序(含 .参数 .局部变量)) /
    //       .全局变量 / .数据类型(含 .成员) / .常量 / .DLL命令(含 .参数)
    // 顺序：照 eclone EcBuildProgTree 与表页的取用方式，声明**同类聚在一起**最稳。
    std::string out;
    // ⚠️ 必须预留！VC6 的 std::string 增长不够几何，
    //   MB 级输出不预留会退化成 O(n^2)。
    //   实测：精易模块（32172 行正文）reserve(1MB) -> 拼接 8681ms；
    //   reserve(充足) -> 拼接 **15ms**。
    out.reserve(len * 4 + (1 << 20));
    EcLine(out, _T(".版本 2"));

    // 子程序按所属程序集分组；classId 与程序集 id 的高 16 位对应关系已实测
    // （Bubble Sort.e：程序集 id=0x19000104，子程序 class=0x19000104 的高位一致），
    // 这里用「classId == 程序集 id」优先，退化成「classId 的高 16 位相等」。
    std::vector<int> owner(prog.methods.size(), -1);
    for (size_t mi = 0; mi < prog.methods.size(); mi++)
    {
        for (size_t ai = 0; ai < prog.asms.size(); ai++)
        {
            if (prog.methods[mi].classId == prog.asms[ai].id) { owner[mi] = (int)ai; break; }
        }
        if (owner[mi] < 0)
        {
            for (size_t ai = 0; ai < prog.asms.size(); ai++)
            {
                if ((prog.methods[mi].classId & 0xFFFF0000u) == (prog.asms[ai].id & 0xFFFF0000u))
                { owner[mi] = (int)ai; break; }
            }
        }
    }

    // 支持库下标：子程序名本地化要从支持库事件元数据取中文事件名，
    // 而这些元数据只对**已加载**的库有效（EcLibFindByBaseName 按 "krnln" 这种基名找）。
    // 库串形如 "名称\rGUID\r主版本\r次版本\r中文名"，取第一段即可。
    int libIdx = -1;
    if (!prog.libs.empty())
    {
        CString sLib = EcToGbk(prog.libs[0], cp);
        int k = sLib.Find(_T('\r'));
        CString base = (k > 0) ? sLib.Left(k) : sLib;
        base.TrimLeft(); base.TrimRight();
        if (!base.IsEmpty()) libIdx = EcLibFindByBaseName(base);
    }

    // 符号名表：`%08X|名字`，08X = 元素 id。
    // 依据：原版渲染器 sub_4C1150 在 case 0x38（变量/子程序引用）里做的是
    //   `sub_4C7F10(id, &结构, 0)` 查一张 **{id → 元素结构*}** 表，命中就输出结构体
    //   **+12** 处的名字字符串（4.0 反编译实证，见 D:\EPL\_ida\NAME_MAP_FINDING.md）。
    // 也就是说原版没有"翻译 id"的算法，只有一张覆盖**所有元素**的表：子程序 / 变量 /
    // 常量 / 程序集 / 数据类型 / DLL 命令 …… 全都进同一张表。这里就按这个口径建。
    // ⚠️ 在第 41 轮之前，传给 EcDecodeExpression 的是一张**恒为空**的表，
    //    于是正文里每个子程序调用都退化成 `用户子程序_%08X`、变量退化成 `var_%08X`、
    //    常量退化成 `#常量%d`。第 42 轮起按原版口径建全表。
    std::vector<CString> symTable;

    // 局部小工具：把一个 (id, 名字) 加进表（名字为空就跳过，避免脏项遮住有效项）
    struct EcSymAdder
    {
        std::vector<CString>* pOut;
        void Add(unsigned int id, const CString& nm)
        {
            if (id == 0 || nm.IsEmpty()) return;
            CString s;
            s.Format(_T("%08X|%s"), id, (LPCTSTR)nm);
            pOut->push_back(s);
        }
    };
    EcSymAdder add;
    add.pOut = &symTable;

    // ⚠️ VC6：循环变量作用域不隔离，这里所有变量都必须用**本文件别处没用过**的名字
    size_t symG, symM, symA, symC, symD, symS, symV;
    for (symG = 0; symG < prog.globals.size(); symG++)
        add.Add(prog.globals[symG].id, EcToGbk(prog.globals[symG].name, cp));
    for (symC = 0; symC < prog.consts.size(); symC++)
        add.Add(prog.consts[symC].id, EcToGbk(prog.consts[symC].name, cp));
    for (symS = 0; symS < prog.structs.size(); symS++)
    {
        add.Add(prog.structs[symS].id, EcToGbk(prog.structs[symS].name, cp));
        for (symV = 0; symV < prog.structs[symS].members.size(); symV++)
            add.Add(prog.structs[symS].members[symV].id,
                    EcToGbk(prog.structs[symS].members[symV].name, cp));
    }
    for (symD = 0; symD < prog.dlls.size(); symD++)
    {
        add.Add(prog.dlls[symD].id, EcToGbk(prog.dlls[symD].name, cp));
        for (symV = 0; symV < prog.dlls[symD].params.size(); symV++)
            add.Add(prog.dlls[symD].params[symV].id,
                    EcToGbk(prog.dlls[symD].params[symV].name, cp));
    }
    for (symA = 0; symA < prog.asms.size(); symA++)
    {
        add.Add(prog.asms[symA].id, EcToGbk(prog.asms[symA].name, cp));
        for (symV = 0; symV < prog.asms[symA].vars.size(); symV++)
            add.Add(prog.asms[symA].vars[symV].id,
                    EcToGbk(prog.asms[symA].vars[symV].name, cp));
    }
    for (symM = 0; symM < prog.methods.size(); symM++)
    {
        CString sNm = EcToGbk(prog.methods[symM].name, cp);
        add.Add(prog.methods[symM].id, EcLocalizeSubName(sNm, libIdx));
        // 子程序的参数与局部变量也在同一张符号表里（原版表按 id 全局索引）
        for (symV = 0; symV < prog.methods[symM].params.size(); symV++)
            add.Add(prog.methods[symM].params[symV].id,
                    EcToGbk(prog.methods[symM].params[symV].name, cp));
        for (symV = 0; symV < prog.methods[symM].locals.size(); symV++)
            add.Add(prog.methods[symM].locals[symV].id,
                    EcToGbk(prog.methods[symM].locals[symV].name, cp));
    }

    for (size_t ai = 0; ai < prog.asms.size(); ai++)
    {
        const EcAsm& a = prog.asms[ai];
        EcLine(out, _T(""));
        CString line;
        line.Format(_T(".程序集 %s"), (LPCTSTR)EcToGbk(a.name, cp));
        EcLine(out, line);
        for (size_t vi = 0; vi < a.vars.size(); vi++)
            EcLine(out, EcVarLine(_T(".程序集变量"), a.vars[vi], cp, FALSE, libIdx, prog));

        for (size_t mi = 0; mi < prog.methods.size(); mi++)
        {
            if (owner[mi] != (int)ai) continue;
            const EcMethod& m = prog.methods[mi];

            EcLine(out, _T(""));
            CString ret = EcEpTypeName(m.retType, libIdx);
            // 名字要按原版本地化（.e 里是被英化插件改过的英文事件名，见文件头说明）
            CString name = EcToGbk(m.name, cp);
            name = EcLocalizeSubName(name, libIdx);
            CString note = EcToGbk(m.comment, cp);
            // .子程序 名, 返回值类型, , 公开, 备注
            line.Format(_T(".子程序 %s, %s, , %s, %s"),
                        (LPCTSTR)name, (LPCTSTR)ret,
                        (m.flags & 0x80) ? _T("公开") : _T(""), (LPCTSTR)note);
            EcLine(out, line);

            for (size_t pi = 0; pi < m.params.size(); pi++)
                EcLine(out, EcVarLine(_T(".参数"), m.params[pi], cp, TRUE, libIdx, prog));
            for (size_t li = 0; li < m.locals.size(); li++)
                EcLine(out, EcVarLine(_T(".局部变量"), m.locals[li], cp, FALSE, libIdx, prog));

            dwT3 = ::GetTickCount();
    EcTimeMark2A("3_textgen", dwT2);

    // ---- 子程序正文：解码 expression 字节码 ----
            // 规格见 D:\EPL\_ida\SEGMENT_SPEC.md §3.6（321 子程序验证过）
            {
                DWORD tA = ::GetTickCount();
                CString strErr;
                std::vector<CString> body = EcDecodeExpression(
                    m.expr.empty() ? NULL : &m.expr[0], m.expr.size(),
                    m.lineOffset.empty() ? NULL : &m.lineOffset[0], m.lineOffset.size(),
                    libIdx, strErr, &symTable);
                DWORD tB = ::GetTickCount();
                g_dwDecodeMs += (tB - tA);
                for (size_t bi = 0; bi < body.size(); bi++)
                    EcLinePref(out, "    ", body[bi]);
                if (!strErr.IsEmpty())
                    EcLinePref(out, "    ' [解码提示] ", strErr);
                g_dwAppendMs += (::GetTickCount() - tB);
                g_nBodyLines += (unsigned)body.size();
                g_nSubs++;
            }
        }
    }

    // 全局变量
    for (size_t gi = 0; gi < prog.globals.size(); gi++)
        EcLine(out, EcVarLine(_T(".全局变量"), prog.globals[gi], cp, FALSE, libIdx, prog));

    // 自定义数据类型
    for (size_t si = 0; si < prog.structs.size(); si++)
    {
        const EcStruct& st = prog.structs[si];
        EcLine(out, _T(""));
        CString line;
        line.Format(_T(".数据类型 %s"), (LPCTSTR)EcToGbk(st.name, cp));
        EcLine(out, line);
        if (!st.comment.empty())
        {
            line.Format(_T(".备注 %s"), (LPCTSTR)EcToGbk(st.comment, cp));
            EcLine(out, line);
        }
        for (size_t mi = 0; mi < st.members.size(); mi++)
            EcLine(out, EcVarLine(_T(".成员"), st.members[mi], cp, FALSE, libIdx, prog));
    }

    // 常量表
    for (size_t ci = 0; ci < prog.consts.size(); ci++)
    {
        const EcEpConst& c = prog.consts[ci];
        CString val;
        if (c.kind == 1)      val = c.boolVal ? _T("真") : _T("假");
        else if (c.kind == 3) val.Format(_T("\"%s\""), (LPCTSTR)EcToGbk(c.strVal, cp));
        else if (c.kind == 4) val.Format(_T("字节集(%u 字节)"), c.binLen);
        else                   val = EcNumberText(c.numVal);

        CString line;
        line.Format(_T(".常量 %s, %s, %s, %s"),
                    (LPCTSTR)EcToGbk(c.name, cp), (LPCTSTR)val,
                    (c.flags & 0x1) ? _T("公开") : _T(""),
                    (LPCTSTR)EcToGbk(c.comment, cp));
        EcLine(out, line);
    }

    // DLL 命令
    for (size_t di = 0; di < prog.dlls.size(); di++)
    {
        const EcDll& d = prog.dlls[di];
        EcLine(out, _T(""));
        CString line;
        // 位置语义（ec_editor.cpp）：名, 返回值类型, "库文件名", "在库中对应命令名", 公开, 备注
        line.Format(_T(".DLL命令 %s, %s, \"%s\", \"%s\", %s, %s"),
                    (LPCTSTR)EcToGbk(d.name, cp),
                    (LPCTSTR)EcEpTypeName(d.retType, libIdx),
                    (LPCTSTR)EcToGbk(d.dll, cp),
                    (LPCTSTR)EcToGbk(d.entry, cp),
                    (d.flags & 0x2) ? _T("公开") : _T(""),
                    (LPCTSTR)EcToGbk(d.comment, cp));
        EcLine(out, line);
        for (size_t pi = 0; pi < d.params.size(); pi++)
            EcLine(out, EcVarLine(_T(".参数"), d.params[pi], cp, TRUE, libIdx, prog));
    }

    {
        char m[256];
        ::wsprintfA(m, "[ECLONE_TIME] D_decode=%lu ms  D_append=%lu ms  subs=%u  bodylines=%u\n",
                    (unsigned long)g_dwDecodeMs, (unsigned long)g_dwAppendMs,
                    g_nSubs, g_nBodyLines);
        ::fputs(m, stderr);
    }
    strOut = out.c_str();
    return TRUE;
}

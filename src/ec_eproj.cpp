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
#include <map>

// ------------------------------------------------------------------ 段 key
#define ESEG_USERINFO     0x01007319u
#define ESEG_SYSINFO      0x02007319u
#define ESEG_PROGRAM      0x03007319u
#define ESEG_RESOURCE     0x04007319u
#define ESEG_LOSSY        0x05007319u
#define ESEG_EDITINFO     0x06007319u
#define ESEG_END          0x07007319u
#define ESEG_INITMODULE   0x08007319u
#define ESEG_EDITINFO2    0x09007319u
#define ESEG_AUXINFO1     0x0A007319u
#define ESEG_AUXINFO2     0x0B007319u
#define ESEG_MODREC       0x0C007319u
#define ESEG_EPKGINFO1    0x0D007319u
#define ESEG_EDITFILTER   0x0E007319u
#define ESEG_COMPILECOND  0x0F007319u
#define ESEG_AUXINFO3     0x10007319u
#define ESEG_MODREC2      0x11007319u

namespace {

// 正文里的解码诊断行（`' [解码提示] …`）**不是原版行为**，默认关闭。
// 依据：原版自己的剪贴板文本（tools/text_oracle.py 落到 shots/cmp/orig_sel.txt）里
// 没有任何提示行 —— 它只画 .e 里真实存在的语句与注释。
BOOL EcShowDecodeDiag()
{
    TCHAR szD[8];
    return ::GetEnvironmentVariable(_T("ECLONE_SHOW_DECODE_DIAG"), szD, 8) > 0;
}

// ------------------------------------------------- Resource 段：组件 (id, 名字)
// EcToGbk 定义在本文件后面（按 GBK→GBK 校验一遍），这里先声明。
CString EcToGbk(const std::string& raw, UINT srcCp);

// 判据（2026-10 实测，flowcmd.e）：
//   · AuxInfo1 段给出 `form=52010001 comp=16010003 slot=0 method=04010007`，
//     而 04010007 就是 `_按钮1_被单击` ⇒ 按钮1 = **0x16010003**（tools/auxinfo_dump.py）；
//   · 记录的 `[u32 part_count][u32 part_len]` 之后就是 part_data，布局是
//       `[u32 id × n][u32 offset × n][ (u32 len)(元素) × n ]`（n = part_count）
//     —— **没有额外的 count/totalSize 前缀**，offset 相对「块区」起点（= part_data + 8n）。
//     flowcmd.e 实测：n=3，ids={06010000,16010003,16010002}，offsets={0,217,361}，
//     块区=body+0x18；block0 off=0 → len=213 → 数据在 0x1C（`01 00 01 00`=65537 窗口类型）✓，
//     block1 off=217 → 0xF1 处 len=140 → 数据在 0xF5（`0B 00 01 00`=65547 按钮）✓，
//     block2 off=361 → 0x185 处 `08 00 01 00`=65544 画板 ✓（217+4+140=361 正好接上）。
//   · 元素 = `[I32 dataType][20 字节][+24 处 NUL 结尾的 GBK 名字] …`：
//     按钮1 的数据起点 0xF5 + 0x18 = 0x10D = `按钮1`，画板1 的 0x185 + 0x18 = 0x19D = `画板1`。
//   ⇒ 原版渲染器 sub_4C1150 的 case '8' 用 `sub_4C7F10(id,&elem,0)` 查的
//     「{id→元素*} 全元素表」里就有这些组件，所以正文才能写出 `画板1.滚动写行 (…)`。
static unsigned int EcRdU32(const std::string& s, size_t o)
{
    return (unsigned int)(unsigned char)s[o]
         | ((unsigned int)(unsigned char)s[o+1] << 8)
         | ((unsigned int)(unsigned char)s[o+2] << 16)
         | ((unsigned int)(unsigned char)s[o+3] << 24);
}

// 名字合法性：1..64 字节、无控制字符（防止把随机的 4 字节 token 当名字）
static BOOL EcPlausibleName(const std::string& s)
{
    if (s.empty() || s.size() > 64) return FALSE;
    for (size_t k = 0; k < s.size(); k++)
    {
        unsigned char c = (unsigned char)s[k];
        if (c < 0x20 || c == 0x7F) return FALSE;
    }
    return TRUE;
}

static void EcParseResourceComponentNames(const std::string& p, std::vector<CString>& out, UINT cp)
{
    if (p.size() < 8) return;
    size_t i = 4 + (size_t)EcRdU32(p, 0);          // 跳过窗口描述符区
    for (int guard = 0; guard < 4096; guard++)
    {
        if (i + 16 > p.size()) break;
        size_t q = i + 8;                          // [u32 hash][u32 tag]
        unsigned int l1 = EcRdU32(p, q); q += 4;   // 窗口名（mfc_string，本函数不用）
        if (l1 > p.size() - q) break;
        q += l1;
        unsigned int l2 = EcRdU32(p, q); q += 4;   // 备注
        if (l2 > p.size() - q) break;
        q += l2;
        if (q + 8 > p.size()) break;
        unsigned int n    = EcRdU32(p, q);         // [u32 part_count]
        unsigned int blen = EcRdU32(p, q + 4);     // [u32 part_len]
        size_t b = q + 8;
        if (blen == 0 || blen > p.size() - b) break;

        if (n > 0 && n < 4096 && 8 * (size_t)n <= blen)
        {
            size_t blocks = b + 8 * (size_t)n;                 // 块区起点
            for (unsigned int e = 0; e < n; e++)
            {
                unsigned int id  = EcRdU32(p, b + 4 * e);
                unsigned int off = EcRdU32(p, b + 4 * (size_t)n + 4 * e);
                size_t blk = blocks + off;
                if (blk + 4 > b + blen) continue;                // 越界保护
                unsigned int elen = EcRdU32(p, blk);
                size_t el = blk + 4;
                if (elen < 28 || el + elen > b + blen) continue;
                // 数据类型高 16 位 = 0x0001（窗口/组件族），当防错校验
                if ((EcRdU32(p, el) & 0xFFFF0000u) != 0x00010000u) continue;
                size_t nm = el + 24;
                std::string nmS;
                while (nm < el + elen && p[nm] != '\0' && nmS.size() < 65) { nmS += p[nm]; nm++; }
                if (!EcPlausibleName(nmS)) continue;
                CString s;
                s.Format(_T("%08X|%s"), id, (LPCTSTR)EcToGbk(nmS, cp));
                out.push_back(s);
            }
        }
        i = b + blen + 12;                         // 12 字节零结尾
    }
}

// ===========================================================================
// 程序资源段（0x04007319）→ 窗体表 + 组件表（设计器要用）
//
// 与上面那个启发式函数的分工：那个只抠 (id, 名字) 喂正文渲染的符号表；
// 这个按**逐字段**顺序解析，把几何、父组件、显示状态、以及**支持库属性数据**
// （= PFN_CREATE_UNIT 的 pAllData）都取出来，供设计器还原真窗体。
// 字段序依据：4 个真样本逐字节互证（PLAN_EPARSE.md R3），读法与 e-packager 的
// ReadFormElements 字段序一致（那是旁证，不作判据）。
static BOOL EcRdCStr(const std::string& s, size_t& o, std::string& out)
{
    size_t e = o;
    while (e < s.size() && s[e] != '\0') e++;
    if (e >= s.size()) return FALSE;              // 没有终止符 = 越界
    out.assign(s.data() + o, e - o);
    o = e + 1;
    return TRUE;
}

static BOOL EcRdMfcStr(const std::string& s, size_t& o, std::string& out)
{
    if (o + 4 > s.size()) return FALSE;
    unsigned int n = EcRdU32(s, o);
    o += 4;
    if (n > 65535u || o + (size_t)n > s.size()) return FALSE;
    out.assign(s.data() + o, n);
    o += n;
    return TRUE;
}

static void EcParseResource(const std::string& res, UINT cp, std::vector<EcUnitForm>& forms)
{
    forms.clear();
    if (res.size() < 16) return;
    size_t i = 4 + (size_t)EcRdU32(res, 0);     // 跳过「表单描述符区」（8 字节 × 窗口数）
    while (i + 16 <= res.size())
    {
        size_t q = i + 8;                          // [u32 hash][u32 tag]
        std::string wname, wcmt;
        if (!EcRdMfcStr(res, q, wname)) break;
        if (!EcRdMfcStr(res, q, wcmt)) break;
        if (q + 8 > res.size()) break;
        unsigned int n    = EcRdU32(res, q);
        unsigned int blen = EcRdU32(res, q + 4);
        size_t b = q + 8;
        if (n == 0 || n > 1024 || blen == 0 || b + blen > res.size() || 8u * n > blen) break;
        size_t blocks = b + 8 * (size_t)n;         // 块区起点（offset 相对这里）

        EcUnitForm f;
        f.name    = EcToGbk(wname, cp);
        f.comment = EcToGbk(wcmt, cp);
        int fw = 0, fh = 0;
        for (unsigned int e = 0; e < n; e++)
        {
            unsigned int off = EcRdU32(res, b + 4 * (size_t)n + 4 * (size_t)e);
            size_t blk = blocks + off;
            if (blk + 4 > b + blen) continue;
            unsigned int elen = EcRdU32(res, blk);
            size_t el = blk + 4;
            if (elen < 32 || el + elen > b + blen) continue;

            EcUnitBlob u;
            u.id       = EcRdU32(res, b + 4 * (size_t)e);
            u.dataType = EcRdU32(res, el);
            size_t o = el + 24;                    // 4 = dataType，20 = 未解字节
            std::string nm, cm, tag;
            if (!EcRdCStr(res, o, nm)) continue;
            if (!EcRdCStr(res, o, cm)) continue;
            if (o + 20 > el + elen) continue;
            u.name   = EcToGbk(nm, cp);
            u.left   = (int)EcRdU32(res, o +  4);
            u.top    = (int)EcRdU32(res, o +  8);
            u.width  = (int)EcRdU32(res, o + 12);
            u.height = (int)EcRdU32(res, o + 16);
            o += 20;
            o += 4;                                // unknownBeforeParent
            u.parent = EcRdU32(res, o); o += 4;
            unsigned int nch = EcRdU32(res, o);   o += 4;
            if (o + 4u * nch > el + elen) continue;
            o += 4u * nch;                          // 子组件 id 表
            unsigned int clen = EcRdU32(res, o); o += 4;   // cursor
            if (o + clen > el + elen) continue;
            o += clen;
            if (!EcRdCStr(res, o, tag)) continue;   // tag 是 NUL 串（实测：按长度前缀读会把
                                                    // 后面所有字段推 3 字节，库直接拒收属性流）
            o += 4;                                 // unknownBeforeVisible
            if (o + 4 > el + elen) continue;
            u.showStatus = EcRdU32(res, o); o += 4;
            o += 4;                                 // tabIndex
            unsigned int nevt = EcRdU32(res, o); o += 4;
            if (o + 8u * nevt + 20 > el + elen) continue;
            o += 8u * nevt;                         // [key,value] × n
            o += 20;                                // 20 字节（实测全 0）
            u.blob.assign(res.data() + o, el + elen - o);   // ★ 支持库属性数据
            if ((u.dataType & 0xFFFFu) == 1u && fw == 0)    // 窗体自己那条 = 窗体几何
            {
                f.left = u.left; f.top = u.top; f.width = u.width; f.height = u.height;
                fw = 1;
            }
            f.units.push_back(u);
        }
        if (fw == 0) { f.left = f.top = 0; f.width = 0; f.height = 0; }
        forms.push_back(f);

        size_t nxt = b + blen;
        BOOL bZeroTail = FALSE;
        if (nxt + 12 <= res.size())
        {
            bZeroTail = TRUE;
            for (int k = 0; k < 12; k++) if (res[nxt + k] != '\0') { bZeroTail = FALSE; break; }
        }
        i = bZeroTail ? (nxt + 12) : nxt;
        if (i + 16 > res.size()) break;
    }
}

static std::vector<EcUnitForm> g_vEForms;

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
    // 诊断（ECLONE_DUMP_CONSTS 时开）：Program 段走到常量表时 buffer 的真实状态。
    // 用途：判定 `count=0` 到底是「这里就是段尾、本来就没常量」，还是「偏移走错了」。
    if (::GetEnvironmentVariable(_T("ECLONE_DUMP_CONSTS"), NULL, 0) > 0)
    {
        FILE* fd = _tfopen(_T("D:\\EPL\\_ida\\consts_probe.txt"), _T("a"));
        if (fd != NULL)
        {
            fprintf(fd, "EcParseConsts: b.i=%u b.n=%u left=%u  count=%u datalenth=%u bad=%d\n",
                    (unsigned)b.i, (unsigned)b.n, (unsigned)b.Left(), count, datalenth,
                    (int)b.bad);
            fclose(fd);
        }
    }
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

// 解析「自定义常量表」—— 它在**程序资源段**，不在程序段。
//
// 判据（第 42 轮字节级实证，详见 D:\EPL\HANDOFF.md §5）：
//   * 精易模块程序段头部第 5 字段 minRequiredConstants = 0xF4(244) -> 确实有常量，
//     但 `EcParseConsts` 在程序段尾部读到的是 `"dwNumberOfBytesWritten"` 垃圾
//     （诊断：b.i=4293370 b.n=4293402 left=32 count=0）。
//   * 程序资源段 payload 布局（段内相对偏移）：
//       +0      u32 0
//       +4      u32 条目数 N
//       +8      u32 数据长度
//       +12     u32 ids[N]      全部 kind==0x08（自定义常量族，见 sub_44F1C0）
//       +12+4N  u32 offs[N]     每条记录相对「记录区起点」的偏移
//       20980   记录区起点（第一条实测 total=744 / mark=0x10 / GBK "精易论坛模块注释"）
//     记录格式： [u32 total][u32 mark][GBK 名字]\0 ... 到 total 为止
//   * 验真：正文里我们渲染成 `#常量402825463` 的 id `0x1802A0F7` 用这套算法
//     解出名字 **`_gb2312`**（实测）。
//
// ⚠️ 记录区起点 20980 与 `offs` 的基准是**实测**出来的（试过 12+4N、试过绝对偏移，
//    都会解出垃圾）。若将来换样本失败，先用
//    `python -B D:\EPL\_ida\const_lookup.py <file.e> <id>` 复量这个基准。
static bool EcParseUserConsts(const std::string& payload, std::vector<EcEpConst>& out)
{
    out.clear();
    if (payload.size() < 16) return false;

    const unsigned char* p = (const unsigned char*)payload.data();
    size_t n = payload.size();

    unsigned int count = (unsigned int)p[4] | ((unsigned int)p[5] << 8)
                       | ((unsigned int)p[6] << 16) | ((unsigned int)p[7] << 24);
    if (count == 0 || count > 200000) return false;

    size_t idsAt  = 12;
    size_t offsAt = idsAt + (size_t)count * 4;
    size_t recAt  = 20980;                    // 记录区起点（实测，见上面说明）
    if (offsAt + (size_t)count * 4 > n) return false;
    if (recAt >= n) return false;

    const unsigned int REC_AREA = 20980;
    unsigned int k;
    for (k = 0; k < count; k++)
    {
        unsigned int id = (unsigned int)p[idsAt + k * 4]
                        | ((unsigned int)p[idsAt + k * 4 + 1] << 8)
                        | ((unsigned int)p[idsAt + k * 4 + 2] << 16)
                        | ((unsigned int)p[idsAt + k * 4 + 3] << 24);
        unsigned int off = (unsigned int)p[offsAt + k * 4]
                         | ((unsigned int)p[offsAt + k * 4 + 1] << 8)
                         | ((unsigned int)p[offsAt + k * 4 + 2] << 16)
                         | ((unsigned int)p[offsAt + k * 4 + 3] << 24);
        size_t rp = (size_t)REC_AREA + off;
        if (rp + 8 > n) continue;
        unsigned int total = (unsigned int)p[rp] | ((unsigned int)p[rp + 1] << 8)
                           | ((unsigned int)p[rp + 2] << 16) | ((unsigned int)p[rp + 3] << 24);
        if (total < 8 || total > 40000) continue;
        if (rp + 4 + total > n) continue;

        // 名字从 +8 起（跳过 mark 那 4 字节），到 NUL 或记录尾为止
        size_t nmAt = rp + 8;
        size_t nmEnd = rp + 4 + total;
        if (nmEnd > n) nmEnd = n;
        size_t e = nmAt;
        while (e < nmEnd && p[e] != 0) e++;
        std::string nm((const char*)p + nmAt, e - nmAt);

        EcEpConst c;
        c.id      = id;
        c.flags   = 0;
        c.kind    = 0;
        c.numVal  = 0.0;
        c.boolVal = false;
        c.binLen  = 0;
        c.name    = nm;
        out.push_back(c);
    }
    return !out.empty();
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
    // ⚠️ minRequiredConstants 原本被丢弃。它其实说明「这个工程需要哪些常量」，
    //    是判定「常量表该不该有内容 / 我们是不是读错了地方」的直接证据。
    {
        std::string sReq = b.MfcString();
        if (::GetEnvironmentVariable(_T("ECLONE_DUMP_CONSTS"), NULL, 0) > 0)
        {
            FILE* fr = _tfopen(_T("D:\\EPL\\_ida\\consts_probe.txt"), _T("a"));
            if (fr != NULL)
            {
                fprintf(fr, "minRequiredConstants: len=%d bytes=", (int)sReq.size());
                size_t q;
                for (q = 0; q < sReq.size() && q < 64; q++)
                    fprintf(fr, "%02X", (unsigned char)sReq[q]);
                fprintf(fr, "\n");
                fclose(fr);
            }
        }
    }
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
// 原版的**文本形式**省略尾部空字段：`.子程序 _按钮1_被单击`、`.局部变量 变量1, 整数型`
// 而不是 `…, , , , `（判据：原版自己的剪贴板文本 tools/text_oracle.py → shots/cmp/orig_sel.txt
// 第 5/6 行）。屏幕上这些字段是表格列，列由表头决定，不靠这些逗号。
CString EcTrimTailFields(CString s)
{
    for (;;)
    {
        s.TrimRight();
        if (s.Right(2) == _T(", ")) { s = s.Left(s.GetLength() - 2); continue; }
        if (s.Right(1) == _T(","))  { s = s.Left(s.GetLength() - 1); continue; }
        break;
    }
    return s;
}

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
    return EcTrimTailFields(line);
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

// ===========================================================================
// 段表（e.exe 口径）—— 依据 REA/IDA 伪 C `sub_45B9B0`（读盘总控）
//
//   文件 = "CNWTEPRG" + N × [u32 magic=0x15117319][u32 checksum][92 字节段体] + payload
//     · checksum = `sub_4FF040(段体, 92)` = **4 通道 lane-XOR**（逐字节 buf[i] ^= lane[i&3]，
//       不是"单字节 XOR 和"；571 个真 .e 全量复现，见 tools\e_sections.py）
//     · 段体 +0  = 段 key（小端 4 字节）
//     · 段体 +4  = 段名 30 字节，解码 `Name[i] ^= Key[(i+1)%4]`（`sub_4FF010`），内容是 **GBK**
//       —— 5.9 实测是中文：系统信息段 / 用户信息段 / 程序资源段 / 程序段 / 初始模块段 /
//       可丢失程序段 / 编辑信息段2 / 辅助信息段1 / "s"(End)（旧文档写的英文名是照 4.0 猜的）
//     · 段体 +36 = index，读盘时按 1,2,3… 逐段校验
//     · 段体 +48 = payload 长度；payload 紧跟 100 字节段头，读完整段后 seek 到 位置+size
//       （**不信任读者**：读者少读了就 seek 过去，多读了就 seek 回来 + 记一条提示）
struct EcSegInfo
{
    unsigned int idx;
    unsigned int key;
    CString      name;
    unsigned int size;
    size_t       off;
    BOOL         ckOk;
    BOOL         idxOk;
};

struct EcEventBind          // 辅助信息段1 的一条：组件事件 → 子程序
{
    unsigned int form, comp, slot, method;
};

static std::vector<EcSegInfo>   g_vESegs;        // 本文件的段表（倒档/诊断用）
static std::vector<EcEventBind> g_vEEventBinds;  // 辅助信息段1 解出的绑定表
static std::vector<CString>     g_vEUserStrings; // 用户信息段的 10 个字符串（U00 = 程序名称）
static CString                  g_sEProjPath;    // 最近一次打开的 .e 全路径（原版标题里带它）

static unsigned int EcRd32At(const std::string& s, size_t o)
{
    return (unsigned int)(unsigned char)s[o]
         | ((unsigned int)(unsigned char)s[o+1] << 8)
         | ((unsigned int)(unsigned char)s[o+2] << 16)
         | ((unsigned int)(unsigned char)s[o+3] << 24);
}

static unsigned int EcRd32Raw(const unsigned char* p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8)
         | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

// `sub_4FF040`：4 通道 lane-XOR 校验和
static unsigned int EcLaneXor(const unsigned char* p, size_t n)
{
    unsigned char lane[4] = { 0, 0, 0, 0 };
    for (size_t i = 0; i < n; i++) lane[i & 3] ^= p[i];
    return (unsigned int)lane[0]
         | ((unsigned int)lane[1] << 8)
         | ((unsigned int)lane[2] << 16)
         | ((unsigned int)lane[3] << 24);
}

// `sub_4FF010`：段名解码 Name[i] ^= Key[(i+1)%4]（GBK 文本）
static CString EcSegNameDecode(const unsigned char* body, unsigned int key)
{
    unsigned char k[4];
    k[0] = (unsigned char)(key      );
    k[1] = (unsigned char)(key >>  8);
    k[2] = (unsigned char)(key >> 16);
    k[3] = (unsigned char)(key >> 24);
    std::string s;
    for (int i = 0; i < 30; i++)
    {
        unsigned char c = (unsigned char)(body[4 + i] ^ k[(i + 1) & 3]);
        if (c == 0) break;
        s += (char)c;
    }
    return EcToGbk(s, 936);
}

// 段 key → 中文名（名字本身也从段体里解出来；这张表只给"key 有意义但段名不可读"时兜底）
static LPCTSTR EcSegKeyName(unsigned int key)
{
    switch (key)
    {
    case ESEG_USERINFO:    return _T("用户信息段");
    case ESEG_SYSINFO:     return _T("系统信息段");
    case ESEG_PROGRAM:     return _T("程序段");
    case ESEG_RESOURCE:    return _T("程序资源段");
    case ESEG_LOSSY:       return _T("可丢失程序段");
    case ESEG_EDITINFO:    return _T("编辑信息段");
    case ESEG_END:         return _T("End");
    case ESEG_INITMODULE:  return _T("初始模块段");
    case ESEG_EDITINFO2:   return _T("编辑信息段2");
    case ESEG_AUXINFO1:    return _T("辅助信息段1");
    case ESEG_AUXINFO2:    return _T("辅助信息段2");
    case ESEG_MODREC:      return _T("易模块记录段");
    case ESEG_EPKGINFO1:   return _T("易包信息段1");
    case ESEG_EDITFILTER:  return _T("编辑过滤段");
    case ESEG_COMPILECOND: return _T("编译条件信息段");
    case ESEG_AUXINFO3:    return _T("辅助信息段3");
    case ESEG_MODREC2:     return _T("易模块记录段2");
    }
    return _T("(未知段)");
}

// 用户信息段（`sub_4556F0` 读侧）：
//   10 个 CString（[u32 len][len 字节]）+ 5 个 u32 + 20 字节 + u32 + 4 字节
//   实测 flowcmd.e：size=138，解析消费 138/138（逐字节吻合）；
//   U00 = 程序名称（原版标题栏第一段）、U01 = 程序说明。
static void EcParseUserInfo(const unsigned char* pay, unsigned int size, std::vector<CString>& out)
{
    size_t q = 0;
    std::vector<std::string> ss;
    for (int i = 0; i < 10; i++)
    {
        if (q + 4 > size) { ss.clear(); break; }
        unsigned int sl = (unsigned int)pay[q]
                        | ((unsigned int)pay[q+1] << 8)
                        | ((unsigned int)pay[q+2] << 16)
                        | ((unsigned int)pay[q+3] << 24);
        q += 4;
        if (sl > size - q) { ss.clear(); break; }
        ss.push_back(std::string((const char*)pay + q, sl));
        q += sl;
    }
    // ⚠️ VC6 的 for-init 变量作用域是**外层**作用域（不是循环体），所以每个循环要用不同名字
    for (size_t k = 0; k < ss.size(); k++) out.push_back(EcToGbk(ss[k], 936));
}

// ECLONE_DUMP_EPROJ 给了路径时，把段表 + 用户信息段 + 事件绑定写到 <路径>.segs.txt（GBK）
static void EcDumpSegsIfAsked()
{
    TCHAR szDump[MAX_PATH * 2];
    if (::GetEnvironmentVariable(_T("ECLONE_DUMP_EPROJ"), szDump, MAX_PATH * 2) <= 0) return;
    CString sPath = szDump;
    sPath += _T(".segs.txt");
    CStdioFile f;
    CFileException e;
    if (!f.Open(sPath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary, &e)) return;
    CString line;
    line.Format(_T("idx key      name                 size   ck idx\r\n"));
    f.Write(line, (UINT)line.GetLength());
    for (size_t a = 0; a < g_vESegs.size(); a++)
    {
        const EcSegInfo& s = g_vESegs[a];
        line.Format(_T("%2u  %08X %-20s %-6u %s %s\r\n"),
                    s.idx, s.key, (LPCTSTR)s.name, s.size,
                    s.ckOk ? _T("ok ") : _T("BAD"), s.idxOk ? _T("ok") : _T("BAD"));
        f.Write(line, (UINT)line.GetLength());
    }
    line.Format(_T("userStrings=%u\r\n"), (unsigned)g_vEUserStrings.size());
    f.Write(line, (UINT)line.GetLength());
    for (size_t b = 0; b < g_vEUserStrings.size(); b++)
    {
        line.Format(_T("  U%02u %s\r\n"), (unsigned)b, (LPCTSTR)g_vEUserStrings[b]);
        f.Write(line, (UINT)line.GetLength());
    }
    line.Format(_T("eventBinds=%u\r\n"), (unsigned)g_vEEventBinds.size());
    f.Write(line, (UINT)line.GetLength());
    for (size_t c = 0; c < g_vEEventBinds.size(); c++)
    {
        const EcEventBind& b = g_vEEventBinds[c];
        line.Format(_T("  form=%08X comp=%08X slot=%u method=%08X\r\n"),
                    b.form, b.comp, b.slot, b.method);
        f.Write(line, (UINT)line.GetLength());
    }
    // 窗体表（程序资源段逐字段解析的结果；与 tools\e_resource.py 的输出应逐字段相同）
    line.Format(_T("forms=%u\r\n"), (unsigned)g_vEForms.size());
    f.Write(line, (UINT)line.GetLength());
    for (size_t fm = 0; fm < g_vEForms.size(); fm++)
    {
        const EcUnitForm& r = g_vEForms[fm];
        line.Format(_T("  FORM %s [%s] rect=(%d,%d,%dx%d) units=%u\r\n"),
                    (LPCTSTR)r.name, (LPCTSTR)r.comment,
                    r.left, r.top, r.width, r.height, (unsigned)r.units.size());
        f.Write(line, (UINT)line.GetLength());
        for (size_t un = 0; un < r.units.size(); un++)
        {
            const EcUnitBlob& u = r.units[un];
            line.Format(_T("    id=%08X dt=%08X %-24s (%d,%d,%dx%d) parent=%08X blob=%u\r\n"),
                        u.id, u.dataType, (LPCTSTR)u.name,
                        u.left, u.top, u.width, u.height, u.parent, (unsigned)u.blob.size());
            f.Write(line, (UINT)line.GetLength());
        }
    }
    f.Close();
}

// ---- 给上层用的取值口 ----
LPCTSTR EcProjectName()
{
    return g_vEUserStrings.size() > 0 ? (LPCTSTR)g_vEUserStrings[0] : _T("");
}

LPCTSTR EcProjectDesc()
{
    return g_vEUserStrings.size() > 1 ? (LPCTSTR)g_vEUserStrings[1] : _T("");
}

// 原版标题栏的**前缀**（实测 tools\win_tree.py 抓 e.exe 主窗口）：
//   `流程控制类命令演示 - C:\…\流程类命令.e [无编译条件] - Windows窗口程序 - [程序集: … / …]`
//   ⇒ 前缀 = `<程序名称> - <完整路径> [无编译条件] - Windows窗口程序`，后面的 `[子窗口标题]`
//     由 MFC/主框架拼（本工程在 EcUpdateFrameTitle 里拼）。
//   `[无编译条件]` 的原版依据：编译条件信息段（0x0F007319）不存在时才有这一段；
//   flowcmd.e 没有该段，实测标题里就是 `[无编译条件]`。
LPCTSTR EcProjectTitlePrefix()
{
    if (g_vEUserStrings.empty() || g_vEUserStrings[0].IsEmpty() || g_sEProjPath.IsEmpty())
        return _T("");
    static CString s;                       // 静态：调用方直接 SetWindowText
    CString sCond = _T(" [无编译条件]");
    for (size_t z = 0; z < g_vESegs.size(); z++)
        if (g_vESegs[z].key == ESEG_COMPILECOND) sCond = _T("");   // 有编译条件段就不写这四个字
    s.Format(_T("%s - %s%s - Windows窗口程序"),
             (LPCTSTR)g_vEUserStrings[0], (LPCTSTR)g_sEProjPath, (LPCTSTR)sCond);
    return s;
}

int EcProjectEventBindCount()
{
    return (int)g_vEEventBinds.size();
}

BOOL EcProjectEventBindAt(int i, unsigned int* pForm, unsigned int* pComp,
                          unsigned int* pSlot, unsigned int* pMethod)
{
    if (i < 0 || i >= (int)g_vEEventBinds.size()) return FALSE;
    const EcEventBind& b = g_vEEventBinds[i];
    if (pForm)   *pForm   = b.form;
    if (pComp)   *pComp   = b.comp;
    if (pSlot)   *pSlot   = b.slot;
    if (pMethod) *pMethod = b.method;
    return TRUE;
}

// ⚠️ 这个定义必须放在**文件作用域**（即匿名 namespace 之外）：
//    放在匿名 namespace 里会与外层头文件里的 extern 声明构成「同名两实体」，
//    VC6 在调用点直接报 C2872 ambiguous symbol（试过换成函数返回也一样报 C2668）。
const std::vector<EcUnitForm>* g_pEcForms = &g_vEForms;

BOOL EcLoadEProject(LPCTSTR pszPath, CString& strOut)
{
    strOut.Empty();
    g_sEProjPath = pszPath;          // 原版标题栏前缀里要带完整路径
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
    std::string resPayloadAll;           // Resource 段体内含窗口/组件数据（见 EcParseResourceComponentNames）
    // ⚠️ 自定义常量表在**程序资源段**（第 42 轮实证，见 EcParseUserConsts 的说明）。
    //    程序段里没有它 —— 在程序段尾部读只会拿到 "dwNumberOfBytesWritten" 垃圾。
    std::map<unsigned int, std::string> userConstNames;
    {
        size_t pos = 8;
        unsigned int nExpectIdx = 1;     // e.exe：段序从 1 起，逐段比对段体 +36
        g_vESegs.clear();
        g_vEEventBinds.clear();
        g_vEUserStrings.clear();
        while (pos + 100 <= len)
        {
            unsigned int magic = (unsigned int)(unsigned char)data[pos]
                               | ((unsigned int)(unsigned char)data[pos+1] << 8)
                               | ((unsigned int)(unsigned char)data[pos+2] << 16)
                               | ((unsigned int)(unsigned char)data[pos+3] << 24);
            // `sub_45B9B0`：magic 不对就是硬失败（原版走清理路径返回 0）
            if (magic != 0x15117319u)
            {
                CString s; s.Format(_T("seg magic @%u"), (unsigned)pos);
                EcDbgFail(_T("seg_magic"), s);
                return FALSE;
            }

            unsigned int segKey = (unsigned int)(unsigned char)data[pos+8]
                                | ((unsigned int)(unsigned char)data[pos+9] << 8)
                                | ((unsigned int)(unsigned char)data[pos+10] << 16)
                                | ((unsigned int)(unsigned char)data[pos+11] << 24);
            unsigned int size = (unsigned int)(unsigned char)data[pos+56]
                              | ((unsigned int)(unsigned char)data[pos+57] << 8)
                              | ((unsigned int)(unsigned char)data[pos+58] << 16)
                              | ((unsigned int)(unsigned char)data[pos+59] << 24);

            // ---- 段头校验（照 `sub_45B9B0`）：checksum = lane-XOR(92 字节段体)，index 递增 ----
            EcSegInfo si;
            si.idx   = nExpectIdx;
            si.key   = segKey;
            si.size  = size;
            si.off   = pos + 100;
            si.ckOk  = (EcLaneXor((const unsigned char*)data.data() + pos + 8, 92)
                        == EcRd32At(data, pos + 4));
            si.idxOk = (EcRd32At(data, pos + 8 + 36) == nExpectIdx);
            si.name  = EcSegNameDecode((const unsigned char*)data.data() + pos + 8, segKey);
            if (si.name.IsEmpty()) si.name = EcSegKeyName(segKey);
            g_vESegs.push_back(si);
            if (!si.ckOk || !si.idxOk)
            {
                CString s;
                s.Format(_T("seg idx=%u key=%08X size=%u ck=%s idx=%s"),
                         si.idx, segKey, size,
                         si.ckOk ? _T("ok") : _T("BAD"), si.idxOk ? _T("ok") : _T("BAD"));
                EcDbgFail(_T("seg_header"), s);
                return FALSE;
            }
            if (pos + 100 + (size_t)size > len)
            {
                CString s; s.Format(_T("seg @%u size=%u overruns"), (unsigned)pos, size);
                EcDbgFail(_T("seg_overrun"), s);
                return FALSE;
            }
            const unsigned char* pay = (const unsigned char*)data.data() + pos + 100;

            if (segKey == ESEG_USERINFO)
                EcParseUserInfo(pay, size, g_vEUserStrings);

            if (segKey == ESEG_AUXINFO1)
            {
                // 辅助信息段1：16 字节/条 [form][组件ID][事件槽][子程序ID]
                // （实测 flowcmd.e：1 条 = 52010001 / 16010003 / 0 / 04010007 = _按钮1_被单击）
                for (size_t q = 0; q + 16 <= (size_t)size; q += 16)
                {
                    EcEventBind b;
                    b.form   = EcRd32Raw(pay + q     );
                    b.comp   = EcRd32Raw(pay + q +  4);
                    b.slot   = EcRd32Raw(pay + q +  8);
                    b.method = EcRd32Raw(pay + q + 12);
                    g_vEEventBinds.push_back(b);
                }
            }

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
            if (segKey == ESEG_RESOURCE && size > 16)
            {
                // 就地解析：资源段常量表 -> id -> 名字（用于渲染 `#常量名`）
                std::string resPayload;
                resPayload.assign(data, pos + 100, size);
                resPayloadAll = resPayload;
                std::vector<EcEpConst> ucon;
                if (EcParseUserConsts(resPayload, ucon))
                {
                    size_t ui;
                    for (ui = 0; ui < ucon.size(); ui++)
                        if (!ucon[ui].name.empty())
                            userConstNames[ucon[ui].id] = ucon[ui].name;
                }
            }

            pos += 100 + size;
            nExpectIdx++;
            if (segKey == ESEG_END) break;
        }
        // 资源段 -> 窗体表/组件表（设计器用）。放在循环之后：这时语言码（cp）已经确定。
        g_vEForms.clear();
        if (!resPayloadAll.empty())
            EcParseResource(resPayloadAll, cp, g_vEForms);
        EcDumpSegsIfAsked();
    }
    if (progPayload.empty()) return FALSE;

    dwT1 = ::GetTickCount();
    EcTimeMark2A("1_segwalk", dwT0);

    // ---- 2. 解程序段 ----
    EcProgram prog;
    if (!EcParseProgram(progPayload, cp, prog)) return FALSE;

    // 自定义常量名（来自资源段）：并进 prog.consts。
    // 判据： `EcParseConsts` 在程序段尾部拿到的 count 恒为 0（读到的是段尾垃圾），
    // 真正的表在资源段 —— 用 userConstNames 覆盖，渲染 `#名字` 就有名字了。
    if (!userConstNames.empty())
    {
        prog.consts.clear();
        std::map<unsigned int, std::string>::const_iterator it;
        for (it = userConstNames.begin(); it != userConstNames.end(); ++it)
        {
            EcEpConst c;
            c.id      = it->first;
            c.flags   = 0;
            c.kind    = 0;
            c.numVal  = 0.0;
            c.boolVal = false;
            c.binLen  = 0;
            c.name    = it->second;
            prog.consts.push_back(c);
        }
    }

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
    // 诊断（ECLONE_DUMP_CONSTS=<path>）：把常量表的 (id, name) 全倒出来。
    // 用途：判定渲染器输出的 `#常量<id>` 到底在不在表里 —— 在表里说明查表没命中（键口径错），
    // 不在表里说明这个 id 根本不是"用户常量"（可能是系统常量/别的元素种类）。
    {
        TCHAR szC[MAX_PATH];
        if (::GetEnvironmentVariable(_T("ECLONE_DUMP_CONSTS"), szC, MAX_PATH) > 0)
        {
            FILE* fC = _tfopen(szC, _T("w"));
            if (fC != NULL)
            {
                fprintf(fC, "count=%d\n", (int)prog.consts.size());
                size_t ck;
                for (ck = 0; ck < prog.consts.size(); ck++)
                    fprintf(fC, "%08X\t%s\n", (unsigned)prog.consts[ck].id,
                            (LPCSTR)EcToGbk(prog.consts[ck].name, cp));
                fclose(fC);
            }
        }
    }
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

    // 组件（窗体上的控件）也是同一张表的元素：从 Resource 段取 (id, 名字)。
    // 少了这一块，正文里 `画板1.滚动写行 (…)` 就会退化成 `滚动写行 (var_16010002, …)`。
    {
        std::vector<CString> compSyms;
        EcParseResourceComponentNames(resPayloadAll, compSyms, cp);
        // 再加一遍**逐字段解析**出来的组件名/窗体名（新解析器：几何 + blob 都拿得到，
        // 名字也更可信）。两条来源都进表，谁有名字用谁 —— 保证不因换解析器丢名字。
        {
            const std::vector<EcUnitForm>* pf = g_pEcForms;
            for (size_t fI = 0; pf != NULL && fI < pf->size(); fI++)
            {
                const EcUnitForm& fm = (*pf)[fI];
                for (size_t uI = 0; uI < fm.units.size(); uI++)
                {
                    const EcUnitBlob& ub = fm.units[uI];
                    if (ub.name.IsEmpty()) continue;
                    CString s;
                    s.Format(_T("%08X|%s"), ub.id, (LPCTSTR)ub.name);
                    compSyms.push_back(s);
                }
            }
        }
        for (size_t symR = 0; symR < compSyms.size(); symR++)
            symTable.push_back(compSyms[symR]);
    }

    // 诊断（ECLONE_DUMP_SYMS=<path>）：把**最终**符号表整张倒出来（id|名字）。
    // 用途：判定正文里的 `var_%08X` 到底是"不在表里"还是"键口径不对"——
    //   把这里 dump 出来的 id 与正文里出现的 id 直接比即可，不用从截图反推。
    {
        TCHAR szS[MAX_PATH];
        if (::GetEnvironmentVariable(_T("ECLONE_DUMP_SYMS"), szS, MAX_PATH) > 0)
        {
            FILE* fS = _tfopen(szS, _T("w"));
            if (fS != NULL)
            {
                fprintf(fS, "count=%d\n", (int)symTable.size());
                for (size_t sk = 0; sk < symTable.size(); sk++)
                    fprintf(fS, "%s\n", (LPCSTR)symTable[sk]);   // 表项本来就是 GBK
                // 分作用域列出 id：判定"某个元素 id 到底是没解析到，还是标签口径不同"
                fprintf(fS, "-- asms (%d) --\n", (int)prog.asms.size());
                for (size_t daj = 0; daj < prog.asms.size(); daj++)
                {
                    fprintf(fS, "asm %08X %s vars=%d\n", (unsigned)prog.asms[daj].id,
                            (LPCSTR)EcToGbk(prog.asms[daj].name, cp),
                            (int)prog.asms[daj].vars.size());
                    for (size_t dvj = 0; dvj < prog.asms[daj].vars.size(); dvj++)
                        fprintf(fS, "   var %08X %s\n", (unsigned)prog.asms[daj].vars[dvj].id,
                                (LPCSTR)EcToGbk(prog.asms[daj].vars[dvj].name, cp));
                }
                fprintf(fS, "-- methods (%d) --\n", (int)prog.methods.size());
                for (size_t dmj = 0; dmj < prog.methods.size(); dmj++)
                {
                    fprintf(fS, "method %08X %s params=%d locals=%d\n",
                            (unsigned)prog.methods[dmj].id,
                            (LPCSTR)EcToGbk(prog.methods[dmj].name, cp),
                            (int)prog.methods[dmj].params.size(),
                            (int)prog.methods[dmj].locals.size());
                    for (size_t dpj = 0; dpj < prog.methods[dmj].params.size(); dpj++)
                        fprintf(fS, "   param %08X %s\n",
                                (unsigned)prog.methods[dmj].params[dpj].id,
                                (LPCSTR)EcToGbk(prog.methods[dmj].params[dpj].name, cp));
                    for (size_t dlj = 0; dlj < prog.methods[dmj].locals.size(); dlj++)
                        fprintf(fS, "   local %08X %s\n",
                                (unsigned)prog.methods[dmj].locals[dlj].id,
                                (LPCSTR)EcToGbk(prog.methods[dmj].locals[dlj].name, cp));
                }
                fclose(fS);
            }
        }
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
            EcLine(out, EcTrimTailFields(line));

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
                // ⚠️ 不要按「剪贴板文本里的空行」往正文里插空行：那是文本形式，
                //    原版**屏幕**上并没有这些空行（实测 01_top 2.41% → 5.43%，已回滚）。
                //    判据：tools/cmp_shots.py 的逐像素数字。
                // ⚠️ A/B 试验 A 已做并回滚：在独立注释行前补空行，像素**无变化**
                //    （说明空行被排版压掉）；配合 ec_editor 建行才生效，但那会恶化到 5.81%。
                for (size_t bi = 0; bi < body.size(); bi++)
                    EcLinePref(out, "    ", body[bi]);
                // ⚠️ 原版正文里**没有**这类提示行（判据：原版自己 Ctrl+A/Ctrl+C 的
                //    剪贴板文本，见 tools/text_oracle.py 与 shots/cmp/orig_sel.txt）。
                //    所以默认不写进正文，只在 ECLONE_SHOW_DECODE_DIAG 时输出。
                if (!strErr.IsEmpty() && EcShowDecodeDiag())
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

// ec_eexpr.cpp -- `.e` 子程序正文（expression 字节码）解码 + 渲染。
// 规格：D:\EPL\_ida\SEGMENT_SPEC.md §3.6（321 子程序验证过）。判据一律以 e.exe 为准。
//
// 本文件是 `D:\EPL\_ida\dec3.py` + `render.py` 的 C++ 移植。token 语义逐条对照 dec3.py。
//
// 关键结论（都实测）：
//   - 1 字节 token + 操作数 的语句流
//   - 0x6A 语句头 / 0x21,0x22 表达式调用：都是 16B 头
//       [u32 methodId][i16 spaceId][i16 flag][mfc code][mfc comment]
//     然后参数流：0x36 <expr...> 0x01   或   0x37 <varExpr> <expr...> 0x01
//   - 块首（0x6B/0x6C/0x6D/0x70）与**块尾/分支标记**（0x50..0x55, 0x6E, 0x71, 0x74）
//     也都带 16B 头（0x6F 默认分支例外，没有头）
//   - 变量链：0x38 <u32 id> 然后 0x37 终结 / 0x39 <u32 成员><u32 结构> / 0x3A <expr>
//   - 0x38 且 id == 0x0500FFFE 是 thiscall 特殊形态：吃掉 0x3A 再解一个表达式
//   - line_offset = 每个显示行行首在 expression 里的绝对偏移
#include "ec_eexpr.h"
#include "ec_lib.h"

namespace {

// ---------------------------------------------------------------- 字节游标
struct ECur
{
    const unsigned char* p;
    size_t n, i;
    long   step;                 // 全局步数（防死循环的最后一道防线）
    bool   bad;
    ECur(const unsigned char* d, size_t len) : p(d), n(len), i(0), step(0), bad(false) {}
    bool Ok(size_t k)
    {
        if (bad) return false;
        if (++step > 200000) { bad = true; return false; }   // 任何循环都会因 bad 退出
        if (i + k > n) { bad = true; return false; }
        return true;
    }
    unsigned char u8() { return Ok(1) ? p[i++] : 0; }
    unsigned short u16() { if (!Ok(2)) return 0; unsigned short v = (unsigned short)(p[i] | (p[i + 1] << 8)); i += 2; return v; }
    short i16() { return (short)u16(); }
    unsigned int u32()
    {
        if (!Ok(4)) return 0;
        unsigned int v = (unsigned int)p[i] | ((unsigned int)p[i + 1] << 8)
                       | ((unsigned int)p[i + 2] << 16) | ((unsigned int)p[i + 3] << 24);
        i += 4; return v;
    }
    int i32() { return (int)u32(); }
    double dbl() { unsigned char d[8]; for (int k = 0; k < 8; k++) d[k] = u8();
                   double v = 0.0; ::memcpy(&v, d, 8); return v; }
    std::string mfc()
    {
        unsigned int len = u32();
        if (bad || len == 0) return std::string();
        if (len > (n - i)) { bad = true; return std::string(); }
        std::string s((const char*)p + i, len);
        i += len;
        return s;
    }
    void skip(size_t k) { if (Ok(k)) i += k; }
};

// ---------------------------------------------------------------- 名字表
CString EcOpName(unsigned int mid)
{
    switch (mid)
    {
    case 15: return _T("*");      case 16: return _T("/");
    case 17: return _T("\\");     case 18: return _T("%");
    case 19: return _T("+");      case 20: return _T("-");
    case 38: return _T("=");      case 39: return _T("≠");
    case 40: return _T("<");      case 41: return _T(">");
    case 42: return _T("≤");      case 43: return _T("≥");
    case 45: return _T("且");     case 46: return _T("或");
    case 47: return _T("取反");   case 48: return _T("位取反");
    case 49: return _T("位与");   case 50: return _T("位或");
    case 51: return _T("位异或"); case 54: return _T("&");
    default: return CString();
    }
}

CString EcLoopName(unsigned int mid)
{
    switch (mid)
    {
    case 3:  return _T("判断循环首"); case 4:  return _T("判断循环尾");
    case 5:  return _T("循环判断首"); case 6:  return _T("循环判断尾");
    case 7:  return _T("计次循环首"); case 8:  return _T("计次循环尾");
    case 9:  return _T("变量循环首"); case 10: return _T("变量循环尾");
    default: return CString();
    }
}

// ---------------------------------------------------------------- 渲染器
struct ERender
{
    ECur  s;
    int   libIdx;
    std::vector<CString> subs;
    CString err;
    // 每条语句的起始偏移（与 .e 里的 line_offset 表交叉自检用）
    std::vector<unsigned int> stmtStarts;

    ERender(const unsigned char* p, size_t n, int lib, const std::vector<CString>& subNames)
        : s(p, n), libIdx(lib), subs(subNames) {}

    CString Num(double d)
    {
        CString t;
        if (d == (double)(__int64)d && d > -1e15 && d < 1e15) t.Format(_T("%I64d"), (__int64)d);
        else t.Format(_T("%.15g"), d);
        return t;
    }
    CString Text(const std::string& raw)
    {
        if (raw.empty()) return CString();
        int wn = ::MultiByteToWideChar(936, 0, raw.data(), (int)raw.size(), NULL, 0);
        if (wn <= 0) return CString();
        std::vector<wchar_t> w(wn + 1, 0);
        ::MultiByteToWideChar(936, 0, raw.data(), (int)raw.size(), &w[0], wn);
        return CString(&w[0]);
    }


    // err 是诊断串，绝不允许无限增长（否则一次解析能把内存吃爆）
    void Err(const CString& x)
    {
        if (err.GetLength() < 200) err += x;
    }

    // 轨迹（诊断用）：每次"进入某个 token"时记一条 "tok@偏移"
    std::vector<CString> trace;
    void T(unsigned char tok, size_t at)
    {
        if (trace.size() > 40000) return;
        CString x; x.Format(_T("%02X@%u"), tok, (unsigned)at);
        trace.push_back(x);
    }

    CString Expr(unsigned char tok, int d);
    CString VarExpr(int d);
    CString VarExprBody(int d);
    CString FuncCall(int d, bool bStatement, CString* pComment);
    CString EndHeader(int d, LPCTSTR pszFallback);
    CString ArgsOf(int d);

    void Statement(int d, std::vector<CString>& out, bool* pIsEnd, unsigned char* pEndTok);
    void IfStmt(int d, unsigned char t, std::vector<CString>& out);
    void LoopStmt(int d, std::vector<CString>& out);
    void SwitchStmt(int d, std::vector<CString>& out);

    void Run(std::vector<CString>& out)
    {
        for (int guard = 0; guard < 20000 && !s.bad && s.i < s.n; guard++)
        {
            if (out.size() > 20000) { Err(CString(_T("[输出超限]"))); break; }
            size_t before = s.i;
            bool isEnd = false; unsigned char et = 0;
            Statement(0, out, &isEnd, &et);
            if (s.i == before && !s.bad) s.i++;
            if (isEnd) break;
        }
    }
};

CString ERender::ArgsOf(int d)
{
    CString joined;
    unsigned char t = s.u8();
    std::vector<CString> args;
    if (t == 0x36)
    {
        for (;;)
        {
            if (s.bad || s.i >= s.n) break;
            unsigned char t2 = s.u8();
            if (t2 == 0x01 || t2 == 0xFF) break;
            args.push_back(Expr(t2, d + 1));
        }
    }
    else if (t == 0x38)
    {
        args.push_back(VarExprBody(d));        // 语句级 thiscall：参数流直接以 0x38 开头
        for (;;)
        {
            if (s.bad || s.i >= s.n) break;
            unsigned char t2 = s.u8();
            if (t2 == 0x01 || t2 == 0xFF) break;
            args.push_back(Expr(t2, d + 1));
        }
    }
    else
    {
        CString x; x.Format(_T("?argtok%02X"), t);
        Err(x);
    }
    for (size_t k = 0; k < args.size(); k++)
    {
        if (args[k].IsEmpty()) continue;
        if (!joined.IsEmpty()) joined += _T(", ");
        joined += args[k];
    }
    return joined;
}

CString ERender::Expr(unsigned char tok, int d)
{
    T(tok, s.i - 1);
    switch (tok)
    {
    // dec3.py: tok==22 (0x16) EMPTY_ARGUMENTS -> "空"，并且**跳过一个字节**
    case 0x16: { s.u8(); return CString(_T("空")); }
    case 0x3B: { int v = s.i32(); CString t; t.Format(_T("%d"), v); return t; }
    case 0x17: return Num(s.dbl());
    case 0x18: return CString(s.i16() != 0 ? _T("真") : _T("假"));
    case 0x19: { CString t = _T("日期("); t += Num(s.dbl()); t += _T(")"); return t; }
    case 0x1A: { CString t = _T("\""); t += Text(s.mfc()); t += _T("\""); return t; }
    case 0x1B: { int v = s.i32(); CString t; t.Format(_T("#常量%d"), v); return t; }
    case 0x1C: { short a = s.i16(); short b = s.i16(); CString t;
                 t.Format(_T("#库常量[%d,%d]"), a - 1, b - 1); return t; }
    case 0x23: { short a = s.i16(); short b = s.i16(); unsigned int c = s.u32(); CString t;
                 t.Format(_T("#枚举[%d,%d,%u]"), a, b, c); return t; }
    case 0x1E: { int v = s.i32(); CString t; t.Format(_T("&子程序%d"), v); return t; }
    case 0x1F:
    {
        CString r = _T("{");
        bool first = true;
        for (;;)
        {
            if (s.bad || s.i >= s.n) break;
            unsigned char t2 = s.u8();
            if (t2 == 0x20) break;
            if (!first) r += _T(", ");
            r += Expr(t2, d + 1);
            first = false;
        }
        r += _T("}");
        return r;
    }
    case 0x1D:
    {
        // 0x1D 后面必定是 0x38
        unsigned char t38 = s.u8();
        if (t38 != 0x38) { CString x; x.Format(_T("?1D%02X"), t38); Err(x); return x; }
        return VarExprBody(d);
    }
    case 0x38: return VarExprBody(d);      // token 已读，直接解 id
    // dec3.py: tok==55 (0x37) -> 返回空串（它只是终止符，不是变量表达式）
    case 0x37: return CString();
    case 0x21:
    case 0x34: { CString cmt; return FuncCall(d, false, &cmt); }
    default: break;
    }
    {
        CString x; x.Format(_T("?tok%02X"), tok);
        Err(x);
        return x;
    }
}

CString ERender::VarExpr(int d)
{
    unsigned char t = s.u8();
    if (t != 0x38) { CString x; x.Format(_T("?var%02X"), t); Err(x); return x; }
    return VarExprBody(d);
}

// 0x38 已读掉，从 id 开始解（Expr 的 0x38 分支直接进这里，不能再回退）
CString ERender::VarExprBody(int d)
{
    unsigned int vid = s.u32();
    if (vid == 0x0500FFFEu)
    {
        unsigned char t38 = s.u8();
        if (t38 != 0x3A) { CString x; x.Format(_T("?self%02X"), t38); Err(x); return x; }
        return CString(_T("<")) + Expr(s.u8(), d + 1) + _T(">");
    }
    CString name;
    name.Format(_T("var_%08X"), vid);
    for (int guard = 0; guard < 64; guard++)
    {
        if (s.bad || s.i >= s.n) break;
        unsigned char nt = s.u8();
        if (nt == 0x37) break;
        if (nt == 0x39) { unsigned int mem = s.u32(); s.u32();
                          CString x; x.Format(_T(".成员%u"), mem); name += x; continue; }
        if (nt == 0x3A) { name += _T("[") + Expr(s.u8(), d + 1) + _T("]"); continue; }
        CString x; x.Format(_T("?chain%02X"), nt);
        Err(x);
        break;
    }
    return name;
}

CString ERender::FuncCall(int d, bool bStatement, CString* pComment)
{
    size_t hdr = s.i;
    unsigned int mid = s.u32();
    short sp = s.i16();
    short fl = s.i16();
    if (trace.size() < 40000)
    {
        CString x; x.Format(_T("HDR@%u mid=%u sp=%d fl=%d"), (unsigned)hdr, mid, (int)sp, (int)fl);
        trace.push_back(x);
    }
    std::string code = s.mfc();
    std::string cmt  = s.mfc();
    (void)fl;
    CString comment = Text(cmt);
    if (pComment) *pComment = comment;

    CString argStr = ArgsOf(d);
    if (trace.size() < 40000)
    {
        CString x; x.Format(_T("  END@%u (args=%s)"), (unsigned)s.i, (LPCTSTR)argStr);
        trace.push_back(x);
    }

    CString name;
    if (sp == -2)
    {
        for (size_t k = 0; k < subs.size(); k++)
        {
            int bar = subs[k].Find(_T('|'));
            if (bar <= 0) continue;
            unsigned int v = (unsigned int)_tcstoul(subs[k].Left(bar), NULL, 16);
            if (v == mid) { name = subs[k].Mid(bar + 1); break; }
        }
        if (name.IsEmpty()) name.Format(_T("用户子程序_%08X"), mid);
    }
    else if (sp == -3) name.Format(_T("DLL_%08X"), mid);
    else if (sp == -1) name.Empty();
    else
    {
        // 真命令名：spaceId == 0 是核心库 krnln，>0 是 `prog.libs[]` 的下标。
        // 依据（子智能体实测）：methodId 就是 LIB_INFO.m_pBeginCmdInfo 的下标，
        // 688/688 条全静态可解；Bubble Sort 的 18 个下标 18/18 语义吻合。
        // 规格与证据：D:\EPL\_ida\fne_cmds.txt、_verify_samples.txt、_verify_space.txt
        int libOfSpace = libIdx;
        if (sp > 0) libOfSpace = sp - 1 + (libIdx >= 0 ? libIdx : 0);
        CString real;
        if (libOfSpace >= 0)
        {
            const EcCmd* pc = EcCmdAt(libOfSpace, (int)mid);
            if (pc != NULL) real = pc->name;
        }
        // 运算符排版优先用本地表（EcOpName 与真表一致，且它管 "+ - *" 这类显示）
        CString op = (sp == 0) ? EcOpName(mid) : CString();
        if (!op.IsEmpty()) name = op;
        else if (!real.IsEmpty()) name = real;
        else
        {
            CString loop = (mid <= 10) ? EcLoopName(mid) : CString();
            if (!loop.IsEmpty()) name = loop;
            else name.Format(_T("命令<%d,%u>"), sp, mid);
        }
    }

    if (bStatement && name.IsEmpty())
        return CString();                       // spaceId == -1 的语句：注释/空行

    CString out = name;
    if (!bStatement && sp == 0 && mid != 52 && !argStr.IsEmpty())
    {
        // 运算符表达式：把参数串原样包一层（参数已按 ", " 拼）
        CString op = EcOpName(mid);
        static const TCHAR* kInf[] = { _T("="), _T("≠"), _T("<"), _T(">"), _T("≤"), _T("≥"),
                                       _T("+"), _T("-"), _T("*"), _T("/"), _T("\\"), _T("%"),
                                       _T("且"), _T("或"), _T("&"), NULL };
        for (int k = 0; kInf[k] != NULL; k++)
            if (op == kInf[k]) return CString(_T("(")) + argStr + _T(")");
    }
    if (argStr.IsEmpty()) out += _T(" ()");
    else { out += _T(" ("); out += argStr; out += _T(")"); }
    if (bStatement && !comment.IsEmpty()) out += _T("   ' ") + comment;
    if (bStatement && !code.empty()) { CString x; x.Format(_T("   [code %dB]"), (int)code.size()); out += x; }
    return out;
}

CString ERender::EndHeader(int d, LPCTSTR pszFallback)
{
    CString cmt;
    CString n = FuncCall(d, false, &cmt);
    return n.IsEmpty() ? CString(pszFallback) : n;
}

void ERender::Statement(int d, std::vector<CString>& out, bool* pIsEnd, unsigned char* pEndTok)
{
    *pIsEnd = false;
    if (s.bad || s.i >= s.n) return;
    if (out.size() > 20000) { Err(CString(_T("[输出超限]"))); s.bad = true; return; }

    unsigned char t = s.u8();
    *pEndTok = t;
    T(t, s.i - 1);
    if (stmtStarts.size() < 20000) stmtStarts.push_back((unsigned int)(s.i - 1));

    // 连续空语句（0x01）跳过：用循环而不是递归，避免深栈 + 死循环
    while (t == 0x01)
    {
        if (s.bad || s.i >= s.n) return;
        size_t at2 = s.i;
        t = s.u8();
        *pEndTok = t;
        T(t, at2);
        if (stmtStarts.size() < 20000) stmtStarts.push_back((unsigned int)at2);
    }
    if ((t >= 0x50 && t <= 0x55) || t == 0xFF || t == 0x71 || t == 0x74) { *pIsEnd = true; return; }

    switch (t)
    {
    case 0x6A:
    {
        size_t at = s.i - 1;
        CString cmt;
        CString c = FuncCall(d, true, &cmt);
        if (!c.IsEmpty()) { CString p; p.Format(_T("[%04u] "), (unsigned)at); out.push_back(p + c); }
        return;
    }
    case 0x6B:
    case 0x6C: IfStmt(d, t, out); return;
    case 0x6D: SwitchStmt(d, out); return;
    case 0x70: LoopStmt(d, out); return;
    default:
    {
        CString x; x.Format(_T("?stmt%02X@%u"), t, (unsigned)(s.i - 1));
        Err(x);
        return;
    }
    }
}

void ERender::IfStmt(int d, unsigned char t, std::vector<CString>& out)
{
    CString cmt;
    FuncCall(d, false, &cmt);                       // 块首 16B 头（名字即"如果"/"如果真"）
    unsigned char ct = s.u8();
    CString cond;
    if (ct == 0x36)
    {
        unsigned char t2 = s.u8();
        if (t2 == 0x01) cond = CString();
        else { cond = Expr(t2, d + 1); s.u8(); }
    }
    else { CString x; x.Format(_T("?cond%02X"), ct); Err(x); }

    bool bIfTrue = (t == 0x6C);
    CString kw = bIfTrue ? _T("如果真") : _T("如果");
    out.push_back(kw + (cond.IsEmpty() ? CString(_T(" ()")) : (_T(" (") + cond + _T(")"))));

    if (bIfTrue)
    {
        for (int g = 0; g < 5000 && !s.bad && s.i < s.n; g++)
        {
            size_t before = s.i;
            bool isEnd = false; unsigned char et = 0;
            Statement(d + 1, out, &isEnd, &et);
            if (isEnd) { if (et == 0x52 || et == 0x50) EndHeader(d, _T("")); break; }
            if (s.i == before) { s.bad = true; break; }
        }
        out.push_back(_T("如果真结束 ()"));
        return;
    }

    for (int g = 0; g < 5000 && !s.bad && s.i < s.n; g++)
    {
        size_t before = s.i;
        bool isEnd = false; unsigned char et = 0;
        Statement(d + 1, out, &isEnd, &et);
        if (!isEnd) { if (s.i == before) { s.bad = true; break; } continue; }
        if (et == 0x51) { out.push_back(_T("如果结束 ()")); return; }
        if (et != 0x50) break;
        EndHeader(d, _T(""));                        // 0x50 = 否则（带头）
        out.push_back(_T("否则"));
        for (int g2 = 0; g2 < 5000 && !s.bad && s.i < s.n; g2++)
        {
            size_t before = s.i;
            bool isEnd2 = false; unsigned char et2 = 0;
            Statement(d + 1, out, &isEnd2, &et2);
            if (isEnd2) break;
            if (s.i == before) { s.bad = true; break; }
        }
        out.push_back(_T("如果结束 ()"));
        return;
    }
    out.push_back(_T("如果结束 ()"));
}

void ERender::LoopStmt(int d, std::vector<CString>& out)
{
    CString cmt;
    CString head = FuncCall(d, false, &cmt);
    CString args;
    unsigned char tt = s.u8();
    if (tt == 0x36)
    {
        std::vector<CString> a;
        for (;;)
        {
            if (s.bad || s.i >= s.n) break;
            unsigned char t3 = s.u8();
            if (t3 == 0x01) break;
            a.push_back(Expr(t3, d + 1));
        }
        for (size_t k = 0; k < a.size(); k++)
        { if (a[k].IsEmpty()) continue; if (!args.IsEmpty()) args += _T(", "); args += a[k]; }
    }
    if (head.IsEmpty()) head = _T("循环首");
    out.push_back(head + (args.IsEmpty() ? CString(_T(" ()")) : (_T(" (") + args + _T(")"))));

    for (int g = 0; g < 5000 && !s.bad && s.i < s.n; g++)
    {
        size_t before = s.i;
        bool isEnd = false; unsigned char et = 0;
        Statement(d + 1, out, &isEnd, &et);
        if (isEnd) break;
        if (s.i == before) { s.bad = true; break; }
    }
    CString cmt2;
    CString tail = FuncCall(d, false, &cmt2);        // 0x71 + 头
    CString args2;
    unsigned char t2 = s.u8();
    if (t2 == 0x36)
    {
        std::vector<CString> a;
        for (;;)
        {
            if (s.bad || s.i >= s.n) break;
            unsigned char t3 = s.u8();
            if (t3 == 0x01) break;
            a.push_back(Expr(t3, d + 1));
        }
        for (size_t k = 0; k < a.size(); k++)
        { if (a[k].IsEmpty()) continue; if (!args2.IsEmpty()) args2 += _T(", "); args2 += a[k]; }
    }
    if (tail.IsEmpty()) tail = _T("循环尾");
    out.push_back(tail + (args2.IsEmpty() ? CString(_T(" ()")) : (_T(" (") + args2 + _T(")"))));
}

void ERender::SwitchStmt(int d, std::vector<CString>& out)
{
    unsigned char t = s.u8();
    if (t != 0x6E) { CString x; x.Format(_T("?switch%02X"), t); Err(x); return; }
    out.push_back(_T("判断 ()"));

    for (int branch = 0; branch < 200 && !s.bad && s.i < s.n; branch++)
    {
        CString cmt;
        FuncCall(d, false, &cmt);                    // 分支 16B 头
        unsigned char ct = s.u8();
        CString cond;
        if (ct == 0x36) { unsigned char t2 = s.u8(); if (t2 != 0x01) { cond = Expr(t2, d + 1); s.u8(); } }
        out.push_back(cond.IsEmpty() ? CString(_T("判断 ()")) : (_T("判断 (") + cond + _T(")")));

        for (int g2 = 0; g2 < 5000 && !s.bad && s.i < s.n; g2++)
        {
            size_t before = s.i;
            bool isEnd = false; unsigned char et = 0;
            Statement(d + 1, out, &isEnd, &et);
            if (!isEnd) { if (s.i == before) { s.bad = true; break; } continue; }
            if (et == 0x6E) break;                   // 下一分支
            if (et == 0x6F)                          // 默认分支（无头）
            {
                out.push_back(_T("默认"));
                for (int g3 = 0; g3 < 5000 && !s.bad && s.i < s.n; g3++)
                {
                    bool isEnd3 = false; unsigned char et3 = 0;
                    Statement(d + 1, out, &isEnd3, &et3);
                    if (!isEnd3) continue;
                    if (et3 == 0x54) continue;
                    if (et3 == 0x74) { out.push_back(_T("判断结束 ()")); return; }
                    break;
                }
                out.push_back(_T("判断结束 ()"));
                return;
            }
            if (et == 0x54) continue;
            if (et == 0x74) { out.push_back(_T("判断结束 ()")); return; }
            break;
        }
    }
    out.push_back(_T("判断结束 ()"));
}

} // namespace

std::vector<CString> EcDecodeExpression(const unsigned char* pExpr, size_t nExpr,
                                       const unsigned char* pLineOff, size_t nLineOff,
                                       int libIdx, CString& strErr)
{
    std::vector<CString> out;
    strErr.Empty();
    (void)pLineOff; (void)nLineOff;
    if (pExpr == NULL || nExpr == 0) return out;

    std::vector<CString> noSubs;
    ERender r(pExpr, nExpr, libIdx, noSubs);
    r.Run(out);

    // ---- 交叉自检：语句起始偏移必须落在 line_offset 表里 ----
    // 依据：line_offset 是 **e.exe 写文件时留下的行边界**（不是任何开源实现的产物），
    // 所以"每条语句的起点都出现在 line_offset 里"是独立于 eplc 的硬断言。
    if (pLineOff != NULL && nLineOff >= 4)
    {
        size_t nLo = nLineOff / 4;
        std::vector<unsigned int> lo(nLo);
        for (size_t k = 0; k < nLo; k++)
            lo[k] = (unsigned int)pLineOff[k*4] | ((unsigned int)pLineOff[k*4+1] << 8)
                  | ((unsigned int)pLineOff[k*4+2] << 16) | ((unsigned int)pLineOff[k*4+3] << 24);
        size_t j = 0;
        int nBad = 0;
        CString detail;
        for (size_t m = 0; m < r.stmtStarts.size(); m++)
        {
            unsigned int at = r.stmtStarts[m];
            while (j < lo.size() && lo[j] < at) j++;
            if (j < lo.size() && lo[j] == at) continue;
            if (nBad < 6)
            {
                CString one;
                one.Format(_T(" #%d:@%u(期望%u)"), (int)k, at,
                           (j < lo.size()) ? lo[j] : 0xFFFFFFFFu);
                detail += one;
            }
            nBad++;
        }
        if (nBad > 0)
        {
            CString one;
            one.Format(_T("line_offset 不符 %d/%d:%s"),
                       nBad, (int)r.stmtStarts.size(), (LPCTSTR)detail);
            strErr = strErr.IsEmpty() ? one : (strErr + _T(" | ") + one);
        }
        else
        {
            CString one;
            one.Format(_T("line_offset 全部命中 (%d/%d)"),
                       (int)r.stmtStarts.size(), (int)r.stmtStarts.size());
            strErr = strErr.IsEmpty() ? one : (strErr + _T(" | ") + one);
        }
    }
    // 诊断：ECLONE_TRACE_EXPR 给了路径就把 token 轨迹落盘（GBK）
    {
        TCHAR szTrace[MAX_PATH * 2];
        if (::GetEnvironmentVariable(_T("ECLONE_TRACE_EXPR"), szTrace, MAX_PATH * 2) > 0)
        {
            FILE* f = _tfopen(szTrace, _T("ab"));
            if (f)
            {
                for (size_t k = 0; k < r.trace.size(); k++)
                    _ftprintf(f, _T("%s\n"), (LPCTSTR)r.trace[k]);
                fclose(f);
            }
        }
    }
    if (r.s.bad) strErr = _T("字节码解码中断（游标越界）");
    if (!r.err.IsEmpty()) strErr = _T("未识别 token: ") + r.err;
    return out;
}

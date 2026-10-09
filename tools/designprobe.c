/* designprobe.c -- 验证「窗口设计器能不能用支持库的 ITF 接口建真组件窗口」。
 *
 * 依据（全部来自 krnln.fne 的伪 C 逆向，见 work\...\krnln\window_designer_notes.md）：
 *   LIB_DATA_TYPE_INFO(+56 字节/项) 的 +44 = m_pfnGetInterface
 *   pfnGetInterface(1) = PFN_CREATE_UNIT
 *   PFN_CREATE_UNIT(pAllData, nAllDataSize, dwStyle, hParentWnd, uID, hMenu,
 *                   x, y, cx, cy, dwWinFormID, dwUnitID, hDesignWnd, blInDesignMode)
 *   新建组件时 pAllData=NULL / nAllDataSize=0 合法（krnln 的 sub_100311F0 直接返回 1）
 *   原版 IDE 就是这么建的（e.exe sub_50D520 传 (0,0,0,父窗,id,0,x,y,cx,cy,form,unit,设计窗,TRUE)）
 *
 * Build (VC6): tools\build_designprobe.bat   ->  tools\designprobe.exe
 * Usage:       designprobe.exe [krnln路径] [最多试几个组件]
 * 退出码 0 = 全部断言通过。
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    DWORD   format;
    char*   guid;
    int     maj, min, build;
    int     rqSysMaj, rqSysMin, rqKrnlMaj, rqKrnlMin;
    char*   name;
    int     language;
    char*   explain;
    DWORD   state;
    char*   author; char* zip; char* addr; char* photo; char* fax; char* email;
    char*   home; char* other;
    int     nDataType;
    void*   pDataType;
    int     nCategory;
    char*   szzCategory;
    int     nCmd;
    void*   pCmd;
    void*   pCmdsFunc;
    void*   pfnAddIn;
    char*   szzAddIn;
    void*   pfnNotify;
    void*   pfnSuper;
    char*   szzSuper;
    int     nConst;
    void*   pConst;
    char*   szzDepend;
} LIB_INFO;

typedef LIB_INFO* (WINAPI *PFN_GET)(void);
typedef void* (WINAPI *PFN_GET_INTERFACE)(int);
typedef INT (WINAPI *PFN_NOTIFY_LIB)(INT, DWORD, DWORD);

/* HUNIT 就是组件对象指针（krnln 里是 MFC CWnd 派生类），HWND 在 +28 */
#define UNIT_HWND(u) (*(HWND*)((char*)(u) + 28))

/* lib2.h 的 DATA_TYPE 常量 */
#define SDT_MASK 0x80000000UL
#define DT_IS_ARY 0x20000000UL
#define LDT_WIN_UNIT 0x40UL

static LIB_INFO* g_lib;
static int       g_call[4096];      /* 收到的宿主通知码计数 */
static int       g_unknown = 0;
static const char* g_verStr = "5.7";

/* 宿主通知函数（= 原版 IDE 交给库的 PFN_NOTIFY_SYS） */
static INT WINAPI HostNotify(INT msg, DWORD p1, DWORD p2)
{
    if (msg >= 0 && msg < 4096) g_call[msg]++;
    switch (msg) {
    case 2002: return (INT)(INT_PTR)"";                       /* 命令行 */
    case 2003: return (INT)(INT_PTR)".\\";                    /* EXE 目录 */
    case 2004: return (INT)(INT_PTR)"designprobe.exe";        /* EXE 名 */
    case 2008: return 0;                                      /* EVENT_NOTIFY：丢弃 */
    case 2018: return 0;                                      /* DO_EVENTS */
    case 2022: return 0;                                      /* 取组件数据类型 */
    case 2024: return (INT)(INT_PTR)malloc(p1);               /* MALLOC */
    case 2025: free((void*)p1); return 0;                     /* MFREE */
    case 2026: return (INT)(INT_PTR)realloc((void*)p1, p2);   /* MREALLOC */
    case 2027: printf("      [notify] RUNTIME_ERR: %s\n", p1 ? (char*)p1 : ""); return 0;
    case 2028: printf("      [notify] EXIT_PROGRAM(%lu) - 忽略\n", (unsigned long)p1); return 0;
    case 1004: return 1;                                      /* 语言：中文(大陆) */
    case 1005: return 0x00050007;                             /* 版本 5.7 */
    case 1006:                                                /* 目录 */
        if (p2) { lstrcpynA((char*)p2, ".\\", 260); return 1; }
        return 0;
    case 1014: return 1;                                      /* 是否易窗口：是 */
    default: g_unknown++; return 0;
    }
}

static const char* s(LPSTR p) { return (p && *p) ? p : ""; }

static LRESULT CALLBACK HostProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    return DefWindowProcA(h, m, w, l);
}

/* 建一个普通父窗口当"设计画布" */
static HWND MakeHost(void)
{
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = HostProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "DesignProbeHost";
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        printf("  RegisterClassA failed err=%lu\n", GetLastError());
        return NULL;
    }
    return CreateWindowExA(0, "DesignProbeHost", "design canvas",
                           WS_OVERLAPPEDWINDOW, 40, 40, 640, 480, NULL, NULL,
                           GetModuleHandle(NULL), NULL);
}

int main(int argc, char** argv)
{
    HMODULE h;
    PFN_GET f;
    LIB_INFO* p;
    unsigned char* dt;
    int i, nTried = 0, nOk = 0;
    const char* path = (argc > 1) ? argv[1] : "C:\\Program Files (x86)\\e\\lib\\krnln.fne";
    int maxTry = (argc > 2) ? atoi(argv[2]) : 6;
    HWND host;

    printf("=== designprobe：用支持库 ITF 建真组件窗口 ===\n");
    setvbuf(stdout, NULL, _IONBF, 0);      /* 崩溃时也要看到最后一行打印到哪了 */
    h = LoadLibraryA(path);
    if (!h) { printf("[FAIL] LoadLibrary(%s) err=%lu\n", path, GetLastError()); return 1; }
    f = (PFN_GET)GetProcAddress(h, "GetNewInf");
    if (!f) { printf("[FAIL] GetProcAddress(GetNewInf)\n"); return 2; }
    p = f();
    if (!p) { printf("[FAIL] GetNewInf -> NULL\n"); return 3; }
    g_lib = p;

    printf("[1] 库 %s  %d.%d.%d  dataTypes=%d\n", s(p->name), p->maj, p->min, p->build, p->nDataType);

    /* ★ 关键一步：把自己注册成"系统通知函数"（原版 IDE 也是先做这一步） */
    ((PFN_NOTIFY_LIB)p->pfnNotify)(1, (DWORD)(INT_PTR)HostNotify, 0);
    printf("[2] 已注册宿主通知函数 pfnNotify(1,%p)\n", HostNotify);

    host = MakeHost();
    if (!host) { printf("[FAIL] 建父窗口失败\n"); return 4; }
    printf("[3] 设计画布 HWND=%p class=DesignProbeHost\n", host);

    dt = (unsigned char*)p->pDataType;
    printf("[4] 数据类型表（name/state/pfnGetInterface/nProp/nEvent/cmds）:\n");
    for (i = 0; i < p->nDataType; i++) {
        unsigned char* r = dt + i * 56;
        char* nm = *(char**)(r + 0);
        DWORD st = *(DWORD*)(r + 20);
        void* gi = *(void**)(r + 44);
        if (nm && (st & LDT_WIN_UNIT)) {
            printf("    [%2d] %-16s state=%08lX nProp=%-3d nEvent=%-3d nCmd=%-3d pfnGetInterface=%s\n",
                   i, nm, (unsigned long)st, *(int*)(r + 36), *(int*)(r + 28),
                   *(int*)(r + 12), gi ? "有" : "无");
        }
    }

    printf("[5] 逐个试建组件（ITF 1 = CREATE_UNIT，数据传 NULL/0，设计模式 TRUE）:\n");
    for (i = 0; i < p->nDataType && nTried < maxTry; i++) {
        unsigned char* r = dt + i * 56;
        char* nm = *(char**)(r + 0);
        DWORD st = *(DWORD*)(r + 20);
        PFN_GET_INTERFACE gi;
        void* cu;
        DWORD unit;
        HWND hw;
        char cls[64] = "";
        RECT rc;

        if (!nm || !(st & LDT_WIN_UNIT)) continue;
        if (lstrcmpA(nm, "窗口") == 0) continue;      /* 窗口本体单独在后面试 */
        gi = *(PFN_GET_INTERFACE*)(r + 44);
        if (!gi) { printf("    [%2d] %-16s 没有接口，跳过\n", i, nm); continue; }
        cu = gi(1);
        if (!cu) { printf("    [%2d] %-16s ITF 1 返回空，跳过\n", i, nm); continue; }
        nTried++;
        printf("    [%2d] %-16s ITF1=%p -> ", i, nm, cu);
        fflush(stdout);
        unit = ((DWORD (WINAPI*)(LPBYTE, INT, DWORD, HWND, UINT, HMENU, INT, INT, INT, INT,
                                 DWORD, DWORD, HWND, BOOL))cu)
               (NULL, 0, 0, host, 100 + i, NULL, 10, 10 + i * 30, 100, 60, 1, 100 + i, host, TRUE);
        hw = unit ? UNIT_HWND(unit) : NULL;
        if (unit && hw && IsWindow(hw)) {
            GetClassNameA(hw, cls, sizeof(cls));
            GetWindowRect(hw, &rc);
            printf("OK unit=%p hwnd=%p class=%-14s style=%08lX ex=%08lX rect=%d,%d %dx%d\n",
                   (void*)unit, hw, cls,
                   (unsigned long)GetWindowLongA(hw, GWL_STYLE),
                   (unsigned long)GetWindowLongA(hw, GWL_EXSTYLE),
                   rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
            nOk++;
        } else {
            printf("失败 unit=%p hwnd=%p\n", (void*)unit, hw);
        }
        fflush(stdout);
    }

    printf("[6] 试建窗口本体（类型 0，父窗口传 NULL）:\n");
    {
        unsigned char* r = dt + 0 * 56;
        PFN_GET_INTERFACE gi = *(PFN_GET_INTERFACE*)(r + 44);
        void* cu = gi ? gi(1) : NULL;
        printf("    type0 name=%s state=%08lX ITF1=%p\n", s(*(char**)r), (unsigned long)*(DWORD*)(r + 20), cu);
        if (cu) {
            DWORD unit = ((DWORD (WINAPI*)(LPBYTE, INT, DWORD, HWND, UINT, HMENU, INT, INT, INT, INT,
                                           DWORD, DWORD, HWND, BOOL))cu)
                         (NULL, 0, 0, host, 900, NULL, 60, 60, 380, 250, 1, 900, host, TRUE);
            HWND hw = unit ? UNIT_HWND(unit) : NULL;
            char cls[64] = "";
            if (hw && IsWindow(hw)) { GetClassNameA(hw, cls, sizeof(cls));
                printf("    以父窗口=%p 建：OK unit=%p hwnd=%p class=%s style=%08lX\n",
                       host, (void*)unit, hw, cls,
                       (unsigned long)GetWindowLongA(hw, GWL_STYLE)); }
            else printf("    以父窗口=%p 建：失败 unit=%p hwnd=%p\n", host, (void*)unit, hw);
        }
    }

    printf("[6b] 同一个组件、设计模式 FALSE 对比（看 design 模式是否改样式）:\n");
    {
        int i2;
        for (i2 = 0; i2 < p->nDataType; i2++) {
            unsigned char* r = dt + i2 * 56;
            char* nm = *(char**)(r + 0);
            PFN_GET_INTERFACE gi;
            void* cu;
            DWORD unit;
            HWND hw;
            if (!nm || !(*(DWORD*)(r + 20) & LDT_WIN_UNIT)) continue;
            if (lstrcmpA(nm, "标签") != 0) continue;
            gi = *(PFN_GET_INTERFACE*)(r + 44);
            cu = gi ? gi(1) : NULL;
            if (!cu) break;
            unit = ((DWORD (WINAPI*)(LPBYTE, INT, DWORD, HWND, UINT, HMENU, INT, INT, INT, INT,
                                     DWORD, DWORD, HWND, BOOL))cu)
                   (NULL, 0, 0, host, 950, NULL, 300, 300, 80, 24, 1, 950, host, FALSE);
            hw = unit ? UNIT_HWND(unit) : NULL;
            if (hw && IsWindow(hw))
                printf("    design=FALSE hwnd=%p style=%08lX (WS_DISABLED=%d)\n", hw,
                       (unsigned long)GetWindowLongA(hw, GWL_STYLE),
                       (GetWindowLongA(hw, GWL_STYLE) & WS_DISABLED) ? 1 : 0);
            break;
        }
    }

    printf("[7] 库反调宿主的通知码统计：\n");
    for (i = 0; i < 4096; i++)
        if (g_call[i]) printf("    nMsg=%-5d 次数=%d\n", i, g_call[i]);
    printf("    未处理的通知码次数=%d\n", g_unknown);

    /* ---- ITF 6 读属性 / ITF 4 写属性：属性表的 ABI 验证 ---- */
    {
        int pi = -1, pu = 0;
        for (i = 0; i < p->nDataType; i++) {
            unsigned char* r = dt + i * 56;
            char* nm = *(char**)(r + 0);
            if (!nm || !(*(DWORD*)(r + 20) & LDT_WIN_UNIT)) continue;
            if (lstrcmpA(nm, "标签") == 0) { pi = i; break; }
        }
        if (pi >= 0) {
            unsigned char* r = dt + pi * 56;
            int nprop = *(int*)(r + 36);
            unsigned char* props = *(unsigned char**)(r + 40);
            PFN_GET_INTERFACE gi = *(PFN_GET_INTERFACE*)(r + 44);
            void* cu = gi ? gi(1) : NULL;
            DWORD unit = 0;
            if (cu)
                unit = ((DWORD (WINAPI*)(LPBYTE, INT, DWORD, HWND, UINT, HMENU, INT, INT, INT, INT,
                                         DWORD, DWORD, HWND, BOOL))cu)
                       (NULL, 0, 0, host, 700, NULL, 20, 20, 120, 24, 1, 700, host, TRUE);
            if (unit) {
                void* fget = gi(6);
                void* fset = gi(4);
                int titleIdx = -1, k;
                printf("[9] 属性读（ITF 6）：%s 共 %d 个属性，前 12 个：\n", s(*(char**)r), nprop);
                for (k = 0; k < nprop; k++) {
                    unsigned char* pr = props + k * 20;      /* UNIT_PROPERTY = 20 字节 */
                    short ty = *(short*)(pr + 12);
                    WORD st = *(WORD*)(pr + 14);
                    char val[64] = "";
                    union { int i; double d; DWORD c; LPCTSTR sz; BYTE raw[8]; } v;
                    int bShow = (k < 12);
                    memset(&v, 0, sizeof(v));
                    if (fget) {
                        if (ty == 1001 || ty == 1006 || ty == 1000) {
                            if (((BOOL (WINAPI*)(DWORD,int,void*))fget)(unit, k, &v))
                                sprintf(val, "%d", v.i);
                        } else if (ty == 1002 || ty == 1004) {
                            if (((BOOL (WINAPI*)(DWORD,int,void*))fget)(unit, k, &v))
                                sprintf(val, "%g", v.d);
                        } else if (ty == 1003) {
                            if (((BOOL (WINAPI*)(DWORD,int,void*))fget)(unit, k, &v))
                                sprintf(val, "%s", v.i ? "true" : "false");
                        } else if (ty == 1014 || ty == 1015 || ty == 1017) {
                            if (((BOOL (WINAPI*)(DWORD,int,void*))fget)(unit, k, &v))
                                sprintf(val, "#%02X%02X%02X",
                                        v.c & 0xFF, (v.c >> 8) & 0xFF, (v.c >> 16) & 0xFF);
                        } else if (ty == 1005 || ty == 1007 || ty == 1008 || ty == 1016) {
                            if (((BOOL (WINAPI*)(DWORD,int,void*))fget)(unit, k, &v)) {
                                /* ⚠️ 库可能返回它自己的内部指针（也可能返回垃圾）；先 IsBadStringPtr */
                                if (v.sz == NULL)
                                    sprintf(val, "(null)");
                                else if (IsBadStringPtrA(v.sz, 2048))
                                    sprintf(val, "<bad ptr %p>", v.sz);
                                else
                                    sprintf(val, "%.60s", v.sz);
                            }
                        } else {
                            sprintf(val, "<type %d>", ty);
                        }
                    }
                    if (bShow) {
                        /* 属性名按 GBK 字节十六进制打出来（控制台中文会乱码，脚本里解码）*/
                        char hex[80 * 3 + 1];
                        int hn = 0, hb;
                        const unsigned char* np = (const unsigned char*)*(char**)pr;
                        hex[0] = 0;
                        for (hb = 0; np && np[hb] && hb < 20; hb++)
                            hn += sprintf(hex + hn, "%02X", np[hb]);
                        printf("    [%2d] type=%-5d state=%04X name=%s val=%s\n",
                               k, ty, st, hex, val);
                    }
                    if (lstrcmpA(s(*(char**)pr), "标题") == 0 || lstrcmpA(s(*(char**)pr), "文本") == 0)
                        if (titleIdx < 0) titleIdx = k;
                }
                printf("[10] 属性写（ITF 4）：把\"标题\"设成 probe-title 再读回来\n");
                if (fset && titleIdx >= 0 && fget) {
                    /* ⚠️ NOTIFY_PROPERTY_CHANGED 的返回值是"需不需要重建窗口"，**不是**成功/失败
                       （实测 krnln 的标签类每个 case 都 return 0）。成功与否只能靠回读。 */
                    union { int i; double d; DWORD c; LPCTSTR sz; BYTE raw[8]; } v;
                    char want[] = "probe-title";
                    char back[64] = "";
                    int needRecreate;
                    memset(&v, 0, sizeof(v));
                    v.sz = want;
                    needRecreate = ((BOOL (WINAPI*)(DWORD,int,void*,void*))fset)(unit, titleIdx, &v, NULL);
                    memset(&v, 0, sizeof(v));
                    if (((BOOL (WINAPI*)(DWORD,int,void*))fget)(unit, titleIdx, &v) &&
                        v.sz != NULL && !IsBadStringPtrA(v.sz, 2048))
                        lstrcpynA(back, v.sz, sizeof(back));
                    printf("    needRecreate=%d, read back = \"%s\"  -> %s\n", needRecreate, back,
                           lstrcmpA(back, want) == 0 ? "PASS" : "FAIL");
                    if (lstrcmpA(back, want) != 0) nOk = -1;      /* 标记失败 */
                } else {
                    printf("    该类型没有\"标题\"属性或没有 ITF 4/6，跳过\n");
                }
            } else {
                printf("[9] 建标签失败，跳过属性测试\n");
            }
        } else {
            printf("[9] 没找到\"标签\"类型，跳过属性测试\n");
        }
    }

    DestroyWindow(host);
    printf("[11] 结果：试建 %d 个，成功 %d 个  -> %s\n", nTried, nOk,
           (nOk > 0) ? "PASS" : "FAIL");
    return (nOk > 0) ? 0 : 5;
}

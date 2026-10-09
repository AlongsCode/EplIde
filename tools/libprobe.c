/* libprobe.c -- LoadLibrary an E-language support library (.fne) and call GetNewInf,
 * printing the LIB_INFO that the original IDE would see.  32-bit only.
 *
 * Build (VC6): cl /nologo /MT /O2 libprobe.c user32.lib
 * Usage:       libprobe.exe <full path to .fne> [maxDt] [maxCmd] [maxConst]
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

static const char* s(LPSTR p) { return (p && *p) ? p : ""; }

/* DATA_TYPE -> 中文名（系统类型表见 lib2.h） */
static const char* dtname(DWORD v)
{
    static char buf[64];
    int a = (v >> 16) & 0xFF, b = (v >> 24) & 0xFF;
    if (v == 0) return "";
    if (v & 0x20000000) return dtname(v & ~0x20000000);
    if (!(v & 0x80000000)) { sprintf(buf, "库类型[%u]", v); return buf; }
    if (b == 0 && a == 0) return "通用型";
    if (b == 1) { const char* n[] = {0, "字节", "短整数", "整数", "长整数", "小数", "双精度小数"};
                  return (a >= 1 && a <= 6) ? n[a] : "数字型"; }
    if (b == 2) return "逻辑型";
    if (b == 3) return "日期时间型";
    if (b == 4) return "文本型";
    if (b == 5) return "字节集";
    if (b == 6) return "子程序指针";
    if (b == 8) return "子语句型";
    sprintf(buf, "系统类型(%08X)", v);
    return buf;
}

int main(int argc, char** argv)
{
    HMODULE h; PFN_GET f; LIB_INFO* p;
    int i, maxDt, maxCmd, maxConst;
    char* q;

    if (argc < 2) { printf("usage: libprobe.exe <fne path> [maxDt] [maxCmd] [maxConst]\n"); return 1; }
    maxDt = argc > 2 ? atoi(argv[2]) : 6;
    maxCmd = argc > 3 ? atoi(argv[3]) : 6;
    maxConst = argc > 4 ? atoi(argv[4]) : 6;

    h = LoadLibraryA(argv[1]);
    if (!h) { printf("LoadLibrary failed, err=%lu\n", GetLastError()); return 1; }
    f = (PFN_GET)GetProcAddress(h, "GetNewInf");
    if (!f) { printf("GetProcAddress(GetNewInf) failed, err=%lu\n", GetLastError()); return 2; }
    p = f();
    if (!p) { printf("GetNewInf returned NULL\n"); return 3; }

    printf("LIB_INFO at %p\n", p);
    printf("format   = %lu\n", (unsigned long)p->format);
    printf("guid     = %s\n", s(p->guid));
    printf("version  = %d.%d.%d   rq: e %d.%d / krnln %d.%d\n", p->maj, p->min, p->build,
           p->rqSysMaj, p->rqSysMin, p->rqKrnlMaj, p->rqKrnlMin);
    printf("name     = %s\n", s(p->name));
    printf("language = %d\n", p->language);
    printf("explain  = %.160s\n", s(p->explain));
    printf("state    = 0x%08lX\n", (unsigned long)p->state);
    printf("counts   = dataTypes=%d categories=%d commands=%d consts=%d\n",
           p->nDataType, p->nCategory, p->nCmd, p->nConst);
    printf("author   = %s | %s | %s | %s | %s | %s | %s | %s\n",
           s(p->author), s(p->zip), s(p->addr), s(p->photo), s(p->fax), s(p->email),
           s(p->home), s(p->other));

    printf("\n-- categories --\n");
    q = p->szzCategory;
    for (i = 0; i < p->nCategory && q; i++) {
        printf("  [%2d] %s\n", i + 1, q);
        q += lstrlenA(q) + 1;
    }

    printf("\n-- data types (first %d of %d) --\n", maxDt, p->nDataType);
    {
        unsigned char* d = (unsigned char*)p->pDataType;
        for (i = 0; i < p->nDataType && i < maxDt; i++) {
            unsigned char* r = d + i * 56;
            char* nm = *(char**)(r + 0);
            int ncmd = *(int*)(r + 12);
            DWORD st = *(DWORD*)(r + 20);
            int nev = *(int*)(r + 28), npr = *(int*)(r + 36), nel = *(int*)(r + 48);
            printf("  [%2d] name=%-14s eg=%-14s ncmd=%-3d state=0x%08lX ev=%d prop=%d elem=%d\n",
                   i, s(nm), s(*(char**)(r + 4)), ncmd, (unsigned long)st, nev, npr, nel);
        }
    }

    printf("\n-- commands (first %d of %d) --\n", maxCmd, p->nCmd);
    {
        unsigned char* c = (unsigned char*)p->pCmd;
        for (i = 0; i < p->nCmd && i < maxCmd; i++) {
            unsigned char* r = c + i * 36;
            short cat = *(short*)(r + 12);
            DWORD ret = *(DWORD*)(r + 16);
            int lvl = *(short*)(r + 22);
            int na = *(int*)(r + 28);
            void* pa = *(void**)(r + 32);
            printf("  [%3d] %-16s cat=%-3d ret=%-10s lvl=%d nargs=%d\n",
                   i, s(*(char**)(r + 0)), cat, dtname(ret), lvl, na);
            if (pa && i < 3) {
                int k;
                for (k = 0; k < na; k++) {
                    unsigned char* a = (unsigned char*)pa + k * 24;
                    printf("        arg%d %-10s type=%-10s default=%d state=%08lX\n", k,
                           s(*(char**)(a + 0)), dtname(*(DWORD*)(a + 12)),
                           *(int*)(a + 16), (unsigned long)*(DWORD*)(a + 20));
                }
            }
        }
    }

    printf("\n-- consts (first %d of %d) --\n", maxConst, p->nConst);
    {
        unsigned char* c = (unsigned char*)p->pConst;
        for (i = 0; i < p->nConst && i < maxConst; i++) {
            unsigned char* r = c + i * 32;
            printf("  [%3d] %-12s type=%d text=%.40s value=%g\n", i,
                   s(*(char**)(r + 0)), *(short*)(r + 14), s(*(char**)(r + 16)),
                   *(double*)(r + 24));
        }
    }

    FreeLibrary(h);
    return 0;
}

/* treeinfo.c -- dump the SysTreeView32 item trees of another (32-bit) process.
 *
 * Why a separate 32-bit helper: TVM_GETITEM is a WM_USER-range message whose lParam
 * pointer is NOT marshalled across processes, so the TVITEM struct has to live inside
 * the target process (OpenProcess + VirtualAllocEx + ReadProcessMemory). Same trick as
 * tbprobe.c.
 *
 * Build (VC6):  cl /nologo /MT /O2 treeinfo.c user32.lib kernel32.lib
 * Usage:
 *   treeinfo.exe <exe name>                       -> dump every tree (expanded, with text)
 *   treeinfo.exe <exe name> sel <tree> <item>     -> TVM_SELECTITEM item #<item> in tree #<tree>
 *   treeinfo.exe <exe name> click <tree> <item>   -> REAL mouse click on that item (the IDE
 *                                                    ignores TVM_SELECTITEM, it wants a click)
 *                                                    then print the frame's Edit/Static texts
 */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <tlhelp32.h>

/* commctrl.h already provides TVM_*, TVGN_*, TVIF_*, TVITEMW (x86 layout = 40 bytes) */

typedef struct { HWND h; } HIT;
static HIT   g_hit[64];
static int   g_n;
static DWORD g_pid;
static HANDLE g_proc;
static LPVOID g_remote;     /* TVITEMW + text buffer */

static void FindPid(const char* exe)
{
    HANDLE snap;
    PROCESSENTRY32 pe;
    /* a numeric argument is a pid directly (several e.exe instances may be running) */
    {
        int i, allDigits = exe[0] != 0;
        for (i = 0; exe[i]; i++) if (exe[i] < '0' || exe[i] > '9') { allDigits = 0; break; }
        if (allDigits) { g_pid = (DWORD)atoi(exe); return; }
    }
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) {
        do {
            if (lstrcmpiA(pe.szExeFile, exe) == 0) { g_pid = pe.th32ProcessID; break; }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
}

static BOOL CALLBACK EnumKids(HWND h, LPARAM l)
{
    char cls[64];
    GetClassNameA(h, cls, sizeof(cls));
    if (lstrcmpiA(cls, "SysTreeView32") == 0) {
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid == g_pid && g_n < 64) g_hit[g_n++].h = h;
    }
    return TRUE;
}

static BOOL CALLBACK EnumTops(HWND h, LPARAM l)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == g_pid) EnumChildWindows(h, EnumKids, 0);
    return TRUE;
}

/* write a TVITEM into the target process and fetch the item text (ANSI: e.exe is not
   manifested for comctl32 v6, and GBK bytes also print correctly on this console) */
static int GetItem(HWND tv, HTREEITEM it, TVITEMW* out, char* text)
{
    TVITEMW ti; SIZE_T rd;
    memset(&ti, 0, sizeof(ti));
    memset(text, 0, 512);
    ti.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_STATE | TVIF_HANDLE | TVIF_CHILDREN;
    ti.hItem = it;
    ti.stateMask = 0xFFFFFFFF;
    ti.pszText = (LPWSTR)((char*)g_remote + sizeof(TVITEMW) + 16);
    ti.cchTextMax = 512;
    if (!WriteProcessMemory(g_proc, g_remote, &ti, sizeof(ti), &rd)) return 0;
    SendMessageA(tv, TVM_GETITEMA, 0, (LPARAM)g_remote);
    if (!ReadProcessMemory(g_proc, g_remote, &ti, sizeof(ti), &rd)) return 0;
    if (text) {
        if (!ReadProcessMemory(g_proc, ti.pszText, text, 511, &rd)) text[0] = 0;
        text[511] = 0;
    }
    if (out) *out = ti;
    return 1;
}

static void Walk(HWND tv, HTREEITEM it, int depth, int* counter)
{
    while (it) {
        TVITEMW ti; char tx[512]; int i;
        tx[0] = 0;
        if (!GetItem(tv, it, &ti, tx)) { printf("  <GETITEM failed>\n"); return; }
        for (i = 0; i < depth; i++) printf("  ");
        printf("[%4d] img=%-3d sel=%-3d st=0x%04X ch=%-2d |%s|\n",
               (*counter)++, ti.iImage, ti.iSelectedImage,
               ti.state & 0xFFFF, ti.cChildren, tx);
        {
            HTREEITEM c = (HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)it);
            if (c) Walk(tv, c, depth + 1, counter);
        }
        it = (HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)it);
    }
}

/* post a REAL left click on an item. TVM_GETITEMRECT writes its RECT into the target process
   and still claims "not visible" for this tree, so the row is computed by walking the
   TVGN_FIRSTVISIBLE -> TVGN_NEXTVISIBLE chain instead; the resulting TVN_SELCHANGED is what
   the IDE reacts to (it ignores TVM_SELECTITEM). */
static void ClickItem(HWND tv, HTREEITEM it, int dbl, int real)
{
    HTREEITEM cur;
    char tx[512];
    int row = 0, ih;
    int x, y;
    if (!GetItem(tv, it, NULL, tx)) { printf("  <GETITEM failed>\n"); return; }
    SendMessageA(tv, TVM_ENSUREVISIBLE, 0, (LPARAM)it);
    Sleep(300);
    cur = (HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_FIRSTVISIBLE, 0);
    while (cur && cur != it) {
        cur = (HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_NEXTVISIBLE, (LPARAM)cur);
        row++;
    }
    if (!cur) {
        printf("  <item |%s| is not in the visible chain (row>%d)>\n", tx, row);
        return;
    }
    ih = (int)SendMessageA(tv, TVM_GETITEMHEIGHT, 0, 0);
    if (ih <= 0) ih = 16;
    x = 60;
    y = row * ih + ih / 2;
    printf("  click |%s| at client %d,%d (row %d of %d)", tx, x, y, row, ih);
    printf("\n");
    if (real) {
        POINT pt;
        HWND root = tv;
        while (GetParent(root)) root = GetParent(root);   /* VC6 headers have no GetAncestor */
        pt.x = x; pt.y = y;
        ClientToScreen(tv, &pt);
        SetForegroundWindow(root);
        Sleep(400);
        SetCursorPos(pt.x, pt.y);
        Sleep(200);
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Sleep(80);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        if (dbl) {
            Sleep(80);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Sleep(80);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        }
    } else {
        PostMessageA(tv, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        Sleep(120);
        PostMessageA(tv, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        if (dbl) {
            Sleep(120);
            PostMessageA(tv, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(x, y));
            Sleep(120);
            PostMessageA(tv, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        }
    }
    Sleep(250);
    {
        HTREEITEM care = (HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_CARET, 0);
        char ct[512];
        if (care && GetItem(tv, care, NULL, ct))
            printf("  caret is now |%s| (moved=%d)\n", ct, care == it);
        else
            printf("  caret unreadable\n");
    }
}

/* count items + expand everything (2 passes so grandchildren show) */
static void ExpandAll(HWND tv)
{
    HTREEITEM root;
    int pass;
    for (pass = 0; pass < 3; pass++) {
        /* walk manually: expand node then recurse (TVM_EXPAND is marshalled fine) */
        struct { HTREEITEM stack[4096]; int sp; } st;
        st.sp = 0;
        root = (HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_ROOT, 0);
        if (root) st.stack[st.sp++] = root;
        while (st.sp > 0) {
            HTREEITEM cur = st.stack[--st.sp];
            while (cur) {
                HTREEITEM c;
                SendMessageA(tv, TVM_EXPAND, TVE_EXPAND, (LPARAM)cur);
                c = (HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)cur);
                if (c && st.sp < 4090) st.stack[st.sp++] = c;
                cur = (HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)cur);
            }
        }
        Sleep(200);
    }
}

/* select the Nth item (same pre-order as Walk) */
static HTREEITEM SelectNth(HWND tv, int want, int* counter)
{
    HTREEITEM stack[8192];
    int sp = 0;
    HTREEITEM cur = (HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_ROOT, 0);
    while (cur || sp > 0) {
        if (!cur) { cur = stack[--sp]; continue; }
        if ((*counter)++ == want) return cur;
        {
            HTREEITEM nxt = (HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)cur);
            if (nxt && sp < 8190) stack[sp++] = nxt;
            cur = (HTREEITEM)SendMessageA(tv, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)cur);
        }
    }
    return 0;
}

/* print every Edit/Static text under the process' top-level windows */
static BOOL CALLBACK TextKids(HWND h, LPARAM l)
{
    char cls[64];
    char buf[65536];
    GetClassNameA(h, cls, sizeof(cls));
    {
        DWORD pid = 0;
        int n;
        GetWindowThreadProcessId(h, &pid);
        if (pid != g_pid) return TRUE;
        n = (int)SendMessageA(h, WM_GETTEXTLENGTH, 0, 0);
        printf("    [child] %-20s hwnd=%p len=%d\n", cls, h, n);
        if (n > 0 && n < 60000) {
            buf[0] = 0;
            SendMessageA(h, WM_GETTEXT, sizeof(buf) - 1, (LPARAM)buf);
            printf("      |%s|\n", buf);
        }
    }
    return TRUE;
}

static BOOL CALLBACK TextTops(HWND h, LPARAM l)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == g_pid) {
        TextKids(h, 0);
        EnumChildWindows(h, TextKids, 0);
    }
    return TRUE;
}

int main(int argc, char** argv)
{
    int i, counter = 0;
    const char* mode;
    if (argc < 2) { printf("usage: treeinfo.exe <exe> [sel|click <tree> <item>]\n"); return 1; }
    if (argc >= 5 && (lstrcmpiA(argv[2], "sel") == 0 || lstrcmpiA(argv[2], "click") == 0
                      || lstrcmpiA(argv[2], "dclick") == 0 || lstrcmpiA(argv[2], "mclick") == 0
                      || lstrcmpiA(argv[2], "mdclick") == 0))
        mode = argv[2];
    else
        mode = "dump";
    FindPid(argv[1]);
    if (!g_pid) { printf("process '%s' not running\n", argv[1]); return 2; }
    printf("process %s pid=%lu\n", argv[1], (unsigned long)g_pid);
    EnumWindows(EnumTops, 0);
    printf("trees found: %d\n", g_n);
    if (!g_n) return 3;
    g_proc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, g_pid);
    if (!g_proc) { printf("OpenProcess failed %lu\n", GetLastError()); return 4; }
    g_remote = VirtualAllocEx(g_proc, NULL, 8192, MEM_COMMIT, PAGE_READWRITE);
    if (!g_remote) { printf("VirtualAllocEx failed %lu\n", GetLastError()); return 5; }

    for (i = 0; i < g_n; i++) {
        RECT r; char txt[128]; int cnt;
        GetWindowRect(g_hit[i].h, &r);
        txt[0] = 0; GetWindowTextA(g_hit[i].h, txt, sizeof(txt));
        cnt = (int)SendMessageA(g_hit[i].h, TVM_GETCOUNT, 0, 0);
        printf("\n=== tree %d hwnd=%p \"%s\" %dx%d items=%d ===\n",
               i, g_hit[i].h, txt, r.right - r.left, r.bottom - r.top, cnt);
        if (lstrcmpiA(mode, "dump") != 0) {
            HTREEITEM t; int wantTree = atoi(argv[3]), want = atoi(argv[4]);
            if (i != wantTree) continue;
            counter = 0;
            ExpandAll(g_hit[i].h);
            t = SelectNth(g_hit[i].h, want, &counter);
            printf("%s item #%d -> %p\n", mode, want, t);
            if (t) {
                if (lstrcmpiA(mode, "dump") != 0 && lstrcmpiA(mode, "sel") != 0) {
                    ClickItem(g_hit[i].h, t,
                              (lstrcmpiA(mode, "dclick") == 0 || lstrcmpiA(mode, "mdclick") == 0),
                              (lstrcmpiA(mode, "mclick") == 0 || lstrcmpiA(mode, "mdclick") == 0));
                } else {
                    SendMessageA(g_hit[i].h, TVM_SELECTITEM, TVGN_CARET, (LPARAM)t);
                }
            }
        } else {
            ExpandAll(g_hit[i].h);
            counter = 0;
            Walk(g_hit[i].h, (HTREEITEM)SendMessageA(g_hit[i].h, TVM_GETNEXTITEM, TVGN_ROOT, 0), 0, &counter);
        }
    }
    if (lstrcmpiA(mode, "dump") != 0) {
        Sleep(800);
        printf("\n--- all Edit/Static texts in process ---\n");
        EnumWindows(TextTops, 0);
    }
    VirtualFreeEx(g_proc, g_remote, 0, MEM_RELEASE);
    CloseHandle(g_proc);
    return 0;
}

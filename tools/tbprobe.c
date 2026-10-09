/* tbprobe.c -- 读另一个进程里所有 ToolbarWindow32 的按钮表（32 位小程序）
 *
 * 为什么需要它：TB_GETBUTTON 属于 WM_USER 段消息，参数指针**不会被系统跨进程 marshal**，
 * 所以必须：OpenProcess + VirtualAllocEx 在目标进程里开一块内存，把该远端地址当 lParam 发过去，
 * 再 ReadProcessMemory 读回来（Spy++ 也是这么干的）。
 * ⚠️ 目标必须是 32 位进程（e.exe 是），本工具也必须编译成 32 位 —— TBBUTTON 的布局才对得上。
 *
 * 编译（VC6）：  cl /nologo /MT tbprobe.c user32.lib
 * 用法：         tbprobe.exe <进程名，如 e.exe>
 */
#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>

#define TB_BUTTONCOUNT   (WM_USER + 24)
#define TB_GETBUTTON     (WM_USER + 23)
#define TB_GETBUTTONSIZE (WM_USER + 58)
#define TB_GETBITMAPFLAGS (WM_USER + 41)
#define TBSTYLE_SEP      0x0001
#define TBSTATE_ENABLED  0x0004
#define TBSTATE_CHECKED  0x0001
#define TBSTATE_PRESSED  0x0002
#define TBSTATE_HIDDEN   0x0008
#define TBSTATE_INDETERMINATE 0x0010
#define TBSTATE_WRAP     0x0020

typedef struct {
    int   iBitmap;
    int   idCommand;
    BYTE  fsState;
    BYTE  fsStyle;
    BYTE  bReserved[2];
    DWORD dwData;
    INT_PTR iString;
} TB;

static HWND  g_hit[16];
static int   g_n;
static DWORD g_pid = 0;
static DWORD g_foundPid = 0;

static void FindPid(const char* exe)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) {
        do {
            if (lstrcmpiA(pe.szExeFile, exe) == 0) { g_foundPid = pe.th32ProcessID; break; }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
}

static BOOL CALLBACK EnumKids(HWND h, LPARAM l)
{
    char cls[64];
    GetClassNameA(h, cls, sizeof(cls));
    if (lstrcmpiA(cls, "ToolbarWindow32") == 0) {
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid == g_pid && g_n < 16) g_hit[g_n++] = h;
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

static void DumpToolbar(HWND h, HANDLE hp, LPVOID rm)
{
    char txt[128] = "", cls[64] = "";
    RECT r;
    int n, i;
    DWORD sz;
    GetWindowTextA(h, txt, sizeof(txt));
    GetClassNameA(h, cls, sizeof(cls));
    GetWindowRect(h, &r);
    n = (int)SendMessageA(h, TB_BUTTONCOUNT, 0, 0);
    sz = (DWORD)SendMessageA(h, TB_GETBUTTONSIZE, 0, 0);
    printf("\n[%s] \"%s\"  rect=%d,%d %dx%d  buttons=%d  buttonsize=%dx%d  bitmapflags=0x%X\n",
           cls, txt, r.left, r.top, r.right - r.left, r.bottom - r.top, n,
           (int)LOWORD(sz), (int)HIWORD(sz),
           (unsigned)SendMessageA(h, TB_GETBITMAPFLAGS, 0, 0));
    printf("  idx  iBitmap idCommand style     state\n");
    for (i = 0; i < n; i++) {
        TB tb; SIZE_T rd = 0;
        char st[64]; int k = 0;
        st[0] = 0;
        if (!SendMessageA(h, TB_GETBUTTON, i, (LPARAM)rm)) { printf("  %3d  <TB_GETBUTTON failed>\n", i); continue; }
        if (!ReadProcessMemory(hp, rm, &tb, sizeof(tb), &rd)) { printf("  %3d  <ReadProcessMemory failed %lu>\n", i, GetLastError()); continue; }
        if (tb.fsStyle & TBSTYLE_SEP) lstrcpyA(st, "SEP");
        else {
            if (tb.fsStyle & 0x04) lstrcatA(st, "BTN|");       /* TBSTYLE_BUTTON */
            if (tb.fsStyle & 0x08) lstrcatA(st, "CHECK|");     /* TBSTYLE_CHECK */
            if (tb.fsStyle & 0x02) lstrcatA(st, "DROPDOWN|");  /* TBSTYLE_DROPDOWN */
            if (tb.fsStyle == 0) lstrcpyA(st, "-");
        }
        printf("  %3d  %6d %8d  %-10s 0x%02X %s%s%s%s%s\n", i, tb.iBitmap, tb.idCommand, st, tb.fsState,
               (tb.fsState & TBSTATE_ENABLED) ? "EN " : "",
               (tb.fsState & TBSTATE_CHECKED) ? "CHK " : "",
               (tb.fsState & TBSTATE_PRESSED) ? "PRS " : "",
               (tb.fsState & TBSTATE_HIDDEN) ? "HID " : "",
               (tb.fsState & TBSTATE_INDETERMINATE) ? "IND " : "");
    }
}

int main(int argc, char** argv)
{
    HANDLE hp; LPVOID rm;
    int i;
    if (argc < 2) { printf("usage: tbprobe.exe <exe name>\n"); return 1; }
    FindPid(argv[1]);
    if (g_foundPid == 0) { printf("process '%s' not running\n", argv[1]); return 2; }
    g_pid = g_foundPid;
    printf("process %s pid=%lu\n", argv[1], (unsigned long)g_pid);
    EnumWindows(EnumTops, 0);
    printf("toolbars found: %d\n", g_n);
    if (g_n == 0) return 3;
    hp = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, g_pid);
    if (hp == NULL) { printf("OpenProcess failed %lu\n", GetLastError()); return 4; }
    rm = VirtualAllocEx(hp, NULL, 4096, MEM_COMMIT, PAGE_READWRITE);
    if (rm == NULL) { printf("VirtualAllocEx failed %lu\n", GetLastError()); return 5; }
    for (i = 0; i < g_n; i++) DumpToolbar(g_hit[i], hp, rm);
    VirtualFreeEx(hp, rm, 0, MEM_RELEASE);
    CloseHandle(hp);
    return 0;
}

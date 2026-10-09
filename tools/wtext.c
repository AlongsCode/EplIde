/* wtext.c -- read window texts of another (32-bit or 64-bit) process.
 *
 * WM_GETTEXT does not marshal its buffer across processes, so GetWindowText on a foreign
 * window silently returns "". That bit the project before (HANDOFF 3.9-16). This helper
 * does the classic OpenProcess + VirtualAllocEx + SendMessage(WM_GETTEXT) dance.
 *
 * Build (VC6): cl /nologo /MT /O2 /Fewtext.exe wtext.c user32.lib kernel32.lib
 * Usage:
 *   wtext.exe <pid> [class-filter]        -> every child window with a non-empty text
 *   wtext.exe <pid> @<hwnd-hex>           -> just that window (text length + text)
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DWORD  g_pid;
static HANDLE g_proc;
static LPVOID g_remote;
static const char* g_filt;

static int ReadText(HWND h, char* out, int cap)
{
    int n = (int)SendMessageA(h, WM_GETTEXTLENGTH, 0, 0);
    SIZE_T rd;
    if (n <= 0) { out[0] = 0; return 0; }
    if (n >= 4096) n = 4095;                    /* keep the remote buffer/format small */
    if (n >= cap - 1) n = cap - 2;
    printf("    ReadText %p len=%d\n", h, n);
    memset(g_remote, 0, 8);
    if (!WriteProcessMemory(g_proc, g_remote, "", 1, &rd)) { out[0] = 0; return 0; }
    SendMessageA(h, WM_GETTEXT, (WPARAM)(n + 1), (LPARAM)g_remote);
    if (!ReadProcessMemory(g_proc, g_remote, out, n + 1, &rd)) { out[0] = 0; return 0; }
    out[n] = 0;
    return n;
}

/* GBK console: write raw bytes so Chinese shows up in the console's code page */
static char g_tx[66000];    /* static: a 66KB local plus call recursion overflows the 1MB stack */

static void PrintKid(HWND h, int depth)
{
    char cls[128];
    int n;
    DWORD pid = 0;
    RECT r;
    GetWindowThreadProcessId(h, &pid);
    if (pid != g_pid) return;
    GetClassNameA(h, cls, sizeof(cls));
    n = ReadText(h, g_tx, sizeof(g_tx));
    if (g_filt && g_filt[0] && !strstr(cls, g_filt)) return;
    if (n <= 0) return;
    GetWindowRect(h, &r);
    printf("%*s%-22s hwnd=%p %dx%d len=%d\n%*s|%s|\n",
           depth * 2, "", cls, h, r.right - r.left, r.bottom - r.top, n, depth * 2, "", g_tx);
}

static BOOL CALLBACK EnumKids(HWND h, LPARAM l)
{
    if (l > 8) return TRUE;
    PrintKid(h, (int)l);
    EnumChildWindows(h, EnumKids, l + 1);
    return TRUE;
}

static BOOL CALLBACK EnumTops(HWND h, LPARAM l)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == g_pid) EnumChildWindows(h, EnumKids, 0);
    return TRUE;
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) { printf("usage: wtext.exe <pid> [class-filter|@hwnd]\n"); return 1; }
    g_pid = (DWORD)atoi(argv[1]);
    printf("pid=%lu\n", (unsigned long)g_pid);
    g_filt = (argc > 2 && argv[2][0] != '@') ? argv[2] : "";
    g_proc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, g_pid);
    if (!g_proc) { printf("OpenProcess failed %lu\n", GetLastError()); return 2; }
    g_remote = VirtualAllocEx(g_proc, NULL, 66560, MEM_COMMIT, PAGE_READWRITE);
    if (!g_remote) { printf("VirtualAllocEx failed %lu\n", GetLastError()); return 3; }
    printf("remote=%p\n", g_remote);

    if (argc > 2 && argv[2][0] == '@') {
        HWND h = (HWND)(DWORD)strtoul(argv[2] + 1, NULL, 16);
        char tx[66000];
        int n = ReadText(h, tx, sizeof(tx));
        printf("hwnd=%p len=%d\n|%s|\n", h, n, tx);
    } else {
        EnumWindows(EnumTops, 0);
    }
    VirtualFreeEx(g_proc, g_remote, 0, MEM_RELEASE);
    CloseHandle(g_proc);
    return 0;
}

/* wndproc32.c -- which module owns the window proc of the (first) window with a given class?
 * 32-bit, so GetClassLong(GCL_WNDPROC) works against 32-bit targets.
 * usage: wndproc32.exe <pid> <classname>
 */
#include <windows.h>
#include <stdio.h>

/* psapi.h is not in VC6; declare the little we need and bind psapi.dll dynamically. */
typedef struct { LPVOID lpBaseOfDll; DWORD SizeOfImage; LPVOID EntryPoint; } MY_MODULEINFO;
typedef BOOL (WINAPI *PFN_EnumProcessModules)(HANDLE, HMODULE*, DWORD, LPDWORD);
typedef BOOL (WINAPI *PFN_GetModuleInformation)(HANDLE, HMODULE, MY_MODULEINFO*, DWORD);

static DWORD g_pid;
static char g_want[128];
static HWND g_hit;

static BOOL CALLBACK cbChild(HWND h, LPARAM l);

static BOOL CALLBACK cb(HWND h, LPARAM l)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == g_pid)
    {
        char cls[128];
        GetClassNameA(h, cls, 127);
        if (lstrcmpiA(cls, g_want) == 0 && g_hit == 0)
            g_hit = h;
        EnumChildWindows(h, cbChild, 0);
    }
    return TRUE;
}

static BOOL CALLBACK cbChild(HWND h, LPARAM l)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == g_pid)
    {
        char cls[128];
        GetClassNameA(h, cls, 127);
        if (lstrcmpiA(cls, g_want) == 0 && g_hit == 0)
            g_hit = h;
    }
    return TRUE;
}

int main(int argc, char** argv)
{
    g_pid = (DWORD)atoi(argv[1]);
    lstrcpynA(g_want, argv[2], 127);
    EnumWindows(cb, 0);
    if (g_hit == 0) { printf("no window with class %s in pid %u\n", g_want, g_pid); return 1; }
    {
        DWORD wp = (DWORD)GetClassLongA(g_hit, GCL_WNDPROC);
        DWORD gw = (DWORD)GetWindowLongA(g_hit, GWL_WNDPROC);
        DWORD ud = (DWORD)GetWindowLongA(g_hit, GWL_USERDATA);
        DWORD ex = (DWORD)GetWindowLongA(g_hit, GWL_EXSTYLE);
        DWORD st = (DWORD)GetWindowLongA(g_hit, GWL_STYLE);
        RECT wr, cr;
        GetWindowRect(g_hit, &wr);
        GetClientRect(g_hit, &cr);
        HMODULE mods[512]; DWORD need = 0;
        char path[512];
        HANDLE hp = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, g_pid);
        printf("hwnd=0x%08X class=%s style=0x%08X ex=0x%08X\n", (unsigned)g_hit, g_want, st, ex);
        printf("classproc=0x%08X  gwlp=0x%08X  userdata=0x%08X\n", wp, gw, ud);
        printf("win=%dx%d client=%dx%d\n", wr.right - wr.left, wr.bottom - wr.top,
               cr.right, cr.bottom);
        printf("icon: wm_geticon small=0x%08X big=0x%08X small2=0x%08X | class hicon=0x%08X hiconsm=0x%08X\n",
               (unsigned)SendMessageA(g_hit, WM_GETICON, 0, 0),
               (unsigned)SendMessageA(g_hit, WM_GETICON, 1, 0),
               (unsigned)SendMessageA(g_hit, WM_GETICON, 2, 0),
               (unsigned)GetClassLongA(g_hit, GCL_HICON),
               (unsigned)GetClassLongA(g_hit, GCL_HICONSM));
        {
            HMODULE hPs = LoadLibraryA("psapi.dll");
            PFN_EnumProcessModules pEnum = (PFN_EnumProcessModules)GetProcAddress(hPs, "EnumProcessModules");
            PFN_GetModuleInformation pInfo = (PFN_GetModuleInformation)GetProcAddress(hPs, "GetModuleInformation");
        if (hp && pEnum && pInfo && pEnum(hp, mods, sizeof(mods), &need))
        {
            DWORD n = need / sizeof(HMODULE), i;
            for (i = 0; i < n; i++)
            {
                MY_MODULEINFO mi;
                if (!pInfo(hp, mods[i], &mi, sizeof(mi))) continue;
                if ((DWORD)mi.lpBaseOfDll <= wp && wp < (DWORD)mi.lpBaseOfDll + mi.SizeOfImage)
                {
                    GetModuleFileNameA(mods[i], path, 511);
                    printf("classproc is in %s +0x%X\n", path, wp - (DWORD)mi.lpBaseOfDll);
                }
                if ((DWORD)mi.lpBaseOfDll <= gw && gw < (DWORD)mi.lpBaseOfDll + mi.SizeOfImage)
                {
                    GetModuleFileNameA(mods[i], path, 511);
                    printf("gwlp      is in %s +0x%X\n", path, gw - (DWORD)mi.lpBaseOfDll);
                }
            }
        }
        else
            printf("OpenProcess/EnumProcessModules failed (%u)\n", GetLastError());
        }
        printf("foreground=%s (fg=0x%08X)\n", GetForegroundWindow() == g_hit ? "same" : "other",
               (unsigned)GetForegroundWindow());
    }
    return 0;
}

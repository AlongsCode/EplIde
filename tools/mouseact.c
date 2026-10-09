/* mouseact.c -- send a REAL mouse event / key press into another process's window.
 *
 * Why: the original e.exe IDE ignores PostMessage'd mouse messages (it wants real input),
 * so every "click the original and look" probe needs this. Coordinates are CLIENT
 * coordinates of a given HWND, which is far easier to compute than screen coordinates.
 *
 * Build (VC6): cl /nologo /MT /O2 /Fe..\tools\mouseact.exe mouseact.c user32.lib
 * Usage:
 *   mouseact.exe <hwnd-hex> click <x> <y> [dbl]
 *   mouseact.exe <hwnd-hex> move  <x> <y>
 *   mouseact.exe <hwnd-hex> key   <vk-hex> [ctrl] [shift]
 *   mouseact.exe <hwnd-hex> type  <gbk-bytes-as-text>
 *   mouseact.exe <hwnd-hex> drag  <x1> <y1> <x2> <y2>
 *
 * hwnd may be given as 0x... ; the root (top-level) window is focused first.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HWND  g_hwnd;
static HWND  g_root;

static HWND RootOf(HWND h)
{
    HWND p = h;
    while (GetParent(p)) p = GetParent(p);
    return p;
}

static void ClickAt(int x, int y, int dbl)
{
    POINT pt;
    pt.x = x; pt.y = y;
    ClientToScreen(g_hwnd, &pt);
    SetForegroundWindow(g_root);
    Sleep(300);
    SetCursorPos(pt.x, pt.y);
    Sleep(150);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(60);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    if (dbl) {
        Sleep(60);
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Sleep(60);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    }
    Sleep(250);
    printf("click client %d,%d -> screen %ld,%ld (root %p)\n", x, y, pt.x, pt.y, g_root);
}

static void MoveAt(int x, int y)
{
    POINT pt;
    pt.x = x; pt.y = y;
    ClientToScreen(g_hwnd, &pt);
    SetForegroundWindow(g_root);
    Sleep(200);
    SetCursorPos(pt.x, pt.y);
    Sleep(600);              /* let a tooltip appear */
    printf("move client %d,%d -> screen %ld,%ld\n", x, y, pt.x, pt.y);
}

static void DragFromTo(int x1, int y1, int x2, int y2)
{
    POINT a, b;
    int i, steps = 12;
    a.x = x1; a.y = y1; ClientToScreen(g_hwnd, &a);
    b.x = x2; b.y = y2; ClientToScreen(g_hwnd, &b);
    SetForegroundWindow(g_root);
    Sleep(300);
    SetCursorPos(a.x, a.y);
    Sleep(150);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(150);
    for (i = 1; i <= steps; i++) {
        SetCursorPos(a.x + (b.x - a.x) * i / steps, a.y + (b.y - a.y) * i / steps);
        Sleep(30);
    }
    Sleep(150);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    Sleep(250);
    printf("drag client %d,%d -> %d,%d\n", x1, y1, x2, y2);
}

static void PressKey(int vk, int ctrl, int shift)
{
    SetForegroundWindow(g_root);
    Sleep(300);
    if (ctrl)  keybd_event(VK_CONTROL, 0, 0, 0);
    if (shift) keybd_event(VK_SHIFT, 0, 0, 0);
    keybd_event((BYTE)vk, 0, 0, 0);
    Sleep(60);
    keybd_event((BYTE)vk, 0, KEYEVENTF_KEYUP, 0);
    if (shift) keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
    if (ctrl)  keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    Sleep(250);
    printf("key vk=0x%02X ctrl=%d shift=%d\n", vk, ctrl, shift);
}

/* type raw bytes as keystrokes (WM_CHAR to the focused window is not enough for the
   original's custom editor, so we fake the scan codes through keybd_event where we can,
   and fall back to posting WM_CHAR for the rest) */
static void TypeText(const char* s)
{
    int i;
    SetForegroundWindow(g_root);
    Sleep(300);
    for (i = 0; s[i]; i++) {
        SHORT vk = VkKeyScanA(s[i]);
        if (vk == -1) {
            PostMessageA(g_hwnd, WM_CHAR, (WPARAM)(unsigned char)s[i], 0);
            Sleep(40);
            continue;
        }
        keybd_event((BYTE)(vk & 0xFF), 0, 0, 0);
        Sleep(30);
        keybd_event((BYTE)(vk & 0xFF), 0, KEYEVENTF_KEYUP, 0);
        Sleep(60);
    }
    printf("typed %d bytes\n", i);
}

int main(int argc, char** argv)
{
    const char* mode;
    if (argc < 3) {
        printf("usage: mouseact.exe <hwnd-hex> click|move|key|type|drag ...\n");
        return 1;
    }
    g_hwnd = (HWND)(DWORD)strtoul(argv[1], NULL, 16);
    if (!IsWindow(g_hwnd)) { printf("bad hwnd %s\n", argv[1]); return 2; }
    g_root = RootOf(g_hwnd);
    mode = argv[2];

    if (strcmp(mode, "click") == 0)      ClickAt(atoi(argv[3]), atoi(argv[4]), argc > 5);
    else if (strcmp(mode, "move") == 0)  MoveAt(atoi(argv[3]), atoi(argv[4]));
    else if (strcmp(mode, "key") == 0)   PressKey((int)strtol(argv[3], NULL, 16),
                                                 argc > 4 && atoi(argv[4]),
                                                 argc > 5 && atoi(argv[5]));
    else if (strcmp(mode, "type") == 0)  TypeText(argv[3]);
    else if (strcmp(mode, "drag") == 0)  DragFromTo(atoi(argv[3]), atoi(argv[4]),
                                                    atoi(argv[5]), atoi(argv[6]));
    else { printf("unknown mode %s\n", mode); return 1; }
    return 0;
}

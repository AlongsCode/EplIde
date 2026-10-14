/* textofs.c -- where does the classic caption put its text, with and without an icon? */
#include <windows.h>
#include <stdio.h>
typedef BOOL (WINAPI *PFN_PRINTWINDOW)(HWND, HDC, UINT);

static void grab(HWND h, const char* tag)
{
    RECT wr; GetWindowRect(h, &wr);
    int w = wr.right - wr.left, hh = wr.bottom - wr.top;
    HDC hdc = GetDC(0), mdc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, hh); HGDIOBJ old = SelectObject(mdc, bmp);
    PFN_PRINTWINDOW pw = (PFN_PRINTWINDOW)GetProcAddress(LoadLibraryA("user32.dll"), "PrintWindow");
    pw(h, mdc, 0);
    BITMAPINFOHEADER bi; memset(&bi, 0, sizeof(bi));
    bi.biSize = 40; bi.biWidth = w; bi.biHeight = -hh; bi.biPlanes = 1; bi.biBitCount = 32;
    unsigned char* buf = (unsigned char*)malloc(w * hh * 4);
    GetDIBits(mdc, bmp, 0, hh, buf, (BITMAPINFO*)&bi, 0);
    /* find first x in rows 4..22 whose colour differs from the gradient of row0 col2 */
    int x, y, first = -1;
    for (x = 2; x < w; x++)
    {
        for (y = 6; y < 22; y++)
        {
            unsigned char* p = buf + (y * w + x) * 4;
            /* gradient is a smooth blue; text pixels are dark or yellow-ish */
            int r = p[2], g = p[1], b = p[0];
            if (r < 0x70 || (r > 0x90 && g < 0x80)) { if (first < 0 || x < first) first = x; }
        }
    }
    printf("%-22s %dx%d first-text-x=%d\n", tag, w, hh, first);
    SelectObject(mdc, old); DeleteObject(bmp); DeleteDC(mdc); ReleaseDC(0, hdc);
    free(buf);
}

int main()
{
    WNDCLASSA wc; memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(0);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = "TParent"; RegisterClassA(&wc);
    HWND p = CreateWindowExA(0, "TParent", "parent", WS_OVERLAPPEDWINDOW, 20, 20, 520, 420, 0, 0, wc.hInstance, 0);
    ShowWindow(p, SW_SHOW);
    WNDCLASSA wc2 = wc; wc2.lpszClassName = "TChild";
    wc2.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    RegisterClassA(&wc2);
    DWORD st = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU;
    HWND c1 = CreateWindowExA(0x101, "TChild", "Abcdefg", st, 0, 0, 300, 150, p, 0, wc.hInstance, 0);
    HWND c2 = CreateWindowExA(0x101, "TChild", "Abcdefg", st, 0, 160, 300, 150, p, 0, wc.hInstance, 0);
    SetWindowLongA(c2, GWL_EXSTYLE, 0x101);
    SendMessageA(c2, WM_SETICON, 0, (LPARAM)LoadIconA(0, IDI_APPLICATION));
    SendMessageA(c2, WM_SETICON, 1, (LPARAM)LoadIconA(0, IDI_APPLICATION));
    SendMessageA(c1, WM_NCACTIVATE, TRUE, 0);
    SendMessageA(c2, WM_NCACTIVATE, TRUE, 0);
    SetWindowTextA(c1, "Abcdefg");
    SetWindowTextA(c2, "Abcdefg");
    Sleep(400);
    grab(c1, "no-icon");
    grab(c2, "with-icon");
    return 0;
}

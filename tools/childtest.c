#include <windows.h>
#include <stdio.h>
typedef BOOL (WINAPI *PFN_PRINTWINDOW)(HWND, HDC, UINT);
#define PrintWindow(h,dc,f) ((PFN_PRINTWINDOW)GetProcAddress(LoadLibraryA("user32.dll"),"PrintWindow"))(h,dc,f)
static BITMAPINFOHEADER bi;
int main(){
  WNDCLASSA wc; memset(&wc,0,sizeof(wc));
  wc.lpfnWndProc=DefWindowProcA; wc.hInstance=GetModuleHandleA(0);
  wc.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
  wc.lpszClassName="ECParent"; RegisterClassA(&wc);
  HWND p=CreateWindowExA(0,"ECParent","parent",WS_OVERLAPPEDWINDOW,50,50,500,400,0,0,wc.hInstance,0);
  ShowWindow(p,SW_SHOW);
  WNDCLASSA wc2=wc; wc2.lpszClassName="ECChild"; wc2.hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH); RegisterClassA(&wc2);
  HWND c=CreateWindowExA(0,"ECChild","hello child",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CAPTION|WS_SYSMENU,
                         0,0,200,150,p,0,wc.hInstance,0);
  HWND c2=CreateWindowExA(0,"ECChild","hello child",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CAPTION|WS_SYSMENU,
                          0,170,200,150,p,0,wc.hInstance,0);
  { typedef HRESULT (WINAPI *PFN_SETWINDOWTHEME)(HWND, LPCWSTR, LPCWSTR);
    PFN_SETWINDOWTHEME f=(PFN_SETWINDOWTHEME)GetProcAddress(LoadLibraryA("uxtheme.dll"),"SetWindowTheme");
    if(f) f(c2,L"",L""); }
  RECT wr,cr; GetWindowRect(c,&wr); GetClientRect(c,&cr);
  printf("child style=0x%08X win=%dx%d client=%dx%d nc=(%d,%d,%d,%d)\n",
     (unsigned)GetWindowLongA(c,GWL_STYLE), wr.right-wr.left, wr.bottom-wr.top, cr.right, cr.bottom,
     0,0,0,0);
  printf("parent style=0x%08X\n",(unsigned)GetWindowLongA(p,GWL_STYLE));
  Sleep(300);
  int w=wr.right-wr.left,h=wr.bottom-wr.top;
  HDC hdc=GetDC(0); HDC mdc=CreateCompatibleDC(hdc);
  HBITMAP bmp=CreateCompatibleBitmap(hdc,w,h); HGDIOBJ old=SelectObject(mdc,bmp);
  BOOL ok=PrintWindow(c,mdc,0);
  memset(&bi,0,sizeof(bi)); bi.biSize=40; bi.biWidth=w; bi.biHeight=-h; bi.biPlanes=1; bi.biBitCount=32;
  unsigned char* buf=(unsigned char*)malloc(w*h*4);
  GetDIBits(mdc,bmp,0,h,buf,(BITMAPINFO*)&bi,0);
  printf("PrintWindow=%d row0: ",ok);
  int x; for(x=0;x<12;x++) printf("%02X%02X%02X ",buf[(0*w+x)*4+2],buf[(0*w+x)*4+1],buf[(0*w+x)*4+0]);
  printf("\n row5: "); for(x=0;x<12;x++) printf("%02X%02X%02X ",buf[(5*w+x)*4+2],buf[(5*w+x)*4+1],buf[(5*w+x)*4+0]);
  printf("\n row40: "); for(x=0;x<12;x++) printf("%02X%02X%02X ",buf[(40*w+x)*4+2],buf[(40*w+x)*4+1],buf[(40*w+x)*4+0]);
  printf("\n");
  { FILE* f=fopen("childtest.raw","wb"); fwrite(buf,1,w*h*4,f); fclose(f); }
  printf("saved %dx%d\n",w,h);
  { /* second child: theming disabled */
    RECT w2; GetWindowRect(c2,&w2); int w2w=w2.right-w2.left,h2=w2.bottom-w2.top;
    SelectObject(mdc,old); DeleteObject(bmp);
    HBITMAP b2=CreateCompatibleBitmap(hdc,w2w,h2); SelectObject(mdc,b2);
    PrintWindow(c2,mdc,0);
    memset(&bi,0,sizeof(bi)); bi.biSize=40; bi.biWidth=w2w; bi.biHeight=-h2; bi.biPlanes=1; bi.biBitCount=32;
    unsigned char* b2buf=(unsigned char*)malloc(w2w*h2*4);
    GetDIBits(mdc,b2,0,h2,b2buf,(BITMAPINFO*)&bi,0);
    printf("nonthemed row0: "); int y; for(y=0;y<12;y++) printf("%02X%02X%02X ",b2buf[(0*w2w+y)*4+2],b2buf[(0*w2w+y)*4+1],b2buf[(0*w2w+y)*4+0]);
    printf("\n  row5: "); for(y=0;y<12;y++) printf("%02X%02X%02X ",b2buf[(5*w2w+y)*4+2],b2buf[(5*w2w+y)*4+1],b2buf[(5*w2w+y)*4+0]);
    printf("\n  row12: "); for(y=170;y<190;y++) printf("%02X%02X%02X ",b2buf[(12*w2w+y)*4+2],b2buf[(12*w2w+y)*4+1],b2buf[(12*w2w+y)*4+0]);
    printf("\n");
  }
  return 0;
}

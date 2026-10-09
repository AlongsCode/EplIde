// 最小 MFC 程序：验证 VC6 工具链能编译并静态链接 MFC
#include <afxwin.h>

class CTestApp : public CWinApp
{
public:
    virtual BOOL InitInstance();
};

BOOL CTestApp::InitInstance()
{
    CFrameWnd* pFrame = new CFrameWnd();
    pFrame->Create(0, _T("EClone 工具链测试"));
    m_pMainWnd = pFrame;
    pFrame->ShowWindow(SW_SHOW);
    pFrame->UpdateWindow();
    return TRUE;
}

CTestApp theApp;

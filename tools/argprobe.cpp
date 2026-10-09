// argprobe.cpp -- 打印支持库里某个命令的参数名（确认「参数归属线」的权威数据源）。
// 用法: argprobe.exe [lib目录] 命令名
#include <afxwin.h>
#include <stdio.h>
#include "ec_lib.h"

int main(int argc, char** argv)
{
    if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
        return 1;
    const char* dir = (argc > 1) ? argv[1] : "C:\\Program Files (x86)\\e\\lib";
    const char* nm  = (argc > 2) ? argv[2] : "信息框";

    CStringArray load;
    load.Add(_T("krnln"));
    CString log;
    if (!EcLoadLibs(CString(dir), &load, &log)) { printf("load failed\n"); return 1; }
    EcBuildTree();

    std::vector<CString> n, t;
    std::vector<BOOL>    d;
    BOOL ok = EcCmdArgsByName(CString(nm), &n, &t, &d);
    printf("cmd=%s found=%d args=%d\n", nm, ok, (int)n.size());
    for (size_t i = 0; i < n.size(); i++)
        printf("  [%d] name=%s type=%s def=%d\n", (int)i, (LPCSTR)n[i], (LPCSTR)t[i], (int)d[i]);

    // 顺带把几个常用命令打出来，方便对照原版截图
    static const char* more[] = { "取文本长度", "超级列表框类名", "是否为调试版", "取数组成员数", NULL };
    for (int k = 0; more[k]; k++)
    {
        n.clear(); t.clear(); d.clear();
        if (EcCmdArgsByName(CString(more[k]), &n, &t, &d))
            printf("cmd=%s args=%d\n", more[k], (int)n.size());
    }
    return 0;
}

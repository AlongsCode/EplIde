// libtest.cpp -- 脱离 GUI 跑一遍 ec_lib：载入支持库 -> 建树 -> 生成每个节点的提示文本。
// 用途：ec_lib 崩了的时候用它定位（EClone.exe 是 /subsystem:windows，没有 stdout）。
//
// 编译: tools\build_libtest.bat  ->  tools\libtest.exe
// 用法: libtest.exe [lib 目录]
#include <afxwin.h>
#include <stdio.h>
#include "ec_lib.h"

int main(int argc, char** argv)
{
    if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
    {
        printf("AfxWinInit failed\n");
        return 1;
    }
    const char* dir = (argc > 1) ? argv[1] : "C:\\Program Files (x86)\\e\\lib";
    int dumpAll = (argc > 2 && lstrcmpA(argv[2], "dump") == 0);

    CStringArray load;
    load.Add(_T("krnln"));
    CString log;
    printf("[1] EcLoadLibs(%s)\n", dir);
    fflush(stdout);
    BOOL ok = EcLoadLibs(CString(dir), &load, &log);
    printf("[2] ok=%d libs=%d\n", ok, EcLibCount());
    fflush(stdout);

    int i;
    for (i = 0; i < EcLibCount(); i++)
    {
        const EcLib& L = EcLibAt(i);
        printf("    lib%d name=%s ver=%d.%d.%d dt=%d cats=%d cmd=%d const=%d\n",
               i, (LPCSTR)L.name, L.maj, L.min, L.build,
               L.nDataType, (int)L.cats.size(), L.nCmd, L.nConst);
        fflush(stdout);
    }
    printf("[3] EcBuildTree\n");
    fflush(stdout);
    EcBuildTree();
    int n = EcTreeCount();
    printf("[4] nodes=%d\n", n);
    fflush(stdout);
    {
        int cnt[12];
        for (i = 0; i < 12; i++) cnt[i] = 0;
        for (i = 0; i < n; i++) cnt[EcTreeNodeAt(i).kind]++;
        printf("    kinds: lib=%d cmdcat=%d dtcat=%d constcat=%d cmd=%d dt=%d member=%d prop=%d event=%d elem=%d const=%d\n",
               cnt[EN_LIB], cnt[EN_CMD_CAT], cnt[EN_DT_CAT], cnt[EN_CONST_CAT], cnt[EN_CMD],
               cnt[EN_DT], cnt[EN_MEMBER], cnt[EN_PROP], cnt[EN_EVENT], cnt[EN_ELEM], cnt[EN_CONST]);
        fflush(stdout);

        // 回归断言：这一组数字是原版 IDE 工作夹支持库树的实测值
        // (shots\orig_tree_libs.txt, treeinfo.exe e.exe 全量 dump)。
        // 只对本机的 krnln.fne 5.7.64 成立；换了支持库版本就要重新标定。
        static const int want[11] = { 1, 22, 1, 1, 375, 48, 293, 376, 64, 44, 335 };
        static const char* nm[11] = { "lib", "cmdcat", "dtcat", "constcat", "cmd", "dt",
                                      "member", "prop", "event", "elem", "const" };
        int bad = 0;
        for (i = 0; i < 11; i++)
            if (cnt[i] != want[i])
            {
                printf("    ** MISMATCH %s: got %d want %d\n", nm[i], cnt[i], want[i]);
                bad = 1;
            }
        if (bad || n != 1560)
        {
            printf("[FAIL] tree does not match the reference dump (nodes=%d want 1560)\n", n);
            return 2;
        }
        printf("    [PASS] 节点构成与原版 dump 逐项一致\n");
        fflush(stdout);
    }
    // 每个数据类型的子节点数（和原版 dump 对齐用）
    {
        const char* cur = NULL;
        int m = 0, pr = 0, ev = 0;
        for (i = 0; i < n; i++)
        {
            const EcTreeNode& nd = EcTreeNodeAt(i);
            if (nd.kind == EN_DT)
            {
                if (cur != NULL)
                    printf("DT\t%s\t%d\t%d\t%d\n", cur, m, pr, ev);
                cur = (LPCSTR)nd.text; m = pr = ev = 0;
            }
            else if (nd.kind == EN_MEMBER) m++;
            else if (nd.kind == EN_PROP) pr++;
            else if (nd.kind == EN_EVENT) ev++;
        }
        if (cur != NULL)
            printf("DT\t%s\t%d\t%d\t%d\n", cur, m, pr, ev);
        fflush(stdout);
    }

    for (i = 0; i < n; i++)
    {
        const EcTreeNode& nd = EcTreeNodeAt(i);
        CString h = EcNodeHint(i);
        if (dumpAll)
        {
            printf("===== %d =====\n%s\n", i, (LPCSTR)h);
            continue;
        }
        if (i < 3 || (i % 200) == 0)
            printf("    node %4d kind=%d |%s| hint=%d chars\n",
                   i, nd.kind, (LPCSTR)nd.text, h.GetLength());
        fflush(stdout);
    }
    printf("[5] all %d hints built, no crash\n", n);
    return 0;
}

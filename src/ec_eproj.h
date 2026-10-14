// ec_eproj.h -- 把「真 .e 二进制工程文件」读成 eclone 的文本源码形式。
//
// 为什么是这个做法：eclone 的工作夹->程序树、以及所有表页（程序集头/子程序头/参数表/
// 变量表/数据类型表/常量表…）**全都由一份文本源码驱动**（见 EClone.cpp 的 EcBuildProgTree
// 与 ec_editor.cpp 的 SetText/Rebuild）。所以打开 .e 不需要新写任何渲染代码，
// 只要把 .e 解析出的东西**生成成同一份文本**，下游全自动。
//
// 格式判据：以 e.exe 为准（REA/IDA 伪 C）。
//   5.9 读盘总控 = `sub_45B9B0`；段头 = [u32 magic 0x15117319][u32 lane-XOR 校验][92 字节段体]，
//   段体 +0 key / +4 段名(GBK, ^= Key[(i+1)%4]) / +36 index / +48 payload 长度。
//   段 key 分派（17 种）与各段读者见 `D:\EPL\_ida\SEGMENT_SPEC.md`（结论已回 e.exe 复核）。
// 详细坐标见 D:\EPL\HANDOFF.md。
#ifndef __EC_EPROJ_H
#define __EC_EPROJ_H

#include <afxwin.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 程序资源段（0x04007319）解出来的**窗体/组件**——设计器照这个建真组件。
// 元素字段布局（逐字节依据 4 个真样本互证 + e-packager 读侧字段序 cross-check，
// 明细见 PLAN_EPARSE.md 的 R3）：
//   I32 dataType / 20 字节 / NUL 名字 / NUL 注释 / I32 cWndAddress /
//   I32 left,top,width,height / I32 unknown,parent / [u32 n][id×n] /
//   [u32 len][cursor] / NUL tag / I32 unknown,showStatus,tabIndex /
//   I32 eventCount + [key,value]×n / 20 字节 / **剩余 = 支持库属性数据**
// ⚠️ 窗体自己也是其中一条元素（dataType 低 16 位 = 1），它的 w/h 就是窗体尺寸。
struct EcUnitBlob
{
    unsigned int id;
    unsigned int dataType;
    CString      name;
    int          left, top, width, height;
    unsigned int parent;
    unsigned int showStatus;
    std::string  blob;      // = PFN_CREATE_UNIT 的 pAllData / nAllDataSize
};

struct EcUnitForm
{
    CString name, comment;
    int     left, top, width, height;
    std::vector<EcUnitBlob> units;
};

// 本工程解析出来的窗体表（NULL = 没打开 .e；空表 = 打开了但没有窗体）。
// ⚠️ 用全局指针而不是「返回 vector 指针的 0 参函数」：VC6 对后者的调用点报
//    C2668 ambiguous call（换名字也照样报），改用指针绕开。
extern const std::vector<EcUnitForm>* g_pEcForms;

// 打开一个 .e 二进制工程，生成 eclone 文本源码。
// pszPath : .e 文件路径
// strOut  : 生成的源码（CRLF 分行，GBK 内容）
// 返回 TRUE = 成功
BOOL EcLoadEProject(LPCTSTR pszPath, CString& strOut);

// 供自测：只判文件头是不是 .e
BOOL EcIsEFile(LPCTSTR pszPath);

// ---- 段表读取出来的工程级信息（都以 e.exe 的读者为准）----
// 用户信息段（`sub_4556F0`）U00：原版标题栏第一段就是它，空 = 没读到
LPCTSTR EcProjectName();
// 用户信息段 U01：程序说明
LPCTSTR EcProjectDesc();
// 原版标题栏前缀：`<程序名称> - <完整路径> [无编译条件] - Windows窗口程序`
// （没读到程序名 / 没打开过 .e 时返回空串，调用方保留老标题）
LPCTSTR EcProjectTitlePrefix();
// 辅助信息段1（16 字节/条 [form][组件ID][事件槽][子程序ID]）解出的 组件事件 → 子程序 绑定
int     EcProjectEventBindCount();
BOOL    EcProjectEventBindAt(int i, unsigned int* pForm, unsigned int* pComp,
                             unsigned int* pSlot, unsigned int* pMethod);

#endif // __EC_EPROJ_H

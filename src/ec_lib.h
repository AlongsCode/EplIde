// ec_lib.h -- 载入易语言支持库(.fne/.fnr)，按 sdk\cpp\elib\lib2.h 的结构读 LIB_INFO，
//             并生成「工作夹->支持库」树的节点与状态栏提示文本。
//
// 为什么是"真载入"而不是离线解析：核心库 krnln.fne 的 LIB_DATA_TYPE_INFO 数组
// 在文件里只有第 0 项是静态初始化好的，其余 50 项由库自己的初始化代码在运行期填。
// 离线静态解析只能拿到 1 个数据类型，所以必须 LoadLibrary + GetNewInf()（原版就是这么干的）。
#ifndef __EC_LIB_H
#define __EC_LIB_H

#include <afxwin.h>
#include <afxtempl.h>
#include <vector>

// ------------------------------------------------------------------ lib2.h 的字段（只抄用得着的）
typedef DWORD EC_DATA_TYPE;

struct EcArg
{
    CString      name;
    CString      explain;
    EC_DATA_TYPE type;
    int          def;      // m_nDefault：逻辑型=真值/文本型=字符串指针/数字型=数值
    DWORD        state;    // AS_* 掩码
};

// ⚠️ 一律用 std::vector：MFC 4.2 的 CArray 用 memset 构造元素，装含 CString 的结构体会崩。
//    （CStringArray 是独立实现，可以用，但它没有 operator=，不能当成员。）

struct EcProp
{
    CString name;
    CString eg;
    CString explain;
    short   type;          // UD_* 1000..1024
    WORD    state;         // UW_* 掩码
    std::vector<CString> picks;      // m_szzPickStr 解出来的备选文本
};

struct EcEvent
{
    CString name;
    CString explain;
    DWORD   state;         // EV_* 掩码
    EC_DATA_TYPE ret;      // m_dtRetDataType（0=不返回数据）
    int     argStart;
    int     argCount;
};

struct EcElem
{
    CString      name;
    CString      eg;
    CString      explain;
    EC_DATA_TYPE type;
    DWORD        state;    // LES_*
    int          def;
};

struct EcCmd
{
    CString      name;
    CString      eg;
    CString      explain;
    int          category;     // 1 基（-1 = 对象成员命令）
    DWORD        state;        // CT_* 掩码
    EC_DATA_TYPE ret;
    int          level;        // LVL_*
    int          bmpIdx;       // m_shtBitmapIndex
    int          argStart;
    int          argCount;
};

struct EcDataType
{
    CString name;
    CString eg;
    CString explain;
    DWORD   state;             // LDT_* 掩码
    DWORD   bmpId;
    int     cmdStart;
    int     cmdCount;          // m_pnCmdsIndex 里的索引（指向 EcCmd）
    int     propStart;
    int     propCount;
    int     eventStart;
    int     eventCount;
    int     elemStart;
    int     elemCount;
    void*   pfnGetInterface;   // LIB_DATA_TYPE_INFO+44：取该类型的 ITF 接口（1=建组件，见 lib2.h ITF_*）
};

struct EcConst
{
    CString  name;
    CString  eg;
    CString  explain;
    short    type;             // CT_NULL/CT_NUM/CT_BOOL/CT_TEXT
    CString  text;
    double   value;
};

struct EcLib
{
    CString name;
    CString guid;
    CString explain;
    CString path;              // 磁盘上的 .fne 全路径
    CString author, zip, addr, phone, qq, email, home, other;
    int     maj, min, build;
    int     rqSysMaj, rqSysMin, rqKrnlMaj, rqKrnlMin;
    int     language;
    DWORD   state;             // LBS_* + __OS_*
    int     nDataType, nCategory, nCmd, nConst;
    int     dtStart;           // 本库的数据类型在全局表里的起始下标
    int     cmdStart;
    int     constStart;
    std::vector<CString> cats;   // 原始类别串（形如 "0001流程控制"）
};

// ------------------------------------------------------------------ 树的节点
enum EcNodeKind
{
    EN_LIB = 0,      // a=lib
    EN_CMD_CAT,      // a=lib b=类别号(1 基)
    EN_DT_CAT,       // a=lib
    EN_CONST_CAT,    // a=lib
    EN_CMD,          // a=lib b=命令号
    EN_DT,           // a=lib b=数据类型号
    EN_MEMBER,       // a=lib b=数据类型号 c=命令号
    EN_PROP,         // a=lib b=数据类型号 c=属性号
    EN_EVENT,        // a=lib b=数据类型号 c=事件号
    EN_ELEM,         // a=lib b=数据类型号 c=子数据成员号
    EN_CONST         // a=lib b=常量号
};

struct EcTreeNode
{
    int     kind;
    int     a, b, c;
    // ★ 第 24 轮：节点要用的图标号。原版拿支持库类别串的**4 位前缀**当 image index
    //   （`0014算术运算` -> img 14），而 `text` 走的是 CatDisplay()（前缀已被去掉），
    //   所以号必须在这里单独存一份 —— 从 display 文本里反解是解不出来的
    //   （我第一版就是那么写的：所有类别都落到 0，全树显示同一个图标）。
    int     icon;
    CString text;
};

// ------------------------------------------------------------------ 载入
// pszLibDir: 支持库目录（原版是 <安装目录>\lib）。目录里的 .fne 会被枚举，
//            但只载入 pszLoad 里列出的库（原版只把"当前程序用到的库"放进工作夹树）。
// pLoad = NULL 表示目录里所有 .fne 都载入。
BOOL EcLoadLibs(LPCTSTR pszLibDir, const CStringArray* pLoad, CString* pszLog);

// ------------------------------------------------------------------ 动态载入/卸载
// 「工具->支持库配置」（原版命令 #32906 / 模板 RT_DIALOG id=321）用这三个：
//   · 枚举目录里所有 .fne（升序全路径），核心库 krnln.fne 也在里面（由调用方过滤）
//   · EcLibIsLoaded 判断某个 .fne 当前载入了没有
//   · EcUnloadAllLibs 把已载入的库全部 FreeLibrary 并清空全局表
//     ⚠️ 调用前必须先把所有**设计期真窗口**销毁（它们是库建的对象），
//        否则 FreeLibrary 之后那些对象全成野指针。见 ec_design.h 的 EcDesignerDestroyAllUnits。
int  EcScanLibFiles(LPCTSTR pszLibDir, CStringArray* pOut);
BOOL EcLibIsLoaded(LPCTSTR pszPath);
BOOL EcUnloadAllLibs();

// ------------------------------------------------------------------ 访问
int      EcLibCount();
const EcLib& EcLibAt(int i);
int      EcLibFindByBaseName(LPCTSTR pszBase);   // "krnln" -> 索引，找不到返回 -1

// ------------------------------------------------------------------ 建树
// 生成节点表（可在 EcLoadLibs 之后随时调用，重复调用会重建）
void            EcBuildTree();
int             EcTreeCount();
const EcTreeNode& EcTreeNodeAt(int i);

// ------------------------------------------------------------------ 提示文本（状态栏用）
CString EcNodeHint(int i);

// ------------------------------------------------------------------ 名字分类（编辑器着色用）
// 返回 0=不认识 1=支持库命令 2=数据类型（含系统类型） 3=常量
int EcNameKind(LPCTSTR pszName);

// ------------------------------------------------------------------ 参数名（编辑器的「参数归属线」用）
// 按命令名（或英文名 eg）找支持库命令的参数表。返回 1 = 命中。
//   pNames 收参数名；pTypes 收 EcTypeName 短名；pDef 收"该参数有默认值"。
// 同名命令在多个库里都有时取先命中的。
BOOL EcCmdArgsByName(LPCTSTR pszName, std::vector<CString>* pNames,
                     std::vector<CString>* pTypes, std::vector<BOOL>* pDef);

// 供外部拼装用的小工具
CString EcTypeName(EC_DATA_TYPE v, int lib, BOOL bShort);
CString EcPropTypeName(short t);
CString EcOsText(DWORD state, BOOL bIsLib);

// ------------------------------------------------------------------ 设计器用：按支持库建**真**组件窗口
// 依据：LIB_DATA_TYPE_INFO.m_pfnGetInterface(+44) → ITF 1 = PFN_CREATE_UNIT（lib2.h:524/549）。
// 原版 IDE 的窗口设计器就是这么建的（e.exe sub_50D520，见
// work\...\krnln\window_designer_notes.md §7）。新建组件时 pAllData=NULL/nAllDataSize=0 合法。
#define EC_ITF_CREATE_UNIT              1
#define EC_ITF_PROPERTY_UPDATE_UI       2
#define EC_ITF_DLG_INIT_CUSTOMIZE_DATA  3
#define EC_ITF_NOTIFY_PROPERTY_CHANGED  4
#define EC_ITF_GET_ALL_PROPERTY_DATA    5
#define EC_ITF_GET_PROPERTY_DATA        6
#define EC_ITF_LANG_CNV                 9
#define EC_ITF_GET_NOTIFY_RECEIVER      12

#define EC_LDT_WIN_UNIT                (1 << 6)    // 是窗口/组件
#define EC_LDT_IS_CONTAINER            (1 << 7)
#define EC_LDT_IS_FUNCTION_PROVIDER    (1 << 15)   // 只提供功能、没有可视外形（时钟/对话框…）
#define EC_UW_IS_HIDED                 (1 << 4)    // UNIT_PROPERTY：隐藏但可用（属性表里不列）

const EcDataType* EcDtAt(int lib, int dt);              // 越界返回 NULL
const EcProp* EcPropAt(int lib, int dt, int idx);       // 第 idx 个属性（元数据里的下标 = ITF 6/4 的属性号）
// 第 idx 个事件（EcEvent::name = **中文事件名**，如 "被单击"）。
// 用途：打开真 .e 时，文件里存的是**被"英化插件"改过的英文事件名**
// （`_cmdSort_Click`），而原版左树显示的是 `_cmdSort_被单击`。
// 中文事件名只能从支持库元数据取（实测 `被单击` 在 krnln.fne 里有、在 e.exe 里搜不到）。
const EcEvent* EcEventAt(int lib, int dt, int idx);
// 诊断：把所有支持库数据类型的**事件名表**倒成文本（`ECLONE_DUMP_EVENTS=<路径>`）。
// 中文事件名只能从支持库元数据取（`被单击` 在 krnln.fne 里有、在 e.exe 里搜不到）；
// 这张表是"英文事件名 -> 中文事件名"对应的权威来源。
void EcDumpEventsIfAsked();
BOOL  EcDtIsUnit(int lib, int dt);                      // 是窗口组件（含不可视的功能组件）
BOOL  EcDtIsVisualUnit(int lib, int dt);                // 是**可视**组件（能放到设计窗体上画出来）
// 组件箱图标诊断（`ECLONE_DUMP_DTICON=<path>`）：倒出每个窗口组件类型的 m_nBmpID 等字段。
// 原版组件箱是**图标网格**，图标来自这个字段，所以先把真值取出来再决定怎么解。
void  EcDumpDtIconsIfAsked();
// 支持库自己的模块句柄：组件的图标就是这个 .fne 里的 RT_BITMAP，资源号 = EcDataType::bmpId。
// 实测（tools\pe_bmp_ids.py + ECLONE_DUMP_DTICON）krnln.fne 的 RT_BITMAP 261..272/1002..1147
// 与各组件类型的 bmpId 逐个相等。
HINSTANCE EcLibModule(int lib);
void* EcDtInterface(int lib, int dt, int itfNo);         // 取 ITF 接口函数指针，无则 NULL
BOOL  EcDtDefaultSize(int lib, int dt, int* pcx, int* pcy);   // ITF 12：设计期默认尺寸
// 用支持库建一个真窗口组件；返回组件对象指针（HUNIT，krnln 里是 MFC CWnd 派生类），失败 NULL
void* EcCreateUnit(int lib, int dt, HWND hParent, UINT uID, int x, int y, int cx, int cy,
                   HWND hDesignWnd, BOOL bInDesignMode);
// 同上，但把"全部属性数据"喂给库（ITF 1 的前两个参数 pAllData/nAllDataSize）。
// ⚠️ 这条是**重建组件**用的正路：属性改过之后原版就是 ITF 5 取旧数据 -> ITF 1 用这份数据重建
//    （e.exe 的 sub_4AE230）。pData=NULL/nData=0 时等价于新建。
// ★★ 「组件的默认属性数据」：用户 2026-10 指正的关键点 —— 建组件时不能一律传 NULL，
// 要先把**每个属性的默认值**取出来存成一份字节（原版 e.exe 就是这么干的）。
// 实现：先建一个探针组件 -> ITF 5 取它那份完整属性数据 -> 缓存；以后同类组件都用它建。
// 详见 ec_lib.cpp 里 EcGetDefaultPropertyData 的说明。
BOOL  EcGetDefaultPropertyData(int lib, int dt, int cx, int cy, BYTE** ppData, int* pnData);
void  EcForgetDefaultPropertyData();                  // 重载支持库时要清（库的代码段变了）
// 用默认属性数据建组件（新建组件的正路；没有默认数据时自动退回 NULL/0）
void* EcCreateUnitDefault(int lib, int dt, HWND hParent, UINT uID, int x, int y, int cx, int cy,
                          HWND hDesignWnd, BOOL bInDesignMode);
void* EcCreateUnitWithData(int lib, int dt, LPBYTE pData, int nData,
                           HWND hParent, UINT uID, int x, int y, int cx, int cy,
                           HWND hDesignWnd, BOOL bInDesignMode);
// ITF 5 = PFN_GET_ALL_PROPERTY_DATA：把组件**当前全部属性**序列化成一块内存（返回 HGLOBAL）。
// ★ 用户 2026-10 明确要求："每个属性有返回的接口，拿过来要保存"。
// 调用方拿到后把字节拷进自己的缓冲，再 `::GlobalFree` 掉返回的 HGLOBAL。
// 成功返回 TRUE 并把指针/长度写进 *ppData/*pnData（*ppData 是 new[] 出来的，调用方 delete[]）。
BOOL  EcGetAllPropertyData(int lib, int dt, void* hunit, BYTE** ppData, int* pnData);
// ITF 5 + ITF 1 的合体：把组件数据取出来再用同一份数据重建窗口。
// 原版"属性改了要重建窗口"就是这条（e.exe sub_4AE230）。返回新的 HUNIT（失败返回原对象）。
void* EcRebuildUnitWithProperties(int lib, int dt, void* hunit, HWND hParent, UINT uID,
                                  HWND hDesignWnd, BOOL bInDesignMode);
HWND  EcUnitHwnd(void* hunit);                          // 组件对象的 HWND（对象 +28 = CWnd::m_hWnd）
void  EcDestroyUnit(void* hunit);                       // 走对象虚表 vtbl[1](obj,1)：与 krnln 自己的回滚路径一致
// 设计期设"标题"属性（走 ITF 4 = NOTIFY_PROPERTY_CHANGED，和原版属性表改属性是同一条路）。
// 找不到"标题"属性或库不接受就返回 FALSE（调用方可以退回 SetWindowText）。
BOOL  EcSetUnitTitle(int lib, int dt, void* hunit, LPCTSTR pszText);

// ------------------------------------------------------------------ 属性表（设计器右侧/工作夹"属性"页）
// 读某属性（ITF 6 = PFN_GET_PROPERTY_DATA）；值按属性的 UD_* 类型格式化成显示文本。
// ⚠️ 取回的文本只**抄一份**、不释放（库可能返回它自己的内部存储，见 lib2.h:681 的说明）。
BOOL  EcGetUnitProperty(int lib, int dt, void* hunit, int idx, CString* pOut);
// 写某属性（ITF 4）。成功返回 TRUE；*pbNeedRecreate 非空时返回"库要求重建窗口"。
// 只支持数值/逻辑/文本/颜色，其它（图片/字体/自定义）返回 FALSE。
BOOL  EcSetUnitProperty(int lib, int dt, void* hunit, int idx, LPCTSTR pszValue,
                        BOOL* pbNeedRecreate);
// 该属性此刻可否编辑（ITF 2 = PFN_PROPERTY_UPDATE_UI）；没有接口时返回 TRUE
BOOL  EcPropertyEditable(int lib, int dt, void* hunit, int idx);
// 建完组件后把 左边/顶边/宽度/高度/可视/禁止 这六个固定属性写成我们摆的位置
// （NULL 数据建出来的对象这些字段是垃圾值，属性表会显示乱码）
BOOL  EcInitUnitGeometry(int lib, int dt, void* hunit, int x, int y, int cx, int cy);

#endif // __EC_LIB_H

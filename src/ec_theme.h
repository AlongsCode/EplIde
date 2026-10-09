// ec_theme.h -- 编辑器配色 / 显示选项（对应原版「工具->系统配置->程序显示」）
//
// 逆向依据（原版 e.exe 5.9.0.0，全部实测）：
//   · 菜单 工具->系统配置 = 命令 #32876；对话框模板 RT_DIALOG id=282
//     （267x217 对话框单位、78 个控件、SysTabControl32 id=1149），左侧 6 个页：
//     安全 / 存根 / 编译 / 设计 / 显示 / 编辑。其中「显示」页就是编辑器外观。
//   · 显示页控件（原文照抄）：
//       id=1122 Static   '语句行间距：'      id=1004 Edit
//       id=1124 Static   '字体：'            id=10   Button '设置(&F)'
//       id=1120 Static   '颜色配置：'        id=531  ComboBox（列出 clr 目录里的配色文件）
//       id=11   Button   '导入(&I)'          id=9    Button '保存颜色设置(&W)'
//       id=128  Button   '分支流程线画为虚线'
//       id=129  Button   '表格线画为虚线'
//       id=1155 Button   '画表格线'
//       id=149  Button   '画分隔线'
//       id=1135 Button   '画程序分支流程线'
//       id=1137 Button   '画参数归属线'
//       id=1139 Button   '显示参数名称'
//   · 配色文件 = <e>\clr\*.clr，第 1 个 DWORD 是版本号（0x009CCA6B/6C/6D，
//     读入时三个都接受），其后是颜色 DWORD 数组。e.exe 里存结构偏移 +0xa60、
//     长度 0x108（保存路径 sub_447840 写 0x9CCA6D，读取路径 sub_447370 校验魔数）。
//   · 颜色项名单（GBK，0x65E048 起，前 6 项是 6 个页名）：
//       通常前景 通常背景 表格标题前景 表格文本前景 通常表头背景 通常表格背景
//       程序集表头背景 程序集表背景 子程序定义表头背景 子程序定义表背景
//       子程序变量表头背景 子程序变量表背景 块内文本前景 块内文本背景
//       块内表格标题前景 通常调用名前景 流程命令名前景 语句中变量或组件名
//       成员方法名前景 成员变量或属性名前景 备注或草稿前景 数组下标中括号前景
//       小括号前景 操作符前景 成员引用句点前景 逗号分隔符前景 常量前景 数值前景
//       逻辑值前景 日期时间前景 字符串前景 常量集花括号前景 子程序指针前景
//       连接线 表格线 块内表格线 分隔线 参数开启状态指示器 参数名称文本前景
//       标志列背景 变量或参数定义名 数据类型 数组定义 选中标记 ...
//   · 默认配色（原版的下拉第一项「默认配色」）是 e.exe 里的硬编码数组，
//     在 .data 0x66D5D8 起共 42 个 DWORD。本文件把实测到的值写进
//     g_ecDefaultPalette[]，索引 = 「通常前景」起的颜色项序号。
//
// ⚠️ 已知不确定处（想继续加精度就从这里下手）：.clr 里的 DWORD 顺序与
//    e.exe 里那份硬编码数组**不一致**（例如 #D9E3F0 在文件里是第 9 个、
//    在硬编码数组里是第 8 个）。本模块按「颜色项名单」的序号线性取用，
//    并把实测确认过的槽位（网格/流程线/表头填充/语法色）用 EC_DEF_* 兜底，
//    所以**默认外观与原版逐色相同**；只有用户主动换 .clr 时，没实测过的
//    槽位（如「选中标记」）可能与原版有 ±1 的错位。
#ifndef _EC_THEME_H_
#define _EC_THEME_H_

#include <afxwin.h>

// 颜色槽（索引 = 「默认配色」数组下标 = 「通常前景」起的颜色项序号 + 1）
// 名字里的 EC_CS_ = EClone Colour Slot。
enum EcColorSlot
{
    EC_CS_VERSION = 0,          // 不是颜色：配色文件版本号
    EC_CS_FG,                   // 1  通常前景
    EC_CS_BG,                   // 2  通常背景
    EC_CS_TABLE_TITLE_FG,       // 3  表格标题前景
    EC_CS_TABLE_TEXT_FG,        // 4  表格文本前景
    EC_CS_HEADER_BG,            // 5  通常表头背景
    EC_CS_TABLE_BG,             // 6  通常表格背景
    EC_CS_ASM_HEADER_BG,        // 7  程序集表头背景
    EC_CS_ASM_BG,               // 8  程序集表背景
    EC_CS_SUB_HEADER_BG,        // 9  子程序定义表头背景
    EC_CS_SUB_BG,               // 10 子程序定义表背景
    EC_CS_VAR_HEADER_BG,        // 11 子程序变量表头背景
    EC_CS_VAR_BG,               // 12 子程序变量表背景
    EC_CS_BLOCK_FG,             // 13 块内文本前景
    EC_CS_BLOCK_BG,             // 14 块内文本背景
    EC_CS_BLOCK_TITLE_FG,       // 15 块内表格标题前景
    EC_CS_CALL_FG,              // 16 通常调用名前景
    EC_CS_FLOW_CMD_FG,          // 17 流程命令名前景（流程关键字 / 数据类型）
    EC_CS_VARNAME_FG,           // 18 语句中变量或组件名
    EC_CS_MEMBER_FG,            // 19 成员方法名前景（支持库命令）
    EC_CS_PROP_FG,              // 20 成员变量或属性名前景
    EC_CS_COMMENT_FG,           // 21 备注或草稿前景
    EC_CS_BRACKET_FG,           // 22 数组下标中括号前景
    EC_CS_PAREN_FG,             // 23 小括号前景
    EC_CS_OPER_FG,              // 24 操作符前景
    EC_CS_DOT_FG,               // 25 成员引用句点前景
    EC_CS_COMMA_FG,             // 26 逗号分隔符前景
    EC_CS_CONST_FG,             // 27 常量前景
    EC_CS_NUMBER_FG,            // 28 数值前景
    EC_CS_LOGIC_FG,             // 29 逻辑值前景
    EC_CS_DATETIME_FG,          // 30 日期时间前景
    EC_CS_STRING_FG,            // 31 字符串前景
    EC_CS_BRACE_FG,             // 32 常量集花括号前景
    EC_CS_SUBPTR_FG,            // 33 子程序指针前景
    EC_CS_LINK,                 // 34 连接线（流程控制虚线）
    EC_CS_GRID,                 // 35 表格线
    EC_CS_GRID_IN_BLOCK,        // 36 块内表格线
    EC_CS_SEP,                  // 37 分隔线
    EC_CS_PARAM_FLAG,           // 38 参数开启状态指示器（✔）
    EC_CS_PARAM_NAME_FG,        // 39 参数名称文本前景
    EC_CS_FLAGCOL_BG,           // 40 标志列背景（流程槽底色）
    EC_CS_DEFNAME_FG,           // 41 变量或参数定义名
    EC_CS_DATATYPE_FG,          // 42 数据类型
    EC_CS_ARRAY_FG,             // 43 数组定义
    EC_CS_MARK,                 // 44 选中标记
    EC_COUNT
};

// 显示选项（原版「程序显示」页上的复选框 / 编辑框）
struct EcDispOpt
{
    int  nLinePitch;            // 语句行间距（原版 Edit id=1004），默认 18
    int  nFontH;                // 字号（原版「字体：设置」），默认 12
    BOOL bFlowDot;              // 分支流程线画为虚线（id=128），默认 TRUE
    BOOL bGridDot;              // 表格线画为虚线（id=129），默认 FALSE
    BOOL bDrawGrid;             // 画表格线（id=1155），默认 TRUE
    BOOL bDrawSep;              // 画分隔线（id=149），默认 TRUE
    BOOL bDrawFlow;             // 画程序分支流程线（id=1135），默认 TRUE
    BOOL bDrawParamLine;        // 画参数归属线（id=1137），默认 TRUE
    BOOL bShowParamName;        // 显示参数名称（id=1139），默认 TRUE

    EcDispOpt();
};

// 全局主题：编辑器绘制时一律从这里取色 / 取度量
struct EcTheme
{
    COLORREF  pal[EC_COUNT];    // 下标就是 EcColorSlot
    EcDispOpt opt;
    CString   sScheme;          // 当前配色名（"默认配色" 或 .clr 文件名）

    EcTheme();

    COLORREF At(int slot) const
    {
        if (slot <= 0 || slot >= EC_COUNT) return RGB(0, 0, 0);
        return pal[slot];
    }
    void ResetToDefault();                                  // 恢复原版默认配色
    BOOL LoadFile(LPCTSTR pszPath);                         // 读 .clr
    BOOL SaveFile(LPCTSTR pszPath) const;                   // 写 .clr
    BOOL ApplyNamed(LPCTSTR pszScheme);                     // "默认配色" / "<clr 文件名>"
    void DumpIfAsked();                                     // ECLONE_DUMP_THEME 诊断
};

extern EcTheme g_ecTheme;

// ---------------------------------------------------------------------------
// 「系统配置」其余 6 个页签的设置（原版 RT_DIALOG id=282 的 通常/编 辑/设计/编译/存根/安全 页）
//
// 控件（id / 文字 / 坐标）**已经全部照原版模板摆好**（src\ec_syscfg.rc 的 IDD_SYSCFG，
// 78 个控件一条命令核对：python tools\dlg_list.py 282）；本结构管的是"值"——
// 进对话框读进来、按确认写回去、持久化到注册表（HKCU\Software\FlySky\E\EClone40，
// 与配色同一处，见 ec_theme.cpp 的 EC_REG_KEY）。
//
// ⚠️ 有行为的部分只有 EClone 本体真需要的那几个（见 EcCfgApply）：
//    · 新建窗口宽度/高度、是否显示网点、网点间隔宽高、是否对齐到网点 -> 设计器
//    · 撤消内存尺寸 -> 编辑器撤消栈上限
//    · 启动方式/_启动窗口名、备份文件、作者信息、调试输出文件名、清单、编译开关、
//      花指令/打乱码 —— **原版这些是给编译器和生成程序用的**，本工程没有编译器，
//      所以只做"摆出来 + 记住 + 置回默认值"，不产生行为（别以为忘了接）。
//    · 输入法 / 即时提示那一组是原版**编写环境自己**的行为（爱码输入法），
//      对 EClone 无意义，同样只摆出来 + 记住。
// ---------------------------------------------------------------------------
struct EcCfg
{
    // [0] 通常
    int     nStartMode;         // 1101 = 通过 _启动窗口 / 1102 = 通过 _启动子程序
    CString sStartWin;          // 按原版：启动窗口名固定为 _启动窗口（不可编辑，这里只存不改）
    BOOL    bBak;               // 1041 程序存盘时是否保存 .BAK 备份文件
    BOOL    bAuthor;            // 1042 编译发布版本时是否保存入作者信息
    int     nAutoSaveMin;       // 1001 自动保存编辑现场的间隔（分钟），0 = 禁止
    int     nUndoMemKB;         // 1011 撤消功能可用内存尺寸（千字节），0 = 不限制
    CString sDebugOut;          // 1236 调试编译输出文件名（空 = 系统默认）
    // [1] 编 辑
    int     nInputMethod;       // 1127 首拼及全拼 / 1128 双拼 / 1129 英文
    BOOL    bSouth;             // 1130 拼音类输入法支持南方音
    BOOL    bTip;               // 1180 即时输入提示
    BOOL    bSyntaxTip;         // 1132 输入新语句行后自动检查并提示语法错误
    int     nClipHistory;       // 1201 剪辑历史记录数
    // [2] 显 示（设计器外观；和编辑器外观的 EcDispOpt 不是一回事）
    int     nNewFormW;          // 1006 新建窗口宽度
    int     nNewFormH;          // 1007 新建窗口高度
    BOOL    bShowGrid;          // 1144 是否显示网点
    int     nGridW;             // 1008 网点间隔宽度
    int     nGridH;             // 1009 网点间隔高度
    BOOL    bSnapGrid;          // 1146 是否对齐到网点
    BOOL    bRoProps;           // 1147 属性表中是否列出设计时不可用或只读性质的属性
    // [4] 编 译
    BOOL    bFastAry;           // 1165 快速数组访问
    BOOL    bDllStack;          // 1168 调用 DLL 命令后检查堆栈错误
    BOOL    bDeadLoop;          // 1170 编译时检查死循环代码
    BOOL    bDupName;           // 1136 严格的重复名称检查
    BOOL    bStrictRef;         // 1140 严格的参数参考传递检查
    BOOL    bAskWriteLib;       // 1138 非独立编译后是否询问写出相关库
    BOOL    bPrecLoss;          // 1141 数值精度丢失检查
    CString sSignCert;          // 1235 模块编译后自动签名证书
    // [5] 存 根
    int     nManifest;          // 1103 = 使用预定义清单 / 1104 = 使用用户自定义清单
    BOOL    bComCtl6;           // 1208 是否使用 Windows 通用组件库 6.0 版
    BOOL    bUac;               // 1207 运行前是否请求管理员权限 (UAC)
    CString sManifest;          // 1010 自定义清单文本
    // [6] 安 全
    int     nJunkLevel;         // 1198 花指令插入级别
    int     nXorCode;           // 613  编译结果打乱码（-1 = 随机）

    EcCfg();
};

extern EcCfg g_ecCfg;

void EcCfgLoad();                       // 从注册表读（没有就用默认值）
void EcCfgSave();                       // 写回注册表
void EcCfgDumpIfAsked();                // ECLONE_DUMP_CFG=<path> 时倒成文本（回归用）

// 把配置里"有行为"的那几项推给正在跑的窗口（设计器网格/新建尺寸、编辑器撤消上限）。
// 调用点：启动时一次 + 「系统配置」按确认之后。
void EcCfgApply();

// 把「系统配置」对话框上的**其余 6 页**控件与 g_ecCfg 互相同步（实现见 ec_syscfg.cpp）。
// 这是"其余 5 个页签只摆样子"那一步的落地：所有控件都真的读/写/持久化。
void EcCfgToDlg(CWnd* pDlg);
void EcCfgFromDlg(CWnd* pDlg);

// --- 配色文件辅助 ---------------------------------------------------------
CString EcClrDir();                                     // <e 目录>\clr
void    EcEnumClrSchemes(CStringArray& out);            // "默认配色" + 目录里的 *.clr 名
BOOL    EcIsClrFile(LPCTSTR pszPath);                   // 校验魔数

// --- 持久化（HKCU\Software\FlySky\E\EClone40，不动原版的 EInf40）---------
void EcThemeLoad();
void EcThemeSave();

// 诊断：环境变量 ECLONE_DUMP_THEME=<path> 时把当前配色倒成可回归比对的文本
// （排错利器，和 ECLONE_DUMP_MODEL 一个套路；不设这个变量就没有任何开销）

#endif // _EC_THEME_H_

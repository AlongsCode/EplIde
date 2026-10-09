// resource.h -- EClone resource IDs
#ifndef _ECLONE_RESOURCE_H_
#define _ECLONE_RESOURCE_H_

#define IDR_MAINFRAME           128

// dock pane dialog templates
#define IDD_WORKPANE            201   // 工作夹 (left)
#define IDD_STATUSPANE          202   // 状态夹 (bottom)
#define IDD_COMPPANE            203   // 窗口组件箱 (right)

// 面板窗口 ID —— 与原版一致（查看->工具条 菜单里的编号）
#define ID_VIEW_WORKPANE        129
#define ID_VIEW_STATUSPANE      130
#define ID_VIEW_COMPPANE        110
#define ID_VIEW_WORKSTATUS      132

// 工具条 ID（原版编号）
#define IDR_TOOLBAR_STANDARD    59392
#define IDR_TOOLBAR_ALIGN       133
#define IDR_TOOLBAR_POSITION    135
#define IDR_SELECTOR_BAR        136

// 工具条位图（从 e.exe 资源导出：131 标准 / 297 对齐(灰) / 231 定位）
#define IDB_TOOLBAR_STD         2001
#define IDB_TOOLBAR_ALIGN       2002

// 标签页图标位图（e.exe 的 146 工作夹 / 148 状态夹，各 7 个 16x16）
#define IDB_TABICON_WORK        2003
#define IDB_TABICON_STATUS      2004
// 支持库树图标（原版来自 krnln.fne，e.exe 里没有，从原版截图抠的 24 个 16x16）
#define IDB_TREE_ICONS          2005
#define IDB_TOOLBAR_POS         2006
// 「程序」页树的节点图标（e.exe 里没有这组资源，从原版实测截图抠的 16x16 条带）
#define IDB_PROGTREE_ICONS      2007

// 选择器条上的两个下拉框
#define IDC_CBO_PROC            1301
#define IDC_CBO_EVENT           1302

// 工具条按钮命令 ID 区段（demo 不做功能，仅保证按钮不为灰）
#define IDC_TB_FIRST            40000
#define IDC_TB_LAST             40099

// 工作夹
#define IDC_TAB_WORK            1000
#define IDC_TREE_WORK           1019   // 支持库 / 程序 / 属性 三页依次 1019,1020,1021
// 状态夹
#define IDC_TAB_STATUS          1100
#define IDC_EDIT_OUTPUT         1110   // 八页依次 1110..1117
// 组件箱
#define IDC_TAB_COMP            1200   // 组件组下拉框（ListBox，原版是 SysTreeView32 的下拉）
#define IDC_TREE_COMP           1202   // 组件图标按钮的第一个（40 个连续 id）
// 原版实测（tools\probe_compbox3.py 倒的原版窗口树）：组件箱里是
// 「基本组件」下拉框 + **40 个 28x28 的 Button**（4 列 x 10 行，列/行距 29）。
// 按钮 id 用 2300..2339（2300..2699 段在本工程的 .rc 里没被占用）。
#define IDC_COMP_BTN_FIRST      2300
#define IDC_COMP_BTN_COUNT      40

// ---------------------------------------------------------------- 系统配置
// 原版：菜单「工具->系统配置」= 命令 32876，对话框模板 RT_DIALOG id=282
// （267x217 对话框单位、78 个控件、SysTabControl32 id=1149），
// 「程序显示」页上的控件 ID 原样沿用（见 ec_theme.h 的逆向依据）。
#define IDD_SYSCFG              282
#define ID_SYSCFG_TAB           1149
#define ID_SYSCFG_OK            1
#define ID_SYSCFG_CANCEL        2
#define ID_SYSCFG_DEFAULT       1150    // 置回默认值(&D)
#define IDC_CFG_LINEPITCH       1004    // 语句行间距 Edit
#define IDC_CFG_FONT            10      // 字体：设置(&F)
#define IDC_CFG_SCHEME          531     // 颜色配置 ComboBox
#define IDC_CFG_IMPORT          11      // 导入(&I)
#define IDC_CFG_SAVE            9       // 保存颜色设置(&W)
#define IDC_CFG_FLOWDOT         128     // 分支流程线画为虚线
#define IDC_CFG_GRIDDOT         129     // 表格线画为虚线
#define IDC_CFG_DRAWGRID        1155    // 画表格线
#define IDC_CFG_DRAWSEP         149     // 画分隔线
#define IDC_CFG_DRAWFLOW        1135    // 画程序分支流程线
#define IDC_CFG_DRAWPARAM       1137    // 画参数归属线
#define IDC_CFG_SHOWPARAM       1139    // 显示参数名称

// 「系统配置」另外 6 个页签上的控件（id 全部沿用原版 id=282 的编号）。
// 页签次序（原版实拍 shots\live_syscfg\tabs_zoom2.png）：
//     [0] 通常  [1] 编 辑  [2] 显示  [3] 设计  [4] 编译  [5] 存根  [6] 安全
// ⚠️ RC 侧只能看到 resource.h，C++ 侧 ec_syscfg.h 自带 #ifndef 兜底 —— 两边都要加。
// [0] 通常
#define IDC_CFG_G1              1116    // 分组框「Windows 窗口程序启动方式」
#define IDC_CFG_G2              1117    // 分组框「默认输入法」
#define IDC_CFG_STARTWIN        1101    // 通过 _启动窗口 启动
#define IDC_CFG_STARTSUB        1102    // 通过 _启动子程序 启动
#define IDC_CFG_BAK             1041    // 保存 .BAK 备份文件
#define IDC_CFG_AUTHOR          1042    // 编译发布版本时保存作者信息
#define IDC_CFG_SAVEMIN         1001    // 自动保存间隔（分钟）
#define IDC_CFG_UNDOMEM         1011    // 撤消可用内存（千字节）
#define IDC_CFG_DEBUGOUT        1236    // 调试编译输出文件名
#define IDC_CFG_DEBUGBROWSE     1242    // "..."
#define IDC_CFG_IM1             1127    // 首拼及全拼
#define IDC_CFG_IM2             1128    // 双拼
#define IDC_CFG_IM3             1129    // 英文
#define IDC_CFG_IMEDIT          8       // 编辑名称输入词组(&E)
#define IDC_CFG_SOUTH           1130    // 拼音类输入法支持南方音
#define IDC_CFG_TIP             1180    // 即时输入提示
#define IDC_CFG_SYNTAX          1132    // 输入新语句行后自动检查语法
#define IDC_CFG_CLIPNUM         1201    // 剪辑历史记录数
// [1] 编 辑（沿用上面「显示」页那批 id：原版这两页共用同一组显示开关）
// [2] 显示（设计器外观）
#define IDC_CFG_NEWW            1006    // 新建窗口宽度
#define IDC_CFG_NEWH            1007    // 新建窗口高度
#define IDC_CFG_GRID            1144    // 是否显示网点
#define IDC_CFG_GRIDW           1008    // 网点间隔宽度
#define IDC_CFG_GRIDH           1009    // 网点间隔高度
#define IDC_CFG_SNAP            1146    // 是否对齐到网点
#define IDC_CFG_ROPROP          1147    // 属性表列出只读属性
#define IDC_CFG_COLOR2          1142    // 第二个颜色下拉（原版 id=1142）
// ⚠️ 原版这个自绘色块的控件 id 是 5，但本工程 IDC_LC_DEL（支持库配置的「删除」）也是 5 ——
//    同一个进程里 GetDlgItem 会串到另一个对话框的控件上，所以这里**故意换个内部 id**
//    （22xx 是本工程自留段，静态标签也用它）。
#define IDC_CFG_COLORBOX        2250    // 自绘色块（原版 id=5，BS_OWNERDRAW）
// [3] 编译
#define IDC_CFG_FASTARY         1165    // 是否启用快速数组访问方式
#define IDC_CFG_DLLSTK          1168    // 调用DLL命令后是否检查堆栈错误
#define IDC_CFG_DEADLOOP        1170    // 编译时是否检查死循环代码
#define IDC_CFG_DUPNAME         1136    // 严格的重复名称检查
#define IDC_CFG_STRICTREF       1140    // 严格的参数参考传递检查
#define IDC_CFG_ASKWRITE        1138    // 非独立编译后是否询问写出相关库
#define IDC_CFG_PRECLOSS        1141    // 数值精度丢失检查
#define IDC_CFG_SIGNCERT        1235    // 模块编译后自动签名证书
#define IDC_CFG_SIGNBROWSE      1241    // "..."
// [4] 存根（清单）
#define IDC_CFG_MANIPRE         1103    // 使用预定义的清单
#define IDC_CFG_COMCTL6         1208    // 使用 Windows 通用组件库 6.0
#define IDC_CFG_UAC             1207    // 运行前请求管理员权限(UAC)
#define IDC_CFG_MANIUSER        1104    // 使用用户自定义清单
#define IDC_CFG_MANIIMPORT      1246    // 从文件导入(&F)
#define IDC_CFG_MANITEXT        1010    // 清单正文（多行）
// [5] 安全（混淆）
#define IDC_CFG_JUNKLEVEL       1198    // 花指令插入级别
#define IDC_CFG_XORCODE         613     // 编译结果打乱码

// ---------------------------------------------------------------- 支持库配置
// 原版：菜单「工具->支持库配置」= 命令 32906，对话框模板 RT_DIALOG **id=321**
// （322x290 对话框单位、11 个控件，dump 见 shots\e_dialogs.txt）。
// 控件 ID / 坐标 / 尺寸全部照抄模板。
#define IDD_LIBCFG              321
#define IDD_FIND                284     // 原版「寻找」对话框 (RT_DIALOG id=284)
#define IDC_FIND_TEXT           1151    // 查找文本框（下拉；原版控件 id）
#define IDC_FIND_CASE           1041    // 区分大小写（原版控件 id）
// 「寻找替换」对话框 = 原版 RT_DIALOG id=578（控件 id 照抄原版）
// ⚠️ RC 侧只能看到 resource.h（assemble_rc.py 把这里的 #define 内联进 EClone.rc），
//    所以新增控件 id 必须**两边都加**：这里 + ec_syscfg.h 的 #ifndef 兜底。
//    （踩过：只加 ec_syscfg.h → `EClone.rc(540): error RC2104 : undefined keyword or key name: IDC_REPL_HINT`）
#define IDD_REPLACE             578     // 寻找替换对话框 (270x82, 10 控件)
// 「书签跳转」对话框 = 原版 RT_DIALOG id=607（控件 id 照抄原版）
// ⚠️ 新增控件 id 必须 resource.h（给 RC）和 ec_syscfg.h（给 C++，用 #ifndef）**两边都加**。
#define IDD_BOOKMARK            607
#define IDC_BM_LIST             1036    // 书签列表（SysListView32，原版控件 id）
#define IDC_BM_DEL              3       // 「删除(&R)」
#define IDC_BM_DELALL           4       // 「全部删除(&L)」
#define IDC_REPL_TEXT           1181    // 搜索文本（下拉；原版控件 id）
#define IDC_REPL_WITH           1152    // 替换文本（下拉；原版控件 id）
#define IDC_REPL_HINT           1047    // 一行提示/状态（原版控件 id，模板里初始为空）
#define IDC_REPL_DO             3       // 「替换(&R)」
#define IDC_REPL_ALL            4       // 「全部替换(&A)」
#define IDC_LC_SEARCH           1011    // 查找（可输入拼音字母）
#define IDC_LC_LIST             1036    // 支持库列表（LBS_OWNERDRAWFIXED）
#define IDC_LC_DETAILS          1001    // 详情（只读多行）
#define IDC_LC_LABEL            1115    // "查找（可输入拼音字母）："
#define IDC_LC_HINT             65535   // 底部提示
#define IDC_LC_BUY              6       // 购买授权(&B)
#define IDC_LC_ALL              3       // 全选(&A)
#define IDC_LC_NONE             4       // 全不选(&N)
#define IDC_LC_DEL              5       // 删除(&D)

// 「整体搜寻」对话框 = 原版 RT_DIALOG **id=30773**（254x163，12 控件，标题「整体搜寻对话框」）
// ★ 它同时推翻了上一轮的一个推断：原版状态夹的「搜寻1/搜寻2」是**结果输出夹**（单选），
//   不是搜索栏 —— 证据是模板里那两条 BS_AUTORADIOBUTTON 的原文。
// ⚠️ 新增控件 id 必须 resource.h（给 RC）和 ec_syscfg.h（给 C++，用 #ifndef）**两边都加**。
#define IDD_WHOLEFIND           30773
#define IDC_WF_TEXT             1151    // 欲搜寻文本（下拉）
#define IDC_WF_SCOPE            1152    // 搜寻范围（下拉）
#define IDC_WF_CASE             1041    // 区分大小写
#define IDC_WF_EXPAND           1042    // 搜寻前自动展开所有被收缩子程序
#define IDC_WF_PANE1            1101    // “搜寻1”系统输出夹
#define IDC_WF_PANE2            1102    // “搜寻2”系统输出夹
#define IDC_WF_HINT             1047    // 一行提示/状态（模板初始为空）

#endif // _ECLONE_RESOURCE_H_

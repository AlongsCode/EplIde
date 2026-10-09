# EClone —— 易语言 5.9 IDE 界面复刻（VC6 / MFC 4.2）

用 IDA 逆向 + 运行时枚举拿到的确切数据，1:1 复刻易语言 IDE 的**界面骨架**与**代码编辑**能力。
不含编译器/调试器功能。

> **对标形态**：本机那个装了 iDraw/ExUI 美化套件的 IDE 的**实际观感**，但只复刻"易语言本体"部分。
> iDraw 带来的三样**明确不做**：自绘菜单条（Menulwy）、输出面板（Scintilla）、ExUI 皮肤；菜单用原生 HMENU。
> 交接文档见 `HANDOFF.md`（含全部踩过的坑和原版实测数字）。

## 构建

```
D:\eclone\build.bat          :: 没有 pwsh，用 cmd
```

- 编译器：本机 VC6（`C:\Program Files (x86)\Microsoft Visual Studio\VC98`，完整版）
- RC：`C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98\Bin\RC.EXE`
- 产物：`D:\eclone\bin\EClone.exe`（MFC 静态链接，无需额外 DLL）

构建管线（为什么这么绕，见下面的"坑"）：

```
src\*.cpp/.h/.rc   (UTF-8，手写)
   ↓ tools\assemble_rc.py    把 rc_head.rc + resource.h + bitmaps.rc + menu_main.rc + panes.rc 拼成单个 EClone.rc
   ↓ tools\utf8togbk.ps1     UTF-8 → GBK（VC6 按 ANSI 读源码），文本转 CRLF，二进制原样拷贝
gbk\               (编译输入)
   ↓ cl /c /MT /GX /O2  →  link nafxcw.lib ...
bin\EClone.exe
```

## 目录

| 路径 | 说明 |
|---|---|
| `EClone.cpp` | 应用 + 主框架（菜单/三行工具条/状态栏/四个停靠面板/面板内容/**「程序」页真程序集树**）+ MDI 子窗口（**3 个文档页：窗口设计器 / 程序集 / Dll命令**，点树节点切页） |
| `src\ec_lib.cpp/.h` | **支持库层**：`LoadLibrary` 各 `.fne` + `GetNewInf()`，按 `lib2.h` 的布局读 LIB_INFO，生成「支持库树」节点表和状态栏提示文本 |
| `src\ec_editor.cpp/.h` | **代码编辑器**：全自绘的结构化编辑器 —— 程序集头表格 / 子程序头表格 / 参数表 / 变量表 / **Dll命令块**（`.DLL命令`）/ 代码区 + 流程槽 + 流程控制虚线；语法着色、光标、键入、滚动。颜色一律从 `g_ecTheme` 取（没有写死的 RGB） |
| `src\ec_theme.cpp/.h` | **配色 / 显示选项**：64 个颜色槽 + 原版实测默认配色 + `clr\*.clr` 读写（魔数 `0x009CCA6B/6C/6D`）+ 持久化到 `HKCU\Software\FlySky\E\EClone40` + `ECLONE_DUMP_THEME` 诊断倒档 |
| `src\ec_syscfg.cpp/.h`、`src\ec_syscfg.rc` | **「工具→系统配置」对话框**（原版 RT_DIALOG id=282）：6 个页签（安全/存根/编译/设计/显示/编辑），已实现「程序显示」页 —— 语句行间距 / 字体 / 颜色配置下拉 / 导入 / 保存颜色设置 / 7 个绘制开关。**同一个文件里还有「工具→支持库配置」**（原版命令 #32906 / 模板 **RT_DIALOG id=321**）：列出 lib 目录下所有 `.fne`、勾选即载入，点确认后**运行期动态换库** |
| `src\ec_design.cpp/.h` | 文档标签条 + 窗体设计器。设计期的窗体/组件**全部由支持库真建**（`ITF 1 = PFN_CREATE_UNIT`，`blInDesignMode=TRUE`）；工作区 `#A0A0A0` + 窗体客户区"纸" `#F0F0F0` + **8px 黑点阵** + 选中框与 7x7 白框手柄；放置/选中/拖动/**8 手柄缩放**/Delete；组件箱与属性页的数据都来自支持库 |
| `src\sizecbar.cpp/.h` `src\scbarg.cpp/.h` | **vendored**：Cristi Posea 的 sizecbar v2.45（原版面板就是它）。只改了 include（本工程无 stdafx.h） |
| `src\resource.h` | 资源 ID（面板 ID 用原版编号：工作夹 129 / 状态夹 130 / 组件箱 110） |
| `src\panes.rc` | 三个停靠面板的对话框模板（`#32770` 内容，尺寸运行时会被像素级重排覆盖） |
| `src\ec_syscfg.rc` | 「系统配置」对话框模板（`assemble_rc.py` 会把它拼进 `EClone.rc`） |
| `src\menu_main.rc` | **自动生成**：从 `menus_main.txt`（e.exe 的 RT_MENU id=127 还原）转出的 19 弹出 / 137 命令 |
| `src\res\*.bmp` | **自动生成**（`tools\extract_icons.py`）：工具条/标签页/树的图标 |
| `src\rc_head.rc` | rc 头（`#include <afxres.h>`） |
| `tools\sizecbar\` | sizecbar v2.45 原始压缩包解开的参考源码（含 Demo1/Demo2） |
| `tools\` | 生成/转换/截图/测量/回归检查脚本（见下） |
| `shots\` | 各里程碑截图 + 窗口树 dump + 与原版的对照图 |
| `gbk\` `bin\` | 生成物（可删） |

### tools 里常用的脚本

| 脚本 | 作用 |
|---|---|
| `assemble_rc.py` | 把多个 rc 片段拼成单个 `.rc`（绕开 VC6 RC 的嵌套 include bug） |
| `utf8togbk.ps1` | UTF-8→GBK、去 BOM、统一 CRLF、二进制原样拷贝。**纯 ASCII** |
| `extract_icons.py` | 从 e.exe 的 RT_BITMAP + 从原版截图，生成 `src\res\*.bmp`（图标唯一来源） |
| `dump_res.py` / `dump_icons.py` | 列出/导出 e.exe 的所有资源与图标（`dump_icons.py` 会正确解 RT_ICON：高度翻倍的 XOR+AND 掩码） |
| `dump_bitmaps.py` | 上一轮的位图导出（PIL 路径，对 4bpp 位图不认；保留作参考） |
| `check_layout.py` | **布局回归检查**：拿窗口树 dump 对照原版实测值，期望 `14 passed, 0 failed` |
| `shot.ps1` | 启动 exe → PrintWindow 截图 → 导出窗口树 → 杀进程。支持 `-ExeArgs` |
| `crop.py` / `pair.py` / `measure.py` | 裁剪放大 / 多图竖排对照 / 量深色像素包围盒（定控件位置） |
| `cmp_windows.py` / `make_compare.py` | 窗口类层级对比 / 拼上下对比图 |
| `gen_menu_rc.py` | 菜单 rc 生成（从 `tools\menus_main.txt` 转 `src\menu_main.rc`） |
| **`libtest.cpp` + `build_libtest.bat`** | **支持库层的回归用例**：载入 .fne → 建树 → 断言 1560 个节点/各类计数与原版 dump 一致（脱离 GUI 跑，退出码 0 = 过）；`libtest.exe <libdir> dump` 还能打印全部提示文本 |
| **`cmp_hints.py`** | 把上面 dump 出来的提示文本和原版抓包（`shots\sel_<N>.txt`）逐字节对比 |
| **`treeinfo.c` + `build_treeinfo.bat`** | 跨进程读另一个进程的 `SysTreeView32` 全量内容（32 位；`TVM_GETITEM` 的指针不跨进程 marshal，必须 VirtualAllocEx） |
| **`libprobe.c` + `build_libprobe.bat`** | `LoadLibrary` 一个 `.fne` 并打印 `GetNewInf()` 返回的 LIB_INFO |
| **`dump_dialogs.py`** | 导出 e.exe 的 55 个 RT_DIALOG 模板（类名/ID/样式/坐标/文本全解） |
| **`parse_fne.py`** | `.fne` 的离线静态解析（⚠️ 只能拿到第 0 个数据类型，见 HANDOFF 坑 1；留作参考） |
| **`ida_krnln.py`** | krnln.fne / e.exe 的 IDA headless 查询工具：`base / big / dw / gbk / decstr / decimp / decxref / decname / batch / pat / imm / calloff`…（第 18 轮用它把 krnln 全库伪 C 倒进 `…\krnln\out\`，结论写在 `…\krnln\krnln_anatomy.md`，见 HANDOFF §2.5/§2.5d） |
| **`extract_lib_bitmap.py`** / `match_tree_icons.py` | 抠 krnln 的 `RT_BITMAP LIB_BITMAP`（336x13/4bpp/16 色）并与 `src\res\tree_icons.bmp` 比对。⚠️ 格宽与格序未定死，**暂不替换图标** |
| **`designprobe.c` + `build_designprobe.bat`** | **脱离 GUI 的探针**：`LoadLibrary` 任意 `.fne` → 按 `LIB_DATA_TYPE_INFO+44` 取 ITF 1 → 用 `PFN_CREATE_UNIT(NULL,0,…)` 真建组件窗口，打印类名/样式/矩形 + 库反调宿主的通知码统计。退出码 0 = 全过 |
| **`design_smoke.ps1`** | **设计器冒烟**（五条断言）：支持库建出 `WTWindow`+`_EL_RgnButton`、第三方库组件也放上窗体、组件箱从支持库枚举、属性页从支持库填充 —— 期望五条 `[PASS]` |
| **`libcfg_smoke.ps1`** | **支持库配置冒烟**（七条断言）：对话框 489x464 + 11 个原版控件 id、列出 71 个 `.fne`、查找框过滤、单击翻转勾选框、**运行期动态载入**（工作夹树 1560→1819）、重开显示已勾选、**运行期动态卸载**（回到 1560） |
| **`resize_smoke.ps1`** | **组件缩放冒烟**（四条断言）：拖 SE 手柄把组件放大 40x30、拖 NW 手柄挪原点并缩小 20x10、全程不崩 |
| **`regress.bat`** | **一条龙回归**：把 HANDOFF 第 1 节那串验收按顺序全跑一遍（**28 步**） |
| **`ui_smoke.ps1`** | **第 23 轮 UI 冒烟**（33 条）：① 8 条工具条/面板开关命令真的改可见性并复原；② 6 条「窗口」表页命令把 MDI 文档标题换成页名；③ **整体搜寻**（#32934 / 原版 `DIALOG id=30773`）尺寸+标题+9 个控件 id+两条单选原文，并把 `abc` 填进下拉编辑框按「开始搜寻」→ 状态夹第 5 页真的收到结果且翻到那页；④ `插入→窗口`(#32831) 真建支持库 `WTWindow` 且**任何时刻只有一个窗体可见**。**已接进 regress 第 27 步** |
| **`cfg_smoke.ps1`** | **第 23 轮系统配置 6 页冒烟**（20 条）：判据是倒档 `ECLONE_DUMP_CFG` —— 默认值、对话框控件里的值、改成 12 后**重启仍生效**、「置回默认值」后回到 8。**已接进 regress 第 28 步** |
| **`probe_forms.ps1`** | 手工探针：起 EClone 后发若干菜单命令，打印每个 `WTWindow` 的 hwnd / 可见性 / 标题 |
| **`probe_bartoggle.ps1`** | 手工探针：`<pid> <cmdid>` 发一条工具条/面板开关命令，前后各打一遍各控制条的可见性 |
| **`probe_cfgctl.ps1`** | 手工探针：把系统配置对话框里 12 个控件的文字用 `GetWindowTextA`/`W`/`WM_GETTEXT` **三种读法并排打出来**（证明跨进程只有 `WM_GETTEXT` 管用） |
| **`probe_cmd_dlg.ps1`** | 手工探针：`<pid> <cmdid>` 发一条菜单命令，列出新出现的 `#32770` 及其关键控件（尺寸/类/文字） |
| **`win_tree.py`** | 递归倒一个进程的窗口树（Python + ctypes，`python tools\win_tree.py <pid> [类名过滤] [深度]`） |
| **`menu_missing.py`** | 把「还没有处理函数的菜单命令」倒成 `shots\menu_missing.txt`（UTF-8，避免 GBK 控制台乱码） |
| **`dump_manifest.py`** | 查一个 exe 有没有 `RT_MANIFEST`（= 有没有启用「通用组件库 6.0」视觉样式）。**实测原版 `e.exe` 与本工程都没有** ⇒ 两边都是经典 5.x 外观（证据 `shots\live_progtree\work.png` 的方形页签）。别为了好看去加清单 —— 会把所有经典控件的像素对拍全废掉，也偏离原版 |
| **`probe_design_orig.ps1`** | 起原版 e.exe → `WM_MDIACTIVATE` 到"窗口: _启动窗口"设计器页 → 1:1 拍 `WTWindow` 与文档视图（设计器配色/点阵/手柄的**基准图来源**） |
| **`probe_repaint.ps1`** | **判断"这块像素是谁画的"**：屏幕品红盖住目标客户区 → 只 `InvalidateRect(目标,TRUE)` → 抓屏。⚠️ 判断自绘归属**只能用这个方法**（`probe_formpaint.ps1` 的 WM_PRINTCLIENT 品红 DC 对 krnln 的窗口无效） |
| **`probe_libcfg.ps1`** | 起原版 → `PostMessage WM_COMMAND 32906` → 拍「支持库配置」对话框 + 倒控件表（`shots\live_libcfg\`） |
| **`probe_wndproc.ps1`** / **`probe_classbrush.ps1`** | 看窗口过程/类背景刷归属于哪个模块（实测原版与本工程的 `WTWindow` 类 `WndProc` 是同一个地址） |
| **`probe_orig.ps1`** | 启动原版 e.exe、展开树、截整窗、可选 `-CmdId` 打开某个菜单命令 |
| **`edit_smoke.ps1`** | 编辑器交互冒烟：起 EClone -> 按类名找自绘编辑器视图 -> 打一次点击 + 若干按键 -> 确认进程没崩 + 截图 |
| **`fold_smoke.ps1`** | **子程序收缩冒烟**：读 `ECLONE_DUMP_MODEL` 的模型算出折叠标记坐标，`PostMessage` 点它，再发 `EC_WM_REDUMP`(WM_USER+102) 重倒模型，比对行数 —— 期望 `[PASS] collapse works` |
| **`scan_runs.py`** | 逐行 run-length 扫描：给定颜色，打出每行的精确 x 区间 —— 量原版流程线 / 三角 / 网格就靠它 |
| **`probe_progtree.ps1`** | 起一份原版 e.exe → 点工作夹「程序」标签翻页 → 用 `treeinfo.exe <pid>` 倒出「程序」页树的全部节点（文字/image index/层号）+ 1:1 截图。**程序树构成的唯一依据** |
| **`extract_progtree_icons.py`** | 从 `shots\live_progtree\tree0.png`（原版实测）与 `shots\ref_prog_tree.png`（用户截图）抠出 15 个 16x16 程序树图标 → `src\res\progtree_icons.bmp` |
| **`dll_smoke.ps1`** | **Dll命令页冒烟**：点工作夹「程序」标签 → 点程序树第 4 行(Dll命令) → 确认编辑器切到 Dll 文档（模型倒档里出现 Dll 表）+ 两张 1:1 截图；末尾倒出程序树做核对 —— 期望 `[PASS] program tree selection switched the document page` |
| **`check_dll.py`** | **Dll命令页结构断言**：把 1:1 编辑器截图按「Dll命令块」的实测结构逐项断言（行序/竖线条数/底色/行距/块间 8px/表头备注列=数组列+备注列/四种文字色）。**同一份断言同时跑在原版截图和本工程渲染上** |
| **`tabs_smoke.ps1`** | **面板分页回归**：用 `VK_RIGHT`/`VK_HOME` 走标签，逐页断言 `cursel` **和** 实际可见页（工作夹 3 页 + 状态夹 8 页）—— 期望 `[PASS] pane tab paging works` |
| **`diff_png.py`** | **逐像素对照两张 1:1 截图**：打印差异像素数/颜色对/连通块包围盒，并生成"上=A 中=差异 下=B"的对照图；`--max=N` 可当断言用（差异超过 N 就退出码 1）；**`--ashift=N` / `--bshift=N`** 用来对齐内容偏 N 行的截图（用户给的 crop 比真客户区低 4px，比它要 `--ashift=4`） |
| **`rows_scan.py`** | 打印一张 1:1 编辑器截图的横线 y 列表 + 每行的竖线位置（判断表头/数据/代码行分带，量行距/列宽用） |
| **`marker_scan.py`** | 扫出流程槽里的 `-`/`+` 折叠标记与流程箭头：包围盒、点阵 ASCII、前景色、左右底色 |
| **`px_map.py`** | 把一小块按颜色打成字符画（认标签条梯形、✓、冒号、箭头这种形状） |
| **`tree_section.py`** | 从 `treeinfo.exe` 的整档里按内容挑出某棵树的文本（如 `程序数据`） |
| **`tree_row.py`** | 算出某节点在"只展开根"的默认树里的**可见行号**（`probe_orig_page.ps1` 靠它点节点） |
| **`cmp_tree_icons.py`** | 把原版与本工程的程序树 1:1 截图逐行抠图标并排对照 |
| **`patch_sample_graphic.py`** | 把采样源码换成 **graphic.e** 那 6 行（原版 `shots\live_tabs\view0.png` 就是它的 1:1），用来对拍代码区几何；跑完用 `restore_sample.py` 还原 |
| **`flow_scan.py`** | 逐行扫 1:1 编辑器截图的「流程槽 + 流程线」：每行非白像素的 run + 颜色分类（`B`=藏青 `d`=灰 `#`=黑 `g`=填充）。流程线的 3 亮 3 灭 / ►► / ◆ 全靠它量 |
| **`cellpos.py`** | 从模型倒档算出某个表格单元格的像素中心（`cell_smoke.ps1` 用它定位点击） |
| **`cell_smoke.ps1`** | **单元格编辑冒烟**：块尾关键字行不画 + 点 `_图形按钮1_被单击` 的公开格（断言模型里那个格出现勾选）+ 双击子程序名格改字回车（断言模型里出现新名字），三条 `[PASS]` |
| **`param_smoke.ps1`** | **参数归属线冒烟**：从模型倒档里找 `par>0` 的代码行（不硬编码像素带）→ `WM_VSCROLL` 滚到可见 → 点流程槽里的 `-/+` → 断言 `paramshow` **0→1→0**、且**第一行参数**那条带的墨迹真的从 0 涨上去再落回来（`[PASS] parameter lines expand and collapse`） |
| **`sel_smoke.ps1`** | **编辑器交互冒烟**（第 17 轮新增）：点选定位 / 鼠标框选 / 选区涂 `#000080` / `Ctrl+A` 全选 / `Ctrl+C` 复制 / `Ctrl+X` 剪切 / `Ctrl+V` 粘贴 / 滚轮 —— 八条 `[PASS]`。鼠标与按键都用 `PostMessage` 发（`keybd_event` 只用来按住 Ctrl，因为 `GetKeyState` 不认 PostMessage）。断言靠 `<dump>.fold.txt` 里的 `anchor,caret` / `scroll` / `lines` |
| **`probe_sel.ps1`** | 起原版 e.exe + 指定 `.e`，点进代码编辑器、`Ctrl+A`，拍 1:1 并打印剪贴板格式（⚠️ 有模态警告框时会失败，量选中态请用 `probe_drag.ps1`）|
| **`probe_drag.ps1`** | 起原版 + 指定 `.e`，**用鼠标真拖选**一段再拍 1:1 —— 选区配色（`#000080` 底 + 白字）的唯一来源 |
| **`redump.ps1`** | 起 EClone（带 `ECLONE_DUMP_MODEL`）、找到自绘编辑器、`PostMessage` `EC_WM_REDUMP`、退出；产出 `model_dbg.txt` + `model_dbg.fold.txt`（`scroll` / `anchor,caret` / `lines` / `fold row` / `param row`）|
| **`param_cols.py`** | 量一行文字的**逐字符 x 落点**（连续有墨的列分成块），用来跟原版对「参数行第几个字落在哪一列」 |
| **`check_block_comments.py`** | **构建守卫**：扫 `gbk\` 里转换后的源码，按 C 规则跳过字符串/字符字面量与行注释，再要求 `/*` `*/` 配平。`build.bat` 每次都会跑；不通过直接 `BUILD FAILED`。它挡的是「`//` 注释里的中文标点转 GBK 后夹出 `*/`，把后面某个块注释提前关掉、静默吃掉下一条语句」这个坑（HANDOFF 3.1.2-14） |
| **`build_argprobe.bat` + `argprobe.exe`** | 打印支持库里某个命令的**参数名/类型/是否有默认值**（`argprobe.exe "<e>\lib" 信息框`）。参数归属线的参数名就靠它核对。GBK 副本与 obj 都写 `gbk\probe\`，不污染主构建的 `cl *.cpp` |
| **`patch_sample_flow.py`** | 把采样换成「代码区就在第一屏」的短流程样例，用来拍流程线（测试脚本发的滚动消息对本编辑器无效） |
| **`save_sample_backup.py`** | 把**当前** `g_szSampleCode` 存成 `shots\zoom\sample_backup.txt` —— 改过采样源码之后必须跑，否则下次 `restore_sample.py` 会把你新加的东西吃掉 |
| **`probe_live.ps1`** | 起一份原版 e.exe + 指定 `.e`，倒窗口树 + 拍整窗/每个编辑器视图 1:1，**并拍文档标签条(`CCustomTabCtrl`) 1:1** |
| **`probe_fold_orig.ps1`** | 起原版 + 指定 `.e`，自动在流程槽里找到 `-/+` 标记、点它、拍前后 1:1（`view_before/after.png`） |
| **`probe_orig_page.ps1`** | 起原版 + 指定 `.e`，按文字点程序树节点（如 `全局变量`）、拍该文档页 1:1（`tree_row.py` 算行号） |
| **`px_dump.py`** | 把一个区域按颜色打成 ASCII 点阵（看 ✓ 的形状、冒号的点、引号的笔画这种级别就靠它） |
| **`code_shot.ps1`** | 给**代码页**拍 1:1（`shots\code_view.png`，1195x394），用来跟 `shots\user_ref.png` 对照 |
| **`gen_masks.py`** | 从原版截图里抠出 ✓(9x7) 与红铅笔(14x14) 的**精确点阵**，打印成 C 数组贴进 `ec_editor.cpp` |
| **`font_probe.py`** | GDI 字体探针：换字体/字号/quality/outprecision 渲染一个字符，看字形点阵（本轮用来确认"标点更粗"不是简单字体参数问题） |
| **`ida_font.py`** | 用 **idalib** 查原版 e.exe：`fonts` 列出 CreateFont* 调用点，`<地址列表>` 反编译函数，`find <hex字节>` 按字节找串 |
| **`probe_mark.ps1`** | 起原版 e.exe → 截图 → 点一下 → 再截图，用来判定流程槽那个红铅笔标记的行为（结论：文档刚打开时画在插入点行，首次交互后消失） |
| **`scan_small_bmps.py`** | 列出 e.exe 的 RT_BITMAP/RT_CURSOR/RT_ICON 里的小尺寸资源（找自绘小图标用） |
| **`syscfg_probe.ps1`** | 起 EClone -> `PostMessage` 命令 32876 打开「系统配置」-> 倒出对话框控件树 + 颜色配置下拉的全部条目 + 各复选框状态（**判定对话框是否真的工作**） |
| **`syscfg_shot.ps1`** | 同上并截图 `shots\syscfg\ec_syscfg.png`；`-ComboSel N` 可以先切到第 N 个配色 |
| **`theme_e2e.ps1`** | 配色端到端：开系统配置 -> 选第 N 个配色 -> 确认 -> 截图；再**重启一次**截图验证注册表持久化 |
| **`check_theme.py`** | **配色回归检查**：吃 `ECLONE_DUMP_THEME` 倒出的文本，逐槽比对内置默认配色是否等于原版实测值，并检查显示选项合法 —— 期望 `80 passed, 0 failed` |
| **`clr_dump.py` / `clr_dump2.py`** | 解 `C:\Program Files (x86)\e\clr\*.clr`：`clr_dump.py` 把每个 DWORD 映射到 e.exe 里的颜色项名单；`clr_dump2.py` 把 13 个配色并排铺开（用来推字段序） |
| **`dlg_tree.py`** | 按几何包含关系把 e.exe 的某个 `RT_DIALOG` 打成缩进树（原版模板是平铺的，面板结构只能靠坐标推） |
| **`dump_rdata.py` / `scan_gbk.py`** | 按字节走 NUL 分隔串 / 按正则扫 GBK 串（**IDA 的库没有 .rsrc，`idautils.Strings()` 也扫不到 GBK**，只能这样读） |
| **`crop2.py` / `fills.py`** | 裁图放大 / 统计一张图里所有纯色填充及其包围盒（量原版配色就靠它） |
| **`probe_live.ps1`** | 用指定 `.e` 启动**自己的一份**原版 e.exe，导出窗口树 + 对整窗和每个编辑器视图做 1:1 `PrintWindow` 截图（编编辑器的像素基准靠它） |
| **`probe_pid.ps1`** | 只读地 dump **已经跑着的**那个 e.exe（PID）的窗口树（用户自己开着的实例也能读，不做任何交互） |
| **`restore_sample.py`** | 把 `shots\zoom\sample_backup.txt` 里的 `g_szSampleCode` 还原回 `EClone.cpp`（`patch_sample_ref.py` 的反操作） |
| **`patch_sample_ref.py`** | 一次性：把采样源码换成用户截图里那段（精易模块.e / 集_组件），用来逐列核对表格几何 |
| **`build_map.bat`** | 重新链接并生成 `shots\EClone.map`（EClone 崩了用它把 fault offset 映射回函数名） |

## 已复刻（与原版逐项对照）

| 项 | 原版 | EClone |
|---|---|---|
| 主窗口类名 | `ENewFrame` | 相同（自己注册同名类） |
| 窗口尺寸 | 134,57 1440x753 | 相同 |
| 菜单 | e.exe `RT_MENU` id=127 | 逐条照抄（137 条命令）；菜单条形态不同（原版被 iDraw 改成自绘停靠条） |
| 菜单条最左端的"易"图标 | MDI 子窗口最大化时 Windows 画在父窗口菜单条最左端 | 相同（图标取自 e.exe 的 `RT_GROUP_ICON 127`，框架+子窗口都设了 WM_SETICON） |
| 每行最前面那根竖杠 | MFC 的 control bar gripper（`CBRS_GRIPPER`） | 相同（四个条都设了，像素位置一致：截图 x=12） |
| 停靠面板 | `AfxControlBar42s` 外壳 + `Afx:400000:8:...` 面板窗口 | 相同（外壳 CDockBar + sizecbar 面板，窗口类同形） |
| 面板内部 | `#32770` + `SysTabControl32` | 相同，且客户区偏移 (5,17)/(17,5) 与原版实测一致 |
| 面板标题行 | 两条凹线 + □ + ×（11x11，间距 13） | 相同（□ 按原版位图 596 点阵自绘，× 用 sizecbar 的 Marlett) |
| 工作夹 | 3 页：支持库/程序/属性，**标签在底部** | 相同。支持库页是**运行期真载入 krnln.fne** 生成的树：**1560 个节点**（支持库→命令类别→命令 / 数据类型→成员命令·成员属性·组件事件·子数据成员 / 常量），节点文字与层级与原版 dump 逐项一致 |
| 状态栏 | `msctls_statusbar32`，选中支持库树的节点时显示该节点的提示（调用格式/英文名/解释/参数/操作系统需求…） | 相同，24 个抽样节点的提示文本与原版**逐字节相同** |
| 状态夹 | 8 页：提示/输出/…/剪辑历史，**标签在顶部** | 相同 |
| 窗口组件箱 | 基本/扩展组件 + 树，默认收起 | 相同（43 个组件名取自逆向） |
| 工具条 | 3 行可见：标准 534x25(23 钮) / 对齐 357x25(14 钮,灰) / 定位 519x25(默认收起) | 相同 + 第 3 行换成两个下拉框（原版第 3 行就是"定位条 + 两个下拉框"） |
| 子程序/事件选择器 | ComboBox 15,99 200x20 与 215,99 300x20（主框架上） | 相同 |
| 状态栏 | `msctls_statusbar32` | 相同 |
| MDI 子窗口类 | `AfxMDIFrame42s` | 相同 |
| 文档标签条 | 窗口类 **`CCustomTabCtrl`**，1195x22，挂在 MDI 客户区**下侧**（紧贴状态夹上方）；标签是**下边内缩 8px 的梯形**、上边互相重叠 9px；条底 `#F0F0F0`、顶 `#A0A0A0`+`#696969`、底 `#E3E3E3`+白；选中=白面黑粗体字、未选中=`#000080`；**没有 × 关闭按钮**；文字 `_启动窗口` / `启动窗口程序集` | 相同：`CECTabStrip` 挪到 MDI 子窗口客户区下侧并按上面这套几何自绘；对照图见 `shots\cmp_tabbar4x.png`，1:1 基准 `shots\live_tabs\tabbar0.png` |
| 设计器 | 真窗口 `WTWindow`(380x250) + `_EL_RgnButton`(104x112)；工作区 `#A0A0A0`、窗体客户区"纸" `#F0F0F0` + **8px 黑点阵**（锚在窗体客户区原点）；选中框 = 外扩 3px 的 1px 黑点线 + 8 个 **7x7 白框**手柄 | 相同：窗体与组件都**由支持库真建**（`ITF 1`，`blInDesignMode=TRUE`），纸/点阵/框/手柄由覆盖层 `ECDesignOverlay` 代画（见 HANDOFF §2.7a）；可放置/选中/拖动/**拖手柄缩放**/Delete。基准 `shots\live_des_orig\{form0,view2_client}.png` |
| 编辑器 | **全部自绘的结构化编辑器**（原版编辑器窗口里没有任何子控件，实测）：程序集头表格 / 子程序头表格 / 参数表 / 变量表 + 代码区 + 流程槽 + 流程线 | 相同：`src\ec_editor.cpp` 自绘。**流程线第 17 轮按原版自带例程 1:1 重做**：竖虚线在「上一级正文列 + 1」（级别 1 → x=47）、**3 亮 3 灭** `#808080`，从块首行中线拉到块尾行中线；块首行一条 3/3 横虚线（循环类 x..x+15、如果家族 x+2..x+15）；**循环类的块首行**末端一个 **►**（4x7 灰三角，x+12..x+15）；**块尾关键字不可见时**（如果真结束/如果结束/判断结束）竖线多延一格、末端一个 **▼**；块体第一条直接语句上两个 **►►**（藏青，x-14 / x-10）。子程序体第一条语句的流程槽里有 **↓**（x=9，藏青）。基准 `shots\live_flowcmd\view0.png`（原版自带 `流程类命令.e`），扫描工具 `tools\flow_scan.py` |
| **流程线画哪些行** | 块首关键字（如果/如果真/判断开始/计次循环首/判断循环首/循环判断首/变量循环首）**和四个循环尾**（计次循环尾/判断循环尾/循环判断尾/变量循环尾）都照画；只有 `如果结束/如果真结束/判断结束/否则/默认` 不画 | 相同：`g_szHidden[]` 只留这 5 个（第 15 轮误把 4 个循环尾也塞进去，第 17 轮对着原版例程改回）。断言：`tools\cell_smoke.ps1` → `[PASS] block-end keywords: end-suffix hidden, loop-tail rendered` |
| **缩进规则** | 原版**不是**「每进一层块就深一级」：块首行 = 外层 +1 级，**块体的语句和块首行同一级**，可见的块尾行也和块首同级，块结束后回到外层 | 相同：`Rebuild()` 里 `nContext`（当前块体级别）+ `blkStack`（块首行号）。逐行量正文左边缘得到（三个原版自带样本互证，见 HANDOFF 2.1e）：`' 注释` 46 / `.计次循环首` 68 / `.如果真` 90 / `到循环尾` **90** / `画板1` 68 / `.计次循环尾` 68 |
| **块首/块尾配对** | 用**源码里的关键字栈**配对；块尾关键字被隐藏时，竖线按「块体最后一行 + 一行」收在 ▼ 上 | 相同：`Rebuild()` 里 `blkStack` + `EcRow::blkEnd/blkLast`（旧那个按缩进推边界的 `ComputeBlocks()` 已删 —— 它会把 `返回 ()` 吞进循环体、竖线拖到文档尾） |
| **鼠标选中 / 复制粘贴 / 滚轮** | 点击定位插入点、按住左键框选（选中部分 `#000080` 底 + 白字、高度 = 文字 12px）；`Ctrl+A/C/X/V`；右键菜单；滚轮一格 3 行 | 相同：`CaretFromPoint()` / `CaretRect()` 的正文左边缘改用 `TextXOfIndent()`（以前写成流程槽的 `EC_FLOW_X()`，偏 22px → 点哪儿都跑到行尾、框选选不出东西）；`DrawAll()` 先铺选区底、`TokenizeDraw()` 把选中那段画成白字；剪贴板走 Win32 `CF_TEXT`（GBK）；`OnContextMenu` 运行时建菜单。断言：`tools\sel_smoke.ps1` → 八条 `[PASS]` |
| **参数归属线**（语句下面那些 `※参数名：值`） | 每条带参数的支持库命令调用，在语句下面按参数画行：`※提示信息： "…"` / `※按钮： 0` / `※窗口标题：`，左侧一整条灰色 **3 亮 3 灭**连接虚线（顶端一个 ▼）+ 每行一条横虚线；`※` 藏青、参数名黑、值走词法着色。**默认收起**，语句行流程槽里一个 `-/+`，点它整篇显示/隐藏 | 相同：`src\ec_editor.cpp` 的 `DrawParamLines()` + `DrawAll()` 末尾那个 `-/+`（`m_bParamShow`）。几何逐像素量自原版 1:1 `shots\live_fold\view_before/after.png`：第一行参数行顶 = 语句行顶 + 16、参数行距 **16**、`※` 左边缘 = 语句正文列 + 44、竖虚线 = 正文列 + 31。行数 = **实际写了的参数槽数**（`信息框（"…", 0, ）` 画 3 行），参数名来自支持库（`EcCmdArgsByName`）。断言：`tools\param_smoke.ps1` → `[PASS] parameter lines expand and collapse`；逐字符列位置 `tools\param_cols.py` 与原版逐块相同 |
| **单元格就地编辑** | 点「公开/传址/数组/静态/可空/参考/易包」格直接改源码；双击文字格临时建一个 `EDIT` 子控件，回车/失焦提交、ESC 取消 | 相同：`HitTestCell`（列语义取同表同格数的最近表头行）+ `ToggleFlagCell` / `BeginEditCell`+`CommitEditCell`。冒烟：`tools\cell_smoke.ps1` → `[PASS] flag cell toggled` + `[PASS] in-place text edit committed` |
| **流程槽里的 `-/+`（参数归属线开关）** | 只画在**带参数的支持库命令**语句上 | 相同：判据 = `EcParseCall` 成功 + 至少一个非空实参 + 支持库查得到参数名。⚠️ 第 17 轮修掉一个真 bug：`EcRow::bParam` 从来没初始化（POD，读到栈垃圾 `4402570`），于是**每一行**都挂上了 `-/+` —— 用户截图里那排满屏的藏青横杠就是这个 |
| 编辑器配色 | 原版「工具→系统配置→程序显示」里有 **颜色配置** 下拉，列出 `<e>\clr\*.clr`（13 个） | 相同：菜单 工具→系统配置（命令 #32876，对话框 id=282）→ 程序显示 → 颜色配置；默认配色逐槽等于原版实测值，可换 `.clr`、可导入/保存，选择记在 `HKCU\Software\FlySky\E\EClone40` |
| **Dll命令页** | 点程序树里的 `Dll命令` 打开一个文档页，内容 = 一串「Dll命令块」：`Dll命令名/返回值类型/公开/备注` 表头 + 数据行 + `库文件名:` / `在库中对应命令名:` 两对整行格 + `参数名/类型/传址/数组/备注` 参数表 | 相同：列位置与原版截图**逐像素一致**（第一块竖线同为 `44,212,284,320,440`、宽度 `168,72,36,120`），行距 21 / 块内末行 20 / 块间 8、底色 `#E6EDE4`、四种文字色、`✔` 居中都与原版实测相符。对照图 `shots\cmp_dll_page.png` |
| **「程序」页程序集树** | 程序数据 → **文件夹**(黄) / 程序集(菱形，窗口与普通程序集**同一图标**) / 类模块(与自定义数据类型**同一图标**) / 子程序(带↓)；窗口；**全局变量**(圆柱，有内容才出现，子项同图标)；**自定义数据类型**(有内容才出现)；Dll命令(带↓)；常量表…；资源表(声音/图片或图片组)；模块引用表(带蓝↓)；外部文件记录表 | 相同：节点构成/图标/顺序都按原版实测两套样本定死（`shots\live_jy\prog_tree.txt` 5189 节点 + `shots\live_progtree\prog_tree.txt` 11 节点）。本工程倒档 `shots\live_progtree_mine\prog_tree_u8.txt`（23 节点）、1:1 截图 `tree0.png`。⚠️ 原版**每个工程一套图片号**，别按号跨工程对 |
| **文档分页** | 窗口 / 程序集 / Dll命令 … 各是一个文档页，点程序树节点切换 | 相同：3 个文档页（`_启动窗口`(设计器) / `启动窗口程序集` / `Dll命令`），点标签或点程序树节点都能切 |
| **逐像素对齐** | 原版 1:1 基准：`shots\live_tabs\view0.png`（graphic.e 代码页，我自己拍的真客户区）+ `shots\live_tabs\tabbar0.png`（标签条）+ `shots\ref_dll_page.png`（用户给的 Dll 页 crop） | Dll命令页 **0 像素差异**；代码页与原版 1:1 基准 **0.62%**（只剩备注列宽与字体字形）。⚠️ 用户给的那两张 crop **比真客户区低 4px**，比它们要 `--ashift=4`。本轮据此修掉：`EC_TOP_PAD` 8→4、正文基准 69→46（+22n）、子程序体入口 ↓ 箭头（以前只设标记不画）、收起行的浅蓝底（原版无）、程序集表头 `#ECECEC`、以及代码页左边界/✓/冒号等第 13 轮的 5 项 |
| 显示开关 | 原版「程序显示」页 7 个复选框 + 语句行间距 + 字体 | 相同：分支流程线画为虚线 / 表格线画为虚线 / 画表格线 / 画分隔线 / 画程序分支流程线 / 画参数归属线 / 显示参数名称 + 行距 + 字号 |
| 树/标签页图标 | 支持库树 16x14、标签页 16x16 | 相同（标签页取自 e.exe 位图；树与工作夹标签从原版截图抠） |

## 未做（与"1:1"的差距）

1. **起始页**（内嵌 IE 渲染 HTML）—— 按用户要求不做。
2. **iDraw 的三样**：自绘菜单条（30px 停靠条，带加速键）、Scintilla 输出面板、ExUI 皮肤 —— 用户明确先不做。
3. **支持库树根节点多一个"−"展开框**（原版根节点无框、子项少内缩 16px；两边树控件的 style 位完全一样，
   不是样式问题）。现象对照 `shots\cmp_tree_orig_left.png` vs `shots\cmp_rootbox_mine.png`。
4. **树节点图标只有根和类别**：命令/属性/事件/子数据成员/常量都还没有图标。
   方案已实测确定（HANDOFF 第 5 节）：图标号 = 库 `LIB_BITMAP` 格号 + 9，固定号 0..9 另算。
5. **类别显示顺序与原版差 3 项**（`时间操作/数值转换/字节集操作` 的相对次序），原因未查明。
6. **编辑器已改成全自绘的结构化编辑器**（`ec_editor.cpp`，窗口类仍是 `Afx:400000:8:10005:6:0`，与原版
   `Afx:400000:b:10003:900015:0` 不同类名）。已复刻：五类表格（程序集/子程序/参数/变量/**Dll命令块**）+
   列宽自动适配 + 表头填充 + 语法着色 + 流程槽 + 流程控制虚线 + 光标/**鼠标框选**/键入/滚动/
   **Ctrl+A C X V + 右键菜单**。**还没做**：`Shift+方向键` 扩展选区、撤销、查找替换、多行备注。
7. **「全局变量 / 自定义数据类型 / 类模块 / 常量表 / 资源表 / 模块引用表 / 外部文件记录表」还没有各自的文档页**
   （程序树的节点已按原版的"非空才出现"规则建好，但点了不切页；`item data = -1`）。
   探针已经备好：`tools\probe_orig_page.ps1 -File shots\e_samples\jy.e -Node 全局变量`（先拍原版基准再动手）。
8. **子程序体第一条语句那个 `+/-`（参数归属线的展开/收起）已做**（`tools\param_smoke.ps1`）；
    还没实测的两处：嵌套调用里**内层命令**的参数行、以及展开态下**最后一条语句**的参数行会不会伸出内容区。
9. **流程线本体已按原版自带例程 1:1 重做**（第 17 轮，见上面「已复刻」表）。**还差 1 个反例**：
   原版第 6 个块（块体只有一条简单语句、块结束后面还跟一条语句）把 `►►` 放在**块结束后那一行**，
   本工程放在块体那条语句上 —— 规则没定死（详见 HANDOFF 第 6 节）。
9b. **「多行注释块」没做**：原版把连续的多行注释画成一个**绿框**（每行左边缘一条绿色实竖线 x=63、
    首末行各一条绿色 3 亮 3 灭横虚线拉到正文右边）。证据 `shots\live_fold_jy\view_before.png`
    （原版打开精易模块.e：`模块开发/模块说明/使用注意/特别声明/使用声明` 6 段全是绿框，绿像素 21957 个）；
    而 `shots\live_tabs\view0.png` 与用户截图 `shots\user_ref_code2.png` 里绿像素都是 0 —— 判据待定。
    本工程只把 `'` 之后整行染成注释色，不画框。
10. 设计器：能放置/选中/拖动/**缩放**/Delete/**新建窗体**（`插入→窗口` #32831 真建支持库 `WTWindow`，
    程序树里点不同窗口节点切窗体 —— 第 23 轮做完）。**还没有**：每个窗体一个独立文档页
    （原版是"一窗体一页"，本工程所有窗体共用 `ECDOC_FORM` 一个页）；`.e` 打开/保存。
11. 工具条按钮无功能（仅保证不置灰）；**「窗口」菜单里那 8 条工具条/面板开关是好的**（第 23 轮）。
12. 面板 □ 按钮做成"收起/还原"，原版语义未验证。
13. `工具→支持库配置`（菜单 #32906）**已做**：对话框照抄原版 `RT_DIALOG id=321`（489x464 px），
    能运行期动态载入/卸载 `.fne`。已知取舍见 HANDOFF §5 第 13 条（未载入的库只显示文件基名；
    「删除」不删磁盘文件；查找只支持名字子串）。
14. 「系统配置」**7 个页签都已做**：第 22 轮把 78 个控件照原版模板 1:1 摆好，第 23 轮把**另外 6 页**
    的 34 个控件接上"真的读/写/持久化"（`EcCfg` + `HKCU\…\EClone40`）。其中**真有行为**的只有 3 项
    （设计器网点 / 新建窗体尺寸 / 编辑器撤消栈上限）—— 原版其余那些是给**编译器和生成程序**用的
    （备份文件、作者信息、调试输出名、清单、编译开关、花指令/打乱码），本工程没有编译器。
15. 设计器的"纸"色硬编码 `COLOR_BTNFACE`（原版取窗体**底色属性**）—— 本工程窗体是 `NULL` 数据建的，
    底色字段是垃圾，等能打开 `.e` 再改成读属性。
16. **「整体搜寻」(`#32934`) 已做**（第 23 轮，原版 `RT_DIALOG id=30773`）：结果写进状态夹的
    「搜寻1/搜寻2」页（那两页是**输出夹**）。**还没做**：两个下拉的条目文字没基准（自己填的）、
    「搜寻范围」只记值不接行为（总是全篇扫）。
17. **菜单覆盖率**：137 条命令里 `handled by ON_COMMAND` **45 条**（第 23 轮 +7），
    `python tools\menu_coverage.py` 随时可查；没做的 86 条见 `shots\menu_missing.txt`。

## ⚠️ 踩过的坑（重来时别再踩，详见 HANDOFF.md 第 3 节）

1. **`edit`/`write` 工具改 `.bat` 会变 LF-only**，cmd 报的错完全误导 → 改完必须转回 CRLF。
2. **`CToolBar::LoadBitmap` 让 comctl32 自己猜图标单元高度**（e.exe 的位图只有 15 行）→ 图标整体歪 2 行。
   正解：自己建 `CImageList(16,15)` + `TB_SETIMAGELIST`。
3. **工具条位图别用 PIL 存**（会变 24bpp）→ MFC 的系统色映射算错 → **按钮全变灰方块**。原样写 DIB。
4. 本机**没有 `pwsh`**，用 `powershell -NoProfile -ExecutionPolicy Bypass -File`。
5. VC6 的 `INCLUDE` 要加 `%VC98%\ATL\Include`（sizecbar 要 include `<afxpriv.h>`）。
6. **原版截图坐标 ≠ 窗口树 dump 坐标**（原版 +8；本工程 -134,-57），量之前先校准。
7. sizecbar 里 `bHorz` = "上下停靠"（此时 gripper 竖着在左边），名字极易看反。
8. `CRect` 在 MFC 4.2 没有 `(CPoint,CSize)` 构造；`CControlBar::OnUpdateCmdUI` 是纯虚；VC6 没有 `I_IMAGENONE`。
9. `e.exe` 的位图 146 解出来是坏的（不是工作夹标签图标）→ 那三个图标从原版截图抠。
10. **状态夹标签在顶部、工作夹标签在底部**（上一轮文档写反了）。
11. MFC 的 `SetHeight(n)` 实际得到 `n+2` 的行高（要 25 就写 23）。
12. 上一轮那 15 条仍然有效：RC 只认 CRLF、RC 不能嵌套 include、源码 GBK+无BOM+CRLF、`.bmp` 不能文本转码、
    `rc /fo` 不能带 `/nologo`、透明色是洋红 (255,0,255)、`C:\Program Files (x86)\e` 不可写。
13. **`.ps1` 里连中文注释都不能有**（不只是字符串常量）：本机按 ANSI(GBK) 读 .ps1，一个 UTF-8 中文
    注释的**末字节**可能被当成 GBK 前导字节，把**下一行吃掉** —— 现象是"某个分支静默不执行"，
    或解析器在**后面的行号**上报别的语法错。检查：`python -c "print(sum(1 for x in open(r'X.ps1','rb').read() if x>127))"`
    必须是 0。要比较中文就用 `[char]0x5DE5` 拼。
14. **`ON_NOTIFY(code, id, fn)` 里的 `id` 是"控件 ID 范围"（nID..nLastID），写 0 = 范围 0..0，
    任何控件都匹配不上** —— 处理器**永远不会被调用**，但控件自己的通知确实发出去了
    （在宿主对话框的 `WindowProc` 里能看到 WM_NOTIFY）。正解：逐条登记真实控件 ID。
    本工程曾因此让工作夹/状态夹的标签页点击全废（`cursel` 会变、内容页不换）。
15. **倒档文件是 GBK**（`ECLONE_DUMP_MODEL` / `ECLONE_DUMP_THEME`），读的时候一律 `encoding="gbk"`。
16. **`treeinfo.exe` 支持直接传数字 pid**（按进程名找会撞上用户自己开着的 e.exe，报 `OpenProcess failed 5`）；
    它倒出的是**所有** SysTreeView32（含隐藏的），按 `items=` 挑；它还会 `TVM_EXPAND` 展开整棵树，
    所以**要点击就点完再倒档**。
17. **`EnumChildWindows` 的顺序是 z-order，不是创建顺序** —— 工作夹里两个树谁先枚举出来不一定，
    要区分就用 `GetDlgCtrlID` 或可见性。
18. **像素断言别在 `cell+3` 采底色**（编辑器插入点光标正好压在那儿，会偶发读成黑色）→ 取一小段的众数颜色。
19. **`PrintWindow` 出来的编辑器图带滚动条**，扫网格竖线时右边要留 ~24px，否则滚动条上的灰像素混进来。
20. **link 报 `LNK1104: cannot open ...EClone.exe` = 有 EClone 进程还在跑**；先 stop-process 再 build，
    否则 build 失败但你接着截图会截到**旧 exe**。
21. **⚠️⚠️ `//` 行注释里的中文标点转成 GBK 后可能夹出块注释的结束符 `*/`**，它会把**文件后面**
    某个 `/* … */` 提前关掉 —— 于是块注释下面**第一条语句被吃进注释里**，**编译不报错、行为不对**。
    真踩过：`ec_editor.cpp` 里一句 `// 易语言的括号是全角的：a3a8「（」…` 让紧跟块注释的
    `if (s[i] != 0xA3 …) return FALSE;` 整句消失，症状是"参数行一行都不画"，
    而日志/字节看着全都正常。**正解**：`build.bat` 每次自动跑
    `python tools\check_block_comments.py gbk`（跳过字符串与行注释后要求 `/*` `*/` 配平），
    不通过直接 `BUILD FAILED`；行注释里不要写全角方括号那类字符。
22. **`.ps1` 里连中文注释都不能有**（同第 13 条，第 16 轮又踩）：`param_smoke.ps1` 带了中文注释后
    某个函数**静默返回 `$null`**，报错完全不指向原因。写完一律查非 ASCII 字节数为 0。
23. **PowerShell 函数参数别用 `[string]` 收数组**（会被拼成一个长串，正则全不匹配）→ 写 `[string[]]`；
    另外 `-match A -and -match B` 之后 `$Matches` 只剩最后一次的结果，要用 `[regex]::Match()` 各自取组。
24. **控制台探针（argprobe 那种）的 GBK 副本别放到 `gbk\` 根目录**：`build.bat` 是 `cl *.cpp`
    全编译，会出现 `LNK2005: _main already defined`（和 libtest 撞）→ 放进 `gbk\probe\`。
25. **⚠️ 自造的「文本形式 `.e`」原版打不开**（第 17 轮又试了一次，连原版自带的 `dll_sample.e` 也打不开）：
    `probe_live.ps1` 报 `views=0`、标题里没有文档名。**别再浪费时间去造 `.e`** —— 要用原版当基准就拍
    **原版自带的例程**：`C:\Program Files (x86)\e\lib\krnln\samples\*.e`、
    `C:\Program Files (x86)\e\samples\经典数学算法集\*.e`（85 个算法例程全带循环/分支）。
26. **⚠️ `.ps1` 里一个非 ASCII 字节都不能有**（第 22 条，第 17 轮又踩）：给 `cell_smoke.ps1` 写中文提示串后
    输出直接变乱码。写完查 `python -c "print(sum(1 for x in open(r'X.ps1','rb').read() if x>127))"` 必须是 0。
27. **⚠️ PowerShell 里 `LP` 是 `Out-Printer` 的别名**：拿 `LP` 当"打包 lParam"的函数名会变成打印机命令报错。
    辅助函数名别用 2 个字母。
28. **⚠️ `ECLONE_DUMP_MODEL` 有两个输出文件**：`<path>` 是显示模型（row/par），
    `<path 改后缀>.fold.txt` 才是 `scroll=` / `anchor,caret` / `fold row=` / `param row=`。
29. **⚠️ `PostMessage` 出来的 `WM_KEYDOWN VK_CONTROL` 不会更新 `GetKeyState`**：自动化测试发 Ctrl+A 时
    `bCtrl` 是 FALSE。正解：修饰键写 `(GetKeyState(k) | GetAsyncKeyState(k)) & 0x8000`，
    测试里用 `keybd_event` 按住 Ctrl、`PostMessage` 发字母键、再松开。
30. **⚠️ `InsertNewline()` 必须同步选区 anchor**：否则紧接着的 `InsertText()` 会把刚断开的行合回去，
    现象是"粘贴多行文本时换行全丢、挤成一行"。只测回车发现不了。
31. **⚠️ `EcRow` 是 POD，加字段要在 `AddTableRow`/`AddCodeRow` **两个**创建点都初始化**：
    第 16 轮加 `bParam` 时漏了 → 读到栈垃圾 → "这一行有没有参数归属线"变成随机真。
32. **⚠️ 原版代码行距不是恒定的 22**：graphic.e 是 21，流程类命令/精易模块是 **18**。
    量几何前先在本张基准图上量行距，表达式一律写「行顶 + N」。
33. **⚠️ 第 15 轮"滚动消息对自绘编辑器无效"是错的**：第 17 轮实测 `WM_VSCROLL(SB_PAGEDOWN)` →
    `scroll=460`、`WM_MOUSEWHEEL(-120)` → `scroll=66`。现在可以直接滚，不用再改采样源码。
15. **面板缩放加固**：`ShowPage()` 加了越界保护（标签控件 `GetCurSel()` 返回 -1 时不能把所有页藏掉），
    `LayoutPages()` 每次重排重新声明页的可见性，`OnSize()` 里 `RDW_ALLCHILDREN` 失效重画。
    （用户报"改窗口大小后左面板内容整块不重绘"，5 条复现路径都没重现，属于防御性加固。）
16. **菜单条最左端那个图标不是"画上去的装饰"**：MDI 子窗口最大化时 Windows 会把**子窗口的图标**
    画在父窗口菜单条最左端。所以要对框架**和** MDI 子窗口都设 `WM_SETICON`，并给窗口类设 `wc.hIcon`。
    图标组别：e.exe 的 `RT_GROUP_ICON 126/134` 是红色"易"、**127 是蓝色"易"（原版菜单条上那个）**、1151 是加密狗钥匙。
17. **每行最前面那根竖杠**是 MFC 的 gripper：`CControlBar::DrawGripper()` 只在 `CBRS_GRIPPER` 时画
    （横向 bar 画在 left+2 处、宽 5px）。不设这个样式就少一根线，而且第一条按钮会跟着左移 5px。
    原生菜单条那一行没有 bar，只能在 `OnNcPaint` 里自己补一根（原版菜单条也是停靠条）。
18. **工具条按钮要"平的"**：`CToolBar::Create()` 内部传的 `dwCtrlStyle` 是 **0**，所以按钮普通状态就带凸起方框；
    要用 `CreateEx(this, TBSTYLE_FLAT, ...)`（AFXEXT.H 里 CreateEx 的默认值就是 TBSTYLE_FLAT）。
19. **工具条的真分隔条**：位图里没有分隔图案，得在按钮表里插 `TBSTYLE_SEP`（`SetButtons` 传 `id=0`）。
    MFC 里**分隔条不占位图格**（`iBitmap` 只对非分隔按钮递增），漏了分隔条会导致其后所有图标整体错位。
18. **PowerShell 的 `-replace` 不能拿 `+` 拼替换串**（报 `-ireplace 运算符只允许后面包含 2 个元素`），
    而且报错前已经替换成功的部分会留下 —— 半改状态最坑。改完用 `Select-String` 复查。

## 验证

```
cmd /c D:\eclone\build.bat
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\shot.ps1 -Exe D:\eclone\bin\EClone.exe -Tag check
python D:\eclone\tools\check_layout.py D:\eclone\shots\check.windows.txt      :: 期望 14 passed, 0 failed

:: 支持库层（载入 .fne -> 建树 -> 断言 1560 节点 + 各类计数 = 原版 dump）
cmd /c D:\eclone\tools\build_libtest.bat
D:\eclone\tools\libtest.exe                                                   :: 退出码 0 = 过

:: 配色层（工具->系统配置->程序显示->颜色配置）
set ECLONE_DUMP_THEME=D:\eclone\shots\theme.txt
D:\eclone\bin\EClone.exe                                                      :: 启动一次即倒档
python D:\eclone\tools\check_theme.py D:\eclone\shots\theme.txt                :: 期望 80 passed, 0 failed

:: 子程序收缩 / 展开（点流程槽里的 - / +）
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\fold_smoke.ps1
                                                                              :: 期望 [PASS] collapse works

:: 面板分页（工作夹 3 页 + 状态夹 8 页，逐页断言 cursel 和实际可见页）
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\tabs_smoke.ps1
                                                                              :: 期望 [PASS] pane tab paging works

:: Dll命令页（点程序树的 Dll命令 节点切页；列位置/行距/底色/文字色与原版截图逐项对照）
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\dll_smoke.ps1
                                                                              :: 期望 [PASS] program tree selection switched the document page
                                                                              :: （它在程序树里点第 6 行 = Dll命令；采样源码一改，这个行号要用
                                                                              ::   python tools\tree_row.py shots\live_progtree_mine\trees.txt Dll命令 重新算）
python D:\eclone\tools\check_dll.py D:\eclone\shots\dll_view.png D:\eclone\shots\dll_view2.png D:\eclone\shots\ref_dll_page.png
                                                                              :: 期望 48 / 31 / 80 passed, 0 failed

:: 像素级对照（最硬的一条）：Dll命令页与原版 1:1 截图逐像素比，期望 0 差异
:: ⚠️ 用户给的那张 crop 比真正的编辑器客户区低 4px，所以必须 --ashift=4
python D:\eclone\tools\diff_png.py D:\eclone\shots\ref_dll_page.png D:\eclone\shots\dll_view.png 0 0 460 390 --ashift=4
                                                                              :: 期望 "差异像素 = 0 (0.00%)"

:: 代码页几何对拍（同一份源码：graphic.e）
python D:\eclone\tools\patch_sample_graphic.py
cmd /c D:\eclone\build.bat
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\code_shot.ps1   :: 拍 code_view.png + tabbar_mine.png
python D:\eclone\tools\diff_png.py D:\eclone\shots\live_tabs\view0.png D:\eclone\shots\code_view.png 0 0 1174 388
                                                                              :: 期望 ≈ 0.62%（只剩备注列宽/字形）
python D:\eclone\tools\restore_sample.py                                      :: ⚠️ 必须还原，否则后面全跑在错采样上
cmd /c D:\eclone\build.bat

:: 文档标签条对拍（原版那个 CCustomTabCtrl 是 1:1 基准）
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\probe_live.ps1 -File D:\eclone\shots\e_samples\graphic.e -OutDir D:\eclone\shots\live_tabs
python D:\eclone\tools\diff_png.py D:\eclone\shots\live_tabs\tabbar0.png D:\eclone\shots\tabbar_mine.png 0 0 190 22

:: 程序树（节点构成 + 图标 + 顺序；对照两套原版倒档）
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\probe_progtree.ps1 -Exe D:\eclone\bin\EClone.exe -OutDir D:\eclone\shots\live_progtree_mine
python D:\eclone\tools\tree_row.py D:\eclone\shots\live_progtree_mine\trees.txt Dll命令            :: 期望 6

:: 单元格就地编辑（点公开格 + 双击名字格改字）+ 块尾关键字可见性
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\cell_smoke.ps1
                                                                              :: 期望三条 [PASS]
                                                                              :: （xx结束 不画、xx循环尾 照画）

:: 编辑器交互：点选定位 / 框选 / 选区配色 / Ctrl+A C X V / 滚轮
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\sel_smoke.ps1
                                                                              :: 期望八条 [PASS]，末行
                                                                              :: [PASS] selection / clipboard / wheel work
                                                                              :: 框选那一步的截图落在 shots\sel_drag.png
                                                                              :: （选区 = #000080 底 + 白字，与原版 shots\live_drag\after.png 一致）

:: 参数归属线（语句下面那些 ※参数名：值 行 + 流程槽里的 -/+）
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\param_smoke.ps1
                                                                              :: 期望 [PASS] parameter lines expand and collapse
                                                                              :: 两态 1:1 截图落在 shots\param_before/after/back.png；
                                                                              :: 跟原版基准逐像素比：
                                                                              ::   python tools\diff_png.py shots\live_fold\view_after.png shots\param_after.png 60 113 300 165 shots\cmp_paramblock.png
                                                                              ::   python tools\param_cols.py shots\live_fold\view_after.png 117 129

:: 构建守卫（build.bat 里已自动跑；手工复查用）
python D:\eclone\tools\check_block_comments.py D:\eclone\gbk
                                                                              :: 期望 "block comments balanced in N file(s)"
                                                                              :: ⚠️ 不通过说明某个 // 注释转 GBK 后夹出了 */ ，会静默吃掉后面的语句

:: 流程线（原版 1:1 几何：竖线列/3亮3灭/►/▼/►►）
python D:\eclone\tools\patch_sample_flow.py
cmd /c D:\eclone\build.bat
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\code_shot.ps1 -Tag flowtest
python D:\eclone\tools\flow_scan.py D:\eclone\shots\flowtest.png 0 80 95 260
                                                                              :: 期望：级别0 的块竖线 x=25、级别1 的 47，两列都是 3 亮 3 灭；
                                                                              :: 循环尾那行有横虚线、`计次循环尾` 照画；如果真块竖线末端是 ▼
python D:\eclone\tools\restore_sample.py
cmd /c D:\eclone\build.bat

:: 流程线真值基准（原版自带例程，随时可重拍）
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\probe_live.ps1 -File "C:\Program Files (x86)\e\lib\krnln\samples\流程类命令.e" -OutDir D:\eclone\shots\live_flowcmd
                                                                              :: → view0.png：6 个循环块 + 2 个嵌套如果真，流程线的权威样本

:: 原版的选中态（拖选一段 → 量选区配色）
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\probe_drag.ps1 -File "C:\Program Files (x86)\e\lib\krnln\samples\流程类命令.e"
                                                                              :: → shots\live_drag\after.png：选区 = #000080 底 + 白字

:: 强制重倒显示模型 + 命中框（scroll / anchor,caret / fold row / param row）
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\redump.ps1
                                                                              :: → shots\model_dbg.txt 与 shots\model_dbg.fold.txt

:: 「系统配置」对话框真的能开、下拉里真的有 13 个 .clr
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\syscfg_probe.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\syscfg_shot.ps1

:: 提示文本与原版逐字节比对（原版抓包在 shots\sel_<N>.txt）
D:\eclone\tools\libtest.exe "C:\Program Files (x86)\e\lib" dump > shots\mine_hints.txt
python D:\eclone\tools\cmp_hints.py shots\mine_hints.txt 0 1 398 1224 2 14 400 401 1235 1225 399 460 466 1144 1167 1194 442 423 447 448 454 467 472 1211

:: 第 23 轮新增：UI 冒烟（工具条/面板开关 + 6 条表页命令 + 整体搜寻 + 新建窗体）
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\ui_smoke.ps1
                                                                               :: 期望 33 条 [PASS]，末行
                                                                               :: [PASS] ui_smoke: all checks passed

:: 第 23 轮新增：系统配置另外 6 个页签真的读写 + 持久化
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\cfg_smoke.ps1
                                                                               :: 期望 20 条 [PASS]，末行
                                                                               :: [PASS] cfg_smoke: the other system-config pages read, write and persist
                                                                               :: 判据是倒档 ECLONE_DUMP_CFG=<path>（一行一项 INT/BOOL/STR）
```

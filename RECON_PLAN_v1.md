# 与原版一致的还原路线（多轮计划）

目标：**语义与绘制都和 e.exe 一致**，判据不是"看起来像"，而是
① 同一份真实 .e + 相同真实鼠标/键盘动作下的**逐像素差异**（`tools/interact_diff.py` +
`tools/step_pixdiff.py`），② 原版自己的 dump（树/几何），③ 每条改动都能指到 e.exe 的伪 C 出处。

## 工具（已就绪）

| 工具 | 作用 |
| --- | --- |
| `tools/interact_diff.py` | 同一样例载入两边 → 相同动作（点正文/点标志列/输入/回车/退格/Ctrl+A）→ 每步 PrintWindow 截图 |
| `tools/step_pixdiff.py` | 逐步骤逐像素差异 + 差异最集中的行带 + magenta 差异图 |
| `tools/tree_pair.py` | 两边树的同状态并排量化 |
| 样例 | `%TEMP%\e_re\samples\flowcmd.e`（官方 `lib\krnln\samples\流程类命令.e` 的副本） |

基线（2026-10-10 首轮）：`01_initial` 差异 **65966 px / 14.01%**，最集中的行带
**y=0..35（表格）**、y=96..131、y=144..155（编辑后出现）、`08_ctrl_a` 21.13%。

## 轮次

每轮格式：**症状 → 原版出处（伪 C 函数/地址）→ 本工程改动点 → 判据**。

- **R1 参数表达式**：块类命令 `判断 ()/如果 ()` 括号里为空（原版是条件表达式）。
  出处：块配对与参数流 `sub_4C1B10`（块）、`sub_4B5000`（解析）、命令引擎 `sub_507660`。
  改动：`src/ec_eexpr.cpp` 的 `ArgsOf/FuncCall/IfStmt/SwitchStmt/LoopStmt`。
  判据：`判断 (…)` 非空，`01_initial` 的 y=96..131 带差异显著下降。
- **R2 层级与块配对**：`判断` 被层层嵌套（原版同级）；残留 `?stmt73@738`。
  出处：`sub_4C1B10`；工程自注（`ec_eexpr.cpp` 710-722）也指向这里。
  改动：`ec_eexpr.cpp` 的块标记（0x6E/0x6F/0x71..0x74）处理。
  判据：与原版 tree dump 逐行同层级；`01_initial` 差异下降。
- **R3 符号名**：正文出现 `var_16010002`（变量表里却是 `变量1`）。
  出处：元素表 `sub_4C1150`（`{id→元素*}`）。
  改动：`src/ec_eexpr.cpp:436` 的兜底不再触发（把局部变量/参数 id 加进表）。
  判据：全篇 `var_`/`用户子程序_` 出现 0 次。
  **2026-10-10 侦察结论（把这条钉死了）**：
  ① `0x4C1150` 落在 **`sub_4C0E00`**（按命令 id 大 switch 分发的函数）里，不是独立"表"函数；
  ② 本工程的查表是 `ec_eexpr.cpp:129 SymName()` → `SymMap(subs)`，而 `subs` 是由
     **`%08X|名字` 形式的静态字符串表**建出来的（即某个 .e 的 dump），**不是当前打开文件解析出来的**
     → 所以换个 .e（本例 flowcmd.e）就大面积 miss，才有 `var_16010002`。
  ③ 正确做法：在打开 .e 时把**该文件的**元素 id→名字 表建起来（变量表已经能显示 `变量1`，
     说明名字已解析到手，缺的只是它与元素 id 的关联），再喂给 `ERender`。
- **R4 注释行**：注释缺 `' ` 前缀、颜色/占行与原版不同。
  出处：正文绘制 `sub_4CAC80`（第 41 轮已证）与注释分支。
  改动：`ec_editor.cpp` 的注释 token 分支。
  判据：注释行像素一致。
- **R5 光标**：点/退格后 y=144..155 带出现差异。
  出处：**全程序唯一 `SetCaretPos` 调用点 = 0x4B5638，在函数 `0x4B53F0`**；
  尺寸 `CreateCaret(hwnd,NULL,2,dword_673904+4)`。
  改动：`ec_editor.cpp` 的 `CaretRect()/EnsureCaret()`。
  判据：`07_click_gutter` 的 y=144..155 归零。
- **R6 输入上屏**：`04_typed_A`/`06_backspace` 与原版不同。
  出处：`WM_CHAR 0x4BFEF0` → `sub_4CA000` → 命令 `0x0202006D`（走 vtable 槽 56）。
  改动：`ec_editor.cpp` 的 `OnChar` 链路。
  判据：两步差异带下降/归零。
- **R7 回车**：原版把 CR(13) 从 WM_CHAR 过滤，回车走 `PreTranslateMessage 0x4CFA00`
  → `sub_507C90(msg, this+0xD4)`。改动：`ec_editor.cpp` 的 `PreTranslateMessage/OnKeyDown`。
  判据：`05_enter` 与原版一致。
- **R8 选中**：`08_ctrl_a` 差异 21.13%。
  出处：原版用 `sub_4B9130`（区域刷背景）涂 #000080 + 白字，范围=程序集表/子程序表/首行代码。
  改动：`DrawTableRow` 的 bFull 分支 + `DrawAll` 代码行段。
  判据：`08_ctrl_a` 差异 < 1%。
- **R9 表格（最大差异带）**：`01_initial` 的 y=0..35 差 14340+14176+10916 px。
  出处：`sub_4BBC00` 的表格分支（底色/列线/文字基线）。
  改动：`ec_editor.cpp` 的 `DrawTableRow`。
  判据：该行带 < 200 px。
- **R10 流程线**：`sub_4B7E80`（线段，PS_DOT 笔）/`sub_4B80E0`（接头，图元 0xA3/0xE4/0xA2）/
  `sub_4CA030`（色键贴图）。图元已按原资源 ID 进工程（157/158/159/162/163/225/228/575/577/230）。
  改动：`ec_editor.cpp` 的 `EcFlowLine/EcGlyphBlit` 与 `DrawAll` 流程线段。
  判据：流程线区域像素一致。

## 规矩（沿用工程既有教训）

1. 改代码前先 `Copy-Item` 备份大文件（HANDOFF 有被截断的历史）。
2. 不"按看起来差不多"改：每条改动都要有伪 C 出处或对拍数字支撑。
3. 每轮结束必须给出：改动点、伪 C 出处、`step_pixdiff` 前后数字。
4. REA 伪 C 已修好（`decompile` 分页 + 文本净化；`idb_close` 幂等），**需重启一次**才装载。

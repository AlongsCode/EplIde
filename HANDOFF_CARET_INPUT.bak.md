# 交接：光标 + 输入（表格 / 代码）——2026-10-10 夜

## 现在的状态（可复现）
* 构建：`build.bat`（src UTF-8 → gbk → VC6）→ `bin\EClone.exe`，当前 **BUILD OK**。
* 对拍数字（同一份 `%TEMP%\e_re\samples\flowcmd.e`、同几何、滚轮对齐到文档头）：
  `01_top` = **2.01%**（19620 px）、`07_ctrl_a` = **5.49%**（53515 px）、逐行总差 **17371 px**。
* 工具：`tools/cmp_shots.py`（逐步同几何截图+差异）、`tools/line_cmp.py`（逐行条图+dy）、
  `tools/text_oracle.py`（原版自己的剪贴板文本真值）、`tools/res_dump.py`、
  `tools/auxinfo_dump.py`、`tools/lossy_syms.py`、`tools/find_comment_bytes.py`。
* 计划与全部试错数字：`RECON_PLAN.md`（2.6/2.8/2.9 是"已试已回滚 + 数字"）。

## 本次要解决的问题（用户原话：表格、代码的光标与输入都乱）
已定位的三处，全部有代码位置：
1. **表格里出现两根竖条**（证据 `shots/cmp/caretcmp_T.png` 下半）：一根是系统光标
   （`UpdateCaret`），一根是表格行渲染里自绘的"格内光标条"。只保留系统光标。
2. **光标高度/位置按行类型取值**：原版 `CreateCaret(*(HWND*)(this+28), NULL, 2, dword_673904 + 4)`
   的 `dword_673904` = **当前行行高**（代码行 vs 表格行不同）。本次已把 `ec_editor.cpp`
   `UpdateCaret` 改成按 `m_rows[n].h` / `EC_CODE_H` 取，`CaretRect()` 表格分支矩形改成整行高；
   **还要把自绘那根删掉**，并确认 `CaretRect()` 表格分支的 x = 单元格文字左边缘 + `EC_CELL_PAD`。
3. **`SetCaretPos` 调用点太多**：原版全程序只有 **一个** 调用点 `0x4B5638`（函数 `0x4B53F0`），
   EClone 现在在重画路径上处处可能触发 → 旧位置残留一帧，看着就是"飘"。
   目标：`EnsureCaret/UpdateCaret` 只在"焦点变化 / 行类型变化 / 光标逻辑位置变化"时更新。

## 输入相关（同一批）
* 原版链路：`WM_CHAR 0x4BFEF0` → `sub_4CA000`（vtable 槽 56，cmd `0x0202006D`）；
  回车不走 WM_CHAR（CR 被过滤），走 `PreTranslateMessage 0x4CFA00` → `sub_507C90(msg, this+0xD4)`。
* EClone 对应 `ec_editor.cpp` 的 `OnChar` / `PreTranslateMessage` / `OnKeyDown`，
  要比对：表格单元格内的输入（子程序名表/变量表/参数表：`m_nCaretCell`/`m_nCaretCellCol`）、
  代码行输入（`m_nCaretCol`）、回车后的行/块重排。

## 判据（必须给数字，不许只给截图）
1. `cmp_shots.py --steps 1,2,3,4,5,6,7`：各步不得高于基线 `01_top`（即不引入新差异）。
2. `line_cmp.py`：`best dy` 保持 0；光标/输入涉及的行差异要下降。
3. 光标几何：点表格表头格 / 数据格 / 代码行各抓一帧，与原版并排（表格里**只应有一根** 2px 竖条，
   高度 = 该行行高）。

## 取证地址（伪 C 已存）
`0x4B5638` SetCaretPos 唯一调用点、`0x4B53F0`（含 CreateCaret 实参）、`sub_4B4EA0` WM_SETFOCUS、
表格绘制 `sub_4BBC00`、gutter `sub_4B7E80`/`sub_4B80E0`/`sub_4B8F70`、渲染器 `sub_4C1150`、
`WM_CHAR 0x4BFEF0`/`sub_4CA000`、`PreTranslateMessage 0x4CFA00`/`sub_507C90`。

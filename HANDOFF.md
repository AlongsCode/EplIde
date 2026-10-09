> ⚠️ **2026-10-03 修复记录**：本轮一个"就地改写"的 patch 脚本在写回时抛
> `UnicodeEncodeError`，把本文件**截断成 1.6 KB**。已从 DSH 会话记录里恢复：
> `tools\recover_handoff_v2.py`（取结构正确的头部片段）+ `tools\recover_handoff_merge.py`
> （按行号并集合并各次 read 的结果，共 2510 行）+ `tools\handoff_assemble.py`。
> **恢复率约 72%**（部分 §2.x/§3.x 小节丢失），第 26 轮内容见 **§3.10**（事后补写）。
> ⚠️ 教训：**改大文件之前先备份**，或者"读-改-写新文件再替换"，不要 `open(w)` 就地覆盖。

1: # EClone 交接文档（交给下一个对话）
2: 
3: > 本文件**自包含**：不需要上一轮对话的上下文，读完就能接着干。
4: >
5: > 第 17 轮（本轮）= **流程线按原版自带例程 1:1 重做**（循环尾关键字照画 / 块首块尾按源码关键字
6: > 配对 / 块首行那个 ► / 块尾隐藏时用 ▼ 收尾）+ **修掉「EcRow::bParam 没初始化」这个让流程槽
7: > 凭空长出一堆 `-/+` 的 bug** + **代码编辑器终于能选中/复制/剪切/粘贴/全选/滚轮**
8: > （并修掉「点击定位的 x 基准错 22px」「InsertNewline 没同步选区 anchor 导致粘贴丢换行」两个根因）。
9: > 第 16（参数归属线）、15（流程线首版 + 单元格编辑）、14（左侧树/标签条/折叠底色）、
10: > 13（逐像素对齐）、12 及以前的内容都并入下面。
11: 
12: ---
13: 
14: ## ⭐ 复制下面这段给下一个对话（直接粘贴即可）
15: 
16: ```text
17: 继续做 D:\eclone 里的 VC6/MFC 工程，复刻易语言 5.9 IDE 的界面与代码编辑器。
18: 
19: 第一步：先读 D:\eclone\HANDOFF.md（自包含交接）和 D:\eclone\README.md，再动手。
20: 第二步：跑这批回归确认没坏（约 15 分钟）：
21:         cmd /c "D:\eclone\build.bat"                       -> BUILD OK
22:         D:\eclone\tools\libtest.exe                        -> 退出码 0
23:         powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\shot.ps1 -Exe D:\eclone\bin\EClone.exe -Tag check
24:         python D:\eclone\tools\check_layout.py D:\eclone\shots\check.windows.txt   -> 14 passed, 0 failed
25:         powershell -File tools\edit_smoke.ps1              -> [PASS] survived keystrokes
26:         powershell -File tools\fold_smoke.ps1              -> [PASS] collapse works（标记坐标从模型里找）
27:         powershell -File tools\tabs_smoke.ps1              -> [PASS] pane tab paging works
28:         powershell -File tools\cell_smoke.ps1              -> 三条 [PASS]（块尾关键字那条本轮改了语义，见 2.1d）
29:         powershell -File tools\param_smoke.ps1             -> [PASS] parameter lines expand and collapse
30:         powershell -File tools\sel_smoke.ps1               -> 八条 [PASS]（本轮新增：点选/框选/复制/剪切/粘贴/滚轮）
31:         set ECLONE_DUMP_THEME=D:\eclone\shots\theme.txt 启动再关
32:         python tools\check_theme.py shots\theme.txt         -> 80 passed, 0 failed
33:         powershell -File tools\dll_smoke.ps1
34:         python tools\check_dll.py shots\dll_view.png shots\dll_view2.png shots\ref_dll_page.png
35:                                                            -> 48/31/80 passed, 0 failed
36:         :: 像素级（最硬的一条）—— ⚠️ 参照截图比真客户区低 4px，必须 --ashift=4
37:         python tools\diff_png.py shots\ref_dll_page.png shots\dll_view.png 0 0 460 390 --ashift=4
38:                                                            -> 差异像素 = 0
39:         :: 代码页几何（0.62%，只剩备注列宽与字形）
40:         python tools\patch_sample_graphic.py && cmd /c "D:\eclone\build.bat"
41:         powershell -File tools\code_shot.ps1
42:         python tools\diff_png.py shots\live_tabs\view0.png shots\code_view.png 0 0 1174 388
43:         python tools\restore_sample.py && cmd /c "D:\eclone\build.bat"
44: 
45: 对标形态（用户 2026-10 已确认，别再问）：看本机装了 iDraw/ExUI 的那个 IDE 的**实际观感**，
46: 但只复刻"易语言本体"部分。iDraw 插件带来的三样东西**明确不做**：自绘菜单条(Menulwy)、
47: 输出面板(Scintilla)、ExUI 皮肤。菜单继续用原生 HMENU。
48: 用户 2026-10 追加指示：**能读 IDA 逆向源码就以源码为准**；**改任何东西都要与原版截图对比**。
49: ```
50: 
51: ---
52: 
53: ## 1. 目标与验收（done 的可观测条件）
54: 
55: | 项 | 内容 |
56: |---|---|
57: | 目标 | 1:1 复刻易语言 5.9 IDE 的**界面骨架 + 代码编辑**，不含编译器/调试器 |
58: | 验收 1 | `cmd /c D:\eclone\build.bat` 输出 `BUILD OK` |
59: | 验收 2 | `tools\libtest.exe` 退出码 0 |
60: | 验收 3 | `python tools\check_layout.py shots\check.windows.txt` → `14 passed, 0 failed` |
61: | 验收 4 | `tools\edit_smoke.ps1` → `[PASS] survived keystrokes` |
62: | 验收 5 | `tools\fold_smoke.ps1` → `[PASS] collapse works` |
63: | 验收 6 | `python tools\check_theme.py shots\theme.txt` → `80 passed, 0 failed` |
64: | 验收 7 | `tools\tabs_smoke.ps1` → `[PASS] pane tab paging works` |
65: | 验收 8 | `tools\dll_smoke.ps1` + `python tools\check_dll.py …` → `48/31/80 passed, 0 failed` |
66: | 验收 9 | `python tools\diff_png.py shots\ref_dll_page.png shots\dll_view.png 0 0 460 390 --ashift=4` → **差异像素 = 0** |
67: | 验收 10 | 程序树节点构成 = 原版（见 2.2 节） |
68: | 验收 11 | `tools\param_smoke.ps1` → `[PASS] parameter lines expand and collapse` |
69: | 验收 12 | 构建自带的守卫：`build.bat` 里跑 `python tools\check_block_comments.py gbk`，不通过就 `BUILD FAILED` |
70: | **验收 13（本轮）** | `tools\sel_smoke.ps1` → 八条 `[PASS]`：点选定位 / 框选出选区 / 选区涂 `#000080` / Ctrl+A / Ctrl+C / Ctrl+X / Ctrl+V / 滚轮 |
71: | **验收 14（本轮）** | `tools\cell_smoke.ps1` → `[PASS] block-end keywords: end-suffix hidden, loop-tail rendered` |
72: | **验收 15（本轮）** | 流程线几何见 2.1d（对着原版自带例程 `shots\live_flowcmd\view0.png` 逐像素量） |
73: | **验收 16（本轮）** | 缩进规则见 2.1e：正文左边缘必须是 `' 注释` 46 / 块首 68 / 嵌套块首 90 / **嵌套块体 90**（和块首同级）/ 块后 68。自测图 `shots\flowtest6.png` + `cmp_flow_17.png` |
74: | 用户明确不做 | 起始页（内嵌 IE）、iDraw 三样（自绘菜单条 / Scintilla 输出面板 / ExUI 皮肤） |
75: 
76: ---
77: 
78: ## 2. 已完成（做了什么 → 怎么验证的）
79: 
80: ### 2.1e ⚠️ 缩进规则（用户 2026-10 指出：**之前整棵块结构都浅了一级**）
81: 
82: 把三个原版自带样本的**每一行正文左边缘**逐行量出来（正文列 = 46 + 22n），规则不是
83: 「每进一层块就深一级」，而是：
84: 
85: | 行 | 原版实测 x | 级别 |
86: |---|---|---|
87: | `' 注释`（子程序体里） | 46 | 0 |
88: | `.计次循环首` | 68 | 1 |
89: | `.如果真`（在循环体里） | 90 | 2 |
90: | `到循环尾 ()`（**如果真的体**） | **90** | **2** ← 和「如果真」同一列 |
91: | `画板1.滚动写行`（如果真之后） | 68 | 1 |
92: | `.计次循环尾` | 68 | 1 |
93: 
94: 即：**块首行 = 外层上下文 + 1 级；块体的语句和块首行同一级；可见的块尾行也和块首同一级；
95: 块结束之后回到外层级别。** 三个样本互证：
96: 
97: | 样本 | 证据（正文左边缘） |
98: |---|---|
99: | `shots\live_flowcmd\view0.png`（流程类命令.e） | 68 / 90 / 90 / 68 / 68（上表） |
100: | `shots\live_fold_jy\view_before.png`（精易模块 `_启动子程序`） | `如果真` 68、体 68、体 68、块后 `m_CodePage` 46 |
101: | `shots\live_learn2\view0.png`（一步一步跟我学） | `如果真` 68 / 体 68；`如果` 68 / 体 68；**嵌套**的 `如果真` 90 / 体 90；内层块之后 68 |
102: 
103: 实现：`Rebuild()` 里维护 `nContext`（当前块体的级别）+ `blkStack`（块首行的显示行号）。
104: `块首 → indent = nContext + 1，再把 nContext 设为这个级别`；`普通语句 → indent = nContext`；
105: `块尾 → indent = 块首那一级，然后 nContext 回退到外层`。
106: 
107: ⚠️ 以前是「块首用 nContext、然后 nContext++」，于是：① 每个块首比原版浅一级
108: （流程竖线也跟着落在 25 而不是 47）；② 块体比块首深一级 —— 用户就是看这个
109: （说「跳出循环和如果真你看看对齐了吗」）一眼指出来的。
110: 
111: **怎么量**（下次还要量别的样本时照这个来）：
112: ```python
113: # 取一行的正文左边缘：把「流程槽灰底/灰虚线/灰三角/浅灰」全排除后取最小 x
114: BAD = {(255,255,255),(128,128,128),(236,236,236),(105,105,105),(192,192,192),(0,0,128)}
115: min(x for y in range(y0, y0+12) for x in range(30, 900) if px[x,y] not in BAD)
116: ```
117: （`shots\live_flowcmd\view0.png` 的行顶：217 注释 / 235 计次循环首 / 253 如果真 / 271 到循环尾 /
118: 289 画板1 / 307 计次循环尾；`view_before.png`：295 如果真 / 313/331 体 / 371 m_CodePage）
119: 
120: ### 2.1d 本轮（第 17 轮）流程线 1:1 重做 + 编辑器交互
121: 
122: #### 真值基准是怎么来的（**这是本轮最重要的方法**）
123: 
124: 自造的**文本形式 `.e` 原版打不开**（第 3.3-1 条，又试了一次），所以真值只能从**原版自带的例程**
125: 拍。用 `tools\probe_live.ps1 -File <原版自带 .e> -OutDir ...` 拍 1:1 客户区：
126: 
127: | 基准图 | 是什么 | 怎么来的 |
128: |---|---|---|
129: | `shots\live_flowcmd\view0.png`（1670x683） | 原版自带 **`C:\Program Files (x86)\e\lib\krnln\samples\流程类命令.e`** —— 6 个循环块 + 2 个嵌套如果真，**流程线的权威样本** | `probe_live.ps1 -File "…\流程类命令.e" -OutDir shots\live_flowcmd` |
130: | `shots\live_learn2\view0.png` | 原版自带 `…\krnln\samples\一步一步跟我学写易程序二.e` —— `如果` / `如果真` 块 + `否则` 不画 + ▼ 收尾 | 同上 |
131: | `shots\live_99\view0.png` | 原版自带 `…\samples\经典数学算法集\005.99乘法表.e` —— 嵌套 `变量循环首` | 同上 |
132: | `shots\live_sel\after_a.png`（1707x743） | 同一个 `流程类命令.e` 的整窗（自动滚到底），用来核对第 4/5/6 个块的 ►► 落点 | `tools\probe_sel.ps1`（点进编辑器 + Ctrl+A，本轮新写） |
133: | `shots\live_drag\after.png` | 原版编辑器里**鼠标拖选**一段的样子 → 选区配色 | `tools\probe_drag.ps1`（本轮新写） |
134: 
135: #### 流程线规范（全部逐像素量出来，单位=1:1 像素）
136: 
137: 记 `x = EC_TEXT_X_OF(块首行缩进级 - 1) + 1`（级别 1 → 47，级别 0 → 25），
138: 灰 = `#808080`（`EC_CS_LINK`），藏青 = `#000080`（`EC_CS_OPER_FG`），
139: "行中线" = `行顶 + 9`，代码行距按原版样本是 18（本工程配置值 22）：
140: 
141: | 元素 | 原版实测 | 实现 |
142: |---|---|---|
143: | 竖虚线 | 3 亮 3 灭，从**块首行中线**到**块尾行中线** | `EcFlowLine`（`LineTo(y+3)`，⚠️ 写 y+2 只亮 2px） |
144: | 块首行横虚线 | 循环类：`x .. x+15`；如果家族：`x+2 .. x+15`（实测差 2px，别问为什么，照抄） | `EcFlowLine` |
145: | 块首行的 **►** | **只有循环类（计次/变量/判断/循环判断 循环首）有**：`x+12 .. x+15`，4 宽 7 高，灰实心右三角，逐行宽 1/2/3/4/3/2/1 | `DrawAll` 里 `pw[7]` |
146: | 块尾行的横虚线 | 循环尾**可见**，横虚线 `x .. x+15`，**没有 ►**（第 15 轮猜成有 ► 是错的） | 同 |
147: | 块尾行**不可见**时 | 如果结束/如果真结束/判断结束不画，竖线延到**块体最后一行的行顶 + EC_CODE_H**，末端一个灰 ▼（逐行宽 1/7/5/3/1） | `EcRow::blkEnd == -1` 那条分支 |
148: | 块体第一个 ►► | `x-14`、`x-10` 两个藏青实心右三角，各 3 宽 5 高（逐行宽 1/2/3/2/1），**竖直中心 = 行中线**（以前写 `行顶+6`，低 3px） | `DrawAll` |
149: | ►► 落在哪一行 | ① **只有跨度 ≥ 3 个显示行的块才画**（实测：`如果真` 体里 1 条语句 → 没有；2 条 → 第一条上有；循环块 `首+体+尾` 天然 3 行 → 总有）；② 位置 =「块首行之后、**块尾行之前**第一条 `indent == 块首 indent`、且自身不是块首关键字的语句」（新缩进语义下块体直接语句和块首同级，见 2.1e） | 见第 6 节「存疑」——原版第 6 个块是个反例 |
150: 
151: **哪些关键字画、哪些不画**（`g_szHidden` 本轮改了）：
152: 
153: - **不画**：`如果结束`、`如果真结束`、`判断结束`、`否则`、`默认`
154:   （证据：`live_flowcmd\view0.png` 里 `.如果真 (变量1 ＞ 3)` 的体后面直接是下一条语句；
155:   `live_learn2\view0.png` 里 `如果` 块没有对应的结束行、也没有 `否则` 行）
156: - **画**（本轮从"不画"改成"画"）：`计次循环尾`、`判断循环尾`、`循环判断尾`、`变量循环尾`
157:   （证据：`live_flowcmd\view0.png` 里 `.计次循环尾 ()` / `.变量循环尾 ()` 明明都在，
158:   而且用户 2026-10 给的截图 b/c 里也是成对出现的）
159:   ⚠️ 第 15 轮把这 4 个塞进 `g_szHidden` 是**误读了用户的话**（用户说的是"如果真结束、计次循环尾…
160:   这些不显示"，implementer 把"尾"看成了"首"）。
161: 
162: **块首/块尾怎么配对**：`Rebuild()` 里维护一个**源码关键字栈** `blkStack`
163: （遇 `g_szBlockStart` push 当前显示行号，遇 `g_szBlockEnd` pop 并写回块首行的
164: `EcRow::blkEnd` / `blkLast`）。**删掉了旧的 `ComputeBlocks()`** —— 它按「缩进」推块边界，
165: 而被隐藏的块尾行不在显示模型里，于是把 `返回 ()` 也吞进循环体、竖线一直拖到文档尾
166: （这就是用户 `a.png` 里那条错的线）。
167: 
168: #### 顺手修掉的：`EcRow::bParam` 从未初始化（流程槽凭空多出 `-/+`）
169: 
170: `EcRow` 是 POD，`AddTableRow`/`AddCodeRow` 都没给 `bParam` 赋值 → 读到栈上的垃圾。
171: 实测同一进程里连着几行都是 `4402570`（真值），于是**每一行**都挂上了参数归属线的 `-/+`
172: （用户 `a.png` 里每个语句行左边那根 8px 藏青横杠就是这个）。
173: **正解**：两个构造函数里都 `r.bParam = FALSE;`。修完 `redump` 只剩 `param row=4`（真的带参那句）。
174: 诊断命令：`powershell -File tools\redump.ps1` → 看 `shots\model_dbg.fold.txt` 的 `param row=` 行。
175: 
176: #### 编辑器交互（选中 / 复制粘贴 / 滚轮）
177: 
178: | 功能 | 实现 | 验证 |
179: |---|---|---|
180: | 点选定位 | **根因**：`CaretFromPoint()` / `CaretRect()` 用的是 `EC_FLOW_X(缩进级)`（= 24+22n，**流程槽**的列）当正文左边缘，比真值浅 22px → 点哪儿插入点都跑到行尾。改成 `TextXOfIndent()`（= 46+22n） | `sel_smoke.ps1`：点 x=150 后 `caret=6,17`（行中间） |
181: | 框选 | `OnLButtonDown` 记 anchor + `OnMouseMove` 拖（原有），**本轮补上选区绘制** | `sel_smoke.ps1`：`anchor=6,1 caret=6,31` |
182: | 选区外观 | **原版实测**（`shots\live_drag\after.png`，在原版编辑器里真拖了一段）：底色 `#000080`、**白字**、高度 = 文字那 **12px**（不是整行 22px） | `sel_smoke.ps1`：选区那一行里 `#000080` 最长连续 56px |
183: | 配色来源 | `EC_CS_MARK`（"选中标记"槽），并把默认值实测成 `#000080` 写进 `g_ecMeasured` | `check_theme.py` 仍 80/0 |
184: | Ctrl+A / C / X / V | `OnKeyDown` 里加 4 个 case；剪贴板走 Win32 `CF_TEXT`（GBK） | `sel_smoke.ps1` 四条 |
185: | 右键菜单 | `OnContextMenu` 运行时建 `CMenu`（剪切/复制/粘贴/全选），不进资源 | 手工验证（无自动断言） |
186: | 鼠标滚轮 | `OnMouseWheel` 一格 3 行；`zDelta` 不是 120 的整数倍时至少走一行 | `sel_smoke.ps1`：`scroll 0 -> 66` |
187: 
188: **⚠️ 顺带把第 15 轮那个"滚动消息对自绘编辑器无效"的坑推翻了**：现在
189: `WM_VSCROLL`(SB_PAGEDOWN) 和 `WM_MOUSEWHEEL` 都能滚（`scroll=460` / `66`）。
190: 第 3.2-9 条作废，**要滚到文档下半部分不用再改采样源码了**。
191: 
192: ### 2.1c 第 16 轮 参数归属线（仍然有效）
193: 
194: **原版长什么样**：`shots\live_fold\view_before.png` / `view_after.png`（原版打开 graphic.e，
195: 点/不点 `信息框（…）` 那行的 `+`），1174x388，**默认是收起**（`m_bParamShow = FALSE`）。
196: 
197: | 元素 | 原版实测 | 实现 |
198: |---|---|---|
199: | 参数行行顶 | **语句行顶 + 16**，行距 **16**（语句行距 22，两者别混） | `y0 = 行 y + 16`，`lh = EC_CODE_H*8/11` |
200: | 文字再往下 | 行顶 + 4，行高 12 | `TextOut(x, y0 + lh*k + 4, …)` |
201: | `※` 左边缘 | **x = 语句正文列 + 44** | `xStar = xStmt + 44` |
202: | 参数名 | 从 `x+55` 起，冒号后 1 空格；值从 `x+134` 起 | 依次 TextOut |
203: | 颜色 | `※` = `#000080`；参数名 = 黑；值走词法着色（字符串 `#008080`、数值 `#800000`） | 同 |
204: | 左侧竖虚线 | **x = 正文列 + 31**（正文 46 → 77），3 亮 3 灭 `#808080` | `EcFlowLine` |
205: | 每行横虚线 | 参数行顶 + 6，一条 3px + 隔 3px 再一条 2px | 同 |
206: | 顶端 ▼ | x=77、参数行顶 - 3，1/3/5/7 宽四行 | `FillSolidRect` |
207: | 行数 | **= 实际写了的参数槽数**（`信息框（"…", 0, ）` 画 3 行；`(…, 0 )` 画 2 行） | `EcParseCall` 的 `bLastSep` |
208: | 参数名来源 | 支持库（`EcCmdArgsByName`），只有一条权威路径 | `tools\argprobe.exe "<e>\lib" 信息框` |
209: | `-/+` 语义 | **整篇**显示/隐藏（全局开关） | `m_bParamShow` |
210: 
211: **什么语句才算"有参数"**：必须是**支持库命令**（`EcNameKind==1`）**且命令名紧接全角左括号 `（`（a3a8）
212: 且至少有一个非空实参**。`返回 （x）`、`计次循环首 (10, i)` 这些命令名后面有空格的是流程语句，不画。
213: 
214: ### 2.1b 第 15 轮 单元格就地编辑（流程线那部分已被 2.1d 取代）
215: 
216: | 功能 | 实现 | 验证 |
217: |---|---|---|
218: | 点「公开/易包/参考/可空/数组/静态/传址」格 | `HitTestCell`（列语义 = 同表同格数的最近表头行）→ `ToggleFlagCell` → `Rebuild()+Layout()` | `cell_smoke.ps1` → `[PASS] flag cell toggled` |
219: | 双击文字格 | `BeginEditCell` 建 `CEdit`，回车/失焦提交，ESC 取消 | `[PASS] in-place text edit committed` |
220: | 块尾/分支关键字 | `g_szHidden` 命中只调缩进级、不建显示行（源码行还在） | `[PASS] block-end keywords: …`（本轮改了语义） |
221: 
222: ### 2.1 第 14 轮的四件事（左侧树 / 标签条 / 折叠底色 / 箭头）
223: 
224: | # | 用户说的 | 原版实测证据 | 改了什么 |
225: |---|---|---|---|
226: | 1 | 「左侧文件夹、自定义数据类型、全局变量、类」 | `shots\live_jy\prog_tree.txt`（5189 节点）＋`shots\ref_prog_tree.png` | `EcBuildProgTree` 重写：文件夹(img=16)、类模块(img=0)、程序集=菱形(img=3)、全局变量/自定义数据类型有内容才出现 |
227: | 2 | 「程序编辑界面的表头在下侧」 | `shots\live2\tree.txt`：`CCustomTabCtrl` 412,685 1195x22，挂在 MDI 客户区**下侧** | `CECChildFrame::LayoutViews()` 标签条挪到底部；`CECTabStrip::OnPaint()` 按原版重画 |
228: | 3 | 「折叠后的加减背景色都不对」 | `shots\live_fold\view_before.png`：`-`/`+` 是藏青 **9x9**，画在**白底**上 | 删掉收起行的浅蓝底；横杠 9px（x=22..30），竖杠在 x=26 |
229: | 4 | 「带箭头啥的都不对」 | 同上：子程序体第一条语句的流程槽里有 **↓**（竖线 x=9 高 7px + 7x4 实心三角，藏青） | `ECM_DOWN` 现在真的画出来（`DrawCodeRow`） |
230: 
231: **顺带修掉的**：`EC_TOP_PAD` 8→4；`EC_TEXT_BASE` 69→**46**；程序集表头行填 `#ECECEC`；
232: 没有参数/局部变量的子程序不画 `-/+`。
233: 
234: ### 2.2 程序树的两套权威依据（别再拿别的图比）
235: 
236: | 来源 | 内容 | 文件 |
237: |---|---|---|
238: | 原版 + **图形按钮.e** | 11 节点 | `shots\live_progtree\prog_tree.txt` |
239: | 原版 + **精易模块.e** | 5189 节点，含文件夹/类模块/全局变量/自定义数据类型/Dll命令 | `shots\live_jy\prog_tree.txt` |
240: | 用户给的树截图 | 图标一一对应 | `shots\ref_prog_tree.png` |
241: 
242: **定死的图标规则**（`src\res\progtree_icons.bmp` 的槽位号 = 代码里的 `EP_*`）：
243: 
244: ```
245: 0 程序数据   1 程序集(菱形)   2 文件夹(黄)   3 子程序(带↓)   4 窗口
246: 5 _启动窗口  6 常量表   7 资源表   8 声音   9 图片或图片组   10 模块引用表(带蓝↓)
247: 11 外部文件记录表   12 全局变量(圆柱)   13 自定义数据类型＝类模块(方块)   14 Dll命令
248: ```
249: **节点顺序**：`文件夹/程序集/类…` → `窗口` → `全局变量` → `自定义数据类型` → `Dll命令`
250: → `常量表...` → `资源表` → `模块引用表` → `外部文件记录表`。
251: 
252: ### 2.3 逐像素对拍（基准表）
253: 
254: | 基准图 | 是什么 |
255: |---|---|
256: | `shots\live_tabs\view0.png`（1174x388） | 原版打开 **graphic.e** 的代码页 1:1 客户区（权威） |
257: | `shots\live_tabs\tabbar0.png`（1195x22） | 原版那条 `CCustomTabCtrl` 的 1:1 |
258: | `shots\live_fold\view_before/after.png` | 原版点参数归属线 `+` 前后的 1:1（graphic.e） |
259: | `shots\live_fold_jy\view_before/after.png`（1670x705） | 原版打开 **精易模块.e** 的代码页 1:1 |
260: | `shots\live_flowcmd\` `live_learn2\` `live_99\` `live_sel\` `live_drag\` | **本轮新增**，见 2.1d 的表 |
261: | `shots\ref_dll_page.png` | 用户给的 Dll 页 crop（**比真客户区低 4px → 对比要 `--ashift=4`**） |
262: 
263: **代码页对拍流程**：`python tools\patch_sample_graphic.py` → `build.bat` →
264: `powershell -File tools\code_shot.ps1` → `python tools\diff_png.py shots\live_tabs\view0.png
265: shots\code_view.png 0 0 1174 388` → **跑完务必 `python tools\restore_sample.py`**。
266: 本轮结果仍是 **2832 / 455512 = 0.62%**（和上一轮一样，说明流程线改动没有影响这个样本的收起态）。
267: 
268: ### 2.5 附带逆向：krnln.fne 的「窗口调度」（第 17 轮）
269: 
270: 用户让「看看核心库 krnln 的代码，包括窗口调度」。完整笔记在
271: `D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\krnln\window_notes.md`
272: （原始反编译 `…\krnln\decomp_window_cluster.txt` / `decomp_event_helpers.txt`），
273: IDB 是 `…\ida_work\krnln.fne`（已存盘，funcs=5558），工具 `tools\ida_krnln.py`。四条要点：
274: 
275: 1. **krnln.fne 里静态链接了一整份 MFC 4.2** —— `sub_100E5B87` 就是 `AfxRegisterWndClass`
276:    （`wsprintfA(buf,"Afx:%x:%x:%x:%x:%x", hInst, style, hCursor, hBrush, hIcon)`），
277:    `sub_100E76EA` 是 `AfxDeferRegisterClass`（`AfxWnd42s`/`AfxMDIFrame42s`/`AfxFrameOrView42s`/
278:    `AfxControlBar42s`/`AfxOleControl42s`）。**所以我们一直在对的那个类名
279:    `Afx:400000:8:10003:100063:0` 是 MFC 生成的，不是易语言自己起的** —— 本工程用 VC6+MFC 4.2，
280:    这部分天然一致，不用仿。
281: 2. **窗口子类化靠全局 `WH_CBT` 钩子**（`0x100E521E`）：在 `HCBT_CREATEWND` 时把旧 WndProc 存进
282:    窗口属性 **`AfxOldWndProc423`**（MFC 4.2 自己的属性名，`GlobalAddAtomA` 注册），
283:    再 `SetWindowLongA(hwnd, GWL_WNDPROC, sub_100E5043|sub_100E51A2)`。
284:    即 **krnln 只在最前面插一层**，真正干活的是原来那个 proc。
285: 3. **那一层只拦 4 类消息**（`sub_100E5043`）：`WM_ACTIVATE`(6) → `0x36E` 通知；
286:    `WM_SETCURSOR`(0x20)；`WM_NCDESTROY`(0x82) → 还原 + RemoveProp + DeleteAtom；
287:    `WM_INITDIALOG`(272) → 前后存/还原矩形。另一个变体 `sub_100E51A2` 多拦
288:    `WM_CTLCOLOR*`(306..312) 让易语言决定组件配色。
289:    HWND→对象用**带锁的全局表**（`sub_100E4F62` / `sub_100E4FA7`），不是 SetProp。
290:    `sub_100E4DA1` 还维护一套「当前窗口/当前消息」的栈式上下文（`取窗口()`/`取事件组件()` 读它）。
291: 4. **事件分派器不在 krnln 里，在编译产物里**：krnln 提供的是对象登记 + 上下文 + 元数据
292:    （`LIB_DATA_TYPE_INFO.events` 真有事件表，`krnln_fne_events.json` 里 51 个类型带事件）。
293:    ⚠️ 内部私有消息是 `0x36E/0x36F`（**在 WM_USER=0x400 以下**）和 `0x8000+` 段，
294:    **跟本工程的 `EC_WM_HINT = WM_USER+111` / `EC_WM_OPENDOC = WM_USER+112` 不撞号**。
295: 
296: ### 2.5b 附带逆向：原版 IDE 是**怎么把支持库接进来的**（调完 GetNewInf 之后）
297: 
298: 完整笔记 `…\krnln\ide_lib_host_notes.md`，原始反编译 `…\krnln\e_notify.txt`（若已归档）。
299: 扒的是 e.exe 的 `sub_45F7F0`（装库主函数，0x45f7f0）和 `sub_467610`（0x467610）：
300: 
301: 1. `LoadLibraryA(库文件)` → **优先 `GetProcAddress(h,"InitCtl")`**（lib2.h 里查不到的第二个导出，
302:    `InitCtl(路径, &LIB_INFO, &handle)`；给打包库 `.opk/.npk` 和新式库用）
303:    → 没有才 `GetNewInf()`。
304:    **实测 krnln.fne 的 307 个导出里只有 `GetNewInf` 和 `GetNewSock`，没有 `InitCtl`
305:    → EClone 的加载路径本来就是对的，不用改。**
306: 2. **八项校验**：`m_dwLibFormatVer==20000101`、`m_pfnNotify(+120)!=NULL`、`m_nMajorVersion(+8)>0`、
307:    GUID/名字串合法、GUID 与请求一致、语言匹配、`nDataType/nCategory/nLibConst` 下限、
308:    `LBS_NO_EDIT_INFO` 不许有、要求的系统版本与**核心库版本**（`+28/+32`）都要满足。
309: 3. **★ 核心一句：`m_pfnNotify(1, sub_467610, 0)`**（`NL_SYS_NOTIFY_FUNCTION`）——
310:    IDE 把自己那个 `PFN_NOTIFY_SYS` 交给库；库以后靠它反调：
311:    IDE 里是 `NES_*`（取主窗口/跑功能/选图片），编译产物里是 `NRS_*`（取 EXE 路径、分配内存、
312:    `EVENT_NOTIFY`、报运行时错误、退出程序…）。**同一套函数指针，两套码。**
313: 4. 失败/卸载：`m_pfnNotify(6, handle, 0)` = `NL_FREE_LIB_DATA` + `FreeLibrary`；
314:    成功则把 `LIB_INFO*`、`handle`、`HMODULE` 存进宿主三张表，**此后不再 FreeLibrary**。
315: 5. IDE → 库的按需查询走 `NAS_*`（1000 段）：图标 1000 / 数据类型 1002 / HBITMAP 1003 /
316:    语言 1004 / 版本 1005 / 目录 1006 / CWnd 桥接 1007-1011 / 是否易窗口 1014。
317: 6. **「默认载入核心库」的那一行**：`if (_mbsicmp(Str1,"krn")==0) Str1="krnln";`
318:    —— 核心库**短名是 `krn`、文件名是 `krnln.fne`**，IDE 做了一次别名替换。
319: 7. `m_pfnNotify` 在 `LIB_INFO` 里的字节偏移 = **120**（索引 30）；`CMD_INFO` 步长 = **36** 字节
320:    —— 可以拿来交叉验证 `ec_lib.cpp` 的 `RAW_*` 镜像。
321: 
322: ### 2.5c 附带逆向：`GetNewSock` 是什么（krnln 的第二导出）
323: 
324: 笔记 `…\krnln\getnewsock_notes.md`。要点：
325: 
326: - `GetNewInf`（0x10064d30，6 字节，`return &unk_101388A0;`）和 `GetNewSock`（0x10064d40，121 字节）
327:   是**并列的两个导出**：前者给**元数据**（`LIB_INFO*`），后者给**函数指针**。
328: - `GetNewSock(nNo)` 是个小分派表：`1000/1001/1003 → sub_1002ED08`、`1002 → sub_10064C80`（按 id 释放对象）、
329:   其它 → `NULL`。`sub_1002ED08` → `sub_1002E1BF` = 「**按 id 取函数指针并调用**」，
330:   带 `this[298]=GetProcessHeap()` 和 `++this[49]` 可重入计数。
331: - **`Sock` 就是网络套接字**：krnln 里有字符串 `_EL_UDPSock` / `_EL_ClientSock` / `_EL_ServerSock`
332:   （`_EL_` 前缀跟设计器的 `_EL_RgnButton` 同族），各自被一个建窗口函数引用
333:   （`sub_10051AE0` / `sub_10051F60` / `sub_100523D0`）→ **易语言的 UDP/客户端/服务器 三个网络组件
334:   是 krnln 内部以窗口类实现的**，`GetNewSock` 就是它们的入口。
335: - e.exe 里 `"GetNewSock"` 出现 4 次，**每次都紧贴核心库装载代码**
336:   （邻居：`krnln.fne` / `krnln.fnr` / `Software\FlySky\E\Install` /
337:   `Not found the kernel library!` / `Failed to decompress data!`）→
338:   **`krnln.fnr` 是压缩版核心库**；那些英文错误文本属于"编译产物的加载器"那一份。
339:   这 4 个地址**没有任何代码交叉引用**（push/mov/dword 三种搜法全 0 命中），落在 `.data`，
340:   是数据块/模板，不是 `GetProcAddress` 硬编码。
341: - **lib2.h / SDK 里完全没有 `GetNewSock` 的记载** —— 和 `InitCtl` 一样属于未公开接口。
342: - **对 EClone：不用做**（我们只读元数据）。但将来要跑带网络组件的程序时，
343:   这就是必需的一块。
344: 
345: ### 2.4 第 13 轮及以前（仍然有效，别重做）
346: 
347: Dll 命令页逐像素 0 差异、流程槽红铅笔、✓ 9x7 点阵、`EcSplit/EcJoinFrom` 原样分隔符、
348: 3 个文档页与 `EC_WM_OPENDOC`、程序集树图标表、`check_theme.py` 的 GBK、
349: 工作夹/状态夹分页、支持库层 1560 节点、布局 14 项回归。
350: 
351: ---
352: 
353: ## 3. ⚠️ 坑（每条都实际踩过；"别再试什么"是重点）
354: 
355: ### 3.3 本轮（第 17 轮）踩的
356: 
357: 1. ⚠️⚠️ **自造的「文本形式 `.e`」原版根本打不开** —— 本机这份原版（`C:\Program Files (x86)\e\e.exe`）
358:    对纯文本 `.e` 一律显示空框架（`first-party probe` 的 `views=0`，标题里没有文档名）。
359:    实测连原版自带的文本形式样本 `shots\e_samples\dll_sample.e` 也打不开。
360:    **别再试**：不要浪费时间造 `.e`。**正解**：拿**原版自带的例程**当样本
361:    （`C:\Program Files (x86)\e\lib\krnln\samples\*.e`、`C:\Program Files (x86)\e\samples\经典数学算法集\*.e`，
362:    85 个算法例程全带循环/分支）。
363: 2. ⚠️ **`.ps1` 里一个非 ASCII 字节都不能有（第 16、15、13 轮都踩过，本轮又踩）**：
364:    写中文提示串会静默改变行为/报不相关的语法错。写完一律
365:    `python -c "print(sum(1 for x in open(r'X.ps1','rb').read() if x>127))"` 必须是 **0**。
366: 3. ⚠️ **PowerShell 里 `LP` 是 `Out-Printer` 的别名** —— 我拿 `LP` 当"打包 lParam"的函数名，
367:    调用点直接变成打印机命令、报 `Out-Printer : 找不到接受参数"153"的位置参数`。
368:    辅助函数名别用 2 个字母（`MkLp` 这种才行）。
369: 4. ⚠️ **`ECLONE_DUMP_MODEL` 有两个输出文件**：`<path>` 是**显示模型**（row/par），
370:    `<path 改后缀>.fold.txt` 才是 `scroll=` / `anchor,caret` / `fold row=` / `param row=`。
371:    本轮一开始读错文件，断言全假失败（`Meta` 正则匹配不到 → 全 0）。
372: 5. ⚠️ **`PostMessage` 出来的 `WM_KEYDOWN VK_CONTROL` 不会更新 `GetKeyState`**
373:    （Windows 只对真实输入更新队列键状态）→ 自动化测试里发 Ctrl+A 时 `bCtrl` 是 FALSE。
374:    **正解**（本轮采用）：修饰键判断写 `(GetKeyState(k) | GetAsyncKeyState(k)) & 0x8000`；
375:    测试里用 `keybd_event` 把 Ctrl 按下去、`PostMessage` 发字母键、再松开。
376: 6. ⚠️ **`InsertNewline()` 必须同步更新选区 anchor** —— 否则 `HasSel()` 还是 TRUE，
377:    紧接着那次 `InsertText()` 会把刚断开的行又合回去。现象：粘贴多行文本时**换行全丢、挤成一行**
378:    （`lines` 不变、caret 停在同一行）。这个坑很隐蔽：单独测回车没问题，只有"粘贴"这种
379:    "插入+断行+再插入"的序列才暴露。
380: 7. ⚠️ **`EcRow` 是 POD：加字段一定要在两个创建点都初始化**（`AddTableRow` / `AddCodeRow`）。
381:    第 16 轮加 `bParam` 时漏了，读到栈垃圾 `4402570`，于是"这一行有没有参数归属线"变成随机真。
382: 8. ⚠️ **原版代码行距不是恒定的 22**：`live_tabs\view0.png`（graphic.e）是 21，
383:    `live_flowcmd\view0.png` / `live_fold_jy\view_before.png`（精易模块、流程类命令）是 **18**。
384:    本工程用配置值 `nLinePitch`（默认 22）。量几何时**先在本张基准图上量行距**，
385:    别拿上一轮的数字套。几何表达一律用「行顶 + N」相对式，别写死像素。
386: 9. ⚠️ **第 15 轮「滚动消息对自绘编辑器无效」是错的**（第 3.2-9 条）：本轮实测
387:    `WM_VSCROLL(SB_PAGEDOWN)` → `scroll=460`、`WM_MOUSEWHEEL(-120)` → `scroll=66`。
388:    现在可以直接滚，不用再改采样源码。
389: 9b. ⚠️⚠️ **块结构的缩进规则反了**（用户 2026-10 一眼看出来的：「跳出循环和如果真你看看对齐了吗」）：
390:    原版是「块首 = 外层 +1 级、**块体和块首同级**」，本工程写成「块首 = 外层级别、块体 = 块首 +1」。
391:    现象：整棵块结构浅一级、流程竖线落在 25 而不是 47、而且「如果真的体」比「如果真」多缩一格
392:    （用户给的截图里 `到循环尾 ()` 明明和 `如果真` 同一列）。
393:    **别靠眼睛判断**：把行顶定下来后逐行量正文左边缘，量法见 2.1e。
394: 9c. ⚠️ **`►►` 的搜索范围一定要卡在 `[块首+1, 块尾)`**：新缩进语义下**块尾行和块首行同级**，
395:    不排除块尾就会把 `►►` 画到 `计次循环尾 ()` 那一行上（真踩过，见 `shots\flowtest5.png`）。
396: 
397: ### 3.1.2 第 16 轮踩的
398: 
399: 10. ⚠️⚠️⚠️ **`//` 行注释里的中文标点转成 GBK 后可能夹着 `*/`，把文件后面某个块注释提前关掉**
400:     —— 块注释下面**第一条语句被吃进注释里**，**编译不报错、运行行为不对**。
401:     真踩过：`ec_editor.cpp` 里 `// 易语言的括号是全角的：a3a8「（」…`。
402:     **别再试**：不要靠肉眼找 —— 它在源文件里长得完全正常。
403:     **正解**：`build.bat` 每次自动跑 `python tools\check_block_comments.py gbk`，不通过直接 `BUILD FAILED`。
404: 11. ⚠️ **被注释掉的那句"看不到效果"的假象**：日志/断点都在被吃掉的那句之前。
405:     判据：字符串/逻辑看着对但行为不对时，先跑 `check_block_comments.py gbk`。
406: 12. ⚠️ **全角标点不能一律按「GBK 双字节」吃掉**：`EcIdentHead` 里必须排除 0xA1/0xA3 区
407:     （标点），否则 `信息框（"…` 整段被当成命令名。
408: 13. ⚠️ **参数行行距 16 ≠ 语句行高 22**，第一行参数行顶 = 语句行顶 + 16。
409: 14. ⚠️ **`(a, b, ）` 尾部分隔符**：原版照画最后一个空槽，要用 `bLastSep` 区分「尾部空槽」和「压根没写」。
410: 15. ⚠️ **别把「叫得出名字的调用」当成「有参数的调用」**。
411: 16. ⚠️ **参数行是"叠"在行上的，不占纵向空间** —— 要在 `DrawAll()` **最后**画，
412:     并且 `Layout()` 里给 `m_nContentH` 补余量。
413: 17. ⚠️ **PowerShell 函数参数别用 `[string]` 收数组**（写 `[string[]]`）；
414:     `-match A -and -match B` 之后 `$Matches` 只剩最后一次的结果，要用 `[regex]::Match()`。
415: 18. ⚠️ **控制台探针的 GBK 副本别放 `gbk\` 根目录**（`build.bat` 是 `cl *.cpp`，会 `LNK2005: _main already defined`）
416:     → 放 `gbk\probe\`（见 `tools\build_argprobe.bat`）。
417: 
418: ### 3.1.1 第 14/15 轮踩的（仍然有效）
419: 
420: 19. ⚠️ **参照截图 `ref_dll_page.png` / `user_ref.png` 比真客户区原点低 4px** → 要 `--ashift=4`。
421:     别再为了让 crop 对上而改代码里的 4px。
422: 20. ⚠️ **`diff_png.py` 的 x0,y0 只是"共同区域起点"，不是偏移量**；偏移用 `--ashift`。
423: 21. ⚠️ **原版正文列 = 46 + 22n**（不是 69+22n）；流程列 = 上一级正文列 + 1。
424: 22. ⚠️ **`EcHeadWord`/`EcTrim` 是 `ec_editor.cpp` 里的 `static`**，`EClone.cpp` 里用不了。
425: 23. ⚠️ **读原版树的节点文字要 `TVM_GETITEMA`(0x110E) + `TVITEMA.pszText` 偏移 12**
426:     （64 位 PowerShell 发 `TVM_GETITEM` 永远读不到）。绕开办法：`treeinfo.exe` 倒档。
427: 24. ⚠️ **原版图片号是"每个工程自己一套"**，别按号跨工程对。
428: 25. ⚠️ **`ON_WM_COMMAND` 这个宏不存在**；`OnCommand` 是 `CWnd` 的虚函数。
429: 26. ⚠️ **`CEdit` 子控件的通知不走 `PreTranslateMessage` 的按键分支**：回车要按
430:     `pMsg->hwnd == m_wndEdit.GetSafeHwnd()` 判，失焦走 `OnCommand` 的 `EN_KILLFOCUS`（两条都要有）。
431: 27. ⚠️ **改了采样源码之后必须 `python tools\save_sample_backup.py`**，否则 `restore_sample` 会把新东西冲掉。
432: 28. ⚠️ **`LNK1104: cannot open ...EClone.exe` = 还有 EClone 进程在跑**；先 `Stop-Process` 再 build。
433:     本轮又踩：后台回归脚本还开着 EClone，build 直接失败。
434: 29. ⚠️ **`ON_NOTIFY(code, id, fn)` 的 `id` 是控件 ID 范围**，写 0 永远不匹配。
435: 30. ⚠️ **倒档文件（`ECLONE_DUMP_MODEL` / `ECLONE_DUMP_THEME`）是 GBK**，读的时候 `encoding='gbk'` / `GetEncoding(936)`。
436: 31. ⚠️ **原版截图坐标 ≠ dump 坐标**（原版最大化 `截图y = dumpy+8`；本工程 `截图 = dump-(134,57)`）。
437: 32. ⚠️ **字体**：原版正文字体来自「系统配置→程序显示→字体」，字形不可能 100% 一致，
438:     别把像素差都赖到布局上。
439: 
440: ---
441: 
442: ## 4. 试过的路径（成功 + 失败）
443: 
444: | 路径 | 结果 |
445: |---|---|
446: | 自造文本形式 `.e` 喂原版（`flow_ref.e`） | ❌ `views=0`，原版打不开（连它自带的 `dll_sample.e` 也打不开） |
447: | 用**原版自带例程**当样本拍 1:1 | ✅ 采用 —— 流程线真值的唯一可靠来源 |
448: | 在原版里真拖选一段量选区配色 | ✅ 采用（`probe_drag.ps1`）：`#000080` 底 + 白字，高 12px |
449: | 在原版里 `Ctrl+A` 量选中态（`probe_sel.ps1`） | ❌ 弹了个模态警告框，按键被吃掉（剪贴板拿到的是对话框文本）→ 改用拖选 |
450: | 按「用户说的」把 4 个循环尾关键字也隐藏 | ❌ 第 15 轮的误读；本轮对着原版例程改回"照画" |
451: | 按缩进推块边界（`ComputeBlocks`） | ❌ 块尾隐藏时把 `返回 ()` 吞进循环体 → 改成源码关键字栈配对 |
452: | `EcRow::bParam` 不初始化 | ❌ 流程槽凭空多出一堆 `-/+` → 两个创建点都初始化 |
453: | 点选定位用 `EC_FLOW_X(缩进)` 当正文 x | ❌ 偏 22px，插入点永远在行尾、框选选不出东西 → `TextXOfIndent()` |
454: | `InsertNewline` 不同步 anchor | ❌ 粘贴多行时换行全丢 → 同步 anchor |
455: | 用 `LP` 当 PowerShell 函数名 | ❌ 撞上 `Out-Printer` 别名 → `MkLp` |
456: | `probe_live.ps1` 顺便拍标签条 | ✅ 采用（标签条 1:1 基准的唯一来源） |
457: | `probe_sel.ps1` / `probe_drag.ps1` / `redump.ps1` / `sel_smoke.ps1` | ✅ 本轮新增的工具 |
458: 
459: ---
460: 
461: ## 5. 当前状态与下一步
462: 
463: **状态**：能编译、能跑；**验收 1~15 全过**（第 1 节的表）。
464: Dll 命令页与原版 crop **逐像素 0 差异**；代码页与原版 1:1 基准 **0.62%**；
465: 程序树/标签条/折叠标记/参数归属线都与原版一致；
466: **流程线本轮按原版自带例程 1:1 重做**（含循环尾关键字、►、▼、►►）；
467: **代码编辑器现在能点选/框选/复制/剪切/粘贴/全选/滚轮**。
468: 
469: **已知的剩余差距（按性价比排序）**
470: 
471: 1. **►► 的落点规则还有 1 个反例**（见第 6 节第 1 条）。本工程按
472:    「块首行之后第一条缩进深一级、且自身不是块首关键字的语句」实现，
473:    在 `live_flowcmd` 的 6 个块里对 5 个，第 6 个（`返回`演示：块体只有一条简单语句）
474:    原版把 ►► 放在**块结束后的那一行**。要再拍一两个样本才能定死规则。
475: 2. **「多行注释块」整个没做**：原版把连续的多行注释画成一个**绿框**
476:    （每行左边缘绿色实竖线 x=63、首末行各一条绿色 3 亮 3 灭横虚线拉到正文右边）。
477:    证据 `shots\live_fold_jy\view_before.png`（6 段全绿框，绿像素 21957 个）；
478:    而 `shots\live_tabs\view0.png` 与 `shots\user_ref_code2.png` 里绿像素都是 0
479:    —— 判据待定（大概只对「连续 ≥2 行」成立）。本工程只把 `'` 之后整行染成注释色，不画框。
480:    ⚠️ 那个绿框会让「绿像素计数」这类断言永久失真。
481: 3. **参数归属线的两处边角**（都还没实测）：① 嵌套调用（`信息框（取文本（a,b）, 0）`）里
482:    内层命令画不画参数行；② 展开态下**最后一条语句**的参数行会伸出内容区，原版怎么处理未知。
483: 4. **「全局变量 / 自定义数据类型 / 类模块 / 常量表 / 资源表 / 模块引用表 / 外部文件记录表」
484:    还没有文档页**（树节点建好了，`item data = -1`）。先拍基准：
485:    ```
486:    powershell -File tools\probe_orig_page.ps1 -File D:\eclone\shots\e_samples\jy.e -Node 全局变量 -OutDir shots\live_page_gvar
487:    ```
488: 5. **子程序表「备注」列宽**：原版 78、本工程 48（程序集表「保 留」列原版 54、本工程 42），规律未查明。
489: 6. **单元格编辑只做了"能改"这一层**：没有撤销、多行备注、插入/删除字段；
490:    `Shift+方向键`（现在按 Shift 也会更新 anchor，但没有"扩展选区"的语义）/撤销/查找替换没接。
491: 7. **参数行不跟随光标/编辑**（`DrawParamLines` 每次绘制重新解析，改了语句文字会跟着变，这是好事）。
492: 8. 编辑器窗口类名仍不同（本工程 `Afx:400000:8:10005:6:0`，原版两层 `…8:10003:900015:0` + `…b:…`）。
493: 9. 「系统配置」6 个页签只实现了「显示」页；`工具→支持库配置`（#32906）没做。
494: 10. 支持库树根节点多一个"−"展开框；树图标只有根和类别；类别顺序差 3 项。
495: 11. **代码区那 0.62%**：子程序表「备注」列宽 + 字体字形。
496:     另外**数值字色存疑**：原版 `信息框（"…", 0, ）` 的那个 `0` 实测是 `#800000`，
497:     而本工程内置配色的「数值前景」槽是 `#008000`（绿）。本轮没改（依据不足）。
498: 12. 设计器只能看，不能拖拽/缩放组件；无 `.e` 打开/保存。
499: 
500: **下一步的确切动作**（按顺序）
501: 
502: - 想接**►► 落点规则**：多拍两个原版例程（`C:\Program Files (x86)\e\samples\经典数学算法集\*.e`），
503:   用 `tools\probe_sel.ps1`（它会点进编辑器并把整窗拍下来）看「块体第一条是简单语句」和
504:   「块体第一条是嵌套块」两种情形各落在哪一行。
505: - 想调流程线：`python tools\patch_sample_flow.py` → `build.bat` →
506:   `powershell -File tools\code_shot.ps1 -Tag flowtest` →
507:   `python tools\flow_scan.py shots\flowtest.png 0 80 95 260`，跟 2.1d 的表逐项比 →
508:   **跑完 `python tools\restore_sample.py` + `build.bat`**。
509: - 想调参数归属线：`patch_sample_graphic.py` → build → `powershell -File tools\param_smoke.ps1`
510:   （两态截图落在 `shots\param_before/after/back.png`）。
511: - 想查原版某个函数：`python tools\ida_font.py "0x地址1,0x地址2" out.txt`；
512:   找"谁调用了某个 API"用 `python tools\ida_font.py calls <API名>`。
513: - 想查支持库里某个命令的参数名：`cmd /c tools\build_argprobe.bat` →
514:   `tools\argprobe.exe "C:\Program Files (x86)\e\lib" 信息框`。
515: - **改完 `.cpp` 里任何中文注释，构建时留意 `check_block_comments.py` 的输出**；
516:   `.ps1` 一定要跑一遍非 ASCII 计数（第 3.3-2 条）。
517: 
518: ---
519: 
520: ## 6. 未验证 / 存疑
521: 
522: - **►► 的落点规则**（第 5 节第 1 条）：本轮把「跨行数 ≥ 3 才画」这半条规则**用数据定死了**：
523:   * `shots\live_learn2\view0.png` 的 4 个 `如果/如果真`（体里各 1 条语句）→ 一个 ►► 都没有
524:     （x=20..66 全图扫，5 行高的藏青带 = 0）；
525:   * `shots\live_flowcmd\view0.png` 里嵌在循环里的 `如果真`（体里 1 条语句）→ 也没有；
526:   * `shots\live_fold_jy\view_before.png` 的 `如果真（是否为调试版（））`（体里 **2** 条语句）
527:     → 第一条上有一个（y=322..326，x=33/37）；
528:   * 循环块因为「首+体+尾」天然 3 行，永远满足这条。
529:   **还没定死的是"落在哪一行"**：`shots\live_sel\after_a.png` 的 6 个块里，
530:   第 1/2/3 个块（块体唯一那条简单语句）本工程的规则对；
531:   第 4/5 个块（块体第一条是嵌套 `如果真`）→ 原版落在**嵌套块结束后的那条语句**上，本工程也对；
532:   但第 6 个块（`.计次循环首 / 子程序1 (变量1) / .计次循环尾` 后面跟 `画板1.滚动写行`）
533:   原版把 ►► 放在**循环结束后的那一行**，本工程会放在 `子程序1` 那行。**这 1 个反例没搞懂**。
534: - **选区配色**只在**默认配色**下实测过（`#000080`）。换 `.clr` 后 `EC_CS_MARK` 的实际值
535:   与字段序是否与原版一致**没验证**（.clr 的 DWORD 序本来就有已知的 ±1 错位）。
536: - **滚轮在"鼠标悬停在编辑器上但编辑器没有焦点"时**会不会滚**没在真机上手动试过**
537:   （自动化测试是直接 `PostMessage` 到编辑器窗口的，绕过了焦点路由）。
538:   Windows 会把 `WM_MOUSEWHEEL` 发给光标下的窗口，理论上没问题。
539: - **参数归属线的嵌套调用 / 展开态最后一条语句**（同第 5 节第 3 条）。
540: - **`数值前景` 到底是哪个色**：原版 `信息框（"…", 0, ）` 里那个 `0` 实测 `#800000`，
541:   本工程「数值前景」槽是 `#008000`。本轮保持按语义取槽，没有凭一处证据改色。
542: - **多行注释绿框的判据**（第 5 节第 2 条）。
543: - **子程序数据行那个 `-/+` 到底折叠什么**：本工程折叠整个子程序体；原版点它会怎样**没实测**。
544: - **程序集表头 `#ECECEC`** 是"当前程序集高亮"还是固定底色：两个样本都只有"当前程序集"可见，无法区分。
545: - 字体：`'“'` 4px vs 2px 那类差异无法消除。
546: - `check_dll.py` 的第一条断言（`x=44 有全高竖线`）是**滚动位置相关**的，滚动一变就会假失败。
547: 
548: ---
549: 
550: ### 3.10 第 26 轮：设计器逐像素对拍（窗口/组件/选中框/属性数据）

> ⚠️ 本节是**事后补写**的（原因见文末"本节来历"）。内容都是本轮实测出来的，命令都能重跑。

0. **量设计器的专用工具**（这一轮新写的，先记住这几条）：
   ```
   python tools\des_cmp.py both        # 设计器页几何/颜色对拍：窗体偏移、纸色、点阵、组件箱
   python tools\des_chain.py both      # 设计器窗口链（视图/窗体/覆盖层 的类名+矩形+z 序）
   python tools\handles_navy.py orig|mine   # 按**颜色**在截图里找 8 个选中手柄，并拖 SE 手柄验证
   python tools\frame_measure.py <png> <x> <y> <w> <h>   # 选中框的横竖扫描线 + 颜色普查
   python tools\paper_probe.py <view.png> <x> <y> <w> <h> # "纸"的范围与点阵位置
   python tools\overlay_stability.py 4 # 连开 4 次，数藏青手柄像素：必须每次一样（稳定性）
   python tools\probe_rgnbtn.py both   # 设计期组件的类名/样式/矩形 对比
   python tools\probe_theme.py         # 标题栏是"经典"还是"v6 主题"
   ```
   ⚠️ 所有验证脚本都要**真鼠标**（`drive.py`），并且记住两条铁律：
   **抓屏（`ImageGrab.grab`）才代表用户看到什么；`PrintWindow` 只代表"那个窗口自己画了什么"。**
   两者一起用，就能一眼分开"没画" / "画了但被盖住"。

1. **选中框 + 8 个手柄：整个画法都是错的**（本轮最大的视觉 bug）。原版实测（`frame_measure.py`）：
   ```
   阴影   单位右下各外扩 5px 的 L 形实心 #C0C0C0（上、左没有）
   红框   单位矩形外扩 3px 的 3px 粗**实线** #FF0000（外边缘 -3、内边缘 0）
   手柄   8 个 **5x5 实心藏青 #000080**，中心 = 红框的四角与四边中点
   ```
   本工程原来画的是"1px 黑点线框 + 7x7 白色空框手柄 + 没有阴影" —— 全错。
   改完两边手柄偏移逐个对齐（相对单位右下角）：原版 `(-3,-3)…` / 本工程同。

2. **设计期那块"纸 + 8px 点阵"必须在覆盖层里画，而且覆盖层的重画必须"事件驱动"**。
   * 纸 = **`GetClientRect(窗体)`**，不是"覆盖层尺寸 − PAD"。覆盖层铺的是"窗口矩形 + 每边 PAD"，
     而窗口矩形含标题栏(≈24px)/边框(≈3px)，所以纸会多出 29px 高、6px 宽，糊到标题栏下沿。
   * 点在**窗体客户区原点**起铺、间距 8px（`系统配置->显示` 的 网点间隔，默认 8x8）。
   * ⚠️⚠️ **画在覆盖层里**：设计器视图是窗体的**父窗口**，父窗口的像素会被子窗口整块盖住
     （实测把纸/点阵挪到视图 `OnPaint` 里画，抓屏一个点都没有）。
   * ⚠️⚠️ **重画必须事件驱动**：`CECDesigner::EcHookFormWindow()` 用
     `SetWindowLongA(hwnd, GWL_WNDPROC)` 钩住设计窗体的窗口过程，它每次
     `WM_PAINT/WM_ERASEBKGND/WM_NCPAINT/WM_SIZE/WM_MOVE` 之后立刻 `RedrawOverlay()`。
     只用"挂完覆盖层之后定时补画 8 帧"**治不了根**：实测连开 4 次，藏青手柄像素
     `62 / 200 / 200`（那次纸/点阵/红框全没了）→ 修完 `[200,200,200,200]` 稳定。
     钩子用 `SetProp(hwnd, EC_FORMPROP_HOOK, ...)` 把「旧窗口过程 + 设计器」挂在窗口自己身上
     （多窗体各自独立）。`RedrawOverlay()` 因此从 protected 改成 public。

3. **`WS_EX_TRANSPARENT` 绝对不能加在覆盖层上**（第 26 轮真机踩到）：加了它真实鼠标点击会
   **穿过**覆盖层送给下面的窗体，`OnLButtonDown` 一次都不触发 —— 现象是"点组件选不中、
   8 个手柄怎么拖都不缩放"，而 `tools\resize_smoke.ps1` 却是绿的（它用 PostMessage 直接往
   覆盖层塞鼠标消息，绕过命中测试）。

4. **`CECDesigner::HitTest` 的坐标系**：收到的是**覆盖层坐标**，而原来拿窗体客户区矩形直接比，
   整体偏了 `(PAD,PAD)`（点组件边缘选不中）。已改成 `UnitRectInOverlay()`。

5. **标题栏样式**：本工程渲染成 **v6 主题**（Aero 淡渐变 + 亮蓝按钮），原版是**经典** 3D 标题栏
   （每行各通道 +1 的渐变，基色 RGB(160,184,208)，25 行）。两边 exe 都**没有 RT_MANIFEST**
   （`tools\dump_manifest.py` 验过），所以是 `SetThemeAppProperties(0)` 在 `InitInstance` 里
   关掉主题（uxtheme.dll 动态取，VC6 头里没有）。
   设计期第一个窗体**没有标题文字**（原版 `GetWindowText` 是空串）—— 别再 `SetWindowText`。

6. **组件箱**：原版**固定 4 列**（5/34/63/92，列距 29）；原来按面板宽度算列数 → 面板一宽就 6 列。
   面板宽度取原版展开后的内层尺寸 **135**（客户区 126），原来 196。

7. **组件的"默认属性数据"**（用户指正的关键点）：原来一律用 `pAllData=NULL, nAllDataSize=0`
   建组件 —— 库对空数据"算通过"，但**不会替你把属性表填成默认值**。新增：
   ```
   BOOL  EcGetAllPropertyData(lib, dt, hunit, BYTE** ppData, int* pnData);   // ITF 5
   void* EcCreateUnitWithData(lib, dt, pData, nData, ...);                   // ITF 1 带数据
   void* EcRebuildUnitWithProperties(lib, dt, hunit, ...);                   // ITF 5 -> ITF 1
   BOOL  EcGetDefaultPropertyData(lib, dt, cx, cy, BYTE** ppData, int* pnData);  // 探针 + 缓存
   void* EcCreateUnitDefault(lib, dt, HWND, UINT, x, y, cx, cy, HWND, BOOL);     // 新建正路
   ```
   组件建好立刻 `EcGetAllPropertyData` 存进 `EcUnitInst::pData/nData`（重建窗口/存 .e 要用；
   对应 e.exe 的 `sub_4AE230`）。"放下的默认尺寸"走 `EcDtDefaultSize()`（ITF 12）。
   ⚠️ **顺序不能反**：先写固定属性（左边/顶边/宽度/高度、标记=1、可视=真、禁止=假），
   **再** ITF 5 序列化。早先是先取字节后写尺寸 → 那份数据里 `宽度=0/高度=0`，组件全变 0x0 黑块。

8. ⚠️ **ITF 4 写 / ITF 6 读"对不上"是假警报**：加了 `createDefault` 诊断后实测
   ```
   createDefault lib=0 dt=44 want=(142,74 104x112) data=32 bytes winrect=(534,322 104x112) in-parent=(142,74)
   ```
   → ITF 4 写位置/尺寸是**真的生效**（窗口就在要的地方）。ITF 6 回读固定属性得 0 是因为
   **自定义控件类型根本没实现这几个属性的"读"分支**。判断"属性写进去了没有"要看**窗口本身**，
   不是看 ITF 6 回读。别去改 `EC_PROP_VALUE` 的布局（没错）。

9. `_EL_RgnButton` 是**第三方库 `ELogoPanel.fne`** 的类型（不是 krnln）——
   查 ITF 实现要去 `C:\Program Files (x86)\e\lib\ELogoPanel.fne`，krnln 的 ITF 表 `sub_10036470` 对它无效。
   两边真窗口参数**完全一致**（class/style/ex/rect），差的只是**属性数据**。

10. **"设计期组件画成一块黑"的结论**：`shots\e_samples\graphic.e` 是**原版保存过的工程**，
    那个组件的图片属性存在 .e 里的那份 ITF 5 字节；本工程 `ECLONE_DESIGN_DEMO` 是**纯代码演示**
    建的（空数据），所以只画出底黑。原版实测配色：`红 4289 / #F0F0F0 4073 / 灰 3023 / #C0C0C0 208 / 黑 55`。
    要一样就得做 **.e 解析**（把 `pAllData` 从文件读出来喂 `EcCreateUnitWithData`）。

11. 真机设计器行为对拍（`tools\cmp_desdeep2.py both`，两边一致）：
    ```
    组件选中      8 个 5x5 藏青手柄（偏移逐个对齐）
    组件 SE 手柄  原版 104x112 -> 128x136；本工程 -> 129x132（差在是否吸附格点，未定死）
    组件本体拖动  两边都移动、尺寸不变
    窗体选中      两边都不画手柄（红框=整张纸的外框）
    窗体本体拖动  两边都**不动**（原版行为）
    窗体 SE 手柄  本工程 380x250 -> 403x268
    ```

**本节来历**：本轮我用一个就地改写的 patch 脚本（先 `open(P,'w')` 再写回）在写回时抛了
`UnicodeEncodeError` —— 文件被**截断成 1.6 KB**。恢复办法（留档，万一再遇到）：
```
python tools\recover_handoff_merge.py     # 从 DSH 会话记录里按行号并集重建（本次恢复 2510 行）
```
⚠️ 教训：**就地改写大文件之前先 `Copy-Item` 备份**，或者改成"读-改-写新文件再替换"。


## 7. 关键坐标（路径/命令/文件/数字）
551: 
552: ```
553: 工程           D:\eclone\                  构建 D:\eclone\build.bat -> bin\EClone.exe
554: 构建           cmd /c "D:\eclone\build.bat"          （本机没有 pwsh！）
555: 运行           D:\eclone\bin\EClone.exe [designer]
556: ⚠️ 构建前先   Get-Process EClone | Stop-Process -Force   （否则 LNK1104）
557: 截图+窗口树    powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\shot.ps1 -Exe D:\eclone\bin\EClone.exe -Tag mNN
558: 布局回归       python D:\eclone\tools\check_layout.py shots\mNN.windows.txt   -> 14 passed, 0 failed
559: 
560: 逐像素对照
561:   通用           python tools\diff_png.py <A> <B> <x0> <y0> <x1> <y1> [out.png] [--ashift=N] [--bshift=N]
562:                  ⚠️ 跟 shots\ref_dll_page.png / user_ref.png 比要 --ashift=4
563:   行/列扫描      python tools\rows_scan.py <png> [编辑器x偏移] [y上限]
564:   标记扫描       python tools\marker_scan.py <png> [x0] [x1]
565:   颜色点阵       python tools\px_map.py <png> <x0> <x1> <y0> <y1>
566:   代码页 1:1     powershell -File tools\code_shot.ps1 [-Tag flowtest] [-ScrollPages N]
567:   流程线扫描     python tools\flow_scan.py <png> [x0] [x1] [y0] [y1]
568:                  （逐行列非白 run + 颜色分类：B=藏青 d=灰 #=黑 g=填充）
569:   选区/剪贴板    powershell -File tools\sel_smoke.ps1     （八条 [PASS]，本轮新增）
570:   强制重倒       powershell -File tools\redump.ps1        → shots\model_dbg.txt + model_dbg.fold.txt
571:                  也可以 PostMessage(editorHwnd, WM_USER+102, 0, 0)  ← EC_WM_REDUMP
572:                  （fold.txt 里有 scroll / anchor,caret / lines / fold row / param row）
573:   原版样本 1:1   powershell -File tools\probe_live.ps1 -File "<原版自带 .e>" -OutDir shots\live_xxx
574:                  推荐样本：C:\Program Files (x86)\e\lib\krnln\samples\流程类命令.e
575:                            C:\Program Files (x86)\e\lib\krnln\samples\一步一步跟我学写易程序二.e
576:                            C:\Program Files (x86)\e\samples\经典数学算法集\*.e（85 个）
577:   原版选中态     powershell -File tools\probe_sel.ps1  -File "<.e>"     （点进编辑器 + Ctrl+A）
578:   原版拖选       powershell -File tools\probe_drag.ps1 -File "<.e>"     （鼠标拖选 → 选区配色）
579:   原版折叠前后   powershell -File tools\probe_fold_orig.ps1 -File shots\e_samples\graphic.e
580:   原版某文档页   powershell -File tools\probe_orig_page.ps1 -File shots\e_samples\jy.e -Node 全局变量
581:                   （行号由 python tools\tree_row.py <trees.txt> <节点名> 算出）
582:   采样换/还      python tools\patch_sample_graphic.py / patch_sample_flow.py → 跑完 python tools\restore_sample.py
583:   刷新采样备份   python tools\save_sample_backup.py   （改了采样源码之后必须跑）
584:   构建守卫       python tools\check_block_comments.py gbk
585:   支持库参数名   cmd /c tools\build_argprobe.bat → tools\argprobe.exe "C:\Program Files (x86)\e\lib" 信息框
586:   IDA 资料       D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\
587:                    ida_work\e_copy.exe（.id0/.id1/.id2，idalib 直接开）；editor_impl.json；IDE-REPLICATION.md
588:                    ida_work\krnln.fne（**已分析并存盘**，funcs=5558）→ krnln\window_notes.md（窗口调度笔记）
589:                    krnln\decomp_window_cluster.txt / decomp_event_helpers.txt（原始反编译）
590:    krnln 逆向     python tools\ida_krnln.py survey|imports|calls|str|xref|dec|funcs
591:                   python tools\ida_krnln.py report <out.txt> "0x地址,0x地址,..."
592:    ⚠️ 两个坑      ① 地址**一定要加引号**，PowerShell 会把裸写的 0x100619e0 转成十进制再传进去
593:                   ② 别「跑一次 survey 存盘、再开一次 dec」：idalib 反复开关 krnln.fne 会随机 open rc=4
594:                     （func_qty=0），要一次进程里全倒出来（就是 report 模式）
595:    idalib         系统 Python312 `import idapro`；一次开关库约 30 秒
596: 
597:   shots 里最该看的几张
598:   cmp_flow_17.png        流程线对比：上=原版（流程类命令.e 的「到循环尾」块），下=本工程同一形状
599:   sel_drag.png           选区实拍（#000080 底 + 白字），与原版 live_drag\after.png 一致
600:   flowtest6.png          流程线自测图（配合 flow_scan.py 逐项比）
601: 
602: 本轮定死的原版数字（改代码时照抄，别猜）
603:   缩进           正文列 = 46 + 22n；块首 = 外层 + 1 级；**块体和块首同一级**；块尾和块首同一级；
604:                  块结束后回到外层（详见 2.1e，三个原版样本互证）
605:   流程线         竖虚线 3 亮 3 灭；x = 正文列(n-1) + 1（级别 1 → 47、级别 0 → 25）
606:                  块首横虚线：循环类 x..x+15、如果家族 x+2..x+15
607:                  块首行的 ►：只有循环类，x+12..x+15，4 宽 7 高，逐行 1/2/3/4/3/2/1
608:                  块尾可见（循环尾）：横虚线 x..x+15，**没有 ►**
609:                  块尾不可见（如果结束）：竖线延到「块体最后一行行顶 + EC_CODE_H」，▼ 逐行 1/7/5/3/1
610:                  ►►：x-14 / x-10，各 3 宽 5 高（逐行 1/2/3/2/1），藏青，竖直中心 = 行中线
611:   关键字可见性   画：如果/如果真/判断开始/计次循环首/判断循环首/循环判断首/变量循环首
612:                      ＋计次循环尾/判断循环尾/循环判断尾/变量循环尾
613:                  不画：如果结束/如果真结束/判断结束/否则/默认
614:   选区           #000080 底 + 白字，高 12px（= 文字高，不是整行）
615:   代码区         TOP_PAD=4；正文 x = 46 + 22n；流程竖虚线 x = 24 + 22n
616:   参数归属线     第一行参数行顶 = 语句行顶 + 16、行距 16、文字再下 4px
617:                  `※` 左边缘 x = 语句正文列 + 44；左侧竖虚线 x = 正文列 + 31
618:   折叠标记       横杠 x=22..30（9px）、竖杠正中 x=26、颜色 #000080、画在白底上
619:   表格           程序集表头整行 #ECECEC；子程序表头 #E6EDE4；数据行白底
620:   程序树         图标号/顺序见 2.2
621: 
622:   注册表         HKCU\Software\FlySky\E\EClone40（本工程；**别动** …\EInf40 = 原版）
623:   配色目录       C:\Program Files (x86)\e\clr\*.clr（13 个，只读）
624:   原版样本       C:\Program Files (x86)\e\e.exe（5.9.0.0）；shots\e_samples\{graphic.e, jy.e, dll_sample.e}
625:   SDK 权威       C:\Program Files (x86)\e\sdk\cpp\elib\lib2.h（GBK，1392 行）
626:   VC6 / RC       C:\Program Files (x86)\Microsoft Visual Studio\VC98\Bin\ ；…\Common\MSDev98\Bin\RC.EXE
627:   文本形式语法   D:\逆向\cases\e_epl\refs\OpenEpl\TextECode\TextECode\Grammar\EplParser.g4

(Output capped. Showing lines 1-627. Use offset=628 to continue.)

// ec_editor.cpp -- 易语言风格「结构化」代码编辑器的实现（全部自绘）
rem Full round-19 regression list from HANDOFF.md section header. ASCII only.
> 工作区 `C:\Program Files (x86)\e` 对当前用户不可写（`Program Files` 需管理员，实测 `Set-Content` 报"访问被拒绝"），
> 因此本文件与全部产物放在本项目目录：`D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\`。
> 第 21 轮第十二/十三批 = **F3/Shift+F3 寻找验收 + 20 步回归全绿**：新 `tools\find_smoke.ps1`
> （krnln.fne / 任意第三方 .fne）通过 `LIB_DATA_TYPE_INFO(+44).m_pfnGetInterface → ITF 1 = PFN_CREATE_UNIT`
> ② `工具→支持库配置`（原版命令 #32906 / 模板 **RT_DIALOG id=321**）实现，并能**运行期动态载入/卸载**
> 原版在 `C:\Program Files (x86)\e\e.exe`（sha256 `f43396685553eb95b6e4fb31909d7af0e0febd63d19e4a5e3a01ddce9a00359c`）。
- **done 的可观测条件**：（a）核心库的规模以数字给出（命令/类型/属性/常量数）；（b）IDE 与核心库的技术栈被识别并给出证据；（c）`.e → EXE` 的编译链路被证实；（d）"界面""核心库"分别给出难度分层与最省事路径。
> 类直方图 `Button:44/Static:18/Edit:12/ComboBox:3/**SysTabControl32:1**` ⇒ 确实是页签对话框，
> （基本组件 = 核心库 / 扩展组件 = 其它库，与原版两个单选一致）、画布上可**单击放置/拖出矩形/选中/拖动/Delete 删除**、
// ------------------------------------------------------------------ 度量
> cmd /c "D:\eclone\tools\regress.bat"  -> 24 步回归；最近一次 shots\regress_r28.log 全绿（[FAIL] 0 条）
        powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\shot.ps1 -Exe D:\eclone\bin\EClone.exe -Tag check
| 样本测绘 | `Get-ChildItem` 递归统计 | 易语言 5.9.0.0，535 MB / 4151 文件；`e.exe` `lib\krnln.fne` `lib\krnln.fnr` `static_lib\krnln_static.lib` `linker\vc98linker-c` `sdk\` |
| PE 指纹 | `scripts\pe_recon.py`（pefile） | e.exe/krnln 均 PE32、linker 6.0（VC6）、e.exe 无 `.reloc`；krnln 导出 307 符号含 `GetNewInf` |
| 核心库命令表 | `scripts\extract_krnln.py` 离线解析 `GetNewInf` 返回的 `LIB_INFO` | 688 命令（393 全局 / 295 方法）、23 类别、51 数据类型、335 常量 → `krnln_fne_commands.json/.md` |
| 数据类型字段 | `scripts\init_patch.py` 用 capstone 重放 `.text` 中静态初始化写入（396 次） | 43 个窗口/组件、627 属性、233 方法；结果写入 `krnln_fne_datatypes.json` |
| 事件表 | `scripts\extract_events.py` 从 `pEventBegin` 按 20B 步长走到 `pPropertyBegin` | 约 64 条事件（下界估计）→ `krnln_fne_events.json`；`窗口`13 项、`按钮`→被单击 |
| 编译链路 | `tools\link.dll` 字符串 + `sdk\static_docs\index.html` 原文 + `static_lib\krnln_static.lib` 符号 | 生成 COFF obj（含 `libinfos/libinfoN/libN_name/libN_guid/libN_cmdfuncs` 节）→ VC6 `LINK.EXE`；`krnln_static.lib` 含 `_krnl_MCallKrnlLibCmd` |
        python D:\eclone\tools\check_layout.py D:\eclone\shots\check.windows.txt   -> 14 passed, 0 failed
| 现有开源实现 | web_search + 抓 e-packager 架构文档 / 精易论坛开源帖 | 核心库已被黑月/BlackMoonKernelStaticLib 完整重写并开源；界面/窗口工程仍是空白 |
| 编译流水线拆段 | `scripts\grep_pipeline.py` 定位阶段串 + `scripts\find_compiler.py` 扫 `push/mov imm32` 引用点 | 预处理(0x430916) → 预编译(0x4c693f) → 编译(0x41886b/0x434e41) → 写 .obj/.lib(0x436d84) 是**四个不同阶段**，不是同一个东西 |
| 运行时宿主接口 | `scripts\dump_entry.py` 反汇编 `krnln.fne!GetNewSock` | `sub eax,1000; cmp eax,3; jmp [eax*4+表]` → 恰好 4 个 ITL 接口 1000–1003，与官方 .asm 一致 |
| 官方封装器源码 | 读 `epackage\source\{epackage,dll_pkg,el}.asm` + `_build.bat` | **随安装包提供**（MASM32）：找 `krnln.fnr`→注册表 `HKCU\Software\FlySky\E\Install\Path`→`GetNewSock(1000)`→跳 E-code |
| IDE 的回填代码 | `scripts\check_pack_markers.py` 搜标记常量并反汇编 | e.exe `0x4245eb mov edi,0x12345678`、`0x424d02 cmp [edx+eax],0x86543210` → **打包/回填逻辑只在 `0x4245xx–0x424Dxx` 几 KB** |
| 链接器插件 ABI | `scripts\dump_entry.py` 反汇编 `link.dll!Link` | `Link(void*)` 单参数、`ret 4`，读 `[arg+0x3C]`（PE e_lfanew 位）、回调 `[arg+0x24]` |
| 本机工具链 | `Test-Path` / `Get-ChildItem -Filter cl.exe` | **有完整 VC6**：`C:\Program Files (x86)\Microsoft Visual Studio\VC98\Bin\CL.EXE` |
| 全树 EXE 指纹 | `scripts\exe_sweep.py`（249 个 PE 的节区+特征） | 非静态编译产物＝小 EXE + 含 `krnln.fne/fnr` 串（`tools\about.exe` 26 KB 等）；静态编译产物＝含 `Afx` 无 `krnln` 串（`mksetup.exe`） |
| IDE 功能面量化 | `scripts\ide_features.py` | 111,142 条字符串；调试器串（条件断点表达式编译/监视表/反跟踪）、支持库 221 条、加密狗 125 条 |
| 代码规模 | `scripts\scale_metrics.py` | e.exe .text 2,045,219 B、序言 973 个；krnln.fne .text 1,046,706 B；静态产物 mksetup.exe .text 625,774 B |
| **IDE 实机走查** | 启动 IDE + `scripts\ide_probe{,2,3,4}.ps1` 枚举窗口/控件 + PrintWindow 截图 | 主框架类 `ENewFrame`；界面 = MFC(`AfxControlBar42s`/`AfxMDIFrame42s`) + Win32 标准控件 + 起始页内嵌 IE；共 139 个子窗口。产物 `ide_probe\ide_frame_only.png`、`ide_opened.png`、`ide_frame_children.txt`、`ide_opened_windows.txt`；写进 `IDE-WALKTHROUGH.md` |
| 窗口类的归属映射 | `scripts\window_class_owner.py` | `Menulwy`/`ColorMade`/`Setfd` 来自 `lib\iDraw\plugin\*.dll`；`ScintillaForEIDETools` 来自 `lib\iDraw\superTools\plugin\eOutPutControl.dll`；`MyHyperLinker`/`CCustomTabCtrl` 在 e.exe 内；→ **用户装的 IDE 带第三方魔改插件** |
| 核心库的窗口类清单 | `krnln_fne_ascii.txt` 过滤 `^_EL_` | **krnln 注册 24 个窗口类**：`WTWindow` + 23 个 `_EL_*`（Label/RgnButton/PicBox/ShapeBox/DrawPanel/Tab/Grid/Timer/CommonDlg/Printer/ColourPopup/HyperLinker/DataSource/Provider/DBProvider/ODBCDB/ODBCProvider/ClientSock/ServerSock/UDPSock/SerialPort/ErrorControl/PicWnd/HideOwner）。编辑框/按钮/列表框等无 `_EL_` 类 → 直接用系统标准控件类 |
| 设计期=真实窗口 | 打开工程副本后枚举 | 设计器里是真实 `WTWindow`(380×250) 里挂着真实 `_EL_RgnButton`(104×112) → 所见即所得不靠另一套渲染引擎 |
| 主菜单形态 | `ide_windows.txt` + `scripts\pe_menu.py` | `ENewFrame` 上 `GetMenu()` 为空 → 菜单是自绘的；e.exe 资源里仍有 17 个 `RT_MENU`（主菜单 id=127/4496B） |
        python D:\eclone\tools\diff_png.py D:\eclone\shots\ref_dll_page.png D:\eclone\shots\dll_view.png 0 0 460 394
| 验收 3 | `python tools\check_layout.py shots\<tag>.windows.txt` → `14 passed, 0 failed` |
| 验收 4（本轮） | 编辑器是**自绘的结构化编辑器**：四类表格列宽与原版**逐列相同**（子程序块 `174,72,36,36,36,426`；变量表 `96,96,36,36,96`，见第 2 节复现命令）；行距 21px、代码行 18px、网格 `#888888`、表头填充 `#E6EDE4`/`#D9E3F0`、字号 12px 宋体 全部与原版实测一致；`shots\cmp_editor_table.png` 上下对齐 |
| A | **左侧面板的标签页在底部** | `工作夹` = `#32770`(1,137 224x710) → `SysTabControl32`(**4,140 218x704**，几乎占满整个 pane)，`SysTreeView32` 在 **y=144（顶部）** | 标签页在**顶部**、只占 20px 高 | 对话框模板里给 tab 控件加 **`TCS_BOTTOM`**，并让它覆盖 pane 的大部分高度，内容控件放在它上方 |
- ⚠️ **krnln 的字符串有两种编码**。库名/命令名是 **GBK**（`b4 b0 bf da` = 窗口），不是 UTF-8。用 UTF-8 优先解码会把中文变成"绯荤粺鏍稿績鏀寔搴?"这种假 UTF-8 乱码；而 PowerShell `Get-Content` 读 UTF-8 文件会显示成 GBK 乱码。判据：先 UTF-8 试解码、失败再 GBK。
- ⚠️ **`LIB_INFO` 里数据类型表在文件里全是 0**。名称/状态/属性数/方法数/指针都是映射时由一个 `.text` 静态初始化函数写入的。直接读文件只会得到 index 0（窗口）有数据、其余 50 项全 0 的假象。必须重放 `mov dword ptr [imm], imm/reg` 写入。**别再照 `lib2.h` 布局直读文件然后宣布"只有一个组件"。**
| D | **树/列表项带小图标** | 支持库树、组件箱、底部标签页（`支持库/程序/属性`）都有 9x9 小图标 | 全纯文字 | `e.exe` 的 `RT_BITMAP` 里 `157/158/159/575/577` 都是 **9x9**（很可能就是这些图标），用 `tools\dump_bitmaps.py` 导出后做成 `CImageList` |
| E | **工具条多行、图标更大** | 原版启动后可见 2~3 行工具条；另有 `id 231`(272x15, 17 钮) / `296`/`297`(224x15, 14 钮) / `146`(112x15) / `150`(80x15) / `280`(64x15) 等多张条带位图 | 只 1 行、16x15、被 MFC 拉满整行宽 | 用「标准工具条 + 一个 `CBRS_SIZE_DYNAMIC` 的第二行」；`SetSizes(CSize(23,22), CSize(16,15))` 调按钮尺寸 |
| F | **窗口组件箱默认收起** | `AfxControlBar42s vis=True 1920,120 0x726 窗口组件箱` → **宽度 0，收起** | 默认展开 | 创建后 `ShowControlBar(&m_wndComp, FALSE, FALSE)` |
- ⚠️ **PowerShell here-string 传多行 Python 很脆**（`@'...'@` 里出现引号/`@` 就报"缺少终止符"）。本任务多次踩坑，改为写 `scripts\*.py` 文件后再执行。
- ⚠️ **`Start-Process -ArgumentList` 会把带空格的路径拆开**（`C:\Program Files\...` 变成 `C:\Program`）。要用 `& python "路径"` 或在参数里显式加引号。
python "%ROOT%\tools\check_dll.py" "%ROOT%\shots\dll_view.png" "%ROOT%\shots\dll_view2.png" "%ROOT%\shots\ref_dll_page.png"
- ⚠️ **别把 "预编译" 和 "编译器" 当成同一个东西**。e.exe 里 `预编译` 只出现 1 次（"预编译处理完毕。"@0x4c693f），而 `正在预处理现行程序`、`正在编译现行程序`、`.obj`/`.lib` 各自在不同的代码页（0x430000 / 0x418000 / 0x436000）。四个阶段四份实现：前端 → 代码生成 → 封装（官方 .asm） → 链接（link.dll+VC6）。按"一个函数搞定"的思路找不到东西。
- ⚠️ **`gbk_runs()` 的输出含大量二进制噪声**（把任意相邻双字节当 GBK 解），做"字符串集合运算/命中率统计"时会得到假结果（我用它做内嵌运行时判定就失败了：e.exe 13.7% vs mksetup 20.6% vs 无关程序 6%，信噪比不够）。要判"某段代码是否被链接进来"，用类名/RTTI 指纹或 PE 结构特征，别用字符串命中率。
- ⚠️ **`.ps1` 脚本必须写成纯 ASCII**。本机的 shell 按 ANSI/GBK 读 `.ps1`（`powershell -File x.ps1` 和直接 `& x.ps1` 都一样），UTF-8 无 BOM 的中文会被解析成乱码并直接报"缺少 )"之类的**语法错误**。Python 脚本没有这个限制（`python x.py` 读 UTF-8 源码），所以中文只写在 `.py` 里。
- ⚠️ **64 位 PowerShell 枚举 32 位进程的模块不可靠**：`$p.Modules` 和 `tasklist /m` 对 e.exe（32 位 + WOW64）只返回 `ntdll.dll, wow64*.dll`，看不到 `krnln.fne` 等真实模块。要判断"谁注册了某个窗口类"，用**磁盘字符串检索**（`window_class_owner.py`），不要用运行时模块列表。
- ⚠️ **运行 IDE 会改文件**：这台机器上 IDE 启动时加载了带"【自动保存】30 秒"的助手插件，所以**不要直接打开安装目录里的 `.e` 样例**，先复制到工作目录再打开（`ide_probe\sample\` 下就是副本）。
- ⚠️ **用户这台 IDE 是魔改版**：`lib\iDraw\`（自绘菜单/取色器/Scintilla 输出面板）和 `lib\ExuiKrnln\` 都是第三方扩展。看界面截图时不要把插件提供的外观当成易语言原版。
- ⚠️ **PowerShell 里用 `@' ... '@` 或 `@" ... "@` 包多行 Python 极不可靠**（内容里出现 `r"..."`、`@`、`?` 就会报"缺少终止符"或把引号吃掉）。本轮又踩两次。**一律写成 `scripts\*.py` 文件再 `& python 脚本.py`。**
| **验收 9** | `python tools\diff_png.py shots\ref_dll_page.png shots\dll_view.png 0 0 460 390 --ashift=4` → **差异像素 = 0** |
   - 产物：`shots\live_windows.txt`（用户实例）、`shots\live\tree.txt` + `shots\live2\tree.txt`（自起实例）。
python "%ROOT%\tools\diff_png.py" "%ROOT%\shots\live_tabs\view0.png" "%ROOT%\shots\code_view.png" 0 0 1174 388
| **验收 12（本轮）** | 构建自带的守卫：`build.bat` 里跑 `python tools\check_block_comments.py gbk`，不通过就 `BUILD FAILED`（见 3.1-14） |
3. **e.exe 自己管光标、自己画字**（IDA，`D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\ida_work\editor_impl.json`）：
| 验收 8 | `tools\dll_smoke.ps1` + `python tools\check_dll.py …` → `[PASS]` + `46/37/80 passed, 0 failed` |
| **验收 9（本轮）** | `python tools\diff_png.py shots\ref_dll_page.png shots\dll_view.png 0 0 460 394` → **差异像素 = 0** |
| 验收 9 | `python tools\diff_png.py shots\ref_dll_page.png shots\dll_view.png 0 0 460 390 --ashift=4` → **差异像素 = 0** |
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\libcfg_smoke.ps1"
| 验收 3 | `python tools\check_layout.py shots\check.windows.txt` → `14 passed, 0 failed` |
| 验收 12 | 构建自带的守卫：`build.bat` 里跑 `python tools\check_block_comments.py gbk`，不通过就 `BUILD FAILED` |
| **验收 13（本轮）** | `tools\sel_smoke.ps1` → 八条 `[PASS]`：点选定位 / 框选出选区 / 选区涂 `#000080` / Ctrl+A / Ctrl+C / Ctrl+X / Ctrl+V / 滚轮 |
   python tools\cmp_hints.py shots\mine_hints.txt 0 1 398 1224 2 14 400 401 1235 1225 399 460 466 1144 1167 1194 442 423 447 448 454 467 472 1211
| **验收 18（第 20 轮）** | `tools\libcfg_smoke.ps1` → 七条 `[PASS]`：① 对话框 489x464 px + 11 个原版控件 id 齐全；② 列表列出 lib 目录下 71 个 .fne；③ 查找框过滤；④ **单击列表行能翻转勾选框**；⑤ 点确认后支持库**运行期真的载入**（工作夹树 1560 → 1819）；⑥ 重开对话框该项显示为已勾选；⑦ 取消勾选后**运行期真的卸载**（树回到 1560） |
| **`ida_krnln.py`** | krnln.fne / e.exe 的 IDA headless 查询工具：`base / big / dw / gbk / decstr / decimp / decxref / decname / batch / pat / imm / calloff`…（第 18 轮用它把 krnln 全库伪 C 倒进 `…\krnln\out\`，结论写在 `…\krnln\krnln_anatomy.md`，见 HANDOFF §2.5/§2.5d） |
| **`extract_lib_bitmap.py`** / `match_tree_icons.py` | 抠 krnln 的 `RT_BITMAP LIB_BITMAP`（336x13/4bpp/16 色）并与 `src\res\tree_icons.bmp` 比对。⚠️ 格宽与格序未定死，**暂不替换图标** |
| **`designprobe.c` + `build_designprobe.bat`** | **脱离 GUI 的探针**：`LoadLibrary` 任意 `.fne` → 按 `LIB_DATA_TYPE_INFO+44` 取 ITF 1 → 用 `PFN_CREATE_UNIT(NULL,0,…)` 真建组件窗口，打印类名/样式/矩形 + 库反调宿主的通知码统计。退出码 0 = 全过 |
  1. **编译器核心测绘**：从 `0x004245xx`（打包回填）与 `0x00436d84`（写 .obj）两个锚点分别向上/向下用 IDA 做函数级测绘，切出"代码生成器"的代码区间与调用图。这是唯一没有替代品、也没有资料的部分。建议直接上 IDA（`D:\ida\ida.exe`，本任务未用），先 `ida.exe -A -S<脚本>` 全量分析 e.exe 再按字符串锚点回溯。
| **验收 23（第 21 轮）** | `python tools\check_comment_box.py shots\cmt.png 22` → **7 passed, 0 failed**（多行注释绿框：横线 2*行距 一条、2px 高、11 亮 1 灭；左竖线 2px 宽、每行一段 12px）；同一脚本在原版基准 `shots\live_fold_jy\view_before.png` 上也是 7 passed |
  3. **事件数校准**：`LIB_DATA_TYPE_INFO` 的 `m_nEventCount`(+28) 在载入后为 0；本项目的静态初始化重放里该偏移 0 命中，但**未被跟踪的寄存器写入会被我的重放静默丢弃**（`val is None` 就跳过），所以"从未写入"这个结论不严密。要校准就在 `.text` 里全量枚举 `mov dword ptr [imm32], reg32` 并记录**所有**目标地址（不管值可知与否），再看 +28 是否出现。
| **`fold_smoke.ps1`** | **子程序收缩冒烟**：读 `ECLONE_DUMP_MODEL` 的模型算出折叠标记坐标，`PostMessage` 点它，再发 `EC_WM_REDUMP`(WM_USER+102) 重倒模型，比对行数 —— 期望 `[PASS] collapse works` |
| **验收 16（本轮）** | 缩进规则见 2.1e：正文左边缘必须是 `' 注释` 46 / 块首 68 / 嵌套块首 90 / **嵌套块体 90**（和块首同级）/ 块后 68。自测图 `shots\flowtest6.png` + `cmp_flow_17.png` |
| **`extract_progtree_icons.py`** | 从 `shots\live_progtree\tree0.png`（原版实测）与 `shots\ref_prog_tree.png`（用户截图）抠出 15 个 16x16 程序树图标 → `src\res\progtree_icons.bmp` |
| **`dll_smoke.ps1`** | **Dll命令页冒烟**：点工作夹「程序」标签 → 点程序树第 4 行(Dll命令) → 确认编辑器切到 Dll 文档（模型倒档里出现 Dll 表）+ 两张 1:1 截图；末尾倒出程序树做核对 —— 期望 `[PASS] program tree selection switched the document page` |
| **验收 14（本轮）** | `tools\cell_smoke.ps1` → `[PASS] block-end keywords: end-suffix hidden, loop-tail rendered` |
| 列宽规则 | **列宽 = 该列所有单元格文字宽度最大值 + 12**（左右各 6 内边距）；跨列单元格（子程序行的"备注"占 2 列）不参与单列统计，但要保证合并后放得下 |
| **验收 16（第 17 轮）** | 缩进规则见 2.1e：正文左边缘必须是 `' 注释` 46 / 块首 68 / 嵌套块首 90 / **嵌套块体 90**（和块首同级）/ 块后 68。自测图 `shots\flowtest6.png` + `cmp_flow_17.png` |
| 颜色表 | 流程关键字/数据类型 = 蓝 `#0000FF`；支持库命令 = 紫红 `#800000`；括号运算符 = 藏青 `#000080`；字符串 = 青 `#008080`；注释/备注 = 绿 `#008000`；表格里的子程序名/程序集名 = 藏青 + 下划线；`✔` = 紫红；变量名/参数名 = 黑 |
| **验收 20（第 20 轮）** | 设计器背景与原版逐像素对齐（见 2.7）：工作区 `#A0A0A0`、窗体"纸" `#F0F0F0`、点阵 1px 纯黑 / 间距 8px / 锚在窗体客户区原点；基准 `shots\live_des_orig\{form0,view2_client}.png` |
       D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\krnln\krnln_anatomy.md
IDE         e.exe                                5.9.0.0 / 2020-02-12 / PE32 / linker 6.0
| 1 | 原版 **x=44 那条竖线只在表格行里**，块间 8px 空隙处断开、表格下面的代码区**根本没有**；本工程画成全高 | `DrawAll()` 里额外补了一条全高竖线 | **删掉**那条；表格自己的左边界由 `DrawTableRow` 逐行画 |
| 2 | 每个填充行的底色比原版**少 1 行**（原版 20 行，本工程 19） | `FillSolidRect(..., EC_CELL_H - 1)` | 改成 `EC_CELL_H`（=20） |
| 点「公开/易包/参考/可空/数组/静态/传址」这些格子 | `HitTestCell`（列语义 = **同表、同格数的最近一条表头行**的对应格）→ `ToggleFlagCell` 改源码那一行的第 N 个逗号字段（字段号按声明头定，见 `EcFlagField`）→ `Rebuild()+Layout()` | `tools\cell_smoke.ps1`：点 `_图形按钮1_被单击` 的公开格，模型从 `[][][][]` 变 `[][]*[][]` → `[PASS] flag cell toggled` |
| 双击文字格（名/类型/备注） | `BeginEditCell` 在该格矩形上建一个 `CEdit`（第一次双击时才建），回车/失焦提交（`CommitEditCell`），ESC 取消；提交走 `SetCellText`（备注 = 最后一个字段） | 同脚本后半段：双击子程序名格 → `WM_SETTEXT` → 回车 → 模型里出现新名字 → `[PASS] in-place text edit committed` |
| 5 | `库文件名:` / `在库中对应命令名:` 的冒号比原版细（原版 2x2 方点，本工程 1x1） | 标签里用的是**半角** `:` | 改成**全角** `：`（原版就是全角，改完 Dll 页差异归 0） |
ABI 头      sdk\cpp\elib\lib2.h  mtypes.h  mem.h  PublicIDEFunctions.h   sdk\delphi\e.pas
    → 改用 `CreateEx(this, **TBSTYLE_FLAT**, ...)`（AFXEXT.H 里 CreateEx 的默认值就是 TBSTYLE_FLAT，
| `shots\live_fold_jy\view_before.png`（精易模块 `_启动子程序`） | `如果真` 68、体 68、体 68、块后 `m_CodePage` 46 |
| 1 | 「左侧文件夹、自定义数据类型、全局变量、类」 | `shots\live_jy\prog_tree.txt`（我起原版 e.exe + 精易模块.e，treeinfo 倒出 5189 个节点）＋用户截图 `shots\ref_prog_tree.png` | `EcBuildProgTree` 重写：新增**文件夹**(img=16 黄文件夹)、**类模块**(img=0)，程序集图标改成原版那个菱形(img=3，窗口/普通程序集**同一个**)；全局变量/自定义数据类型**有内容才建节点**且子项同图标；采样源码加了 `.文件夹/.类/.全局变量/.数据类型` | 重新倒档：`shots\live_progtree_mine\prog_tree_u8.txt`（23 节点）＋ 1:1 截图 `shots\live_progtree_mine\tree0.png`，与用户截图逐节点核对 |
| 2 | 「程序编辑界面的表头在下侧」（= 文档标签条） | `shots\live2\tree.txt`：原版那条是窗口类 **`CCustomTabCtrl`**、矩形 `412,685 1195x22`，挂在 MDI 客户区**下侧**；1:1 截图 `shots\live_tabs\tabbar0.png` | `CECChildFrame::LayoutViews()` 把标签条挪到底部；`CECTabStrip::OnPaint()` 按原版重画；标签文字改成 `_启动窗口` / `启动窗口程序集` | `shots\tabbar_mine.png`（1195x22，`code_shot.ps1` 现在会一起拍）与 `tabbar0.png` 逐像素比：tab1 文字墨迹 **(15,66) 两边完全一致**、tab2 **(86,174) vs (86,175)**、斜边位置一致；整窗 `shots\mtab.png` |
| 3 | 「折叠后的加减背景色都不对」 | `shots\live_fold\view_before.png`（原版 graphic.e 1:1）：`-`/`+` 就是藏青 `#000080` 的 **9x9 十字/横杠**（x=22..30），画在**白底**上，收起时行底色不变 | 删掉 `DrawAll()` 里给收起行补的 `EC_C_SELBG` 浅蓝底；横杠改成 9px（22..30）、竖杠在正中 x=26 | `fold_smoke` 仍 `[PASS] collapse works` |
| 4 | 「带箭头啥的都不对」 | 同上图：子程序体第一条语句的流程槽里有一个 **↓**（竖线 x=9、高 7px ＋ 底下一个 7x4 实心三角），颜色藏青 | `ECM_DOWN` 这个标记以前**只设不画**，现在在 `DrawCodeRow()` 里真画出来 | 见 2.3 的代码页对拍（差异从 2.94% 降到 0.62%） |
本工程：`EcRow::bParam` + `m_paramHit[]`（命中框同 `m_foldHit`）+ `CECEditor::m_bParamShow`。
    `OpenProcess`+`VirtualAllocEx` 在目标进程里开内存当 lParam，再 `ReadProcessMemory` 读回来；
| 用户给的 `shots\user_ref.png`（精易模块）代码区 | `如果真 (是否为空 (超级列表框类名))` 的体（`超级列表框类名 ＝ "SysListView32"`）后面**直接**是下一个 `如果真 (…)` —— **中间没有 `如果真结束`**（第 13 轮把它当源码抄进 `patch_sample_ref.py` 的 fixture 是错的） |
| `shots\live_acc\view0.png`（ACCESS示例.e，我起原版拍的 1:1） | `如果 (…执行（…）＝假)` / `信息框（"执行SQL错误!")` 之后**没有 `否则` 行**，直接就是 `信息框（"执行SQL成功!")`；两个 `如果结束` 也都不画。分支是用流程槽里那个 **►** 表示的 |
| `shots\live_learn2\view0.png`（一步一步跟我学） | `如果真` 68 / 体 68；`如果` 68 / 体 68；**嵌套**的 `如果真` 90 / 体 90；内层块之后 68 |
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "[FAIL] no editor window"; Stop-Process -Id $p.Id -Force; exit 1 }
    iBitmap: 0 1 2 |SEP| 3 4 5 7 8 |SEP| 22 |SEP| 10 11 12 |SEP| 13 14 15 16 17 18 19 20 |SEP| 21
pwsh -File D:\eclone\tools\shot.ps1 -Exe D:\eclone\bin\EClone.exe -Tag m17_designer -ExeArgs designer
    // ---------------------------------------------------------- 内部
BAD = {(255,255,255),(128,128,128),(236,236,236),(105,105,105),(192,192,192),(0,0,128)}
# The 1:1 editor shot is the visible viewport only (~394px), so make sure the statement
    ".......##", "......###", "##...###.", "###.###..", ".#####...", "..###....", "...#.....",
powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\shot.ps1 -Exe D:\eclone\bin\EClone.exe -Tag m_ref3
pwsh -File "D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\scripts\ide_probe4.ps1" `
    "01110000000000", "12211000000000", "12321100000000", "01232110000000",
          linkage_test.py embedded_runtime_test.py parse_libinfo.py trace_parse.py
| `shots\live_flowcmd\view0.png`（1670x683） | 原版自带 **`C:\Program Files (x86)\e\lib\krnln\samples\流程类命令.e`** —— 6 个循环块 + 2 个嵌套如果真，**流程线的权威样本** | `probe_live.ps1 -File "…\流程类命令.e" -OutDir shots\live_flowcmd` |
     `InsertItem(TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE|TVIF_PARAM, text, img, img, 0,0, (LPARAM)i, parent, TVI_LAST)`，
| `shots\live_sel\after_a.png`（1707x743） | 同一个 `流程类命令.e` 的整窗（自动滚到底），用来核对第 4/5/6 个块的 ►► 落点 | `tools\probe_sel.ps1`（点进编辑器 + Ctrl+A，本轮新写） |
| `shots\live_drag\after.png` | 原版编辑器里**鼠标拖选**一段的样子 → 选区配色 | `tools\probe_drag.ps1`（本轮新写） |
| 没有参数的子程序也画了 `-/+` | 规则不对 | 只有**有参数或局部变量**的子程序数据行才画（graphic.e 的 `_图形按钮1_被单击` 没有，精易模块的 `外部超级列表框_保存数据_NEW` 有） |
| **验收 21（第 21 轮）** | `tools\page_smoke.ps1` → 三条 `[PASS]`：点程序树里的 全局变量 / 自定义数据类型 / 常量表... 节点，编辑器的显示模型里出现该页表头、MDI 子窗口标题变成页名（见 §2.8） |
| **验收 22（第 21 轮）** | 全局变量表与原版实拍**逐像素对拍**：`python tools\diff_png.py shots\live_page_gvar\view_全局变量.png shots\page_gvar.png 0 0 460 260 --ashift=4` → **差异 44 px / 119600 = 0.04%**（只剩原版那张图里的红色铅笔标记） |
| **验收 24（第 21 轮第二批）** | `tools\edit_cmd_smoke.ps1` → 七条 `[PASS]`：删除行(#32939) 行数 -1 且那一行没了 / 撤消(#57643) 回来 / 重复(#57644) 又没了 / 再撤消 又回来 / 屏蔽(#32936) 行首出现 `'` / 解除屏蔽(#32937) 又去掉（见 §2.9） |
| **验收 25（第 21 轮第十四~十六批）** | `tools\finddlg_smoke.ps1` → **十一条 `[PASS]`**：Ctrl+F(#32877) 弹出原版那个「寻找」对话框，标题=**寻找对话框**、**387x143 px**（= 原版模板 `DIALOG id=284` 的 254x76 对话框单位）、四个原版控件 id 全在（1151 ComboBox / 1041 区分大小写 / 1 **搜寻下一个(&F)** / 2 取消(&C)）、点关闭后进程还活着。⚠️ 第 16 批修过 3 处**我自己写错的中文**（原来标题写成"寻找"、静态标签写成"查找寻找文本："、按钮写成"查找下一个(&F)"；见 §3.8-11 —— 自洽的冒烟测试抓不出错误期望值） |
| 我起原版 e.exe + **精易模块.e** | 5189 节点，含**文件夹(16)**、**类模块(0)**、**全局变量(1)**、**自定义数据类型(0)**、**Dll命令(4)**；`调试` 只是一个普通程序集(img=3) | `shots\live_jy\prog_tree.txt`、`tree0.png`（用 `python tools\tree_section.py <trees.txt> 程序数据` 挑出来） |
| **验收 27（第 21 轮第二十二批）** | `tools\bm_dlg_smoke.ps1` → **十四条 `[PASS]`**：「书签跳转」对话框（Ctrl+F7/#32965，照原版 `DIALOG id=607`）：标题=书签跳转、**618x425 px**（= 408x264 对话框单位）、5 个原版控件 id 全在（1036 列表 / 3 删除(&R) / 4 全部删除(&L) / 1 跳转(&G) / 2 关闭(&C)），**功能断言**：加一个书签后 `LVM_GETITEMCOUNT=1`，「全部删除」后 `=0` |
| **验收 28（第 21 轮第二十四批）** | `tools\bp_smoke.ps1` → **七条 `[PASS]`**（调试第一步：断点状态）：启动无断点 → F9(#32855) 在插入点行设一个 → 再按取消 → 键盘 VK_F9 同样生效 → 可并存多个 → #32858 清除所有断点全清。判据是行倒档里新增的 **`bp=`** 字段（不用截图） |
| 块尾行**不可见**时 | 如果结束/如果真结束/判断结束不画，竖线延到**块体最后一行的行顶 + EC_CODE_H**，末端一个灰 ▼（逐行宽 1/7/5/3/1） | `EcRow::blkEnd == -1` 那条分支 |
| 构建管线 | `cmd /c build.bat` | `assemble_rc.py`(拼 rc) → `utf8togbk.ps1`(转码/CRLF/二进制拷贝) → `cl /MT /GX` → `link nafxcw.lib` → `bin\EClone.exe`（约 300KB） |
| ►► 落在哪一行 | ① **只有跨度 ≥ 3 个显示行的块才画**（实测：`如果真` 体里 1 条语句 → 没有；2 条 → 第一条上有；循环块 `首+体+尾` 天然 3 行 → 总有）；② 位置 =「块首行之后、**块尾行之前**第一条 `indent == 块首 indent`、且自身不是块首关键字的语句」（新缩进语义下块体直接语句和块首同级，见 2.1e） | 见第 6 节「存疑」——原版第 6 个块是个反例 |
| 块首行的 **►** | **只有循环类（计次/变量/判断/循环判断 循环首）有**：`x+12 .. x+15`，4 宽 7 高，灰实心右三角，逐行宽 1/2/3/4/3/2/1 | `DrawAll` 里 `pw[7]` |
| `shots\live_tabs\view0.png`（1174x388） | 原版打开 **graphic.e** 的代码页 **1:1 客户区**（权威！） | `powershell -File tools\probe_live.ps1 -File shots\e_samples\graphic.e -OutDir shots\live_tabs` |
| **验收 29（第 22 轮）** | `tools\syscfg_smoke.ps1` → **十一条 `[PASS]`**：「系统配置」= 原版 **7 个页签**（页名 通常/编 辑/显示/设计/编译/存根/安全，从原版截图 `shots\live_syscfg\tabs_zoom2.png` 读出）；（真鼠标点页签）第 0 页控件可见、另两页控件隐藏 → 切到第 1 页显示 `id=1180`、藏第 0 页 `id=1101` → 切到第 2 页显示 `id=1004`、藏第 1 页 → 切回第 0 页复原；进程活着。**已接进 `regress.bat` 第 25 步** |
| **验收 30（第 22 轮）** | `tools\insert_smoke.ps1` → **十四条 `[PASS]`**：「插入」菜单 12 条命令（#32777 子程序 / #32782 程序集 / #32831 窗口 / #32778 数据类型 / #32779 全局变量 / #32780 Dll命令 / #32828 常量 / #32829 图片 / #32830 声音 / #32786 参数 / #32787 局部变量 / #32929 类）每条**真的往源码加一行合法易语言声明**（判据 = 模型倒档末尾新增的 `SRC <下标> <原文>` 行，见 `DumpModel`）；#32783 现行单元是 no-op；进程活着。**已接进 `regress.bat` 第 26 步** |
| **验收 31（第 22 轮）** | `tools\edit_cmd_smoke.ps1` 扩到 **十二条 `[PASS]`**：在原来的七条之外加了「加到当前块首尾(#32801) 把选中的行包进块 → 显示行 +1（隐藏的 `如果真结束` 不建显示行）」和「转换为 -> (#32812) 把当前行变成块首并插入可见的 `计次循环尾` 行 → 显示行 +1」。**同一脚本仍是 `regress.bat` 第 19 步** |
| **验收 19（第 20 轮）** | `tools\resize_smoke.ps1` → 四条 `[PASS]`：拖 SE 手柄把组件放大 40x30、拖 NW 手柄把原点挪走并缩小 20x10、全程不崩 |
| **验收 26（第 21 轮第十八批）** | `tools\bookmark_smoke.ps1` → **五条 `[PASS]`**：Ctrl+F6(#32964) 在光标行加书签、F6(#32968) 停在它上面；VK_DOWN 移开后 **Shift+F6(#32967) 跳回**；再按 Ctrl+F6 取消书签后 **F6 不再移动插入点** |
| **验收 32（第 23 轮）** | `tools\ui_smoke.ps1` → **34 条 `[PASS]`**（**regress.bat 第 27 步**）：① 4 条工具条/面板开关命令真的改可见性并复原（#59392 标准条 / #129 工作夹）；② **新建窗体**：`插入→窗口`(#32831) 真建一个支持库 `WTWindow`（连按两次 → 3 个窗体），**任何时刻只有一个窗体可见**，新窗体的**窗口矩形 = 系统配置里的 380x250**；③ **切换窗体**：`Ctrl+Tab` 让当前窗体轮换（两次 Ctrl+Tab 都换到另一个窗体）；④ 6 条「窗口」表页命令把 MDI 文档标题换成 自定义数据类型表/全局变量表/Dll命令定义表/常量数据表/图片资源表/声音资源表；⑤ **整体搜寻**（#32934）弹出 `#32770`、**387x274 px**（= 原版 `RT_DIALOG id=30773` 的 254x163 对话框单位）、标题 `整体搜寻对话框`、9 个原版控件 id 全在、两个单选的原文 `“搜寻1”系统输出夹`/`“搜寻2”系统输出夹` 逐字相等，**且功能断言**：把 `abc` 填进 1151 的下拉编辑框、按「开始搜寻」→ 状态夹第 5 页（控件 id 1115）真的收到结果（`WM_GETTEXTLENGTH` = 17）且该页变成可见页 |
| **验收 33（第 23 轮）** | `tools\cfg_smoke.ps1` → **20 条 `[PASS]`**（**regress.bat 第 28 步**）：系统配置**另外 6 个页签**的设置真的读/写/持久化。判据是新加的倒档 **`ECLONE_DUMP_CFG=<path>`**（一行一项 `INT/BOOL/STR <名字> <值>`）：① 默认值 = 新建窗体 380x250 / 网点间距 8 / 显示网点开 / 撤消内存 0 / 启动方式 1101；② 打开对话框（#32876）后用 `WM_GETTEXT` 读到设计页的 1006=380、1008=8、通常页 1001=0、1236 空；③ 把 1008 改成 12 → 确认；④ **重启一次**倒档仍是 12（真的落到 `HKCU\Software\FlySky\E\EClone40`）；⑤ 按「置回默认值(&D)」(#1150) 后再重启回到 8 |
| 转换为 -> = `CECEditor::ConvertSelectionToBlock(nKind)` | 选到的行**自己**当块首，紧跟其后插一行对应的块尾（从后往前插，行号才不会漂） |
10. ⚠️ **倒档文件是 GBK**（`ECLONE_DUMP_MODEL` / `ECLONE_DUMP_THEME`），读要 `encoding="gbk"`。
| 鼠标滚轮 | `OnMouseWheel` 一格 3 行；`zDelta` 不是 120 的整数倍时至少走一行 | `sel_smoke.ps1`：`scroll 0 -> 66` |
| 配色来源 | `EC_CS_MARK`（"选中标记"槽），并把默认值实测成 `#000080` 写进 `g_ecMeasured` | `check_theme.py` 仍 80/0 |
| 面板改 `CSizingControlBarG` + 内层 `#32770`（自己 `CreateDialogIndirect` + `SubclassWindow` + 转发 WM_NOTIFY） | ✅ 采用。层级与原版一致，客户区由 sizecbar 算，DLU 问题消失 |
   现象对照：`shots\cmp_tree_orig_left.png`（原版，根无框）vs `shots\cmp_rootbox_mine.png`（本工程，根有框）。
工具        D:\eclone\tools\{assemble_rc.py,gen_menu_rc.py,utf8togbk.ps1,dump_bitmaps.py,diag_bitmap.py,shot.ps1}
截图        D:\eclone\shots\m1a.png(m1 菜单) m2.png(面板) m4.png(编辑器) m6.png(工具条) m7.png(全部填充)
VC6         C:\Program Files (x86)\Microsoft Visual Studio\VC98\Bin\{CL,LINK,NMAKE,LIB}.EXE
| 正文整体比原版深一格（25px） | `EC_TEXT_BASE` 写 69 | 改 **46**（原版那句 `信息框（…）` 的深红字从 x=46 起、嵌在如果真里的从 x=68 起） |
| 用 PIL 直接解 e.exe 的 DIB（自己写 `decode_dib`） | ✅ 对 4bpp/8bpp 都准（`dump_bitmaps.py` 的 PIL 路径反而不认这些头）。`tools\dump_res.py` 里留了这个函数 |
19. ⚠️ **link 报 `LNK1104: cannot open ...EClone.exe` = 还有 EClone 进程在跑**；先 Stop-Process 再 build，
| `shots\live_learn2\view0.png` | 原版自带 `…\krnln\samples\一步一步跟我学写易程序二.e` —— `如果` / `如果真` 块 + `否则` 不画 + ▼ 收尾 | 同上 |
| `shots\live_99\view0.png` | 原版自带 `…\samples\经典数学算法集\005.99乘法表.e` —— 嵌套 `变量循环首` | 同上 |
| 我起原版 e.exe + **图形按钮.e** | 11 节点：`程序数据 → 启动窗口程序集(3) → 子程序(2)`、`窗口(6) → _启动窗口(6)`、`常量表...(9)`、`资源表(12) → 声音(10)/图片或图片组(11)`、`模块引用表(14)`、`外部文件记录表(13)` | `shots\live_progtree\prog_tree.txt`、`tree0.png` |
| 用户给的树截图（精易模块） | 图标一一对应：黄文件夹=文件夹、菱形=程序集、连着的方块=类/自定义数据类型、圆柱=全局变量、带↓的文件=子程序/Dll命令、带蓝↓的文件夹=模块引用表 | `shots\ref_prog_tree.png` |
    VC6 没有 `I_IMAGENONE`（自己 `#define`，值 -2）；`SetHeight(n)` 实际得到 `n+2` 的行高（要 25 就写 23）。
24. ⚠️ RC 只认 CRLF；RC 不能嵌套 include（构建时拼成单个 .rc）；源码 GBK+无BOM+CRLF；`.bmp` 不能走文本转码；
| **原版代码编辑框是自绘 CWnd，不是 Edit/RichEdit** | IDA：`CreateCaret`/`SetCaretPos`/`ShowCaret`/`HideCaret`/`DestroyCaret` 五个小函数挤在 `0x4B4EA0–0x4B55C0`（约 1.8KB） |
25. ⚠️ **IDA 数据库没有 `.rsrc` 段** → 资源里的字符串/对话框查不到，要用 `tools\dump_dialogs.py` / `dump_res.py`；
| 左侧竖虚线 | **x = 正文列 + 31**（正文 46 → 77），3 亮 3 灭 `#808080` | `EcFlowLine` |
    int   HitTestRow(CPoint pt) const;                  // 返回 m_rows 下标，-1 = 无
    int   HitTestFold(CPoint pt) const;                 // 返回 m_rows 下标（点中折叠标记），-1 = 无
| 行数 | **= 实际写了的参数槽数**（`信息框（"…", 0, ）` 画 3 行；`(…, 0 )` 画 2 行） | `EcParseCall` 的 `bLastSep` |
| 块首行横虚线 | 循环类：`x .. x+15`；如果家族：`x+2 .. x+15`（实测差 2px，别问为什么，照抄） | `EcFlowLine` |
**节点顺序**（原版实测，照抄）：`文件夹/程序集/类…` → `窗口` → `全局变量` → `自定义数据类型` → `Dll命令` → `常量表...` → `资源表(声音/图片)` → `模块引用表` → `外部文件记录表`。
| 点「公开/易包/参考/可空/数组/静态/传址」格 | `HitTestCell`（列语义 = 同表同格数的最近表头行）→ `ToggleFlagCell` → `Rebuild()+Layout()` | `cell_smoke.ps1` → `[PASS] flag cell toggled` |
| 块体第一个 ►► | `x-14`、`x-10` 两个藏青实心右三角，各 3 宽 5 高（逐行宽 1/2/3/2/1），**竖直中心 = 行中线**（以前写 `行顶+6`，低 3px） | `DrawAll` |
| `tools\probe_orig.ps1` 直接探用户开着的 e.exe | ❌ 它会 `Stop-Process` 先杀 e.exe，而用户那个是**提权**进程（Access is denied）。→ 另写 `probe_pid.ps1`（只读 window tree）和 `probe_live.ps1`（起自己的一份） |
| `tools\probe_live.ps1`（起自己一份 e.exe + `PrintWindow` 每个编辑器视图） | ✅ 采用。`shots\live\view0.png`（图形按钮.e）、`shots\live2\view0.png`（ACCESS示例.e，含局部变量表）—— 无 EXUI 皮肤的经典外观基准 |
| `shots\live_fold\view_before/after.png` | 原版点 `+` 前后的 1:1（证明那个 `+` 是**参数归属线**的开关，不是收子程序） | `powershell -File tools\probe_fold_orig.ps1 -File shots\e_samples\graphic.e` |
| 1 | 「左侧文件夹、自定义数据类型、全局变量、类」 | `shots\live_jy\prog_tree.txt`（5189 节点）＋`shots\ref_prog_tree.png` | `EcBuildProgTree` 重写：文件夹(img=16)、类模块(img=0)、程序集=菱形(img=3)、全局变量/自定义数据类型有内容才出现 |
| 2 | 「程序编辑界面的表头在下侧」 | `shots\live2\tree.txt`：`CCustomTabCtrl` 412,685 1195x22，挂在 MDI 客户区**下侧** | `CECChildFrame::LayoutViews()` 标签条挪到底部；`CECTabStrip::OnPaint()` 按原版重画 |
| 3 | 「折叠后的加减背景色都不对」 | `shots\live_fold\view_before.png`：`-`/`+` 是藏青 **9x9**，画在**白底**上 | 删掉收起行的浅蓝底；横杠 9px（x=22..30），竖杠在 x=26 |
| 靠 `shots\zoom\` 里第 9 轮留下的中间产物（rowmap/gutter_scan/ref_codeline）推流程线几何 | ❌ 那些文件坐标口径不一（有的编辑器相对、有的截图绝对），越看越乱。→ 直接对 `shots\live2\view0.png` 重新扫，写 `tools\scan_runs.py` |
  `ShowPage(n)`（只显示第 n 页）。`OnInitDialog` 建 7 个页签、默认选第 0 页，**并且启动时也要
3. **iDraw 的三样**：自绘菜单条（30px 高的停靠条，`AfxControlBar42s 菜单条`）、输出面板 Scintilla、ExUI 皮肤 —— 用户明确说先不做。
| 4 | 「带箭头啥的都不对」 | 同上：子程序体第一条语句的流程槽里有 **↓**（竖线 x=9 高 7px + 7x4 实心三角，藏青） | `ECM_DOWN` 现在真的画出来（`DrawCodeRow`） |
6. **narrow 构建下 `_T("\x2193")`（↓）编译不过**（`C2022: too big for character`）→ 用 GBK 字节转义 `_T("\xa1\xfd")`。
7. **VC6 的 `for (int i = ...)` 变量作用域会外泄**，同一作用域两个 `for (int i...)` 报 `C2374: redefinition` → 第二个换变量名。
- **工具条已经和原版逐项对齐**（`shots\toolbar_table_orig.txt` vs `toolbar_table_mine.txt` 两张表对比即可）。
    `%VC98%\ATL\Include`、`LNK1104` = 还有进程在跑、`ECLONE_DUMP_*` 是 GBK。
16. ⚠️ 原版截图坐标 ≠ dump 坐标（原版最大化 `截图y = dumpy+8`；本工程 `截图 = dump-(134,57)`）。
  `CreateWindowEx(0,"EDIT",…,单元格矩形,this,id)`，`EN_KILLFOCUS` 写回 `m_lines[row.src]` 再
   `Edit` 一闪而过）。做法：双击 -> 在单元格位置 `CreateWindowEx("EDIT")` + 挂 `EN_KILLFOCUS` 写回模型。
实现 = `CECEditor::InsertDeclaration(nWhat)`：往源码插一行合法声明（名字用 `EcNewName` 在整篇里
诊断命令：`powershell -File tools\redump.ps1` → 看 `shots\model_dbg.fold.txt` 的 `param row=` 行。
13. **PowerShell 的 `@'...'@` / `@"..."@` 包多行 Python 极不可靠**（内容里的引号会破坏解析）→ 一律写 `.py` 文件再执行。
14. **DLL/进程位数**：本机没有 32 位 Python，无法 `ctypes` 加载 `krnln.fne`；64 位 PowerShell 枚举 32 位进程的模块也不可靠。
15. **`C:\Program Files (x86)\e` 对当前用户不可写**，所以所有产物都在 `D:\`。别试图往那个目录写。
   原版 `Afx:400000:b:10003:900015:0` 且**滚动条挂在 MDI 子框架上**（CScrollView 写法）。
    // ---------------------------------------------------------- 消息
| 点选定位 | **根因**：`CaretFromPoint()` / `CaretRect()` 用的是 `EC_FLOW_X(缩进级)`（= 24+22n，**流程槽**的列）当正文左边缘，比真值浅 22px → 点哪儿插入点都跑到行尾。改成 `TextXOfIndent()`（= 46+22n） | `sel_smoke.ps1`：点 x=150 后 `caret=6,17`（行中间） |
| 框选 | `OnLButtonDown` 记 anchor + `OnMouseMove` 拖（原有），**本轮补上选区绘制** | `sel_smoke.ps1`：`anchor=6,1 caret=6,31` |
| 选区外观 | **原版实测**（`shots\live_drag\after.png`，在原版编辑器里真拖了一段）：底色 `#000080`、**白字**、高度 = 文字那 **12px**（不是整行 22px） | `sel_smoke.ps1`：选区那一行里 `#000080` 最长连续 56px |
| `probe_fold_orig.ps1` 点 MDI 子框架 | ❌ 点不动（消息要发给真正的编辑器视图）→ 改成找 `AfxMDIFrame42s` 里的 `Afx:400000:b` 子窗口 |
| 把「文件夹」做成 `EcHeadWord` 判断 | ❌ static 函数跨编译单元不可见 → 改用 `line.Mid(1,6)` 比字节 |
   powershell -File tools\probe_orig_page.ps1 -File D:\eclone\shots\e_samples\jy.e -Node 全局变量 -OutDir shots\live_page_gvar
| `shots\live_fold_jy\view_before/after.png`（1670x705） | 原版打开 **精易模块.e** 的代码页 1:1 |
   探针现在能用了（行号靠 `tools\tree_row.py` 算：`python tools\tree_row.py shots\live_page_gvar\trees.txt 全局变量` → 14）。
   先拍 `全局变量` / `自定义数据类型` / `常量表...` 三张基准，再照 `ec_editor.cpp` 里 `ECT_ASM/ECT_SUB/ECT_VAR`
截图+窗口树    powershell -NoProfile -ExecutionPolicy Bypass -File D:\eclone\tools\shot.ps1 -Exe D:\eclone\bin\EClone.exe -Tag mNN
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
               附加参数：-SizeA 1200x700 -SizeB 900x500 / -Screen / -ExeArgs designer / -WaitSec N / -Keep
布局回归检查   python D:\eclone\tools\check_layout.py shots\mNN.windows.txt   → 期望 14 passed, 0 failed
**原版长什么样**：`shots\live_fold\view_before.png` / `view_after.png`（原版打开 graphic.e，
| `shots\live_flowcmd\` `live_learn2\` `live_99\` `live_sel\` `live_drag\` | **本轮新增**，见 2.1d 的表 |
                 tools\libtest.exe                                  # 载入+建树+断言节点数，退出码 0 = 过
                 tools\libtest.exe "C:\Program Files (x86)\e\lib" dump   # 打印全部 1560 条提示（给 cmp_hints.py 用）
`D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\krnln\window_notes.md`
`powershell -File tools\code_shot.ps1` → `python tools\diff_png.py shots\live_tabs\view0.png
shots\code_view.png 0 0 1174 388` → **跑完务必 `python tools\restore_sample.py`**。
布局回归       python D:\eclone\tools\check_layout.py shots\mNN.windows.txt   -> 14 passed, 0 failed
   （`wsprintfA(buf,"Afx:%x:%x:%x:%x:%x", hInst, style, hCursor, hBrush, hIcon)`），
                 libprobe.exe "C:\Program Files (x86)\e\lib\krnln.fne" [maxDt] [maxCmd] [maxConst]
  导对话框模板   python D:\eclone\tools\dump_dialogs.py "C:\Program Files (x86)\e\e.exe" shots\dialogs_all.txt
`…\krnln\out\cmdmap.tsv`（命令→实现函数）。IDB `…\ida_work\krnln.fne`（funcs=5559），工具 `tools\ida_krnln.py`。
  原版标记实验   powershell -File tools\probe_mark.ps1 [-File x.e] -> shots\live_mark\{before,after_*}.png
    要分开用 `[regex]::Match(...).Success` 各自取组，不要靠 `-match A -and -match B` 后再读 `$Matches`。
                 实测结果备份在 shots\toolbar_table_orig.txt / toolbar_table_mine.txt
   （`AFX_MODULE_STATE`/`AFX_THREAD_STATE`/`_AFX_WIN_STATE`/`_AFX_CTL3D_STATE`/`CStringArray`）。
               bundled Python（有 PIL 没 pefile）: C:\Users\Along\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe
                 对比：python tools\diff_png.py shots\_ref_code.png shots\code_view.png 0 50 1170 394
  IDA 资料       D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\
   `sub_100E500E(hwnd,msg,wp,lp){ if(msg==0x360) return 1; obj=sub_100E4F89(hwnd); return sub_100E4DA1(obj,hwnd,msg,wp,lp); }`。
原版样本       C:\Program Files (x86)\e\e.exe （5.9.0.0）；lib\krnln.fne v5.7.64；美化插件 lib\iDraw\ lib\ExuiKrnln\
反向资产       D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\（REPORT/IDA库/JSON/菜单dump）
  程序树探原版   powershell -File tools\probe_progtree.ps1 [-File x.e] [-OutDir d]
   跟本工程的 `EC_WM_HINT = WM_USER+111` / `EC_WM_OPENDOC = WM_USER+112` **仍然不撞号**。
   `live_flowcmd\view0.png` / `live_fold_jy\view_before.png`（精易模块、流程类命令）是 **18**。
| **688 条命令里 57 条根本没用库实现**（`m_pCmdsFunc[i] == NULL`）：流程控制（ife/if/switch/while/…/return/end）、四则与取负（mul/div/add/sub/neg）、逻辑（and/or/not）、赋值与数组（set/store/ReDim/CopyAry/…）、iif/choose/Macro/this… —— **它们由 e.exe 的编译器直接生成机器码** | `sub_10086410`（0x10086410，13677 B）逐个 `表项 = sub_10089980(实现函数, "krnln_fnXXX")`；`.data` 里 688 项全是 `0xFFFFFFFF`，运行期才填 | 支持库树/提示文本可以据此标"编译器内建"；`krnln\out\cmdmap.tsv` 有全部 688 条的 idx→实现函数 |
| **命令 ABI**：`cdecl(PMDATA_INF pRetData, INT nArgCount, PMDATA_INF pArgInf)`，`MDATA_INF` **12 字节**（0 字节起是 8 字节 union 值，+8 是 `m_dtDataType`），**参数步长 12**；返回值写 pRetData 的 union（double/文本指针），除 `_SDT_ALL` 外不用填类型 | 整除 `sub_100684C0`（`v3 += 12`）、信息框 `sub_10070610`（`pArg[2]`=arg0 类型、`pArg[6]`=arg2 文本）、取文本 `sub_10078E10` | 将来要"执行支持库命令"就照这个；也能用来解释参数默认值/类型提示 |
| **LIB_INFO 全字段实测值**（+84 nDataType=51、+88 pDataType=0x1012F138、+92 nCategory=23、+96 m_szzCategory、+100 nCmd=688、+104 pBeginCmdInfo=0x101327D8、+108 **pCmdsFunc=0x1017D6B8**、+120 pfnNotify=sub_10064E30、+132 nConst=335） | `python tools\ida_krnln.py dw 0x101388a0 56` | `ec_lib.cpp` 的 `RAW_*` 镜像**逐字段核对通过，不用改** |
| **24 个组件窗口类的注册参数**（类名/style/cursor/brush）—— 除 `_EL_ColourPopup`（0x843 + `COLOR_BTNFACE+1` 背景刷）以外，**背景刷全是 `GetStockObject(5)`=NULL_BRUSH（自绘）**，光标全是 `IDC_ARROW`；`WTWindow` 是 0x0B/NULL_BRUSH/无图标 | 唯一入口 `sub_100170A0(类名, style, 光标, 刷, 图标)`，30 个调用点见 `krnln_anatomy.md §4.1` | 设计器要自绘组件、或做"组件默认样式"时照抄 |
| 组件创建链：`sub_100E49FF(对象尺寸)` → 各自 ctor → `sub_100E54A2(obj, exStyle, 类名, 文本, style, x,y,cx,cy, 父, 菜单, 参数)`；子组件 style 都带 `0x44000000`(WS_CHILD\|WS_CLIPSIBLINGS) | 同上 §4.2 | 设计器"新建组件"这步照着走 |
| **`GetNewSock` 与套接字无关**：1000/1001/1003 → `sub_1002ED08` → `sub_10066EE0`（3890 B）= **内存 PE 映像加载器**（VirtualAlloc + Reloc/Import/Export 修复 + 设当前目录 + OleInitialize），返回 `基址+AddressOfEntryPoint` 再调它；1002 → `sub_10064C80` | `krnln_anatomy.md §1.3` | 别再拿它解释网络组件（网络是 WS2_32，但入口不是它）；也说明 krnln 自己就是编译产物的加载器 |
| `m_pfnNotify`(sub_10064E30) 实测码表：1=存宿主通知指针到 `dword_1017D65C`；14=给出命令实现函数的**名字数组**（静态编译用）；15=返回本函数名字串；**6(NL_FREE_LIB_DATA) 不处理，返回 -1** | 同 §6 | 解释静态编译/`krnln_static.lib` 那条线时用 |
| `LIB_BITMAP`（具名 RT_BITMAP，336x13/4bpp/16 色）就是支持库自带的图标条；但**格宽与格序没定死**（16px→21 格、14px→24 格，两种切法都不干净） | `tools\extract_lib_bitmap.py` + `shots\krnln_icons\` | ⚠️ **别急着拿它替换 `src\res\tree_icons.bmp`**（截图条带里 6..18 号是 IDE 自己的通用图标，不在 LIB_BITMAP 里）；下次先定标 |
    **正解**：`build.bat` 每次自动跑 `python tools\check_block_comments.py gbk`，不通过直接 `BUILD FAILED`。
| **组件对象就是 MFC `CWnd` 的派生类**：`m_hWnd` 在 `obj+28`（= MFC 6.0 的 `CWnd::m_hWnd` 偏移）；所有组件的构造函数第一句都是 `sub_100E4B41`：`*this = &CWnd::\`vftable'; memset(this+7,0,0x20)` | `sub_100E4B41`(0x100e4b41) |
| 组件的消息处理是 **MFC 的"消息映射"机制**：`sub_100E5DED`（1198 B）里用 `_afxMsgCache`（`dword_1017E438`，三项 `{msg,sig,entry}`）+ `sub_100E5D69`(=AfxFindMessageEntry) 查表，再按 `entry+16` 的 **AfxSig 签名码**（case 1..49）翻译 wParam/lParam 调处理函数 | `sub_100E5DED` 伪 C |
| 组件 = 一个 C++ 对象，内含**多个带虚表的子对象**（`obj[15]/[33]/[49]/[52]…`）+ **内嵌 MFC GDI 类**（画板里直接是 `&CPen::vftable`/`&CBrush::vftable`/`&CBitmap::vftable`） | `sub_100364D0`/`sub_10034F30`/`sub_1004E980` |
| 虚表是**共用大段公共实现 + 少数每类专有**：+0x54 = `sub_100E5588`（用 RECT 建子窗口）、+0x5C = `sub_100E5568`（类名空则填 `"AfxWnd42s"`）、+0x70 = `sub_100E7607`（取样式）、+0x90 = `sub_100E582A`（属性通知槽）、+0x98 = `sub_100E5DA9`（分派包装）、+0x9C = `sub_100E5DED`（真消息处理）、+0xA4 = 收尾 | 四类 vtable dump（`krnln\out\s12_vtbl.txt`） |
| **唯一的 `CreateWindowExA` 出口是 `sub_100E54A2`**：`(*(vtbl+92))(this,&lpParam)` → `AfxHookWindowCreate` → `CreateWindowExA` → `AfxUnhookWindowCreate` → 失败/收尾 `(*(vtbl+164))(this)` | `sub_100E54A2`(0x100e54a2, 198 B) |
| ★ **组件的 CreateUnit 就是 lib2.h 的 `PFN_CREATE_UNIT`**：14 个参数，最后两个是 `hDesignWnd`(a13) / `blInDesignMode`(a14)；选择夹里明确 `if (a14 == 1) obj[32] = a13;`（设计模式才记设计窗体） | `sub_100359A0`/`sub_10034300` vs `lib2.h:549` |
| ★ **设计器接口表 `ITF_*`（1..12）在二进制里全对上了**：`LIB_DATA_TYPE_INFO.m_pfnGetInterface`(DT+44) → 每类一个分发器。实测：标签 `sub_10036470`：`1→sub_100359A0`(建)、`2→sub_10035AE0`(属性可否改)、`4→sub_10035BD0`(属性改了)、`5→sub_10045250`(取全部属性=保存)、`6→sub_10036040`(取某属性)、`9→sub_10036290`(语言)；选择夹多一个 `3→sub_10034BE0`(定制属性对话框)；图形按钮/画板同理 | `krnln\out\s13_itf.txt`（四个类的完整表在笔记 §3.1） |
| ★ **属性索引 0..7 = lib2.h 的 `FIXED_WIN_UNIT_PROPERTY` 八个系统固定属性**，顺序一致：左边/顶边/宽度/高度/标记/可视/禁止/鼠标指针；`sub_10036040` 把它们映射到 `obj[16..22]`、鼠标指针在 `obj[25]/[27]` | `sub_10036040` 伪 C |
| e.exe 侧有一份**同源的窗口辅助层**（`sub_502070` ≡ krnln 的 `sub_100170A0`；`sub_5BC471` ≡ `sub_100E54A2`），并且**自己也注册 `_EL_ColourPopup`**（style=2115/brush=`COLOR_BTNFACE+1`，与 krnln 完全一致）；取 krnln 导出用**名字表**（`0x6630E8`：`"CreateWin"`, `"PYToBool"`, `"ComparePYQuickStr*"`）；交给库的通知函数是转发器 `sub_467610` | `…\krnln\out\s14_ide.txt` / `s17_ide3.txt` |
| ★ **"单击把组件放到窗体上"的默认尺寸**走 `ITF_GET_NOTIFY_RECEIVER(12)` → `PFN_ON_NOTIFY_UNIT(0, &cx, &cy)`（`NU_GET_CREATE_SIZE_IN_DESIGNER`） | `lib2.h:535-541` |
27. ⚠️ **改了采样源码之后必须 `python tools\save_sample_backup.py`**，否则 `restore_sample` 会把新东西冲掉。
   `AfxWnd42s`/`AfxOldWndProc423` 里的 "42/423" 是 MFC 自 4.2 起**固定不变的历史标识**，
④ 别再自己注册 `_EL_*` 同名类（现在注册的 `WTWindow`/`_EL_RgnButton` 两个自定义类建议撤掉，避免和 krnln 注册的类重名）。
| 属性改动需要重建窗口 | `sub_4AE230`(0x4ae230) | **ITF 5** 取旧组件全部属性数据 → **ITF 1** 重建 → `SetParent` 迁移子窗口 |
| `tools\tbprobe.c` + `build_tbprobe.bat` | 读**运行中**进程的 `TBBUTTON` 表（必须编 32 位）。`tbprobe.exe e.exe` / `tbprobe.exe EClone.exe` |
| `tools\ida_lookup.py` | IDA DB 查询（系统 Python 3.12 + idalib）。⚠️ 库里**没有 .rsrc 段**，资源字符串查不到；GBK 中文要按原始字节找 |
  短流程采样     python tools\patch_sample_flow.py（代码区在第一屏）→ 跑完 restore_sample.py
| 按 DATA_TYPE 取类型定义 | `sub_461810`(0x461810) | 返回 `LIB_DATA_TYPE_INFO*`（`LIB_INFO+84/+88`，步长 56） |
  查原版函数     python tools\ida_font.py "0x4f5660" shots\ida_paint.txt  （表格/代码行绘制函数）
  原版代码页     powershell -File tools\probe_live.ps1 -File shots\e_samples\graphic.e -OutDir shots\live_tabs
| 工作夹模板 | e.exe `RT_DIALOG` **DIALOGEX 128**（149x133 DLU，Tab 在底 + 两个 SysTreeView32(id 1022/1019) + 两个 ComboBox）|
| 支持库树 | SysTreeView32 210x382，style `0x50000023` = HASBUTTONS\|HASLINES\|SHOWSELALWAYS（**没有 LINESATROOT**）|
  原版某文档页   powershell -File tools\probe_orig_page.ps1 -File shots\e_samples\jy.e -Node 全局变量
                 （行号由 python tools\tree_row.py <trees.txt> <节点名> 算出：只展开根时的可见行）
  采样换/还      python tools\patch_sample_graphic.py / tools\patch_sample_ref.py → 跑完 python tools\restore_sample.py
                                                   自起一份 e.exe，导出 tree.txt + frame.png + view0.png
  样本           D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\ide_probe\sample\{图形按钮.e, ACCESS示例.e}
                   ida_work\e_copy.exe（.id0/.id1/.id2，idalib 直接开）；editor_impl.json；IDE-REPLICATION.md
        0, 0,                 // pAllData=NULL, nAllDataSize=0   ★★ 新建组件不需要"组件数据"blob
1. **新建组件传 `NULL/0` 是合法的** —— krnln 的校验函数 `sub_100311F0` 里 `if (a2 == 0 || a3 <= 0) return 1;`。
   ⇒ **EClone 现在就可以直接 `pfnGetInterface(1)` 建真组件窗口**，不需要先造数据 blob（笔记 §3.4 的"待实测"项已解决）。
2. **`DATA_TYPE = MAKELONG(库内类型索引+1, 库索引+1)`**（`sub_461810` 反推 + `sub_50D520` 正推都对）。
| **验收 17（本轮）** | `tools\design_smoke.ps1` → 四条 `[PASS]`：① 支持库建出 `WTWindow` 设计窗体 + `_EL_RgnButton` 子组件；② 第三方库（`ECLONE_EXTRA_LIBS=iext2`）的组件也放到窗体上（`Button 120x80 vis=True`）；③ 组件箱列出支持库组件（tree items=32）；④ 属性页从支持库填充（35 项，值走 ITF 6） |
3. **`CreateWin` 在 IDE 里是死代码**：e.exe 把 krnln 导出解析进 `esi+0x8B0..0x918` 的表（0x47d636 处存 `CreateWin`→`[esi+918h]`），
| 右键菜单 | `OnContextMenu` 运行时建 `CMenu`（剪切/复制/粘贴/全选），不进资源 | 手工验证（无自动断言） |
   `WM_VSCROLL(SB_PAGEDOWN)` → `scroll=460`、`WM_MOUSEWHEEL(-120)` → `scroll=66`。
  Dll 页断言     python tools\check_dll.py shots\dll_view.png shots\dll_view2.png shots\ref_dll_page.png
| krnln 里 `sub_100E5414`/`sub_100E5460` **就是 MFC 的 `AfxHookWindowCreate`/`AfxUnhookWindowCreate`**（装/卸 `WH_CBT` 钩子）——因为 e.exe 里有一对**同源同参**的函数，那两处直接调真 MFC 的 `Afx*` | `sub_100E5414` / `sub_100E5460` vs `sub_5BC471` |
| 暴露"按类型取接口 + 建/毁组件" | `src\ec_lib.h/.cpp` | `EcDataType` 加 `void* pfnGetInterface`（= `LIB_DATA_TYPE_INFO+44`）；新增 `EcDtIsUnit/EcDtIsVisualUnit/EcDtInterface/EcDtDefaultSize/EcCreateUnit/EcUnitHwnd/EcDestroyUnit/EcSetUnitTitle` |
| 设计器**整段重写** | `src\ec_design.h/.cpp` | 删掉自己注册的 `WTWindow`/`_EL_RgnButton` 假类和 `CECForm`/`CECUnitButton`；改成：窗体 = 支持库"窗口"类型、组件 = 支持库可视组件类型，全部走 `PFN_CREATE_UNIT`；加一层**透明覆盖窗口**接管设计期鼠标（放置/选中/拖动/Delete/框选） |
| 组件箱接支持库 | `src\EClone.cpp` | `CPaneBar::FillComponentBox(bExtend)`：**基本组件 = krnln / 扩展组件 = 其它已载入库**（与原版两个单选同名，见 `ide_probe\ide_frame_children.txt:61-62`）；点表项 → `EC_WM_ARMUNIT`(WM_USER+113) → 设计器进入放置模式 |
| 新探针 + 冒烟 | `tools\designprobe.c` / `build_designprobe.bat` / `design_smoke.ps1` | 见下 |
          lib+1, uID,                            // dwWinFormID, dwUnitID
| 参数名来源 | 支持库（`EcCmdArgsByName`），只有一条权威路径 | `tools\argprobe.exe "<e>\lib" 信息框` |
设标题：   ((PFN_NOTIFY_PROPERTY_CHANGED)((PFN_GET_INTERFACE)dt[+44])(4))(hunit, 属性下标, &值, NULL); // ITF 4
- 想继续抠代码页那 0.62%：先 `patch_sample_graphic.py` → build → `code_shot.ps1` →
  `diff_png.py shots\live_tabs\view0.png shots\code_view.png 0 0 1174 388`，
    `-match A -and -match B` 之后 `$Matches` 只剩最后一次的结果，要用 `[regex]::Match()`。
- `tools\designprobe.exe "C:\Program Files (x86)\e\lib\krnln.fne" 6` → 退出码 **0**：krnln 的 6 个可视组件全部建成真窗口
ShowWindow(hw, SW_SHOW);                         // ⚠️ 支持库建的窗口**不带 WS_VISIBLE**
| 双击文字格 | `BeginEditCell` 建 `CEdit`，回车/失焦提交，ESC 取消 | `[PASS] in-place text edit committed` |
- `tools\design_smoke.ps1` → **四条 `[PASS]`**（截图 `shots\design_smoke.png`：真 `WTWindow` 带 Windows 原生标题栏，
  里面一个是 krnln 的 `_EL_RgnButton`、一个是 iext2 的 `Button`「超级按钮2」且带选中手柄）：
21. ⚠️ **原版正文列 = 46 + 22n**（不是 69+22n）；流程列 = 上一级正文列 + 1。
  （`Edit`/`_EL_PicBox`/`_EL_ShapeBox`/`_EL_DrawPanel`/`Button`/`_EL_Label`），`Window`(对象+28) 都有效；
28. ⚠️ **`LNK1104: cannot open ...EClone.exe` = 还有 EClone 进程在跑**；先 `Stop-Process` 再 build。
  ④ **属性页从支持库填充**（`ECLONE_DESIGN_DUMP` 倒出 `prop|下标|名|值`，实测 35 项、含数值。
29. ⚠️ **`ON_NOTIFY(code, id, fn)` 的 `id` 是控件 ID 范围**，写 0 永远不匹配。
30. ⚠️ **倒档文件（`ECLONE_DUMP_MODEL` / `ECLONE_DUMP_THEME`）是 GBK**，读的时候 `encoding='gbk'` / `GetEncoding(936)`。
- 选中变化：设计器 `NotifySelection()` → **`AfxGetMainWnd()->PostMessage(EC_WM_SELUNIT, MAKELONG(库,类型), 组件对象)`**
  → `CPaneBar::FillProps(lib, dt, hunit)`：按 `EcPropAt()` 的元数据列名字（`EC_UW_IS_HIDED` 的不列），
- 想调流程线：`patch_sample_flow.py` → build → `code_shot.ps1 -Tag flowtest` →
但它的 ITF 4 `sub_10056150`：`case 0` 是**一个 0..1 的二值属性（还会钳位）**、`case 2..5` 收的是**二进制数据**。
- 想调参数归属线：`patch_sample_graphic.py` → build → `powershell -File tools\param_smoke.ps1`
| **编辑器视图（原版，两层）** | 外层 `Afx:400000:8:10003:900015:0`（414,278 1174x388）+ 内层 `Afx:400000:b:10003:900015:0`（同矩形）；**零子控件**；滚动条（ScrollBar）挂在 `AfxMDIFrame42s` 上 |
  两态截图落在 `shots\param_before/after/back.png`，跟 `shots\live_fold\view_before/after.png` 逐像素比：
| **系统配置对话框** | `RT_DIALOG id=282`，267x217 对话框单位，78 个控件，`SysTabControl32 id=1149`，6 页签 |
  python tools\diff_png.py shots\live_fold\view_after.png shots\param_after.png 60 113 300 165 shots\cmp_paramblock.png
| 在原版里真拖选一段量选区配色 | ✅ 采用（`probe_drag.ps1`）：`#000080` 底 + 白字，高 12px |
| 在原版里 `Ctrl+A` 量选中态（`probe_sel.ps1`） | ❌ 弹了个模态警告框，按键被吃掉（剪贴板拿到的是对话框文本）→ 改用拖选 |
而"标签"是一致的（`sub_10035BD0` 的 case 0 写 `obj+64` = `sub_10036040` case 0 读的 `obj[16]` ✓）。
完整笔记 `…\krnln\ide_lib_host_notes.md`，原始反编译 `…\krnln\e_notify.txt`（若已归档）。
| 点选定位用 `EC_FLOW_X(缩进)` 当正文 x | ❌ 偏 22px，插入点永远在行尾、框选选不出东西 → `TextXOfIndent()` |
**不保证窗口跟着动**（要更忠实就得照原版 `sub_4AE230` 那套：ITF 5 取数据 → ITF 1 重建）。
| 可选的额外支持库 | `src\EClone.cpp` | 环境变量 `ECLONE_EXTRA_LIBS=iext2;EChartBar`（默认不载入，见坑 3.5-2） |
| `shots\ref_dll_page.png` | 用户给的 Dll 页 crop（**比真客户区低 4px → 对比要 `--ashift=4`**） |
   `LBS_NO_EDIT_INFO` 不许有、要求的系统版本与**核心库版本**（`+28/+32`）都要满足。
3. **★ 核心一句：`m_pfnNotify(1, sub_467610, 0)`**（`NL_SYS_NOTIFY_FUNCTION`）——
f  = ((PFN_GET_INTERFACE)dt[+44])(1);           // ITF 1 = PFN_CREATE_UNIT（stdcall）
hunit = f(NULL, 0,                               // ★ 新建组件不需要"组件数据"blob（NULL/0 合法）
4. 失败/卸载：`m_pfnNotify(6, handle, 0)` = `NL_FREE_LIB_DATA` + `FreeLibrary`；
   成功则把 `LIB_INFO*`、`handle`、`HMODULE` 存进宿主三张表，**此后不再 FreeLibrary**。
   ⚠️ 第 18 轮补测：krnln 的 `m_pfnNotify`（`sub_10064E30`）**没有 case 6**，落到 default 直接返回 `-1` ——
| 窗体客户区那块"纸" | **#F0F0F0**（= `COLOR_BTNFACE`，就是窗体"底色"属性的默认值） | `EC_DESIGN_PAPER` |
| 点阵 | 1px **纯黑**，间距 **8px**，锚在**窗体客户区原点**：client (8i, 8j) 处都有点，**(0,0) 也有** | `SurfacePaint` 里逐点 `FillSolidRect` |
默认尺寸： ((PFN_ON_NOTIFY_UNIT)((PFN_GET_INTERFACE)dt[+44])(12))(0, &cx, &cy);   // ITF 12
   （每行左边缘绿色实竖线 x=63、首末行各一条绿色 3 亮 3 灭横虚线拉到正文右边）。
工程           D:\eclone\                  构建 D:\eclone\build.bat -> bin\EClone.exe
- `GetNewInf`（0x10064d30，6 字节，`return &unk_101388A0;`）和 `GetNewSock`（0x10064d40，121 字节）
   再装 `sub_100E5043`/`sub_100E51A2` 并 `CallWindowProcA(prev,…)`。
  通用           python tools\diff_png.py <A> <B> <x0> <y0> <x1> <y1> [out.png] [--ashift=N] [--bshift=N]
- **`Sock` 就是网络套接字**：krnln 里有字符串 `_EL_UDPSock` / `_EL_ClientSock` / `_EL_ServerSock`
   （`sub_100E500E` → `sub_100E4DA1` → 对象虚函数 `vtbl+152`；全库有 7 处调 `sub_100E4DA1`）。
  代码页 1:1     powershell -File tools\code_shot.ps1 [-Tag flowtest] [-ScrollPages N]
                 → shots\<Tag>.png + <Tag>_tabbar.png（⚠️ 滚动消息对本编辑器无效，见第 3.1-9 条）
8. 编辑器窗口类名仍不同（本工程 `Afx:400000:8:10005:6:0`，原版两层 `…8:10003:900015:0` + `…b:…`）。
  .e 里抠文本    python tools\e_strings.py <file.e> [最少字符]   （看源码里到底有哪些字符串/注释）
  ITF2 → `sub_100CAC00`、ITF4 → `sub_100CAD20`、ITF5 → `sub_100CB7C0`、ITF6 → `sub_100CB460`、
  参数线冒烟     powershell -File tools\param_smoke.ps1  （从模型找 par>0 的行 → 滚到可见 → 点流程槽 -/+
  程序树 1:1     powershell -File tools\probe_progtree.ps1 -Exe D:\eclone\bin\EClone.exe -OutDir shots\live_progtree_mine
  **`DIALOG id=321 322x290`**（11 个控件，控件 id = 1115/1011/1036/1001/65535/6/3/4/5/1/2）。
1. ⚠️⚠️ **ITF 4（`NOTIFY_PROPERTY_CHANGED`）返回 0 不是失败**：它的返回值是"需不需要重建窗口"。
  单元格冒烟     powershell -File tools\cell_smoke.ps1   （块尾行不画 + 点公开格 + 双击名字格改字，三条断言）
2. ⚠️ **属性值是一个 8 字节联合**（`UNIT_PROPERTY_VALUE`，lib2.h:583）：传给库的缓冲**必须够 8 字节**。
| 原版实拍：设计期就是**真窗口**（`WTWindow 380x250` 里挂 `_EL_RgnButton 104x112`） | `…\ide_probe\ide_opened_windows.txt` |
- 设计器里**拖 8 个手柄缩放选中的组件**（`CECDesigner::HitHandle` + `SurfaceDown/Move/Up` 的 `m_dragMode==3`）。
`ITF_GET_NOTIFY_RECEIVER(12)→PFN_ON_NOTIFY_UNIT(0,…)`；③ **可以直接调 `pfnGetInterface(1)` 拿
  树图标对照     python tools\cmp_tree_icons.py <原版tree.png> <本工程tree.png> out.png
  切文档页       PostMessage(mdiChild, WM_USER+112, docIndex, 0) ← EC_WM_OPENDOC（0=设计器 1=程序集 2=Dll）
   直接把进程打成 `0xC0000005`（探针 exit=-1073741819）。所以 `EcLoadLibs` 只载入**显式列出**的库，
  原版样本       C:\Program Files (x86)\e\e.exe（5.9.0.0）；shots\e_samples\{graphic.e, jy.e, dll_sample.e}
  SDK 权威       C:\Program Files (x86)\e\sdk\cpp\elib\lib2.h（GBK，1392 行）
  VC6 / RC       C:\Program Files (x86)\Microsoft Visual Studio\VC98\Bin\ ；…\Common\MSDev98\Bin\RC.EXE
  文本形式语法   D:\逆向\cases\e_epl\refs\OpenEpl\TextECode\TextECode\Grammar\EplParser.g4
                 （`.全局变量` = Global、`.数据类型` = Struct、`.类` = Class、`.成员` = member；
   跑完不恢复的话，后面 `check_dll.py` 的颜色断言会假失败（实测变成 44/4 和 29/2，命令名变青 `(0,255,255)`、
                 没有 × 关闭按钮；tab1 `_启动窗口` 上边 [2,83]、tab2 `启动窗口程序集` [74,187]
   **正解**：`reg add "HKCU\Software\FlySky\E\EClone40" /v ColorScheme /t REG_SZ /d "默认配色" /f`，
   或把 `theme_e2e.ps1` 放在回归**最后**跑。（HANDOFF 原来的回归顺序里只有 dump 版的 `check_theme.py`，不改状态。）
- `check_dll.py` 的第一条断言（`x=44 有全高竖线`）是**滚动位置相关**的，滚动一变就会假失败。
   **实测 krnln.fne 的 307 个导出里只有 `GetNewInf` 和 `GetNewSock`，没有 `InitCtl`
   VC6 里用 `__fastcall` 模拟 thiscall：`((void(__fastcall*)(void*,int,int))vtbl[1])(obj,0,1)`。
2. **八项校验**：`m_dwLibFormatVer==20000101`、`m_pfnNotify(+120)!=NULL`、`m_nMajorVersion(+8)>0`、
  也就是 `LIB_INFO.pDataType(0x1012F138) + 0x2C`）：ITF1 → `sub_100CA6B0`（建窗）、
构建           cmd /c "D:\eclone\build.bat"          （本机没有 pwsh！）
  `if (sub_100C5E80(v15, 0x50000000, 59396) != 0) { … sub_100F5B87(v15[368], 59419, 0); … }`
   我一开始写成 `line.split(None, 2)` → `pat B8 D6 17 10` 被切成 `parts[1] = "B8"`，
   语言 1004 / 版本 1005 / 目录 1006 / CWnd 桥接 1007-1011 / 是否易窗口 1014。
5. IDE → 库的按需查询走 `NAS_*`（1000 段）：图标 1000 / 数据类型 1002 / HBITMAP 1003 /
7. `m_pfnNotify` 在 `LIB_INFO` 里的字节偏移 = **120**（索引 30）；`CMD_INFO` 步长 = **36** 字节
  模型倒档       set ECLONE_DUMP_MODEL=…\model.txt（GBK；row 行带 mark=，2=折叠标记 / 1=↓；
   必须按原始字节搜：`python tools\ida_krnln.py gbk 信息框`。ASCII 串（`_EL_Label`/`WTWindow`）正常。
4. ⚠️ **别用 MFC 类名/属性名里的 "42"/"423" 判断 MFC 版本**（`AfxWnd42s`、`AfxOldWndProc423` 在 MFC 4.x~6.0 里一模一样）。
5. ⚠️ **`LIB_BITMAP`（krnln 的图标资源）与 EClone 的 `src\res\tree_icons.bmp` 不是一一对应**：
  注册表         HKCU\Software\FlySky\E\EClone40（本工程；**别动** …\EInf40 = 原版）
  强制重倒       powershell -File tools\redump.ps1        → shots\model_dbg.txt + model_dbg.fold.txt
                 （fold.txt 里有 scroll / anchor,caret / lines / fold row / param row）
                           C:\Program Files (x86)\e\lib\krnln\samples\一步一步跟我学写易程序二.e
  原版选中态     powershell -File tools\probe_sel.ps1  -File "<.e>"     （点进编辑器 + Ctrl+A）
  原版拖选       powershell -File tools\probe_drag.ps1 -File "<.e>"     （鼠标拖选 → 选区配色）
   （`C:\Program Files (x86)\e\lib\krnln\samples\*.e`、`C:\Program Files (x86)\e\samples\经典数学算法集\*.e`，
                  （行号由 python tools\tree_row.py <trees.txt> <节点名> 算出）
  采样换/还      python tools\patch_sample_graphic.py / patch_sample_flow.py → 跑完 python tools\restore_sample.py
   `python -c "print(sum(1 for x in open(r'X.ps1','rb').read() if x>127))"` 必须是 **0**。
3. ⚠️ **PowerShell 里 `LP` 是 `Out-Printer` 的别名** —— 我拿 `LP` 当"打包 lParam"的函数名，
  支持库参数名   cmd /c tools\build_argprobe.bat → tools\argprobe.exe "C:\Program Files (x86)\e\lib" 信息框
                   ida_work\krnln.fne（**已分析并存盘**，funcs=5558）→ krnln\window_notes.md（窗口调度笔记）
                   krnln\decomp_window_cluster.txt / decomp_event_helpers.txt（原始反编译）
   krnln 逆向     python tools\ida_krnln.py survey|imports|calls|str|xref|dec|funcs
                  python tools\ida_krnln.py report <out.txt> "0x地址,0x地址,..."
   **正解**（本轮采用）：修饰键判断写 `(GetKeyState(k) | GetAsyncKeyState(k)) & 0x8000`；
                  ② 别「跑一次 survey 存盘、再开一次 dec」：idalib 反复开关 krnln.fne 会随机 open rc=4
6. ⚠️ **`InsertNewline()` 必须同步更新选区 anchor** —— 否则 `HasSel()` 还是 TRUE，
   紧接着那次 `InsertText()` 会把刚断开的行又合回去。现象：粘贴多行文本时**换行全丢、挤成一行**
   （`lines` 不变、caret 停在同一行）。这个坑很隐蔽：单独测回车没问题，只有"粘贴"这种
   "插入+断行+再插入"的序列才暴露。
7. ⚠️ **`EcRow` 是 POD：加字段一定要在两个创建点都初始化**（`AddTableRow` / `AddCodeRow`）。
  sel_drag.png           选区实拍（#000080 底 + 白字），与原版 live_drag\after.png 一致
8. ⚠️ **原版代码行距不是恒定的 22**：`live_tabs\view0.png`（graphic.e）是 21，
1. ⚠️⚠️ **`WM_PRINTCLIENT`/`WM_PRINT` 证明不了"这个窗口不画东西"**。给窗口一个**品红填充**的
0x00540FFE Afx:400000:b:10003:6:51b00427 1707x743 |全局变量表|      <- 新开的文档
0x002A0EA2 Afx:400000:b:10003:6:51b00427 1453x488 |程序集: 调试|     <- 原来那个还在
   ① 先在**屏幕上**（`GetDC(NULL)` + `FillRect`）用品红盖住目标窗口客户区；
                 块首行的 ►：只有循环类，x+12..x+15，4 宽 7 高，逐行 1/2/3/4/3/2/1
   原版是「块首 = 外层 +1 级、**块体和块首同级**」，本工程写成「块首 = 外层级别、块体 = 块首 +1」。
                 块尾不可见（如果结束）：竖线延到「块体最后一行行顶 + EC_CODE_H」，▼ 逐行 1/7/5/3/1
                 ►►：x-14 / x-10，各 3 宽 5 高（逐行 1/2/3/2/1），藏青，竖直中心 = 行中线
   **排除掉子窗口占的矩形**（跟有没有 `WS_CLIPCHILDREN` 无关）。实测：设计器视图
9c. ⚠️ **`►►` 的搜索范围一定要卡在 `[块首+1, 块尾)`**：新缩进语义下**块尾行和块首行同级**，
   不排除块尾就会把 `►►` 画到 `计次循环尾 ()` 那一行上（真踩过，见 `shots\flowtest5.png`）。
                           C:\Program Files (x86)\e\samples\经典数学算法集\*.e（85 个）
  原版折叠前后   powershell -File tools\probe_fold_orig.ps1 -File shots\e_samples\graphic.e
  配色目录       C:\Program Files (x86)\e\clr\*.clr（13 个，只读）
                     krnln\ida_base\{survey,allnames,big,exports,imports,strings}.txt  基线索引
  配色回归       set ECLONE_DUMP_THEME=… 启动再关 → python tools\check_theme.py …
   krnln 逆向     python tools\ida_krnln.py survey|base <dir>|big [N]|allnames <f>|dw <a> [n]|gbk <文字>|
                        imports [子串]|calls <子串>|decimp <子串> [out]|decstr <子串> [out]|
                        dec 0x..,0x.. [out]|report <out> "0x.."|funcs <子串>|calloff <hexoff>|imm <v,..>|
                        pat <hex>|batch <spec.txt> <out.txt>    ← 主力：一次进程倒一批
                   batch 的 spec：data/cstr/func/name/imp/str/gbk/pat/xref/lx（见文件头注释）
   图标资源       python tools\extract_lib_bitmap.py    # 抠 krnln 的 RT_BITMAP LIB_BITMAP → shots\krnln_icons\
                   python tools\match_tree_icons.py       # 与 src\res\tree_icons.bmp 比对（结论：还不能替换）
| 自定义数据类型表 | 数据类型名 \| 公开 \| 备 注；成员子表 成员名 \| 类 型 \| 传址 \| 数组 \| 备 注 | #E6EDE4 |
   ⚠️ 两个坑      ① 地址**一定要加引号**，PowerShell 会把裸写的 0x100619e0 转成十进制再传进去
26. ⚠️ **`CEdit` 子控件的通知不走 `PreTranslateMessage` 的按键分支**：回车要按
    `pMsg->hwnd == m_wndEdit.GetSafeHwnd()` 判，失焦走 `OnCommand` 的 `EN_KILLFOCUS`（两条都要有）。
（= **EC_C_CMD**）、文本 #008080（= EC_C_STR）；`✔` #800000；`备 注` #008000（= EC_C_CMT）。
   我第一版用 `int v;` 接 double/指针，库写 8 字节直接把栈冲掉 → 探针 `0xC0000005` 崩。
   现在探针和 `ec_lib.cpp` 都用 `union { int; double; DWORD; LPCTSTR; BYTE raw[8]; }`。
    * 拖设计窗体中间 -> **不动**；拖窗体 SE 手柄 -> 只改尺寸 380x250 -> 403x268（左上不动）
   标记=0x00808080（野指针）。所以：① 建完要 `EcInitUnitGeometry()` 把 左边/顶边/宽度/高度/可视/禁止 写一遍；
   ② 读文本属性前**必须** `IsBadStringPtrA` 探一下再 `CString` 赋值，否则直接崩。
4. ⚠️ **`NotifySelection` 要发给主框架**（`AfxGetMainWnd()`），发 `GetParentFrame()` 给的是 MDI 子窗口，
    本工程原来画的是 **1px 黑色点线框 + 7x7 白色空框手柄**（HANDOFF 第 20 轮那条结论是误读），
5. ⚠️ 细分坑：**VC6 把 `.c` 按 C89 编译**，块内声明必须全放块首（`BOOL ok = f(...)` 写在语句后面报 C2065）；
6. ⚠️ 探针输出重定向到文件时，`cmd /c "... > file"` 写出来是 **UTF-16**；用 Python 读要按 `utf-16` 解码
- `tools\page_smoke.ps1` → 三条 `[PASS]`（用 treeinfo 的 click 模式点节点，再核对
| 按「用户说的」把 4 个循环尾关键字也隐藏 | ❌ 第 15 轮的误读；本轮对着原版例程改回"照画" |
1. ⚠️⚠️ **支持库建出来的窗口不带 `WS_VISIBLE`**（实测 `style=44000000`，只有 `WS_CHILD|WS_CLIPSIBLINGS`）。
  `--ashift=4` → **44 px / 119600 = 0.04%**，列宽 x=44/158/290/326/362/434 与原版
1. `LoadLibraryA(库文件)` → **优先 `GetProcAddress(h,"InitCtl")`**（lib2.h 里查不到的第二个导出，
2. ⚠️ **第三方 .fne 不是都经得起在 IDE 里直接调**：`spec.fne` / `shellEx.fne` 的 `GetNewInf`（或接口调用）
| `probe_sel.ps1` / `probe_drag.ps1` / `redump.ps1` / `sel_smoke.ps1` | ✅ 本轮新增的工具 |
3. ⚠️ **建"窗口"本体（数据类型 0）不能像组件那样传 `hParentWnd=NULL`**：探针里父窗口给 NULL 时
   `PFN_CREATE_UNIT` 返回 0；给一个真父窗口就成功（出来的是 `WTWindow`，style `42CA0000`）。
   设计器里窗体是挂在设计器视图上的子窗口，所以没问题。
4. ⚠️ **`theme_e2e.ps1` 会改注册表里的 `ColorScheme` 并保存**（它点组合框第 5 项再按确定）。
原版的「连续注释行」会画一个绿色虚框。基准 = `shots\live_fold_jy\view_before.png`
5. ⚠️ **销毁组件要走对象虚表 `vtbl[1](obj,1)`**（krnln 自己建窗失败回滚就是这个），
   直接 `DestroyWindow(hwnd)` 会把 krnln 的对象留成野指针。
6. ⚠️ **VC6 + MFC 4.2 的小坑**：`CString` 没有 `Trim()`；`CWnd` 没有 `SetCursor()`（用 `::SetCursor`）。
7. ⚠️ **别在自己进程里再注册 `WTWindow`/`_EL_*` 同名类**：会和支持库注册的类争抢（同名不同 hInstance），
| 横线 | 2px 高，从左边线拉到「本段最长那行的文字右边缘 + 3」（实测 866+2 = 867..868），**11 亮 1 灭**（第一段 6px：63..68，然后 70..80、82..92…） |
| 横线位置 | **空 `'` 行的行顶 +5**（实测 106/142/178/214/250/286 = 6 个空行的行顶+5，行距 18）；整段没有空行时就画「框住整段」的上下两条 |
| 单行注释 | **不画**（graphic.e 的 `shots\live_tabs\view0.png` 绿像素 = 0） |
   原版把 ►► 放在**块结束后的那一行**。要再拍一两个样本才能定死规则。
验证：`python tools\check_comment_box.py <png> <行距>` —— 同一脚本在**原版基准**上
   于是"搜 `mov eax,0x1017D6B8`"变成"搜字节 0xB8"，一次命中 **7340** 处、还顺手反编译了几十个无关函数。
   而 `shots\live_tabs\view0.png` 与 `shots\user_ref_code2.png` 里绿像素都是 0
   **判据**：命中数远大于预期就先怀疑模式串被截断（已修：`pattern = line[3:].strip()`）。
2. ⚠️ **找不到"谁引用了 LIB_INFO(0x101388A0)"** —— 全库只有 `GetNewInf` 一个引用点。
   那些表（数据类型/命令/常量）是运行期由初始化函数**直接写绝对地址**（如 `mov [0x1017D6B8+4i], fn`），
   所以**要按表的地址去搜字节**：`pat B8 D6 17 10`（= `mov eax, 0x1017D6B8`）。
3. ⚠️ **IDA 的 `Strings()` 认不出中文 GBK 串**（`str 信息框` 一条都搜不到），
5. **子程序表「备注」列宽**：原版 78、本工程 48（程序集表「保 留」列原版 54、本工程 42），规律未查明。
| 排列图标 | 57649 = ID_WINDOW_ARRANGE | MFC 自带；没有最小化的子窗口时它自己返回 FALSE（菜单变灰），行为正确 |
6. ⚠️ **`ida_bytes.find_bytes` 的模式串是十六进制文本**（`"B8 D6 17 10"`），不是原始字节；
| 全部关闭 | 32982 | 易语言自己的命令，本轮实现（`CMainFrame::OnCloseAll`，循环关掉所有 MDI 子窗口） |
复现：`powershell -File tools\winmenu_probe.ps1`（发 WM_COMMAND 后打印子窗口矩形/是否最大化）。
10. 支持库树根节点多一个"−"展开框；树图标只有根和类别；类别顺序差 3 项。
1. ⚠️⚠️ **自造的「文本形式 `.e`」原版根本打不开** —— 本机这份原版（`C:\Program Files (x86)\e\e.exe`）
   对纯文本 `.e` 一律显示空框架（`first-party probe` 的 `views=0`，标题里没有文档名）。
   实测连原版自带的文本形式样本 `shots\e_samples\dll_sample.e` 也打不开。
   **别再试**：不要浪费时间造 `.e`。**正解**：拿**原版自带的例程**当样本
| `idamcp.py` | **直连已经打开的 IDA**（ida-pro-mcp 的 JSON-RPC，端口 13337）。`python tools\idamcp.py get_xrefs_to "['0x100ca6b0']"`；参数用 **Python 字面量**（不是 JSON），因为 PowerShell 会把双引号吃掉 |
| `tree_pick.py` / `tree_index.py` | 从 treeinfo 的倒档里取「根节点文字 → 树 hwnd」/「(树号, 前序项号)」 |
| `treeinfo.c`（改） | 新增 `click` / `dclick` / `mdclick` / `mdclick` 模式：点树项。**md* = 真鼠标**（SetCursorPos+mouse_event），`click` = PostMessage（原版不认，见 §3.8-1） |
| `probe_page2.ps1` / `probe_page2m.ps1` / `probe_page2d.ps1` | 拍原版「点某个节点之后的那个页面」：自动切工作夹到"程序"页 → 点节点 → 抓整框 + 文档客户区 1:1 + 倒窗口表。`…2m` 用真鼠标（**拍原版一律用这个**） |
| `measure_page.py` / `measure_cells.py` | 从一张表格页截图里量出网格线位置、每行底色、**每个单元格的主色**（本轮所有列宽/配色都是它量的） |
| `menu_coverage.py` | 列「原版菜单里哪些命令还没有处理函数」（现状：137 条中 122 条没有） |
4. ⚠️ **`ECLONE_DUMP_MODEL` 有两个输出文件**：`<path>` 是**显示模型**（row/par），
   `<path 改后缀>.fold.txt` 才是 `scroll=` / `anchor,caret` / `fold row=` / `param row=`。
  3. **窗体也换/新建**：窗体已经是支持库建的（`EcCreateUnit(lib,0,"窗口")` → 真 WTWindow ✓），
5. ⚠️ **`PostMessage` 出来的 `WM_KEYDOWN VK_CONTROL` 不会更新 `GetKeyState`**
   **后继字节**出现在普通汉字里（**常 = B3 A3**、一 = D2 BB…）。于是 `.常量 …` 的"头"被切在
  5. 组件箱的图标（`EcDataType.bmpId` / `CMD_INFO.m_shtBitmapIndex` → krnln 的 `LIB_BITMAP`；格序见笔记 §7）。
- 想接**►► 落点规则**：多拍两个原版例程（`C:\Program Files (x86)\e\samples\经典数学算法集\*.e`），
- 想调流程线：`python tools\patch_sample_flow.py` → `build.bat` →
  `powershell -File tools\code_shot.ps1 -Tag flowtest` →
  `python tools\flow_scan.py shots\flowtest.png 0 80 95 260`，跟 2.1d 的表逐项比 →
   第 16 轮加 `bParam` 时漏了，读到栈垃圾 `4402570`，于是"这一行有没有参数归属线"变成随机真。
（`menu_main.rc` 本来就有）：57643 撤消 / 57644 重复 / 32939 删除行 / 32936 屏蔽 /
  找"谁调用了某个 API"用 `python tools\ida_font.py calls <API名>`。
- 想查支持库里某个命令的参数名：`cmd /c tools\build_argprobe.bat` →
- **改完 `.cpp` 里任何中文注释，构建时留意 `check_block_comments.py` 的输出**；
| 撤消 / 重复 | `CECEditor::Undo/Redo`：**整篇源码 + 插入点 + 滚动位置**的快照栈（`EcSnapshot`，上限 200 步） | 文档只有几十行，快照最省事也最不容易错；真要省内存再换增量栈 |
| 压栈时机 | `PushUndo()` 在**一次用户动作改完之后**调：键盘/输入法的插入、删除/退格/回车、整次粘贴、单元格改动（`SetCellText`/`ToggleFlagCell`）、删除行、屏蔽 | 低层 `InsertText`/`DeleteRange` **不**自己压栈（否则一次粘贴会变成几十步） |
   现象：整棵块结构浅一级、流程竖线落在 25 而不是 47、而且「如果真的体」比「如果真」多缩一格
| 屏蔽/解除屏蔽 | 行首插/去一个 `'`（解除时后面紧跟的一个空格也一起去掉） | ⚠️ 原版删不删那个空格**没对拍**，见 §6 |
| 收缩/展开子程序 | 用光标所在行往上找到最近的 `.子程序` 名字当键，走已有的 `SetCollapsed` | 展开所有 = 清空收起表 |
  * `shots\live_learn2\view0.png` 的 4 个 `如果/如果真`（体里各 1 条语句）→ 一个 ►► 都没有
    （x=20..66 全图扫，5 行高的藏青带 = 0）；
  * `shots\live_flowcmd\view0.png` 里嵌在循环里的 `如果真`（体里 1 条语句）→ 也没有；
  * `shots\live_fold_jy\view_before.png` 的 `如果真（是否为调试版（））`（体里 **2** 条语句）
    —— 块注释下面**第一条语句被吃进注释里**，**编译不报错、运行行为不对**。
    真踩过：`ec_editor.cpp` 里 `// 易语言的括号是全角的：a3a8「（」…`。
  **还没定死的是"落在哪一行"**：`shots\live_sel\after_a.png` 的 6 个块里，
  第 4/5 个块（块体第一条是嵌套 `如果真`）→ 原版落在**嵌套块结束后的那条语句**上，本工程也对；
    判据：字符串/逻辑看着对但行为不对时，先跑 `check_block_comments.py gbk`。
扒的是 e.exe 的 `sub_45F7F0`（装库主函数，0x45f7f0）和 `sub_467610`（0x467610）：
13. ⚠️ **参数行行距 16 ≠ 语句行高 22**，第一行参数行顶 = 语句行顶 + 16。
   `InitCtl(路径, &LIB_INFO, &handle)`；给打包库 `.opk/.npk` 和新式库用）
16. ⚠️ **参数行是"叠"在行上的，不占纵向空间** —— 要在 `DrawAll()` **最后**画，
17. ⚠️ **PowerShell 函数参数别用 `[string]` 收数组**（写 `[string[]]`）；
18. ⚠️ **控制台探针的 GBK 副本别放 `gbk\` 根目录**（`build.bat` 是 `cl *.cpp`，会 `LNK2005: _main already defined`）
   IDE 把自己那个 `PFN_NOTIFY_SYS` 交给库；库以后靠它反调：
   IDE 里是 `NES_*`（取主窗口/跑功能/选图片），编译产物里是 `NRS_*`（取 EXE 路径、分配内存、
19. ⚠️ **参照截图 `ref_dll_page.png` / `user_ref.png` 比真客户区原点低 4px** → 要 `--ashift=4`。
22. ⚠️ **`EcHeadWord`/`EcTrim` 是 `ec_editor.cpp` 里的 `static`**，`EClone.cpp` 里用不了。
23. ⚠️ **读原版树的节点文字要 `TVM_GETITEMA`(0x110E) + `TVITEMA.pszText` 偏移 12**
6. **「默认载入核心库」的那一行**：`if (_mbsicmp(Str1,"krn")==0) Str1="krnln";`
25. ⚠️ **`ON_WM_COMMAND` 这个宏不存在**；`OnCommand` 是 `CWnd` 的虚函数。
    本轮又踩：后台回归脚本还开着 EClone，build 直接失败。
  标记扫描       python tools\marker_scan.py <png> [x0] [x1]
- `GetNewSock(nNo)` 是个小分派表：`1000/1001/1003 → sub_1002ED08`、`1002 → sub_10064C80`（按 id 释放对象）、
  流程线扫描     python tools\flow_scan.py <png> [x0] [x1] [y0] [y1]
  选区/剪贴板    powershell -File tools\sel_smoke.ps1     （八条 [PASS]，本轮新增）
                 也可以 PostMessage(editorHwnd, WM_USER+102, 0, 0)  ← EC_WM_REDUMP
  原版样本 1:1   powershell -File tools\probe_live.ps1 -File "<原版自带 .e>" -OutDir shots\live_xxx
                 推荐样本：C:\Program Files (x86)\e\lib\krnln\samples\流程类命令.e
  刷新采样备份   python tools\save_sample_backup.py   （改了采样源码之后必须跑）
| `probe_sel.ps1` / `probe_drag.ps1` / `redump.ps1` / `sel_smoke.ps1` | ✅ 第 17 轮新增的工具 |
| **`probe_formpaint.ps1`**（品红 DC + WM_PRINTCLIENT） | ❌ **对 krnln 的窗口无效** —— 它不处理 WM_PRINT，测出来永远是"什么都没画" |
| **`probe_repaint.ps1`**（屏幕品红 + InvalidateRect(ERASE) + 抓屏） | ✅ 采用 —— **判断"谁画了这块"的唯一可靠方法**（本轮靠它才定下点阵是窗体自己画的） |
| **`probe_wndproc.ps1` / `probe_classbrush.ps1`** | ✅ 用来看窗口过程/类背景刷归属（实测两边 `class WndProc` 都是 `0x1C3E0A60`，同一个 krnln） |
  设计器环境变量 set ECLONE_DESIGN_DEMO=1      开机就放一个真"图形按钮"（+ 每个额外库放一个）
| 用覆盖层的 `PreTranslateMessage` 拦列表点击 | ❌ 拦不到 → 改成给 ListBox 挂 `WindowProc`（`CEcCheckList`） |
| **`dump_dialogs.py` 倒原版对话框模板**（`DIALOG id=321`） | ✅ 采用 —— 支持库配置对话框的坐标/控件 id 全照它抄，实测运行尺寸与原版一致 |
| **`libcfg_smoke.ps1` / `resize_smoke.ps1` / `regress.bat`** | ✅ 第 20 轮新增的工具（回归一条龙） |
                    reg add "HKCU\Software\FlySky\E\EClone40" /v ColorScheme /t REG_SZ /d "默认配色" /f
                   ida_work\krnln.fne（**已分析并存盘**，funcs=5559）
                   ★ krnln\krnln_anatomy.md     第 18 轮：krnln.fne 全解剖（伪 C 版，优先读这份）
                   ★ krnln\window_designer_notes.md  第 18 轮：窗口辅助类 / 窗口创建调度 / 设计器接口(ITF_*)
                     krnln\window_notes.md      第 1 版窗口调度笔记（§2.5 已列出要改口的几条）
                     krnln\ide_lib_host_notes.md / getnewsock_notes.md
                     krnln\out\s1..s8_*.txt     分批倒出的伪 C（s2 里有 24 个组件类注册）
                     krnln\out\cmdmap.tsv       ★ 688 条命令 idx→实现函数（57 条空=编译器内建）
                     krnln\spec\s*.txt          batch 的输入 spec（想复现某批照着跑）
                     scripts\cmdmap.py          生成 cmdmap.tsv 的脚本
   `python -c "print('m_stageSize' in open(r'shots\zoom\sample_backup.txt',encoding='utf-8').read())"`。）
                    （func_qty=0），要一次进程里全倒出来（就是 report / batch 模式）
                  ③ batch 的 `pat` 是「整行剩余部分是模式串」，用 split 会截断（见坑 3.4-1）
   idalib         系统 Python312 `import idapro`；一次开关库约 30 秒
   正解：`Start-Process -FilePath taskkill -ArgumentList '/f','/im','e.exe' -Wait -WindowStyle Hidden`。
8. ⚠️ **`PrintWindow` 整框截图与"真客户区坐标"差 4px**：表页对拍同样要 `--ashift=4`（原版那张
9. ⚠️ **PowerShell 会把传给 `python -c` 的双引号吃掉**（`'["0x.."]'` 到 python 变成 `[0x..]`）。
   ② 干脆**写成 `.py` 文件再跑**（`menu_coverage.py` 就是这么来的）—— here-string `<<'EOF'` 在
     没做的：**改到"需要重建窗口"的属性时自动重建**（原版 `sub_4AE230`：ITF 5 取数据 → ITF 1 重建 →
  缩进           正文列 = 46 + 22n；块首 = 外层 + 1 级；**块体和块首同一级**；块尾和块首同一级；
  流程线         竖虚线 3 亮 3 灭；x = 正文列(n-1) + 1（级别 1 → 47、级别 0 → 25）
                 块首横虚线：循环类 x..x+15、如果家族 x+2..x+15
    另外**数值字色存疑**：原版 `信息框（"…", 0, ）` 的那个 `0` 实测是 `#800000`，
    （`寻找` / `查找寻找文本：` / `查找下一个(&F)`），第 15 批的 `finddlg_smoke.ps1` 断言的是
13. **支持库配置的已知取舍**：没载入的库在列表里只显示**文件基名**（原版显示名字/版本/数字签名，
    **错的三处一个都没暴露**。第 16 批把原版 dump 按 **UTF-8**（不是 GBK！）读出来才看到原文是：
  1. ~~属性表接支持库~~ **已完成**（ITF 6 读 / ITF 4 改 / ITF 2 灰化；见 §2.6）。
  代码区         TOP_PAD=4；正文 x = 46 + 22n；流程竖虚线 x = 24 + 22n
  参数归属线     第一行参数行顶 = 语句行顶 + 16、行距 16、文字再下 4px
    **判据**：凡是"原版长什么样"的期望值，**只能从 `shots\e_dialogs.txt`（或截图）里抄**，
    直接 `print` 出来是乱码，会让人误以为文件是 GBK（本轮就先"修"成 GBK 读、得到二次乱码
  2. ~~缩放组件~~ **已完成**（`tools\resize_smoke.ps1` 四条 PASS；见 §2.7c）。
    `treeinfo.exe <pid>` 的 dump 里**只有 `root=系统核心支持库` 那一棵树**（外加几棵 items=0 的空树），
     要根治得在 krnln `sub_100CA6B0`(0x100ca6b0) 的 `a14==1` 分支 / `sub_100C5E80`(0x100c5e80)
  **跑完 `python tools\restore_sample.py` + `build.bat`**。
13. ⚠️⚠️ **别反复 `taskkill /f /im e.exe`**（第 21 轮第二十一批实测）：原版（加密狗版）被连续强杀之后
    `易语言-加密狗版 - Windows窗口程序`、**没有文件名**，此时最大的 `Afx:400000:b` 是左边的程序树 210x636。
  （两态截图落在 `shots\param_before/after/back.png`）。
14. ⚠️ **`SendMessage` + 模态对话框 = 死锁**（第 21 轮第二十二批实测）：冒烟脚本里想用
    `SendMessage(child, WM_COMMAND, 32965)` 去开一个 **`DoModal`** 的对话框，
  `tools\argprobe.exe "C:\Program Files (x86)\e\lib" 信息框`。
  `python tools\dump_dialogs.py "C:\Program Files (x86)\e\e.exe" shots\e_dialogs.txt`
  `powershell -File tools\probe_design_orig.ps1 -OutDir shots\live_des_orig`
  （它会 `WM_MDIACTIVATE` 到设计器页，并 1:1 拍 `WTWindow`）。
- **想判断"某块像素是谁画的"**：`powershell -File tools\probe_repaint.ps1`
  ⚠️ `probe_formpaint.ps1`（WM_PRINTCLIENT 品红 DC）对 krnln 的窗口**无效**，别再用它下结论。
   **真正管用的方法**（`tools\probe_repaint.ps1`，以后判断"谁画的"照这个来）：
   ② `InvalidateRect(窗口, NULL, TRUE)` + `UpdateWindow(窗口)`；
   ③ 重新抓屏。**东西回来了 ⇒ 是这个窗口自己画的**。
   还要先把目标文档页 `WM_MDIACTIVATE` 到最前，否则它被别的文档页盖着，重绘根本不发生。
2. ⚠️⚠️ **父窗口画不到子窗口底下**。Windows 会把父窗口 `WM_PAINT` 的更新区/裁剪区
- **►► 的落点规则**（第 5 节第 1 条）：本轮把「跨行数 ≥ 3 才画」这半条规则**用数据定死了**：
   ⇒ **要在"窗体客户区"上画东西，只能用窗体自己的子窗口**（本工程用 `ECDesignOverlay`）。
3. ⚠️ **覆盖层画"纸"之前必须 `ExcludeClipRect` 掉每个真组件**：覆盖层在 z 序顶端
   （`SyncOverlay` 用 `wndTop` 就是为了拿鼠标），否则一铺底就把组件盖没。
   画完要 `SaveDC/RestoreDC` 还原裁剪区，**不要** `SelectClipRgn(NULL)`
   （那会把 `CPaintDC` 自己的更新区裁剪也抹掉，手柄会画到更新区外面去）。
4. ⚠️ **手柄画法**：原版是 **7x7 白框**（1px 白边 + `NULL_BRUSH` 内部透空），
  但第 6 个块（`.计次循环首 / 子程序1 (变量1) / .计次循环尾` 后面跟 `画板1.滚动写行`）
5. ⚠️⚠️ **单选 ListBox 的"单击整行翻转勾选"**：原版模板里没有 `LBS_MULTIPLESEL`，
- **选区配色**只在**默认配色**下实测过（`#000080`）。换 `.clr` 后 `EC_CS_MARK` 的实际值
   - 往**对话框的 `PreTranslateMessage`** 里拦 `WM_LBUTTONUP` —— **实测拦不到**（这条消息到不了
     对话框的 `PreTranslateMessage`，写了等于没写，白折腾两轮）；
   - 正解：给 ListBox 挂自己的窗口过程（`src\ec_syscfg.cpp` 的 `CEcCheckList::WindowProc`），
     ⚠️ **`SubclassDlgItem` 之后一定要自己设 `m_lst.m_pOwner = this`**（忘了设 → 静默不生效，
- **参数归属线的嵌套调用 / 展开态最后一条语句**（同第 5 节第 3 条）。
- **`数值前景` 到底是哪个色**：原版 `信息框（"…", 0, ）` 里那个 `0` 实测 `#800000`，
   所有"往里插东西"的地方都要先清空 —— 树 `DeleteAllItems()`、属性页列 `DeleteColumn(0)` 循环删。
   不清的后果是可观测的：工作夹支持库树 **1560 → 3120 → 4680**（每次重新载入都再插一遍）。
7. ⚠️ **64 位 PowerShell 往 32 位进程发 `WM_SETTEXT` 的字符串指针不可靠**：
   `Marshal.StringToHGlobalUni` 的地址可能 > 4GB，32 位目标只拿到低 32 位（实测搜索框里
   只进去一个乱字符，过滤结果 34 条）。**正解**：清空用 `WM_SETTEXT` + `NULL`，正文用
- **设计器"纸"的颜色是硬编码 `COLOR_BTNFACE`**（`EC_DESIGN_PAPER`）：原版画的是窗体**底色属性**的值。
   要断言控件外观，就**拍整个对话框**，再按对话框坐标取样（勾选框内部 = 对话框 (20..28, 64..72)）。
9. ⚠️ **缩放手柄必须用"鼠标位移"改边，不能用"绝对坐标"**：手柄中心画在框外 **3px**，
- **设计期"网格对象"为什么没建起来**：只有静态伪 C 的推断（`sub_100CA6B0` 的 `a14==1` 分支），
10. ⚠️ **`.ps1` 里一个非 ASCII 字节都不能有**（第 13/15/16/17 轮都踩过，本轮又踩一次：
- **支持库配置对话框**：没载入的库只显示文件基名（原版显示注册表里存的名字/版本/签名）；
11. ⚠️ **"窗口"类的 `底色` 属性用 ITF 4 写不进去**：下标用 `EcPropIndexByName(lib,0,"底色")` 找
    （实测 = 10，`EcGetUnitProperty` 把它按颜色格式化成 `#RRGGBB`），
    `EcSetUnitProperty(..., "#F0F0F0")` 之后**回读还是 `#030000`**。
    这和 §2.6 那条"有的类 ITF 4/6 属性序号跟元数据顺序对不上"同类 ——
    所以设计期纸色**先固定 `COLOR_BTNFACE`**（`EC_DESIGN_PAPER`），别急着做"改底色→纸跟着变"。
    要根治：逆 `sub_100CAD20`（窗口类 ITF 4）里 case 10 到底写哪个字段。
### 3.6 第 19 轮第二批（属性表）踩的
   实测 krnln 标签类 `sub_10035BD0` 的每个 case 都 `return 0`。我第一版把它当成功/失败，
⚠️ 构建前先   Get-Process EClone | Stop-Process -Force   （否则 LNK1104）
3. ⚠️ **NULL 数据建出来的组件，属性字段是未初始化的垃圾**：实测标签的 左边=6249016、禁止=真、
                 ⚠️ 跟 shots\ref_dll_page.png / user_ref.png 比要 --ashift=4
                 （逐行列非白 run + 颜色分类：B=藏青 d=灰 #=黑 g=填充）
  设计器探针     cmd /c tools\build_designprobe.bat → tools\designprobe.exe "<任意 .fne 全路径>" [试几个]
                 set ECLONE_EXTRA_LIBS=iext2     额外载入的支持库（分号分隔），其组件进"扩展组件"
  原版对话框模板 python tools\dump_dialogs.py "C:\Program Files (x86)\e\e.exe" shots\e_dialogs.txt
                  （支持库配置 = DIALOG id=321；系统配置 = id=282）
  原版设计器基准 powershell -File tools\probe_design_orig.ps1 -OutDir shots\live_des_orig
                  （WM_MDIACTIVATE 到"窗口: _启动窗口"页 + 1:1 拍 WTWindow → form0.png / view2_client.png）
  原版支持库配置 powershell -File tools\probe_libcfg.ps1 -OutDir shots\live_libcfg
| **`idamcp.py`（HTTP JSON-RPC 直连已打开的 IDA）** | ✅ 采用 —— 比 `ida_font.py`（idalib 每次开关库 30 秒）快得多，且能读 GUI 里已载入的那个 idb |
| 用 `EcHeadWord` 认新声明关键字 | ❌ GBK 半字节切断（常=B3A3）→ 按 `EcGbTail` 整字推进，见 §2.8f |
   文件里真值只有 **2** 处（`0x1008641e` 填充函数 + `0x1013890c` LIB_INFO 字段本身）。
                 set ECLONE_DESIGN_DUMP=<文件>   把"属性"页倒成 prop|下标|名|值 / end|条数|lib=|dt=（自测用）
   资源位置找到了（`tools\extract_lib_bitmap.py` 能抠），但**格宽/格序没定死**，别急着替换图标。
                        decname <子串> [out]|decxref "0x..,0x.." [out]|str <子串>|xref <名字/0x地址>|
  cmp_flow_17.png        流程线对比：上=原版（流程类命令.e 的「到循环尾」块），下=本工程同一形状
| 给原版发 `PostMessage` 鼠标消息点程序树 | ❌ **原版不认**（caret 动了但不换页）→ 改 `treeinfo.exe <pid> mdclick`（真鼠标），见 §3.8-1 |
12. **设计器**：能放置/选中/拖动/缩放/Delete；**还没有**"新建窗体/切换窗体"、组件箱图标、`.e` 打开/保存。
    正解：`OpenProcess` + `VirtualAllocEx`（**一次分配、循环复用**；反复 alloc 到 32 位目标上
    会开始返回 NULL，实测第 3 次就失败）+ `WriteProcessMemory` 写 `TCITEM`/空白 + 读回。
    ⚠️ 32 位目标的 `TCITEM.pszText` 是 **4 字节**（不是 8），`BitConverter.GetBytes((int)...)`。
     `SetParent` 把旧子窗口迁过去）。判断依据是 ITF 4 的返回值（真 = 需要重建）。
                 `※` 左边缘 x = 语句正文列 + 44；左侧竖虚线 x = 正文列 + 31
> ④ 编辑菜单第一包（验收 24：撤消/重复/删除行/屏蔽/解除屏蔽/收缩展开，`edit_cmd_smoke.ps1` 7 PASS）；
> ⑤ 寻找下一个/上一个 F3 / Shift+F3（`find_smoke.ps1` 4 PASS，实现时抓出并修掉了"Shift+F3 原地不动"的搜索原点 bug）；
  设计器画布     工作区 #A0A0A0（**不是** #808080）；窗体客户区"纸" #F0F0F0（= COLOR_BTNFACE）
> **回归现在 21 步**（新增 17 表页 / 18 绿框 / 19 编辑命令 / 20 寻找下一个·上一个 / **21 寻找对话框**），
> 表页 3 PASS、绿框 7/7、编辑命令 7 PASS、寻找 4 PASS、**寻找对话框 11 PASS**、采样 `restored 2656 bytes`。
> 本会话还完成了第 7 项：**原版那个模态「寻找」对话框**（Ctrl+F，#32877；照 `DIALOG id=284` 1:1，
  支持库配置     原版 DIALOG **id=321** 322x290 DLG（实测 489x464 px，11 个控件）：
                 1115 静态"查找（可输入拼音字母）：" (7,10,28,8)、1011 查找框 (40,7,275,12)、
                 1036 列表 (7,22,308,127) LBS_OWNERDRAWFIXED、1001 详情只读多行 (7,151,308,85)、
> ② 原版「系统配置」= `DIALOG id=282`（267x217 / 78 控件 / 含 1 个 `SysTabControl32`，
                 5 删除(158,265,45,18) / 1 确认(219,265,45,18) / 2 取消(270,265,45,18)
  列表项外观     左边 11x11 方框（1px #808080 边 + 白底，勾上画黑勾）；选中行 COLOR_HIGHLIGHT + 白字
   - `TABLE 0 cols=4 kind=0 w=96,42,42,42`  ← 只有第 2 列差（42 vs 54）
   - `TABLE 1 cols=6 kind=1 w=114,72,36,36,24,24` ← 六列（备注占 2 列），合计 350 vs 原版 380
   （= 2 个汉字 30px + 12 内边距，带空格的 `保 留` 是 30+12+12 = 54 —— 也就是这个字体里
20. ⚠️ **`diff_png.py` 的 x0,y0 只是"共同区域起点"，不是偏移量**；偏移用 `--ashift`。
      ⇒ **改法**：`src\ec_editor.cpp` 里程序集表的表头把**第一个**保留列写成两个字之间的空格数
      ⇒ **改法**：先看原版 `live_tabs/view0.png` 子程序数据行（y=76..94, x=302..380）**有没有
   python tools\measure_page.py shots\live_tabs\view0.png 600        # 竖线/横线位置
31. ⚠️ **原版截图坐标 ≠ dump 坐标**（原版最大化 `截图y = dumpy+8`；本工程 `截图 = dump-(134,57)`）。
32. ⚠️ **字体**：原版正文字体来自「系统配置→程序显示→字体」，字形不可能 100% 一致，
6. **子程序表「备注」列宽**：原版 78（graphic.e）/ 216（jy.e），本工程 48 ⇒ 这一列是按内容
   算的，本工程 48 是"最小值"，**八成是 step-14 采样里漏了那条备注文字**（见上面第 5 条
   的改法 2）。**先别改算法**。
7. **单元格编辑只做了"能改"这一层**：没有多行备注、插入/删除字段；
   `Shift+方向键`（现在按 Shift 也会更新 anchor，但没有"扩展选区"的语义）没接。
   （撤消已经做完了，见 §2.9）
7. **参数行不跟随光标/编辑**（`DrawParamLines` 每次绘制重新解析，改了语句文字会跟着变，这是好事）。
| 自造文本形式 `.e` 喂原版（`flow_ref.e`） | ❌ `views=0`，原版打不开（连它自带的 `dll_sample.e` 也打不开） |
| 用**原版自带例程**当样本拍 1:1 | ✅ 采用 —— 流程线真值的唯一可靠来源 |
| 按缩进推块边界（`ComputeBlocks`） | ❌ 块尾隐藏时把 `返回 ()` 吞进循环体 → 改成源码关键字栈配对 |
| `EcRow::bParam` 不初始化 | ❌ 流程槽凭空多出一堆 `-/+` → 两个创建点都初始化 |
| `InsertNewline` 不同步 anchor | ❌ 粘贴多行时换行全丢 → 同步 anchor |
| 用 `LP` 当 PowerShell 函数名 | ❌ 撞上 `Out-Printer` 别名 → `MkLp` |
| `probe_live.ps1` 顺便拍标签条 | ✅ 采用（标签条 1:1 基准的唯一来源） |
| 在设计器视图（窗体父窗口）里画"纸 + 点阵" | ❌ 被窗体这个子窗口裁掉，永远画不上去 → 改成由**覆盖层**画 |
| 给原版发 `TVM_SELECTITEM`（TVGN_CARET） | ❌ 也不换页 —— 别拿它当"等价于点击" |
| **`probe_page2m.ps1`（真鼠标点节点 + 抓整框/客户区）** | ✅ 采用 —— 本轮所有"原版某个表页长什么样"都是它拍的 |
| **`measure_page.py` / `measure_cells.py`** | ✅ 采用 —— 表页的列宽、行高、每格字色全是这两个量的 |
| **`check_comment_box.py`** | ✅ 采用 —— 绿框几何断言，原版基准 + 本工程渲染两边都跑 |
| `EcSplit` 不认引号 | ❌ 长文本常量里的全角逗号会把字段切断 → 引号内不切分 |
  第 1/2/3 个块（块体唯一那条简单语句）本工程的规则对；
  原版把 ►► 放在**循环结束后的那一行**，本工程会放在 `子程序1` 那行。**这 1 个反例没搞懂**。
  与字段序是否与原版一致**没验证**（.clr 的 DWORD 序本来就有已知的 ±1 错位）。
- **滚轮在"鼠标悬停在编辑器上但编辑器没有焦点"时**会不会滚**没在真机上手动试过**
> ② 多行注释绿框（验收 23：`check_comment_box.py` 两边各 7/7）；
> ③ 「窗口」菜单（实测层叠/平铺/排列图标/分隔本来就是 MFC 标准 id，本就能用；补了「全部关闭」）；
> ⑥ 代码页列宽两处差异（表头空格 + 子程序表备注最小宽 78）→ **像素差 0.62% → 0.31%**；
> ⑦ **原版那个模态「寻找」对话框**（验收 25：Ctrl+F / #32877，照 `DIALOG id=284` 1:1 抄；
>    标题 寻找、387x143 px、四个原版控件 id 1151/1041/1/2 与标签全对；`finddlg_smoke.ps1` 11 PASS）。
- **程序集表头 `#ECECEC`** 是"当前程序集高亮"还是固定底色：两个样本都只有"当前程序集"可见，无法区分。
> 21 寻找对话框 + 寻找替换对话框 / 22 书签 / **23 书签跳转对话框**），
> 最近一次 `shots\regress_r27.log`：**`[FAIL]` 0 条**，`[13]` Dll 页 0 px、`[14]` 代码页 1404 px / 0.31%、
  本轮试过"读/写底色属性"两条路**都没成**（写不进去，见坑 3.7-11），所以先固定成原版设计期的默认纸色。
> 本会话（第 21 轮，第 1~23 批）**已完成并验证 10 项**：5 张表页 / 多行注释绿框 / 窗口菜单 /
> 编辑菜单第一包 / F3·Shift+F3 / 寻找对话框 / 寻找替换（Ctrl+H,#3） / 书签（Ctrl+F6,F6,Shift+F6） /
> **书签跳转（Ctrl+F7,#32965）** / 代码页列宽 0.62%→0.31%。
> ① 原版「系统配置」= `DIALOG id=282`（267x217 / 78 控件 / 含 1 个 `SysTabControl32`，形态是
>    "一个模板放全部页面 + 页签切换"；一条命令列全：`python tools\dialog_inv.py shots\e_dialogs.txt 282`）
> ② 寻找替换 Ctrl+H（原版是**另一个模板**，先 `python tools\dialog_inv.py shots\e_dialogs.txt <id>`
>    把它找出来；`id=284` 是"开始寻找"那个）
> ③ 然后是 插入菜单 → 设计器"新建/切换窗体" → 设计期网格对象链 → **运行/调试**
> 本会话结束时：构建 `BUILD OK`、无残留进程、采样源码完好（`m_stageSize` + 注释块都在）。
**状态**：能编译、能跑；**验收 1~27 全过**（第 1 节的表），**23 步回归全绿**
（最近一次 `shots\regress_r27.log`：`[13]` Dll 页 0 像素差、`[14]` 代码页 0.31%、`[21]`/`[23]` 两个对话框共 38 PASS、`[FAIL]` 0 条）。
运行           D:\eclone\bin\EClone.exe [designer]
代码编辑器能点选/框选/复制/剪切/粘贴/全选/滚轮；**多行注释绿框**按实测几何重做；
**5 张表页（全局变量/数据类型/常量/图片/声音）点程序树节点即可打开**；
  行/列扫描      python tools\rows_scan.py <png> [编辑器x偏移] [y上限]
**已知的剩余差距（按性价比排序）**
1. **►► 的落点规则还有 1 个反例**（见第 6 节第 1 条）。本工程按
   「块首行之后第一条缩进深一级、且自身不是块首关键字的语句」实现，
   在 `live_flowcmd` 的 6 个块里对 5 个，第 6 个（`返回`演示：块体只有一条简单语句）
2. **「多行注释块」整个没做**：原版把连续的多行注释画成一个**绿框**
   证据 `shots\live_fold_jy\view_before.png`（6 段全绿框，绿像素 21957 个）；
   —— 判据待定（大概只对「连续 ≥2 行」成立）。本工程只把 `'` 之后整行染成注释色，不画框。
   ⚠️ 那个绿框会让「绿像素计数」这类断言永久失真。
3. **参数归属线的两处边角**（都还没实测）：① 嵌套调用（`信息框（取文本（a,b）, 0）`）里
     程序集表  竖线 44,104,158,200,242        -> 列宽 60 / 54 / 42 / 42
    第 21 轮第二批之后是 **11 条自己写的**（系统配置/支持库配置/全部关闭 + 编辑菜单 8 条）
  设计器冒烟     powershell -File tools\design_smoke.ps1    （五条断言，见验收 17）
  一条龙回归     cmd /c "D:\eclone\tools\regress.bat"        （第 20 轮新增：上面整串 16 步）
  支持库配置冒烟 powershell -File tools\libcfg_smoke.ps1     （七条断言，见验收 18）
  缩放冒烟       powershell -File tools\resize_smoke.ps1     （四条断言，见验收 19）
      把第一个「保留」列的表头从 `保 留` 改成 `保   留`（**三个空格**；本工程这个字体一个空格 6px，
      ⚠️ 别再按"两个 保留 列表头一样"改回去 —— 原版实测这两列就是 54 / 42，差的就是那一个空格的**推进宽度**
    **插入点所在的那个「词」**（`EcIsWordChar`/`EcWordAt`，按 GBK 整字推进，全角标点 A1/A3 区不算词）；
   2. ~~**子程序表备注列**：原版 78(graphic.e) / 216(jy.e)，本工程 48。~~ → **第 21 轮第六批改完并验证了**：
      规则 = `max(内容宽 + 12, 78)`（graphic.e 的备注格是空的、原版照样 78；jy.e 有 17 字备注 → 216）。
      实现：`Layout()` 的跨列补齐那一段里，对 `ECT_SUB` 的跨列格把 `need` 抬到 78
      （本工程原来是两个 24 的子列 = 48）。改完 `TABLE 1 w=114,72,36,36,24,54`（合并 = 78 ✓）。
       ★ **代码页像素差：2832 px (0.62%) → 1404 px (0.31%)** —— 两处加起来正好是那两块
      `[FAIL]` 0 条，`[13]` 仍是 0 px，`[14]` 1404 px / 0.31%，采样 `restored 2656 bytes`）。
      复核命令：`python tools\patch_sample_graphic.py && cmd /c build.bat && powershell -File tools\code_shot.ps1`
    （第 21 轮第十八批）：`CECEditor::ToggleBookmark/GotoBookmark`，书签存在**源码行号**的有序数组上
                  python tools\ida_krnln.py report <out> "0x.."（已并入上面那串模式列表）
      **"一个模板里放全部页面的控件 + 一个 SysTabControl32 按页切换显示"**
      （不是 MFC 的 `CPropertySheet`/每页一个模板）。
    - 第一个页面的控件（GBK 文本从 dump 里读就能看全）：分组框 `Windows 窗口程序的运行方式`、
      两个单选（启动窗口 / 启动窗口的子窗口）、两个复选（保存 .BAK 备份文件 / 编译发布版本时保存附加信息）、
      `Edit id=1001/1011`（31x12，对应"自动保存时间（分钟）"/"最大内存（千字节）"两个 Static 提示）、
      `Static id=1251 + Edit id=1236 + Button id=1242 "..."`（可另保存的文件名 + 浏览）、
      分组框 `默认输入法` + 三个单选（全拼/双拼/英文）+ `Button id=8 编辑输入法列表(&E)` +
      两个复选（拼音输入法支持南方音 / 临时…）。
    ⇒ **下一步**：照 78 个控件的坐标/id 把整个模板抄进 `src\ec_syscfg.rc`（同一模板 + 一个页签控件，
    按页显示/隐藏），就能一次把 6 个页签都做出来 —— 比一页一页猜快得多。
    读文本的正确姿势：`python -c "import io;print(io.open(r'shots\e_dialogs.txt',encoding='utf-8').read())"` 会按 UTF-8 读，
    dump 文件本身就是 UTF-8；**控制台显示成乱码只是 GBK 控制台的问题**，别以为是文件坏了。
11. 支持库树根节点多一个"−"展开框；树图标只有根和类别；类别顺序差 3 项。
   注：~~**代码区那 0.62%**：子程序表「备注」列宽 + 字体字形~~ → **第 5/6 条已定位到两处具体的
    差异**（① 程序集表第 2 列的表头少一个空格 ② step-14 采样漏了子程序的备注文字）；
    **"字体字形"这条已被证伪**（两边 ink 宽度完全一致：47 / 89 / 71）。
    「数值字色 #800000」的去留见第 6 节的存疑条目。
    而本工程内置配色的「数值前景」槽是 `#008000`（绿）。本轮**又拿到两处新证据支持 #800000**：
    常量数据表的数值常量、图片资源表的「内容」（字节数）**都是 #800000**。三处独立证据了，
    下一轮可以直接把「数值前景」槽改成 #800000 再跑 `check_theme.py` 看是不是全过
    （**注意**：`check_theme.py` 是从 `.clr` 文件读的，改内置默认值不一定影响它 —— 先看清楚再动）。
    那是它安装时写进注册表的）；「删除」只把该项从"要载入"里去掉，不删磁盘文件；
    「购买授权」是提示框；查找只支持名字子串（原版支持拼音首字母）。
14. **菜单 137 条命令里还有 116 条没有处理函数**（`tools\menu_coverage.py` 随时可查）。
    第 21 轮之后是 **18 条自己写的**（系统配置/支持库配置/全部关闭 + 编辑菜单 10 条
    （撤消/重复/删除行/屏蔽/解除屏蔽/收缩/展开子程序/展开所有/开始寻找/寻找下一个·上一个/寻找替换）
    + 书签 3 条），`python tools\menu_coverage.py` 的输出里 `handled by ON_COMMAND` 一栏就是实时的数字
    + 12 条 MFC 自带（窗口菜单 + 剪贴板的撤消/复制那几条 —— ⚠️ 剪贴板"能用"是编辑器的键盘
    处理，不是菜单处理函数）。
**下一步的确切动作**（按性价比排序；第 21 轮重新排过）
- **① 编辑菜单**（用户最容易感觉到的缺口）：
  - ~~撤消/重复（Ctrl+Z / Ctrl+Y）+ 删除行（F10）+ 屏蔽/解除屏蔽（Ctrl+K / Ctrl+M）+ 收缩展开~~
    **第 21 轮第二批做完了**（见 §2.9，验收 24）。
  - **做完了（第 21 轮第十一/十二批）**：**寻找下一个 / 寻找上一个**（F3 / Shift+F3，#32878 / #32879）
    → `CECEditor::FindNext(BOOL bForward)`：找的词 = 当前选区（单行内），没有选区就用
    沿方向绕文档一圈找，找到就**选中它**。
    ★ **验收：`tools\find_smoke.ps1` 四条 `[PASS]`**（已接进 `regress.bat` 第 20 步）：
    ```
    点第一处「信息框」(src=24)  -> anchor/caret = 24,5
    F3        -> 跳到第二处：caret=28,12..28,18   ← 正好选中「信息框」三个字的 6 个字节
    Shift+F3  -> 回到第一处：caret=24,4..24,10
> 本会话（第 21 轮，第 1~15 批）**已完成并验证**：
    ⚠️ **实现时踩到的真坑（已修，写在这里免得重犯）**：搜索原点**不能**用插入点当前值 ——
    找到之后选区正好是「词首..词尾」、插入点在词尾，反向搜 `at + len <= caret`
    会把**刚找到的这一个**再命中一次（现象：Shift+F3 原地不动，`find_smoke` 就是这么抓到的）。
> **回归现在 23 步**（新增 17 表页 / 18 绿框 / 19 编辑命令 / 20 寻找下一个·上一个 /
    ⚠️ **原版还会在流程槽里画一个书签标记，本工程暂时不画** —— 没有拍到那个标记的基准
> 表页 3 PASS、绿框 7/7、编辑命令 7 PASS、寻找 4 PASS、两个对话框 24 PASS、书签 5 PASS、
  - 书签跳转（#32965 Ctrl+F7）**做完了**（第 21 轮第二十二批）：原版模板 **`DIALOG id=607`**
    （408x264、5 个控件）——`src\ec_syscfg.rc` 的 `IDD_BOOKMARK`（`SysListView32 id=1036` +
    中文**全部照 `shots\e_dialogs.txt` 的原文**）+ `CEcBmDlg`/`EcShowBookmarks` +
    `ON_COMMAND(32965, OnEditBookmarkDlg)`；跳转用"行号+1"当 DoModal 返回值（0 = 取消）。
    **验收 27 = `tools\bm_dlg_smoke.ps1` 十四条 PASS**（已接进 `regress.bat` 第 23 步）：
    `LVM_GETITEMCOUNT(0x1004) = 1`，按「全部删除」后 `= 0`。
    ⚠️ **列标题是推的**（模板里只有 `List1`，没有列信息）：本工程按「行号 / 内容」两列做，
    ★★★ **第 21 轮第十七批做完了**（`DIALOG id=578` 270x82、**10 个控件**、标题 **寻找替换对话框**）：
    模板原文来自 `shots\e_dialogs.txt`（**按 UTF-8 读**，中文一个字都没猜），
    + `ON_COMMAND(3, OnEditReplace)` + 编辑器侧 `m_sReplaceText`/`ReplaceCurrent()`/`ReplaceAll()`）。
    **验收**：`tools\finddlg_smoke.ps1` 现在同时管两个对话框，**24 条全 PASS**，其中新增的实测值是
    标题 `寻找替换对话框`、**411x152 px**（= 270x82 对话框单位）、8 个原版控件 id 全在
    （1181/1152/1047/1041/1/3/4/2）、按钮文字 `搜寻下一个(&F)`/`替换(&R)`/`全部替换(&A)`/`关闭(&C)`
**流程线**按原版自带例程 1:1 重做（含循环尾关键字、►、▼、►►）；
    ⚠️ 本批又踩了两个坑（都已写进 §3.8-11）：**RC 侧只能看到 `resource.h`**——新增控件 id 必须
    在 `resource.h` 和 `ec_syscfg.h` **两边都加**（只加后者 → `EClone.rc(540): error RC2104 :
    undefined keyword or key name: IDC_REPL_HINT`）；以及 `.ps1` 里**又混进了中文**（`Check "...欲搜寻文本："`
    这几行）→ 直接语法错，最后靠"**剔掉文件里所有非 ASCII 字节**"一键治好（断言值本来就是码点拼的，
「窗口」菜单的层叠/平铺/排列图标/分隔（MFC 自带）+ 全部关闭可用。
    ⚠️ 与原版的**已知差异**：原版这个框是**非模态**的（点「搜寻下一个」不关窗），本工程用 `DoModal`，
    点任一按钮就返回按钮 id 再执行（见 §6）。
    [00] Static 65535  "欲搜寻文本："      7,10   53x8
    [01] Static 65535  "替换文本："        7,26   53x8
    [02] Static 1047   ""                 71,44  192x8    (WS_GROUP，一行提示/状态)
    [03] ComboBox 1181  -                66,7   197x135  CBS_DROPDOWN|CBS_DROPDOWNLIST|CBS_AUTOHSCROLL
    [04] ComboBox 1152  -                66,23  197x135  同上（替换文本）
    [05] Button 1041   "区分大小写"        7,44   62x8    BS_AUTOCHECKBOX
    [06] Button 1      "搜寻下一个(&F)"    7,59   64x16   BS_DEFPUSHBUTTON
    [09] Button 2      "关闭(&C)"        207,59  56x16
    实现路子与第 14 批的 `IDD_FIND` 完全一样（.rc 模板 + 对话框类 + `ON_COMMAND(3, ...)`；
    Ctrl+H 在编辑器 OnKeyDown 里 PostMessage(WM_COMMAND,3)）。编辑器侧只差两个方法：
    `ReplaceCurrent()`（把当前选区换成 `m_sReplaceText`）和 `ReplaceAll()`
    （`while (FindNext(TRUE)) ReplaceCurrent()` + 防死循环计数上限），`m_sReplaceText` 照 `m_sFindText` 加。
  - ★ **做完了（第 21 轮第十四批）**：原版那个**模态「寻找」对话框**（Ctrl+F / #32877）。
    规格完全照 `DIALOG id=284` 抄：`src\ec_syscfg.rc` 的 **`IDD_FIND DIALOGEX 0,0,254,76`**
    （5 个控件、控件 id 沿用原版 1151/1041/1/2、`查找下一个(&F)` 当 `IDOK`、`取消(&C)` 当 `IDCANCEL`）；
    `src\ec_syscfg.h/.cpp` 的 `CEcFindDlg` + `EcShowFind()`；
    `CECChildFrame::OnEditFindDlg()` 弹框后把文本/「区分大小写」交给编辑器再 `FindNext(TRUE)`；
    编辑器侧加了 `m_sFindText`/`m_bFindCase` + `SetFindText/GetFindText`，
    不勾「区分大小写」时走新加的 `EcFindNoCase`（只折叠 ASCII 大小写；GBK 首字节 ≥0x81 落不到 A-Z 上，对汉字安全）。
    快捷键 Ctrl+F 在编辑器 `OnKeyDown` 里 PostMessage(WM_COMMAND,32877) 给子窗口。
     子程序表  竖线 44,158,230,266,302,380    -> 列宽 114 / 72 / 36 / 36 / 78
    **标题=寻找、387x143 px**（= 254x76 对话框单位按 9pt 宋体换算，与 id=284 模板一致；同 libcfg 的 322x290→489x464），
    `GetDlgItem` 四个原版 id 全在：`1151 ComboBox 272x20` / `1041 Button |区分大小写|` /
    `1 Button |查找下一个(&F)|` / `2 Button |取消(&C)|`；**且 `tools\find_smoke.ps1` 四条仍全 PASS**。
    ⚠️ **踩的坑**：只在 `src\resource.h` 里加 `#define IDD_FIND` 时，C++ 侧编译 `ec_syscfg.h`
    仍报 `error C2065: 'IDD_FIND' : undeclared identifier`（该头文件自己只 include 了
    `#ifndef IDD_FIND / #define ... / #endif` **自带一份**（控件 id 同理），两边都定义也不冲突。
  - 加书签/到前书签/到后书签（#32964 Ctrl+F6 / #32967 Shift+F6 / #32968 F6）**做完了**
    （编辑让行号漂移时顺序仍然对），绕回到头/尾；F6 系列接在编辑器 `OnKeyDown` 的 `case VK_F6`
      `普通采样 → w=60,54,42,42`（原版 jy.e 60/54/42/42 ✓）。
    （要拍就在原版里点一行 → Ctrl+F6 → 1:1 截流程槽，见 §7 的 `probe_page2m.ps1` 那套）。
      → `python tools\diff_png.py shots\live_tabs\view0.png shots\code_view.png 0 0 1174 388`
      **跑完记得 `python tools\restore_sample.py && cmd /c build.bat`**。
    实现与 `IDD_FIND` 同一套路（`src\ec_syscfg.rc` 的 `IDD_REPLACE` + `CEcReplDlg`/`EcShowReplace`
8. **参数行不跟随光标/编辑**（`DrawParamLines` 每次绘制重新解析，改了语句文字会跟着变，这是好事）。
9. 编辑器窗口类名仍不同（本工程 `Afx:400000:8:10005:6:0`，原版两层 `…8:10003:900015:0` + `…b:…`）。
10. 「系统配置」6 个页签只实现了「显示」页。★ **第 21 轮第九批把原版模板查清了**
    （新工具：`python tools\dialog_inv.py shots\e_dialogs.txt 282`）：
      **`Button:44 / Static:18 / Edit:12 / ComboBox:3 / SysTabControl32:1`**
    afxwin/afxtempl/ec_theme/ec_lib，靠调用方先含 resource.h；而 `.rc` 那边走
    `assemble_rc.py` 内联 resource.h 的路，本来就能看到）。**正解**：在 `ec_syscfg.h` 里用
    ★ **第 21 轮第八批把原版的「寻找」对话框模板量出来了**（`shots\e_dialogs.txt` 里
    **`DIALOG id=284` 254x76 DLG，5 个控件**，照抄进 `src\*.rc` 就能 1:1）：
    [00] id=65535 Static    7,10   53x8    text='查找寻找文本：'      (WS_CHILD|WS_VISIBLE)
    [01] id=1151  ComboBox 66,7   181x135  style=0x50210042 CBS_DROPDOWN|CBS_DROPDOWNLIST
    [02] id=1041  Button    7,30   62x8    text='区分大小写'  BS_AUTOCHECKBOX
    风格同其它原版对话框：0x80C808C0 [WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_SETFONT|DS_CENTER]
    原版到底是用这个模态对话框还是状态夹里的搜索栏，**先拍一张原版按 Ctrl+F 之后的截图再定**。
    里面是**分组框**（`WS_GROUPBOX`，第一组标题 `Windows 窗口程序的运行方式`，还有
    "系统自动保存编辑文件的时间（分钟），为 0 则禁止"、"支持程序运行时最大内存尺寸（千字节）"
  - **还没做的**：加书签/到前书签/到后书签/书签跳转（#32964/32967/32968/32965）、
    置为只读（#32862）、导入其它程序（#32874）、加到当前块首尾（#32800-32806）、
    照 §2.9 的路子加就行（改 `m_lines` → `AfterEdit()`）。
- **② 系统配置的另外 5 个页签**（原版模板 `DIALOG id=282`，`tools\dump_dialogs.py` 能倒；
    （Ctrl 决定加/取消，Shift 决定方向）。**验收 26 = `tools\bookmark_smoke.ps1` 五条 PASS**（已接进
- **④ 窗口设计器的"新建窗体/切换窗体"**（见下面"窗口设计器剩下的活"）。
  `CECEditor::ToggleBreakpoint/HasBreakpoint/ClearBreakpoints` + `m_breakpoints`（按源码行号存，同书签），
  接线 `ON_COMMAND(32855, OnDebugToggleBp)` / `ON_COMMAND(32858, OnDebugClearBp)`，
    标题 `书签跳转`、**618x425 px**（= 408x264 对话框单位）、5 个原版控件 id 全在、
    - 判据（一条命令）：`python tools\dump_manifest.py "C:\Program Files (x86)\e\e.exe" bin\EClone.exe`
      → 两边都是 **`NO RT_MANIFEST (type 24)`**。而带 v6 样式的程序（拿 `notepad.exe` 验过）
      一定有 `RT_MANIFEST name=1`，内容是 `<assemblyIdentity ... comctl32 version="6.0.0.0">`。
    - **正解**：`InitCommonControls()`（`EClone.cpp:2544`）+ 不加清单 = 与原版同样的
      原版实拍证据：`shots\live_progtree\work.png`（工作夹的 支持库/程序/属性 三个页签是经典样式）。
  **一行 `src` 可能对应多个显示行**（表格行 + 代码行），所以断言要 **`Sort-Object -Unique` 数不同的源码行**，
  → 轮询直到 `ENewFrame` 标题含 `.e`（状态 C）→ 找最大的 `Afx:400000:b` 并**断言宽度 > 800**
  `keybd_event(VK_F9=0x78)` → `PrintWindow` 拍 1:1：`shots\live_bp\{before,clicked,after_f9,after_f9_again}.png`。
  | A 未载入 | `ENewFrame` 标题只有 `易语言-加密狗版 - Windows窗口程序`（**没有文件名**）；最大的 `Afx:400000:b` 是左边 210x636 的程序树 | ✗（会拍成树） |
  | B 窗口都没建 | **整个进程只有一堆隐藏的 `tooltips_class32`**，24 秒都没有 `ENewFrame` | ✗（没东西可拍） |
  | C 正常 | 标题 = `流程控制类命令演示 - C:\...\流程类命令.e [无编译条件] - Windows窗口程序 - [程序集: 启动窗口程序集 / _启动窗口]`；最大的 `Afx:400000:b` = **代码编辑器 1707x743** | ✓ |
  ⇒ **判据（写进探针里了）**：必须 `ENewFrame` 存在 **且标题含 `.e`**，否则重开；`probe_bp2.ps1` 现在会明确报
  `FAIL: the sample never loaded (title has no .e)` / `FAIL: the code editor is not the biggest view`，
  ④ 一旦拿到状态 C，`probe_bp2.ps1` 后面的流程（找编辑器→点一行→F9→拍三张）**已经证明能跑通**
  （文件状态：`tools\probe_bp.ps1` 是旧版（会拍错窗口，本轮已弃用）；**用 `tools\probe_bp2.ps1`**。）
     `python tools\tree_index.py <trees.txt> 程序数据 启动窗口程序集` 算；**不能用 PostMessage，原版不认**，见 §3.8-1）。
     `treeinfo.exe <pid>` 的 dump 里**只有 root=系统核心支持库 那一棵树**，程序树根本不存在。
     必须先点工作夹的第 2 个页签（`SysTabControl32` 里 x≈100 那一下，`page_smoke.ps1` 就是这么干的），
  3. 量标记的判据：把 `clicked.png` 与 `after_f9.png` **逐像素 diff**，变化像素的 bbox + 颜色就是标记；
    **验收（实测，可复现）**：发 `WM_COMMAND 32877` → 弹出 `#32770` 对话框
    `10 23 14 24 11 1 1 … 1 30 1 1 1 5 6`，减 9 后正好是 1..24 的一个排列）。
    **(c) 条带本身**：`tools\extract_icons.py:crop_tree` 切出来的 `src\res\tree_icons.bmp`
    ⇒ 用 **`tools\build_icons_identity.py`** 重建：它按"**显示位置 p → 第 p 格**"逐行从
    `shots\live2\frame.png` 抠（原版第 p 个类别在第 `y = 167 + 14*(p-1)` 行，x=36），
    python tools\build_icons_identity.py     # 重建条带（31 格：0=库根，1..24=各显示位置）
    python tools\show_tree_icons.py          # 渲染成一张带格号的图，肉眼核对
    `reextract_all_icons.py`（全被 `build_icons_identity.py` 取代，已删除）。
    逐行对图标。`shots\cmp_v6.png` = **修好之后的样子**（`cmp_v5.png` 是中间态）。
    ⚠️ 诊断小技巧：往条带某几格**涂纯色方块**再跑（见 git 历史里的 `paint_icons_test.py`），
  —— 本工程按"框住整段、上下各一条"实现（`DrawCommentBoxes` 里 bHasBlank 的两个分支）；
    [03] id=1     Button  105,53   64x16   BS_DEFPUSHBUTTON
    可以直接在 `frame.png` 里找**形状相同**的那几格填进去（`tools\strip_cells.py` 逐格打
  Ctrl+K/M → 截图看行首，或者用 §7 的 `treeinfo` 那套思路读源码文本）。
- **子程序数据行那个 `-/+` 到底折叠什么**：本工程折叠整个子程序体；原版点它会怎样**没实测**。
    ⚠️ 顺手发现一条**要核实的事**：原版 `DIALOG id=282`（系统配置）是 **267x217、78 个控件**，
    - 实测原版几何：`shots\live_des_orig\windows.txt` 里 `WTWindow` 在屏幕 `(275,189)`，
      它的设计器视图在 `(265,179)` ⇒ 窗体固定在视图客户区 **(10,10)**（本工程原来写的是
      (30,30)，差 20px）。常量：`EC_DESIGN_FORM_X/Y`（`src\ec_design.h`）。
    到最近修改处（#32954）、整体搜寻（#32934）、高亮标记待寻找单词（#33006）、
      `CECDesigner::HitHandle()` 第一行是 `if (m_sel < 0) return FALSE;`，
  「全选」会勾上目录里 **71 个 .fne 中的全部**，其中 `spec.fne` / `shellEx.fne` 已知会把进程打成
      客户区 == 窗体客户区；可手柄中心落在"窗体矩形**外扩 3px**"的框线上，也就是窗体客户区
      **之外** —— 画了也被客户区裁掉。现在覆盖层挂成**设计器视图的子窗口**、并比窗体矩形
  本质都是"往源码里插一行合法声明"，然后 `Rebuild()`；`SetCellText` 已经在改源码，路子是通的。
        * 覆盖层坐标 = 设计器视图坐标；窗体客户区坐标要减 `m_formOff`（`SyncOverlay()` 里算）。
        * 所有"画/命中判定"用 **`UnitRectInOverlay()`**（`UnitRect()` + `OffsetRect(m_formOff)`）。
  启动时没有断点 → F9(#32855) 在插入点那行设了一个(src=3)
    - ⚠️ `tools\resize_smoke.ps1` 里手柄坐标原来用 `ScreenToClient(form, ...)` 算，覆盖层搬家后
    - `LIB_DATA_TYPE_INFO` 的 **+20 是 `m_nBmpID`**（`src\ec_lib.cpp` 的 `RAW_DT::bmpId`），
  颜色点阵       python tools\px_map.py <png> <x0> <x1> <y0> <y1>
      set ECLONE_DUMP_DTICON=D:\eclone\shots\dticon.txt   & bin\EClone.exe   # 倒出每个组件的 bmpId
      python tools\pe_bmp_ids.py "C:\Program Files (x86)\e\lib\krnln.fne"   # 列出 .fne 里的 RT_BITMAP
  不能数 `bp=1` 的行数；以及 **`.fold.txt` 是快照**——读 `caret=` 之前必须先 `EC_WM_REDUMP`，否则拿到旧值。
      （`EcLibModule()` 是本轮新加的访问器；`e.exe` 侧同样调 `LoadBitmapA(*((HINSTANCE*)ModuleState+3), 0x26C)`
    - 验证：`python tools\shot_compbox.py shots\compbox2.png`（会自己发 #110 命令打开组件箱）
                   powershell -File tools\probe_page2m.ps1 -File shots\e_samples\jy.e -Node 全局变量 -Root 程序数据 -OutDir shots\live_page_gvar
                   ! 旧的 probe_orig_page.ps1 用 PostMessage 点树，原版不认（3.8-1），别再用它下结论
                   python tools\tree_index.py <trees.txt> <根名> <节点名>  # (树号, 前序项号)，喂给 treeinfo
                   python tools\tree_pick.py <trees.txt> <根名>           # 根名 -> 树 hwnd
   原版点树节点   D:\eclone\tools\treeinfo.exe <pid> [sel|click|dclick|mclick|mdclick] <树号> <项号>
   表格页量尺寸   python tools\measure_page.py <png> [xmax]     # 网格线位置 + 每行底色 + 该行主色
                   python tools\measure_cells.py <png> [xmax]   # 每格主色（本轮所有配色都是这么量的）
   绿框断言       python tools\check_comment_box.py <png> [行距]  # 原版基准 7/7、本工程渲染 7/7
    验证：`python tools\probe_compboxview.py`（设计器=visible / 程序集=hidden / 切回=visible）。
                   ⚠️ 撤消会把插入点恢复到快照里那份；撤消之后再测行局部命令要**重新点那一行**
   窗口菜单探针   powershell -File tools\winmenu_probe.ps1     （发 WM_COMMAND 后打印子窗口矩形）
       **标题栏**（蓝色渐变 + 最小化/最大化/关闭）是画出来的（`shots\orig_designer_activated.png`）。
   IDA MCP 直连   python tools\idamcp.py <方法名> "[参数]"     <- 参数写 Python 字面量
                   ! 连的是已经打开的 IDA GUI（ida-pro-mcp 插件，端口 13337，本机开的是 e.exe）。
                      PowerShell 会吃掉 JSON 的双引号 —— 参数用单引号写、按 Python 字面量解析。
       （实测轨迹：`form=(389,222 380x250) client0=(392,248) ovl_win=(765,454 …)`）。
  构建守卫       python tools\check_block_comments.py gbk
                 （脱离 GUI 验证"支持库能不能建真组件窗口"；退出码 0 = 全过）
    4. 诊断开关：`ECLONE_TRACE_DES=1` + `ECLONE_DUMP_MODEL=<path>` 会把 `SyncOverlay()` 的
  #32855 断点 F9 / #32945 条件断点 / #32858 清除所有断点 / #32870 到现行执行位置。
        WTWindow              (738,305  380x250)   <- 基准窗口（不能拖动，只能 8 个手柄改尺寸）
                  （PostMessage WM_COMMAND 32906 → 拍对话框 + 倒控件表）
          _EL_RgnButton       (877,379  104x112)   <- 放上去的"图形按钮"，选中框是红的
  窗口过程归属   powershell -File tools\probe_wndproc.ps1 -Class WTWindow
    ⚠️ 激活原版设计器页的可用办法：`SendMessage(MDIClient, WM_MDIACTIVATE, 拥有 WTWindow 的那个
                 ⚠️ 跑过 tools\theme_e2e.ps1 之后必须把配色恢复成默认，否则 check_dll 颜色断言假失败：
3. ⚠️ **`::CreateBitmap(w, h, nPlanes, nBitsPerPixel, bits)`：1 个位面时 bpp 只能是 1**。
   正解：掩码自己打包成 1bpp、**行按 WORD 对齐**（`stride = ((cx+15)/16)*2`，1 = 透明）。
   调试办法：`tools\diag_compbox.py` 会打印按钮样式 + `EcTrace` 的 drawitem 行数。
本轮定死的原版数字（改代码时照抄，别猜）
                 块结束后回到外层（详见 2.1e，三个原版样本互证）
> ① **工具条/面板开关 8 条**（#59392/#59393/#133/#135/#129/#130/#110/#132）——
>    原版这 8 个 id 就是 MFC 的 control bar id；`CMDIFrameWnd` **不带** `OnBarCheck`，要自己接
> ② **「窗口」菜单 6 条表页命令**（#32817/32818/32819/32825/32826/32827）切文档页；
> ③ **「编辑→整体搜寻」Ctrl+Alt+F（#32934）** 照原版 `RT_DIALOG id=30773`「整体搜寻对话框」1:1，
>    结果真的写进状态夹的「搜寻1/搜寻2」页（**那两页是输出夹，不是搜索栏** —— 推翻了上一轮的推断）；
> ④ **系统配置另外 6 个页签真的读/写/持久化**（`EcCfg` + `ECLONE_DUMP_CFG` 倒档 + `cfg_smoke.ps1`）；
> ⑤ **设计器「新建窗体 / 切换窗体」**（窗体表 + `插入→窗口` 真建 WTWindow + 程序树节点切窗体）；
  折叠标记       横杠 x=22..30（9px）、竖杠正中 x=26、颜色 #000080、画在白底上
> 1. **运行/调试**（用户列表第 ⑤ 项，最难）：断点**状态**已做完（验收 28），缺 ① **流程槽里的断点
>    标记**（原版会画；`tools\probe_bp2.ps1` 已就位，⚠️ 拍之前先看 §5 下面那段"原版 3 种启动状态"）
>    ② 执行引擎（F5/F8/单步）。**动手前先跟用户确认**要做"解释执行选中的子程序"还是"调外部 exe"。
  设计器点阵     1px 纯黑，间距 8px，锚在**窗体客户区原点**（client (8i,8j) 全有点，(0,0) 也有）
> 3. **剩下的 86 条菜单**（`python tools\menu_coverage.py`）：文件 新建/打开/保存/另存为（要 `.e`
>    格式，风险高）、编译那 17 条（要有编译器）、数据库那 6 条、向导/帮助那 8 条、打印、置为只读(#32862)、
>    高亮标记待寻找单词(#33006)、到最近修改处(#32954)、导入其它程序(#32874)。
                 65535 底部提示 (7,242,308,19)、
                 按钮 6 购买授权(7,265,49,18) / 3 全选(60,265,45,18) / 4 全不选(109,265,45,18) /
   40 字节、**头 8 字节是 Name**，所以 `VirtualSize/VirtualAddress/SizeOfRawData/PointerToRawData`
  Dll 页冒烟     powershell -File tools\dll_smoke.ps1（点程序树里 Dll命令 那一行）
                 代码行尾部 par=N = 该语句下面画几行参数归属线，0 = 不画；
   python tools\ctrl_a_probe.py --out shots\live_ctrla2      # 出 before/after，并打印反白带
**状态**：能编译、能跑；**验收 1~33 全过**（第 1 节的表），**28 步回归**
（最近一次 `shots\regress_r35.log`：`[13]` Dll 页 **0 像素差**、`[14]` 代码页 **1404 px / 0.31%**、
`[27]` ui_smoke 33 PASS、`[28]` cfg_smoke 20 PASS；⚠️ 第 10 步 `design_smoke` 与第 15 步 `libcfg_smoke`
   powershell -File tools\sel_shot.ps1 -Tag selX
用 `Move-Item bin\EClone.exe bin\EClone_tmp.exe` 绕开之后这两步也是绿的）。
   python tools\diff_sel.py shots\live_ctrla2\after_ctrla.png shots\selX_sel.png
（第 21 轮第六批从 0.62% 降下来的，见 §5 第 5 条）；
**全局变量表与原版逐像素 0.04%（44 px，全是插入点铅笔标记）**；
   `y 5..24 / 26..44 / 55..74 / 76..94 / 101..112`，最大宽度 `230 / 230 / 331 / 331 / 178`；
   带内"字"的像素是 **#FFFFFF**（在 #000080 上写的白字），纯 navy 约占 17%。
   **对拍才发现差 10.69%**；改成整格涂满后差异降到 **0.83%**。
**设计器背景（#A0A0A0 画布 + #F0F0F0 纸 + 8px 点阵 + 白框手柄）与原版逐点对齐**；
**支持库配置能运行期动态载入/卸载 .fne**；**组件能拖 8 个手柄缩放**；
   `python tools\patch_sample_graphic.py` → `build.bat` → 拍 → `python tools\restore_sample.py` → `build.bat`。
   内层命令画不画参数行；② 展开态下**最后一条语句**的参数行会伸出内容区，原版怎么处理未知。
4. ~~「全局变量 / 自定义数据类型 / 类模块 / 常量表 / 资源表 / 模块引用表 / 外部文件记录表」
   还没有文档页~~ **第 21 轮做完了 5 张表页**（见 §2.8b，验收 21/22）。**这一类还剩**：
   ① 类模块页（jy.e 里没有类，没拍到基准）；② 「资源表」父节点本身不开页（已对齐原版）；
   ③ 模块引用表 / 外部文件记录表（原版样本里空的、点了不开页 —— 有内容时开不开**没测**）；
   ④ 图片/声音资源表的「内 容」列（原版画的是**二进制里那份资源的字节数**，文本形式里没有这个信息，
   本工程这一列**空着**）。
5. **代码页那 0.31%（原来 0.62%）里的「表格列宽」——第 21 轮第四~六批做完了**
   （⚠️ 第三批那次的数字是**量错了**：我按 y 1..25 / 25..45 取"子程序表"的竖线，那两条其实
   都在程序集表里 —— 正确的子程序表在 y 54..75。**以本条为准**）：
   ```
   原版 graphic.e (shots\live_tabs\view0.png, 1174x388)  横线 y=4/25/45/54/75/95
     程序集表  竖线 44,140,194,236,278        -> 列宽 96 / 54 / 42 / 42
   原版 jy.e     (shots\live_fold_jy\view_before.png)    横线同上
     子程序表  竖线 44,122,194,230,266,482    -> 列宽 78 / 72 / 36 / 36 / 216
   本工程 (ECLONE_DUMP_MODEL 的 TABLE 行, graphic.e 采样)
     TABLE 0 cols=4 w=96,42,42,42      程序集表
     TABLE 1 cols=6 w=114,72,36,36,24,24   子程序表(备注占 2 列 -> 48)
   **逐列对上之后只剩两处差**（其余列**完全一致**，包括子程序名 114、返回值类型 72、公开 36、易包 36）：
   1. ~~**程序集表第 2 列：原版 54 / 本工程 42**~~ → **第 21 轮第五批改完并验证了**：
      54 = 24 + 18 + 12），改完两个样本都**和原版逐列相等**：
      `graphic.e → TABLE 0 w=96,54,42,42`（原版 96/54/42/42 ✓）、
      （实测三个表头的 ink 宽度都是 29px，空格不产生 ink 只产生推进，所以只能靠**列宽**反推空格数）。
      （asm 表 345px + sub 表 748px），实测比预估还多消掉一点。
      **第 21 轮第七批跑完整回归确认没连带坏掉任何东西**（`shots\regress_r23.log`：
      剩下的 1404 px 以字形差异为主（最大一块是 `A=#FFFFFF B=#000000`，即某个字的笔画），
      再往下就是逐字形抠的活了（收益低，见 §6 的"字体"那条）。
   量法（下次照做，别再猜）：
   :: 本工程的宽度：设 ECLONE_DUMP_MODEL 启动一次，看 model 里的 TABLE 行
    - `DIALOG id=282` 267x217、**78 个控件**，类直方图 =
      ⇒ **确实是一个页签对话框**（本工程按页签做是对的方向 ✓），但形态是
    正解：有选区时**正向用选区末尾、反向用选区开头**当搜索原点（`nOrg`）。
    `regress.bat` 第 22 步）。
    标记本身不会影响现有像素对拍（有书签才有标记，启动时没有）。
    `删除(&R) id=3` / `全部删除(&L) id=4` / `跳转(&G) id=1`(默认) / `关闭(&C) id=2`，
    四个按钮文字逐字等于原版，**而且有两条功能性断言**：加一个书签后列表
  （把 `static const TCHAR* g_szSampleCode =` 粘进上一行注释）。修复过程中采样被重写过，
  现在这一份是 66 行 / 62 条 `_T()` 字面量（`m_stageSize`、`计次循环首`、`信息框` 那些
    原版到底几列、多宽**没量**（已记 §6）。
- **「插入」菜单的插入位置**（§2.10c）同样是**推断**：原版是"插入到当前位置后面"，本工程按
- **页签切换只能用真鼠标**（§3.8-16）：`TCM_SETCURSEL` 不发 `TCN_SELCHANGE`，手工构造远程
    [07] Button 3      "替换(&R)"         78,59  56x16
    `FindNext` 的取词顺序变成 **对话框文本 → 选区 → 光标处的词**，
    与标签 `欲搜寻文本：` **逐字等于原版 dump**。
    只有注释/提示串受影响）。
    [08] Button 4      "全部替换(&A)"    141,59  56x16
    （items=5，还有 id=2 —— 应该是"取消"）
    另外原版状态夹里有「搜寻1/搜寻2」两页（本工程状态夹的页签位置已经有了）——
    这类静态文字）—— **不像是"6 个页签"**。本工程现在按 6 个页签做的（只实现了「显示」页），
    下一轮**先核实原版是页签还是分组框**再往下做，别继续按页签堆。
    转换为（#32807-32813）。**「加到当前块首尾/转换为」这两组是纯源码变换**，
  `src\ec_syscfg.rc` 里已经有一套坐标，本轮没动）。
- **③ 插入菜单**（子程序/程序集/窗口/全局变量/数据类型/Dll命令/常量/参数/局部变量）：
- **⑤ 运行/调试**：用户 2026-10 明确开口了「如果全部都搞定你甚至可以尝试搞定调试」。
  ★ **第 21 轮第二十四批：调试第一步做完了（断点的"状态"）**
  键盘走编辑器 `OnKeyDown` 的 `case VK_F9`。
  **验收 28 = `tools\bp_smoke.ps1` 七条 PASS**（已接进 `regress.bat` 第 24 步）：
  ```
                 → 再按 F9 取消 → 键盘 VK_F9 也生效(src=5，跟着插入点走)
                 → 能同时存在多个 → 清除所有断点(#32858) 全清
  ⚠️ **怎么验的**：给 `ECLONE_DUMP_MODEL` 的行倒档加了一个 **`bp=%d`** 字段
  （`row ... src=3 bp=1 | ...`）——这样"断点状态"不用截图就能断言，**也就不用先猜原版的图形**。
  ⚠️ **还缺什么（都写进 §6 了）**：
  ① **流程槽里的断点标记**（原版会画一个）—— 没基准，**本工程暂时不画**（有状态、无图形）；
  ② 运行(F5)/终止/单步/执行到光标这些**要执行引擎**，还没做；
  ③ 条件断点(#32945)、到现行执行位置(#32870)同理。
  ⚠️ 写冒烟时踩的两个坑（都是我的错，不是功能的错，记下来省时间）：
  ★ **（拍原版用的探针）`tools\probe_bp2.ps1`**（旧版 `probe_bp.ps1` 已删）：原版开样本（先等旧实例真的退出）
  （否则就是拍到了 210x636 的程序树，直接 FAIL 而不是接着拍）→ 真鼠标点一行 →
  **目标是量出原版断点标记的颜色/几何**（F9 命令 id = #32855）。
  ⚠️⚠️⚠️ **第 21 批把"开不出窗口"这件事查清到了根：原版有 3 种启动状态，只有第 3 种能探**
  （实测：连续 `taskkill /f /im e.exe` 反复重开后越来越容易进坏状态）：
  | 状态 | 现象 | 能不能探 |
  |---|---|---|
  **不会再把程序树当编辑器拍下来**。
  ⇒ **下一轮的最省事做法**（状态 B 时别硬刚）：① **别再连着 `taskkill /f`**（疑似加密狗版被强杀后卡住，
  A/B 两个坏状态都是在反复强杀之后出现的）；② 先**不带文件**启动、等出 `ENewFrame`，
  再用 IDE 自己的"打开"或直接把 `.e` 拖进去；③ 或者隔几秒重开最多 3 次，每次等满 30 秒；
  （内联版实测找到过 1707x743 的编辑器）。
  **量标记的判据**（没变）：`clicked.png` vs `after_f9.png` 逐像素 diff，变化像素的 bbox + 颜色就是标记；
  `after_f9_again.png` 确认"再按一次取消"。
  ⚠️⚠️ **现在它拍错窗口了**（拍到 210x636 的**程序树**，不是代码编辑器），原因有两条，
  都是本轮实测出来的、下一轮照着补两段就行：
  1. **窗口程序样本打开时停在"设计器/起始页"**，代码编辑窗口还不存在 ⇒ 必须先点开代码页。
     点法：真鼠标点程序树里的「启动窗口程序集」节点 —— 用自己那套
     `treeinfo.exe <pid> mdclick <树号> <项号>`（树号/项号由
  2. **「程序」树窗口是懒创建的**（新坑，已记 §3.8-12）：工作夹停在"支持库"页时，
     再 dump/点树。
     `after_f9_again.png` 用来确认"再按一次取消"（标记消失）。
  原版菜单里调试相关的命令 id：#32854 运行 F5 / #32860 终止 / #32940 查看表达式变量 /
  #32853 单步 F8 / #32852 单步进入 Shift+F8 / #32867 跟踪返回 / #32871 执行到光标 /
  **先做数据源**：编译产物（`.e` → exe）要不要真做没定；最低成本的"调试"是
  「解释执行选中的子程序」或「接一个外部调试器（让 e.exe 自己编出来的 exe 跑起来）」，
  动手前先跟用户确认要做哪一种（见 §5 上面那段"用户 2026-10 追加指示"）。
- **窗口设计器剩下的活**：
     还能加的：多选、对齐工具条、方向键微调。
     但"新建窗体/切换窗体"还没有——原版是"程序树→窗口→_启动窗口"。
  4. ~~放置失败时的提示~~ **已完成**（状态栏 + 一声响，见 §2.7c）。
  6. **设计期"网格对象"那条链没走通**（见 §2.7a 的 krnln 线索）：现在纸 + 点阵是**覆盖层代画**的。
  用 `tools\probe_sel.ps1`（它会点进编辑器并把整窗拍下来）看「块体第一条是简单语句」和
                 不画：如果结束/如果真结束/判断结束/否则/默认
  表格           程序集表头整行 #ECECEC；子程序表头 #E6EDE4；数据行白底
- 想查原版某个函数：`python tools\ida_font.py "0x地址1,0x地址2" out.txt`；
- **想倒原版某个对话框的模板**（支持库配置 / 系统配置都是这么来的）：
  → 找 `DIALOG id=<n>`，坐标/控件 id/风格照抄进 `src\*.rc`。
  `.ps1` 一定要跑一遍非 ASCII 计数（第 3.3-2 条）。
  "必须存在"的采样都在）。
  **判据**：回归第 13 步 Dll 页仍是 **0 px**、第 14 步代码页仍是 **1404 px (0.31%)** ——
- **「加到当前块首尾 / 转换为 ->」的落点与缩进量**（§2.10a）是**推断**的：原版这两个命令的
  "参数/局部变量进当前子程序的对应段末尾、其余进文件末尾"。#32783「现行单元」是 no-op。
- **「系统配置」另外 6 页的控件只有外观、没有行为**（§2.10b）：只有「显示」页那条链
  （语句行间距/字体/配色/7 个绘制开关）真的接在主题上（`WriteCtrls`/`ReadCtrls`）。
  其余页的复选框/编辑框点了不存任何东西，也不影响编译/运行（本工程没有编译/运行）。
  ⇒ **第 23 轮已修正**（§2.11d）：34 个控件**全部真的读写 + 持久化**，但"有行为"的只有 3 项
  （设计器网点 / 新建窗体尺寸 / 编辑器的撤消栈上限）—— 因为原版其余那些是**给编译器和生成程序**用的
  （备份文件、作者信息、调试输出名、清单、编译开关、花指令/打乱码），本工程没有编译器。
  **这不算没做完，是本工程没有对应子系统**（见 §2.11d 的分类）。
- **「整体搜寻」两个下拉的条目文字没抓到基准**（§2.11c）：原版模板里那两个 ComboBox 是空的，
  条目运行期才填。本工程自己填了 `整个程序/当前程序集/当前子程序/当前窗口` 与
  `搜寻1/搜寻2`（后者是模板里两条单选的原文改的）。**要精确复刻得拍原版打开这个框时的下拉**。
  另外「搜寻范围」目前**只记值没接行为**（搜索总是全篇扫）。
- **系统配置「设计」页的 `属性表中是否列出设计时不可用或只读性质的属性`(#1147)**：值存了，
  但属性表还没按它过滤（`FillProps` 现在把支持库返回的属性全列出来）。
- **「新建窗体 / 切换窗体」现在共用 `ECDOC_FORM` 一个文档页**（§2.11e）：原版是**一个窗体一个页**
  （程序树里每个窗口节点是一个独立页）。本工程点不同窗口节点只切"当前设计窗体"，标签条上仍是一个页。
  要 1:1 得把 `ECDOC_*` 从固定 8 个改成"动态文档表"。
- **窗体标题是中文，跨进程读回来是乱码**：本工程是非 Unicode 构建（VC6 默认 char），
  `SetWindowText` 写进去的是 GBK 字节，64 位 PowerShell 用 `GetWindowTextW` 读会被按 UTF-16 解 → 乱码。
  所以 `ui_smoke.ps1` **不按标题**断言窗体，只断言"WTWindow 个数 + 可见性"。
  想按标题断言得让测试进程用 `GetWindowTextA` + `Encoding.GetEncoding(936)`。
  `WM_NOTIFY` 也送不到处理函数。想要"程序化换页"得另找路子（比如加一个自定义消息）。
  ⇒ `CPaneBar::GotoPage()`（第 23 轮新增）就是给"整体搜寻要把状态夹翻到搜寻页"用的：
  `SetCurSel(n)` + 自己 `ShowPage(n)` 两步走，不依赖通知。
    → 第一条上有一个（y=322..326，x=33/37）；
  * 循环块因为「首+体+尾」天然 3 行，永远满足这条。
  （自动化测试是直接 `PostMessage` 到编辑器窗口的，绕过了焦点路由）。
  Windows 会把 `WM_MOUSEWHEEL` 发给光标下的窗口，理论上没问题。
  本工程「数值前景」槽是 `#008000`。第 21 轮又拿到两处独立证据（常量数据表的数值常量、
  图片资源表的「内容」字节数都是 `#800000`）—— 三处都是 #800000，下一轮可以动手改，
  但**先看清 `check_theme.py` 是从 `.clr` 文件读还是从内置默认读**（见 §5 第 11 条）。
- **多行注释绿框的判据**（第 5 节第 2 条）：**只拍到一个样本**（jy.e 的 11 行、
  空行/有字行交替）。已经定死的：颜色 #008000、左竖线 x=正文列+17 且 2px 宽、
  每行一段 12px 的竖虚线、横线 2px 高 11 亮 1 灭、**横线只画在空注释行的行顶+5**、
  单行注释不画。**没定死的**：① 连续多行**都带文字**（中间没有空 `'` 行）时怎么画
  ② 右竖线：原版实测**只在空行那几段有**，本工程按整框画满（差 2px 的断续）。
  想定死就再拍一个样本（原版打开 `c:\Program Files (x86)\e\samples\经典数学算法集\*.e`
  里带连续注释的，或者用真鼠标 + 键盘在原版编辑器里**自己敲**两行 `'aa`/`'bb` 出来再截屏）。
- **「解除屏蔽」删不删那个空格没对拍**：本工程去掉 `'` 之后如果紧跟一个空格也一起删
  （因为原版样本里的注释都是 `' 文字`）。原版到底删不删，得在原版里 Ctrl+K 再 Ctrl+M
  看那一行的实际文本（拍法：`probe_page2m.ps1` 进编辑器 → 真鼠标点一行 → keybd_event 发
- 字体：`'“'` 4px vs 2px 那类差异无法消除。
  将来能打开 `.e`（有真窗体数据）或逆出 ITF 4 的属性映射后，这里要改成读底色属性。
  没在调试器里跟过。现象是确定的（本工程窗体不画点阵），原因是推断。
  `0xC0000005`（坑 3.5-2）—— 所以**别在自动化测试里点「全选」**。
---
```
逐像素对照
   原版某文档页   * 用这个（真鼠标，第 21 轮验证过）：
                   （自动切工作夹到"程序"页 → 真鼠标点节点 → 拍整框 frame_*.png + 文档客户区
                     view_*.png + 倒窗口表 windows.txt + 节点日志 node_*.txt，并打印标题栏 BEFORE/AFTER）
   原版树节点定位 python tools\tree_row.py <trees.txt> <节点名>          # 可见行号（老工具）
                   mdclick = 真鼠标（拍原版一律用这个）；click/dclick = PostMessage（本工程自己认）
   表页冒烟       powershell -File tools\page_smoke.ps1        （三条断言，见验收 21）
   编辑命令冒烟   powershell -File tools\edit_cmd_smoke.ps1     （七条断言，见验收 24）
   菜单覆盖率     python tools\menu_coverage.py                （137 条里哪些还没处理函数）
                      krnln.fne 不在那个 idb 里，查 krnln 还是用 tools\ida_krnln.py。
  一条龙回归     cmd /c "D:\eclone\tools\regress.bat"        （**26 步**；第 25 步系统配置 7 页、
                  第 26 步插入菜单是第 22 轮加的）
  ⚠️ 跑之前先看文件顶部那个"僵尸进程"提醒：有两个杀不掉的 EClone 会让第 10/15 步假失败
   sampleblock.py   找 `g_szSampleCode` 块的 (起, 止) 偏移 —— **采样源码有两个陷阱**：
                    ① 块内的 `.常量 常量_文本, "短文本"` / `.DLL命令 … ""` 让朴素的
                    ② 标记串 `g_szSampleCode` **也出现在 `EP_CLASS` 的注释里**。
                    所以它要求"标记所在行**整行**就是那条声明且以 `=` 结尾"，再从 `=` 起
   kill_EClone.ps1  循环强杀 EClone（对杀不掉的僵尸无效，但能清干净正常实例）
   probe_syscfg.ps1 起原版 + 打开「系统配置」→ 拍整框（**页签文字就是这么读出来的**：放大 4 倍看图；
   syscfg_smoke.ps1 验收 29：7 个页签 + 真鼠标换页 + 控件可见性（回归第 25 步）
   syscfg_pages_shot.ps1 把本工程对话框的 7 页各拍一张（shots\syscfg\mine_page0..6.png）
   dlg_list.py      列某个原版对话框的全部控件（id/类/坐标/文字）：`python tools\dlg_list.py 282`
  类背景刷       powershell -File tools\probe_classbrush.ps1
   ⚠️ 三个坑      ① 地址**一定要加引号**，PowerShell 会把裸写的 0x100619e0 转成十进制再传进去
  shots 里最该看的几张
  一条龙回归     cmd /c "D:\eclone\tools\regress.bat"        （**30 步**；第 25 步系统配置 7 页、第 26 步插入菜单是第 22 轮加的、
                 第 27/28 步是第 23 轮加的、**第 29 步提示页 / 第 30 步全选整页是第 26 轮加的**；
                 最近一次整轮 shots\regress_r36.log：FAIL flag = 0、[13] Dll 页 0 px、[14] 代码页 1404 px / 0.31%。此处原本写作：
  关键字可见性   画：如果/如果真/判断开始/计次循环首/判断循环首/循环判断首/变量循环首
                     ＋计次循环尾/判断循环尾/循环判断尾/变量循环尾
  选区           #000080 底 + 白字，高 12px（= 文字高，不是整行）
  第 20 轮定死的原版数字（改代码时照抄，别猜）
  设计器选中框   离窗口矩形外扩 3px 的 1px PS_DOT 黑线；
                 8 个手柄 = 7x7 **白框**（1px 白边 + NULL_BRUSH 内部透空），
                 中心 = 外扩 3px 矩形的四角与四边中点
  程序树         图标号/顺序见 2.2
  第 23 轮定死的原版数字 / 坐标（改代码时照抄，别猜）
  整体搜寻框     原版 RT_DIALOG **id=30773**「整体搜寻对话框」254x163 DLG（实测 **387x274 px**、12 控件）：
                 65535 静态"欲搜寻文本：" (7,10,53,8)、1151 下拉 (66,7,181,135) CBS_DROPDOWN、
                 65535 静态"搜寻范围：" (7,27,53,8)、1152 下拉 (66,24,181,135) CBS_DROPDOWNLIST、
                 1041 复选"区分大小写" (7,47,62,8)、1042 复选"搜寻前自动展开所有被收缩子程序" (83,47,139,8)、
                 65535 分组框"搜寻结果列出位置" (7,66,240,51)、
                 1101 单选"“搜寻1”系统输出夹" (28,83,159,8)、1102 单选"“搜寻2”系统输出夹" (28,100,159,8)、
                 1 默认钮"开始搜寻(&F)" (116,141,64,16)、2 "关闭(&C)" (192,141,55,16)、
                 1047 静态提示行 (7,123,240,8)
                 核对命令：`python tools\dlg_list.py 30773`
  工具条/面板 id  标准工具条 59392 / 状态条 59393 / 对齐条 133 / 定位条 135 /
                  工作夹 129 / 状态夹 130 / 组件箱 110 / 组合夹 132（= MFC 的 control bar id）
  系统配置 6 页值  倒档 `ECLONE_DUMP_CFG=<path>` → `INT/BOOL/STR <名字> <值>` 一行一项；
                  注册表值名都带 `Cfg` 前缀（CfgNewFormW=380 / CfgNewFormH=250 / CfgGridW=8 /
                  CfgGridH=8 / CfgShowGrid=1 / CfgSnapGrid=1 / CfgUndoMemKB=0 / CfgStartMode=1101 …）
  设计器窗体      `EcFormInst` 表；`插入→窗口`(#32831) 建新窗体；程序树窗口节点 item data
                  高 16 位 = 窗体下标、低 16 位 = ECDOC_FORM
                  **没有文件夹**这个概念）
其它沿用的命令
                 文件尾 paramshow=0/1 = 参数是收起还是展开）
                 ★ 第 23 轮起 lParam 不再是 0：**lParam = 窗体下标 + 1**（0 = 不动窗体）。
                 程序树点「窗口」节点就是这么切窗体的（高 16 位存下标，见 §2.11e）。
  验证脚本（第 23 轮新增）
  UI 冒烟        powershell -File tools\ui_smoke.ps1        -> 34 PASS（回归第 27 步）
  配置冒烟       powershell -File tools\cfg_smoke.ps1       -> 20 PASS（回归第 28 步）
  手工探针       tools\probe_forms.ps1 <pid> [cmd...]       WTWindow 个数/可见性/尺寸/标题
                 tools\probe_bartoggle.ps1 <pid> <cmdid>    控制条可见性前后对比
                 tools\probe_cmd_dlg.ps1 <pid> <cmdid> [title]  发一条菜单命令并列出 #32770 及其控件
                 tools\probe_statuspanes.ps1 <pid>          倒出 id 129..137 的控件与文本
                 tools\win_tree.py <pid> [class-filter] [depth] 递归倒窗口树（Python + ctypes）
                 tools\dump_manifest.py <exe...>            查某个 exe 有没有 RT_MANIFEST（= 有没有
                                                            启用通用组件库 6.0 视觉样式；见 §3.9-24）
                  外部事件组件、COM包装支持库所封装的COM事件组件
                  本工程只产生前两组：0 基本组件 = krnln（**按文件名认**，别用 state 位，见 §3.10-7）、
                  1 扩展组件 = 其它 .fne
  全选反白        原版 Ctrl+A 的 #000080 分段（1174x388 的代码页）：
                  y=5..44（程序集表）、55..94（子程序表）、101..112（首行代码）
                  基准图 `shots\live_ctrla\after_ctrla.png`；本工程效果 `shots\ctrl_a_mine.png`
                  y 5..24 / 26..44（程序集表，宽 230）· 55..74 / 76..94（子程序表，宽 331）
                  · 101..112（首行代码，宽 178）
                  实现 = `CECEditor::DrawTableRow()`（填充那趟涂空格子 + 文字那趟判 bFull）
                        + `CECEditor::DrawAll()` 里的代码行那一段
                  对拍命令见 §3.10-12；同一样本差异 0.83%（改之前是 10.69%）
12. ⚠️⚠️ **本机抓屏（`ImageGrab.grab`）不可信 —— 别的窗口盖住本程序时，抓到的是别的窗口的像素**。
    本轮为"覆盖层时好时坏"排查了很久：`overlay_stability.py` 连开 5 次，读数
    `navy=0/200/200/200/200`、`dots=11493`（整块黑）等等，看着像渲染 bug。实际原因：
    抓屏拿到的是**盖在上面的终端窗口**的像素（实测抓出来的图是代码编辑器/终端的画面）。
    修法（两条一起用）：
    * **验证覆盖层画得全不全** → 用 `tools\overlay_paint_check.py`：它让覆盖层自己
      `RedrawWindow(...RDW_UPDATENOW...)` 再 `PrintWindow` 读回，**与被谁盖住无关**。
      实测 4 轮全是 `navy=200 red=1248` → 覆盖层本身一直是完整的。
    * **验证几何（位置/尺寸）** → `tools\des_cmp.py` 直接用 `GetWindowRect`/`GetClientRect`
      比对，不依赖像素。
    ⚠️ 所以：**抓屏只在"确认目标窗口在最前面"之后才能用来当证据**（`SetForegroundWindow`
    也未必够 —— 本环境里它没能把窗口弄到最前）。不确定时用 PrintWindow + 几何测量。

13. ✅ **撤掉了 `SetThemeAppProperties(0)`**（用户 2026-10 指正："变成 VC98 古老风格了，还有厚重边框"）。
    那是我上一轮为了"设计期窗体的经典 3D 标题栏"加的 —— 它是**进程级**开关，一关连
    **主框架自己的标题栏/边框/工具条都变经典样式**，用户一眼就看出来了。
    实测（`tools\probe_frame_metrics.py`）：主框架非客户区都没有主题
    （`OpenThemeData(ENewFrame)=0`，class style 都是 `0x8 = CS_DBLCLKS`），
    所以**别再拿全局开关换局部外观**；要单独处理设计窗体的非客户区就单独钩它的 `WM_NCPAINT`。
    `tools\frame_chrome.py` 用 `PrintWindow` 逐行量主框架的样板色（标题栏渐变/菜单带/工具条行/MDI 客户区），
    这样即使别的窗口盖住程序也能量（抓屏在这个环境不可信，见第 12 条）。
14. ⚠️ **代码编辑器是 e.exe 里自研的**（不是系统 RichEdit、也不是 Scintilla 插件）：
    * 实测 EClone 里 `Edit` 类只有 4 个，是"就地编辑单元格"用的临时控件；
      代码编辑区没有子控件 → **全部由我们自绘**（`ec_editor.cpp` 的 `DrawAll/DrawTableRow/…`）。
    * 原版：`IDE-WALKTHROUGH.md` 的结论表里写明"**中文代码编辑器（中文词法、语句块、流程箭头槽、
      事件下拉、跳转/关联名称）= e.exe 里自研**"；`ScintillaForEIDETools`
      只是"输出"面板（`lib\iDraw\superTools\plugin\eOutPutControl.dll`），不是代码编辑器。
    * 所以"编辑器像不像"完全取决于 `ec_editor.cpp` 的自绘颜色/线宽，**和系统主题无关**
      —— 网格线颜色走 `g_ecTheme`（`EC_C_GRID`），虚/实线由「系统配置」的 `id=129 表格线画为虚线` 控制。
15. ⚠️ **已知差异：工具条只有 2 行，原版 3 行**（`AfxControlBar42s` 实测 1424x**69** vs 原版 1920x**97**，
    少 28px ≈ 一行）。原版三行 = 标准条 / 对齐条 / 定位条（§7 有原始记录）。

---
### 2026-10-03 15:1x 暂停点（用户说"先停一下"）
* **代码状态**：`bin\EClone.exe` 是最新的（15:14:22，BUILD OK）。
  最新回归 `shots\regress_r48.log`：**FAIL flag = 0**，像素基准 **Dll 页 0 px / 代码页 1404 px (0.31%)** 未动。
* **本轮已提交的改动**：撤掉 `SetThemeAppProperties(0)`（见第 13 条）、组件默认属性数据（§3.10-7）、
  选中框/手柄/纸/点阵重画（§3.10-1/2）、`HitTest` 坐标系（§3.10-4）、组件箱 4 列 135 宽（§3.10-6）、
  `EcHookFormWindow` 事件驱动补画（§3.10-2）、常驻自愈定时器。
* **没做完、下次接着做的**：
  1. **用户指出的"VC98 古老风格 / 厚重边框"还没定位**（我已撤掉全局主题开关；
     量到工具条 69px vs 原版 97px，疑似少一行"定位条"）。**需要用户给一张 EClone 截图**，
     因为本机抓屏会抓到别的窗口（见第 12 条），我不敢只靠抓屏下结论。
  2. **IDA 里"代码编辑器在哪个函数"**：已确认编辑器是 e.exe 自研、`ScruttillaForEIDETools`
     只是输出面板；具体函数地址的搜寻被用户暂停打断了（子代理没产出），要重跑。
     查法：`python tools\idamcp.py list_strings "[0,4000]"` 找编辑器的窗口类/资源串，
     再 `get_xrefs_to` + `decompile_function` 定位到绘制/输入处理函数。
  3. `.e` 解析（让设计期组件画出真内容）—— 见 §3.10-10。
  4. 工具条补第三行（定位条）。
* **⚠️ 提醒**：`HANDOFF.md` 在本轮被一个就地改写的 patch 脚本截断过，已从会话记录恢复约 72%；
  **改大文件前先 `Copy-Item` 备份**。

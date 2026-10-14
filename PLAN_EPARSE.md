# 多轮计划：把 `.e` 读取做到与 e.exe 一致（含设计器）

## 0. 判据（只有一条）

**e.exe 的伪 C（REA / IDA，`C:\Program Files (x86)\e\e.exe`，SHA-256 `f43396…0359c`）就是唯一权威。**

| 来源 | 用法 |
| --- | --- |
| `e.exe` 伪 C（REA `ida` provider） | **判据**：每个字段都要能指到地址 |
| `D:\EPL\_ida\*.md`（本项目自己的早先笔记） | 线索表：告诉我去看哪个地址；结论一律回 e.exe 复核 |
| `eclone\e-packager-master`、`eplc2`、`e2txt` | 旁证/参考，**不作证据** |

读盘/写盘的入口（5.9，实测）：

| 角色 | 地址 |
| --- | --- |
| 读盘总控（按段 key 分派） | `sub_45B9B0` |
| 迷你扫描器（只认 01/02，扫过文件） | `sub_45B0D0` |
| 写盘总控 | `sub_457910` |
| 段头写出器 | `sub_4597F0` |
| 段头校验（magic/checksum/index） | `sub_444C40`（4.0）/ 5.9 见 §R1 |
| 正文渲染器（元素→文本） | `sub_4C1150` |

样本：`%TEMP%\e_re\samples\flowcmd.e`（= 官方 `lib\krnln\samples\流程类命令.e` 副本）；
另有 571 个真 `.e` 可作全量语料。

## 1. 轮次

每轮格式：**症状 → e.exe 出处（地址）→ 改动点 → 判据（前/后数字）**。

| 轮 | 内容 | 判据 |
| --- | --- | --- |
| R1 | 段目录：段头 100 字节字段、段名异或解码、段序/校验、**段 key 全表分派** | `ECLONE_DUMP_EPROJ` 段表 == e.exe 扫出的段表（571 样本） |
| R2 | 工程/系统信息段 `01`/`02`（工程名、作者、语言码 → 代码页） | cp 与 e.exe 一致；工程名进标题栏 |
| R3 | 程序段 `03`：程序集/子程序/变量/参数/常量/Dll 命令 的逐字段布局 | `EcParseProgram` 消费字节数 == 段长（无残留） |
| R4 | 资源段 `04`：窗口记录 + `part_data` + **元素属性流** → 设计器读真值 | 设计器纸色/窗体尺寸/组件属性来自 .e（不再硬编码） |
| R5 | 可选段 `05`、编辑信息段 `06`/`09`、辅助信息段 `0A`/`0B`（事件绑定/名称索引） | 事件 ↔ 子程序 绑定表与 e.exe 一致 |
| R6 | 表达式字节码 → 文本（`sub_4C1150`）+ `line_offset` 语义 | `text_oracle.py` 文本真值 0 差异行 |
| R7 | 写回：`.e` 另存（`sub_457910` + `sub_4597F0`） | 存出的 .e 能被原版打开且 `byte_coverage` 6 条断言全过 |
| R8 | 设计器 1:1：真 `.e` 打开后 设计器页逐像素 | `des_cmp.py` / `probe_design_orig.ps1` 差异 < 阈值 |

## 2. 每轮固定动作

```
cmd /c build.bat                       # 必须 BUILD OK
python tools\cmp_shots.py --steps 1,2,3,4,5,6,7   # 编辑器回归不许退化
python tools\line_cmp.py shots\cmp\01_top_orig.png shots\cmp\01_top_mine.png --dy 0
```

eclone 侧倒档：`set ECLONE_DUMP_EPROJ=<文件>`（段表 + 解析结果）。

## 3. 本轮日志

### R1 **已完成**（2026-10-11）

**做法**：先用 REA 把读盘总控 `sub_45B9B0`（+ 迷你扫描器 `sub_45B0D0`）逐条读懂，再照抄。

| 拿到的事实 | e.exe 出处 | 判据 |
| --- | --- | --- |
| 文件头 `CNWTEPRG`，每段 `[u32 magic 0x15117319][u32 checksum][92 字节段体]` | `0x45BBB2`~`0x45BBC9` | — |
| checksum = **4 通道 lane-XOR**（`buf[i] ^= lane[i&3]`），不是单字节 XOR 和 | `sub_4FF040` | `tools\e_sections.py`：571 文件全量 `ck=ok` |
| 段体 +36 = index，按 1,2,3… **逐段校验**（不符即失败） | `0x45BBF9` | 全量 `idx=ok` |
| 段体 +4 = 段名 30 字节，`Name[i] ^= Key[(i+1)%4]`，**内容是 GBK** | `sub_4FF010` | 解出中文名（见下） |
| 段体 +48 = payload 长度；payload 紧跟 100 字节段头 | `0x45BBAD/0x45BBC9` | `consumed == filesize` |
| 17 个段 key 的分派 + 每段的读者 | `sub_45B9B0`（cmp eax,imm32 全表） | 见 `SEGMENT_SPEC.md` §0.1.3（已回 e.exe 复核） |

**★ 一处纠错**：5.9 的段名是**中文**（GBK），不是早先笔记里的英文：
`02007319`=系统信息段、`01007319`=用户信息段、`04007319`=**程序资源段**、`03007319`=程序段、
`08007319`=初始模块段、`05007319`=可丢失程序段、`09007319`=编辑信息段2、`0A007319`=辅助信息段1、
`07007319`=**"s"**（End）。旧文档的 "User info section / AuxInfoSection1" 是照 4.0 猜的。

**用户信息段（`sub_4556F0` 读侧）已逐字节验证**：10 个 CString（`[u32 len][bytes]`，见 `sub_4FEBA0`）
+ 5 个 u32 + 20 字节 + u32 + 4 字节；flowcmd.e 的 size=138，解析消费 **138/138**。
`U00` = **程序名称**（`流程控制类命令演示`，= 原版标题栏第一段）、`U01` = 程序说明。

**辅助信息段1（16 字节/条）**：实测 flowcmd.e 1 条 = `form=52010001 comp=16010003 slot=0 method=04010007`
（`04010007` = `_按钮1_被单击`）⇒ 这是原版的「组件事件 → 子程序」绑定表。

**改动点**（`src/ec_eproj.cpp` / `src/ec_eproj.h`）：
1. 段遍历补齐 e.exe 的三条校验：magic 必须命中、`lane-XOR(段体 92B) == +4`、`段体+36 == 递增序号`；
   任一条不过 = 硬失败（照 `sub_45B9B0` 的清理返回 0）。
2. 段名按 `sub_4FF010` 解码（GBK），17 个 key 全表登记；新增 `EcSegKeyName` 兜底。
3. 新增用户信息段解析（`EcParseUserInfo`）→ `EcProjectName()/EcProjectDesc()`。
4. 新增辅助信息段1解析 → `EcProjectEventBindCount()/EcProjectEventBindAt()`。
5. 新增倒档 `<ECLONE_DUMP_EPROJ>.segs.txt`（段表 + 用户信息段 + 事件绑定）。
6. 新增工具 `tools\e_sections.py`（独立于 C++ 的同一套算法，用来对拍/全量扫描）。

**回测（本轮的硬数字）**：`BUILD OK`；
EClone 倒档 vs `tools\e_sections.py` 两边段表逐字段相同（9 段 / ck ok / idx ok / size 一致）；
`tools\cmp_shots.py --steps 1,2,3,4,5,6,7` 与开工前**逐字节相同**：
647 / 719 / 732 / 1253 / 945 / 951 / 4568（无回归）。

### R2 **已完成**（2026-10-11）

**目标**：主窗口标题按原版拼（原版第一段来自用户信息段 U00，不是固定字串）。

**做法 / 依据**：
1. 先用 `tools/shot.ps1` 起原版并抓主窗口（`ENewFrame`）标题，实测原文：
   `流程控制类命令演示 - C:\…\flowcmd.e [无编译条件] - Windows窗口程序 - [程序集: 启动窗口程序集 / _启动窗口]`
2. 新增 `EcProjectTitlePrefix()`（`ec_eproj.cpp`）= `<U00> - <完整路径> [无编译条件] - Windows窗口程序`；
   `[无编译条件]` 只在**没有**编译条件信息段（`0F007319`）时才写（flowcmd.e 就没有）。
3. `EClone.cpp` 新增 `EcUpdateFrameTitle()`：打开 `.e` 时把主窗口标题设成这个**前缀**，
   末尾 ` - [子窗口标题]` 由 MFC 的 MDI 主框架自己补；没打开 `.e` 时保留本工程老标题。
4. ⚠️ 两个坑（都已写进注释）：
   · MFC 主框架带 `FWS_ADDTOTITLE`：**不要自己拼方括号尾巴**，否则会拼出两个尾巴
     （实测 `… - [程序集: …] - [程序集: …]`）；
   · 只写前缀时，`SetWindowText` 返回后**立刻**读回就已经带上了 ` - [子窗口标题]`（实测倒档）。

**逐像素对比（截图 + 像素差，`shots/title/`）**：

| 项 | 结果 |
| --- | --- |
| 标题文本 | 两边**逐字相同**（`shot.ps1` 打出的窗口标题全文对比） |
| 标题带像素 | 对齐后 `onlyA=1296 / onlyB=983`（A=原版 20213 墨迹像素，B=EClone 19900，重合 18917） |
| 差异**只**集中在最左边 | 差异图 `shots/title/cap_diff.png`：第 4 个字之后整条文字**全部重合**（黑）；红/蓝只出现在窗口图标和头 3 个字上 |

⇒ **新发现（下一轮的小活）**：标题栏最左边那个小图标两边**不一样**（原版是深红"易"，EClone 偏蓝）。
原版那个图标来自 MDI 子窗口的小图标（`EC_SetWindowIcon(pChild, IDR_MAINFRAME)`），
所以要么 `IDR_MAINFRAME` 里 16x16 那档抠错了，要么原版子窗口用的不是主图标 —— 下一轮用
`tools/extract_icons.py` / `dump_icons.py` 把两边的 16x16 图标抠出来逐像素比。

**回测**：`BUILD OK`；`cmp_shots.py --steps 1..7` 与 R1 后**逐字节相同**（647/719/732/1253/945/951/4568）。

### R3/R4（设计器读 .e 的入口）

`RES2_READ_FINDING.md`（本项目早先笔记）指出 `04007319` 段体 = **3 次序列化拼接**，
顺序 1→2→3，写侧 `0x45844A/0x4584A3/0x4584D7` 与读侧 `0x45C717/0x45C72D/0x45C7E7` 一一对应
（这四条要回 e.exe 复核，别直接信）。EClone 现在的 `EcParseResourceComponentNames` 是**启发式**：
从 `4 + u32(0)` 开始按 `[hash][tag][窗口名][备注][count][len]` 走。要 1:1 就得改成照抄这三次读。

### R3 **侦察已完成**（2026-10-11）——元素块布局定死

**元素（非菜单）逐字段布局**（4 个真样本互证 + e-packager 的读侧字段序cross-check）：

```
I32  dataType                     0x0001xxxx（低 16 位 = 组件类型码；0x0001 = 窗体本身）
20 字节                            实测全 0（未解）
NUL 串 name                        例 `按钮1`；窗体那条为空（窗体名在记录的 mfc 窗口名里）
NUL 串 comment
I32  cWndAddress                   运行期窗口句柄（每次保存都不同，无语义）
I32  left, top, width, height      ★ 设计期几何；**窗体自己那条的 w/h 就是窗体尺寸**
I32  unknownBeforeParent, I32 parent
[u32 nChildren][I32 childId × n]
[u32 cursorLen][cursor bytes]
NUL 串 tag
I32  unknownBeforeVisible
I32  showStatus                    bit0 可见 / bit1 禁止 / bit2 Tab停 / bit4 锁定
I32  tabIndex
I32  eventCount + [I32 key, I32 value] × n
20 字节                            实测全 0
剩余字节                           ★★ **支持库属性数据 = `PFN_CREATE_UNIT(pAllData, nAllDataSize)` 的入参**
```

**实测样本（`tools/e_resource.py` 可复现）**：

| 样本 | 窗体尺寸 | 组件（几何全部落在窗体内） |
| --- | --- | --- |
| `flowcmd.e` | 404×423 | 按钮1 (312,352,80×32)、画板1 (8,8,272×376) |
| `明暗模式.e` | **380×250**（= IDE 新建窗体的默认尺寸，独立旁证） | 标签1 (0,0,216×16)、时钟1 (280,144,40×48) |
| `超级列表框操作类.e` | 651×487 | 56 个组件，逐个都在窗体内 |
| `启动易包/LaunchEPackage.e` | 0 个窗口（纯模块） | — |

**属性数据（blob）的形态**：按钮的 blob 末尾是 `[u32 12]["取现行选中项"]`、标签的是
`[u32 36]["使用空格键来切换#平版模式#光版模式"]` ⇒ 组件**标题/文本**属性就在 blob 里，
且 blob 整体由支持库读写（`ITF_GET_ALL_PROPERTY_DATA` = 5，见官方 `lib2.h`）。
⇒ **设计器要拿真值，正确做法就是照 e.exe：把这段 blob 原样喂给 `PFN_CREATE_UNIT`**，
不要自己解属性表。

**新增工具**：`tools/e_resource.py`（倒窗口记录 + 元素 + 几何 + blob 边界）。

### R3 解析侧 **已落地**（2026-10-11）

**改动**：`src/ec_eproj.h` 新增 `EcUnitBlob` / `EcUnitForm` 与 `g_pEcForms`；
`src/ec_eproj.cpp` 新增 `EcParseResource()`（逐字段版，旧那个启发式函数保留给正文符号表，
两条来源的 (id, 名字) 都进符号表 —— 这样换解析器不会丢名字）。倒档 `<ECLONE_DUMP_EPROJ>.segs.txt`
现在带 `forms=` 段。

**对拍（C++ vs `tools\e_resource.py`，逐字段相同）**：

| 样本 | 窗体 | 元素数 | 抽样（几何 / blob 字节数） |
| --- | --- | --- | --- |
| `flowcmd.e` | `_启动窗口` (50,50,404×423) | 3 | 按钮1 (312,352,80×32) blob=32；画板1 (8,8,272×376) blob=64 |
| `明暗模式.e` | (50,50,380×250) | 3 | 标签1 (0,0,216×16) blob=124；时钟1 (280,144,40×48) blob=8 |
| `超级列表框操作类.e` | (50,50,651×487) | **56** | 按钮_取现行选中项 (516,333,80×32) blob=40 |

**回测**：`BUILD OK`；`cmp_shots --steps 1..7` 与 R2 后逐字节相同（647/719/732/1253/945/951/4568）。

⚠️ **VC6 坑（新，别再踩）**：`ec_eproj.cpp` 顶部有个 `namespace { ... }` 匿名命名空间。
把「`extern` 声明在头文件、定义在匿名 namespace 里」的同一个名字放进去，VC6 会在**调用点**
报 `C2872 ambiguous symbol`（若是函数则报 `C2668 ambiguous call`）—— 换名字也没用。
正解：这个定义必须放在**文件作用域**（见 `g_pEcForms` 的定义位置）。

**R3 剩下的（下一步从这开始）**：
1. `ec_design.cpp`：打开 `.e` 时按窗体表建真窗体（尺寸/名字），组件用
   `PFN_CREATE_UNIT(blob 指针, blob 长度, …, x, y, cx, cy, …)` 真建。
2. **判据（截图 + 逐像素）**：同一份 `.e`，原版 `probe_design_orig.ps1` vs EClone
   `shot.ps1 -ExeArgs designer`，`diff_png.py` 出差异像素/包围盒/对照图（差多少写进日志）。

~~R3 第一步（旧）~~：
1. REA 反编译 `0x45C6E9..0x45C80E` 整段（Resource 分支），把三次读各自的**接收对象**定下来
   （`[工程+0x2B8]` / `[工程+0x2D4]` / `{vtbl=off_5F5A04,0}`）；用 `xrefs` 找 vtable 槽的真函数。
2. 目标产物（设计器要用）：**窗体**的 标题 / 宽高 / 底色属性；**每个组件**的 类型 id / 名字 /
   x,y,cx,cy / 属性流。拿到后设计器就不再硬编码 `EC_DESIGN_PAPER = COLOR_BTNFACE`。
3. **判据（这一轮必须出截图 + 逐像素）**：同一份真 `.e`，原版 `tools/probe_design_orig.ps1`
   拍设计器页 1:1，EClone `tools/shot.ps1 -ExeArgs designer` 拍同页，`tools/diff_png.py` 出
   「差异像素数 / 包围盒 / 对照图」；差多少像素要写进日志（参照 R2 的 `cap_diff.png` 做法）。
   1:1 基准图已有：`shots/live_des_orig/{form0,view2_client}.png`。

## R5 设计器页 1:1：31.07% → **99.59%**（2026-10-11，8 个可复现轮次）

**本轮的工具**（都在手边，可复现）：
```
python tools\des_round.py <tag>      # 原版 + 本工程各拍一次 WTWindow 1:1，逐像素比 + 对照图
                                     # -> shots\des_round\<tag>\{numbers.txt,orig.png,mine.png,cmp.png}
python tools\des_zoom.py x0 y0 x1 y1 <zoom> out.png   # 局部放大对照（原版上 / 本工程下）
python tools\des_probe.py            # 两边的 WTWindow：style/ex/父子链/标题/窗口矩形
python tools\wndproc32.exe <pid> <类名>   # 32 位：读目标进程窗口的 style/ex/图标（64 位 python 读不到）
python tools\childtest.bat / textofs.bat  # 32 位小实验：系统怎么画 WS_CHILD|WS_CAPTION 的标题栏
```

**每轮数字（同一样本 `C:\...\Temp\e_re\samples\flowcmd.e`，同几何 404x423）**：

| 轮 | 关键动作 | 相同率 | 剩余差异 |
| --- | --- | --- | --- |
| 1 | 基线（复现 HANDOFF 的 31.07%） | 31.07% | 画板1 那块 99438 px 全黑红 |
| 2 | **类型下标 = 低16位 − 1** + 属性数据原样喂 ITF 1 | **90.89%** | 标题栏 10504 + 点阵 ~5000 |
| 3 | 窗体属性改用"样板窗口"的文字/样式（ITF 6 逐属性复制被证伪） | 90.89% | 同上（无变化，见下注） |
| 4 | 同上重拍（确认稳定） | 90.89% | 同上 |
| 5 | 抓图前把光标挪开（排除"关闭按钮热点态"） | 90.89% | 同上 |
| 6 | **WM_NCACTIVATE(TRUE)** 让窗体显示"活动"标题栏 | **98.43%** | 文字右移 18px + 点阵 |
| 7 | **清掉窗体图标**（WM_SETICON ×3）+ **窗体自己画 8px 点阵** | **99.59%** | 只剩 704 px（组件底部 2 行） |
| 9 | hDesignWnd 传设计视图（语义对齐 lib2.h） | 99.59% | 同上（无像素变化） |
| 10 | 定稿重拍 | **99.59%** | 704 px |

**这轮定死的六件事**（都有出处/实测）：

1. **★ 元素 `dataType` 低 16 位 = 库内下标 + 1**（不是"就是下标"）。
   8 个真样本互证：窗体 0x01→0、编辑框 0x05→4、画板 0x08→7、标签 0x0A→9、按钮 0x0B→10、
   通用对话框 0x1F→30、时钟 0x20→31、图形按钮 0x2D→44；与本工程 `ECLONE_DUMP_DTICON`
   倒出来的类型表逐个对上。旧代码直接用低 16 位 ⇒ 画板被建成"分组框"、按钮被建成"选择框"
   （窗口类名实测 `Button`，原版是 `_EL_DrawPanel`）——这才是"喂属性数据画成纯黑"的真原因。
2. **元素尾部那段字节就是 ITF 1 的入参**，起点 = `tailOff + 45`（cursor/tag/showStatus/tabIndex/
   eventCount/20 字节之后）。实测：起点 so=0 喂进去，ITF 5 回吐**逐字节相同**，窗口类名也对。
   属性数据里**没有几何**（几何走 ITF 1 的 x/y/cx/cy）。
3. **设计窗体标题来自"窗体那条元素的属性数据"**，不是程序名：flowcmd.e 的 blob 末尾是
   `[u32 18]["流程控制类命令演示"]` ⇒ 原版标题栏就是它；superlist.e 的 blob 里没有这条 ⇒
   原版标题栏**是空的**（两边的 U00 都非空，实测排除）。取法：拿 dataType 低16位=1 的那条
   blob **临时**建一个"窗口"对象，读它的 `GetWindowText` / `GetWindowLong(GWL_STYLE)`
   当样板（⚠️ 逐属性 ITF 6 复制是错的：窗口类型的属性号与本工程元数据下标整片错位，
   标题读出 41904672、左边读出 2 —— r3 实测像素一个点没变）。
4. **标题栏配色 = "这个窗口活动不活动"**：本机 `HKCU\Control Panel\Colors` 的
   ActiveTitle=153,180,209 / GradientActiveTitle=185,209,234 正是原版那条渐变的两端；
   非活动则用 InactiveTitle=191,205,219 / GradientInactiveTitle=215,228,242（= 我们原来的浅色）。
   窗体是子窗口，系统不会自己给它 `WM_NCACTIVATE` ⇒ 必须自己补一次（r6：90.89% → 98.43%）。
5. **窗体不能带图标**：那个"复用的空窗体"带着 krnln 给的默认小图标（`WM_GETICON small`
   = 0x100CA97C），系统因此**在标题栏左边留 18px 图标位**，整条标题文字右移 18px
   （原版文字包围盒 x=7..114，我们 x=25..132，宽度一致）。清掉三种图标位即对齐。
6. **设计期的 8px 点阵是"窗体自己"画的**（原版那个 `WTWindow` 的 1:1 图上就有：
   1px 纯黑、间距 = 系统配置的网点间距、锚在**客户区原点**、组件盖住的地方没有点）。
   本工程原来画在视图/覆盖层上（屏幕上看得见，`PrintWindow(窗体)` 拍不到）⇒ 1:1 图整块差异。
   现在在窗体的 `WM_PAINT` 里补画（`GetDC` + 排除所有子窗口矩形）。

**剩下的 704 px**（0.41%，全部在 y=408..409 = 两个组件各自的**最后两行**）：

| 项 | 现象 | 已排除 |
| --- | --- | --- |
| 现象 | 1:1 图里 `画板1`/`按钮1` 的最后两行是**未画**的纯黑；原版那里是画板的底边框 `#646464`、按钮的 `#A0A0A0`/`#696969` | — |
| 不是组件本身 | **单独** PrintWindow 这两个组件，两边**逐像素相同**（含最后两行 374/375） | ✓ |
| 不是我们的钩子 | `ECLONE_NOFORMHOOK=1` / `ECLONE_NOGRID=1` / `ECLONE_NOOVLHOOK=1` 三档都测：这 704 px 一直在 | ✓ |
| 不是样式/图标/hDesignWnd | 两边的 style=0x52C80000、ex=0x101、类名、`WM_GETICON` 全 0、hDesignWnd 传视图或窗体，都一模一样 | ✓ |
| 下一步（判据式） | 怀疑是"窗体在 WM_PRINT 合成子窗口时，krnln 的设计期路径（`obj+368` = 设计/网格对象）**自己重画了组件预览**"——原版有那个对象、我们没有。做法：在 krnln `sub_100CA6B0`（0x100ca6b0）的 `a14==1` 分支下断，看 `obj+368` 到底建没建出来（`sub_100C5E80(obj, 0x50000200, 59396)` / `sub_100C6330(127)` 的返回值） | — |

**回测**：`BUILD OK`；`cmp_shots.py --steps 1,2,3,4,5,6,7` 与开工前**逐字节相同**
（647/719/732/1253/945/951/4568）。

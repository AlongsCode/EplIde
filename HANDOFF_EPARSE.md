# 交接：`.e` 读取 与 设计器页（启动窗口页）1:1 —— 2026-10-11

> 目标：**设计器页（启动窗口页）与原版逐像素 ≥99%**，判据是「同一份真 `.e`、同几何、
> 各拍那个真 `WTWindow` 的 1:1 图，再逐像素比」。方法一律 **REA 反编译 e.exe 为主**，
> `D:\EPL\_ida\*.md` 只当线索表，`e-packager`/`eplc2` 只当旁证。

## 0. 一句话状态

`build.bat` → **BUILD OK**；编辑器七步对拍未回归（647/719/732/1253/945/951/4568）。
设计器页（`flowcmd.e` 404×423）**99.59% 相同**（差 704 px，全在两个组件各自的最后两行）。
每一轮的判据、出处与数字见 `PLAN_EPARSE.md` 的「R5 设计器页 1:1」一节；
本轮新增工具：`tools/des_round.py`（拍+比+对照图）、`tools/des_probe.py`、`tools/wndproc32.c`。

## 1. 已经定死的东西（可复现）

| 项 | 结论 | 依据 |
| --- | --- | --- |
| 段头/段表 | `[u32 magic 0x15117319][u32 lane-XOR 校验][92 字节段体]`；段体 +0 key / +4 段名(GBK, `^=Key[(i+1)%4]`) / +36 index / +48 size | `sub_45B9B0` / `sub_4FF040` / `sub_4FF010`；`tools/e_sections.py` 全量复现 |
| 5.9 段名是**中文** | 用户信息段 / 程序资源段 / 初始模块段 / 可丢失程序段 / 编辑信息段2 / 辅助信息段1 / `"s"` | 同上（旧文档的英文名是照 4.0 猜的） |
| 用户信息段 | 10 个 `[u32 len][GBK]` + 5×u32 + 20B + u32 + 4B；`flowcmd.e` 138/138 消费 | `sub_4556F0`；`U00`=程序名称（原版标题第一段） |
| 标题栏 | `<程序名称> - <完整路径> [无编译条件] - Windows窗口程序 - [子窗口标题]`；末尾那段由 MFC 自己补 | 原版窗口标题实测 + `EcProjectTitlePrefix` |
| 资源段体 | 段体 = **3 次序列化拼接**：①`[u32 A][A 字节]` 描述符区 ②窗口记录区+`[u32 a][u32 b][b 字节]` ③恒空 `[u32 0]` | `E_FIELD_EVIDENCE.md` §4.0/§4.0.2（写出者 `sub_4F2700`） |
| **元素布局** | `I32 dataType / 20B / NUL 名字 / NUL 注释 / I32 cWnd / I32 left,top,w,h / I32 ?,parent / [u32 n][子 id×n] / [u32 cursorLen][cursor] / NUL tag / I32 ?,showStatus,tabIndex / I32 eventCount+[k,v]×n / 20B / 余下=支持库属性数据` | 4 个真样本互证：`flowcmd.e` 404×423、`明暗模式.e` 380×250（=IDE 默认新建尺寸）、`超级列表框操作类.e` 651×487/56 组件全部落在窗体内 |
| dataType→类型 | 高 16 位 = 库 space id（1=krnln）；**低 16 位 ≠ 库内下标**：`0x0B`→下标 **10**=按钮、`0x08`→下标 8=画板、`0x01`→窗口（走 `EcFindType`） | 运行时打印 `EcDtAt(lib,dt)->name` 实测 |

## 2. 缺口（下一轮从这里开始）

**唯一剩下的缺口 = 704 px**：1:1 图里 `画板1`/`按钮1` 各自的**最后两行**（窗体 y=408..409）没被画出来
（纯黑 = PrintWindow 的 DC 初始内容），原版那里是画板的底边框 `#646464`、按钮的 `#A0A0A0`/`#696969`。

已排除的（全部实测，别重做）：

| 试过 | 结果 |
| --- | --- |
| 单独 PrintWindow 这两个组件 | 两边**逐像素相同**（含最后两行）⇒ 不是组件自己的问题 |
| `ECLONE_NOFORMHOOK=1` / `ECLONE_NOGRID=1` / `ECLONE_NOOVLHOOK=1` | 704 px 一直在 ⇒ 不是本工程的钩子/画点阵造成的 |
| 样式 / 图标 / hDesignWnd | 两边 style=0x52C80000、ex=0x101、类名同、`WM_GETICON` 全 0；hDesignWnd 传视图或窗体结果相同 |
| `ECLONE_DES_NOOVERLAY=1` | 差异一个像素没变 ⇒ 不是覆盖层 |

**下一步（判据驱动）**：怀疑原版那条链是 krnln 的**设计期对象**（`obj+368`，见下）在
`WM_PRINT` 合成时**自己把组件预览画进了窗体 DC**，我们没有那个对象。
在 krnln `sub_100CA6B0`（0x100ca6b0）的 `a14==1` 分支下断，看 `obj+368` 建出来没有
——判据就是 `sub_100C5E80(obj, 0x50000200, 59396)` 与 `sub_100C6330(127)` 的返回值。

（下面这一节是上一轮的卡点记录，**已解决**，留作追溯：）

| 试过 | 结果 |
| --- | --- |
| 喂 `.e` 属性切片（起点 111） | 库**认**（回吐逐字节相同）但画成**纯黑** |
| 起点推 3 字节（tag 按长度前缀读 → 114） | 库**拒收**，两个组件都建不出来 ⇒ 回滚，tag 保持 NUL 串 |
| 用默认属性数据建（`PlaceAt` 的路）+ `EcInitUnitGeometry` + `EcSetUnitTitle` | 画板1 变成**暗红 `(120,1,0)`**，差异总数不变 |
| `hDesignWnd` 改传窗体 | 颜色微变，总数不变 |

**标尺数据**（同类型默认建一个，读 ITF 5）：
- 画板 默认 = `[1][2][272][376][0][0]`（第 3、4 个 dword = 建窗时传的宽高）
- 画板 的 `.e` 切片（起点 111）= `[1][5][0xFF000000][1][0][0][1][12][0][0][1][0x00FFFFFF][0][0][0xFF000000][0][0]`
⇒ `.e` 切片起点不对（正确起点下 `dword[2/3]` 应等于元素的 width/height）。

**下一步（判据驱动，不猜）**：从元素偏移 **108~124** 起按 4 字节步进试属性流起点，
每试一个就 `EcCreateUnitWithData` 建一次，读回 ITF 5，**取 `dword[2]==width && dword[3]==height` 的起点**；
命中后回拍，看画板1 是否变回纸色（`#F0F0F0`），再逐像素压到 ≥99%。

## 3. 复现命令（全在手边）

```
cmd /c build.bat                                                        # BUILD OK
python tools\e_sections.py <x.e> -s                                      # 段表/用户信息段
python tools\e_resource.py <x.e> [--dump 0x16010002]                     # 窗体/元素/几何/blob
set ECLONE_START_PAGE=form & set ECLONE_DUMP_EPROJ=<path>                # 倒档 <path>.segs.txt / .des.txt
powershell -File tools\probe_design_orig.ps1 -File <x.e> -OutDir ...     # 原版：抓真 WTWindow(form0.png)
powershell -File tools\probe_design_orig.ps1 -Exe bin\EClone.exe -File <x.e> -OutDir ...   # 本工程
```
比像素：两张 `form0.png` 同尺寸直接逐像素数（本轮脚本见对话/`shots/des_cmp/`）。

## 4. 坑（别再踩）

1. **VC6 的 for-init 变量作用域是外层作用域**，两个循环用同一个名字会 C2371；用不同名字。
2. 头文件 `extern` + 匿名 namespace 里定义同名实体 → VC6 在**调用点**报 `C2872/C2668`。定义必须放文件作用域。
3. MFC 主框架带 `FWS_ADDTOTITLE`：**别自己拼 ` - [子窗口标题]`**，只写前缀即可。
4. `Start-Process` 传**中文路径**参数会失败（进程读不到文件）→ 先复制到 ASCII 路径。
5. 元素里 `tag` 是 **NUL 串**（按长度前缀读会把后面所有字段推 3 字节，库直接拒收属性流）。
6. 元素字段是**顺序**不是定长：名字/注释变长 ⇒ 后面所有偏移都不定，别用"对齐到 4 字节"。
7. 设计器建窗体必须**复用设计器已有的那个空窗体**（`MoveWindow` 改尺寸），另建一个会变两个 `WTWindow`。

## 5. 本会话新增/改动的文件

| 文件 | 说明 |
| --- | --- |
| `tools/e_sections.py` | 段表 + 用户信息段 + 事件绑定（独立于 C++ 的对拍工具） |
| `tools/e_resource.py` | 程序资源段：窗口记录 + 元素 + 几何 + blob 边界 |
| `src/ec_eproj.{h,cpp}` | 段表校验/段名解码/用户信息段/辅助信息段1 + `EcParseResource`→`g_pEcForms` |
| `src/ec_design.cpp` | `CECDesigner::BuildFromProject`（按 `.e` 建真窗体+真组件）、诊断 `<dump>.des.txt`、`ECLONE_DES_NOOVERLAY` |
| `src/EClone.cpp` | 标题栏（原版格式）、进设计器页时建窗体/组件 |
| `PLAN_EPARSE.md` | 多轮计划与逐轮日志（数字/证据都在里面） |

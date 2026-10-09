# -*- coding: utf-8 -*-
"""Re-add this session's HANDOFF section (§3.10, the designer rounds) to the recovered file.

⚠️ Why: a buggy in-place patch script truncated HANDOFF.md to 1.6 KB. The bulk (2510 lines) was
recovered from the DSH session transcripts by tools/recover_handoff_merge.py, but the newest
section (the designer / property-data rounds) existed only in the still-open session, so it is
re-written here from the round's own records.
"""
import io

P = r"D:\eclone\HANDOFF.md"
s = io.open(P, encoding="utf-8", errors="replace").read()

SEC = """
### 3.10 第 26 轮：设计器逐像素对拍（窗口/组件/选中框/属性数据）

> ⚠️ 本节是**事后补写**的（原因见文末"本节来历"）。内容都是本轮实测出来的，命令都能重跑。

0. **量设计器的专用工具**（这一轮新写的，先记住这几条）：
   ```
   python tools\\des_cmp.py both        # 设计器页几何/颜色对拍：窗体偏移、纸色、点阵、组件箱
   python tools\\des_chain.py both      # 设计器窗口链（视图/窗体/覆盖层 的类名+矩形+z 序）
   python tools\\handles_navy.py orig|mine   # 按**颜色**在截图里找 8 个选中手柄，并拖 SE 手柄验证
   python tools\\frame_measure.py <png> <x> <y> <w> <h>   # 选中框的横竖扫描线 + 颜色普查
   python tools\\paper_probe.py <view.png> <x> <y> <w> <h> # "纸"的范围与点阵位置
   python tools\\overlay_stability.py 4 # 连开 4 次，数藏青手柄像素：必须每次一样（稳定性）
   python tools\\probe_rgnbtn.py both   # 设计期组件的类名/样式/矩形 对比
   python tools\\probe_theme.py         # 标题栏是"经典"还是"v6 主题"
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
   8 个手柄怎么拖都不缩放"，而 `tools\\resize_smoke.ps1` 却是绿的（它用 PostMessage 直接往
   覆盖层塞鼠标消息，绕过命中测试）。

4. **`CECDesigner::HitTest` 的坐标系**：收到的是**覆盖层坐标**，而原来拿窗体客户区矩形直接比，
   整体偏了 `(PAD,PAD)`（点组件边缘选不中）。已改成 `UnitRectInOverlay()`。

5. **标题栏样式**：本工程渲染成 **v6 主题**（Aero 淡渐变 + 亮蓝按钮），原版是**经典** 3D 标题栏
   （每行各通道 +1 的渐变，基色 RGB(160,184,208)，25 行）。两边 exe 都**没有 RT_MANIFEST**
   （`tools\\dump_manifest.py` 验过），所以是 `SetThemeAppProperties(0)` 在 `InitInstance` 里
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
   查 ITF 实现要去 `C:\\Program Files (x86)\\e\\lib\\ELogoPanel.fne`，krnln 的 ITF 表 `sub_10036470` 对它无效。
   两边真窗口参数**完全一致**（class/style/ex/rect），差的只是**属性数据**。

10. **"设计期组件画成一块黑"的结论**：`shots\\e_samples\\graphic.e` 是**原版保存过的工程**，
    那个组件的图片属性存在 .e 里的那份 ITF 5 字节；本工程 `ECLONE_DESIGN_DEMO` 是**纯代码演示**
    建的（空数据），所以只画出底黑。原版实测配色：`红 4289 / #F0F0F0 4073 / 灰 3023 / #C0C0C0 208 / 黑 55`。
    要一样就得做 **.e 解析**（把 `pAllData` 从文件读出来喂 `EcCreateUnitWithData`）。

11. 真机设计器行为对拍（`tools\\cmp_desdeep2.py both`，两边一致）：
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
python tools\\recover_handoff_merge.py     # 从 DSH 会话记录里按行号并集重建（本次恢复 2510 行）
```
⚠️ 教训：**就地改写大文件之前先 `Copy-Item` 备份**，或者改成"读-改-写新文件再替换"。

"""

i = s.find("## 7. \u5173\u952e\u5750\u6807")
if i < 0:
    raise SystemExit("anchor '## 7.' not found")
s = s[:i] + SEC.lstrip("\n") + "\n" + s[i:]
io.open(P, "w", encoding="utf-8", newline="").write(s)
print("re-added section 3.10 (%d chars)" % len(SEC))

# DLL-command page smoke test (ASCII only).
#   1) start EClone
#   2) click the "Dll" node in the work pane "program" tree
#      -> EClone switches document page -> editor gets g_szSampleDll -> model re-dumped
#   3) PrintWindow the editor 1:1 so check_dll.py can measure the real pixels
#   4) dump the program tree contents with treeinfo.exe (sanity: node set + image ids)
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File dll_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 7
)
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class DH {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string Txt(IntPtr h){ var sb=new StringBuilder(256); GetWindowTextW(h,sb,256); return sb.ToString(); }
  public static void Collect(IntPtr parent, string cls, List<IntPtr> o){
    EnumChildWindows(parent, delegate(IntPtr c, IntPtr p){
      if (Cls(c) == cls) o.Add(c);
      return true;
    }, IntPtr.Zero);
  }
}
"@

function Shot([string]$path, [IntPtr]$hwnd) {
    $r = New-Object DH+RECT
    [void][DH]::GetWindowRect($hwnd, [ref]$r)
    $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
    if ($w -le 0 -or $h -le 0) { return }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][DH]::PrintWindow($hwnd, $hdc, 0)
    $g.ReleaseHdc($hdc)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host ("shot -> " + $path + " " + $w + "x" + $h)
}

$model = Join-Path $OutDir "model_dll.txt"
Remove-Item $model -Force -ErrorAction SilentlyContinue
$env:ECLONE_DUMP_MODEL = $model

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero
[void][DH]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][DH]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [DH]::IsWindowVisible($h) -and [DH]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "FAIL: no frame"; Stop-Process -Id $p.Id -Force; exit 1 }
Write-Host ("frame=" + $script:frame + " pid=" + $p.Id)

# work pane = the AfxControlBar42s whose caption starts with U+5DE5
$script:work = [IntPtr]::Zero
[void][DH]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([DH]::Cls($h) -eq 'AfxControlBar42s') {
        $t = [DH]::Txt($h)
        if ($t.Length -ge 1 -and ([int][char]$t[0]) -eq 0x5DE5) { $script:work = $h; return $false }
    }
    return $true
}, [IntPtr]::Zero)
if ($script:work -eq [IntPtr]::Zero) { Write-Host "FAIL: no work pane"; Stop-Process -Id $p.Id -Force; exit 1 }

# the "program" tree is the HIDDEN SysTreeView32 of the work pane (the library tree is visible;
# EnumChildWindows walks z-order, so never index into that list blindly)
$trees = New-Object System.Collections.Generic.List[IntPtr]
[DH]::Collect($script:work, 'SysTreeView32', $trees)
$tree = [IntPtr]::Zero
foreach ($t in $trees) {
    Write-Host ("  tree " + $t + " visible=" + [DH]::IsWindowVisible($t))
    if (-not [DH]::IsWindowVisible($t)) { $tree = $t }
}
if ($tree -eq [IntPtr]::Zero) { Write-Host "FAIL: program tree not found"; Stop-Process -Id $p.Id -Force; exit 1 }

# click the work pane's 2nd tab so the program tree is the visible page (and the screenshots
# match what a user sees). Tab item widths are ~44px, so x=100 lands on tab 1.
$tab = [IntPtr]::Zero
$tabs = New-Object System.Collections.Generic.List[IntPtr]
[DH]::Collect($script:work, 'SysTabControl32', $tabs)
if ($tabs.Count -ge 1) { $tab = $tabs[0] }
$curTab = -1
if ($tab -ne [IntPtr]::Zero) {
    $cr = New-Object DH+RECT
    [void][DH]::GetClientRect($tab, [ref]$cr)
    $cy2 = $cr.Bottom - 10
    $lp2 = [IntPtr]((100 -band 0xFFFF) -bor ($cy2 -shl 16))
    [void][DH]::PostMessage($tab, 0x0201, [IntPtr]1, $lp2)
    Start-Sleep -Milliseconds 120
    [void][DH]::PostMessage($tab, 0x0202, [IntPtr]0, $lp2)
    Start-Sleep -Milliseconds 600
    $curTab = [int][DH]::SendMessage($tab, 0x130B, [IntPtr]::Zero, [IntPtr]::Zero)
    Write-Host ("work tab cursel = " + $curTab)
}
if ($curTab -ne 1) {
    Write-Host "FAIL: could not switch the work pane to the program page"
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Remove-Item Env:\ECLONE_DUMP_MODEL
    exit 1
}

# find the "Dll" node and click it. The tree is built with only the root expanded, so the
# visible rows are the root's children in order (see EcBuildProgTree + the fixture):
#   row0 程序数据 / row1 集_演示(文件夹) / row2 启动窗口程序集 / row3 窗口 /
#   row4 全局变量 / row5 自定义数据类型 / row6 Dll命令 / ...
# TVM_GETNEXTITEM = 0x110A, TVGN_ROOT = 0, TVGN_CHILD = 4, TVGN_NEXT = 1, TVM_GETITEMHEIGHT = 0x1111
$root = [DH]::SendMessage($tree, 0x110A, [IntPtr]0, [IntPtr]::Zero)
$cur  = [DH]::SendMessage($tree, 0x110A, [IntPtr]4, $root)
for ($k = 0; $k -lt 5; $k++) { $cur = [DH]::SendMessage($tree, 0x110A, [IntPtr]1, $cur) }
if ($cur -eq [IntPtr]::Zero) { Write-Host "FAIL: dll node not found"; Stop-Process -Id $p.Id -Force; exit 1 }
$ih = [int][DH]::SendMessage($tree, 0x1111, [IntPtr]::Zero, [IntPtr]::Zero)
if ($ih -le 0) { $ih = 16 }
$cy = 2 + 6 * $ih + [int]($ih / 2)
$cx = 60
Write-Host ("click program tree row 6 (Dll) at client " + $cx + "," + $cy + " itemH=" + $ih)
$lp = [IntPtr](($cx -band 0xFFFF) -bor ($cy -shl 16))
[void][DH]::PostMessage($tree, 0x0201, [IntPtr]1, $lp)
Start-Sleep -Milliseconds 150
[void][DH]::PostMessage($tree, 0x0202, [IntPtr]0, $lp)
Start-Sleep -Seconds 2

$m = Get-Content $model -Encoding Default
if (-not ($m | Where-Object { $_ -match 'Dll' })) {
    Write-Host "[FAIL] editor did not switch to the Dll document page (model has no Dll table)"
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Remove-Item Env:\ECLONE_DUMP_MODEL
    exit 1
}
Write-Host "[PASS] program tree selection switched the document page"

# editor view 1:1 (shots\ref_dll_page.png is a crop of exactly this window)
$script:ed = [IntPtr]::Zero
[void][DH]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([DH]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "FAIL: no editor"; Stop-Process -Id $p.Id -Force; exit 1 }
# the editor draws a system caret; hide it before the 1:1 shots so they compare with the
# reference capture (shots\ref_dll_page.png has no caret in it either)
[void][DH]::PostMessage($script:ed, 0x0008, [IntPtr]::Zero, [IntPtr]::Zero)   # WM_KILLFOCUS
Start-Sleep -Milliseconds 300

Shot (Join-Path $OutDir "dll_view.png") $script:ed
# the editor view is only ~394px tall, so scroll down to reach the 3rd Dll block
[void][DH]::SendMessage($script:ed, 0x0115, [IntPtr]3, [IntPtr]::Zero)   # WM_VSCROLL / SB_PAGEDOWN
Start-Sleep -Milliseconds 300
[void][DH]::SendMessage($script:ed, 0x0115, [IntPtr]3, [IntPtr]::Zero)
Start-Sleep -Milliseconds 400
Shot (Join-Path $OutDir "dll_view2.png") $script:ed
Shot (Join-Path $OutDir "dll_frame.png") $script:frame

# tree contents (treeinfo expands everything, so do it last)
cmd /c "`"D:\eclone\tools\treeinfo.exe`" $($p.Id) > `"$(Join-Path $OutDir 'progtree_mine.txt')`""
Write-Host ("program tree dump -> " + (Join-Path $OutDir "progtree_mine.txt"))

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_MODEL
Write-Host ("model -> " + $model)

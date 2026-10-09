# Dump the original IDE's program page tree (work pane 2nd tab) contents.
# ASCII-only on purpose (this machine reads .ps1 as ANSI).
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File probe_progtree.ps1 [-File x.e] [-OutDir d]
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\mcp\reverse-skill\work\20260930-222630-5-9-e-exe-ide-krnln-sdk\ide_probe\sample\graphic.e",
    [string]$OutDir = "D:\eclone\shots\live_progtree",
    [int]$WaitSec = 16
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class PT {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static string Txt(IntPtr h){ var sb=new StringBuilder(512); GetWindowTextW(h,sb,512); return sb.ToString(); }
  public static IntPtr FindChild(IntPtr parent, string cls){
    IntPtr found = IntPtr.Zero;
    EnumChildWindows(parent, delegate(IntPtr c, IntPtr p){
      if (found == IntPtr.Zero && Cls(c) == cls) { found = c; return false; }
      return true;
    }, IntPtr.Zero);
    return found;
  }
  public static void CollectDesc(IntPtr parent, string cls, List<IntPtr> o){
    EnumChildWindows(parent, delegate(IntPtr c, IntPtr p){
      if (Cls(c) == cls) o.Add(c);
      return true;
    }, IntPtr.Zero);
  }
}
"@

$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList @($File) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$frame = [IntPtr]::Zero
[void][PT]::EnumWindows([PT+EnumProc]{
    param($h, $q)
    $wpid = 0
    [void][PT]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [PT]::Cls($h) -eq 'ENewFrame') { $script:frame = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "no frame"; $p.Kill(); exit 1 }
Write-Host ("frame=" + $script:frame + " pid=" + $p.Id)

# work pane: the AfxControlBar42s whose caption starts with U+5DE5 (gong)
$work = [IntPtr]::Zero
[void][PT]::EnumChildWindows($script:frame, [PT+EnumProc]{
    param($h, $q)
    if ([PT]::Cls($h) -eq 'AfxControlBar42s') {
        $t = [PT]::Txt($h)
        if ($t.Length -ge 1 -and ([int][char]$t[0]) -eq 0x5DE5) { $script:work = $h; return $false }
    }
    return $true
}, [IntPtr]::Zero)
if ($script:work -eq [IntPtr]::Zero) { Write-Host "no work pane"; $p.Kill(); exit 1 }
Write-Host ("work pane=" + $script:work)

$tabs = New-Object System.Collections.Generic.List[IntPtr]
[PT]::CollectDesc($script:work, 'SysTabControl32', $tabs)
$tab = $tabs[0]
$cr = New-Object PT+RECT
[void][PT]::GetClientRect($tab, [ref]$cr)
$nTab = [PT]::SendMessage($tab, 0x1304, [IntPtr]::Zero, [IntPtr]::Zero)
Write-Host ("tab hwnd=" + $tab + " client=" + $cr.Right + "x" + $cr.Bottom + " count=" + $nTab)

# click candidate x positions along the tab strip (bottom row) until tab 2 is current
$xList = @(78, 84, 70, 90, 60, 100, 110, 50)
foreach ($x in $xList) {
    $y = $cr.Bottom - 10
    [void][PT]::PostMessage($tab, 0x0201, [IntPtr]1, [IntPtr](($y -shl 16) -bor $x))
    Start-Sleep -Milliseconds 120
    [void][PT]::PostMessage($tab, 0x0202, [IntPtr]0, [IntPtr](($y -shl 16) -bor $x))
    Start-Sleep -Milliseconds 900
    $cur = [PT]::SendMessage($tab, 0x130B, [IntPtr]::Zero, [IntPtr]::Zero)
    Write-Host ("click x=" + $x + " y=" + $y + " -> cursel=" + $cur)
    if ([int]$cur -eq 1) { break }
}

$trees = New-Object System.Collections.Generic.List[IntPtr]
[PT]::CollectDesc($script:work, 'SysTreeView32', $trees)
$i = 0
foreach ($t in $trees) { Write-Host ("tree[" + $i + "] hwnd=" + $t + " vis=" + [PT]::IsWindowVisible($t)); $i++ }

$r = New-Object PT+RECT
[void][PT]::GetWindowRect($script:work, [ref]$r)
$w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[void][PT]::PrintWindow($script:work, $hdc, 0)
$g.ReleaseHdc($hdc)
$bmp.Save((Join-Path $OutDir "work.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host ("shot work pane -> " + (Join-Path $OutDir "work.png"))

$dump = Join-Path $OutDir "trees.txt"
# tool path relative to this script (was hard-coded to D:\eclone; the tree may live elsewhere)
$treeinfo = Join-Path $PSScriptRoot "treeinfo.exe"
cmd /c "`"$treeinfo`" $($p.Id) > `"$dump`""
Write-Host ("trees -> " + $dump)

# treeinfo expanded every node; now screenshot the visible tree 1:1 so the node
# icons can be lifted out of it (the program tree icons are not in any e.exe resource)
$i = 0
foreach ($t in $trees) {
    if (-not [PT]::IsWindowVisible($t)) { continue }
    $tr = New-Object PT+RECT
    [void][PT]::GetWindowRect($t, [ref]$tr)
    $tw = $tr.Right - $tr.Left; $th = $tr.Bottom - $tr.Top
    if ($tw -le 0 -or $th -le 0) { continue }
    $tb = New-Object System.Drawing.Bitmap $tw, $th
    $tg = [System.Drawing.Graphics]::FromImage($tb)
    $thdc = $tg.GetHdc()
    [void][PT]::PrintWindow($t, $thdc, 0)
    $tg.ReleaseHdc($thdc)
    $tp = Join-Path $OutDir ("tree" + $i + ".png")
    $tb.Save($tp, [System.Drawing.Imaging.ImageFormat]::Png)
    $tg.Dispose(); $tb.Dispose()
    Write-Host ("tree shot -> " + $tp + " " + $tw + "x" + $th)
    $i++
}

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue

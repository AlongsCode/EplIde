# Probe the real E-language IDE (e.exe): dump windows, walk/expand tree controls,
# screenshot. ASCII-only on purpose (this machine reads .ps1 as ANSI).
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File probe_orig.ps1
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 10,
    [int]$CmdId = 0,
    [switch]$ExpandTrees,
    [switch]$Keep
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class OP {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Text(IntPtr h){
    var sb=new StringBuilder(2048);
    GetWindowTextW(h,sb,2048);
    if (sb.Length > 0) return sb.ToString();
    // cross-process control text: the system marshals WM_GETTEXT
    IntPtr buf = Marshal.AllocHGlobal(4096);
    for (int i=0;i<2048;i++) Marshal.WriteInt16(buf, i*2, 0);
    SendMessage(h, 0x000D, (IntPtr)2048, buf);
    string s = Marshal.PtrToStringUni(buf);
    Marshal.FreeHGlobal(buf);
    return s == null ? "" : s;
  }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static void Dump(IntPtr top, List<string> o){
    // EnumChildWindows already walks the whole subtree; compute depth from the parent chain.
    IntPtr root = top;
    EnumChildWindows(top, delegate(IntPtr c, IntPtr p){
      int d = 0; IntPtr q = GetParent(c);
      while (q != IntPtr.Zero && q != root) { d++; q = GetParent(q); }
      RECT r; GetWindowRect(c, out r);
      o.Add(new string(' ', (d+1)*2) + string.Format("{0,-24} vis={1,-6} {2},{3} {4}x{5}  style=0x{6:X8}  {7}",
          Cls(c), IsWindowVisible(c), r.Left, r.Top, r.Right-r.Left, r.Bottom-r.Top,
          GetWindowLong(c,-16), Text(c)));
      return true;
    }, IntPtr.Zero);
  }
  public static void CollectTrees(IntPtr top, List<IntPtr> o){
    EnumChildWindows(top, delegate(IntPtr c, IntPtr p){
      if (Cls(c) == "SysTreeView32") o.Add(c);
      return true;
    }, IntPtr.Zero);
  }
}
"@

$procs = Get-Process -Name e -ErrorAction SilentlyContinue
if ($procs) { Write-Host "e.exe already running, killing"; $procs | Stop-Process -Force; Start-Sleep -Seconds 2 }
$all = Get-Process -Name e -ErrorAction SilentlyContinue
if (-not $all) { }

# e.exe must run with its own directory as CWD (it loads lib\*.fne by relative path)
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "process exited early: $($p.ExitCode)"; exit 1 }

function Get-Tops {
    $list = New-Object System.Collections.Generic.List[object]
    $cb = [OP+EnumProc]{
        param($h, $p2)
        $wpid = 0
        [void][OP]::GetWindowThreadProcessId($h, [ref]$wpid)
        if ($wpid -eq $p.Id -and [OP]::IsWindowVisible($h)) {
            $r = New-Object OP+RECT
            [void][OP]::GetWindowRect($h, [ref]$r)
            if (($r.Right-$r.Left) -gt 120) {
                $list.Add([pscustomobject]@{ H=$h; C=[OP]::Cls($h); T=[OP]::Text($h); W=($r.Right-$r.Left); Hh=($r.Bottom-$r.Top) })
            }
        }
        return $true
    }
    [void][OP]::EnumWindows($cb, [IntPtr]::Zero)
    return $list
}

$tops = Get-Tops
$frame = $tops | Where-Object { $_.C -eq 'ENewFrame' } | Select-Object -First 1
if (-not $frame) { $frame = $tops | Sort-Object -Property W -Descending | Select-Object -First 1 }
if (-not $frame) { Write-Host "no frame"; if (-not $Keep) { $p.Kill() }; exit 1 }
Write-Host ("frame: " + $frame.C + "  " + $frame.W + "x" + $frame.Hh + "  " + $frame.T + "  hwnd=" + $frame.H)

$lines = New-Object System.Collections.Generic.List[string]
[OP]::Dump($frame.H, $lines)
$lines | Set-Content -Path (Join-Path $OutDir "orig_ui.windows.txt") -Encoding UTF8

function Shot([string]$tag, [IntPtr]$hwnd) {
    $r = New-Object OP+RECT
    [void][OP]::GetWindowRect($hwnd, [ref]$r)
    $w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
    if ($w -le 0 -or $h -le 0) { return }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][OP]::PrintWindow($hwnd, $hdc, 2)
    $g.ReleaseHdc($hdc)
    $png = Join-Path $OutDir $tag
    $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "shot: $png ($w x $h)"
}

Shot "orig_full.png" $frame.H

# --- tree walking (no remote memory needed for handle-only messages) ---
$TVM_GETNEXTITEM = 0x110A
$TVM_EXPAND = 0x1102
$TVM_GETCOUNT = 0x1105
$TVM_GETITEMSTATE = 0x1127
$TVGN_ROOT = 0; $TVGN_NEXT = 1; $TVGN_CHILD = 4; $TVGN_CARET = 9
$TVE_EXPAND = 2

function Walk-Expand([IntPtr]$tv, [IntPtr]$item) {
    while ($item -ne [IntPtr]::Zero) {
        [void][OP]::SendMessage($tv, $TVM_EXPAND, [IntPtr]$TVE_EXPAND, $item)
        $child = [OP]::SendMessage($tv, $TVM_GETNEXTITEM, [IntPtr]$TVGN_CHILD, $item)
        if ($child -ne [IntPtr]::Zero) { Walk-Expand $tv $child }
        $item = [OP]::SendMessage($tv, $TVM_GETNEXTITEM, [IntPtr]$TVGN_NEXT, $item)
    }
}

function Walk-Tree([IntPtr]$tv, [IntPtr]$item, [int]$depth, [System.Collections.Generic.List[string]]$out) {
    while ($item -ne [IntPtr]::Zero) {
        $st = [OP]::SendMessage($tv, $TVM_GETITEMSTATE, $item, [IntPtr]0x3000)   # TVIS_STATEIMAGEMASK
        $out.Add(("  " * $depth) + "item=" + $item + " state=0x" + ([int]$st).ToString("X"))
        $child = [OP]::SendMessage($tv, $TVM_GETNEXTITEM, [IntPtr]$TVGN_CHILD, $item)
        if ($child -ne [IntPtr]::Zero) { Walk-Tree $tv $child ($depth+1) $out }
        $item = [OP]::SendMessage($tv, $TVM_GETNEXTITEM, [IntPtr]$TVGN_NEXT, $item)
    }
}

$trees = New-Object System.Collections.Generic.List[IntPtr]
[OP]::CollectTrees($frame.H, $trees)
Write-Host ("tree controls: " + $trees.Count)
$ti = 0
foreach ($tv in $trees) {
    $r = New-Object OP+RECT
    [void][OP]::GetWindowRect($tv, [ref]$r)
    if (($r.Right-$r.Left) -lt 40 -or ($r.Bottom-$r.Top) -lt 40) { continue }
    $cnt = [OP]::SendMessage($tv, $TVM_GETCOUNT, [IntPtr]::Zero, [IntPtr]::Zero)
    Write-Host ("tree[$ti] hwnd=" + $tv + " " + ($r.Right-$r.Left) + "x" + ($r.Bottom-$r.Top) + " count=" + $cnt)
    if ($ExpandTrees) {
        $root = [OP]::SendMessage($tv, $TVM_GETNEXTITEM, [IntPtr]$TVGN_ROOT, [IntPtr]::Zero)
        # expand twice so grandchildren show up as well
        for ($k=0; $k -lt 2; $k++) {
            Walk-Expand $tv $root
            Start-Sleep -Milliseconds 300
        }
        Start-Sleep -Milliseconds 600
        Shot ("orig_tree$ti.png") $tv
    }
    $dump = New-Object System.Collections.Generic.List[string]
    $root = [OP]::SendMessage($tv, $TVM_GETNEXTITEM, [IntPtr]$TVGN_ROOT, [IntPtr]::Zero)
    Walk-Tree $tv $root 0 $dump
    $dump | Set-Content -Path (Join-Path $OutDir "orig_tree$ti.txt") -Encoding UTF8
    $ti++
}

if ($CmdId -ne 0) {
    Write-Host ("posting WM_COMMAND " + $CmdId + " to frame")
    [void][OP]::SetForegroundWindow($frame.H)
    [void][OP]::PostMessage($frame.H, 0x0111, [IntPtr]$CmdId, [IntPtr]::Zero)
    Start-Sleep -Seconds 4
    Get-Tops | ForEach-Object { Write-Host ("  top: " + $_.C + " " + $_.W + "x" + $_.Hh + " " + $_.T + " hwnd=" + $_.H) }
    $dlg = Get-Tops | Where-Object { $_.C -eq '#32770' } | Select-Object -First 1
    if ($dlg) {
        $lines2 = New-Object System.Collections.Generic.List[string]
        [OP]::Dump($dlg.H, $lines2)
        $lines2 | Set-Content -Path (Join-Path $OutDir "orig_dialog.windows.txt") -Encoding UTF8
        Shot "orig_dialog.png" $dlg.H
    } else { Write-Host "no dialog appeared" }
}

if (-not $Keep) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }

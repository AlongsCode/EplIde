# Launch a built exe, screenshot its main window (PrintWindow), then kill it.
# ASCII-only on purpose.
# Usage: powershell -File shot.ps1 -Exe <path> -Tag <name> [-WaitSec 4] [-Keep]
#        [-ExeArgs designer] [-SizeA 1200x700] [-SizeB 900x500] [-Screen]
#   -SizeA/-SizeB : resize the frame window before the screenshot (reproduce resize bugs)
#   -Screen          : grab the window rect off the screen instead of PrintWindow
#                      (PrintWindow asks the window to render itself, which can hide repaint bugs)
param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [Parameter(Mandatory=$true)][string]$Tag,
    [int]$WaitSec = 4,
    [string]$OutDir = "D:\eclone\shots",
    [string]$ExeArgs = "",
    [string]$SizeA = "",
    [string]$SizeB = "",
    [switch]$Screen,
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
public class SH {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int hh, bool repaint);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool RedrawWindow(IntPtr h, IntPtr r, IntPtr rgn, uint flags);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern bool UpdateWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Text(IntPtr h){ var sb=new StringBuilder(2048); GetWindowTextW(h,sb,2048); return sb.ToString(); }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static void Dump(IntPtr h, int d, List<string> o){
    RECT r; GetWindowRect(h, out r);
    o.Add(new string(' ', d*2) + string.Format("{0,-30} vis={1,-6} {2},{3} {4}x{5}  {6}",
        Cls(h), IsWindowVisible(h), r.Left, r.Top, r.Right-r.Left, r.Bottom-r.Top, Text(h)));
    EnumChildWindows(h, delegate(IntPtr c, IntPtr p){ Dump(c, d+1, o); return true; }, IntPtr.Zero);
  }
}
"@

if ($ExeArgs -ne '') { $p = Start-Process -FilePath $Exe -ArgumentList $ExeArgs -PassThru } else { $p = Start-Process -FilePath $Exe -PassThru }
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "process exited early: $($p.ExitCode)"; exit 1 }

$tops = New-Object System.Collections.Generic.List[object]
$cb = [SH+EnumProc]{
    param($h, $p2)
    $pid2 = 0
    [void][SH]::GetWindowThreadProcessId($h, [ref]$pid2)
    if ($pid2 -eq $p.Id -and [SH]::IsWindowVisible($h)) {
        $r = New-Object SH+RECT
        [void][SH]::GetWindowRect($h, [ref]$r)
        if (($r.Right-$r.Left) -gt 300) {
            $tops.Add([pscustomobject]@{ H=$h; C=[SH]::Cls($h); T=[SH]::Text($h); W=($r.Right-$r.Left); Hh=($r.Bottom-$r.Top) })
        }
    }
    return $true
}
[void][SH]::EnumWindows($cb, [IntPtr]::Zero)
if ($tops.Count -eq 0) { Write-Host "no frame found"; if (-not $Keep) { $p.Kill() }; exit 1 }
$frame = $tops | Sort-Object -Property W -Descending | Select-Object -First 1
Write-Host ("frame: " + $frame.C + "  " + $frame.W + "x" + $frame.Hh + "  " + $frame.T)

function Invoke-Resize([string]$spec) {
    if ($spec -eq '') { return }
    $parts = $spec -split 'x'
    $w = [int]$parts[0]; $h = [int]$parts[1]
    $r = New-Object SH+RECT
    [void][SH]::GetWindowRect($frame.H, [ref]$r)
    [void][SH]::MoveWindow($frame.H, $r.Left, $r.Top, $w, $h, $true)
    Start-Sleep -Milliseconds 900
    Write-Host ("resized to " + $w + "x" + $h)
}
# NOTE: this file must stay pure ASCII - the machine reads .ps1 as ANSI, and a
# non-ASCII comment silently eats the statement that follows it.
if ($SizeA -ne '') { Invoke-Resize $SizeA }
if ($SizeB -ne '') { Invoke-Resize $SizeB }

$lines = New-Object System.Collections.Generic.List[string]
[SH]::Dump($frame.H, 0, $lines)
$lines | Set-Content -Path (Join-Path $OutDir "$Tag.windows.txt") -Encoding UTF8

$r = New-Object SH+RECT
[void][SH]::GetWindowRect($frame.H, [ref]$r)
$w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
if ($Screen) {
    [void][SH]::SetWindowPos($frame.H, [IntPtr](-1), 0,0,0,0, 0x0003)  # HWND_TOPMOST|SWP_NOMOVE|SWP_NOSIZE
    [void][SH]::SetForegroundWindow($frame.H)
    Start-Sleep -Milliseconds 600
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size $w, $h))
    [void][SH]::SetWindowPos($frame.H, [IntPtr](-2), 0,0,0,0, 0x0003)  # HWND_NOTOPMOST
} else {
    $hdc = $g.GetHdc()
    [void][SH]::PrintWindow($frame.H, $hdc, 2)
    $g.ReleaseHdc($hdc)
}
$png = Join-Path $OutDir "$Tag.png"
$bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "screenshot: $png ($w x $h)"
if (-not $Keep) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }

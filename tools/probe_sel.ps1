# Launch a fresh original e.exe with a sample .e, click into the code editor,
# select all (Ctrl+A), copy (Ctrl+C), screenshot every editor view 1:1 and dump
# the clipboard. Read-only w.r.t. the source file (we never save).
# ASCII-only on purpose (this machine reads .ps1 as ANSI).
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "",
    [string]$OutDir = "D:\eclone\shots\live_sel",
    [int]$WaitSec = 12
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class SL {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static string Txt(IntPtr h){ var sb=new StringBuilder(512); GetWindowTextW(h,sb,512); return sb.ToString(); }
  public static void Click(int x, int y){
    SetCursorPos(x, y);
    System.Threading.Thread.Sleep(180);
    mouse_event(2, 0, 0, 0, IntPtr.Zero);
    System.Threading.Thread.Sleep(60);
    mouse_event(4, 0, 0, 0, IntPtr.Zero);
    System.Threading.Thread.Sleep(180);
  }
  public static void CtrlKey(byte vk){
    keybd_event(0x11, 0, 0, IntPtr.Zero);
    System.Threading.Thread.Sleep(80);
    keybd_event(vk, 0, 0, IntPtr.Zero);
    System.Threading.Thread.Sleep(80);
    keybd_event(vk, 0, 2, IntPtr.Zero);
    System.Threading.Thread.Sleep(80);
    keybd_event(0x11, 0, 2, IntPtr.Zero);
    System.Threading.Thread.Sleep(120);
  }
}
"@

$args2 = @()
if ($File -ne "") { $args2 += $File }
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList $args2 -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$frame = [IntPtr]::Zero
[void][SL]::EnumWindows([SL+EnumProc]{
    param($h, $q)
    $wpid = 0
    [void][SL]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [SL]::IsWindowVisible($h) -and [SL]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
if ($frame -eq [IntPtr]::Zero) { Write-Host "no frame"; $p.Kill(); exit 1 }
Write-Host ("frame=" + $frame + " title=" + [SL]::Txt($frame))

[void][SL]::BringWindowToTop($frame)
[void][SL]::SetWindowPos($frame, [IntPtr]::Zero, 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040)
[void][SL]::SetForegroundWindow($frame)
Start-Sleep -Milliseconds 900

$views = New-Object System.Collections.Generic.List[IntPtr]
[void][SL]::EnumChildWindows($frame, [SL+EnumProc]{
    param($h, $q)
    if ([SL]::Cls($h).StartsWith('Afx:400000:b')) { $views.Add($h) }
    return $true
}, [IntPtr]::Zero)
Write-Host ("views=" + $views.Count)

function Shot([string]$tag, [IntPtr]$hwnd) {
    $r = New-Object SL+RECT
    [void][SL]::GetWindowRect($hwnd, [ref]$r)
    $w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
    if ($w -le 0 -or $h -le 0) { return }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][SL]::PrintWindow($hwnd, $hdc, 0)
    $g.ReleaseHdc($hdc)
    $png = Join-Path $OutDir $tag
    $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host ("shot: " + $png + " " + $w + "x" + $h)
}

# the biggest Afx:400000:b child is the code editor view
$best = [IntPtr]::Zero; $area = 0
foreach ($v in $views) {
    $r = New-Object SL+RECT
    [void][SL]::GetWindowRect($v, [ref]$r)
    $a = ($r.Right-$r.Left) * ($r.Bottom-$r.Top)
    if ($a -gt $area) { $area = $a; $best = $v; $bestr = $r }
}
Write-Host ("editor view=" + $best + " rect=" + $bestr.Left + "," + $bestr.Top + " " + ($bestr.Right-$bestr.Left) + "x" + ($bestr.Bottom-$bestr.Top))
if ($best -eq [IntPtr]::Zero) { $p.Kill(); exit 1 }

Shot "before.png" $best
[void][SL]::SetFocus($best)
[SL]::Click($bestr.Left + 200, $bestr.Top + 60)
Start-Sleep -Milliseconds 400
Shot "afterclick.png" $best
[SL]::CtrlKey(0x41)
Start-Sleep -Milliseconds 500
Shot "after_a.png" $best
[SL]::CtrlKey(0x43)
Start-Sleep -Milliseconds 700

$clip = [System.Windows.Forms.Clipboard]::GetDataObject()
$fmts = $clip.GetFormats()
Write-Host ("clip formats: " + ($fmts -join ", "))
$txt = [System.Windows.Forms.Clipboard]::GetText()
$enc = [System.Text.Encoding]::Default
[System.IO.File]::WriteAllText((Join-Path $OutDir "clip.txt"), $txt, $enc)
Write-Host ("clip text chars=" + $txt.Length)

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue

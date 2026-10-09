# Capture EClone's editor view 1:1 for the "code page" comparison (the program-set document).
# Started with the sample swapped to the content of shots\user_ref.png (tools\patch_sample_ref.py).
# ASCII only.
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 7,
    [int]$ScrollPages = 0,          # >0: press PageDown N times first (to shoot the code area / flow lines)
    [int]$ScrollLines = 0,          # >0: send SB_LINEDOWN N times (finer control)
    [string]$Tag = "code_view"      # output file name (without .png)
)
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class CV {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
}
"@
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }

$script:frame = [IntPtr]::Zero
[void][CV]::EnumWindows([CV+EnumProc]{
    param($h,$q)
    $wpid = 0; [void][CV]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [CV]::Cls($h) -eq 'ENewFrame') { $script:frame = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "FAIL: no frame"; Stop-Process -Id $p.Id -Force; exit 1 }

$script:ed = [IntPtr]::Zero
[void][CV]::EnumChildWindows($script:frame, [CV+EnumProc]{
    param($h,$q)
    if ([CV]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "FAIL: no editor"; Stop-Process -Id $p.Id -Force; exit 1 }

[void][CV]::PostMessage($script:ed, 0x0008, [IntPtr]::Zero, [IntPtr]::Zero)   # WM_KILLFOCUS: drop the caret
Start-Sleep -Milliseconds 300

if ($ScrollPages -gt 0) {
    # NOTE: WM_VSCROLL / SB_PAGEDOWN was tried first and had no effect in this app
    # (probably because the editor never gets the scrollbar range from a real thumb),
    # so drive it through the keyboard handler instead: VK_NEXT scrolls one page.
    for ($i = 0; $i -lt $ScrollPages; $i++) {
        [void][CV]::SendMessage($script:ed, 0x0100, [IntPtr]0x22, [IntPtr]::Zero)   # WM_KEYDOWN / VK_NEXT
        Start-Sleep -Milliseconds 150
        [void][CV]::SendMessage($script:ed, 0x0101, [IntPtr]0x22, [IntPtr]::Zero)   # WM_KEYUP
        Start-Sleep -Milliseconds 150
    }
    [void][CV]::PostMessage($script:ed, 0x0008, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 300
}
if ($ScrollLines -gt 0) {
    for ($i = 0; $i -lt $ScrollLines; $i++) {
        [void][CV]::SendMessage($script:ed, 0x0115, [IntPtr]1, [IntPtr]::Zero)   # WM_VSCROLL / SB_LINEDOWN
        Start-Sleep -Milliseconds 40
    }
    Start-Sleep -Milliseconds 300
}

$r = New-Object CV+RECT
[void][CV]::GetWindowRect($script:ed, [ref]$r)
$w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[void][CV]::PrintWindow($script:ed, $hdc, 0)
$g.ReleaseHdc($hdc)
$png = Join-Path $OutDir ($Tag + ".png")
$bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host ("editor shot -> " + $png + " " + $w + "x" + $h)

# document tab strip = the sibling of the editor whose height is 22 (TAB_H),
# i.e. the bottom bar. Shot 1:1 so it can be diffed against the original's
# CCustomTabCtrl capture (shots\live_tabs\tabbar0.png, also 1:1).
$script:tab = [IntPtr]::Zero
[void][CV]::EnumChildWindows([CV]::GetParent($script:ed), [CV+EnumProc]{
    param($h,$q)
    $rr = New-Object CV+RECT
    [void][CV]::GetWindowRect($h, [ref]$rr)
    if (($rr.Bottom - $rr.Top) -eq 22 -and ($rr.Right - $rr.Left) -gt 500) { $script:tab = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:tab -ne [IntPtr]::Zero) {
    $tr = New-Object CV+RECT
    [void][CV]::GetWindowRect($script:tab, [ref]$tr)
    $tw = $tr.Right - $tr.Left; $th = $tr.Bottom - $tr.Top
    $tb = New-Object System.Drawing.Bitmap $tw, $th
    $tg = [System.Drawing.Graphics]::FromImage($tb)
    $thdc = $tg.GetHdc()
    [void][CV]::PrintWindow($script:tab, $thdc, 0)
    $tg.ReleaseHdc($thdc)
    $tpng = Join-Path $OutDir ($Tag + "_tabbar.png")
    $tb.Save($tpng, [System.Drawing.Imaging.ImageFormat]::Png)
    $tg.Dispose(); $tb.Dispose()
    Write-Host ("tabbar shot -> " + $tpng + " " + $tw + "x" + $th)
} else {
    Write-Host "tabbar: not found"
}
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue

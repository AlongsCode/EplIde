# Smoke test: start EClone, find the self-drawn editor view, post a few keystrokes
# straight into it, then confirm the process survived and grab a screenshot.
# ASCII-only on purpose (this machine reads .ps1 as ANSI).
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots"
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class EK {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
}
"@

$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru
Start-Sleep -Seconds 4

$frame = [IntPtr]::Zero
for ($try = 0; $try -lt 20 -and $frame -eq [IntPtr]::Zero; $try++) {
    Start-Sleep -Seconds 1
    [EK]::EnumWindows([EK+EnumProc]{
        param($h,$q)
        $wpid = 0; [void][EK]::GetWindowThreadProcessId($h, [ref]$wpid)
        if ($wpid -eq $p.Id -and [EK]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
        return $true
    }, [IntPtr]::Zero) | Out-Null
}
Write-Host ("frame=" + $frame + " (waited " + $try + "s)")

# editor = the Afx:400000:8:... child of AfxMDIFrame42s
$ed = [IntPtr]::Zero
[EK]::EnumChildWindows($frame, [EK+EnumProc]{
    param($h,$q)
    $c = [EK]::Cls($h)
    if ($c -like 'Afx:400000:8:10005*') { $script:ed = $h }
    return $true
}, [IntPtr]::Zero) | Out-Null

if ($ed -eq [IntPtr]::Zero) {
    Write-Host "editor view NOT FOUND; children of frame:"
    [EK]::EnumChildWindows($frame, [EK+EnumProc]{
        param($h,$q); Write-Host ("   " + [EK]::Cls($h)); return $true
    }, [IntPtr]::Zero) | Out-Null
    $p.Kill(); exit 1
}
Write-Host ("editor hwnd=" + $ed + " pid=" + $p.Id)

$WM_CHAR = 0x0102; $WM_KEYDOWN = 0x0100; $WM_KEYUP = 0x0101
$WM_SETFOCUS = 0x0007; $WM_LBUTTONDOWN = 0x0201; $WM_LBUTTONUP = 0x0202
# click inside the code area so the caret lands on a code line, then type
[void][EK]::PostMessage($ed, $WM_SETFOCUS, [IntPtr]::Zero, [IntPtr]::Zero)
[void][EK]::PostMessage($ed, $WM_LBUTTONDOWN, [IntPtr]1, [IntPtr]((300 -band 0xFFFF) -bor (330 -shl 16)))
[void][EK]::PostMessage($ed, $WM_LBUTTONUP,   [IntPtr]0, [IntPtr]((300 -band 0xFFFF) -bor (330 -shl 16)))
Start-Sleep -Milliseconds 300
foreach ($ch in @('X','Y','Z')) { [void][EK]::PostMessage($ed, $WM_CHAR, [IntPtr][int][char]$ch, [IntPtr]1) }
foreach ($vk in @(0x0D, 0x08, 0x28, 0x26, 0x23, 0x24, 0x22, 0x21, 0x22, 0x09)) {   # Enter Back Down Up End Home Right PgDn PgUp Tab
    [void][EK]::PostMessage($ed, $WM_KEYDOWN, [IntPtr]$vk, [IntPtr]1)
    [void][EK]::PostMessage($ed, $WM_KEYUP,   [IntPtr]$vk, [IntPtr]1)
    Start-Sleep -Milliseconds 120
}
Start-Sleep -Seconds 1

if ($p.HasExited) { Write-Host ("FAIL: process died, exit=" + $p.ExitCode); exit 1 }

$r = New-Object EK+RECT
[void][EK]::GetWindowRect($frame, [ref]$r)
$bmp = New-Object System.Drawing.Bitmap ($r.Right-$r.Left), ($r.Bottom-$r.Top)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[void][EK]::PrintWindow($frame, $hdc, 0)
$g.ReleaseHdc($hdc)
$png = Join-Path $OutDir "edit_smoke.png"
$bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host ("[PASS] survived keystrokes, shot=" + $png)
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue

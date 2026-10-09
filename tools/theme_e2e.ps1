# End-to-end check for the colour-scheme feature:
#   1. launch EClone, open Tools->System Config, pick scheme N, press OK
#   2. screenshot the main window  -> shots\theme_<N>.png
#   3. kill, relaunch, screenshot again -> shots\theme_<N>_reload.png (persistence)
# ASCII only.
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$ComboSel = 5,
    [string]$Tag = "theme",
    [int]$WaitSec = 8
)
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class LE {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
}
"@

function Find-Top([int]$procId, [string]$cls, [IntPtr]$exclude) {
    $script:found = [IntPtr]::Zero
    $script:pid_ = $procId; $script:cls_ = $cls; $script:ex_ = $exclude
    [void][LE]::EnumWindows({
        param($h, $q)
        $wpid = 0; [void][LE]::GetWindowThreadProcessId($h, [ref]$wpid)
        if ($wpid -eq $script:pid_ -and [LE]::IsWindowVisible($h) -and $h -ne $script:ex_ -and [LE]::Cls($h) -eq $script:cls_) { $script:found = $h }
        return $true
    }, [IntPtr]::Zero)
    return $script:found
}

function Shot([IntPtr]$hwnd, [string]$path) {
    $r = New-Object LE+RECT; [void][LE]::GetWindowRect($hwnd, [ref]$r)
    $bmp = New-Object System.Drawing.Bitmap ($r.Right-$r.Left), ($r.Bottom-$r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $hdc = $g.GetHdc()
    [void][LE]::PrintWindow($hwnd, $hdc, 0)
    $g.ReleaseHdc($hdc); $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    Write-Host ("shot: " + $path)
}

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }
$frame = Find-Top $p.Id 'ENewFrame' ([IntPtr]::Zero)
if ($frame -eq [IntPtr]::Zero) { Write-Host "no frame"; Stop-Process -Id $p.Id -Force; exit 1 }
[void][LE]::PostMessage($frame, 0x0111, [IntPtr]32876, [IntPtr]::Zero)
Start-Sleep -Seconds 2
$dlg = Find-Top $p.Id '#32770' $frame
if ($dlg -eq [IntPtr]::Zero) { Write-Host "no dialog"; Stop-Process -Id $p.Id -Force; exit 2 }
if ($ComboSel -ge 0) {
    $cbo = [LE]::GetDlgItem($dlg, 531)
    [void][LE]::SendMessage($cbo, 0x014E, [IntPtr]$ComboSel, [IntPtr]::Zero)
    [void][LE]::SendMessage($dlg, 0x0111, [IntPtr]531, [IntPtr]0)
    Start-Sleep -Milliseconds 900
}
[void][LE]::PostMessage($dlg, 0x0111, [IntPtr]1, [IntPtr]::Zero)   # IDOK
Start-Sleep -Seconds 2
Shot $frame (Join-Path $OutDir ($Tag + ".png"))
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

# relaunch -> persistence check
$p2 = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p2.HasExited) { Write-Host "relaunch exited early"; exit 1 }
$f2 = Find-Top $p2.Id 'ENewFrame' ([IntPtr]::Zero)
if ($f2 -eq [IntPtr]::Zero) { Write-Host "no frame on reload"; Stop-Process -Id $p2.Id -Force; exit 1 }
Shot $f2 (Join-Path $OutDir ($Tag + "_reload.png"))
Stop-Process -Id $p2.Id -Force -ErrorAction SilentlyContinue
Write-Host "done"

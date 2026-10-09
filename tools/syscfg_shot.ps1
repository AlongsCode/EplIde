# Screenshot the EClone system-config dialog. ASCII only.
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$Out = "D:\eclone\shots\syscfg\ec_syscfg.png",
    [int]$CmdId = 32876,
    [int]$WaitSec = 7,
    [int]$ComboSel = -1,
    [switch]$Keep
)
$ErrorActionPreference = 'Continue'
New-Item -ItemType Directory -Force -Path (Split-Path $Out -Parent) | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class LM {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
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
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }
$script:pid_ = $p.Id; $script:frame = [IntPtr]::Zero
[void][LM]::EnumWindows({
    param($h, $q)
    $wpid = 0; [void][LM]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:pid_ -and [LM]::IsWindowVisible($h) -and [LM]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Stop-Process -Id $p.Id -Force; exit 1 }
[void][LM]::PostMessage($script:frame, 0x0111, [IntPtr]$CmdId, [IntPtr]::Zero)
Start-Sleep -Seconds 2
$script:dlg = [IntPtr]::Zero
[void][LM]::EnumWindows({
    param($h, $q)
    $wpid = 0; [void][LM]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:pid_ -and [LM]::IsWindowVisible($h) -and $h -ne $script:frame -and [LM]::Cls($h) -eq '#32770') { $script:dlg = $h }
    return $true
}, [IntPtr]::Zero)
if ($script:dlg -eq [IntPtr]::Zero) { Write-Host "no dialog"; Stop-Process -Id $p.Id -Force; exit 2 }
if ($ComboSel -ge 0) {
    $cbo = [LM]::GetDlgItem($script:dlg, 531)
    [void][LM]::SendMessage($cbo, 0x014E, [IntPtr]$ComboSel, [IntPtr]::Zero)   # CB_SETCURSEL
    [void][LM]::SendMessage($script:dlg, 0x0111, [IntPtr]531, [IntPtr]0)       # WM_COMMAND CBN_SELCHANGE
    Start-Sleep -Milliseconds 800
}
$r = New-Object LM+RECT; [void][LM]::GetWindowRect($script:dlg, [ref]$r)
$bmp = New-Object System.Drawing.Bitmap ($r.Right-$r.Left), ($r.Bottom-$r.Top)
$g = [System.Drawing.Graphics]::FromImage($bmp); $hdc = $g.GetHdc()
[void][LM]::PrintWindow($script:dlg, $hdc, 0)
$g.ReleaseHdc($hdc); $g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
Write-Host ("shot: " + $Out + " " + ($r.Right-$r.Left) + "x" + ($r.Bottom-$r.Top))
if ($Keep) { Write-Host ("pid=" + $p.Id) } else { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }

# Manual probe: launch EClone with ECLONE_DUMP_MODEL, send ONE insert command to the MDI child,
# then print whatever files the editor produced. Used to debug the insert smoke test.
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\dbg_insert.ps1 [cmdId]
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [int]$Cmd = 32779,
    [string]$Model = "D:\eclone\shots\model_dbg_ins.txt",
    [int]$WaitSec = 7
)
$ErrorActionPreference = 'Continue'
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class DI {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
}
"@
Remove-Item $Model -Force -ErrorAction SilentlyContinue
Remove-Item ($Model + ".trace.txt") -Force -ErrorAction SilentlyContinue
$env:ECLONE_DUMP_MODEL = $Model
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
Write-Host ("pid=" + $p.Id + " exited=" + $p.HasExited)
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero; $script:child = [IntPtr]::Zero
[void][DI]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][DI]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [DI]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
Write-Host ("frame=" + $script:frame)
[void][DI]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([DI]::Cls($h) -eq 'AfxMDIFrame42s') { $script:child = $h; return $false }
    return $true
}, [IntPtr]::Zero)
Write-Host ("child=" + $script:child)
[void][DI]::SendMessage($script:child, 0x0111, [IntPtr]$Cmd, [IntPtr]::Zero)
Start-Sleep -Seconds 1
Write-Host ("exited=" + $p.HasExited)
Get-ChildItem (Split-Path $Model) -Filter (Split-Path $Model -Leaf) -Recurse | Select-Object Name,Length
Get-ChildItem (Split-Path $Model) -Filter ((Split-Path $Model -Leaf) + ".trace.txt") | Select-Object Name,Length
$f = $Model + ".trace.txt"
if (Test-Path $f) { Write-Host "--- trace ---"; Get-Content $f -Encoding Default }
else { Write-Host "--- no trace file ---" }
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_MODEL

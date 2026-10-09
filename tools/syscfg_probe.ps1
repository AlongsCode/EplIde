# Minimal EClone window-tree dump around a menu command; ASCII only.
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [int]$CmdId = 32876,
    [int]$WaitSec = 7
)
$ErrorActionPreference = 'Continue'
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class LZ {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern int SendMessageA(IntPtr h, uint m, int w, StringBuilder l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Text(IntPtr h){ var sb=new StringBuilder(1024); GetWindowTextW(h,sb,1024); return sb.ToString(); }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string CText(IntPtr h, int i){ var sb=new StringBuilder(512); SendMessageA(h, 0x0148, i, sb); return sb.ToString(); }
  public static int CCount(IntPtr h){ return (int)SendMessage(h, 0x0146, IntPtr.Zero, IntPtr.Zero); }
  public static int CSel(IntPtr h){ return (int)SendMessage(h, 0x0147, IntPtr.Zero, IntPtr.Zero); }
  public static int BCheck(IntPtr h){ return (int)SendMessage(h, 0x00F0, IntPtr.Zero, IntPtr.Zero); }
}
"@
Write-Host "type loaded"
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }
Write-Host ("pid=" + $p.Id)

$script:frame = [IntPtr]::Zero
$script:pid_ = $p.Id
[void][LZ]::EnumWindows({
    param($h, $q)
    $wpid = 0
    [void][LZ]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:pid_ -and [LZ]::IsWindowVisible($h)) {
        if ([LZ]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    }
    return $true
}, [IntPtr]::Zero)
Write-Host ("frame=" + $script:frame)
if ($script:frame -eq [IntPtr]::Zero) { Stop-Process -Id $p.Id -Force; exit 1 }

[void][LZ]::PostMessage($script:frame, 0x0111, [IntPtr]$CmdId, [IntPtr]::Zero)
Start-Sleep -Seconds 2

$script:dlg = [IntPtr]::Zero
[void][LZ]::EnumWindows({
    param($h, $q)
    $wpid = 0
    [void][LZ]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:pid_ -and [LZ]::IsWindowVisible($h) -and $h -ne $script:frame) {
        if ([LZ]::Cls($h) -eq '#32770') { $script:dlg = $h }
    }
    return $true
}, [IntPtr]::Zero)
Write-Host ("dialog=" + $script:dlg)
if ($script:dlg -eq [IntPtr]::Zero) { Stop-Process -Id $p.Id -Force; exit 2 }

[void][LZ]::EnumChildWindows($script:dlg, {
    param($c, $q)
    $cl = [LZ]::Cls($c)
    $id = [LZ]::GetDlgCtrlID($c)
    $tx = [LZ]::Text($c)
    $line = "" + $cl + "|id=" + $id + "|vis=" + [LZ]::IsWindowVisible($c) + "|" + $tx
    if ($cl -eq 'ComboBox') {
        $n = [LZ]::CCount($c)
        $line = $line + "|count=" + $n + "|sel=" + [LZ]::CSel($c)
        for ($i=0; $i -lt $n; $i++) { $line = $line + "|[" + $i + "]" + [LZ]::CText($c, $i) }
    }
    if ($cl -eq 'Button') { $line = $line + "|check=" + [LZ]::BCheck($c) }
    Write-Host $line
    return $true
}, [IntPtr]::Zero)
Write-Host "enum done"
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue

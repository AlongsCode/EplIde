# Launch EClone with ECLONE_DUMP_MODEL set, force a redump (WM_USER+102) and exit.
# ASCII only.
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$Dump = "D:\eclone\shots\model_dbg.txt"
)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class RD {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
}
"@
$env:ECLONE_DUMP_MODEL = $Dump
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds 5
$view = [IntPtr]::Zero
[void][RD]::EnumWindows([RD+EnumProc]{
    param($h, $q)
    $wpid = 0
    [void][RD]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id) {
        [void][RD]::EnumChildWindows($h, [RD+EnumProc]{
            param($c, $z)
            $sb = New-Object System.Text.StringBuilder 256
            [void][RD]::GetClassNameW($c, $sb, 256)
            if ($sb.ToString().StartsWith('Afx:400000:8:10005')) { $script:view = $c }
            return $true
        }, [IntPtr]::Zero)
    }
    return $true
}, [IntPtr]::Zero)
Write-Host ("view=" + $view)
if ($view -ne [IntPtr]::Zero) { [void][RD]::PostMessage($view, 0x466, [IntPtr]::Zero, [IntPtr]::Zero) }
Start-Sleep -Seconds 2
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue

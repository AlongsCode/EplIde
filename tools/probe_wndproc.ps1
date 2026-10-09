# Which module owns the window procedure of the design form?  If the WndProc lives
# in e.exe (not krnln.fne) then the IDE itself subclasses the design form -- that is
# who paints the dot grid.
# ASCII only.
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\graphic.e",
    [string]$Class = "WTWindow",
    [string]$ExeArg = "",
    [string]$NamedArg = "",
    [int]$WaitSec = 12
)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class WP {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern IntPtr GetWindowLongPtr(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern int GetClassLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern IntPtr GetProp(IntPtr h, string s);
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static string Where(IntPtr addr, List<string> mods) {
    long a = (long)addr;
    foreach (string m in mods) {
      string[] p = m.Split('|');
      long b = long.Parse(p[1]), e = long.Parse(p[2]);
      if (a >= b && a < e) return p[0] + "+0x" + (a - b).ToString("X");
    }
    return "?";
  }
}
"@

$args2 = @()
if ($File -ne "" -and $File -ne "none") { $args2 += $File }
if ($ExeArg -ne "") { $args2 += $ExeArg }
if ($NamedArg -ne "") { $args2 += $NamedArg }
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList $args2 -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$mods = New-Object System.Collections.Generic.List[string]
foreach ($m in $p.Modules) {
    $mods.Add($m.ModuleName + "|" + $m.BaseAddress.ToInt64() + "|" + ($m.BaseAddress.ToInt64() + $m.ModuleMemorySize))
}
Write-Host "modules:"
foreach ($m in $p.Modules) { Write-Host ("  {0,-24} {1} - {2}" -f $m.ModuleName, $m.BaseAddress.ToString("X"), ($m.BaseAddress.ToInt64()+$m.ModuleMemorySize).ToString("X")) }

$targets = New-Object System.Collections.Generic.List[IntPtr]
[void][WP]::EnumWindows([WP+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][WP]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id) {
        [void][WP]::EnumChildWindows($h, [WP+EnumProc]{
            param($c, $q)
            if ([WP]::Cls($c) -eq $Class) { $targets.Add($c) }
            return $true
        }, [IntPtr]::Zero)
    }
    return $true
}, [IntPtr]::Zero)
Write-Host ("targets=" + $targets.Count)
foreach ($t in $targets) {
    $wp2 = [WP]::GetWindowLongPtr($t, -4)
    $cl = [WP]::GetClassLong($t, -24)     # GCL_WNDPROC
    Write-Host ("hwnd={0} class={1}" -f $t, [WP]::Cls($t))
    Write-Host ("   instance WndProc={0:X}  -> {1}" -f $wp2.ToInt64(), [WP]::Where($wp2, $mods))
    Write-Host ("   class    WndProc={0:X}  -> {1}" -f $cl, [WP]::Where([IntPtr]$cl, $mods))
    Write-Host ("   class    hbrBackground={0:X}  style={1:X}" -f [WP]::GetClassLong($t,-10), [WP]::GetClassLong($t,-26))
}
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
